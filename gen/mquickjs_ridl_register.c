#include "mquickjs.h"
#include "mquickjs_ridl_api.h"
#include "mquickjs_ridl_register.h"


// RIDL module require-table + runtime init helpers (generated)
//
// This file is runtime-only (linked into libmquickjs.a). It intentionally stays out of
// the host ROM-generation compilation path.

typedef struct {
    int class_id;
    const char *field_name;
    JSValue (*make_value)(JSContext *ctx);
} RidlProtoVarEntry;

static int ridl_install_proto_vars_all(JSContext *ctx) {
    (void)ctx;
    return 0;
}

#ifdef MQUICKJS_ENABLE_RIDL_EXTENSIONS
#endif

static int ridl_module_extensions_init_once(JSContext *ctx) {
    // Per-context once: tests run multiple JSContext instances in one process.
    // Re-entry on the same ctx is considered an error.
    static const char k_ridl_ext_init_marker[] = "__ridl_ext_module_extensions_inited";

    JSValue global_obj = JS_GetGlobalObject(ctx);
    JSValue v = JS_GetPropertyStr(ctx, global_obj, k_ridl_ext_init_marker);
    if (!JS_IsUndefined(v)) {
        return -1;
    }

    if (JS_SetPropertyStr(ctx, global_obj, k_ridl_ext_init_marker, JS_NewBool(1)) < 0) {
        return -1;
    }

#ifdef MQUICKJS_ENABLE_RIDL_EXTENSIONS
#else
    (void)ctx;
#endif

    return 0;
}

// GLOBAL singleton js-only `var` fields (plain var) are installed onto the
// singleton object by JS_RIDL_StdlibInit below. Module-mode singletons are not
// reachable from JS today, so they get no installation code.
// NOTE: the validator restricts singleton var literal types to
// {i32, bool, string, null}; anything else fails generation loudly.

int JS_RIDL_StdlibInit(JSContext *ctx) {
    if (!ctx)
        return 0;

    if (ridl_module_extensions_init_once(ctx) < 0)
        return -1;

    // Install proto vars once (after module classes are materialized).
    if (ridl_install_proto_vars_all(ctx) < 0)
        return -1;

    // Install GLOBAL singleton js-only var fields (plain var). This runs before
    // any user script, so the fields are readable before any singleton method
    // is first called.

    return 0;
}

const RidlRequireEntry js_ridl_require_table[] = {
};

const int js_ridl_require_table_len = (int)(sizeof(js_ridl_require_table) / sizeof(js_ridl_require_table[0]));