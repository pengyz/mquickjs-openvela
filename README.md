# mquickjs-openvela — mquickjs JS engine SDK 的 OpenVela/NuttX 树内集成

将 [mquickjs-rs-sdk](https://github.com/pengyz/mquickjs-rs-sdk)（mquickjs 引擎
+ Rust 集成 + RIDL 工具链）以 **NSH builtin（`js` 命令）** 形态接入 OpenVela。
本仓由 repo manifest 条目管理（嵌套于 apps 树，参照 apps/** 项目惯用法）：

```xml
<project path="apps/system/mqjs" name="mquickjs-openvela" remote="pengyz" revision="main"/>
```

- 配套 defconfig：pengyz/nuttx 仓 `mqjs-in-tree` 分支
  （`boards/sim/sim/sim/configs/mqjs` 与 `boards/arm64/qemu/qemu-armv8a/configs/mqjs`）
- **构建/哨兵/vendored 溯源/再生成自检**：全部见 [SYNC.md](SYNC.md)
- 双轨已验收：sim（x86_64 宿主，std adapter）与 QEMU aarch64（-M virt，
  no_std adapter）四哨兵全绿（demo_pass / rs_probe / ridl_console / tiny_err）
