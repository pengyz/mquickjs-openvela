/****************************************************************************
 * apps/system/mqjs/js_main_c.c
 *
 * NSH builtin: run JS scripts with the mquickjs engine on OpenVela/NuttX —
 * C-only fallback build (CONFIG_MQJS_RS=n).
 *
 * Source: ports/openvela/app/js_main.c @ mquickjs-rs-demo repo rev 94170f9
 * (Phase 1, M1-C form: C context creation + C hooks, no Rust adapter),
 * adapted to the in-tree layout. Differences vs that historical file:
 * - The platform hooks it defined (print, gc, the Date pair, load, timers)
 *   are NOT needed here: the stdlib table compiled into this image is the
 *   RIDL-variant one (gen/mqjs_ridl_stdlib.h via engine/mqjs_stdlib_impl.c),
 *   which references none of them; the Date entry points come from
 *   engine/mqjs_stdlib_impl.c.
 * - The table DOES reference the adapter-owned entry points cross-TU
 *   (console singleton trio + rsVersion/rsSelfTest probes, baked in by the
 *   template overlay). With CONFIG_MQJS_RS=y the Rust adapter archive
 *   defines them; with RS=n minimal C fallback implementations live HERE so
 *   the image links (see "C fallback stubs" below). The two forms never
 *   coexist in one image: the Makefile picks exactly one MAINSRC.
 *
 * Usage: js <script.js> [<script2.js> ...]
 *
 * Output protocol (sentinel lines, scanned by the host-side runner — NSH
 * builtins have no scriptable exit code):
 *   CASE <basename> PASS
 *   CASE <basename> FAIL: <message>
 *   CASES <passed>/<total> PASS     (summary line)
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mquickjs.h"
#include "mquickjs_ridl_api.h"
#include "mqjs_rs_hooks.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* JS heap for the script contexts. 2 MiB is ample for the case corpus;
 * the engine lives entirely inside this block. JS_NewContext() only
 * asserts a fixed minimum and fails SILENTLY if the block is too small
 * (mquickjs.h "JS_NewContext notes") — hence the explicit size + NULL
 * check in run_case. */

#define JS_HEAP_SIZE (2 * 1024 * 1024)

/* The strong stdlib def (generated table) is defined in engine/mqjs_stdlib_impl.c
 * (external linkage, const-qualified at file scope) — this image's single
 * table, shared with the CONFIG_MQJS_RS=y form. No engine header declares
 * it; the embedder owns the declaration, like any adapter symbol. */

extern const JSSTDLibraryDef js_stdlib;

/****************************************************************************
 * C fallback stubs for the Rust-adapter-owned table entry points
 *
 * Declared by the force-included gen headers (mquickjs_ridl_api.h /
 * mqjs_rs_hooks.h); referenced by the generated stdlib table inside
 * mqjs_stdlib_impl.o (a different TU) — so these definitions must be
 * non-static. With CONFIG_MQJS_RS=y the Rust adapter provides them
 * instead (js_main.c), and this file is not compiled.
 ****************************************************************************/

/* console singleton: degrade to direct stdout printing. Argument
 * rendering mirrors the M1-C js_print (strings via JS_ToCStringLen,
 * everything else via JS_PrintValueF). */

static void console_print_args(JSContext *ctx, int argc, JSValue *argv)
{
    int i;

    for (i = 0; i < argc; i++)
    {
        if (i != 0)
        {
            putchar(' ');
        }

        if (JS_IsString(ctx, argv[i]))
        {
            JSCStringBuf buf;
            const char *str;
            size_t len;
            str = JS_ToCStringLen(ctx, &len, argv[i], &buf);
            fwrite(str, 1, len, stdout);
        }
        else
        {
            JS_PrintValueF(ctx, argv[i], JS_DUMP_LONG);
        }
    }
    putchar('\n');
}

JSValue js_global_singleton_console_log(JSContext *ctx, JSValue *this_val,
                                        int argc, JSValue *argv)
{
    console_print_args(ctx, argc, argv);
    return JS_UNDEFINED;
}

JSValue js_global_singleton_console_error(JSContext *ctx, JSValue *this_val,
                                          int argc, JSValue *argv)
{
    console_print_args(ctx, argc, argv);
    return JS_UNDEFINED;
}

JSValue js_global_singleton_console_get_enabled(JSContext *ctx,
                                                JSValue *this_val,
                                                int argc, JSValue *argv)
{
    /* The C fallback console is always available (stdout). */
    return JS_NewBool(1);
}

/* rs probes: report the C-checkpoint state (no Rust adapter in the image).
 * The string constant is static; JS_NewString copies it into the JS heap
 * (tracing GC) — nothing to free here. */

JSValue js_rs_version(JSContext *ctx, JSValue *this_val, int argc,
                      JSValue *argv)
{
    return JS_NewString(ctx, "mqjs C checkpoint (CONFIG_MQJS_RS=n)");
}

JSValue js_rs_self_test(JSContext *ctx, JSValue *this_val, int argc,
                        JSValue *argv)
{
    /* Minimal self-check on the live context: engine heap allocation +
     * string round-trip (JS_NewString -> JS_ToCString, byte-exact). The
     * returned string is owned by the tracing GC; no explicit free. */
    JSCStringBuf buf;
    JSValue s;
    const char *str;
    int ok;

    s = JS_NewString(ctx, "c-self-test");
    str = JS_IsException(s) ? NULL : JS_ToCString(ctx, s, &buf);
    ok = str != NULL && strcmp(str, "c-self-test") == 0;

    printf("mqjs rs_self_test (C fallback): %s\n", ok ? "ok" : "FAILED");
    return JS_NewInt32(ctx, ok ? 0 : -1);
}

/****************************************************************************
 * Case runner
 ****************************************************************************/

static char *read_file(const char *path, size_t *len_out)
{
    FILE *f;
    char *buf;
    long len;

    f = fopen(path, "rb");
    if (f == NULL)
    {
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0 || (len = ftell(f)) < 0)
    {
        fclose(f);
        return NULL;
    }
    rewind(f);

    buf = malloc((size_t)len + 1);
    if (buf == NULL)
    {
        fclose(f);
        return NULL;
    }

    if (len > 0 && fread(buf, 1, (size_t)len, f) != (size_t)len)
    {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);

    buf[len] = '\0';
    *len_out = (size_t)len;
    return buf;
}

static const char *basename_of(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

/* Some editors add a UTF-8 BOM; QuickJS doesn't accept it. */

static void strip_bom(char *buf, size_t *len)
{
    if (*len >= 3 && (unsigned char)buf[0] == 0xef
        && (unsigned char)buf[1] == 0xbb && (unsigned char)buf[2] == 0xbf)
    {
        *len -= 3;
        memmove(buf, buf + 3, *len + 1);
    }
}

static int run_case(const char *path)
{
    const char *name = basename_of(path);
    size_t len;
    char *buf;
    void *mem;
    JSContext *ctx;
    JSValue rv;
    int failed;

    buf = read_file(path, &len);
    if (buf == NULL)
    {
        printf("CASE %s FAIL: cannot read file\n", name);
        return 1;
    }
    strip_bom(buf, &len);

    mem = malloc(JS_HEAP_SIZE);
    if (mem == NULL)
    {
        printf("CASE %s FAIL: JS heap alloc failed\n", name);
        free(buf);
        return 1;
    }

    ctx = JS_NewContext(mem, JS_HEAP_SIZE, &js_stdlib);
    if (ctx == NULL)
    {
        printf("CASE %s FAIL: JS_NewContext failed (heap too small?)\n", name);
        free(mem);
        free(buf);
        return 1;
    }

    rv = JS_Eval(ctx, buf, len, path, 0);
    failed = JS_IsException(rv);
    if (!failed)
    {
        printf("CASE %s PASS\n", name);
    }
    else
    {
        /* Minimal error reporting: JS_ToCString covers thrown strings,
         * primitives and Error objects (Error.prototype.toString is in the
         * stdlib table, so a SyntaxError reports as "SyntaxError: ...").
         * Validated in Phase 1 (M1-C): tiny_err reports SyntaxError text
         * and does not crash. */

        JSValue exc = JS_GetException(ctx);
        JSCStringBuf cbuf;
        const char *msg = JS_ToCString(ctx, exc, &cbuf);
        printf("CASE %s FAIL: %s\n", name,
               msg != NULL ? msg : "<unprintable error>");
    }

    JS_GC(ctx);
    JS_FreeContext(ctx);
    free(mem);
    free(buf);
    return failed ? 1 : 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int js_main(int argc, char **argv)
{
    int i;
    int total;
    int passed;

    if (argc < 2)
    {
        printf("usage: js <script.js> [<script2.js> ...]\n");
        return 2;
    }

    total = argc - 1;
    passed = 0;
    for (i = 1; i < argc; i++)
    {
        passed += run_case(argv[i]) == 0 ? 1 : 0;
    }

    printf("CASES %d/%d PASS\n", passed, total);
    return passed == total ? 0 : 1;
}
