// Author: Senape3000
// More info: https://github.com/Senape3000/firmware/blob/main/docs_custom/JS_RFID/RFID_API_README.md
// More info: https://github.com/Senape3000/firmware/blob/main/docs_custom/JS_RFID/RFID_SRIX_API_README.md

#if !defined(LITE_VERSION) && !defined(DISABLE_INTERPRETER)

#include "rfid_js.h"

static JSValue rfidModuleDisabled(JSContext *ctx) {
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "success", JS_NewBool(false));
    JS_SetPropertyStr(ctx, obj, "message", JS_NewString(ctx, "RFID module disabled"));
    return obj;
}

JSValue native_rfidRead(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_NULL; }

JSValue native_rfidReadUID(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_NewString(ctx, "");
}

JSValue native_rfidWrite(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return rfidModuleDisabled(ctx);
}

JSValue native_rfidSave(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return rfidModuleDisabled(ctx);
}

JSValue native_rfidLoad(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_NULL;
}

JSValue native_rfidClear(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_UNDEFINED;
}

JSValue native_rfid_AddMifareKey(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return rfidModuleDisabled(ctx);
}

JSValue native_srixRead(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_NULL; }

JSValue native_srixWrite(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return rfidModuleDisabled(ctx);
}

JSValue native_srixSave(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return rfidModuleDisabled(ctx);
}

JSValue native_srixLoad(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_NULL; }

JSValue native_srixClear(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_UNDEFINED;
}

JSValue native_srixWriteBlock(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return rfidModuleDisabled(ctx);
}

#endif
