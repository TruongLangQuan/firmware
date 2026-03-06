#include "idk_apps.h"
#include "idk_vi_font.h"
#include <core/display.h>
#include <globals.h>

#include "../../../other/idk-firmware/plot/plot.h"

void idk_firmware_run_plot() {
    returnToMenu = false;
    idk_vi_font_enable();
    idk_firmware::plotBegin();

    const char *funcs[] = {"sin(x)", "cos(x)", "tan(x/2)"};
    const int funcCount = static_cast<int>(sizeof(funcs) / sizeof(funcs[0]));
    int funcIndex = 0;
    uint32_t statusTimer = 0;
    bool redraw = true;

    auto drawPlot = [&]() {
        drawMainBorderWithTitle(idk_firmware::plotApp.getTitle(), true);
        printCenterFootnote("NEXT/PREV: doi ham  SEL: ve lai  BACK: thoat");

        const int left = 8;
        const int right = tftWidth - 8;
        const int top = 30;
        const int bottom = tftHeight - 12;
        const int w = right - left;
        const int h = bottom - top;

        tft.fillRect(left, top, w, h, TFT_BLACK);
        tft.drawRect(left - 1, top - 1, w + 2, h + 2, bruceConfig.secColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.setCursor(left + 2, top + 2);
        tft.print(funcs[funcIndex]);

        // Center axes
        const int axisX = left + w / 2;
        const int axisY = top + h / 2;
        tft.drawFastVLine(axisX, top, h, 0x39E7);
        tft.drawFastHLine(left, axisY, w, 0x39E7);

        auto points = idk_firmware::plotApp.generatePoints(funcs[funcIndex], 120);
        int prevX = 0;
        int prevY = 0;
        bool havePrev = false;
        for (const auto &p : points) {
            int sx = 0;
            int sy = 0;
            idk_firmware::plotApp.worldToScreen(p.x, p.y, sx, sy, w - 1, h - 1);
            sx += left;
            sy += top;
            if (sx < left || sx >= right || sy < top || sy >= bottom) {
                havePrev = false;
                continue;
            }
            if (havePrev) tft.drawLine(prevX, prevY, sx, sy, TFT_CYAN);
            prevX = sx;
            prevY = sy;
            havePrev = true;
        }
    };

    while (1) {
        if (check(EscPress) || check(LongPress)) break;

        if (check(NextPress)) {
            funcIndex = (funcIndex + 1) % funcCount;
            redraw = true;
        }
        if (check(PrevPress)) {
            funcIndex = (funcIndex + funcCount - 1) % funcCount;
            redraw = true;
        }
        if (check(SelPress)) redraw = true;

        if (redraw) {
            drawPlot();
            redraw = false;
        }

        idk_firmware::plotLoop();
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
