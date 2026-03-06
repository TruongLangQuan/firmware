#include "idk_apps.h"
#include <globals.h>

#define setup idk_chess_setup
#define loop idk_chess_loop
#include "../../../other/idk-chess/src/main.cpp"
#undef setup
#undef loop

void idk_chess_run() {
    returnToMenu = false;
    idk_chess_setup();
    while (1) {
        idk_chess_loop();
        if (returnToMenu) break;
        delay(1);
    }
}
