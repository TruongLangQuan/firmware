#include "idk_apps.h"
#include <globals.h>

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
    static uint32_t prevPressTime = 0;
    static uint32_t nextPressTime = 0;
    static uint32_t lastMoveUp = 0;
    static uint32_t lastMoveDown = 0;

    while (!exitDraw) {
        InputHandler();

        bool prevDown = PrevPress;
        bool nextDown = NextPress;
        bool selDown = SelPress;

        bool prevEdge = prevDown && !prevDownLast;
        bool nextEdge = nextDown && !nextDownLast;
        bool selEdge = selDown && !selDownLast;

        prevDownLast = prevDown;
        nextDownLast = nextDown;
        selDownLast = selDown;

        if (selDown && prevDown) {
            exitDraw = true;
            memset(canvas, 0, sizeof(canvas));
            break;
        }

        if (selEdge && !nextDown && !prevDown) setPixel(px, py, drawW, drawH, true);
        if (selDown && nextEdge) setPixel(px, py, drawW, drawH, false);

        if (!selDown) {
            if (prevEdge) {
                prevPressTime = millis();
                px--;
                if (px < 0) px = 0;
            }
            if (nextEdge) {
                nextPressTime = millis();
                px++;
                if (px >= drawW) px = drawW - 1;
            }

            if (prevDown) {
                if (millis() - prevPressTime > 200) {
                    if (millis() - lastMoveUp > 100) {
                        py--;
                        if (py < 0) py = 0;
                        lastMoveUp = millis();
                    }
                }
            }

            if (nextDown) {
                if (millis() - nextPressTime > 200) {
                    if (millis() - lastMoveDown > 100) {
                        py++;
                        if (py >= drawH) py = drawH - 1;
                        lastMoveDown = millis();
                    }
                }
            }
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
