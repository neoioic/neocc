# NEOioic 相关回归

从固定版本摘选 `robustness.c`、`dispatch.c`、`check.h` 和独立 `xmake.lua`，覆盖队列容量与随机操作、未对齐字节流、引用和模式位、部分关闭、访问登记、共享祖先、动态/外部存储、分配失败和 OID 边界。所需最小应用与菱形继承实现已在同一快照内保留。

需要 C11、xmake 及支持 GNU ld `--wrap` 的 GCC/MinGW 工具链；不能据此声称其他链接器已支持。检查使用 `CHECK`，不随 Release 的 `NDEBUG` 消失。

从 NeoCC 根目录运行，Windows 配置按需增加 `-p mingw`：

```sh
xmake f -P third_party/neoioic/tests --toolchain=gcc -m debug
xmake -P third_party/neoioic/tests
xmake test -P third_party/neoioic/tests -v
```

Release 使用独立配置 `-m release` 后重新构建、测试。两组回归现已在 NeoCC 工作区的
Windows x86-64 / MinGW-w64 GCC 15.1.0 Debug/Release 执行通过；根构建也包含同样
测试。NeoCC 编译分派和故障注入回归的方式及边界见 `../../../docs/testing.md`。
