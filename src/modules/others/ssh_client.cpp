#include "ssh_client.h"

#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/sd_functions.h"
#include "core/wifi/wifi_common.h"
#include <globals.h>

#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_system.h>
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>
#include <time.h>

#ifndef LITE_VERSION
#include "libssh_esp32.h"
#include <libssh/libssh.h>
#endif

namespace {

constexpr const char *kPrefNs = "ssh_cfg";
constexpr const char *kPrefConsent = "consent";
constexpr const char *kPrefHost = "host";
constexpr const char *kPrefPort = "port";
constexpr const char *kPrefUser = "user";
constexpr const char *kPrefAuthType = "auth_t";
constexpr const char *kPrefSecret = "secret";
constexpr const char *kPrefKeyPath = "kpath";
constexpr const char *kPrefSaveCreds = "savecred";
constexpr const char *kPrefPersistHistory = "hist";
constexpr const char *kPrefSecureLog = "slog";
constexpr const char *kPrefSerialInput = "serin";
constexpr const char *kPrefAutoDisc = "adisc";

constexpr const char *kSshDir = "/ssh";
constexpr const char *kSessionDir = "/ssh/sessions";
constexpr const char *kHistoryPath = "/ssh/history.log";
constexpr const char *kExportDefaultPath = "/ssh/history_export.txt";

constexpr size_t kMaxOutputCollect = 8192;
constexpr size_t kMaxConsoleLines = 120;
constexpr size_t kMaxHistoryView = 64;
constexpr uint32_t kDefaultCmdTimeoutMs = 30000;

struct SshProfile {
    String host;
    uint16_t port = 22;
    String user;
    SSHAuthType authType = SSHAuthType::Password;
    String secret; // password or private key text
    String keyPath;
    bool saveCredentials = false;
    bool persistHistory = true;
    bool secureLog = false;
    bool serialInput = false;
    uint16_t autoDisconnectSec = 180;
    bool consentAccepted = false;
};

struct HistoryEntryView {
    String timestamp;
    String host;
    String kind;
    int exitCode = -1;
    String payload;
};

struct RuntimeConfig {
    bool persistHistory = true;
    bool secureLog = false;
    bool serialInput = false;
    uint16_t autoDisconnectSec = 180;
};

RuntimeConfig g_runtime;

#ifndef LITE_VERSION
struct SSHBackendHandle {
    ssh_session session = nullptr;
};
#endif

uint8_t hexNibble(char c) {
    if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(10 + c - 'a');
    if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(10 + c - 'A');
    return 0xFF;
}

String bytesToHex(const uint8_t *data, size_t len) {
    static const char *kHex = "0123456789abcdef";
    String out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out += kHex[(data[i] >> 4) & 0x0F];
        out += kHex[data[i] & 0x0F];
    }
    return out;
}

bool hexToBytes(const String &hex, uint8_t *out, size_t outLen) {
    if ((hex.length() % 2) != 0) return false;
    if ((hex.length() / 2) > outLen) return false;

    size_t outIdx = 0;
    for (size_t i = 0; i < hex.length(); i += 2) {
        uint8_t hi = hexNibble(hex[i]);
        uint8_t lo = hexNibble(hex[i + 1]);
        if (hi == 0xFF || lo == 0xFF) return false;
        out[outIdx++] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

void deriveDeviceKey(uint8_t key[32]) {
    uint64_t efuse = ESP.getEfuseMac();
    String seed;
    seed.reserve(96);
    seed += "idk-ssh-client-v1|";
    seed += String(static_cast<uint32_t>(efuse >> 32), HEX);
    seed += "|";
    seed += String(static_cast<uint32_t>(efuse & 0xFFFFFFFFULL), HEX);

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, reinterpret_cast<const unsigned char *>(seed.c_str()), seed.length());
    mbedtls_sha256_finish(&ctx, key);
    mbedtls_sha256_free(&ctx);
}

String encryptForStorage(const String &plain) {
    if (plain.length() == 0) return "";

    uint8_t key[32];
    deriveDeviceKey(key);

    uint8_t iv[16];
    for (size_t i = 0; i < sizeof(iv); ++i) iv[i] = static_cast<uint8_t>(esp_random() & 0xFF);

    const size_t plainLen = plain.length();
    uint8_t *cipher = static_cast<uint8_t *>(malloc(plainLen));
    if (!cipher) return "";

    unsigned char nonceCounter[16];
    unsigned char streamBlock[16];
    memcpy(nonceCounter, iv, sizeof(iv));
    memset(streamBlock, 0, sizeof(streamBlock));
    size_t ncOff = 0;

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 256);
    mbedtls_aes_crypt_ctr(
        &aes,
        plainLen,
        &ncOff,
        nonceCounter,
        streamBlock,
        reinterpret_cast<const unsigned char *>(plain.c_str()),
        cipher
    );
    mbedtls_aes_free(&aes);

    String blob = bytesToHex(iv, sizeof(iv)) + ":" + bytesToHex(cipher, plainLen);
    free(cipher);
    return blob;
}

String decryptFromStorage(const String &blob) {
    if (blob.length() == 0) return "";

    int sep = blob.indexOf(':');
    if (sep <= 0 || sep >= static_cast<int>(blob.length() - 1)) return "";

    String ivHex = blob.substring(0, sep);
    String ctHex = blob.substring(sep + 1);

    uint8_t iv[16] = {0};
    if (!hexToBytes(ivHex, iv, sizeof(iv))) return "";

    const size_t ctLen = ctHex.length() / 2;
    uint8_t *cipher = static_cast<uint8_t *>(malloc(ctLen));
    uint8_t *plain = static_cast<uint8_t *>(malloc(ctLen + 1));
    if (!cipher || !plain) {
        free(cipher);
        free(plain);
        return "";
    }

    if (!hexToBytes(ctHex, cipher, ctLen)) {
        free(cipher);
        free(plain);
        return "";
    }

    uint8_t key[32];
    deriveDeviceKey(key);

    unsigned char nonceCounter[16];
    unsigned char streamBlock[16];
    memcpy(nonceCounter, iv, sizeof(iv));
    memset(streamBlock, 0, sizeof(streamBlock));
    size_t ncOff = 0;

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 256);
    mbedtls_aes_crypt_ctr(&aes, ctLen, &ncOff, nonceCounter, streamBlock, cipher, plain);
    mbedtls_aes_free(&aes);

    plain[ctLen] = '\0';
    String out = reinterpret_cast<char *>(plain);
    free(cipher);
    free(plain);
    return out;
}

bool ensureSshDirs() {
    if (!LittleFS.begin(true)) return false;

    if (!LittleFS.exists(kSshDir) && !LittleFS.mkdir(kSshDir)) return false;
    if (!LittleFS.exists(kSessionDir) && !LittleFS.mkdir(kSessionDir)) return false;
    return true;
}

String sanitizeSingleLine(String s, int maxLen = 120) {
    s.replace("\r", " ");
    s.replace("\n", " ");
    s.replace("|", "/");
    while (s.indexOf("  ") >= 0) s.replace("  ", " ");
    s.trim();
    if (s.length() > maxLen) {
        s = s.substring(0, maxLen);
        s += "...";
    }
    return s;
}

String nowTimestamp() {
    time_t now = time(nullptr);
    if (now > 1700000000) {
        struct tm tmv;
        localtime_r(&now, &tmv);
        char buf[24];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
        return String(buf);
    }

    uint32_t sec = millis() / 1000;
    uint32_t h = sec / 3600;
    uint32_t m = (sec % 3600) / 60;
    uint32_t s = sec % 60;
    char buf[24];
    snprintf(buf, sizeof(buf), "UP %02lu:%02lu:%02lu", static_cast<unsigned long>(h), static_cast<unsigned long>(m), static_cast<unsigned long>(s));
    return String(buf);
}

String authTypeLabel(SSHAuthType type) {
    switch (type) {
        case SSHAuthType::Password: return "Password";
        case SSHAuthType::PrivateKeyPath: return "Private Key Path";
        case SSHAuthType::PrivateKeyInline: return "Private Key Paste";
        default: return "Unknown";
    }
}

void loadProfile(SshProfile &p) {
    Preferences pref;
    pref.begin(kPrefNs, true);

    p.host = pref.getString(kPrefHost, "");
    p.port = static_cast<uint16_t>(pref.getUShort(kPrefPort, 22));
    p.user = pref.getString(kPrefUser, "");
    p.authType = static_cast<SSHAuthType>(pref.getUChar(kPrefAuthType, static_cast<uint8_t>(SSHAuthType::Password)));
    p.keyPath = pref.getString(kPrefKeyPath, "");
    p.saveCredentials = pref.getBool(kPrefSaveCreds, false);
    p.persistHistory = pref.getBool(kPrefPersistHistory, true);
    p.secureLog = pref.getBool(kPrefSecureLog, false);
    p.serialInput = pref.getBool(kPrefSerialInput, false);
    p.autoDisconnectSec = static_cast<uint16_t>(pref.getUShort(kPrefAutoDisc, 180));
    p.consentAccepted = pref.getBool(kPrefConsent, false);

    String encSecret = pref.getString(kPrefSecret, "");
    pref.end();

    p.secret = decryptFromStorage(encSecret);
}

void saveProfile(const SshProfile &p) {
    Preferences pref;
    pref.begin(kPrefNs, false);

    pref.putString(kPrefHost, p.host);
    pref.putUShort(kPrefPort, p.port);
    pref.putString(kPrefUser, p.user);
    pref.putUChar(kPrefAuthType, static_cast<uint8_t>(p.authType));
    pref.putString(kPrefKeyPath, p.keyPath);
    pref.putBool(kPrefSaveCreds, p.saveCredentials);
    pref.putBool(kPrefPersistHistory, p.persistHistory);
    pref.putBool(kPrefSecureLog, p.secureLog);
    pref.putBool(kPrefSerialInput, p.serialInput);
    pref.putUShort(kPrefAutoDisc, p.autoDisconnectSec);
    pref.putBool(kPrefConsent, p.consentAccepted);

    if (p.saveCredentials && p.secret.length() > 0) {
        pref.putString(kPrefSecret, encryptForStorage(p.secret));
    } else {
        pref.remove(kPrefSecret);
    }

    pref.end();
}

bool ensureConsentAccepted() {
    SshProfile p;
    loadProfile(p);
    if (p.consentAccepted) return true;

    int8_t choice = displayMessage(
        "SSH client for authorized\nremote admin only.\nNo brute-force allowed.",
        "Decline",
        nullptr,
        "I Consent",
        TFT_ORANGE
    );
    if (choice == 1) {
        p.consentAccepted = true;
        saveProfile(p);
        return true;
    }
    return false;
}

bool ensureWifiConnected() {
    if (WiFi.status() == WL_CONNECTED) return true;

    int8_t choice = displayMessage("WiFi is disconnected.\nTry known networks?", "No", nullptr, "Yes", bruceConfig.priColor);
    if (choice == 1) {
        wifiConnecttoKnownNet();
        if (WiFi.status() == WL_CONNECTED) return true;
    }

    choice = displayMessage("Open WiFi manager?", "No", nullptr, "Yes", bruceConfig.priColor);
    if (choice == 1) {
        wifiConnectMenu(WIFI_MODE_STA);
        if (WiFi.status() == WL_CONNECTED) return true;
    }

    displayError("No network connection", true);
    return false;
}

String toHiddenLabel(const String &prefix, const String &value) {
    if (value.length() == 0) return prefix + "(empty)";
    return prefix + "******";
}

String keyToBase64(String keyText) {
    String text = keyText;
    text.replace("\r", "\n");

    int begin = text.indexOf("-----BEGIN");
    int end = text.indexOf("-----END");
    if (begin >= 0 && end > begin) {
        String out;
        out.reserve(text.length());
        int lineStart = 0;
        while (lineStart < static_cast<int>(text.length())) {
            int nl = text.indexOf('\n', lineStart);
            if (nl < 0) nl = text.length();
            String line = text.substring(lineStart, nl);
            line.trim();
            if (!line.startsWith("-----") && line.length() > 0) out += line;
            lineStart = nl + 1;
        }
        return out;
    }

    text.replace("\n", "");
    text.replace(" ", "");
    text.trim();
    return text;
}

void appendLimited(String &dst, const char *data, int len) {
    if (len <= 0 || !data) return;
    if (dst.length() >= kMaxOutputCollect) return;

    size_t room = kMaxOutputCollect - dst.length();
    size_t toCopy = static_cast<size_t>(len);
    if (toCopy > room) toCopy = room;

    for (size_t i = 0; i < toCopy; ++i) dst += data[i];
}

bool appendHistoryEntry(
    const String &host,
    const String &kind,
    int exitCode,
    const String &payload,
    bool secureLog,
    bool persistHistory
) {
    if (!persistHistory) return true;
    if (!ensureSshDirs()) return false;

    File f = LittleFS.open(kHistoryPath, FILE_APPEND);
    if (!f) return false;

    String line;
    line.reserve(512);
    line += nowTimestamp();
    line += "|";
    line += sanitizeSingleLine(host, 64);
    line += "|";
    line += sanitizeSingleLine(kind, 8);
    line += "|";
    line += String(exitCode);
    line += "|";

    String cleanPayload = sanitizeSingleLine(payload, 240);
    if (secureLog) {
        line += "ENC:";
        line += encryptForStorage(cleanPayload);
    } else {
        line += "TXT:";
        line += cleanPayload;
    }

    f.println(line);
    f.close();
    return true;
}

bool openSessionLog(File &logFile, String &outPath) {
    if (!ensureSshDirs()) return false;

    outPath = String(kSessionDir) + "/session_" + String(millis()) + ".log";
    logFile = LittleFS.open(outPath, FILE_WRITE);
    return static_cast<bool>(logFile);
}

void writeSessionLog(File &logFile, const String &kind, const String &text, bool secureLog) {
    if (!logFile) return;
    String line = nowTimestamp() + "|" + sanitizeSingleLine(kind, 8) + "|";
    String clean = sanitizeSingleLine(text, 240);
    if (secureLog) line += "ENC:" + encryptForStorage(clean);
    else line += "TXT:" + clean;
    logFile.println(line);
}

bool parseHistoryLine(const String &line, HistoryEntryView &out) {
    int p1 = line.indexOf('|');
    if (p1 < 0) return false;
    int p2 = line.indexOf('|', p1 + 1);
    if (p2 < 0) return false;
    int p3 = line.indexOf('|', p2 + 1);
    if (p3 < 0) return false;
    int p4 = line.indexOf('|', p3 + 1);
    if (p4 < 0) return false;

    out.timestamp = line.substring(0, p1);
    out.host = line.substring(p1 + 1, p2);
    out.kind = line.substring(p2 + 1, p3);
    out.exitCode = line.substring(p3 + 1, p4).toInt();

    String payload = line.substring(p4 + 1);
    if (payload.startsWith("ENC:")) {
        out.payload = decryptFromStorage(payload.substring(4));
    } else if (payload.startsWith("TXT:")) {
        out.payload = payload.substring(4);
    } else {
        out.payload = payload;
    }

    return true;
}

size_t loadHistoryEntries(HistoryEntryView *entries, size_t maxEntries) {
    if (!entries || maxEntries == 0) return 0;
    if (!ensureSshDirs()) return 0;
    if (!LittleFS.exists(kHistoryPath)) return 0;

    File f = LittleFS.open(kHistoryPath, FILE_READ);
    if (!f) return 0;

    size_t count = 0;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        HistoryEntryView parsed;
        if (!parseHistoryLine(line, parsed)) continue;

        if (count < maxEntries) {
            entries[count++] = parsed;
        } else {
            for (size_t i = 1; i < maxEntries; ++i) entries[i - 1] = entries[i];
            entries[maxEntries - 1] = parsed;
        }
    }
    f.close();
    return count;
}

void renderHistoryPage(HistoryEntryView *entries, size_t count, int index) {
    drawMainBorderWithTitle("SSH History", true);
    tft.setCursor(10, STATUS_BAR_HEIGHT + 6);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(FP);

    if (count == 0) {
        tft.println("No history entries");
        tft.println("Sel/Prev: Back");
        return;
    }

    if (index < 0) index = 0;
    if (index >= static_cast<int>(count)) index = static_cast<int>(count - 1);

    const HistoryEntryView &e = entries[index];
    tft.println(String(index + 1) + "/" + String(count));
    tft.println(e.timestamp);
    tft.println(e.host);
    tft.println(e.kind + " exit=" + String(e.exitCode));
    tft.println("----");

    String payload = e.payload;
    int maxCharsPerLine = (tftWidth - 20) / (LW * FP);
    while (payload.length() > 0) {
        tft.println(payload.substring(0, maxCharsPerLine));
        payload = payload.substring(min(static_cast<int>(payload.length()), maxCharsPerLine));
        if (tft.getCursorY() > tftHeight - (LH * FP * 2)) break;
    }

    printCenterFootnote("Next/Prev Scroll | Sel Back");
}

void showHistoryView() {
    HistoryEntryView entries[kMaxHistoryView];
    size_t count = loadHistoryEntries(entries, kMaxHistoryView);
    int idx = (count == 0) ? 0 : static_cast<int>(count - 1);

    renderHistoryPage(entries, count, idx);
    while (true) {
        if (check(NextPress)) {
            if (count > 0 && idx < static_cast<int>(count - 1)) idx++;
            renderHistoryPage(entries, count, idx);
        }
        if (check(PrevPress)) {
            if (count > 0 && idx > 0) idx--;
            else if (count == 0) break;
            renderHistoryPage(entries, count, idx);
        }
        if (check(SelPress) || check(EscPress)) break;
        delay(20);
    }
}

bool copyHistoryToFile(FS &outFs, const String &destinationPath) {
    if (!ensureSshDirs()) return false;
    File in = LittleFS.open(kHistoryPath, FILE_READ);
    if (!in) return false;

    String folder = destinationPath.substring(0, destinationPath.lastIndexOf('/'));
    if (folder.length() > 0 && !outFs.exists(folder)) outFs.mkdir(folder);

    if (outFs.exists(destinationPath)) outFs.remove(destinationPath);

    File out = outFs.open(destinationPath, FILE_WRITE);
    if (!out) {
        in.close();
        return false;
    }

    while (in.available()) {
        String line = in.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        HistoryEntryView parsed;
        if (!parseHistoryLine(line, parsed)) continue;

        String plain = parsed.timestamp + " | " + parsed.host + " | " + parsed.kind + " | exit=" +
                       String(parsed.exitCode) + " | " + parsed.payload;
        out.println(plain);
    }

    out.close();
    in.close();
    return true;
}

void wipeLogsAndHistory() {
    if (!ensureSshDirs()) return;

    if (LittleFS.exists(kHistoryPath)) LittleFS.remove(kHistoryPath);

    File dir = LittleFS.open(kSessionDir);
    if (dir && dir.isDirectory()) {
        bool isDir = false;
        String path = dir.getNextFileName(&isDir);
        while (path.length() > 0) {
            if (!isDir && path.startsWith(String(kSessionDir) + "/")) LittleFS.remove(path);
            path = dir.getNextFileName(&isDir);
        }
    }
    if (dir) dir.close();
}

void drawConsole(
    const String *lines,
    int lineCount,
    int scrollOffset,
    const String &title,
    const String &footer,
    const String &sessionInfo
) {
    drawMainBorderWithTitle(title, true);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(FP);
    tft.setCursor(10, STATUS_BAR_HEIGHT + 4);
    tft.println(sessionInfo);

    int lineHeight = LH * FP;
    int y0 = STATUS_BAR_HEIGHT + 4 + lineHeight;
    int visibleRows = max(1, (tftHeight - y0 - lineHeight * 2) / lineHeight);

    int start = max(0, lineCount - visibleRows - scrollOffset);
    int end = min(lineCount, start + visibleRows);

    int y = y0;
    for (int i = start; i < end; ++i) {
        tft.setCursor(10, y);
        tft.println(lines[i]);
        y += lineHeight;
    }

    tft.setTextColor(getComplementaryColor2(bruceConfig.bgColor), bruceConfig.bgColor);
    tft.setCursor(10, tftHeight - lineHeight * 2);
    tft.println(footer);
    if (scrollOffset > 0) {
        tft.setCursor(10, tftHeight - lineHeight);
        tft.println("Scroll offset: " + String(scrollOffset));
    }
}

void pushConsoleLine(String *lines, int &lineCount, const String &text) {
    String clean = sanitizeSingleLine(text, 120);
    if (clean.length() == 0) return;

    if (lineCount < static_cast<int>(kMaxConsoleLines)) {
        lines[lineCount++] = clean;
    } else {
        for (size_t i = 1; i < kMaxConsoleLines; ++i) lines[i - 1] = lines[i];
        lines[kMaxConsoleLines - 1] = clean;
    }
}

SSHAuth profileToAuth(const SshProfile &p) {
    SSHAuth auth;
    auth.type = p.authType;
    if (p.authType == SSHAuthType::Password) auth.password = p.secret;
    else if (p.authType == SSHAuthType::PrivateKeyPath) auth.keyPath = p.keyPath;
    else auth.privateKey = p.secret;
    return auth;
}

void chooseAuthMode(SshProfile &p) {
    int8_t choice = displayMessage("Auth Type", "Pwd", "Path", "Paste", bruceConfig.priColor);
    if (choice == 0) {
        p.authType = SSHAuthType::Password;
        String pwd = keyboard(p.secret, 96, "SSH Password", true);
        if (pwd != "\x1B") p.secret = pwd;
    } else if (choice == 1) {
        p.authType = SSHAuthType::PrivateKeyPath;
        String path = keyboard(p.keyPath, 128, "SSH Key Path");
        if (path != "\x1B") p.keyPath = path;
    } else {
        p.authType = SSHAuthType::PrivateKeyInline;
        String keyText = keyboard(p.secret, 512, "Paste Key/Base64");
        if (keyText != "\x1B") p.secret = keyText;
    }
}

bool runConnectConsentConfirm(const SshProfile &p) {
    String summary = "Connect to\n" + p.host + ":" + String(p.port) + "\nas " + p.user;
    int8_t choice = displayMessage(summary.c_str(), "Cancel", nullptr, "Connect", bruceConfig.priColor);
    return choice == 1;
}

void applyProfileRuntime(const SshProfile &p) {
    g_runtime.persistHistory = p.persistHistory;
    g_runtime.secureLog = p.secureLog;
    g_runtime.serialInput = p.serialInput;
    g_runtime.autoDisconnectSec = p.autoDisconnectSec;
}

void showSessionSummary(const SSHSession &session, const String &logPath) {
    drawMainBorderWithTitle("SSH Connected", true);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(FP);
    tft.setCursor(10, STATUS_BAR_HEIGHT + 8);
    tft.println("Host: " + session.host + ":" + String(session.port));
    tft.println("User: " + session.user);
    tft.println("Log: " + logPath);
    tft.println("Sel: terminal");
    tft.println("Prev/Esc: disconnect");

    while (true) {
        if (check(SelPress)) break;
        if (check(PrevPress) || check(EscPress)) return;
        delay(20);
    }
}

void runSelfTest(SshProfile &p) {
    if (!ensureWifiConnected()) return;

    if (p.host.length() == 0 || p.user.length() == 0) {
        displayError("Set host/user first", true);
        return;
    }

    if (!runConnectConsentConfirm(p)) return;

    applyProfileRuntime(p);

    SSHSession session;
    SSHAuth auth = profileToAuth(p);
    String err;
    if (!ssh_connect(p.host, p.port, p.user, auth, session, &err)) {
        if (err.length() == 0) err = "Connection failed";
        displayError(err, true);
        return;
    }

    SSHCommandResult result;
    if (!ssh_run_command(session, "echo ssh_test_ok", result, 15000, &err)) {
        if (err.length() == 0) err = "Test command failed";
        displayError(err, true);
        ssh_disconnect(session);
        return;
    }

    ssh_disconnect(session);
    String preview = result.stdoutText;
    preview.trim();
    if (preview.length() == 0) preview = "(no stdout)";

    drawMainBorderWithTitle("SSH Test Result", true);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(FP);
    tft.setCursor(10, STATUS_BAR_HEIGHT + 8);
    tft.println("exit=" + String(result.exitCode));
    tft.println(sanitizeSingleLine(preview, 80));
    printCenterFootnote("Press any key");
    while (!check(AnyKeyPress)) delay(20);
}

enum MenuItemId : uint8_t {
    MI_CONNECT = 0,
    MI_HOST,
    MI_PORT,
    MI_USER,
    MI_AUTH_MODE,
    MI_AUTH_SECRET,
    MI_SAVE_CREDS,
    MI_PERSIST_HISTORY,
    MI_SECURE_LOGS,
    MI_SERIAL_INPUT,
    MI_AUTO_DISC,
    MI_PRESET_TERMUX,
    MI_VIEW_HISTORY,
    MI_EXPORT_HISTORY,
    MI_WIPE_LOGS,
    MI_SAVE_PROFILE,
    MI_SELF_TEST,
    MI_BACK,
};

int buildMenuEntries(const SshProfile &profile, String *labels, uint8_t *ids, int maxEntries) {
    int c = 0;
    if (c < maxEntries) {
        ids[c] = MI_CONNECT;
        labels[c++] = "Connect & Open Terminal";
    }
    if (c < maxEntries) {
        ids[c] = MI_HOST;
        labels[c++] = "Host: " + (profile.host.length() ? profile.host : "(unset)");
    }
    if (c < maxEntries) {
        ids[c] = MI_PORT;
        labels[c++] = "Port: " + String(profile.port);
    }
    if (c < maxEntries) {
        ids[c] = MI_USER;
        labels[c++] = "User: " + (profile.user.length() ? profile.user : "(unset)");
    }
    if (c < maxEntries) {
        ids[c] = MI_AUTH_MODE;
        labels[c++] = "Auth Mode: " + authTypeLabel(profile.authType);
    }
    if (c < maxEntries) {
        ids[c] = MI_AUTH_SECRET;
        if (profile.authType == SSHAuthType::Password) {
            labels[c++] = toHiddenLabel("Password: ", profile.secret);
        } else if (profile.authType == SSHAuthType::PrivateKeyPath) {
            labels[c++] = "Key Path: " + (profile.keyPath.length() ? profile.keyPath : "(unset)");
        } else {
            labels[c++] = toHiddenLabel("Inline Key: ", profile.secret);
        }
    }
    if (c < maxEntries) {
        ids[c] = MI_SAVE_CREDS;
        labels[c++] = String("Save Credentials: ") + (profile.saveCredentials ? "ON" : "OFF");
    }
    if (c < maxEntries) {
        ids[c] = MI_PERSIST_HISTORY;
        labels[c++] = String("Persist History: ") + (profile.persistHistory ? "ON" : "OFF");
    }
    if (c < maxEntries) {
        ids[c] = MI_SECURE_LOGS;
        labels[c++] = String("Secure Logs: ") + (profile.secureLog ? "ON" : "OFF");
    }
    if (c < maxEntries) {
        ids[c] = MI_SERIAL_INPUT;
        labels[c++] = String("Serial Input: ") + (profile.serialInput ? "ON" : "OFF");
    }
    if (c < maxEntries) {
        ids[c] = MI_AUTO_DISC;
        labels[c++] = "Auto Disconnect(s): " + String(profile.autoDisconnectSec);
    }
    if (c < maxEntries) {
        ids[c] = MI_PRESET_TERMUX;
        labels[c++] = "Preset UGPhone/Termux";
    }
    if (c < maxEntries) {
        ids[c] = MI_VIEW_HISTORY;
        labels[c++] = "View Command History";
    }
    if (c < maxEntries) {
        ids[c] = MI_EXPORT_HISTORY;
        labels[c++] = "Export History";
    }
    if (c < maxEntries) {
        ids[c] = MI_WIPE_LOGS;
        labels[c++] = "Wipe Logs & History";
    }
    if (c < maxEntries) {
        ids[c] = MI_SAVE_PROFILE;
        labels[c++] = "Save Profile";
    }
    if (c < maxEntries) {
        ids[c] = MI_SELF_TEST;
        labels[c++] = "Run Test Harness";
    }
    if (c < maxEntries) {
        ids[c] = MI_BACK;
        labels[c++] = "Back";
    }
    return c;
}

void drawConfigMenu(const String *labels, int count, int selected) {
    drawMainBorderWithTitle("SSH Client", true);
    tft.setTextSize(FP);
    int lineHeight = LH * FP;
    int visible = max(1, (tftHeight - STATUS_BAR_HEIGHT - lineHeight * 2) / lineHeight);
    int start = max(0, selected - visible / 2);
    if (start + visible > count) start = max(0, count - visible);

    int y = STATUS_BAR_HEIGHT + 4;
    for (int i = 0; i < visible && (start + i) < count; ++i) {
        int idx = start + i;
        bool isSel = (idx == selected);
        tft.setCursor(8, y);
        if (isSel) tft.setTextColor(TFT_BLACK, bruceConfig.priColor);
        else tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.print(isSel ? "> " : "  ");
        tft.println(labels[idx]);
        y += lineHeight;
    }

    tft.setTextColor(getComplementaryColor2(bruceConfig.bgColor), bruceConfig.bgColor);
    tft.setCursor(8, tftHeight - lineHeight);
    tft.println("Next/Prev Move | Sel OK | Esc Back");
}

void connectAndRunTerminal(SshProfile &profile) {
    if (!ensureWifiConnected()) return;
    if (profile.host.length() == 0 || profile.user.length() == 0) {
        displayError("Set host and user first", true);
        return;
    }
    if (!runConnectConsentConfirm(profile)) return;

    applyProfileRuntime(profile);

    SSHSession session;
    SSHAuth auth = profileToAuth(profile);
    String err;
    if (!ssh_connect(profile.host, profile.port, profile.user, auth, session, &err)) {
        if (err.length() == 0) err = "SSH connection failed";
        displayError(err, true);
        return;
    }

    if (profile.saveCredentials) saveProfile(profile);

    File logFile;
    String logPath;
    if (openSessionLog(logFile, logPath)) {
        writeSessionLog(logFile, "INFO", "Connected", profile.secureLog);
        logFile.close();
    }

    showSessionSummary(session, logPath);
    ssh_interactive(session);
    ssh_disconnect(session);
}

void editAuthSecret(SshProfile &profile) {
    if (profile.authType == SSHAuthType::Password) {
        String pwd = keyboard(profile.secret, 96, "SSH Password", true);
        if (pwd != "\x1B") profile.secret = pwd;
    } else if (profile.authType == SSHAuthType::PrivateKeyPath) {
        String kp = keyboard(profile.keyPath, 128, "SSH Key Path");
        if (kp != "\x1B") {
            kp.trim();
            profile.keyPath = kp;
        }
    } else {
        String keyText = keyboard(profile.secret, 512, "Paste Key/Base64");
        if (keyText != "\x1B") profile.secret = keyText;
    }
}

bool handleMenuSelection(uint8_t id, SshProfile &profile) {
    switch (id) {
        case MI_CONNECT: connectAndRunTerminal(profile); return false;
        case MI_HOST: {
            String host = keyboard(profile.host, 128, "SSH Host/IP");
            if (host != "\x1B") {
                host.trim();
                profile.host = host;
            }
            return false;
        }
        case MI_PORT: {
            String portText = num_keyboard(String(profile.port), 5, "SSH Port");
            if (portText != "\x1B") {
                int p = portText.toInt();
                if (p > 0 && p <= 65535) profile.port = static_cast<uint16_t>(p);
            }
            return false;
        }
        case MI_USER: {
            String user = keyboard(profile.user, 64, "SSH User");
            if (user != "\x1B") {
                user.trim();
                profile.user = user;
            }
            return false;
        }
        case MI_AUTH_MODE: chooseAuthMode(profile); return false;
        case MI_AUTH_SECRET: editAuthSecret(profile); return false;
        case MI_SAVE_CREDS:
            profile.saveCredentials = !profile.saveCredentials;
            if (!profile.saveCredentials) profile.secret = "";
            return false;
        case MI_PERSIST_HISTORY: profile.persistHistory = !profile.persistHistory; return false;
        case MI_SECURE_LOGS: profile.secureLog = !profile.secureLog; return false;
        case MI_SERIAL_INPUT: profile.serialInput = !profile.serialInput; return false;
        case MI_AUTO_DISC: {
            String sec = num_keyboard(String(profile.autoDisconnectSec), 5, "Timeout Sec (0=off)");
            if (sec != "\x1B") {
                int v = sec.toInt();
                if (v >= 0 && v <= 3600) profile.autoDisconnectSec = static_cast<uint16_t>(v);
            }
            return false;
        }
        case MI_PRESET_TERMUX: {
            profile.port = 8022;
            String host = keyboard(profile.host, 128, "Termux Host/IP");
            if (host != "\x1B") {
                host.trim();
                profile.host = host;
            }
            String user = keyboard(profile.user, 64, "Termux Username");
            if (user != "\x1B") {
                user.trim();
                profile.user = user;
            }
            return false;
        }
        case MI_VIEW_HISTORY: showHistoryView(); return false;
        case MI_EXPORT_HISTORY:
            if (export_history()) displaySuccess("Exported to /ssh/history_export.txt", true);
            else displayError("History export failed", true);
            return false;
        case MI_WIPE_LOGS: {
            int8_t choice = displayMessage("Delete SSH logs/history?", "No", nullptr, "Yes", TFT_RED);
            if (choice == 1) {
                wipeLogsAndHistory();
                displaySuccess("SSH logs removed", true);
            }
            return false;
        }
        case MI_SAVE_PROFILE:
            saveProfile(profile);
            displaySuccess("Profile saved", true);
            return false;
        case MI_SELF_TEST: runSelfTest(profile); return false;
        case MI_BACK: return true;
        default: return false;
    }
}

} // namespace

bool ssh_connect(const String &host, uint16_t port, const String &user, const SSHAuth &auth, SSHSession &outSession) {
    return ssh_connect(host, port, user, auth, outSession, nullptr);
}

bool ssh_connect(
    const String &host,
    uint16_t port,
    const String &user,
    const SSHAuth &auth,
    SSHSession &outSession,
    String *errorOut
) {
    outSession = SSHSession{};

    if (host.length() == 0) {
        if (errorOut) *errorOut = "Host is empty";
        return false;
    }
    if (user.length() == 0) {
        if (errorOut) *errorOut = "Username is empty";
        return false;
    }
    if (port == 0) {
        if (errorOut) *errorOut = "Invalid port";
        return false;
    }

#ifndef LITE_VERSION
    ssh_session raw = ::ssh_new();
    if (!raw) {
        if (errorOut) *errorOut = "SSH session allocation failed";
        return false;
    }

    uint16_t usePort = port;
    ::ssh_options_set(raw, SSH_OPTIONS_HOST, host.c_str());
    ::ssh_options_set(raw, SSH_OPTIONS_PORT, &usePort);
    ::ssh_options_set(raw, SSH_OPTIONS_USER, user.c_str());

    int rc = ::ssh_connect(raw);
    if (rc != SSH_OK) {
        if (errorOut) *errorOut = String("SSH connect failed: ") + ::ssh_get_error(raw);
        ::ssh_free(raw);
        return false;
    }

    bool authOk = false;
    if (auth.type == SSHAuthType::Password) {
        if (auth.password.length() == 0) {
            if (errorOut) *errorOut = "Password is empty";
        } else if (::ssh_userauth_password(raw, nullptr, auth.password.c_str()) == SSH_AUTH_SUCCESS) {
            authOk = true;
        } else if (errorOut) {
            *errorOut = String("Password auth failed: ") + ::ssh_get_error(raw);
        }
    } else if (auth.type == SSHAuthType::PrivateKeyPath) {
        if (auth.keyPath.length() == 0) {
            if (errorOut) *errorOut = "Private key path is empty";
        } else {
            ::ssh_options_set(raw, SSH_OPTIONS_ADD_IDENTITY, auth.keyPath.c_str());
            if (::ssh_userauth_publickey_auto(raw, nullptr, nullptr) == SSH_AUTH_SUCCESS) {
                authOk = true;
            } else {
                ssh_key key = nullptr;
                rc = ::ssh_pki_import_privkey_file(auth.keyPath.c_str(), nullptr, nullptr, nullptr, &key);
                if (rc == SSH_OK && key) {
                    if (::ssh_userauth_publickey(raw, nullptr, key) == SSH_AUTH_SUCCESS) authOk = true;
                }
                if (key) ::ssh_key_free(key);
                if (!authOk && errorOut) *errorOut = String("Key auth failed: ") + ::ssh_get_error(raw);
            }
        }
    } else {
        if (auth.privateKey.length() == 0) {
            if (errorOut) *errorOut = "Private key text is empty";
        } else {
            String b64 = keyToBase64(auth.privateKey);
            ssh_key key = nullptr;
            rc = ::ssh_pki_import_privkey_base64(b64.c_str(), nullptr, nullptr, nullptr, &key);
            if (rc == SSH_OK && key) {
                if (::ssh_userauth_publickey(raw, nullptr, key) == SSH_AUTH_SUCCESS) authOk = true;
            }
            if (key) ::ssh_key_free(key);
            if (!authOk && errorOut) *errorOut = String("Inline key auth failed: ") + ::ssh_get_error(raw);
        }
    }

    if (!authOk) {
        ::ssh_disconnect(raw);
        ::ssh_free(raw);
        return false;
    }

    auto *backend = new SSHBackendHandle();
    if (!backend) {
        if (errorOut) *errorOut = "Out of memory";
        ::ssh_disconnect(raw);
        ::ssh_free(raw);
        return false;
    }

    backend->session = raw;
    outSession.connected = true;
    outSession.host = host;
    outSession.port = port;
    outSession.user = user;
    outSession.connectedAtMs = millis();
    outSession.backendHandle = backend;
    return true;
#else
    (void)auth;
    if (errorOut) {
        *errorOut = "SSH backend unavailable in LITE build. Enable LibSSH-ESP32 in PlatformIO.";
    }
    return false;
#endif
}

bool ssh_run_command(SSHSession &session, const String &cmd, SSHCommandResult &outResult) {
    return ssh_run_command(session, cmd, outResult, kDefaultCmdTimeoutMs, nullptr);
}

bool ssh_run_command(
    SSHSession &session,
    const String &cmd,
    SSHCommandResult &outResult,
    uint32_t timeoutMs,
    String *errorOut
) {
    outResult = SSHCommandResult{};

#ifndef LITE_VERSION
    if (!session.connected || !session.backendHandle) {
        if (errorOut) *errorOut = "SSH session is not connected";
        return false;
    }

    if (cmd.length() == 0) {
        if (errorOut) *errorOut = "Empty command";
        return false;
    }

    auto *backend = reinterpret_cast<SSHBackendHandle *>(session.backendHandle);
    if (!backend || !backend->session) {
        if (errorOut) *errorOut = "SSH backend handle invalid";
        return false;
    }

    ssh_channel channel = ::ssh_channel_new(backend->session);
    if (!channel) {
        if (errorOut) *errorOut = "Failed to allocate SSH channel";
        return false;
    }

    bool ok = false;
    do {
        if (::ssh_channel_open_session(channel) != SSH_OK) {
            if (errorOut) *errorOut = String("Channel open failed: ") + ::ssh_get_error(backend->session);
            break;
        }

        if (::ssh_channel_request_exec(channel, cmd.c_str()) != SSH_OK) {
            if (errorOut) *errorOut = String("Exec request failed: ") + ::ssh_get_error(backend->session);
            break;
        }

        uint32_t started = millis();
        char buffer[256];

        while (true) {
            int nOut = ::ssh_channel_read_nonblocking(channel, buffer, sizeof(buffer), 0);
            if (nOut > 0) {
                appendLimited(outResult.stdoutText, buffer, nOut);
                started = millis();
            }

            int nErr = ::ssh_channel_read_nonblocking(channel, buffer, sizeof(buffer), 1);
            if (nErr > 0) {
                appendLimited(outResult.stderrText, buffer, nErr);
                started = millis();
            }

            if (nOut < 0 || nErr < 0) {
                if (errorOut) *errorOut = "Channel read failed";
                break;
            }

            if (::ssh_channel_is_eof(channel) || ::ssh_channel_is_closed(channel)) {
                ok = true;
                break;
            }

            if ((nOut == 0 && nErr == 0) && (millis() - started > timeoutMs)) {
                if (errorOut) *errorOut = "Command timeout";
                break;
            }

            delay(10);
        }
    } while (false);

    outResult.exitCode = ::ssh_channel_get_exit_status(channel);

    ::ssh_channel_send_eof(channel);
    ::ssh_channel_close(channel);
    ::ssh_channel_free(channel);

    appendHistoryEntry(session.host, "CMD", outResult.exitCode, cmd, g_runtime.secureLog, g_runtime.persistHistory);

    String preview = outResult.stdoutText.length() ? outResult.stdoutText : outResult.stderrText;
    appendHistoryEntry(session.host, "OUT", outResult.exitCode, preview, g_runtime.secureLog, g_runtime.persistHistory);

    return ok;
#else
    (void)session;
    (void)cmd;
    (void)timeoutMs;
    if (errorOut) *errorOut = "SSH backend unavailable in LITE build";
    return false;
#endif
}

bool ssh_interactive(SSHSession &session) {
#ifndef LITE_VERSION
    if (!session.connected || !session.backendHandle) {
        displayError("SSH session not connected", true);
        return false;
    }

    auto *backend = reinterpret_cast<SSHBackendHandle *>(session.backendHandle);
    if (!backend || !backend->session) {
        displayError("Invalid backend handle", true);
        return false;
    }

    ssh_channel channel = ::ssh_channel_new(backend->session);
    if (!channel) {
        displayError("Cannot create channel", true);
        return false;
    }

    if (::ssh_channel_open_session(channel) != SSH_OK) {
        ::ssh_channel_free(channel);
        displayError("Cannot open channel", true);
        return false;
    }

    if (::ssh_channel_request_pty(channel) != SSH_OK) {
        ::ssh_channel_close(channel);
        ::ssh_channel_free(channel);
        displayError("PTY request failed", true);
        return false;
    }

    if (::ssh_channel_request_shell(channel) != SSH_OK) {
        ::ssh_channel_close(channel);
        ::ssh_channel_free(channel);
        displayError("Shell request failed", true);
        return false;
    }

    String lines[kMaxConsoleLines];
    int lineCount = 0;
    int scrollOffset = 0;
    String partialLine;

    File logFile;
    String logPath;
    bool hasLog = openSessionLog(logFile, logPath);
    if (hasLog) writeSessionLog(logFile, "INFO", "Session opened", g_runtime.secureLog);

    uint32_t lastActivity = millis();
    bool redraw = true;

    auto onOutputChunk = [&](const char *buf, int n, const char *kind) {
        if (!buf || n <= 0) return;
        String chunk;
        chunk.reserve(n + 8);
        if (kind && kind[0] == 'E') chunk += "[err] ";
        for (int i = 0; i < n; ++i) chunk += buf[i];

        if (hasLog) writeSessionLog(logFile, kind ? kind : "OUT", chunk, g_runtime.secureLog);

        for (int i = 0; i < static_cast<int>(chunk.length()); ++i) {
            char c = chunk[i];
            if (c == '\r') continue;
            if (c == '\n') {
                pushConsoleLine(lines, lineCount, partialLine);
                partialLine = "";
            } else {
                partialLine += c;
                if (partialLine.length() >= 120) {
                    pushConsoleLine(lines, lineCount, partialLine);
                    partialLine = "";
                }
            }
        }
        redraw = true;
    };

    while (true) {
        if (check(EscPress) || check(PrevPress)) break;

        if (g_runtime.autoDisconnectSec > 0 &&
            (millis() - lastActivity) > (static_cast<uint32_t>(g_runtime.autoDisconnectSec) * 1000UL)) {
            pushConsoleLine(lines, lineCount, "[info] Inactivity timeout, disconnecting");
            if (hasLog) writeSessionLog(logFile, "INFO", "Inactivity timeout", g_runtime.secureLog);
            redraw = true;
            break;
        }

        if (check(NextPress)) {
            int maxOffset = max(0, lineCount - 1);
            scrollOffset = min(maxOffset, scrollOffset + 1);
            redraw = true;
            lastActivity = millis();
        }

        if (g_runtime.serialInput && serialDevice && serialDevice->available() > 0) {
            String serialCmd = serialDevice->readStringUntil('\n');
            serialCmd.trim();
            if (serialCmd.length() > 0) {
                String message = serialCmd + "\n";
                ::ssh_channel_write(channel, message.c_str(), message.length());
                pushConsoleLine(lines, lineCount, "> " + serialCmd);
                appendHistoryEntry(
                    session.host, "CMD", 0, serialCmd, g_runtime.secureLog, g_runtime.persistHistory
                );
                if (hasLog) writeSessionLog(logFile, "CMD", serialCmd, g_runtime.secureLog);
                redraw = true;
                lastActivity = millis();
            }
        }

        if (check(SelPress)) {
            String cmd = keyboard("", 96, "SSH Command");
            cmd.trim();
            if (cmd.length() > 0 && cmd != "\x1B") {
                String message = cmd + "\n";
                ::ssh_channel_write(channel, message.c_str(), message.length());
                pushConsoleLine(lines, lineCount, "> " + cmd);
                appendHistoryEntry(session.host, "CMD", 0, cmd, g_runtime.secureLog, g_runtime.persistHistory);
                if (hasLog) writeSessionLog(logFile, "CMD", cmd, g_runtime.secureLog);
                redraw = true;
                lastActivity = millis();
            }
        }

        char outBuf[192];
        int nOut = ::ssh_channel_read_nonblocking(channel, outBuf, sizeof(outBuf), 0);
        if (nOut > 0) {
            onOutputChunk(outBuf, nOut, "OUT");
            appendHistoryEntry(
                session.host,
                "OUT",
                0,
                String(outBuf, nOut),
                g_runtime.secureLog,
                g_runtime.persistHistory
            );
            lastActivity = millis();
        }

        int nErr = ::ssh_channel_read_nonblocking(channel, outBuf, sizeof(outBuf), 1);
        if (nErr > 0) {
            onOutputChunk(outBuf, nErr, "ERR");
            appendHistoryEntry(
                session.host,
                "ERR",
                0,
                String(outBuf, nErr),
                g_runtime.secureLog,
                g_runtime.persistHistory
            );
            lastActivity = millis();
        }

        if (nOut < 0 || nErr < 0 || ::ssh_channel_is_closed(channel)) {
            pushConsoleLine(lines, lineCount, "[info] Remote channel closed");
            redraw = true;
            break;
        }

        if (redraw) {
            String info = session.user + "@" + session.host + ":" + String(session.port);
            drawConsole(
                lines,
                lineCount,
                scrollOffset,
                "SSH Terminal",
                "Sel Cmd | Prev Exit | Next Scroll",
                info
            );
            redraw = false;
        }

        delay(15);
    }

    if (partialLine.length() > 0) pushConsoleLine(lines, lineCount, partialLine);

    if (hasLog) {
        writeSessionLog(logFile, "INFO", "Session closed", g_runtime.secureLog);
        logFile.close();
    }

    ::ssh_channel_send_eof(channel);
    ::ssh_channel_close(channel);
    ::ssh_channel_free(channel);

    return true;
#else
    (void)session;
    displayError("SSH backend unavailable in LITE build", true);
    return false;
#endif
}

void ssh_disconnect(SSHSession &session) {
#ifndef LITE_VERSION
    if (session.backendHandle) {
        auto *backend = reinterpret_cast<SSHBackendHandle *>(session.backendHandle);
        if (backend->session) {
            ::ssh_disconnect(backend->session);
            ::ssh_free(backend->session);
            backend->session = nullptr;
        }
        delete backend;
        session.backendHandle = nullptr;
    }
#endif
    session.connected = false;
}

bool export_history() {
    FS *fs = nullptr;
    if (!getFsStorage(fs) || !fs) return false;
    return copyHistoryToFile(*fs, kExportDefaultPath);
}

bool export_history(const String &destinationPath) {
    if (destinationPath.length() == 0) return export_history();

    if (!LittleFS.begin(true)) return false;

    // Try LittleFS destination first.
    if (destinationPath.startsWith("/")) {
        if (copyHistoryToFile(LittleFS, destinationPath)) return true;
    }

    // Fallback to currently mounted storage.
    FS *fs = nullptr;
    if (!getFsStorage(fs) || !fs) return false;
    return copyHistoryToFile(*fs, destinationPath);
}

void ssh_client_menu() {
    if (!ensureConsentAccepted()) {
        displayWarning("Consent required", true);
        return;
    }

    SshProfile profile;
    loadProfile(profile);

    constexpr int kMaxMenuEntries = 24;
    String labels[kMaxMenuEntries];
    uint8_t ids[kMaxMenuEntries] = {0};
    int selected = 0;
    bool exitMenu = false;

    while (!exitMenu) {
        int count = buildMenuEntries(profile, labels, ids, kMaxMenuEntries);
        if (count <= 0) return;
        if (selected >= count) selected = count - 1;
        if (selected < 0) selected = 0;

        drawConfigMenu(labels, count, selected);

        while (true) {
            if (check(NextPress)) {
                selected = (selected + 1) % count;
                drawConfigMenu(labels, count, selected);
            }
            if (check(PrevPress)) {
                selected = (selected - 1 + count) % count;
                drawConfigMenu(labels, count, selected);
            }
            if (check(EscPress)) {
                saveProfile(profile);
                return;
            }
            if (check(SelPress)) {
                exitMenu = handleMenuSelection(ids[selected], profile);
                break;
            }
            delay(20);
        }
    }
    saveProfile(profile);
}
