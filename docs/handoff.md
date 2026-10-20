# NeoCC 迁移交接

截至 2026-10-20，最近完成 P01 初始化器工作区与资源对象迁移和对应回归，Debug/Release
全量验证结果见下文。后续迁移使用仓库内材料，不访问外部项目；Agent 不执行 Git 暂存、提交或推送，
TinyCC 基线保持不变。详细设计、命令和验证边界见
[架构](architecture.md)、[构建](build.md) 和 [测试](testing.md)。

后续未完成工作按 [阶段计划](plan.md) 的 P01～P18 编号交接，包含前置条件、源码入口、
职责边界及验收要求。P01 已完成，当前下一阶段为 **P02：临时字符串登记与诊断跳转清理**。
后续每完成一个阶段，同步计划状态与本记录。

## 接手与阶段工作方式

1. 先读根 `AGENTS.md`、本记录及 `third_party/README.md`，再按指南阅读仓库内
   NEOioic/OpenOICC 契约、相关源码和测试。检查 Git 状态，保留已有修改；不要依赖
   会话记忆、旧构建缓存或外部项目目录。交接提示由开发者自行指定，Agent 不额外编写交接提示文件。
2. 继续编译器职责迁移，每次只完成一个可独立审查、可验证的完整阶段。常规实现和
   验证直接推进，不反复请求确认；不得把所有剩余状态一次性塞进巨型对象。
3. 实现、必要回归和受影响文档同阶段完成。编译器迁移阶段串行执行 Debug/Release
   两条完整验证命令；修复失败并复验，明确实际平台和未验证内容。
4. 测试通过后停下，简述改动与验证，输出一句简短中文 Git 提交信息。由开发者自行
   提交备份；收到“已提交，请继续”等指示后再开始下一阶段。不要自动提交或连续
   开始下一个大阶段。纯文档交接只做事实、链接、编码和差异检查。

## 已实现

- 根 xmake 构建 NeoCC、独立编译的 neocc_compiler、NEOioic 静态库、运行库及十九组
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
- symbols 对象拥有语义符号池、回收链、global/local/宏定义/global label/local label
  五类栈头及当前作用域层级，经 VO 分配、入栈、复制、移动、出栈和归还普通符号记录，
  保留 TinyCC 批量分配、逆序挂链、LIFO 复用和 move-ref 栈顺序。删除全局
  sym_free_first、sym_pools、nb_sym_pools、五个栈头、local_scope 及 SYM_DEBUG 分支；
  不再在编译器分配域维护池指针数组。失败先返回 VO，再诊断跳转。
  共用 neocc_symbol/neocc_type 和属性结构，删除 Sym/CType typedef，保留布局与算法。
  宏定义和作用域语义仍由算法解释；清理 token 的符号/宏/标签链接及 switch 后才 reset。
  正常收尾检查标签，错误收尾由 label_discard 静默撤销剩余标签链接。
  回归覆盖 600 个符号的增长/复用、五类栈、作用域边界、copy/move、对象隔离、未对齐
  消息、共享打开、部分关闭、push/池分配失败保留原状态，以及编译中三处池分配失败和
  未定义标签错误后的清理、新会话恢复。
- expressions 对象拥有值栈数组、真实空栈哨兵、栈顶和四种常用类型，删除 vtop、_vstack、
  常用类型全局定义及 SValue typedef，改用 neocc_expression_value 保持布局。工作区独立
  calloc 满足宿主 long double 对齐，经 VO 借用和 reset；正常和错误收尾均清除值与类型
  的符号引用，再 reset 符号池。共享打开、部分关闭和再次打开沿用运行时契约。
  新增对象回归，并扩展双会话和值栈耗尽错误后的清理验证。
- scopes 对象拥有普通作用域帧、当前/根/循环入口和清理/goto 链头，删除原文件级
  scope 结构与 cur_scope/loop_scope/root_scope/all_cleanups/pending_gotos 定义。
  帧经 VO 入栈/出栈，按嵌套深度增长并 LIFO 复用，reset/末次关闭释放 active/available
  两条链；工作区内嵌且 reset 保持地址。短作用域仅保留局部符号边界，既有 VLA、cleanup、
  goto 和直接代码生成算法不变；符号记录仍归 symbols，跳转目标借用解析调用的局部变量。
  正常函数结束出栈根帧，错误收尾先清理 switch/标签，再 reset scopes/functions/expressions/symbols。
  新增普通/故障两组对象回归，覆盖 258 层继承、复用、隔离、非法出栈及部分关闭；会话
  故障覆盖五处根/嵌套帧分配失败，以及 cleanup/goto
  已建立后的诊断跳转和新会话恢复。scope 样例增加清理属性、前后向 goto、VLA、循环/
  switch 和语句表达式，并加入预处理与三种调试配置下的目标字节比较。
- functions 对象拥有当前函数返回类型/名称/起始位置、变参/旧式标志、局部栈偏移、隐式
  结构体返回地址、返回跳转链和临时槽缓存，删除原对应全局状态和共享声明。工作区内嵌，
  经 VO BEGIN、借用、TEMP 和 reset；类型/名称借用 symbols/tokens，不重复拥有。
  旧式参数声明暂用同一 type，函数开始时替换为返回类型。TEMP 保留首次匹配、占用位图、
  short 大小/对齐及八项缓存算法；缓存满后继续分配，偏移计算检查 int 可表示范围，失败
  不改动状态。正常函数结束在调试/异常帧和符号大小输出后 reset；错误收尾在 debug/
  switch/标签清理后 reset scopes/functions/expressions，再 reset symbols。
  新增函数对象回归；会话故障覆盖九个构造与两个独立工作区共十一处分配失败，另在普通/
  DWARF 配置下观察填充的函数状态、临时槽和返回链，触发真实诊断后检查清理及恢复。
  新增 functions 样例覆盖结构体返回、十个结构体实参、递归/间接/内联调用、变参浮点、
  旧式参数、函数指针返回、VLA 参数和 __func__，纳入运行/目标字节比较及阶段测试。
- switches 对象拥有当前 switch 链、case 记录和索引数组，删除原文件级入口及解析器的
  分配/释放路径；经 VO ENTER/CASE/REMOVE/EXIT/reset 管理所有权。沿用 TinyCC 的
  排序、范围合并和分支生成，qsort 比较模式改由显式选择的纯比较函数提供。
  malloc 保证 switch 值的宿主对齐；索引扩容失败释放尚未发布的 case 并保留旧状态。
  RESET/末次关闭清理整条链，不解引用诊断跳转后已失效的解析器局部地址。
  新增普通/故障对象回归覆盖 258 层、case 增长、未对齐消息、非法退出、共享打开、
  部分关闭、构造/记录/索引分配失败；会话回归覆盖六处帧/case 分配与索引扩容失败，
  以及嵌套语法诊断、重复 case/default、非整数 switch、case 表达式错误后的清理。
  新增 switches 语义样例覆盖多 case 查找、范围、64 位无符号边界、嵌套、break/continue
  和代码抑制，纳入运行、预处理/目标字节差分和阶段自举。
- P01：initializers 对象拥有共享拼接字符串和全局复合字面量模式，删除 initstr/global_expr
  全局定义；初始化器、属性、汇编字符串和静态断言消费者均借用该工作区。RESET/末次关闭
  释放字符串并清除模式，正常与错误收尾均执行。初始化器重放流保留 tokens arena 与
  preprocessor 宏栈的已有所有权，未入栈的分配随 arena 回收；init_params 保持局部。
  新增对象生命周期测试和 initializers 语义样例；会话测试新增两个共享缓冲扩容失败、
  三种嵌套初始化错误、清理及新会话恢复验证。通用临时字符串登记仍待 P02 迁移。
- 不含 tinycc/、不带既有缓存的干净副本已验证构建、十九组回归和第二/三阶段自举。
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

2026-10-20，Debug 和 Release 两条统一验证命令均完整通过。包含十九组 C 回归、
六组编译集成检查（含 `scope`、`functions`、`switches` 和新增 `initializers` 语义样例、120 个 TinyCC 输入差分）、NeoCC 编译并运行
NEOioic 健壮性/分派/菱形及源对象/token 对象/预处理对象/符号对象/表达式对象/作用域对象/函数对象/switch 对象/初始化器对象/目录测试。
未纳入当前平台的输入及原因保留在脚本和结果 JSON，不把共同失败当作 C 功能正确的证据。

第二、第三阶段从源码重建 NeoCC、NEOioic、OpenOICC 和运行库：24 个目标文件及
两个可执行文件共 26 对逐字节一致，运行库逐阶段比较。阶段 OpenOICC 两次再生后
source/tokens/preprocessor/symbols/expressions/scopes/functions/switches/initializers 构造区无变化，并使用该阶段 NeoCC 重新编译运行
九个对象测试、C11 语义样例、作用域、函数、switch 和初始化器样例。统一入口还分别对 GCC 构建和第三阶段
构建的 OpenOICC 运行 39 项回归。
结果记录在对应构建目录的 regression/results.json、bootstrap/results.json；
生成器测试辅助 C 程序仍由 GCC 编译。两次验证分别写入 Debug/Release 的
verification.json 成功记录；120 个差分输入及 21 个排除项见脚本和结果文件。
这些 build/ 结果受忽略，不要求随源码交付，新环境需重新生成，不能当作后续修改的验证结果。
新增 initializers 构造区经仓库 OpenOICC 预览后显式写入；阶段工具重复再生无差异。
来源清单与 UTF-8/LF 随迁移复核；Git 由开发者管理，提交状态应在接手时重新检查。

## 待迁移与限制

1. 逐步显式传递预处理算法协作上下文，移除对全局 tcc_state 入口的依赖；
   token/值、源游标、宏展开工作区和宏定义栈已有独立所有者，不宣称预处理器全部迁移完成。
2. 按 tccgen.c 边界迁移临时字符串登记和翻译单元代码生成工作状态，保持直接代码生成。
3. 分别迁移目标代码生成、链接输出和装载资源，删除被替代的活跃路径。
4. 增加 Linux/其他目标的配置、ABI、运行库和差分验证。
5. 扩展 TLS/pthread、回溯/边界检查和复杂 DLL/ELF 场景，当前尚未覆盖。

当前仍有 tcc_state、临时字符串登记、指令游标/代码抑制、目标后端专有全局状态及算法间隐式协作，
尚未完成整个 TinyCC 的对象化重写；不宣称重入、并行或完整 C11 支持。禁止把所有状态
塞进一个对象外壳代替职责迁移。内部基线函数/类型和部分诊断名称按职责迁移逐步更新。

本阶段完成 P01 初始化器工作区与共享字符串的所有权迁移、活跃调用点替换和失败清理验证。
完成阶段验证后停下供开发者提交备份；后续从剩余语法/代码生成状态和算法协作继续。

## 下一阶段入口与注意事项

按 [阶段计划](plan.md) 从 P02 开始；开发者指定其他编号时先核对其前置阶段。
优先阅读 `src/compiler/libtcc.c` 的 `stk_data`、`nb_stk_data`、诊断跳转前清理，以及
`tcc.h` 的 `cstr_new_s`、`cstr_free_s` 和全部调用点，明确登记所有者与局部地址有效期。
P03 再迁移代码抑制、指令游标和当前代码节；不要跨阶段继续推进。

- 初始化器契约在 `include/neocc/initializers.h`，对象在 `src/objects/initializers.c`；
  算法借用 `TCCState.initializer`。RESET 保持工作区地址，释放字符串并清除模式；
  `parse_mult_str` 返回内容仅有效至下一次字符串解析或 reset。重放流不重复归对象拥有。
- switch 契约在 `include/neocc/switches.h`，对象在 `src/objects/switches.c`；算法借用
  `TCCState.switch_state`。EXIT/REMOVE/reset 结束对应记录的借用；reset 先于 scopes、
  expressions 和 symbols，不能解引用 longjmp 后遗留的 break 目标或解析器局部地址。
  语义排序可以重排索引和修改 case 字段，但链、数量、容量及记录生命周期由对象管理。
- 新增表达式工作区契约在 `include/neocc/expressions.h`，对象在 `src/objects/expressions.c`；
  算法借用 `TCCState.expr`，不可重新定义全局栈顶。表达式 reset 必须在符号池 reset 之前。
- 作用域契约在 `include/neocc/scopes.h`，对象在 `src/objects/scopes.c`，算法借用
  `TCCState.scope`。帧出栈即结束借用，reset 后所有帧失效；清理 switch/标签后先 reset
  scopes/functions，再 reset 表达式和符号池。不得保留指向已退出解析调用的 break/continue 局部地址。
- 函数契约在 `include/neocc/functions.h`，对象在 `src/objects/functions.c`，算法借用
  `TCCState.function`。先完成调试/异常帧输出，再 reset 函数工作区，释放符号/token 存储前
  必须清除类型/名称借用；临时槽的占用位图来自表达式 r2 标签，不把表达式栈复制到函数对象。
  x86-64 的补丁位置、scratch/alloca 和 SysV 变参寄存器布局仍是后端状态，不宣称已迁完。
- 现有存储与栈契约在 `include/neocc/symbols.h`，拥有者为 `src/objects/symbols.c`；
  `tokens` 的符号链接只是借用，不另复制一份符号所有权。
- 五个栈头与作用域层级已经对象化；`tccgen_finish` 仍必须先撤销 token 链接、清理
  宏定义、标签、内联函数、switch 和其他借用，再 reset 符号池。对象内失败先正常返回
  VO，撤销访问记录后才允许驱动诊断跳转。
- 参考 `tests/symbols.c`、`tests/session.c`、`tests/session_fault.c`、`tests/cases/scope.c`
  补充与迁移匹配的隔离、生命周期和错误恢复验证；当前交替会话测试不代表支持重入或并行。
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
