#include "idk_apps.h"
#include "idk_vi_font.h"
#include <core/display.h>
#include <globals.h>

#include "../../../other/idk-firmware/cube3d/cube3d.h"

void idk_firmware_run_cube3d() {
    returnToMenu = false;
    idk_vi_font_enable();
    idk_firmware::cube3dBegin();

    uint32_t statusTimer = 0;
    bool redrawStaticUi = true;

    auto drawCubeFrame = [&]() {
        if (redrawStaticUi) {
            drawMainBorderWithTitle(idk_firmware::cube3d.getTitle(), true);
            printCenterFootnote("SEL: auto  NEXT/PREV: toc do  BACK: thoat");
            redrawStaticUi = false;
        }

        const int top = 30;
        const int bottom = tftHeight - 10;
        const int left = 8;
        const int right = tftWidth - 8;
        const int w = right - left;
        const int h = bottom - top;

        tft.fillRect(left, top, w, h, TFT_BLACK);
        tft.drawRect(left - 1, top - 1, w + 2, h + 2, bruceConfig.secColor);

        idk_firmware::cube3d.setCenter(left + w / 2, top + h / 2);
        idk_firmware::cube3d.setScale((float)min(w, h) * 0.28f);

        auto vertices = idk_firmware::cube3d.getCubeVertices();
        std::vector<idk_firmware::Point2D> projected;
        projected.reserve(vertices.size());
        for (const auto &v : vertices) {
            auto tv = idk_firmware::cube3d.transform(v);
            projected.push_back(idk_firmware::cube3d.project(tv));
        }

        auto edges = idk_firmware::cube3d.getCubeEdges();
        for (const auto &e : edges) {
            const auto &a = projected[e.first];
            const auto &b = projected[e.second];
            tft.drawLine(a.x, a.y, b.x, b.y, TFT_CYAN);
        }

        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.setCursor(left + 2, top + 2);
        tft.printf("spd: %.1f", idk_firmware::cube3d.getSpeed());
    };

    while (1) {
        if (check(EscPress) || check(LongPress)) break;

        if (check(SelPress)) idk_firmware::cube3d.setAutoRotate(!idk_firmware::cube3d.isAutoRotate());
        if (check(NextPress)) idk_firmware::cube3d.setSpeed(idk_firmware::cube3d.getSpeed() + 0.2f);
        if (check(PrevPress)) idk_firmware::cube3d.setSpeed(max(0.2f, idk_firmware::cube3d.getSpeed() - 0.2f));

        idk_firmware::cube3dLoop();
        drawCubeFrame();
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
