// Auto-generated C header for RIDL interfaces
#ifndef MJS_RIDL_REGISTER_H
#define MJS_RIDL_REGISTER_H

#include "mquickjs.h"
#include "mquickjs_build.h"

#include "mquickjs_ridl_api.h"

/*
 * ============================
 * RIDL register (definitions)
 *
 * This header defines the static roots used by the ROM build tool.
 * It is NOT a public header for general runtime compilation.
 * ============================
 */

/* ----------------------------
 * RIDL class ids & keepalive entrypoints
 * ----------------------------
 */
//
// JS class ids (JS_CLASS_*) must be compile-time constants for mquickjs-build to
// generate the ROM table. Numeric IDs are allocated starting from JS_CLASS_USER.

#ifndef JS_CLASS_COUNT
#define JS_CLASS_COUNT (JS_CLASS_USER + 0)
#endif

/* ----------------------------
 * RIDL module/user class definitions
 * (JSPropDef/JSClassDef/constructor stubs)
 * ----------------------------
 */

// Module initialization functions

// Singletons
JSValue js_global_singleton_console_log(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_global_singleton_console_error(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_global_singleton_console_get_enabled(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);

// Extension definitions
//
// NOTE: For global singletons (e.g. `console`), registration requires file-scope
// declarations/definitions (props/object defs) *and* global table entries.
// A single macro expanded only inside `js_global_object[]` is insufficient.

// File-scope declarations/definitions for RIDL extensions
#define JS_RIDL_DECLS \
    static const JSPropDef js_ridl_modules_ns_props[] = { \
        JS_PROP_END, \
    }; \
    static const JSClassDef js_ridl_modules_ns_obj = \
        JS_OBJECT_DEF("RidlModules", js_ridl_modules_ns_props); \
    static const JSPropDef js_global_singleton_console_props[] = { \
        JS_CFUNC_DEF("log", 1, js_global_singleton_console_log), \
        JS_CFUNC_DEF("error", 1, js_global_singleton_console_error), \
        JS_CGETSET_DEF("enabled", js_global_singleton_console_get_enabled, NULL), \
        JS_PROP_END, \
    }; \
    static const JSClassDef js_global_singleton_console_obj = \
        JS_OBJECT_DEF("Console", js_global_singleton_console_props); \
    /* empty */ \

/*
 * Expand RIDL file-scope declarations/definitions.
 *
 * Call sites should use `JS_RIDL_DECLS_EXPAND;`.
 */
#define JS_RIDL_DECLS_EXPAND JS_RIDL_DECLS

// RIDL module require-table (generated)
//
// Runtime will provide:
//   - js_ridl_require_table
//   - js_ridl_require_table_len
//
// (implemented in mquickjs_ridl_register.c)

// Entries injected into `js_global_object[]`
// NOTE: stdlib template expects `JS_RIDL_EXTENSIONS`.
#define JS_RIDL_EXTENSIONS JS_STDLIB_EXTENSIONS_GLOBAL

#define JS_STDLIB_EXTENSIONS_GLOBAL \
    JS_CFUNC_DEF("require", 1, js_ridl_require), \
    JS_PROP_CLASS_DEF("__ridl_modules", &js_ridl_modules_ns_obj), \
/* NOTE: module namespaces are exposed under __ridl_modules (module mode). */ \
/* singletons are registered as global object properties (e.g. globalThis.console) */ \
    JS_PROP_CLASS_DEF("console", &js_global_singleton_console_obj), \
/* user classes are exported on global object (global mode) */ \

#endif // MJS_RIDL_REGISTER_H