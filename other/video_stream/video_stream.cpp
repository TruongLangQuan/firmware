#include "video_stream.h"

#include <HTTPClient.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/sd_functions.h"
#include "core/wifi/wifi_common.h"
#include <globals.h>

#include "esp32-hal-psram.h"

namespace {
constexpr size_t kDefaultFrameCap = 64 * 1024;
constexpr uint32_t kReconnectDelayMs = 1500;
constexpr uint32_t kFrameIntervalMs = 120; // ~8 FPS

struct StreamConfig {
    String url;
};

struct StreamState {
    WiFiClient client;
    WiFiClientSecure secureClient;
    WiFiClient *active = nullptr;
    String host;
    String path;
    uint16_t port = 80;
    bool secure = false;
    bool connected = false;
    bool headersDone = false;
    uint32_t headerStart = 0;
    int prevByte = -1;
    bool inFrame = false;
    uint8_t *buf = nullptr;
    size_t cap = 0;
    size_t len = 0;
    uint32_t lastFrame = 0;
    uint32_t lastConnectAttempt = 0;
};

bool tftOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
    if (y >= tftHeight || x >= tftWidth) return false;
    tft.pushImage(x, y, w, h, bitmap);
    return true;
}

bool parseUrl(const String &url, String &host, uint16_t &port, String &path, bool &secure) {
    String u = url;
    u.trim();
    secure = false;
    if (u.startsWith("https://")) {
        secure = true;
        u = u.substring(8);
    } else if (u.startsWith("http://")) {
        u = u.substring(7);
    }
    int slash = u.indexOf('/');
    if (slash < 0) {
        host = u;
        path = "/";
    } else {
        host = u.substring(0, slash);
        path = u.substring(slash);
    }
    int colon = host.indexOf(':');
    if (colon >= 0) {
        port = host.substring(colon + 1).toInt();
        host = host.substring(0, colon);
    } else {
        port = secure ? 443 : 80;
    }
    if (host.length() == 0) return false;
    if (path == "/") path = "/stream";
    return true;
}

StreamConfig loadConfig() {
    StreamConfig cfg;
    cfg.url = "https://4200-171-243-49-104.ngrok-free.app/stream";

    if (!LittleFS.begin(true)) {
        LittleFS.begin(true);
        return cfg;
    }
    if (!LittleFS.exists("/video_stream.conf")) return cfg;
    File f = LittleFS.open("/video_stream.conf", FILE_READ);
    if (!f) return cfg;
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) cfg.url = line;
    f.close();
    return cfg;
}

void saveConfig(const StreamConfig &cfg) {
    if (!LittleFS.begin(true)) LittleFS.begin(true);
    File f = LittleFS.open("/video_stream.conf", FILE_WRITE);
    if (!f) return;
    f.println(cfg.url);
    f.close();
}

void showStatus(const String &msg) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(FP);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(4, 4);
    tft.print(msg);
}

void disconnect(StreamState &s) {
    if (s.active && s.active->connected()) s.active->stop();
    s.connected = false;
    s.headersDone = false;
    s.inFrame = false;
    s.len = 0;
    s.prevByte = -1;
}

bool sendSeek(StreamState &s, int deltaSeconds) {
    if (s.host.length() == 0 || s.port == 0) return false;
    HTTPClient http;
    String scheme = s.secure ? "https://" : "http://";
    String url = scheme + s.host + ":" + String(s.port) + "/seek?delta=" + String(deltaSeconds);
    WiFiClient ctrlPlain;
    WiFiClientSecure ctrlSecure;
    WiFiClient *ctrl = &ctrlPlain;
    if (s.secure) {
        ctrlSecure.setInsecure();
        ctrl = &ctrlSecure;
    }
    if (!http.begin(*ctrl, url)) return false;
    int code = http.GET();
    http.end();
    return (code > 0 && code < 400);
}

bool connectStream(StreamState &s, const StreamConfig &cfg) {
    if (millis() - s.lastConnectAttempt < kReconnectDelayMs) return false;
    s.lastConnectAttempt = millis();

    if (!parseUrl(cfg.url, s.host, s.port, s.path, s.secure)) return false;

    showStatus("Connecting...");
    s.active = s.secure ? static_cast<WiFiClient *>(&s.secureClient) : static_cast<WiFiClient *>(&s.client);
    if (s.secure) s.secureClient.setInsecure();
    if (!s.active->connect(s.host.c_str(), s.port)) {
        disconnect(s);
        return false;
    }
    s.active->setTimeout(3000);

    s.active->print(
        String("GET ") + s.path + " HTTP/1.1\r\n" + "Host: " + s.host + "\r\n" +
        "Connection: keep-alive\r\n" + "User-Agent: Bruce/1.0\r\n\r\n"
    );

    s.connected = true;
    s.headersDone = false;
    s.inFrame = false;
    s.len = 0;
    s.prevByte = -1;
    s.headerStart = millis();
    return true;
}

void consumeHeaders(StreamState &s) {
    if (millis() - s.headerStart > 2000) {
        s.headersDone = true;
        return;
    }
    while (s.active && s.active->connected() && s.active->available()) {
        String line = s.active->readStringUntil('\n');
        if (line == "\r") {
            s.headersDone = true;
            return;
        }
    }
}

void processByte(StreamState &s, int b) {
    if (!s.inFrame) {
        if (s.prevByte == 0xFF && b == 0xD8) {
            s.inFrame = true;
            s.len = 0;
            if (s.cap >= 2) {
                s.buf[s.len++] = 0xFF;
                s.buf[s.len++] = 0xD8;
            }
        }
    } else {
        if (s.len < s.cap) {
            s.buf[s.len++] = (uint8_t)b;
        } else {
            s.inFrame = false;
            s.len = 0;
        }
        if (s.prevByte == 0xFF && b == 0xD9) {
            if (millis() - s.lastFrame >= kFrameIntervalMs) {
                TJpgDec.drawJpg(0, 0, s.buf, s.len);
                s.lastFrame = millis();
            }
            s.inFrame = false;
            s.len = 0;
        }
    }
    s.prevByte = b;
}

void streamLoop(const StreamConfig &cfg) {
    StreamState s;

    parseUrl(cfg.url, s.host, s.port, s.path, s.secure);

    s.cap = psramFound() ? (128 * 1024) : kDefaultFrameCap;
    s.buf = (uint8_t *)heap_caps_malloc(s.cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s.buf) {
        s.cap = kDefaultFrameCap;
        s.buf = (uint8_t *)malloc(s.cap);
    }

    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tftOutput);

    uint32_t lastActivity = millis();

    while (true) {
        InputHandler();
        if (check(SelPress) || check(EscPress)) break;
        if (check(NextPress)) sendSeek(s, 10);
        if (check(PrevPress)) sendSeek(s, -10);

        if (!wifiConnected) {
            showStatus("WiFi not connected");
            vTaskDelay(200 / portTICK_PERIOD_MS);
            continue;
        }

        if (!s.connected || !s.active || !s.active->connected()) {
            disconnect(s);
            connectStream(s, cfg);
            vTaskDelay(50 / portTICK_PERIOD_MS);
            continue;
        }

        if (!s.headersDone) {
            consumeHeaders(s);
            vTaskDelay(5 / portTICK_PERIOD_MS);
            continue;
        }

        while (s.active && s.active->available()) {
            int b = s.active->read();
            if (b < 0) break;
            processByte(s, b);
            lastActivity = millis();
        }

        if (millis() - lastActivity > 5000) { disconnect(s); }

        vTaskDelay(2 / portTICK_PERIOD_MS);
    }

    disconnect(s);
    if (s.buf) free(s.buf);
}

void setUrl(StreamConfig &cfg) {
    String url = keyboard(cfg.url, 128, "Stream URL");
    if (url == "\x1B") return;
    url.trim();
    if (url.length() == 0) return;
    cfg.url = url;
    saveConfig(cfg);
}
} // namespace

void video_stream_run() {
    returnToMenu = false;

    if (!wifiConnected) { wifiConnectMenu(); }

    StreamConfig cfg = loadConfig();

    options = {
        {"Start Stream", [&]() { streamLoop(cfg); }    },
        {"Set URL",      [&]() { setUrl(cfg); }        },
        {"Back",         [&]() { returnToMenu = true; }}
    };

    loopOptions(options, MENU_TYPE_SUBMENU, "Video Stream");
    returnToMenu = true;
}
