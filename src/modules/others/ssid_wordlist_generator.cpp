#include "ssid_wordlist_generator.h"

#include "core/display.h"
#include "core/sd_functions.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <globals.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

namespace {

constexpr size_t MAX_SSIDS = 96;
constexpr size_t MAX_SSID_CHARS = 32;
constexpr size_t MAX_WORDS = 5000;
constexpr size_t MAX_WORD_LEN = 63;
constexpr size_t MAX_BASES = 8;
constexpr size_t MAX_BASE_LEN = 64;
constexpr size_t MAX_PATTERNS = 64;
constexpr size_t MAX_PATTERN_LEN = 24;
constexpr size_t HASH_TABLE_SIZE = 8192; // power of two
constexpr size_t UI_MSG_LEN = 56;

constexpr const char *kOutputDir = "/wordlists";
constexpr const char *kOutputFile = "/wordlists/ssid_wordlist.txt";
constexpr const char *kPatternEndpoint =
    "https://raw.githubusercontent.com/danielmiessler/SecLists/master/Passwords/Common-Credentials/"
    "top-20-common-SSH-passwords.txt";

constexpr const char *kNumberTokens[] = {"123", "1234", "2024", "2026"};
constexpr const char *kSpecialTokens[] = {"!", "@", "#", "$", "%", "^", "&", "*", "_", "-", "+", "=", "?"};

enum class GeneratorStage : uint8_t { Idle, Scanning, Fetching, Generating, Saving, Done, Cancelled, Error };

struct GeneratorContext {
    volatile bool running = false;
    volatile bool cancelRequested = false;
    volatile int ssidCount = 0;
    volatile int processedSsids = 0;
    volatile int generatedWords = 0;
    volatile int fetchPatternCount = 0;
    volatile GeneratorStage stage = GeneratorStage::Idle;
    bool usedSd = false;
    char status[UI_MSG_LEN] = "Ready";

    char ssids[MAX_SSIDS][MAX_SSID_CHARS + 1] = {};
    char patterns[MAX_PATTERNS][MAX_PATTERN_LEN + 1] = {};
    uint64_t *seenWordHashes = nullptr;
};

GeneratorContext g_ctx;
TaskHandle_t g_taskHandle = nullptr;

inline void setStatus(const char *msg) {
    if (!msg) {
        g_ctx.status[0] = '\0';
        return;
    }
    strncpy(g_ctx.status, msg, UI_MSG_LEN - 1);
    g_ctx.status[UI_MSG_LEN - 1] = '\0';
}

uint64_t fnv1a64(const char *str) {
    uint64_t hash = 1469598103934665603ULL;
    while (*str) {
        hash ^= static_cast<uint8_t>(*str++);
        hash *= 1099511628211ULL;
    }
    return hash;
}

bool addWordHash(uint64_t hash) {
    if (!g_ctx.seenWordHashes) return false;
    if (hash == 0) hash = 1;
    size_t idx = static_cast<size_t>(hash) & (HASH_TABLE_SIZE - 1);
    for (size_t step = 0; step < HASH_TABLE_SIZE; ++step) {
        size_t probe = (idx + step) & (HASH_TABLE_SIZE - 1);
        uint64_t &slot = g_ctx.seenWordHashes[probe];
        if (slot == 0) {
            slot = hash;
            return true;
        }
        if (slot == hash) return false;
    }
    return false;
}

void sanitizeSsid(const char *in, char *out, size_t outSize) {
    if (!in || !out || outSize == 0) return;
    size_t j = 0;
    for (size_t i = 0; in[i] != '\0' && j < outSize - 1; ++i) {
        char c = in[i];
        if (c < 32 || c > 126) continue;
        out[j++] = c;
    }
    out[j] = '\0';

    // trim
    size_t start = 0;
    while (out[start] == ' ' || out[start] == '\t') start++;
    if (start > 0) memmove(out, out + start, strlen(out + start) + 1);
    size_t len = strlen(out);
    while (len > 0 && (out[len - 1] == ' ' || out[len - 1] == '\t')) {
        out[len - 1] = '\0';
        --len;
    }
}

bool containsSsid(const char *ssid) {
    for (int i = 0; i < g_ctx.ssidCount; ++i) {
        if (strcmp(g_ctx.ssids[i], ssid) == 0) return true;
    }
    return false;
}

bool addSsid(const char *ssid) {
    if (!ssid || !ssid[0]) return false;
    if (g_ctx.ssidCount >= static_cast<int>(MAX_SSIDS)) return false;
    if (containsSsid(ssid)) return false;
    strncpy(g_ctx.ssids[g_ctx.ssidCount], ssid, MAX_SSID_CHARS);
    g_ctx.ssids[g_ctx.ssidCount][MAX_SSID_CHARS] = '\0';
    ++g_ctx.ssidCount;
    return true;
}

bool containsLocal(char variants[MAX_BASES][MAX_BASE_LEN], int count, const char *value) {
    for (int i = 0; i < count; ++i) {
        if (strcmp(variants[i], value) == 0) return true;
    }
    return false;
}

void addVariant(char variants[MAX_BASES][MAX_BASE_LEN], int &count, const char *value) {
    if (!value || !value[0]) return;
    if (count >= static_cast<int>(MAX_BASES)) return;
    if (containsLocal(variants, count, value)) return;
    strncpy(variants[count], value, MAX_BASE_LEN - 1);
    variants[count][MAX_BASE_LEN - 1] = '\0';
    ++count;
}

void toLowerCopy(const char *in, char *out, size_t outSize) {
    size_t i = 0;
    for (; in[i] != '\0' && i < outSize - 1; ++i) out[i] = static_cast<char>(tolower(static_cast<uint8_t>(in[i])));
    out[i] = '\0';
}

void toUpperCopy(const char *in, char *out, size_t outSize) {
    size_t i = 0;
    for (; in[i] != '\0' && i < outSize - 1; ++i) out[i] = static_cast<char>(toupper(static_cast<uint8_t>(in[i])));
    out[i] = '\0';
}

void capitalizeCopy(const char *in, char *out, size_t outSize) {
    toLowerCopy(in, out, outSize);
    if (out[0]) out[0] = static_cast<char>(toupper(static_cast<uint8_t>(out[0])));
}

void alternatingCaseCopy(const char *in, char *out, size_t outSize) {
    size_t i = 0;
    int alphaIdx = 0;
    for (; in[i] != '\0' && i < outSize - 1; ++i) {
        char c = in[i];
        if (isalpha(static_cast<uint8_t>(c))) {
            bool upper = (alphaIdx % 2) == 0;
            out[i] = upper ? static_cast<char>(toupper(static_cast<uint8_t>(c)))
                           : static_cast<char>(tolower(static_cast<uint8_t>(c)));
            ++alphaIdx;
        } else {
            out[i] = c;
        }
    }
    out[i] = '\0';
}

void leetCopy(const char *in, char *out, size_t outSize) {
    size_t i = 0;
    for (; in[i] != '\0' && i < outSize - 1; ++i) {
        char c = in[i];
        switch (tolower(static_cast<uint8_t>(c))) {
            case 'a': out[i] = '@'; break;
            case 's': out[i] = '$'; break;
            case 'o': out[i] = '0'; break;
            case 'i': out[i] = '1'; break;
            case 'e': out[i] = '3'; break;
            default: out[i] = c; break;
        }
    }
    out[i] = '\0';
}

void removeSpacesCopy(const char *in, char *out, size_t outSize) {
    size_t j = 0;
    for (size_t i = 0; in[i] != '\0' && j < outSize - 1; ++i) {
        if (in[i] == ' ' || in[i] == '\t') continue;
        out[j++] = in[i];
    }
    out[j] = '\0';
}

int buildBaseVariants(const char *ssid, char variants[MAX_BASES][MAX_BASE_LEN]) {
    int count = 0;
    char buf[MAX_BASE_LEN];

    addVariant(variants, count, ssid);
    removeSpacesCopy(ssid, buf, sizeof(buf));
    addVariant(variants, count, buf);
    toLowerCopy(ssid, buf, sizeof(buf));
    addVariant(variants, count, buf);
    toUpperCopy(ssid, buf, sizeof(buf));
    addVariant(variants, count, buf);
    capitalizeCopy(ssid, buf, sizeof(buf));
    addVariant(variants, count, buf);
    alternatingCaseCopy(ssid, buf, sizeof(buf));
    addVariant(variants, count, buf);
    leetCopy(ssid, buf, sizeof(buf));
    addVariant(variants, count, buf);

    return count;
}

bool writeCandidate(File &file, const char *candidate) {
    if (!candidate || !candidate[0]) return false;
    size_t len = strlen(candidate);
    if (len < 8 || len > MAX_WORD_LEN) return false;
    if (g_ctx.generatedWords >= static_cast<int>(MAX_WORDS)) return false;
    uint64_t hash = fnv1a64(candidate);
    if (!addWordHash(hash)) return false;
    file.println(candidate);
    ++g_ctx.generatedWords;
    return true;
}

void emitWithParts(File &file, const char *a, const char *b = nullptr, const char *c = nullptr) {
    if (g_ctx.generatedWords >= static_cast<int>(MAX_WORDS) || g_ctx.cancelRequested) return;
    char candidate[MAX_WORD_LEN + 1];
    size_t pos = 0;
    const char *parts[3] = {a, b, c};
    for (const char *p : parts) {
        if (!p) continue;
        for (size_t i = 0; p[i] != '\0' && pos < MAX_WORD_LEN; ++i) candidate[pos++] = p[i];
        if (pos >= MAX_WORD_LEN && p[strlen(p) - 1] != candidate[pos - 1]) break;
    }
    candidate[pos] = '\0';
    writeCandidate(file, candidate);
}

void generateFromBase(File &file, const char *base, const char patterns[MAX_PATTERNS][MAX_PATTERN_LEN + 1], int patternCount) {
    emitWithParts(file, base);

    for (size_t i = 0; i < sizeof(kNumberTokens) / sizeof(kNumberTokens[0]); ++i) {
        const char *num = kNumberTokens[i];
        emitWithParts(file, base, num);
        emitWithParts(file, num, base);
    }

    for (size_t i = 0; i < sizeof(kSpecialTokens) / sizeof(kSpecialTokens[0]); ++i) {
        const char *special = kSpecialTokens[i];
        emitWithParts(file, base, special);
        emitWithParts(file, special, base);
    }

    for (size_t i = 0; i < sizeof(kNumberTokens) / sizeof(kNumberTokens[0]); ++i) {
        const char *num = kNumberTokens[i];
        for (size_t j = 0; j < sizeof(kSpecialTokens) / sizeof(kSpecialTokens[0]); ++j) {
            const char *special = kSpecialTokens[j];
            emitWithParts(file, base, num, special);
            emitWithParts(file, special, base, num);
        }
    }

    for (int i = 0; i < patternCount; ++i) {
        emitWithParts(file, base, patterns[i]);
        emitWithParts(file, patterns[i], base);
    }
}

void fetchPatternsFromInternet() {
    g_ctx.fetchPatternCount = 0;
    if (WiFi.status() != WL_CONNECTED || g_ctx.cancelRequested) return;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    if (!http.begin(client, kPatternEndpoint)) return;

    http.setTimeout(3000);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        return;
    }

    WiFiClient *stream = http.getStreamPtr();
    char line[MAX_PATTERN_LEN + 1] = {};
    size_t idx = 0;

    while (http.connected() && !g_ctx.cancelRequested && g_ctx.fetchPatternCount < static_cast<int>(MAX_PATTERNS)) {
        while (stream->available()) {
            char c = static_cast<char>(stream->read());
            if (c == '\r') continue;
            if (c == '\n') {
                line[idx] = '\0';
                if (idx > 0) {
                    strncpy(g_ctx.patterns[g_ctx.fetchPatternCount], line, MAX_PATTERN_LEN);
                    g_ctx.patterns[g_ctx.fetchPatternCount][MAX_PATTERN_LEN] = '\0';
                    ++g_ctx.fetchPatternCount;
                }
                idx = 0;
                continue;
            }
            if (idx < MAX_PATTERN_LEN && c >= 33 && c <= 126) line[idx++] = c;
        }
        delay(1);
    }

    if (idx > 0 && g_ctx.fetchPatternCount < static_cast<int>(MAX_PATTERNS)) {
        line[idx] = '\0';
        strncpy(g_ctx.patterns[g_ctx.fetchPatternCount], line, MAX_PATTERN_LEN);
        g_ctx.patterns[g_ctx.fetchPatternCount][MAX_PATTERN_LEN] = '\0';
        ++g_ctx.fetchPatternCount;
    }

    http.end();
}

bool prepareOutputFile(FS *&fs, File &file) {
    if (!getFsStorage(fs) || !fs) {
        setStatus("Storage unavailable");
        return false;
    }

    g_ctx.usedSd = (fs == &SD);

    if (!fs->exists(kOutputDir)) {
        if (!fs->mkdir(kOutputDir)) {
            setStatus("Cannot create /wordlists");
            return false;
        }
    }
    if (fs->exists(kOutputFile)) fs->remove(kOutputFile);

    file = fs->open(kOutputFile, FILE_WRITE);
    if (!file) {
        setStatus("Cannot open output file");
        return false;
    }
    return true;
}

void generatorTask(void * /*arg*/) {
    g_ctx.running = true;
    g_ctx.cancelRequested = false;
    g_ctx.stage = GeneratorStage::Scanning;
    g_ctx.ssidCount = 0;
    g_ctx.processedSsids = 0;
    g_ctx.generatedWords = 0;
    if (g_ctx.seenWordHashes) {
        free(g_ctx.seenWordHashes);
        g_ctx.seenWordHashes = nullptr;
    }
    g_ctx.seenWordHashes = static_cast<uint64_t *>(calloc(HASH_TABLE_SIZE, sizeof(uint64_t)));
    if (!g_ctx.seenWordHashes) {
        setStatus("Not enough memory");
        g_ctx.stage = GeneratorStage::Error;
        g_ctx.running = false;
        g_taskHandle = nullptr;
        vTaskDelete(nullptr);
        return;
    }
    setStatus("Scanning SSIDs...");

    FS *outFs = nullptr;
    File outFile;
    if (!prepareOutputFile(outFs, outFile)) {
        free(g_ctx.seenWordHashes);
        g_ctx.seenWordHashes = nullptr;
        g_ctx.stage = GeneratorStage::Error;
        g_ctx.running = false;
        g_taskHandle = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    int nets = WiFi.scanNetworks(false, false);
    if (nets < 0) {
        setStatus("WiFi scan failed");
        outFile.close();
        free(g_ctx.seenWordHashes);
        g_ctx.seenWordHashes = nullptr;
        g_ctx.stage = GeneratorStage::Error;
        g_ctx.running = false;
        g_taskHandle = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    char cleaned[MAX_SSID_CHARS + 1];
    for (int i = 0; i < nets && !g_ctx.cancelRequested; ++i) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;
        sanitizeSsid(ssid.c_str(), cleaned, sizeof(cleaned));
        if (cleaned[0] == '\0') continue;
        addSsid(cleaned);
        if ((i % 6) == 0) delay(1);
    }
    WiFi.scanDelete();

    if (g_ctx.cancelRequested) {
        outFile.close();
        free(g_ctx.seenWordHashes);
        g_ctx.seenWordHashes = nullptr;
        g_ctx.stage = GeneratorStage::Cancelled;
        setStatus("Cancelled");
        g_ctx.running = false;
        g_taskHandle = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    g_ctx.stage = GeneratorStage::Fetching;
    setStatus("Fetching patterns...");
    fetchPatternsFromInternet();

    g_ctx.stage = GeneratorStage::Generating;
    setStatus("Generating wordlist...");

    char baseVariants[MAX_BASES][MAX_BASE_LEN];
    for (int i = 0; i < g_ctx.ssidCount && !g_ctx.cancelRequested; ++i) {
        int baseCount = buildBaseVariants(g_ctx.ssids[i], baseVariants);
        for (int b = 0; b < baseCount && !g_ctx.cancelRequested; ++b) {
            generateFromBase(outFile, baseVariants[b], g_ctx.patterns, g_ctx.fetchPatternCount);
            if (g_ctx.generatedWords >= static_cast<int>(MAX_WORDS)) break;
        }
        ++g_ctx.processedSsids;
        if (g_ctx.generatedWords >= static_cast<int>(MAX_WORDS)) break;
        delay(1);
    }

    g_ctx.stage = GeneratorStage::Saving;
    setStatus("Saving file...");
    outFile.flush();
    outFile.close();

    if (g_ctx.cancelRequested) {
        g_ctx.stage = GeneratorStage::Cancelled;
        setStatus("Cancelled");
    } else {
        g_ctx.stage = GeneratorStage::Done;
        setStatus("Completed");
    }

    free(g_ctx.seenWordHashes);
    g_ctx.seenWordHashes = nullptr;
    g_ctx.running = false;
    g_taskHandle = nullptr;
    vTaskDelete(nullptr);
}

const char *stageToText(GeneratorStage stage) {
    switch (stage) {
        case GeneratorStage::Idle: return "Idle";
        case GeneratorStage::Scanning: return "Scanning WiFi";
        case GeneratorStage::Fetching: return "Fetching Patterns";
        case GeneratorStage::Generating: return "Generating";
        case GeneratorStage::Saving: return "Saving";
        case GeneratorStage::Done: return "Done";
        case GeneratorStage::Cancelled: return "Cancelled";
        case GeneratorStage::Error: return "Error";
        default: return "Unknown";
    }
}

int progressPercent() {
    switch (g_ctx.stage) {
        case GeneratorStage::Scanning: return 10;
        case GeneratorStage::Fetching: return 20;
        case GeneratorStage::Generating:
            return 20 + (g_ctx.generatedWords * 75 / static_cast<int>(MAX_WORDS));
        case GeneratorStage::Saving: return 96;
        case GeneratorStage::Done: return 100;
        case GeneratorStage::Cancelled: return 100;
        case GeneratorStage::Error: return 100;
        default: return 0;
    }
}

void drawProgressScreen(bool running) {
    drawMainBorderWithTitle("SSID Wordlist Generator", true);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(FP);

    int y = 36;
    tft.setCursor(10, y);
    tft.printf("Stage: %s", stageToText(g_ctx.stage));
    y += 14;
    tft.setCursor(10, y);
    tft.printf("SSIDs found: %d", g_ctx.ssidCount);
    y += 14;
    tft.setCursor(10, y);
    tft.printf("SSIDs done: %d", g_ctx.processedSsids);
    y += 14;
    tft.setCursor(10, y);
    tft.printf("Words: %d / %d", g_ctx.generatedWords, static_cast<int>(MAX_WORDS));
    y += 14;
    tft.setCursor(10, y);
    tft.printf("API patterns: %d", g_ctx.fetchPatternCount);
    y += 14;
    tft.setCursor(10, y);
    tft.printf("Storage: %s", g_ctx.usedSd ? "SD" : "LittleFS");

    const int barX = 10;
    const int barY = tftHeight - 44;
    const int barW = tftWidth - 20;
    const int barH = 14;
    int p = progressPercent();
    int fill = (barW - 2) * p / 100;
    tft.drawRect(barX, barY, barW, barH, bruceConfig.priColor);
    tft.fillRect(barX + 1, barY + 1, fill, barH - 2, bruceConfig.priColor);

    tft.setCursor(10, tftHeight - 24);
    if (running) {
        tft.print("A: running  B: cancel");
    } else {
        tft.print("A/B: back to menu");
    }
}

} // namespace

void ssid_wordlist_generator_menu() {
    g_ctx.stage = GeneratorStage::Idle;
    g_ctx.running = false;
    g_ctx.cancelRequested = false;
    g_ctx.ssidCount = 0;
    g_ctx.processedSsids = 0;
    g_ctx.generatedWords = 0;
    g_ctx.fetchPatternCount = 0;
    g_ctx.usedSd = false;
    setStatus("Ready");

    drawMainBorderWithTitle("SSID Wordlist Generator", true);
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setCursor(10, 38);
    tft.println("Generate password candidates");
    tft.setCursor(10, 50);
    tft.println("from scanned WiFi SSIDs.");
    tft.setCursor(10, 70);
    tft.println("Output: /wordlists/");
    tft.setCursor(10, 82);
    tft.println("ssid_wordlist.txt");
    tft.setCursor(10, tftHeight - 24);
    tft.println("A: start  B: back");

    while (1) {
        if (check(SelPress)) break;
        if (check(EscPress) || check(PrevPress)) return;
        delay(20);
    }

    if (xTaskCreate(generatorTask, "ssid_wordlist_gen", 8192, nullptr, 1, &g_taskHandle) != pdPASS) {
        displayError("Cannot start generator task", true);
        return;
    }

    uint32_t lastDraw = 0;
    while (g_ctx.running) {
        if ((millis() - lastDraw) > 120) {
            drawProgressScreen(true);
            lastDraw = millis();
        }
        if (check(EscPress) || check(PrevPress)) {
            g_ctx.cancelRequested = true;
            setStatus("Cancelling...");
        }
        delay(20);
    }

    drawProgressScreen(false);
    tft.setCursor(10, tftHeight - 36);
    tft.fillRect(10, tftHeight - 38, tftWidth - 20, 12, bruceConfig.bgColor);

    if (g_ctx.stage == GeneratorStage::Done) {
        tft.print("Saved: /wordlists/ssid_wordlist.txt");
    } else if (g_ctx.stage == GeneratorStage::Cancelled) {
        tft.print("Generation cancelled");
    } else if (g_ctx.stage == GeneratorStage::Error) {
        tft.print(g_ctx.status);
    } else {
        tft.print("Stopped");
    }

    while (1) {
        if (check(SelPress) || check(EscPress) || check(PrevPress) || check(NextPress)) break;
        delay(30);
    }
}
