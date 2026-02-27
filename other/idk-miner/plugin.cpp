#include "plugin.h"

#include "core/utils.h"
#include <globals.h>

#include "src/api_client.cpp"
#include "src/ui.cpp"
#include "src/wifi_manager.cpp"
#include "src/main.cpp"

void idk_miner_run() {
    idk_miner_setup();
    while (1) {
        if (SelPress && EscPress) break;
        idk_miner_loop();
        if (returnToMenu) break;
        delay(10);
    }
    returnToMenu = true;
}
