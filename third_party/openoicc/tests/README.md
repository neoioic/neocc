# OpenOICC 相关回归

摘选固定版本的生成器集成测试及其必需 C 辅助程序，保留 39 项测试方法：标记/编码/声明边界、继承图与随机图、布局和模型编号、骨架实际编译运行、宿主生命周期、文件写入、故障注入、区外保留、幂等再生与再生后重编译。

`construct.c`、`alloc.c` / `alloc.h` 检查布局和分配；`host.c`、`scaffold.c` 检查宿主及新建节点；`files.c` 检查文件失败路径；`tool_alloc.c` / `tool_alloc.h` 为测试编译注入失败。它们只用于测试，不引入生产故障开关。

移除了依赖旧继承参考项目的 `test_directory_reference_behavior`、`test_generated_reference_behavior_and_failures` 两项，未导入其专属 `runtime.c`、`delegation.c` 或重复旧示例。NEOioic 的共享祖先和分派样例另有保留。本地测试运行时路径改为 `../neoioic`，不复制第二份运行时。

需要 Python 标准库、PATH 中的 C11 GCC/Clang 及已从源码构建的 OpenOICC。从 NeoCC 根目录执行（可执行文件参数相对于 `third_party/openoicc/`）：

```sh
python third_party/openoicc/tests/test_openoicc.py --openoicc build/<platform>/<architecture>/debug/openoicc --cc gcc
```

替换平台、架构占位；Windows 可执行文件加 `.exe`。Release 测试选择对应程序并增加
`--release`。测试临时文件仅位于该快照的 `build/tests/`，多次运行应串行。
39 项现已在本仓库运行，GCC 及 NeoCC 第三阶段构建的工具分别验证；C 辅助程序由 GCC
构建。完整复现入口和平台边界见 `../../../docs/build.md`、`../../../docs/testing.md`。
