#ifndef MJS_RIDL_API_H
#define MJS_RIDL_API_H

#include "mquickjs.h"

/*
 * ============================
 * RIDL C API (declarations only)
 *
 * This header is the ONLY public entry for runtime C compilation.
 * It must NOT contain any static definitions.
 *
 * - Runtime TU (mqjs_stdlib_impl.c / require.c / generated glue) include this.
 * - Host ROM tool should include mquickjs_ridl_register.h instead.
 * ============================
 */

// Built-in stdlib extensions (mquickjs-rs)
// require() exists only when ridl-extensions is enabled.
JSValue js_ridl_require(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);

/* ----------------------------
 * RIDL global functions
 * ----------------------------
 */

/* ----------------------------
 * RIDL interfaces
 * ----------------------------
 */

// Property definition arrays

// Interface object class definition (not user classes)
// NOTE: runtime must not reference JSClassDef (host-only). Kept out.

/* ----------------------------
 * RIDL module & user class ids
 * ----------------------------
 */

#ifndef JS_CLASS_COUNT
#define JS_CLASS_COUNT (JS_CLASS_USER + 0)
#endif

/* ----------------------------
 * RIDL module constructors
 * (instances are created by require())
 * ----------------------------
 */

/* ----------------------------
 * RIDL user classes
 * (constructors/methods/getters/finalizers)
 * ----------------------------
 */

// Singletons
JSValue js_global_singleton_console_log(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_global_singleton_console_error(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_global_singleton_console_get_enabled(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);

// RIDL module require-table (generated)

typedef struct {
    const char *module_full_name;
    const char *module_base;
    uint16_t v_major;
    uint16_t v_minor;
    uint16_t v_patch;

    /* ensure_class_ids[0] must be module_class_id */
    int module_class_id;
    const int *ensure_class_ids;
    int ensure_class_ids_len;
} RidlRequireEntry;

extern const RidlRequireEntry js_ridl_require_table[];
extern const int js_ridl_require_table_len;

/* stdlib-normalization init (called once per JSContext by host glue).
   - materialize module exports onto module prototypes
   - install proto vars once
   Returns 0 on success, -1 on exception.
*/
int JS_RIDL_StdlibInit(JSContext *ctx);

#endif // MJS_RIDL_API_H