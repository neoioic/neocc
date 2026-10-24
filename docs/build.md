# 构建与使用

当前已验证 Windows x86-64、PATH 中完整 MinGW-w64 GCC 15.1.0、
xmake 2.9.6 和 Python 3。C11 为源码构建标准。Linux、其他目标、线程并行编译、
边界检查运行库和回溯运行库尚未验证/启用。配置阶段拒绝当前未实现的构建组合；
后续按目标后端及其 ABI 测试扩展配置，不把主机字长当成目标 ABI。

从仓库根目录运行：

```sh
xmake f -P . -p mingw -a x86_64 --toolchain=gcc -m debug -y
xmake -P . -y
xmake test -P . -v
```

根入口构建 `neocc`、`neocc_compiler`、唯一来源的 `neoioic` 静态库、
`neocc_runtime` 及回归程序。运行库的 C 和汇编使用刚构建的 NeoCC 编译，
产物和头文件部署在 `build/mingw/x86_64/debug/runtime/`。
`openoicc` 是显式维护目标；存在 `tinycc/` 时另提供差分目标 `tinycc_baseline`。
NeoCC 的构建、运行库和阶段自举不需要该参考树：

```sh
xmake -P . -y tinycc_baseline
xmake -P . -y openoicc
build/mingw/x86_64/debug/neocc.exe --version
build/mingw/x86_64/debug/neocc.exe -Bbuild/mingw/x86_64/debug/runtime -std=c11 -run tests/cases/semantics.c
build/mingw/x86_64/debug/neocc.exe -Bbuild/mingw/x86_64/debug/runtime -std=c11 tests/cases/semantics.c -o build/semantics.exe
build/semantics.exe
```

`-B` 指向所选阶段的运行库根目录，包含 `include/` 和 `lib/`。
`-run` 后第一个源文件之后的参数属于被执行程序；编译多个输入时，其他输入应放在
`-run` 前。CLI 保留 TinyCC 选项及其语义；当前内部诊断仍可能含 `tcc:` 前缀。

Release 配置将 `-m debug` 改为 `-m release`；产物位于独立的 `release/`。
不在 `tinycc/` 或依赖原始源码目录写入配置头、目标文件或运行库。

## 显式构造再生

常规构建直接编译纳入版本管理的构造区。维护时先预览，再显式写入：

```sh
xmake -P . -y openoicc
build/mingw/x86_64/debug/openoicc.exe check src/objects/source.c src/objects/tokens.c src/objects/preprocessor.c src/objects/symbols.c src/objects/expressions.c src/objects/scopes.c src/objects/functions.c src/objects/switches.c src/objects/initializers.c src/objects/temporaries.c src/objects/translation.c
build/mingw/x86_64/debug/openoicc.exe generate src/objects/source.c src/objects/tokens.c src/objects/preprocessor.c src/objects/symbols.c src/objects/expressions.c src/objects/scopes.c src/objects/functions.c src/objects/switches.c src/objects/initializers.c src/objects/temporaries.c src/objects/translation.c
build/mingw/x86_64/debug/openoicc.exe generate --write src/objects/source.c src/objects/tokens.c src/objects/preprocessor.c src/objects/symbols.c src/objects/expressions.c src/objects/scopes.c src/objects/functions.c src/objects/switches.c src/objects/initializers.c src/objects/temporaries.c src/objects/translation.c
xmake -P . -y
xmake test -P . -v
build/mingw/x86_64/debug/openoicc.exe check src/objects/source.c src/objects/tokens.c src/objects/preprocessor.c src/objects/symbols.c src/objects/expressions.c src/objects/scopes.c src/objects/functions.c src/objects/switches.c src/objects/initializers.c src/objects/temporaries.c src/objects/translation.c
```

最后一次应无变化。生成器、辅助头和运行时均来自本仓库固定快照；未使用旧 oicc。

## 完整验证与阶段构建

```sh
python tests/verify.py --mode debug
python tests/verify.py --mode release
```

该入口依次构建、执行二十六组根回归、纯 C 差分、自举、不含参考树的干净构建/运行/自举、
GCC 和第三阶段 NeoCC 构建的
OpenOICC 各 39 项集成测试。测试工具链通过 PATH 发现。生成器测试使用固定共享
临时目录，因此两次完整验证应串行运行。
全部通过才写入对应构建目录的 `verification.json`。

单独执行：

```sh
python tests/regression.py --build-dir build/mingw/x86_64/debug
python tests/bootstrap.py --build-dir build/mingw/x86_64/debug
python tests/independent.py --mode debug
```

阶段目录位于对应构建目录的 `bootstrap/stage2/`、`bootstrap/stage3/`；
自举脚本记录 SHA-256 和逐字节比较结果，不比较带 GCC 调试信息的第一阶段程序。
各阶段再生十一个对象构造区，编译运行对象/延迟内联资源回归及多文件内联语义样例。
独立性脚本在 `build/independent/` 新建不含 `tinycc/` 和缓存的源码副本，从公开工具链
重新构建并运行根回归及自举，结果保存在 `build/independent/debug.json` 或 `release.json`。
完整差分验证入口仍使用参考树语料；其需求不属于 NeoCC 生产构建依赖。
详见 [验证范围](testing.md)。
