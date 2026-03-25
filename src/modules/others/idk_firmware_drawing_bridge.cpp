#include "idk_apps.h"
#include <globals.h>
#include <interface.h>

namespace {
constexpr int MAX_DRAW_W = TFT_WIDTH / 3;
constexpr int MAX_DRAW_H = TFT_HEIGHT / 3;
static uint8_t canvas[(MAX_DRAW_W * MAX_DRAW_H + 7) / 8];
static bool canvasInit = false;
static int lastDrawW = 0;
static int lastDrawH = 0;

inline void setPixel(int x, int y, int drawW, int drawH, bool on) {
    if (x < 0 || x >= drawW || y < 0 || y >= drawH) return;
    int idx = y * drawW + x;
    int byteIdx = idx / 8;
    int bitIdx = idx % 8;
    if (on) canvas[byteIdx] |= (1 << bitIdx);
    else canvas[byteIdx] &= ~(1 << bitIdx);
}

inline bool getPixel(int x, int y, int drawW, int drawH) {
    if (x < 0 || x >= drawW || y < 0 || y >= drawH) return false;
    int idx = y * drawW + x;
    int byteIdx = idx / 8;
    int bitIdx = idx % 8;
    return (canvas[byteIdx] & (1 << bitIdx)) != 0;
}
} // namespace

void idk_firmware_run_drawing() {
    returnToMenu = false;

    const int drawW = min(MAX_DRAW_W, max(1, tftWidth / 3));
    const int drawH = min(MAX_DRAW_H, max(1, tftHeight / 3));
    const int scale = 3;

    if (!canvasInit || lastDrawW != drawW || lastDrawH != drawH) {
        memset(canvas, 0, sizeof(canvas));
        canvasInit = true;
        lastDrawW = drawW;
        lastDrawH = drawH;
    }

    int px = drawW / 2;
    int py = drawH / 2;
    bool exitDraw = false;

    static bool prevDownLast = false;
    static bool nextDownLast = false;
    static bool selDownLast = false;
    static bool selWasPressed = false;
    static bool selHoldActive = false;
    static uint32_t selPressTime = 0;
    static bool nextWasPressed = false;
    static bool prevWasPressed = false;
    static uint32_t nextPressTime = 0;
    static uint32_t prevPressTime = 0;
    static uint32_t lastMoveLeft = 0;
    static uint32_t lastMoveUp = 0;

    while (!exitDraw) {
        InputHandler();

        bool prevDown = (digitalRead(UP_BTN) == LOW);
        bool nextDown = (digitalRead(DW_BTN) == LOW);
        bool selDown = (digitalRead(SEL_BTN) == LOW);

        bool prevEdge = prevDown && !prevDownLast;
        bool nextEdge = nextDown && !nextDownLast;
        bool selEdge = selDown && !selDownLast;
        bool selRelease = !selDown && selDownLast;

        prevDownLast = prevDown;
        nextDownLast = nextDown;
        selDownLast = selDown;

        if (selDown && prevDown && nextDown) {
            exitDraw = true;
            memset(canvas, 0, sizeof(canvas));
            break;
        }

        if (selEdge) {
            selWasPressed = true;
            selHoldActive = false;
            selPressTime = millis();
        }
        if (selDown && selWasPressed) {
            if (!selHoldActive && (millis() - selPressTime > 200)) {
                selHoldActive = true;
            }
            if (selHoldActive) {
                setPixel(px, py, drawW, drawH, false);
            }
        }
        if (selRelease && selWasPressed) {
            if (!selHoldActive) {
                setPixel(px, py, drawW, drawH, true);
            }
            selWasPressed = false;
            selHoldActive = false;
        }

        // Handle Next button (move right on press, left on hold)
        if (nextEdge) {
            nextWasPressed = true;
            nextPressTime = millis();
            px++;
            if (px >= drawW) px = drawW - 1;
        }

        if (nextDown && nextWasPressed) {
            if (millis() - nextPressTime > 200) {
                if (millis() - lastMoveLeft > 100) {
                    px--;
                    if (px < 0) px = 0;
                    lastMoveLeft = millis();
                }
            }
        } else {
            nextWasPressed = false;
        }

        // Handle Prev button (move down on press, up on hold)
        if (prevEdge) {
            prevWasPressed = true;
            prevPressTime = millis();
            py++;
            if (py >= drawH) py = drawH - 1;
        }

        if (prevDown && prevWasPressed) {
            if (millis() - prevPressTime > 200) {
                if (millis() - lastMoveUp > 100) {
                    py--;
                    if (py < 0) py = 0;
                    lastMoveUp = millis();
                }
            }
        } else {
            prevWasPressed = false;
        }

        tft.fillScreen(TFT_BLACK);
        for (int x = 0; x < drawW; x++) {
            for (int y = 0; y < drawH; y++) {
                if (getPixel(x, y, drawW, drawH)) {
                    int sx = x * scale;
                    int sy = y * scale;
                    tft.fillRect(sx, sy, scale, scale, TFT_WHITE);
                }
            }
        }

        if (px >= 0 && px < drawW && py >= 0 && py < drawH) {
            int sx = px * scale;
            int sy = py * scale;
            tft.drawRect(sx - 1, sy - 1, scale + 2, scale + 2, TFT_CYAN);
        }

        if (returnToMenu) break;
        delay(10);
    }

    returnToMenu = true;
}
