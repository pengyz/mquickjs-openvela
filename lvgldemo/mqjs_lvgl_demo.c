/****************************************************************************
 * apps/system/mqjs/lvgldemo/mqjs_lvgl_demo.c
 *
 * B0 spike 宿主 app：mquickjs-ui（RIDL 组件类）+ LVGL 的端到端演示。
 *
 * 职责链（全部单线程——LVGL 事件循环与 JS 引擎共用一个 NuttX task）：
 *   1. LVGL init（镜像 apps/examples/lvgldemo 的 lv_nuttx_init 流程）
 *   2. 引擎装配：mqjs_rs_ridl_context_new（RIDL 聚合含 Label/Button，
 *      来自 libmqjs.a；镜像内 js_stdlib/引擎对象由 mqjs app 的 CSRCS
 *      提供，最终镜像一次性分组链接）
 *   3. 后端装配：mqjs_lvgl_backend_init/attach（本目录 lvgl_backend.c）
 *   4. eval 内嵌 demo JS：创建组件、注册 onClick（回调桥）
 *   5. 统一循环：lv_timer_handler + usleep —— 脚本返回后 LVGL 事件照常
 *      驱动 JS 回调（风险点 c）
 *
 * 四风险点覆盖：
 *   a) 回调内 GC 压力：demo JS 的 handler 每次点击分配 500 个字符串再
 *      更新 UI（触发 minor GC；多次点击后进入压缩路径，回调桥槽位须随
 *      JSGCRef 重定位）。
 *   b) 同线程：整个 demo 单 task；点击路径 LVGL trampoline -> dispatch ->
 *      JS_Call 就在同一调用栈（无跨线程队列）。
 *   c) 脚本返回后回调仍触发：eval 在循环开始前完成。
 *   d) LV_EVENT_DELETE 句柄失效：RISK_D_DELAY_SEC 后 demo 主动删除按钮
 *      （lv_obj_del），DELETE trampoline 注销回调桥槽位；此后点击落空、
 *      UI 不崩。退出阶段同理（先 lv_nuttx_deinit 后 context_free，保证
 *      注销发生在 ctx 存活时）。
 *
 * 输出哨兵（自动化判定）：MQJS_LVGL: <stage>… / MQJS_LVGL: DONE
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/boardctl.h>

#include <lvgl/lvgl.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* JS 堆（与 js builtin 同量级：demo handler 的 GC 压力测试需要余量）。 */
#define JS_HEAP_SIZE (2 * 1024 * 1024)

#define JS_ERRBUF_SIZE 256

/* 风险点 d 定时器：开机后第 N 秒删除演示按钮。 */
#define RISK_D_DELAY_SEC 12

/* 点击注入：真实触摸路径（X11 -> /dev/input0）由 lvgldemo 已验证；本 demo
 * 的自动化证据改用 lv_event_send 定时发送 CLICKED —— 与触摸走同一条
 * 事件分发路径（filter 匹配 -> 注册的 event cb -> click trampoline ->
 * dispatch -> JS_Call），覆盖风险 a/c（eval 返回后由事件循环驱动回调）。
 * 首拍 4s，周期 4s。 */
#define RISK_AC_PERIOD_SEC 4

/* 总演示时长：到点走干净关停（自动化跑批用）。 */
#define DEMO_TOTAL_SEC 40

#undef NEED_BOARDINIT

#if defined(CONFIG_BOARDCTL) && !defined(CONFIG_NSH_ARCHINIT)
#  define NEED_BOARDINIT 1
#endif

/****************************************************************************
 * Rust adapter bridge（libmqjs.a —— 与 js builtin 同一归档，符号经最终
 * 镜像的 --start-group 解析；lvgl_backend.c 声明 mqjs_ui_* 同源）。 */

extern void *mqjs_rs_ridl_context_new(unsigned long heap_bytes);
extern void mqjs_rs_ridl_context_free(void *ctx);
extern int mqjs_rs_ridl_eval(void *ctx, const unsigned char *script,
                             unsigned long len, unsigned char *err_buf,
                             unsigned long err_cap);

/* lvgl_backend.c（本目录） */
extern void mqjs_lvgl_backend_init(void *js_ctx);
extern void mqjs_lvgl_backend_attach(void);

/****************************************************************************
 * Demo script（内嵌；B0 不做脚本装载，缺口清单含 ROM 脚本载体）
 ****************************************************************************/

static const char DEMO_JS[] =
    "var title = new Label();\n"
    "title.setText(\"mquickjs-ui + LVGL\");\n"
    "var info = new Label();\n"
    "info.setText(\"clicks: 0\");\n"
    "var btn = new Button();\n"
    "btn.setText(\"GC stress click\");\n"
    "var n = 0;\n"
    "btn.onClick(function () {\n"
    "    console.log(\"handler entered, n=\" + n);\n"
    "    n = n + 1;\n"
    "    /* 风险点 a：回调内制造分配压力（~200KB/次），驱动 minor GC /\n"
    "     * 堆压缩，验证回调桥槽位随 JSGCRef 重定位、LVGL 对象指针不受影响。 */\n"
    "    var junk = [];\n"
    "    for (var i = 0; i < 2000; i = i + 1) {\n"
    "        junk.push(\"gc-stress-\" + i + \"-\" + (n * 1000 + i));\n"
    "    }\n"
    "    info.setText(\"clicks: \" + n + \"  junk: \" + junk.length);\n"
    "    console.log(\"click \" + n + \" handled, junk=\" + junk.length);\n"
    "});\n"
    "console.log(\"script returned; event loop keeps driving callbacks\");\n";

/****************************************************************************
 * Private Data
 ****************************************************************************/

static volatile bool s_quit;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* 风险点 d：删除屏上第一个 Button（demo 创建的那个）。 */

static lv_obj_t *find_first_button(void)
{
    lv_obj_t *screen = lv_screen_active();
    uint32_t n = lv_obj_get_child_count(screen);

    for (uint32_t i = 0; i < n; i++)
    {
        lv_obj_t *child = lv_obj_get_child(screen, (int32_t)i);
        if (lv_obj_check_type(child, &lv_button_class))
        {
            return child;
        }
    }

    return NULL;
}

/* spike 布局兜底：RIDL v1 无布局/样式 API（缺口清单 #1），宿主在 eval 后
 * 按创建序做最小排版（label 依次顶部居中纵排、button 屏幕居中）。这是
 * 宿主 app 对自家屏幕的排布权，不属于绑定层实现。 */
static void demo_layout(void)
{
    lv_obj_t *screen = lv_screen_active();
    uint32_t n = lv_obj_get_child_count(screen);
    uint32_t label_idx = 0;

    for (uint32_t i = 0; i < n; i++)
    {
        lv_obj_t *child = lv_obj_get_child(screen, (int32_t)i);
        if (lv_obj_check_type(child, &lv_label_class))
        {
            lv_obj_align(child, LV_ALIGN_TOP_MID, 0, 16 + label_idx * 48);
            label_idx++;
        }
        else if (lv_obj_check_type(child, &lv_button_class))
        {
            lv_obj_center(child);
        }
    }
}

static void risk_d_timer_cb(lv_timer_t *timer)
{
    lv_obj_t *btn = find_first_button();

    if (btn == NULL)
    {
        printf("MQJS_LVGL: RISK-D button already gone\n");
        lv_timer_del(timer);
        return;
    }

    printf("MQJS_LVGL: RISK-D deleting button (handle release expected)\n");
    lv_obj_del(btn);
    lv_timer_del(timer);
}

/* 风险 a/c：定时向演示按钮发送 CLICKED（同真实触摸的事件分发路径），
 * 驱动 JS 回调；按钮被 RISK-D 删除后自动停发。 */

static void risk_ac_timer_cb(lv_timer_t *timer)
{
    lv_obj_t *btn = find_first_button();

    if (btn == NULL)
    {
        printf("MQJS_LVGL: RISK-A/C button gone (post-delete, sends stop)\n");
        lv_timer_del(timer);
        return;
    }

    printf("MQJS_LVGL: RISK-A/C send CLICKED\n");
    /* LVGL 9.1 的事件 API：对象级入口是 lv_obj_send_event（lv_event_send
     * 是 (list, e, preprocess) 内部形态——传 (obj, code, param) 会把 code
     * 当指针，正是首跑 SIGSEGV 的根因）。 */
    lv_obj_send_event(btn, LV_EVENT_CLICKED, NULL);
}

static void quit_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    printf("MQJS_LVGL: demo time up, quitting\n");
    s_quit = true;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int mqjslvgl_main(int argc, char *argv[])
{
    lv_nuttx_dsc_t info;
    lv_nuttx_result_t result;
    void *ctx;
    unsigned char errbuf[JS_ERRBUF_SIZE];

    (void)argc;
    (void)argv;

    if (lv_is_initialized())
    {
        printf("MQJS_LVGL: FAIL lvgl already initialized\n");
        return 1;
    }

#ifdef NEED_BOARDINIT
    boardctl(BOARDIOC_INIT, 0);
#endif

    /* 1) LVGL init（lvgldemo 模式） */

    lv_init();
    lv_nuttx_dsc_init(&info);

#ifdef CONFIG_LV_USE_NUTTX_LCD
    info.fb_path = "/dev/lcd0";
#endif

#ifdef CONFIG_INPUT_TOUCHSCREEN
    info.input_path = "/dev/input0";
#endif

    lv_nuttx_init(&info, &result);
    if (result.disp == NULL)
    {
        printf("MQJS_LVGL: FAIL lv_nuttx_init\n");
        return 1;
    }

    /* 2) 引擎装配（RIDL 聚合含 Label/Button；ctx 须由 Rust 侧创建） */

    ctx = mqjs_rs_ridl_context_new(JS_HEAP_SIZE);
    if (ctx == NULL)
    {
        printf("MQJS_LVGL: FAIL ridl context create\n");
        lv_nuttx_deinit(&result);
        lv_deinit();
        return 1;
    }
    printf("MQJS_LVGL: engine ready\n");

    /* 3) 后端装配（vtable 注入 + 回程 ctx 绑定） */

    mqjs_lvgl_backend_init(ctx);
    mqjs_lvgl_backend_attach();

    /* 4) demo 脚本：组件创建 + onClick 注册（eval 返回后循环接管） */

    if (mqjs_rs_ridl_eval(ctx, (const unsigned char *)DEMO_JS,
                          sizeof(DEMO_JS) - 1, errbuf, sizeof(errbuf)) != 0)
    {
        printf("MQJS_LVGL: FAIL eval: %s\n",
               errbuf[0] != '\0' ? (const char *)errbuf : "<unprintable>");
        mqjs_rs_ridl_context_free(ctx);
        lv_nuttx_deinit(&result);
        lv_deinit();
        return 1;
    }
    printf("MQJS_LVGL: eval ok (script returned; risk-c armed)\n");
    demo_layout();

    /* 风险点 a/c/d + 总时长定时器（LVGL 时基，回调仍在同一线程）。
     * repeat_count 默认 -1（无限），timer 回调内部自行 lv_timer_del。 */
    lv_timer_create(risk_ac_timer_cb, RISK_AC_PERIOD_SEC * 1000, NULL);
    lv_timer_create(risk_d_timer_cb, RISK_D_DELAY_SEC * 1000, NULL);
    lv_timer_create(quit_timer_cb, DEMO_TOTAL_SEC * 1000, NULL);

    /* 5) 统一循环（sim 无 libuv：lvgldemo 同款手动泵） */

    while (!s_quit)
    {
        uint32_t idle = lv_timer_handler();
        idle = idle ? idle : 1;
        usleep(idle * 1000);
    }

    /* 关停顺序：先销毁 LVGL（DELETE trampoline 在 ctx 存活时注销回调桥
     * 槽位），再释放引擎（JS 堆 + RIDL CtxExt）。 */

    lv_nuttx_deinit(&result);
    lv_deinit();
    mqjs_rs_ridl_context_free(ctx);

    printf("MQJS_LVGL: DONE\n");
    return 0;
}
