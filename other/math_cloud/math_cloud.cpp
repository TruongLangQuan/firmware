#include "math_cloud.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/sd_functions.h"
#include "core/scrollableTextArea.h"
#include "core/wifi/wifi_common.h"
#include "modules/others/idk_vi_font.h"
#include <globals.h>

namespace {
struct MathJob {
    String expr;
    String result;
    String error;
    volatile bool done = false;
};

struct DownloadJob {
    String url;
    String path;
    volatile bool done = false;
    bool ok = false;
    String error;
};

static TaskHandle_t g_mathTask = nullptr;
static MathJob *g_mathJob = nullptr;
static TaskHandle_t g_dlTask = nullptr;
static DownloadJob *g_dlJob = nullptr;

void mathTask(void *param) {
    MathJob *job = static_cast<MathJob *>(param);
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(8000);

    String url = "https://api.mathjs.org/v4/";
    if (!http.begin(client, url)) {
        job->error = "HTTP begin failed";
        job->done = true;
        g_mathTask = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    StaticJsonDocument<256> reqDoc;
    reqDoc["expr"] = job->expr;
    String body;
    serializeJson(reqDoc, body);

    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    String payload = (code > 0) ? http.getString() : "";
    http.end();

    if (code <= 0) {
        job->error = "HTTP error";
    } else {
        payload.trim();
        if (payload.startsWith("{")) {
            StaticJsonDocument<512> respDoc;
            DeserializationError err = deserializeJson(respDoc, payload);
            if (!err) {
                if (respDoc.containsKey("error") && !respDoc["error"].isNull()) {
                    String err = respDoc["error"].as<String>();
                    err.trim();
                    if (err.length() && err != "null") job->error = err;
                }
                if (!job->error.length()) {
                    if (respDoc.containsKey("result") && !respDoc["result"].isNull()) {
                        job->result = respDoc["result"].as<String>();
                    } else {
                        job->result = payload;
                    }
                }
            } else {
                job->result = payload;
            }
        } else {
            job->result = payload;
        }
    }

    job->done = true;
    g_mathTask = nullptr;
    vTaskDelete(nullptr);
}

void downloadTask(void *param) {
    DownloadJob *job = static_cast<DownloadJob *>(param);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(10000);
    if (!http.begin(client, job->url)) {
        job->error = "HTTP begin failed";
        job->done = true;
        g_dlTask = nullptr;
        vTaskDelete(nullptr);
        return;
    }
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        job->error = "HTTP " + String(code);
        http.end();
        job->done = true;
        g_dlTask = nullptr;
        vTaskDelete(nullptr);
        return;
    }
    int total = http.getSize();
    if (total > (300 * 1024)) {
        job->error = "Image too large";
        http.end();
        job->done = true;
        g_dlTask = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    FS *fs = sdcardMounted ? static_cast<FS *>(&SD) : static_cast<FS *>(&LittleFS);
    File f = fs->open(job->path, FILE_WRITE);
    if (!f) {
        job->error = "FS write failed";
        http.end();
        job->done = true;
        g_dlTask = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    WiFiClient *stream = http.getStreamPtr();
    uint8_t buf[1024];
    while (http.connected() && (total > 0 || total == -1)) {
        size_t avail = stream->available();
        if (avail) {
            int toRead = (avail > sizeof(buf)) ? sizeof(buf) : (int)avail;
            int len = stream->readBytes(buf, toRead);
            if (len > 0) {
                f.write(buf, len);
                if (total > 0) total -= len;
            }
        } else {
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
    f.close();
    http.end();

    job->ok = true;
    job->done = true;
    g_dlTask = nullptr;
    vTaskDelete(nullptr);
}

void showBusy(const char *msg) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(FP);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(8, 20);
    tft.print(msg);
}

void showResult(const String &title, const String &text) {
    idk_vi_font_enable();
    ScrollableTextArea area(FP, 0, 0, tftWidth, tftHeight, false, false);
    area.rebuildLayout();
    area.addLine(title);
    area.addLine("----------------");
    area.fromString(text);
    area.show();
    idk_vi_font_disable();
}

void ensureStorageReady() {
    if (!sdcardMounted) {
        setupSdCard();
    }
    if (!LittleFS.begin(true)) {
        LittleFS.begin(true);
    }
}

void geometryLocal(const char *label) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(4, 4);
    tft.print(label);

    int cx = tftWidth / 2;
    int cy = tftHeight / 2;
    int r = min(tftWidth, tftHeight) / 4;

    if (strcmp(label, "Circle") == 0) {
        tft.drawCircle(cx, cy, r, TFT_CYAN);
        tft.drawCircle(cx, cy, r - 1, TFT_CYAN);
    } else if (strcmp(label, "Triangle") == 0) {
        tft.drawTriangle(cx, cy - r, cx - r, cy + r, cx + r, cy + r, TFT_YELLOW);
    } else if (strcmp(label, "Square") == 0) {
        tft.drawRect(cx - r, cy - r, r * 2, r * 2, TFT_GREEN);
    }

    while (!check(AnyKeyPress)) { vTaskDelay(10 / portTICK_PERIOD_MS); }
}

void geometryOnline(const String &name, const String &url) {
    ensureStorageReady();
    FS *fs = sdcardMounted ? static_cast<FS *>(&SD) : static_cast<FS *>(&LittleFS);
    String base = "/math_cloud";
    if (!fs->exists(base)) fs->mkdir(base);
    String path = base + "/" + name + ".jpg";

    if (!fs->exists(path)) {
        if (g_dlTask) return;
        g_dlJob = new DownloadJob();
        g_dlJob->url = url;
        g_dlJob->path = path;
        showBusy("Downloading...");
        xTaskCreate(downloadTask, "math_cloud_dl", 4096, g_dlJob, 1, &g_dlTask);
        while (g_dlJob && !g_dlJob->done) {
            if (check(EscPress) || check(PrevPress)) break;
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }
        if (g_dlJob && !g_dlJob->ok) {
            displayError(g_dlJob->error.length() ? g_dlJob->error : "Download failed", true);
            delete g_dlJob;
            g_dlJob = nullptr;
            return;
        }
        delete g_dlJob;
        g_dlJob = nullptr;
    }

    tft.fillScreen(TFT_BLACK);
    if (!showJpeg(*fs, path, 0, 0, true)) {
        displayError("Image decode failed", true);
        return;
    }
    while (!check(AnyKeyPress)) { vTaskDelay(10 / portTICK_PERIOD_MS); }
}

void geometryMenu() {
    options = {
        {"Circle (Local)", []() { geometryLocal("Circle"); }},
        {"Triangle (Local)", []() { geometryLocal("Triangle"); }},
        {"Square (Local)", []() { geometryLocal("Square"); }},
        {"Cube (Online)", []() {
             geometryOnline(
                 "cube",
                 "https://upload.wikimedia.org/wikipedia/commons/thumb/6/6b/Bitmap_VS_SVG.svg/240px-"
                 "Bitmap_VS_SVG.svg.jpg"
             );
         }},
        {"Back", []() { returnToMenu = true; }}
    };
    loopOptions(options, MENU_TYPE_SUBMENU, "Geometry");
}

void solveMenu() {
    if (!wifiConnected) {
        displayError("WiFi not connected", true);
        return;
    }

    String expr = keyboard("2+2", 64, "Expr:");
    if (expr == "\x1B") return;

    if (g_mathTask) return;
    g_mathJob = new MathJob();
    g_mathJob->expr = expr;

    showBusy("Solving...");
    xTaskCreate(mathTask, "math_cloud_req", 4096, g_mathJob, 1, &g_mathTask);

    while (g_mathJob && !g_mathJob->done) {
        if (check(EscPress) || check(PrevPress)) break;
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }

    if (!g_mathJob) return;
    if (g_mathJob->error.length()) {
        displayError(g_mathJob->error, true);
    } else {
        showResult(expr, g_mathJob->result);
    }
    delete g_mathJob;
    g_mathJob = nullptr;
}
} // namespace

void math_cloud_run() {
    returnToMenu = false;

    if (!wifiConnected) {
        wifiConnectMenu();
    }

    options = {
        {"Solve Algebra", []() { solveMenu(); }},
        {"Geometry", []() { geometryMenu(); }},
        {"Back", []() { returnToMenu = true; }}
    };

    loopOptions(options, MENU_TYPE_SUBMENU, "Math Cloud");
    returnToMenu = true;
}
