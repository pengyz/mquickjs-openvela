/****************************************************************************
 * apps/system/mqjs/lvgldemo/lvgl_backend.c
 *
 * LvglBackend vtable 的 C 实现（mquickjs-ui 绑定层的平台后端）。
 *
 * 四个函数指针按 mquickjs-ui src/backend.rs 的 LvglBackend 契约实现：
 *   create_label  -> lv_label_create(默认屏) + 初始文本
 *   create_button -> lv_button_create(默认屏) + 内嵌 label
 *   set_text      -> Label 直设 / Button 更新内嵌 label
 *   set_on_click  -> 注册 CLICKED trampoline（回程调 mqjs_ui_dispatch_click，
 *                    显式携带 JSContext*）+ DELETE trampoline（销毁时调
 *                    mqjs_ui_callback_release 注销回调桥槽位并释放 ud）
 *
 * 线程约定：本文件全部函数只在 LVGL/JS 共用线程上被调用（见
 * mqjs_lvgl_demo.c 的统一循环），因此 ctx 句柄用普通静态变量即可。
 *
 * v1 spike 限制（记入 RIDL 缺口清单）：单 JS context 宿主；onClick 每次调用
 * 追加一个监听器（DOM addEventListener 语义），不覆盖旧监听。
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdlib.h>

#include <lvgl/lvgl.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* mquickjs-ui 回程导出（libmqjs.a；声明对齐 backend.rs 的 extern "C" 签名） */

extern void mqjs_ui_dispatch_click(void *ctx, unsigned int handle);
extern void mqjs_ui_callback_release(void *ctx, unsigned int handle);
extern void mqjs_ui_backend_attach(
    void *(*create_label)(const char *text),
    void *(*create_button)(const char *text),
    void (*set_text)(void *obj, const char *text),
    void (*set_on_click)(void *obj, unsigned int handle));

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* 引擎 context（= mqjs_rs_ridl_context_new 的返回值）。demo main 在装配
 * 引擎后、任何 JS 运行前注入一次；关停时在 LVGL 销毁**之后**才释放，
 * 因此 DELETE trampoline 运行时它恒有效。 */

static void *s_js_ctx;

/* 每次点击注册的 user_data：回程显式携带 ctx（eval 返回后 TLS current
 * 栈为空，事件回程只能走 from_js_ctx —— 见 mquickjs-ui backend.rs）。 */

typedef struct
{
    void *ctx;             /* JSContext*（存活受 demo 关停顺序保证） */
    unsigned int handle;   /* 回调桥句柄（CallbackHandle::raw()） */
} click_ud_t;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void click_trampoline(lv_event_t *e)
{
    click_ud_t *ud = lv_event_get_user_data(e);

    mqjs_ui_dispatch_click(ud->ctx, ud->handle);
}

static void delete_trampoline(lv_event_t *e)
{
    click_ud_t *ud = lv_event_get_user_data(e);

    /* 风险点 d（句柄失效）：LVGL 对象消亡即注销回调桥槽位，JS 侧不再
     * 持有该 widget 的堆引用；同时释放注册记录。 */

    printf("MQJS_LVGL: LV_EVENT_DELETE -> release handle %u\n", ud->handle);
    mqjs_ui_callback_release(ud->ctx, ud->handle);
    free(ud);
}

static void *backend_create_label(const char *text)
{
    lv_obj_t *label = lv_label_create(lv_screen_active());

    lv_label_set_text(label, text);
    return label;
}

static void *backend_create_button(const char *text)
{
    lv_obj_t *btn = lv_button_create(lv_screen_active());
    lv_obj_t *label = lv_label_create(btn);

    lv_label_set_text(label, text);
    lv_obj_center(label);
    return btn;
}

static void backend_set_text(void *obj, const char *text)
{
    lv_obj_t *o = obj;

    if (lv_obj_check_type(o, &lv_label_class))
    {
        lv_label_set_text(o, text);
        return;
    }

    /* Button：更新内嵌 label（构造时的第一个 label child）。 */
    uint32_t n = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < n; i++)
    {
        lv_obj_t *child = lv_obj_get_child(o, (int32_t)i);
        if (lv_obj_check_type(child, &lv_label_class))
        {
            lv_label_set_text(child, text);
            return;
        }
    }
}

static void backend_set_on_click(void *obj, unsigned int handle)
{
    click_ud_t *ud = malloc(sizeof(*ud));

    if (ud == NULL)
    {
        printf("MQJS_LVGL: set_on_click OOM (ud)\n");
        return;
    }

    ud->ctx = s_js_ctx;
    ud->handle = handle;
    lv_obj_add_event_cb(obj, click_trampoline, LV_EVENT_CLICKED, ud);
    lv_obj_add_event_cb(obj, delete_trampoline, LV_EVENT_DELETE, ud);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/* 注入引擎 context（回调回程的 ctx 来源）。必须在任何 JS 运行前调用。 */

void mqjs_lvgl_backend_init(void *js_ctx)
{
    s_js_ctx = js_ctx;
}

/* 装配 vtable（进程级一次）。 */

void mqjs_lvgl_backend_attach(void)
{
    mqjs_ui_backend_attach(backend_create_label, backend_create_button,
                           backend_set_text, backend_set_on_click);
}
