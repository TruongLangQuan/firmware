#ifndef __IDK_MINER_M5UNIFIED_COMPAT_H__
#define __IDK_MINER_M5UNIFIED_COMPAT_H__

#include <globals.h>
#include <interface.h>
#include <cstdarg>
#include <cstdio>

constexpr int top_left = 0;

namespace idk_miner_m5_compat {

class DisplayCompat {
public:
    void setRotation(uint8_t rotation) { tft.setRotation(rotation); }
    void fillScreen(uint16_t color) { tft.fillScreen(color); }
    void setTextDatum(int datum) { (void)datum; }
    void setTextSize(uint8_t size) { tft.setTextSize(size); }
    void setTextColor(uint16_t fg, uint16_t bg = TFT_BLACK) { tft.setTextColor(fg, bg); }
    void setCursor(int32_t x, int32_t y) { tft.setCursor(x, y); }
    int width() const { return tftWidth; }
    int height() const { return tftHeight; }
    void drawFastHLine(int32_t x, int32_t y, int32_t w, uint16_t color) { tft.drawFastHLine(x, y, w, color); }

    size_t print(const char *text) { return tft.print(text); }
    size_t print(const String &text) { return tft.print(text); }
    size_t print(float value) { return tft.print(value); }

    void printf(const char *fmt, ...) {
        char buf[192];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        tft.print(buf);
    }
};

class ButtonCompat {
public:
    explicit ButtonCompat(volatile bool &button) : button_(button) {}
    bool wasClicked() { return check(button_); }
    bool wasPressed() { return check(button_); }
    bool isPressed() const { return button_; }

private:
    volatile bool &button_;
};

class PowerCompat {
public:
    int getBatteryLevel() const { return getBattery(); }
};

class M5Compat {
public:
    struct config_t {
        bool clear_display = true;
        bool output_power = true;
    };

    DisplayCompat Display;
    ButtonCompat BtnA;
    ButtonCompat BtnB;
    ButtonCompat BtnPWR;
    PowerCompat Power;

    M5Compat() : BtnA(SelPress), BtnB(NextPress), BtnPWR(PrevPress) {}

    config_t config() const { return config_t{}; }
    void begin(const config_t &) {}
    void update() { InputHandler(); }
};

} // namespace idk_miner_m5_compat

static idk_miner_m5_compat::M5Compat M5;

#endif // __IDK_MINER_M5UNIFIED_COMPAT_H__
