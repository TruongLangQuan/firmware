#ifndef __IDK_M5UNIFIED_COMPAT_H__
#define __IDK_M5UNIFIED_COMPAT_H__

#include <globals.h>
#include <interface.h>
#include <cstdarg>
#include <cstdio>

namespace idk_m5_compat {

class DisplayCompat {
public:
    void fillScreen(uint32_t color) { tft.fillScreen((uint16_t)color); }
    void setTextColor(uint16_t fg, uint16_t bg = TFT_BLACK) { tft.setTextColor(fg, bg); }
    void setCursor(int32_t x, int32_t y) { tft.setCursor(x, y); }
    size_t print(const char *text) { return tft.print(text); }
    size_t print(const String &text) { return tft.print(text); }
    size_t print(char c) { return tft.print(c); }
    size_t print(int value) { return tft.print(value); }
    size_t print(unsigned int value) { return tft.print(value); }
    size_t print(long value) { return tft.print(value); }
    size_t print(unsigned long value) { return tft.print(value); }
    size_t println(const char *text) { return tft.println(text); }
    size_t println(const String &text) { return tft.println(text); }
    void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color) {
        tft.drawRect(x, y, w, h, color);
    }
    void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color) {
        tft.fillRect(x, y, w, h, color);
    }
    void setRotation(uint8_t rotation) { tft.setRotation(rotation); }
    void setTextSize(uint8_t size) { tft.setTextSize(size); }
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return tft.color565(r, g, b); }

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
    ButtonCompat() = default;

    void updateState(bool pressedNow) {
        pressed_ = (!isPressed_ && pressedNow);
        isPressed_ = pressedNow;
    }

    bool wasPressed() {
        bool ret = pressed_;
        pressed_ = false;
        return ret;
    }

    bool isPressed() const { return isPressed_; }

private:
    bool isPressed_ = false;
    bool pressed_ = false;
};

class M5Compat {
public:
    struct config_t {};

    DisplayCompat Display;
    ButtonCompat BtnA;
    ButtonCompat BtnB;
    ButtonCompat BtnPWR;

    config_t config() const { return config_t{}; }
    void begin(const config_t &) {}
    void update() {
        InputHandler();

        bool selPressed = SelPress;
        bool nextPressed = NextPress;
        bool prevPressed = PrevPress;

#if defined(SEL_BTN) && defined(BTN_ACT)
        selPressed = (digitalRead(SEL_BTN) == BTN_ACT);
#endif
#if defined(DW_BTN) && defined(BTN_ACT)
        nextPressed = (digitalRead(DW_BTN) == BTN_ACT);
#endif
#if defined(UP_BTN) && defined(BTN_ACT)
        prevPressed = (digitalRead(UP_BTN) == BTN_ACT);
#endif

        BtnA.updateState(selPressed);
        BtnB.updateState(nextPressed);
        BtnPWR.updateState(prevPressed);
    }
};

} // namespace idk_m5_compat

static idk_m5_compat::M5Compat M5;

#endif // __IDK_M5UNIFIED_COMPAT_H__
