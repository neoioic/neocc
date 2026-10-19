# NeoCC

使用 C11 与 NEOioic 逐步重构 TinyCC，保留其编译算法、直接代码生成、目标 ABI
和输出格式。源文件缓冲栈、文件句柄和真实文件名由 `source` 对象拥有；标识符
驻留、索引表和宏流存储由 `tokens` 对象拥有。`preprocessor` 对象拥有宏展开栈、
当前 token/值、借用源游标、工作字符串、词法模式、字符分类表及计数器。
语义符号池及回收链由 `symbols` 对象拥有，通过 VO 分配和归还普通符号记录。
作用域、符号栈和其余编译状态仍在迁移中，尚未完成全套对象化重写。

当前验证 Windows x86-64 / MinGW-w64，生成 x86-64 PE 程序及 ELF 可重定位目标。
根 xmake 构建不访问外部项目、不联网，也不隐式再生源码。

```sh
xmake f -P . -p mingw -a x86_64 --toolchain=gcc -m debug -y
xmake -P . -y
xmake test -P . -v
```

完整检查：`python tests/verify.py --mode debug`，Release 改为 `--mode release`。
详见 [构建与使用](docs/build.md)、[迁移设计](docs/architecture.md)、
[验证边界](docs/testing.md) 和 [交接记录](docs/handoff.md)。
Agent 按阶段完成迁移、测试通过后停下，由开发者提交备份；接手可使用
[下一位 Agent 提示词](docs/next-agent-prompt.md)，具体规则见 `AGENTS.md`。

`tinycc/` 保持可选独立行为基线；NeoCC 的活跃编译实现位于 `src/`。
`python tests/independent.py --mode debug` 在不含 `tinycc/` 的干净源码副本中验证
构建、运行和第二／第三阶段自举；Release 改为 `--mode release`。
TinyCC 衍生实现沿用其适用许可，见 [第三方声明](THIRD_PARTY_NOTICES.md)。
NeoCC 采用 LGPL-2.1-or-later，Copyright (C) 2026 CLimber-Rong，见 [LICENSE](LICENSE)。
第三方材料保留各自的许可、版权及适用例外。
