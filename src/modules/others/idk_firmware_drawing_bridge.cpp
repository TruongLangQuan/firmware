#include "idk_apps.h"
#include "idk_vi_font.h"
#include <core/display.h>
#include <globals.h>

#include "../../../other/idk-firmware/drawing/drawing.h"

void idk_firmware_run_drawing() {
    returnToMenu = false;
    idk_vi_font_enable();
    idk_firmware::drawingBegin();

    drawMainBorderWithTitle(idk_firmware::drawingApp.getTitle(), true);
    printCenterFootnote("SEL: ve  PREV/NEXT: di chuyen  BACK: thoat");

    const int canvasTop = 28;
    const int canvasBottom = tftHeight - 10;
    tft.fillRect(8, canvasTop, tftWidth - 16, canvasBottom - canvasTop, TFT_BLACK);
    tft.drawRect(7, canvasTop - 1, tftWidth - 14, canvasBottom - canvasTop + 2, bruceConfig.secColor);

    int x = tftWidth / 2;
    int y = (canvasTop + canvasBottom) / 2;
    bool drawing = false;
    bool lastSel = false;
    uint32_t statusTimer = 0;

    tft.drawPixel(x, y, TFT_GREEN);

    while (1) {
        if (check(EscPress) || check(LongPress)) break;

        bool selNow = SelPress;
        if (selNow && !lastSel && !PrevPress && !NextPress) drawing = !drawing;
        lastSel = selNow;

        int oldX = x;
        int oldY = y;
        if (check(PrevPress)) {
            if (selNow) y--;
            else x--;
        }
        if (check(NextPress)) {
            if (selNow) y++;
            else x++;
        }

        if (x < 8) x = 8;
        if (x > tftWidth - 9) x = tftWidth - 9;
        if (y < canvasTop) y = canvasTop;
        if (y > canvasBottom - 1) y = canvasBottom - 1;

        if (drawing && (x != oldX || y != oldY)) tft.drawLine(oldX, oldY, x, y, TFT_WHITE);
        tft.drawPixel(x, y, drawing ? TFT_YELLOW : TFT_GREEN);

        idk_firmware::drawingLoop();
        if (returnToMenu) break;

        if (millis() - statusTimer >= 1000) {
            drawStatusBar();
            statusTimer = millis();
        }
        delay(10);
    }

    idk_vi_font_disable();
    returnToMenu = true;
}
