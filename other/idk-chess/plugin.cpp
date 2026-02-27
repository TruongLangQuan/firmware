#include "plugin.h"

#include "core/utils.h"
#include <globals.h>

#define setup idk_chess_setup
#define loop idk_chess_loop
#include "src/main.cpp"
#undef setup
#undef loop

void idk_chess_run() {
    idk_chess_setup();
    while (1) {
        if (check(EscPress) || check(LongPress)) break;
        idk_chess_loop();
        if (returnToMenu) break;
        delay(1);
    }
    returnToMenu = true;
}
