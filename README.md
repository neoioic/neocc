# NeoCC

使用 C11 与 NEOioic 逐步重构 TinyCC，保留其编译算法、直接代码生成、目标 ABI
和输出格式。源文件缓冲栈、文件句柄和真实文件名由 `source` 对象拥有；标识符
驻留、索引表、宏流存储和延迟内联函数记录/函数体由 `tokens` 对象拥有。`preprocessor` 对象拥有宏展开栈、
当前 token/值、借用源游标、工作字符串、词法模式、字符分类表及计数器。
语义符号池、回收链、global/local/宏定义/标签栈头及当前作用域层级由 `symbols`
对象拥有，符号记录的分配、移动、出栈和归还通过 VO 完成。`expressions` 对象拥有
表达式值栈、栈顶和四种常用类型，算法经 VO 借用工作区。`scopes` 对象拥有作用域帧、
当前/根/循环入口和清理/goto 链头，经 VO 管理帧生命周期。`functions` 对象拥有当前函数
上下文、局部栈偏移和临时栈槽缓存，经 VO 初始化、复用临时槽及重置。
`switches` 对象拥有嵌套 switch 上下文、case 记录与索引数组，经 VO 管理资源生命周期。
`initializers` 对象拥有初始化器和属性/汇编字符串解析共享的拼接缓冲及复合字面量存储模式。
`temporaries` 对象拥有临时字符串和编译边界，诊断跳转前经 VO 释放本次调用的资源。
`translation` 对象拥有翻译单元编号、指令游标、代码抑制与调试模式，仅借用链接代码节；正常和异常收尾均清零。
其余语法与后端状态仍在迁移中，尚未完成全套对象化重写。

当前验证 Windows x86-64 / MinGW-w64，生成 x86-64 PE 程序及 ELF 可重定位目标。
根 xmake 构建不访问外部项目、不联网，也不隐式再生源码。

```sh
xmake f -P . -p mingw -a x86_64 --toolchain=gcc -m debug -y
xmake -P . -y
xmake test -P . -v
```

完整检查：`python tests/verify.py --mode debug`，Release 改为 `--mode release`。
详见 [构建与使用](docs/build.md)、[迁移设计](docs/architecture.md)、
[验证边界](docs/testing.md)、[交接记录](docs/handoff.md) 和
[后续阶段计划](docs/plan.md)。
Agent 按阶段完成迁移、测试通过后停下，由开发者提交备份；具体规则见
`AGENTS.md`，进度见 `docs/handoff.md`。

`tinycc/` 保持可选独立行为基线；NeoCC 的活跃编译实现位于 `src/`。
`python tests/independent.py --mode debug` 在不含 `tinycc/` 的干净源码副本中验证
构建、运行和第二／第三阶段自举；Release 改为 `--mode release`。
TinyCC 衍生实现沿用其适用许可，见 [第三方声明](THIRD_PARTY_NOTICES.md)。
NeoCC 采用 LGPL-2.1-or-later，Copyright (C) 2026 CLimber-Rong，见 [LICENSE](LICENSE)。
第三方材料保留各自的许可、版权及适用例外。
