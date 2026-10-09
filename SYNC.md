# SYNC.md — mquickjs-openvela 集成清单

> 2026-10-09 两仓重构后本仓是**纯适配层**：引擎/Rust 闭包不再 vendored，
> 一切来自 SDK 检出（repo manifest 项目 `external/mquickjs-rs-sdk`）。
> 历史 vendored 布局的溯源记录见 git 历史（本文件 v1）。

## 目录布局

```
apps/system/mqjs/            ← 本仓（pengyz/mquickjs-openvela，main）
├── Kconfig / Make.defs / Makefile / CMakeLists.txt   # 双轨接线
├── js_main.c                # RS=y 入口（Rust 三件套 + 哨兵协议）
├── js_main_c.c              # RS=n 入口（M1-C C 桩形态）
├── gen/                     # 集成期生成、入库的产物（见下表）
└── rust/                    # 适配层 crate（RIDL 叶子，app-id=mqjs）
    ├── Cargo.toml           # 路径依赖指向 ../../../../external/mquickjs-rs-sdk
    ├── .cargo/config.toml   # MQJS_ENGINE_LINK=external + gen 产物 env 接线
    └── src/ build.rs

external/mquickjs-rs-sdk/    ← SDK（pengyz/mquickjs-rs-sdk，master）
├── deps/mquickjs/           # 引擎 C（git 子模块，检出后需
│                            #   git submodule update --init）
├── deps/mquickjs-rs/        # Rust 包装 + mqjs_stdlib_impl.c/require.c
├── deps/mquickjs-sys/ deps/mquickjs-ridl-glue/
├── ridl-modules/stdlib/     # stdlib RIDL 模块（含 generated/ 预生成胶水）
└── ridl-tool/ ridl-builder/ mquickjs-build/   # 再生成时才需要的宿主工具源
```

## gen/ 产物与消费方

| 产物 | 生成者 | 消费方 |
|------|--------|--------|
| gen/mquickjs_atom.h、gen/mqjs_ridl_stdlib.h | `mqjs_stdlib` host 工具（**overlay 后模板** + mquickjs_build.c，`-DMQUICKJS_ENABLE_RIDL_EXTENSIONS`） | 镜像 C 轨（app `-Igen`；强 `js_stdlib` 表含 console + rs 探针） |
| gen/mqjs_rs_hooks.h | 静态内容（声明 js_rs_* 探针钩子） | 每个 app TU 强制包含 |
| gen/mquickjs_ridl_{api.h,register.h,register.c,module_class_ids.h}、gen/ridl-manifest.json | `ridl-builder aggregate`（app=mqjs） | C 轨寄存器/强制包含 + 宿主工具输入；manifest 溯源 |
| gen/ridl/apps/mqjs/aggregate/{ridl_symbols,ridl_context_ext,ridl_bootstrap}.rs | 同上 | adapter build.rs（`MQUICKJS_RIDL_TARGET_DIR=../gen`） |
| gen/mquickjs.build.toml | 手写（自 SDK 根同名文件改写） | mquickjs-sys/build.rs（`MQUICKJS_BUILD_TOML=../gen/...`；json 内相对路径由 SDK 1b4edd2 起 resolve） |
| gen/target/mquickjs-build/framework/{x86_64-unknown-linux-gnu,aarch64-unknown-none}/release/ridl/{include/,mquickjs_build_output.json} | `mquickjs-build build`（按 triple） | include/：bindgen + class-id 生成；json：sys 定位产物 |

**不入库**：框架产物的 `lib/` 与 base 变体——`MQJS_ENGINE_LINK=external`
（rust/.cargo/config.toml）下适配器归档不得 bundle 引擎对象，镜像的
`js_stdlib` 由 C 轨 CSRCS 提供。

## 模板 overlay（rs 探针注入）

镜像 stdlib 表包含 `rsVersion`/`rsSelfTest` 两个探针，来自对 SDK 的
`deps/mquickjs-rs/mqjs_stdlib_template.c` 的 **2 行 sed 注入**（锚点
`JS_PROP_NULL_DEF("globalThis", 0 ),` 后插两条 JS_CFUNC_DEF）。overlay 只
发生在 gen/ 头再生成时（SDK 源保持 pristine）。

## 再生成命令（需 SDK 检出 + 其宿主工具源码构建）

```bash
APP=~/workspace/openvela/apps/system/mqjs
SDK=~/workspace/openvela/external/mquickjs-rs-sdk
cd "$SDK" && git submodule update --init deps/mquickjs
cargo build -q -p ridl-tool -p ridl-builder -p mquickjs-build

# ① 聚合（app=mqjs；产物先落 rust/target/...，再复制入库 gen/ 与 gen/ridl/）
cargo run -q -p ridl-builder -- aggregate --cargo-toml "$APP/rust/Cargo.toml" --intent build
# ② 模块胶水（stdlib.ridl 变更时；日常由 SDK 内 generated/ 覆盖，见其 build.rs 双模式）
# ③ host 工具生成 atom/stdlib 头（模板 overlay 后）：
#    overlay：sed 在 JS_PROP_NULL_DEF("globalThis", 0 ), 后插
#      JS_CFUNC_DEF("rsVersion", 0, js_rs_version),
#      JS_CFUNC_DEF("rsSelfTest", 0, js_rs_self_test),
#    然后 cc -DMQUICKJS_ENABLE_RIDL_EXTENSIONS -I<engine> -I"$APP/gen/ridl/apps/mqjs/aggregate"
#    编译 overlay 后模板 + mquickjs_build.c，运行生成两头（v1 SYNC.md 有完整命令）
# ④ 框架产物（按 triple）：cargo run -q -p mquickjs-build -- build
#    --mquickjs-dir "$SDK/deps/mquickjs" --ridl-register-h "$APP/gen/mquickjs_ridl_register.h"
#    --out "$APP/gen/target/mquickjs-build/framework/<triple>/release/ridl"
```

自检：聚合产物、模块胶水对同一输入**字节确定**；json 相对路径由 SDK
build.rs resolve（cargo:warning 提示无法 canonicalize 的条目）。

## 树内构建期依赖（集成机要求）

- Rust stable + `rustup target add aarch64-unknown-none`（QEMU 轨）
- bindgen → libclang（SDK 的 mquickjs-rs build-dep；寻库规则：外部
  LIBCLANG_PATH 仅当确含 libclang 才沿用，否则 ldconfig 回退——app
  Makefile 内置，见 Makefile 注释）
- crates.io 依赖（首机需网络或 registry 缓存；first-party crate 走 SDK 路径）
- **不依赖**：ridl-tool/ridl-builder/mquickjs-build 预构建二进制（再生成
  时才需要其源码构建）、本树外任何文件

## 构建与哨兵（Make 轨）

```bash
cd ~/workspace/openvela/nuttx && source ../build/envsetup.sh   # PATH 注入
./tools/configure.sh sim:mqjs          # 或 qemu-armv8a:mqjs
make olddefconfig && make -j$(nproc)
```

- 四哨兵：`ports/openvela/cases/`（在 mquickjs-rs-sdk 仓内；hostfs 挂载后
  `js <路径>` 运行）。注意 NSH_LINELEN=80 截断、stdio→PL011 无流控（逐字
  符 ~12ms 喂入）、先 mkdir /mnt
- QEMU arm64 命令行（3.2 实测锁定）：

```bash
/home/peng/workspace/openvela/prebuilts/qemu/linux-x86_64/bin/qemu-system-aarch64 \
  -cpu cortex-a53 -nographic -machine virt,virtualization=on,gic-version=3 \
  -net none -semihosting -m 128M -kernel ./nuttx
# 退出 Ctrl-A x；hostfs：mount -t hostfs -o fs=<宿主目录> /mnt（无 ls，直接 cat 已知路径）
```

- 双轨互斥：Make ↔ CMake 切换需 distclean
- binutils 版本敏感性：适配器归档含 LTO fat object，换机/升级 binutils
  后重跑哨兵（LLVM 插件不配时回退内嵌机器码）
- 良性噪声：debug defconfig 下每条 NSH 命令打印
  `nxposix_spawn_exec: ERROR: exec failed: 2`（命令本身成功）

## CMake 轨（两仓重构后真实现）

- `CMakeLists.txt`：C 侧从 SDK 路径编译 + RS 双形态 + adapter 归档经
  `nuttx_add_extra_library` 链接、`DEPENDS mqjs_rust_adapter` 保证 cargo
  先于链接
- 验收口径：sim CMake 构建 + `js` 在镜像 + 哨兵，Rust 归档由 CMake
  custom target 自产
- 已知差异：CMake 轨的全局 -Werror/多 pass 机制与 Make 轨差异见
  docs/knowledge/gotcha_vela_cmake_track.md

## 历史记录

- Phase 3.1-3.4 实机记录（vendored 布局时期：vendored 白名单、diff 自检、
  setjmp Kconfig 坑、semihosting 限制、FP 结论）：见 git 历史本文件 v1
