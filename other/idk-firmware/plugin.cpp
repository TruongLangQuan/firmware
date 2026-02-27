#include "plugin.h"

#include "core/utils.h"
#include <globals.h>

namespace idk_firmware {
void drawingBegin();
void drawingLoop();
void plotBegin();
void plotLoop();
void cube3dBegin();
void cube3dLoop();
} // namespace idk_firmware

namespace {

void runIdkLoop(void (*beginFn)(), void (*loopFn)()) {
    beginFn();
    while (1) {
        if (check(EscPress) || check(LongPress)) break;
        loopFn();
        if (returnToMenu) break;
        delay(10);
    }
    returnToMenu = true;
}

} // namespace

void idk_firmware_run_drawing() { runIdkLoop(idk_firmware::drawingBegin, idk_firmware::drawingLoop); }

void idk_firmware_run_plot() { runIdkLoop(idk_firmware::plotBegin, idk_firmware::plotLoop); }

void idk_firmware_run_cube3d() { runIdkLoop(idk_firmware::cube3dBegin, idk_firmware::cube3dLoop); }
