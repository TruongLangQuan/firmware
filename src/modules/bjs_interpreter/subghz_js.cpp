#if !defined(LITE_VERSION) && !defined(DISABLE_INTERPRETER)
#include "subghz_js.h"

JSValue native_subghzTransmitFile(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_NewBool(false);
}

JSValue native_subghzTransmit(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_NewBool(false);
}

JSValue native_subghzRead(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_NewString(ctx, "");
}

JSValue native_subghzReadRaw(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_NewString(ctx, "");
}

JSValue native_subghzSetFrequency(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_UNDEFINED;
}

#endif
