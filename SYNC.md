# apps/system/mqjs — SYNC / 对账手册

`js` NSH builtin 的树内集成（mquickjs 引擎 + Rust stdlib adapter + RIDL
console）。本目录是**纯新增**目录：对 openvela 树零受跟踪文件改动，全部
内容要么是 vendored 源码，要么是**集成期生成后入库**的产物（`gen/`）。

上游（真源）在 mquickjs-rs-demo 仓库（下称 `<repo>`，本机路径
`~/workspace/mquickjs-rs-demo`）；本文件记录 vendored 来源 rev、生成产物
的再生成命令、以及重生成后的 diff 自检命令。

## 目录布局

```
apps/system/mqjs/
├── Kconfig / Make.defs / Makefile / CMakeLists.txt / js_main.c
├── engine/            vendored C 源（见下"来源 rev"）
├── gen/               集成期生成、检查入库（git 可见，勿手改）
└── rust/              vendored cargo 工作区（adapter + 全部一等 crate 闭包）
```

`rust/target/` 是构建状态（本目录 .gitignore 排除）；`apps/staging/` 里的
`libmqjs.a` 由 app Makefile 的 `context::` 规则从
`rust/target/x86_64-unknown-linux-gnu/release/libmqjs.a` 复制，同样只是
构建状态。

## 来源 rev（2026-10-09 落树时点；3.4 起为上游工作区状态）

> **3.4 备注**：Phase 3.4（QEMU arm64 no_std × RIDL）对上游
> mquickjs-rs-demo 仓库做了**未 commit 的工作区改动**（feature 卫生拆分、
> glue/context_ext 模板去 std prelude、stdlib no_std 模式、adapter 双模式
> ——详见下节"Phase 3.4"）。本目录 vendored 副本同步的是该**工作区状态**；
> 上表 rev 指向上游改动**之前**的 HEAD（`aba6b87`），待 Phase 3.5 上游
> 提交后统一更新为提交 rev。

| 内容 | 来源 | rev |
|------|------|-----|
| engine/ 的引擎核心（mquickjs.c cutils.c dtoa.c libm.c）与其镜像 TU 依赖的全部私有头（mquickjs.h mquickjs_priv.h mquickjs_opcode.h cutils.h dtoa.h libm.h list.h softfp_template.h softfp_template_icvt.h mquickjs_build.h） | `<repo>/deps/mquickjs`（子模块，工作区 clean） | `b51625d646986bc46ff14793c50c5ea53d9d715a` |
| engine/mqjs_stdlib_impl.c、engine/mqjs_require.c（=require.c）、engine/mqjs_stdlib_template.c（**含 rs overlay**，见下） | `<repo>/deps/mquickjs-rs` | `aba6b8797fe9640c6b24ecdce81f943403653d99`（`<repo>` 主仓 HEAD，同 rev 覆盖下列全部） |
| rust/mquickjs-rs/、rust/mquickjs-sys/、rust/mquickjs-ridl-glue/、rust/stdlib/（=ridl-modules/stdlib）、rust/adapter/（=ports/openvela/rust） | `<repo>` 同上 | 同上 |
| gen/ 聚合产物、gen/ 头、gen/ 框架产物 | `<repo>` 的 ridl-builder / mquickjs-build / mqjs_stdlib host 工具 | 同上（工具由同 rev 源码构建） |
| js_main.c、Kconfig、Make.defs | `<repo>/ports/openvela/app/` | 同上（js_main.c 代码逐字，仅头注释树内化） |

### vendored 内容差异（相对上游的显式适配，共 3 处 + 接线文件）

1. **engine/mqjs_stdlib_template.c**：应用了 rs 探针 overlay——在锚点
   `JS_PROP_NULL_DEF("globalThis", 0 ),` 后插入
   `JS_CFUNC_DEF("rsVersion", 0, js_rs_version),` 与
   `JS_CFUNC_DEF("rsSelfTest", 0, js_rs_self_test),`。
   （与 setup-sim.sh step 1b 的 sed 完全同文；上游模板保持 pristine。）
2. **rust/mquickjs-sys/build.rs**："VENDORED ADAPTATION" 注释块。上游只认
   `mquickjs_build_output.json` 里的**绝对路径**；入库产物为可携带性改用
   **相对 gen/（mquickjs.build.toml 所在目录）的路径**，此处新增 `resolve()`
   把相对路径 join + canonicalize 成绝对路径再 `cargo:rustc-env` 输出
   （下游 bindgen 的 cwd 是各自包根，必须绝对）；canonicalize 失败时打
   `cargo:warning`（防相对路径被下游 cwd 敏感消费方静默误用）。其余逻辑
   逐字未动。
3. **rust/stdlib/build.rs**：上游在构建期调用宿主二进制
   `ridl-tool module src/stdlib.ridl <OUT_DIR>`（经 `MQUICKJS_RIDL_TOOL`）。
   树内构建要求零宿主工具依赖，改为把**预生成**的 api.rs/glue.rs（
   `stdlib/generated/`，已验证对同一 .ridl 输入字节确定）复制进 OUT_DIR。
   include!/宏路径（mquickjs-rs 的 `ridl_include_module!`）不变。
4. **rust/adapter/build.rs**：仅注释级漂移——聚合产物位置的描述按树内
   布局改写（`MQUICKJS_RIDL_TARGET_DIR=../../gen` 指向 app 的 gen/），
   功能代码逐字一致。
5. **接线文件**（树内特有，上游无对应物）：rust/Cargo.toml（workspace）、
   rust/.cargo/config.toml、rust/mquickjs.ridl.toml、rust/adapter/Cargo.toml
   （包改名 `mqjs`、路径依赖指向 vendored 兄弟 crate；上游 adapter 的
   `[workspace]` 空表在树内**必须去掉**——adapter 是 rust/ workspace 的
   成员，保留会触发 cargo "multiple workspace roots" 报错）、
   rust/stdlib/Cargo.toml（同上路径改写）、rust/Cargo.lock、
   gen/mquickjs.build.toml、
   gen/target/.../mquickjs_build_output.json（路径改写为相对，见下）。
   上游 adapter 独有的 `.cargo/`、`rust-toolchain.toml`、
   `mquickjs.ridl.toml` 在树内上移到 rust/ workspace 级（同名语义）。

## gen/ 产物与消费方

| 产物 | 生成者 | 消费方 |
|------|--------|--------|
| gen/mquickjs_atom.h、gen/mqjs_ridl_stdlib.h | `mqjs_stdlib` host 工具（**overlay 后模板** + mquickjs_build.c，`-DMQUICKJS_ENABLE_RIDL_EXTENSIONS`） | 镜像 C 轨（app Makefile `-Igen`；强 `js_stdlib` 表含 console + rs 探针） |
| gen/mqjs_rs_hooks.h | setup-sim.sh step 2a 的静态内容 | 每个 app TU 强制包含（声明 js_rs_* 钩子） |
| gen/mquickjs_ridl_api.h、gen/mquickjs_ridl_register.h、gen/mquickjs_ridl_register.c、gen/mquickjs_ridl_module_class_ids.h、gen/ridl-manifest.json | `ridl-builder aggregate`（app=**mqjs**） | api.h/register.h：镜像 C 轨（强制包含/寄存器 TU 源）+ 宿主工具输入；register.c：镜像 TU（`JS_RIDL_StdlibInit` + require 表）；manifest：溯源快照 |
| gen/ridl/apps/mqjs/aggregate/{ridl_symbols.rs,ridl_context_ext.rs,ridl_bootstrap.rs} | 同上 | adapter build.rs（经 `MQUICKJS_RIDL_TARGET_DIR=../../gen`）→ OUT_DIR |
| gen/mquickjs.build.toml | 手写（改写自 `<repo>` 根的同名文件，app_manifest 指向树内 adapter；该字段仅作 schema 兼容，build.rs 未消费） | mquickjs-sys/build.rs（经 `MQUICKJS_BUILD_TOML=../../gen/mquickjs.build.toml`） |
| gen/target/mquickjs-build/framework/x86_64-unknown-linux-gnu/release/ridl/{include/,mquickjs_build_output.json} | `mquickjs-build build`（ridl 变体） | include/：mquickjs-rs 的 bindgen + class-id 生成；json：mquickjs-sys/build.rs 的产物定位（**路径相对 gen/**，由 vendored 适配解析） |
| gen/target/mquickjs-build/framework/aarch64-unknown-none/release/ridl/{include/,mquickjs_build_output.json} | 同上，`--target aarch64-unknown-none`（3.4 起入库） | 同上，供 aarch64 裸机构建（CONFIG_MQJS_RS=y + CONFIG_ARCH=arm64）解析；include/ 与 x86_64 份**逐字节相同**（头与 triple 无关，见 diff 自检 7） |

**不入库**：框架产物的 `lib/` 归档与 base 变体——`MQJS_ENGINE_LINK=external`
（rust/.cargo/config.toml）下适配器归档不得 bundle 引擎对象，镜像的
`js_stdlib` 由 C 轨 CSRCS 提供；bindgen 只读 include/ 头。

## 再生成命令（在 `<repo>` 内执行）

前置：`<repo>` 处于上表 rev；`cargo run -p ridl-tool/--build` 可用
（ridl-builder/mquickjs-build 需要先 build）。以下 `APP` 指本 app 目录，
`REPO` 指 `<repo>`。

```bash
APP=~/workspace/openvela/apps/system/mqjs
REPO=~/workspace/mquickjs-rs-demo
cd "$REPO"
cargo build -q -p ridl-tool -p ridl-builder -p mquickjs-build

# ① RIDL 聚合（写入 $APP/rust/adapter/target/ridl/apps/mqjs/aggregate；
#    adapter 带 [workspace] 空表（上游形态）时 ridl-builder 的 cargo metadata
#    解析到包级 target，树内 vendored adapter 同理——聚合落 adapter/target/）
cargo run -q -p ridl-builder -- aggregate \
    --cargo-toml "$APP/rust/adapter/Cargo.toml" --intent build
AGG="$APP/rust/adapter/target/ridl/apps/mqjs/aggregate"
cp -f "$AGG"/mquickjs_ridl_api.h "$AGG"/mquickjs_ridl_register.h \
      "$AGG"/mquickjs_ridl_register.c "$AGG"/mquickjs_ridl_module_class_ids.h \
      "$AGG"/ridl-manifest.json "$APP/gen/"
mkdir -p "$APP/gen/ridl/apps/mqjs/aggregate"
cp -f "$AGG"/ridl_symbols.rs "$AGG"/ridl_context_ext.rs \
      "$AGG"/ridl_bootstrap.rs "$APP/gen/ridl/apps/mqjs/aggregate/"

# ② stdlib 模块胶水（vendored stdlib/generated/；字节确定，双跑 diff 自检）
mkdir -p /tmp/mqjs-mod-a /tmp/mqjs-mod-b
target/debug/ridl-tool module "$APP/rust/stdlib/src/stdlib.ridl" /tmp/mqjs-mod-a
target/debug/ridl-tool module "$APP/rust/stdlib/src/stdlib.ridl" /tmp/mqjs-mod-b
diff -r /tmp/mqjs-mod-a /tmp/mqjs-mod-b
cp -f /tmp/mqjs-mod-a/api.rs /tmp/mqjs-mod-a/glue.rs "$APP/rust/stdlib/generated/"

# ③ atom 头 + ridl stdlib 表（host 工具 = overlay 后模板 + mquickjs_build.c；
#    引擎私有头用 $REPO/deps/mquickjs 的 pristine 版本，含 mquickjs_build.h）
HOST="-O2 -D_GNU_SOURCE -fno-math-errno -fno-trapping-math -I$REPO/deps/mquickjs -I$APP/engine -I$APP/gen -DMQUICKJS_ENABLE_RIDL_EXTENSIONS"
mkdir -p /tmp/mqjs-hosttool
cc $HOST -c "$APP/engine/mqjs_stdlib_template.c" -o /tmp/mqjs-hosttool/stdlib.o
cc $HOST -c "$REPO/deps/mquickjs/mquickjs_build.c" -o /tmp/mqjs-hosttool/build.o
cc -O2 -o /tmp/mqjs-hosttool/mqjs_stdlib /tmp/mqjs-hosttool/stdlib.o /tmp/mqjs-hosttool/build.o
/tmp/mqjs-hosttool/mqjs_stdlib -a > "$APP/gen/mquickjs_atom.h"
/tmp/mqjs-hosttool/mqjs_stdlib    > "$APP/gen/mqjs_ridl_stdlib.h"
# （"Too many properties, consider increasing ATOM_ALIGN" 为既有非致命钳制告警）

# ④ 框架产物（ridl 变体；先落临时目录，include/ 入库 + JSON 改写相对路径）
OUT=/tmp/mqjs-framework/x86_64-unknown-linux-gnu/release/ridl
rm -rf /tmp/mqjs-framework
cargo run -q -p mquickjs-build -- build \
    --mquickjs-dir "$REPO/deps/mquickjs" \
    --ridl-register-h "$APP/gen/mquickjs_ridl_register.h" --out "$OUT"
FW="$APP/gen/target/mquickjs-build/framework/x86_64-unknown-linux-gnu/release/ridl"
rm -rf "$FW"; mkdir -p "$FW"
cp -a "$OUT/include" "$FW/include"
# JSON 手工改写：lib_dir/include_dir/inputs 的绝对路径 → 相对 gen/ 的路径
# （lib_dir 指 gen/target/.../lib，目录本身不入库；inputs 的引擎源指 ../engine/...）。
# 完成后用下面"diff 自检"第 5 条验证。

# ④b aarch64 框架产物（Phase 3.4；三重无关性：include/ 与 x86_64 份逐字节
#     相同——头是 triple 无关的 C 头，bindgen 在消费侧按 TARGET 自行重跑；
#     入库的只有 include/ + json，lib/（真正的 aarch64 引擎对象，由
#     clang --target 编译）与 x86_64 份同样不入库（MQJS_ENGINE_LINK=external）
OUT=/tmp/mqjs-framework-a64/aarch64-unknown-none/release/ridl
rm -rf /tmp/mqjs-framework-a64
cargo run -q -p mquickjs-build -- build \
    --mquickjs-dir "$REPO/deps/mquickjs" \
    --ridl-register-h "$APP/gen/mquickjs_ridl_register.h" \
    --target aarch64-unknown-none --out "$OUT"
FWA="$APP/gen/target/mquickjs-build/framework/aarch64-unknown-none/release/ridl"
rm -rf "$FWA"; mkdir -p "$FWA"
cp -a "$OUT/include" "$FWA/include"
# JSON：以 x86_64 份为模板，路径中的 triple 替换为 aarch64-unknown-none
# （相对 gen/ 的形式；用下面"diff 自检"第 5b 条验证）。
```

## 重生成 diff 自检

```bash
APP=~/workspace/openvela/apps/system/mqjs
REPO=~/workspace/mquickjs-rs-demo

# 1) 聚合与旧 sim 应用（mqjs_openvela_adapter，内容与 app-id 无关）逐字节一致
#    （注意：聚合内容与模板挂钩——上游模板变更后两边都要用同一版 ridl-tool
#    重新生成再比对）
OLD=$REPO/ports/openvela/rust/target/ridl/apps/mqjs_openvela_adapter/aggregate
NEW=$APP/rust/adapter/target/ridl/apps/mqjs/aggregate
for f in mquickjs_ridl_api.h mquickjs_ridl_register.h mquickjs_ridl_register.c \
         ridl_symbols.rs ridl_context_ext.rs ridl_bootstrap.rs; do
    cmp "$OLD/$f" "$NEW/$f" || echo "DIFF $f"; done

# 2) gen/ 顶层聚合头 == 聚合产物
for f in mquickjs_ridl_api.h mquickjs_ridl_register.h mquickjs_ridl_register.c \
         mquickjs_ridl_module_class_ids.h; do
    cmp "$APP/gen/$f" "$NEW/$f" || echo "DIFF gen/$f"; done# 3) 生成的 stdlib 表含 rs 探针与 RIDL console
grep -q js_rs_version              "$APP/gen/mqjs_ridl_stdlib.h"
grep -q js_global_singleton_console_log "$APP/gen/mqjs_ridl_stdlib.h"

# 4) gen/ 头与 setup-sim.sh（既有验收流）产物逐字节一致
for f in mquickjs_atom.h mqjs_ridl_stdlib.h; do
    cmp "$REPO/ports/openvela/app/gen/$f" "$APP/gen/$f" || echo "DIFF $f"; done

# 5) JSON 相对路径解析正确（vendored build.rs 的 resolve 语义）
python3 - <<'EOF'
import json, pathlib
fw = pathlib.Path("$APP/gen")  # shell 无法展开时手动替换
j = json.loads((fw/"target/mquickjs-build/framework/x86_64-unknown-linux-gnu/release/ridl/mquickjs_build_output.json").read_text())
for k in ("lib_dir","include_dir"):
    assert not pathlib.Path(j[k]).is_absolute(), j[k]
    assert (fw/j[k]).exists() or k=="lib_dir", (k, j[k])
for i in j["inputs"]:
    assert not pathlib.Path(i).is_absolute(), i
    assert (fw/i).exists(), i
print("JSON relative paths OK")
EOF

# 5b) aarch64 框架 json 同语义（Phase 3.4；lib_dir 不入库的 cargo:warning
#     为预期行为——MQJS_ENGINE_LINK=external 下 lib/ 本就不消费）
python3 - <<'EOF'
import json, pathlib
fw = pathlib.Path("$APP/gen")  # shell 无法展开时手动替换
j = json.loads((fw/"target/mquickjs-build/framework/aarch64-unknown-none/release/ridl/mquickjs_build_output.json").read_text())
for k in ("lib_dir","include_dir"):
    assert not pathlib.Path(j[k]).is_absolute(), j[k]
    assert (fw/j[k]).exists() or k=="lib_dir", (k, j[k])
for i in j["inputs"]:
    assert not pathlib.Path(i).is_absolute(), i
    assert (fw/i).exists(), i
print("aarch64 JSON relative paths OK")
EOF

# 6) 框架 include/ 的 triple 无关性：aarch64 份与 x86_64 份逐字节相同
#    （bindgen 绑定不跨 triple 复用——mquickjs-rs/build.rs 在 target != host
#    时按 TARGET 传 --target=<triple> -ffreestanding 重跑 bindgen，本项只
#    证明入库头本身与 triple 无关）
diff -r "$APP/gen/target/mquickjs-build/framework/aarch64-unknown-none/release/ridl/include" \
        "$APP/gen/target/mquickjs-build/framework/x86_64-unknown-linux-gnu/release/ridl/include" \
    && echo "framework include/ triple-independent OK"

# 7) vendored 适配之外的 crate 源与上游一致（仅白名单差异，见上节）
diff -r "$REPO/deps/mquickjs-sys"        "$APP/rust/mquickjs-sys"   # 预期仅 build.rs 不同
diff -r "$REPO/deps/mquickjs-ridl-glue"  "$APP/rust/mquickjs-ridl-glue"  # 预期无差异
diff -r "$REPO/ports/openvela/rust"      "$APP/rust/adapter"        # 预期仅 build.rs 注释级漂移 + Cargo.toml 接线（白名单 4/5）
diff "$REPO/ridl-modules/stdlib/build.rs" "$APP/rust/stdlib/build.rs"    # 预期整体替换（见"vendored 内容差异"3）
diff "$REPO/deps/mquickjs-rs/mqjs_stdlib_template.c" "$APP/engine/mqjs_stdlib_template.c"  # 预期仅 overlay 2 行（两条 JS_CFUNC_DEF）
```

## 树内构建期依赖（集成机要求）

- **Rust stable 工具链**（rust/rust-toolchain.toml 钉 stable）+ cargo。
- **rustup target**：sim 轨需 `x86_64-unknown-linux-gnu`（宿主自带）；
  qemu arm64 轨（CONFIG_MQJS_RS=y + CONFIG_ARCH=arm64）另需
  `rustup target add aarch64-unknown-none`（tier-2，rustup 预编译 core，
  无 nightly/build-std 依赖）。
- **bindgen → libclang**：mquickjs-rs/build.rs 在树内构建期仍要跑 bindgen
  （生成 FFI 绑定），宿主需 `libclang.so` 可被 bindgen 发现。这是本轮
  spike 定案的**已知集成机要求**（无法仅靠入库产物消除——绑定为
  target 相关，不能跨机预生成入库而不失去 vendored 源的对账性）。
  寻库机制：openvela 的 `build/envsetup.sh` 会把 prebuilts clang 前置进
  PATH 并导出指向 `prebuilts/rust/linux/extra_libs` 的 `LIBCLANG_PATH`
  （部分检出下该目录不存在，且 prebuilts clang 不附带 libclang——两者都
  会令 clang-sys 寻库失败）。app Makefile 因此内置解析规则：外部
  `LIBCLANG_PATH` 仅当目录里确有 `libclang*.so*` 时沿用，否则回退
  `ldconfig` 缓存解析（不硬编码机器路径）；全量检出的 prebuilts rust 包
  存在时自然沿用其 libclang。若宿主既无 prebuilts 包也无 ldconfig 记录，
  手动 `export LIBCLANG_PATH=<含 libclang 的目录>` 即可。
- **crates.io 依赖**：由 cargo 正常解析（Cargo.lock 钉版本；首机需网络或
  已有 registry 缓存）。first-party crate 全部 vendored，无路径外链。
- **binutils 版本敏感性（换机重验）**：适配器归档的 std 系成员是 LTO
  fat object——系统 binutils 的 LLVM 插件若读不懂 rustc 的 IR 版本会报
  "failed to create LTO module" 后**回退内嵌机器码**完成链接（本机实证
  可工作）。换集成机/升级 binutils 后必须重跑哨兵验证链接。
- **Rust 增量陷阱已由 stamp 规则消除**（app Makefile）：rust/ 源变更或
  `make clean` 后，`make context` 会重跑 cargo；无变更时 cargo 为廉价
  no-op。无需手动强制重建。
- **不依赖**：`<repo>` 检出、ridl-tool/ridl-builder/mquickjs-build 二进制、
  openvela 树外任何文件（再生成时才需要）。

## sim 构建与哨兵

```bash
cd ~/workspace/openvela/nuttx && source ../build/envsetup.sh   # PATH 注入
./tools/configure.sh sim:mqjs && make olddefconfig && make -j$(nproc)
# NSH 内（hostfs 挂载宿主语料目录，注意 CONFIG_NSH_LINELEN=80 截断，用短路径；
# sim 与 qemu-armv8a 两 defconfig 的行长上限不同，以各 .config 实测为准）：
mount -t hostfs -o fs=/tmp/mc /m
js /m/demo_pass.js      # CASE demo_pass.js PASS
js /m/rs_probe.js       # rust-bridge ok: ... / CASE rs_probe.js PASS
js /m/ridl_console.js   # ridl console in openvela sim / CASE ridl_console.js PASS
js /m/tiny_err.js       # CASE tiny_err.js FAIL: SyntaxError ...（预期，不崩）
```

验收基线（2026-10-09）：与 setup-sim.sh 流程（ports/openvela，已验收）
镜像四哨兵输出逐行一致。

## C-only fallback（CONFIG_MQJS_RS=n，Phase 3.3）

双形态按 CONFIG 切换（Makefile 选 `MAINSRC`，CMakeLists 占位同步）：

| 形态 | MAINSRC | 上下文创建 | 适配器符号（console 三件 + js_rs_* 探针） |
|------|---------|-----------|------------------------------------------|
| RS=y | js_main.c | Rust 适配器三件套（mqjs_rs_ridl_*） | Rust adapter 归档提供 |
| RS=n | js_main_c.c | C 直连（malloc 堆 + JS_NewContext/JS_Eval） | 本文件内 C 桩提供 |

- **形态决策**：两文件而非同文件宏切换——两种形态在上下文创建、错误上报
  路径、钩子集合上差异大，宏切换会把每个差异点翻倍且互相携带死代码；
  两文件各自可对照上游源审查（js_main.c ↔ 上游 rev，js_main_c.c ↔
  `<repo>` rev `94170f9` 的 M1-C 版），Makefile 按 CONFIG 选 MAINSRC 是
  NuttX 惯用机制。
- **js_main_c.c 来源**：`git show 94170f9:ports/openvela/app/js_main.c`
  （M1-C 形态）适配树内布局，具体差异三处：
  1. 平台钩子（print/gc/Date/load/timers）不再由 main 定义——树内链接的
     是 RIDL 变体表（gen/mqjs_ridl_stdlib.h，经 engine/mqjs_stdlib_impl.c），
     不引用这些钩子；Date 入口由 mqjs_stdlib_impl.c 提供。
  2. 生成表把 rs overlay 与 console 三件烤成了跨 TU 引用（RS=y 时由
     adapter 归档定义）→ RS=n 时由 js_main_c.c 提供最小 C 桩：console
     log/error 直打 stdout（渲染逻辑镜像 M1-C js_print）、get_enabled 返
     真；rsVersion 返回字符串常量、rsSelfTest 做引擎堆上字符串往返自检并
     printf。两形态互斥编译（MAINSRC 二选一），无符号冲突。
  3. `extern const JSSTDLibraryDef js_stdlib;`（定义在 mqjs_stdlib_impl.o，
     无引擎头声明，嵌入方持有声明）。
- **qemu-armv8a 的 configs/mqjs**（nuttx 树新增，未跟踪）：
  `boards/arm64/qemu/qemu-armv8a/configs/mqjs/defconfig` = nsh defconfig
  全量拷贝 + 片段 `CONFIG_MQJS_JS=y`、`# CONFIG_MQJS_RS is not set`、
  `CONFIG_FS_HOSTFS=y`、`CONFIG_BOARDCTL=y`、`CONFIG_ARCH_SETJMP_H=y`。
- **坑（arm64 defconfig 域）**：引擎用 setjmp/longjmp 做语法错误栈展开
  （mquickjs.c JS_Parse2/js_parse_error）；arm64 的实现
  `libs/libc/machine/arm64/arch_setjmp.S` 仅在 `CONFIG_ARCH_SETJMP_H=y`
  时编入 libc，而 qemu-armv8a:nsh 基线未开（无其他引用者）→ 不开则镜像
  链接期 `undefined reference to setjmp/longjmp`（编译期不报，头文件来自
  工具链 newlib）。

## QEMU arm64 C checkpoint 运行记录（3.3 实测）

> 实测日期：2026-10-09；config：`qemu-armv8a:mqjs`（C-only，RS=n）；
> 工具链 aarch64-none-elf-gcc 13.4.0；构建零告警（含全部引擎 TU）。

```bash
cd ~/workspace/openvela/nuttx
source ../build/envsetup.sh
./tools/configure.sh qemu-armv8a:mqjs && make olddefconfig && make -j$(nproc)
ln -sfn ~/workspace/mquickjs-rs-demo/ports/openvela/cases /tmp/hf_cases
# QEMU 命令行同上节 3.2；喂入规则同上节（逐字符 12ms、行间 2s、≤80 字符）
```

会话原文（hostfs 挂语料后）：

```
nsh> mkdir /mnt
nsh> mount -t hostfs -o fs=/tmp/hf_cases /mnt
nsh> js /mnt/demo_pass.js
CASE demo_pass.js PASS
CASES 1/1 PASS
nsh> js /mnt/tiny_err.js
CASE tiny_err.js FAIL: SyntaxError: variable name expected
CASES 0/1 PASS
```

（每条命令间的 `nxposix_spawn_exec: ERROR: exec failed: 2` 为该 debug
defconfig 的良性调试噪声，见 3.2 节；Ctrl-A x 干净退出。）

3.3 验收判定：**demo_pass PASS；tiny_err SyntaxError 文本上报不崩——通过**。
rs_probe/ridl_console 属 Rust 域（3.4），本阶段不做。

## QEMU arm64 Rust 验收（Phase 3.4 实测）

> 实测日期：2026-10-09；config：`qemu-armv8a:mqjs`（**RS=y**）；
> 上游仓（`<repo>`）同期带未 commit 的 feature 卫生改动（见"来源 rev"节的
> 3.4 备注）。

### no_std × RIDL feature 卫生（上游 TDD，未 commit）

`ridl-extensions` 与运行模式解耦（此前隐含 `std`，裸机无法用）：

- **mquickjs-rs**：`ridl-extensions = ["mquickjs-sys/ridl-extensions"]`（去
  `std`）；新增 `glue_prelude` 模块（`Box/CString/Vec/c_int` 从
  `alloc`/`core` 再导出，std 与 no_std 通用）；新增
  `tests/feature_hygiene_no_std_ridl_test.rs`（嵌套 `cargo build`
  `--no-default-features --features no-std,ridl-extensions` 的 mquickjs-rs
  与 stdlib 双锁——Red 阶段两测试实测失败：互斥 compile_error + 22 处
  E0433 + 4 处 eprintln，全为 std 被 ridl-extensions 拉入的连锁）。
- **ridl-tool 模板**：`rust_glue.rs.j2` / `rust_api.rs.j2` /
  `ridl_context_ext.rs.j2` 前导改 `use mquickjs_rs::glue_prelude::*;`
  （生成的胶水不再依赖 std prelude；map 能力的
  `std::collections::HashMap` 路径仍为 std-only，stdlib 未用 map，
  裸机 map 能力留待后续任务）。
- **stdlib（ridl-modules/stdlib）**：`default=["std"]` / `no-std` 互斥
  feature（转发 mquickjs-rs 同名 feature；对 mquickjs-rs 的依赖
  `default-features=false`，否则 feature unification 会把 std 拉回）；
  `stdlib_impl.rs` 输出端按模式分派——std 保持 `print!`/`eprint!`，
  no_std 走 FFI `printf`（`%s` 逐段 + NUL 字面量，UTF-8 校验语义与
  std 路径一致；NuttX flat 链接下 stdout/stderr 同设备，`is_err` 仅保留
  签名对称）；crate-type 收敛为 `["rlib"]`（no_std staticlib 会作为最终
  产物强求 allocator/panic_handler，而叶子应用才是其提供者；历史上无
  staticlib 形态消费方）。
- **adapter（ports/openvela/rust）**：双模式 `#![cfg_attr(
  not(target_os = "linux"), no_std)]`；模式 feature 经 target-specific
  dependencies 注入（**每个 target 表都必须 `default-features = false`**
  ——多表声明按目标合并，漏关即把 default(std) 拉回图）；no_std 分支提供
  裸机三件套（`#[global_allocator]` NuttX malloc/free 桥、
  `#[panic_handler]`（panic=abort 下仍是必需 lang item）、
  `rust_eh_personality` 桩——aarch64 镜像链接实测未引用该桩，链接器按
  未引用回收，保留为兼容形态）；`Once`/`thread_local!` 以
  AtomicBool / 单线程 `UnsafeCell` 静态替代（与 mquickjs-rs `TlsCell`
  同构），业务代码（context trio）两模式共用。
- **上游回归（终态）**：`cargo test --workspace` **618 passed / 0 failed**
  （616 基线 + 2 个 feature 卫生测试）；JS 语料 **31/31 PASS**。

### 树内接线（本目录，3.4 变更）

- **Makefile**：`MQJS_RUST_TARGET` 按 `$(CONFIG_ARCH)` 条件化
  （sim → `x86_64-unknown-linux-gnu`；arm64 → `aarch64-unknown-none` +
  `RUSTFLAGS="-C panic=abort"`——经 RUSTFLAGS 而非 profile，sim 的
  panic 策略不受扰动）；stamp 改名
  `rust/.mqjs-context-stamp-$(MQJS_RUST_TARGET)`（target 感知：同一构建
  目录切换 sim ↔ qemu 配置时强制为新 triple 重跑 cargo + re-stage，
  不会把另一目标的陈旧归档链进镜像；机制仍是 3.1 的普通构建图挂接，
  未回退）。
- **vendored rust/**：同步上游工作区状态（diff 自检 6 的白名单见上节
  接线条目 5 的补充：adapter 去 `[workspace]` 表）。
- **gen/**：聚合按新模板重生成（`ridl_context_ext.rs` 增加 glue_prelude
  导入；与上游重生成聚合逐字节一致，diff 自检 1）；新增 aarch64 框架
  产物目录（`gen/target/mquickjs-build/framework/aarch64-unknown-none/
  release/ridl/`，include/ 与 x86_64 份逐字节相同，见 diff 自检 7）。
- **defconfig**（`boards/arm64/qemu/qemu-armv8a/configs/mqjs/defconfig`，
  untracked）：`CONFIG_MQJS_RS=y`（3.3 为 `n`）。

### aarch64 构建与归档审计（手动命令实录）

```bash
# 构建经 app Makefile stamp 规则自动完成（CONFIG_ARCH=arm64 分支）：
#   cd rust && RUSTFLAGS="-C panic=abort" cargo build --release \
#       --target aarch64-unknown-none -p mqjs
# 归档：rust/target/aarch64-unknown-none/release/libmqjs.a → apps/staging/
NM=../prebuilts/gcc/linux-x86_64/aarch64-none-elf/bin/aarch64-none-elf-nm

# 归档（防重门）：引擎对象成员必须为 0
ar t apps/staging/libmqjs.a | grep -cE '^(mquickjs|cutils|dtoa|libm)\.o$'
# → 0
# 归档导出面（Rust 桥 + console 三件全定义；malloc/free 必须 UND）
$NM apps/staging/libmqjs.a | awk '$2=="T"' | grep -E 'mqjs_rs_|js_global_singleton_console'
# → mqjs_rs_{version,self_test,ridl_context_new,ridl_eval,ridl_context_free}
# → js_global_singleton_console_{log,error,get_enabled}
$NM apps/staging/libmqjs.a | grep -E ' (U|T) (malloc|free)$' | sort -u
# →  U free / U malloc        （两侧均 UND，解析到镜像 NuttX libc）
# 引擎符号留待镜像期（防重门的另一面）
$NM apps/staging/libmqjs.a | grep -E ' U (JS_NewContext|JS_Eval|JS_RIDL_StdlibInit|printf|abort)$'
# → 全部 U
```

镜像链接后（`nuttx`，ELF 64-bit ARM aarch64 静态，8.3MB）：

```bash
$NM nuttx | grep -E ' T (malloc|free)$'
# → 00000000402988c4 T free / 000000004029892c T malloc（各恰一处，NuttX mm_heap）
$NM nuttx | grep -E ' [Tt] (js_global_singleton_console_log|js_global_singleton_console_error|js_global_singleton_console_get_enabled|JS_RIDL_StdlibInit)$'
# → 各恰一处 T
$NM nuttx | grep ' js_stdlib'
# → 0000000040403100 R js_stdlib（const 表，mqjs_stdlib_impl.o，恰一处）
$NM nuttx | grep -E ' (T|t) (printf|puts|setjmp|longjmp)$' | sort
# → printf/puts/setjmp/longjmp 各一处 T（NuttX libc；setjmp 由
#   CONFIG_ARCH_SETJMP_H=y 带入，3.3 坑位复验通过）
$NM -u nuttx
# → 空（镜像零未定义符号）
```

### QEMU 实机哨兵（逐字符喂入规则同 3.2/3.3 节）

```
nsh> js /mnt/rs_probe.js
rust-bridge ok: mquickjs-rs no-std (openvela port, ridl stdlib)
CASE rs_probe.js PASS
CASES 1/1 PASS
nsh> js /mnt/demo_pass.js
CASE demo_pass.js PASS
CASES 1/1 PASS
nsh> js /mnt/tiny_err.js
CASE tiny_err.js FAIL: SyntaxError: variable name expected
CASES 0/1 PASS
```

- **3.4 验收判定：通过**。版本串 `mquickjs-rs no-std (...)` 证实 aarch64
  镜像内运行的是 no_std 模式 adapter；且 `rust-bridge ok` 这一行本身就是
  Rust console singleton 经 FFI printf 桥打到 NSH 控制台的——no_std
  console 链路（Rust stdlib → glue → ctx 槽分派 → printf）全程实证。
- 加跑（3.5 前瞻，非本阶段验收项）：

```
nsh> js /mnt/ridl_console.js
ridl console in openvela sim
CASE ridl_console.js PASS
CASES 1/1 PASS
```

- 每条命令间的 `nxposix_spawn_exec: ERROR: exec failed: 2` 为该 debug
  defconfig 的既有良性噪声（3.2/3.3 节已记录）。
- 双轨互斥处理：单构建目录顺序重配（distclean → `sim:mqjs` → 四哨兵 →
  distclean → `qemu-armv8a:mqjs`）；树最终停留在 qemu-armv8a:mqjs RS=y
  状态。target 感知 stamp（见上）保证切换不串档。

## QEMU arm64 运行说明（3.2 spike 实测）

> 实测日期：2026-10-09；宿主：Linux x86_64；树状态：qemu-armv8a:nsh 基线
> （不含 mqjs config——3.3 才落 configs/mqjs）。本节为 3.2 spike 实测记录。

### 构建（逐字，全部通过）

```bash
cd ~/workspace/openvela/nuttx
source ../build/envsetup.sh                      # 注入 prebuilts PATH
./tools/configure.sh qemu-armv8a:nsh
make olddefconfig
make -j$(nproc)
```

- 工具链：`prebuilts/gcc/linux-x86_64/aarch64-none-elf/bin/aarch64-none-elf-gcc`
  （GCC 13.4.0），由 envsetup.sh 的 `prebuilts/gcc/${SYSTEM}-${SYS_ARCH}/<arch>-none-elf/bin`
  规则命中，无需手动设置 CROSSDEV（`CROSSDEV ?= aarch64-none-elf-` 为默认值）。
- 产物：`nuttx/nuttx`（ELF 64-bit LSB executable, ARM aarch64，~6.4MB，含
  debug_info）。关键 config 已核验：`CONFIG_ARCH_FPU=y`、`CONFIG_ARM64_NEON=y`、
  `CONFIG_ARM64_SEMIHOSTING_HOSTFS=y`、`CONFIG_FS_HOSTFS=y`、`CONFIG_NSH_LINELEN=80`。

### QEMU 启动命令行（逐字，实测可用）

```bash
cd ~/workspace/openvela/nuttx   # hostfs 相对路径基准 = 此 cwd
/home/peng/workspace/openvela/prebuilts/qemu/linux-x86_64/bin/qemu-system-aarch64 \
  -cpu cortex-a53 -nographic \
  -machine virt,virtualization=on,gic-version=3 \
  -net none -semihosting -m 128M \
  -kernel ./nuttx
```

- QEMU 为 prebuilts 版本 10.0.0；`-semihosting` 必需（hostfs 依赖）；`-m 128M`
  与 defconfig `RAM_SIZE=0x8000000` 对齐（virt 默认同为 128M，显式写出仅求确定）。
- 启动横幅原文：`- Ready to Boot Primary CPU / - Boot from EL2 / - Boot from EL1 /
  - Boot to C runtime for OS Initialize`，随后 `NuttShell (NSH)` + `nsh>` 提示符。
- 退出：**Ctrl-A x**（`QEMU: Terminated` 干净退出，实测验证；板 README 所写
  "Ctrl + X" 对应其 chardev mux + mon readline 形态，本命令行形态下无效）。
- **脚本化喂命令的坑（重要）**：
  - `-nographic` 下 QEMU stdio→PL011 无流控，guest 忙时整行/整段输入会丢
    （首字符被吃、行尾被截），即使启动后延迟数秒仍随机丢。
  - 可靠做法：**逐字符喂入 + 每字符 ~12ms 间隔 + 行间 1.5-2s**
    （`for` 循环 `printf '%s'` + `sleep 0.012`，行尾发 `\r`）。
  - **NSH 行长上限 `CONFIG_NSH_LINELEN=80`**：超过 80 字符的命令会被截断/
    错位（实测 `mount -t hostfs -o fs=<长绝对路径> /mnt` 85 字符必失败）。
    长路径一律先在宿主侧做短 symlink（如 `ln -sfn <宿主目录> /tmp/hf_cases`）。

### semihosting hostfs 实测

mount 语法与 sim 完全一致（同一 `fs/hostfs/hostfs.c` 通用层，`fs=` 选项解析相同；
差异仅在后端：sim=直连宿主 libc，arm64=semihosting 调用）：

```
nsh> mkdir /mnt
nsh> mount -t hostfs -o fs=/tmp/hf_cases /mnt      # /tmp/hf_cases → 宿主语料目录 symlink
nsh> mount                                          # 挂载确认原文：
  /mnt type hostfs
  /proc type procfs
  /tmp type tmpfs
```

- **文件读正常**：`cat /mnt/tiny_err.js` → `var ;`（全文）；
  `cat /mnt/demo_pass.js`、`cat /mnt/demo_syntax_error.js` 全文逐行完整返回；
  `cat /mnt/no_such_marker_xyz` → `nsh: cat: open failed: 2`（ENOENT，干净报错不崩）。
- **目录列举不可用（已知限制）**：`ls /mnt` 只回显挂载点自身
  （`ls -l /mnt` → ` -rwxrwxrwx 4096 /mnt`）。根因：
  `arch/arm64/src/common/arm64_hostfs.c:263-280` 的 `host_opendir/host_readdir/
  host_closedir` 为未实现桩（返回 NULL/-ENOSYS，源内注释 "semihosting doesn't
  support directory yet"）；open/read/stat 走 semihosting SYS_OPEN/READ/FSTAT 已实现。
  → **arm64 侧按已知文件名直接 cat/执行，不要依赖 ls**（3.3/3.4 哨兵脚本注意）。
- **路径语义实测**：`fs=` 相对路径**相对 QEMU 进程 cwd**（cwd=/tmp 时
  `mount -t hostfs -o fs=. /mnt` 后 `cat /mnt/feed_rel.sh` 读到宿主 /tmp 下的
  脚本原文）；绝对路径原样使用（symlink 透明解析）。NSH 侧 /mnt 需先
  `mkdir`（根伪文件系统支持 mkdir 造挂载点）。
- 无害噪声：每条命令执行时打印一行
  `nxposix_spawn_exec: ERROR: exec failed: 2`
  （`sched/task/task_posixspawn.c:113`，CONFIG_DEBUG_FEATURES=y 下的 serr 输出；
  命令本身均执行成功，属该 debug defconfig 的良性调试噪声）。

### FP/ABI 实测与结论

**结论：qemu-armv8a nsh 基线下，aarch64 标量浮点硬浮点 ABI（AAPCS64，v 寄存器
传参）全链路可用。3.4 Rust target 直接用 stable 的 `aarch64-unknown-none`
（默认即硬浮点），无需自定义 target json、无需 feature 调整。**

证据链：

1. 编译器默认：`Toolchain.defs` 对 cortex-a53 仅加 `-mcpu=cortex-a53`，
   无 `+nofp` / `-mgeneral-regs-only`；`aarch64-none-elf-gcc 13.4.0` 默认硬浮点。
   编译同一 test 函数 `double fp_add3(double,double,double)` 得
   `fmadd d0, d1, d2, d0`——参数落在 d0/d1/d2（v 寄存器），AAPCS64 硬浮点传参。
2. 内核使能：`CONFIG_ARCH_FPU=y`；boot 代码 `arch/arm64/src/common/arm64_boot.c:195`
   置 `CPACR_EL1.FPEN=NOTRAP`（NEON/FP 上电即可用）；FPU 上下文为懒式 trap
   切换（`arm64_fpu.c`：首次 FPU 访问触发 trap 保存/恢复）。
3. 运行时实证：NSH 内跑 `ostest`（defconfig 自带，含 FPU 交叉污染测试）——
   两个线程各 16 pass 单/双精度浮点运算并跨上下文切换校验 v 寄存器不被污染，
   原文 `FPU#1: Succeeded` / `FPU#2: Succeeded`，无任何 `ERROR FPU` 行；
   ostest 全程 0 ERROR、`user_main: Exiting` 正常收尾回 nsh>。
4. Rust 对齐：本机 stable `rustc 1.94.0`，裸 target `aarch64-unknown-none`
   （无 -C target-feature、无 json）编译 `fn(a: f64,b: f64,c: f64)->f64` 得
   `fmul d1,d1,d2; fadd d0,d0,d1; ret`——与 C 侧 ABI 完全一致
   （`--print cfg` 亦显示 `target_feature="neon"`）。

### 3.4 衔接备忘（已兑现，见"QEMU arm64 Rust 验收（Phase 3.4 实测）"节）

- Rust 交叉构建：`aarch64-unknown-none` + stable，硬浮点 ABI 直用；
  softfloat 变体**不要**用（与引擎 ABI 不匹配，链接期静默破坏）。
- hostfs 语料投递：延续 sim 语义（mount fs=），但脚本需逐字符低速喂入、
  命令 ≤80 字符、不依赖 ls。
