# hello_world

最小控制台程序，输出 Hello world。

这是独立的 xmake 项目，引用仓库共享核心，不依赖预先构建根项目或其他示例。

在本目录使用 MinGW-w64 GCC：

```powershell
xmake f -P . -p mingw -m debug
xmake -P .
xmake run -P . hello_world
```

Linux/macOS 可以配置 `xmake f -P . --toolchain=gcc -m debug`，Clang 使用 `--toolchain=clang`。

构建产物位于本项目的 build/；程序入口在 `objects/application/main/main.c`，当前 `main` 直接打印 `Hello world!`。根目录的 `hello_world.c` 和 `host.c` 保留对象接口与宿主模板，`<crt>` 区域和宿主创建选择尚未填写，当前程序没有创建或访问对象。学习完整生命周期请继续阅读单继承示例。

[构建指南](../../docs/build.md) · [架构](../../docs/architecture.md) · [教程](../../docs/tutorial.md)
