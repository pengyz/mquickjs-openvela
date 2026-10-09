# mquickjs-openvela — mquickjs JS engine SDK 的 OpenVela/NuttX 适配层

将 [mquickjs-rs-sdk](https://github.com/pengyz/mquickjs-rs-sdk)（mquickjs 引擎
+ Rust 集成 + RIDL 工具链）以 **NSH builtin（`js` 命令）** 形态接入 OpenVela。

**两仓架构**（2026-10-09）：本仓是纯适配层（Makefile + CMakeLists + Kconfig +
js_main 双形态 + gen/ 集成产物）；引擎与 Rust 闭包**不 vendored**，由 repo
manifest 把 SDK 检出到 `<tree>/external/mquickjs-rs-sdk`，本仓经路径依赖消费：

```xml
<project path="external/mquickjs-rs-sdk" name="mquickjs-rs-sdk" remote="pengyz" revision="master"/>
<project path="apps/system/mqjs" name="mquickjs-openvela" remote="pengyz" revision="main"/>
```

- 配套 defconfig：pengyz/nuttx 仓 `mqjs-in-tree` 分支（sim + qemu-armv8a）
- **构建/哨兵/再生成/集成机要求**：全部见 [SYNC.md](SYNC.md)
- 双轨已验收：sim（x86_64 宿主，std adapter）与 QEMU aarch64（-M virt，
  no_std adapter）四哨兵全绿（demo_pass / rs_probe / ridl_console / tiny_err）
