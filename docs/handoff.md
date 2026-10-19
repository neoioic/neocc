# NeoCC 迁移交接

截至 2026-10-19，最近完成语义符号池对象迁移及项目许可设置，Debug/Release 全量验证通过。
后续迁移使用仓库内材料，不访问外部项目；Agent 不执行 Git 暂存、提交或推送，
TinyCC 基线保持不变。详细设计、命令和验证边界见
[架构](architecture.md)、[构建](build.md) 和 [测试](testing.md)。

## 接手与阶段工作方式

1. 先读根 `AGENTS.md`、本记录及 `third_party/README.md`，再按指南阅读仓库内
   NEOioic/OpenOICC 契约、相关源码和测试。检查 Git 状态，保留已有修改；不要依赖
   会话记忆、旧构建缓存或外部项目目录。可直接使用[交接提示词](next-agent-prompt.md)。
2. 继续编译器职责迁移，每次只完成一个可独立审查、可验证的完整阶段。常规实现和
   验证直接推进，不反复请求确认；不得把所有剩余状态一次性塞进巨型对象。
3. 实现、必要回归和受影响文档同阶段完成。编译器迁移阶段串行执行 Debug/Release
   两条完整验证命令；修复失败并复验，明确实际平台和未验证内容。
4. 测试通过后停下，简述改动与验证，输出一句简短中文 Git 提交信息。由开发者自行
   提交备份；收到“已提交，请继续”等指示后再开始下一阶段。不要自动提交或连续
   开始下一个大阶段。纯文档交接只做事实、链接、编码和差异检查。

## 已实现

- 根 xmake 构建 NeoCC、独立编译的 neocc_compiler、NEOioic 静态库、运行库及十二组
  C 回归。活跃编译路径在 src/，删除 ONE_SOURCE 聚合分支，tinycc/ 仅作差分基线。
- source 对象实际拥有输入缓冲栈、文件句柄及真实文件名，解析器借用当前缓冲。
  构造区由仓库 OpenOICC 预览并显式生成，常规构建不隐式再生。
- tokens 对象拥有标识符驻留、哈希/索引表、token ID 和两类池化分配块；语法和汇编
  模块经 VO 查询 token。删除 table_ident、hash_ident、tok_ident 和 TinyAlloc 活跃
  实现。分配失败先返回 VO，再诊断跳转；宏流调整失败保留旧存储。
- preprocessor 对象拥有宏展开栈、恢复游标、词法模式、工作字符串、字符分类表和
  预处理诊断/计数状态。宏流入栈/退栈经 VO，字符串按统一分配契约释放；命令行预定义
  文本由对象持有，创建输入失败也能收尾。移除对应全局变量和旧 tccpp_init；流分配块
  仍由 tokens 拥有。收尾顺序为语义资源、宏栈、源输入、预处理工作区、token arena。
- 当前 token/值、借用源游标和 PP_DEBUG 缩进已归 preprocessor，删除 tok/tokc/file/indent
  全局定义；语法、汇编和调试模块使用会话工作区。共用 union neocc_value 替代 CValue，
  保留常量与宏流表示。工作区单独 calloc 以满足宿主 long double 对齐，首次打开分配、
  末次关闭释放；reset 保持借用地址，关闭再打开需重新借用。分配失败清理已打开基节点。
  根回归覆盖新增分配失败、token/源游标隔离、reset、浮点编译执行及关闭后拒绝借用。
- symbols 对象拥有语义符号池和回收链，经 VO 分配/归还普通符号记录，保留 TinyCC
  批量分配、逆序挂链与 LIFO 复用。删除全局 sym_free_first、sym_pools、nb_sym_pools
  及 SYM_DEBUG 分支；不再在编译器分配域维护池指针数组。失败先返回 VO，再诊断跳转。
  共用 neocc_symbol/neocc_type 和属性结构，删除 Sym/CType typedef，保留布局与算法。
  符号栈、宏定义和作用域仍归算法管理；清理这些引用及 switch 后才 reset 符号池。
  回归覆盖 600 个符号的增长/复用、对象隔离、未对齐消息、共享打开、部分关闭、池分配
  失败保留旧内容，以及编译中三处池分配失败后的清理和新会话恢复。
- 不含 tinycc/、不带既有缓存的干净副本已验证构建、十二组回归和第二/三阶段自举。
  参考树只影响可选 tinycc_baseline 与差分语料，不是生产依赖。
- 缓冲/文件名分配失败先正常返回 VO，再执行编译器错误跳转；主文件缓冲失败关闭
  已打开的句柄，早期预定义文件失败只清理已初始化的算法资源。
- OpenOICC 的 Windows 目录枚举改用 Win32 API，排他临时文件创建使用 _open 的
  O_EXCL，分别解除 MinGW dirent 链接依赖及旧 MSVCRT 缺少 fopen x 模式的限制。
  保留禁止覆盖语义和失败清理，POSIX 分支保留；本地调整登记在快照来源记录。
- 目标运行支持摘选到 runtime/，由 NeoCC 从 C/汇编源码构建；kernel32 导入声明
  补入 GetProcessHandleCount 供泄漏检查使用。许可仍按来源文件分别适用。

## 验证与复现

平台为 Windows x86-64、MinGW-w64 GCC 15.1.0、xmake 2.9.6 和 Python 3。
完整命令：

```sh
python tests/verify.py --mode debug
python tests/verify.py --mode release
```

包含十二组 C 回归、六组编译集成检查、TinyCC tests2 差分，NeoCC 编译并运行 NEOioic
健壮性/分派/菱形及源对象/token 对象/预处理对象/符号池对象/目录测试。未纳入当前平台的输入及原因保留在脚本和结果
JSON，不把共同失败当作 C 功能正确的证据。

第二、第三阶段从源码重建 NeoCC、NEOioic、OpenOICC 和运行库：19 个目标文件及
两个可执行文件共 21 对逐字节一致，运行库逐阶段比较。阶段 OpenOICC 两次再生后
source/tokens/preprocessor/symbols 构造区无变化，并使用该阶段 NeoCC 重新编译运行四个对象测试。
统一入口还分别对 GCC 构建和第三阶段构建的 OpenOICC 运行 39 项回归。
结果记录在对应构建目录的 regression/results.json、bootstrap/results.json；
生成器测试辅助 C 程序仍由 GCC 编译。构造再生、阶段稳定性与职责迁移分别记载。
最近一次编译器迁移的 Debug/Release 统一命令已完整通过，分别写入对应构建目录的
verification.json 成功记录；120 个差分输入及 21 个排除项见脚本和结果文件。
这些 build/ 结果受忽略，不要求随源码交付，新环境需重新生成，不能当作后续修改的验证结果。
来源清单与 UTF-8/LF 随迁移复核；Git 由开发者管理，提交状态应在接手时重新检查。

## 待迁移与限制

1. 逐步显式传递预处理算法协作上下文，移除对全局 tcc_state 入口的依赖；
   token/值、源游标和宏展开工作区已有独立所有者，宏定义栈仍随语义符号管理，
   不宣称预处理器全部迁移完成。
2. 按 tccgen.c 边界迁移类型状态、符号栈、作用域和表达式栈，保持直接代码生成。
3. 分别迁移目标代码生成、链接输出和装载资源，删除被替代的活跃路径。
4. 增加 Linux/其他目标的配置、ABI、运行库和差分验证。
5. 扩展 TLS/pthread、回溯/边界检查和复杂 DLL/ELF 场景，当前尚未覆盖。

当前仍有 tcc_state、宏定义/语义符号栈、作用域及代码生成全局状态，尚未完成整个 TinyCC 的
对象化重写；不宣称重入、并行或完整 C11 支持。禁止把所有状态塞进一个对象外壳
代替职责迁移。内部基线函数/类型和部分诊断名称按职责迁移逐步更新。

本阶段完成项目 LGPL-2.1-or-later 许可声明，以及语义符号池的所有权、VO 分配/归还和失败清理迁移。
完成阶段验证后停下供开发者提交备份；后续从符号栈、作用域、表达式状态和算法协作继续，
不把本阶段等同于全套重写。

## 下一阶段入口与注意事项

优先阅读 `src/compiler/tccgen.c` 的文件级状态、`tccgen_init`、`tccgen_finish`、
`sym_push2`、`sym_pop` 及作用域进入/退出逻辑，连同 `src/compiler/tcc.h` 的共享声明、
`tccpp.c` 的宏定义栈调用点，选择符号栈或相关语义状态的一个完整职责继续迁移。
这不是要求下一阶段一次完成全部符号栈、作用域和表达式状态；边界应由调用和生命周期决定。

- 现有存储契约在 `include/neocc/symbols.h`，拥有者为 `src/objects/symbols.c`；
  `tokens` 的符号链接只是借用，不另复制一份符号所有权。
- `tccgen_finish` 必须先清理符号/宏定义栈、内联函数、switch 和其他借用，再 reset
  符号池；对象内失败先正常返回 VO，撤销访问记录后才允许驱动诊断跳转。
- 参考 `tests/symbols.c`、`tests/session.c`、`tests/session_fault.c` 补充与迁移匹配的
  隔离、生命周期和错误恢复验证；当前交替会话测试不代表支持重入或并行。
- 新增活跃源码、对象或测试时同步根构建、`tests/bootstrap.py`、
  `tests/regression.py` 的显式列表与验证数量，以及来源和设计文档。
- `ElfW(Sym)` 中的 `Sym` 是 ELF 格式宏参数，不是已删除的语义符号 typedef。
  OpenOICC 当前扫描不接受 C 行拼接，待再生对象源码避免反斜杠续行。

NEOioic 来源提交 f9d415a3728c461ac64ba4a760a8bd49b3a839a8，OpenOICC 来源提交
 ef33912af0088c784aae1a7fc1697671d9f0cdc0。TinyCC 0.9.28rc 的已有嵌套仓库提交为
 e23dabed154a8495f3ac1ff3cc9d04fa1c207893；不据此断言其原用户修改为空。
各快照 NEOCC_SOURCE.json 和本目录 tinycc_sources.json 记录来源、原始哈希及调整。
开发者已指定 NeoCC 采用 LGPL-2.1-or-later，项目版权为 Copyright (C) 2026 CLimber-Rong。
许可证样本已填写项目声明并命名为根 LICENSE；GNU 许可证正文及第三方原有版权、许可和例外保留。
