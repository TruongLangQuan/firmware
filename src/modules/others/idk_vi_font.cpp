#include "idk_vi_font.h"

#include <globals.h>

#ifdef USE_TFT_ESPI
#include "../../../other/i18n/vi12_font.h"
#endif

void idk_vi_font_enable() {
#ifdef USE_TFT_ESPI
    tft.loadFont(vi12_font);
#endif
}

void idk_vi_font_disable() {
#ifdef USE_TFT_ESPI
    tft.unloadFont();
#endif
    tft.setTextFont(1);
}
