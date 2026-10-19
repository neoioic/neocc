# NeoCC 参考材料

按实际用途摘选，不是上游仓库镜像。后续开发仅访问 NeoCC 内材料；缺项由开发者提供到本仓库。

| 目录 | 用途与范围 |
| --- | --- |
| `neoioic/` | 唯一运行时、xmake 入口、架构/教程/风格文档、最小应用和菱形继承示例、运行时健壮性和分派回归 |
| `openoicc/` | 生成器源码、构造辅助头、生成契约和用户指南、相关生成与再生集成测试，共用上述运行时 |

未导入 WGI、历史教材、旧二进制、Git 元数据或缓存。单继承用法已包含在菱形示例中；OpenOICC 运行时副本与旧继承参考示例不重复导入。测试按用途选择，保留必需的运行依赖，见两个快照内的 `tests/README.md`；TinyCC 自带的编译器测试仍位于 `tinycc/tests/`。

各 `NEOCC_SOURCE.json` 记录固定提交、原文件 SHA-256、导入后 SHA-256 及本地调整。上游摘选文档可能引用未导入内容，不构成回到外部工作区读取材料的授权。

生成器独立构建入口如下；Windows 配置按工具链增加 `-p mingw`：

```sh
xmake f -P third_party/openoicc -m debug
xmake -P third_party/openoicc openoicc
xmake run -P third_party/openoicc openoicc --help
```

生成器通过 `../neoioic/src` 引用同属 NeoCC 的运行时，移除了未导入的参考目标。
NeoCC 现有根构建入口，可用 `xmake -P . openoicc` 构建生成器，
`python tests/verify.py --mode debug`（或 `release`）执行完整验证。
Windows 目录枚举和排他文件创建已为 NeoCC 的目标 CRT 调整，来源记录同步更新；
NEOioic 核心保持固定版本。实际平台、结果及边界见 `../docs/testing.md`。
