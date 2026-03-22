#include "idk_vi_font.h"


#include <globals.h>

#if defined(HAS_SCREEN) && defined(SMOOTH_FONT)
#include "../../../other/i18n/vi12_font.h"
#endif

void idk_vi_font_enable() {
#if defined(HAS_SCREEN) && defined(SMOOTH_FONT)
    tft.loadFont(vi12_font);
#endif
}

void idk_vi_font_disable() {
#if defined(HAS_SCREEN) && defined(SMOOTH_FONT)
    tft.unloadFont();
#endif
    tft.setTextFont(1);
}
