/* Generated once by ports/openvela/setup-sim.sh (mquickjs-rs-demo repo) and
 * checked into the tree — do not edit; see ../SYNC.md for regeneration.
 * Declarations for the rs probe hooks baked into the generated stdlib table
 * by the staged template overlay (engine/mqjs_stdlib_template.c). Every app
 * TU force-includes this header after mquickjs_ridl_api.h (app Makefile
 * CFLAGS). */
#ifndef MQJS_RS_HOOKS_H
#define MQJS_RS_HOOKS_H
JSValue js_rs_version(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_rs_self_test(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
#endif
