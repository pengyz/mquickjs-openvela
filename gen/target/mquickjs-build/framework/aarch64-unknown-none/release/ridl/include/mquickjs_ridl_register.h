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
#define JS_CLASS_GLOBAL_LABEL (JS_CLASS_USER + 0)

void js_global_class_label_class(void);
#define JS_CLASS_GLOBAL_BUTTON (JS_CLASS_USER + 1)

void js_global_class_button_class(void);

#ifndef JS_CLASS_COUNT
#define JS_CLASS_COUNT (JS_CLASS_USER + 2)
#endif

/* ----------------------------
 * RIDL module/user class definitions
 * (JSPropDef/JSClassDef/constructor stubs)
 * ----------------------------
 */

JSValue js_global_class_label_constructor(
    JSContext *ctx,
    JSValue *this_val,
    int argc,
    JSValue *argv
);

void js_global_class_label_finalizer(
    JSContext *ctx,
    void *opaque
);

// NOTE: class-level gc_mark callback 已废弃，不再声明。
// `Traced<T>` 基于引擎 JSGCRef（mark + 重定位均由引擎自动处理），
// 因此注册表中的 gc_mark 槽位恒为 NULL。
// 详见 rust_glue.rs.j2 中同名说明与 docs/knowledge/gotcha_mquickjs_gc_mark_signature.md。
JSValue js_global_class_label_set_text(
    JSContext *ctx,
    JSValue *this_val,
    int argc,
    JSValue *argv
);

static const JSPropDef js_global_class_label_proto_funcs[] = {
    JS_CFUNC_DEF("setText", 1, js_global_class_label_set_text),
    JS_PROP_END,
};

const JSClassDef js_global_class_label_class_def =
    JS_CLASS_DEF(
        "Label",
        1,
        js_global_class_label_constructor,
        JS_CLASS_GLOBAL_LABEL,
        NULL,
        js_global_class_label_proto_funcs,
        NULL,
        js_global_class_label_finalizer,
        NULL  // gc_mark: 恒为 NULL（Traced<T> 由 JSGCRef 自动标记/重定位）
    );

void js_global_class_label_class(void) {
    (void)&js_global_class_label_class_def;
    (void)&js_global_class_label_proto_funcs;
    (void)&js_global_class_label_constructor;
    (void)&js_global_class_label_finalizer;
}

JSValue js_global_class_button_constructor(
    JSContext *ctx,
    JSValue *this_val,
    int argc,
    JSValue *argv
);

void js_global_class_button_finalizer(
    JSContext *ctx,
    void *opaque
);

// NOTE: class-level gc_mark callback 已废弃，不再声明。
// `Traced<T>` 基于引擎 JSGCRef（mark + 重定位均由引擎自动处理），
// 因此注册表中的 gc_mark 槽位恒为 NULL。
// 详见 rust_glue.rs.j2 中同名说明与 docs/knowledge/gotcha_mquickjs_gc_mark_signature.md。
JSValue js_global_class_button_set_text(
    JSContext *ctx,
    JSValue *this_val,
    int argc,
    JSValue *argv
);
JSValue js_global_class_button_on_click(
    JSContext *ctx,
    JSValue *this_val,
    int argc,
    JSValue *argv
);

static const JSPropDef js_global_class_button_proto_funcs[] = {
    JS_CFUNC_DEF("setText", 1, js_global_class_button_set_text),
    JS_CFUNC_DEF("onClick", 1, js_global_class_button_on_click),
    JS_PROP_END,
};

const JSClassDef js_global_class_button_class_def =
    JS_CLASS_DEF(
        "Button",
        2,
        js_global_class_button_constructor,
        JS_CLASS_GLOBAL_BUTTON,
        NULL,
        js_global_class_button_proto_funcs,
        NULL,
        js_global_class_button_finalizer,
        NULL  // gc_mark: 恒为 NULL（Traced<T> 由 JSGCRef 自动标记/重定位）
    );

void js_global_class_button_class(void) {
    (void)&js_global_class_button_class_def;
    (void)&js_global_class_button_proto_funcs;
    (void)&js_global_class_button_constructor;
    (void)&js_global_class_button_finalizer;
}

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
/* user classes are exported on global object (global mode) */ \
    JS_PROP_CLASS_DEF("Label", &js_global_class_label_class_def), \
    JS_PROP_CLASS_DEF("Button", &js_global_class_button_class_def), \
/* singletons are registered as global object properties (e.g. globalThis.console) */ \
    JS_PROP_CLASS_DEF("console", &js_global_singleton_console_obj), \
/* user classes are exported on global object (global mode) */ \

#endif // MJS_RIDL_REGISTER_H