# NeoCC 迁移交接

截至 2026-10-24，最近完成 P06.1 公共字符串、token 与重放流显式上下文和对应回归，
Debug/Release 全量验证均通过，结果见下文。后续迁移使用仓库内材料，不访问外部项目；Agent 不执行 Git 暂存、提交或推送，
TinyCC 基线保持不变。详细设计、命令和验证边界见
[架构](architecture.md)、[构建](build.md) 和 [测试](testing.md)。

后续未完成工作按 [阶段计划](plan.md) 的 P01～P18 编号交接，包含前置条件、源码入口、
职责边界及验收要求。P01～P05 已完成，P06 拆为公共接口及词法/宏/指令两个子阶段；
P06.1 已完成并通过 Debug/Release 全量验证，后续入口为 **P06.2：词法、宏展开与指令算法显式上下文**。
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

- 根 xmake 构建 NeoCC、独立编译的 neocc_compiler、NEOioic 静态库、运行库及二十六组
  C 回归。活跃编译路径在 src/，删除 ONE_SOURCE 聚合分支，tinycc/ 仅作差分基线。
- source 对象实际拥有输入缓冲栈、文件句柄及真实文件名，解析器借用当前缓冲。
  构造区由仓库 OpenOICC 预览并显式生成，常规构建不隐式再生。
- tokens 对象拥有标识符驻留、哈希/索引表、token ID 和两类池化分配块；语法和汇编
  模块经 VO 查询 token。删除 table_ident、hash_ident、tok_ident 和 TinyAlloc 活跃
  实现。分配失败先返回 VO，再诊断跳转；宏流调整失败保留旧存储。
- preprocessor 对象拥有宏展开栈、恢复游标、词法模式、工作字符串、字符分类表和
  预处理诊断/计数状态。宏流入栈/退栈经 VO，字符串按统一分配契约释放；命令行预定义
  文本由对象持有，创建输入失败也能收尾。移除对应全局变量和旧 tccpp_init；流分配块
  仍由 tokens 拥有。收尾先退出宏栈，再清理内联和语义资源、源输入、预处理工作区及 token arena。
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
  新增函数对象回归；会话故障覆盖十一个构造与两个独立工作区共十三处分配失败，另在普通/
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
  三种嵌套初始化错误、清理及新会话恢复验证。通用临时字符串由 P02 的 temporaries 对象接管。
- P02：temporaries 拥有普通临时字符串记录、缓冲及编译边界；所有操作经 VO，
  删除 stk_data/nb_stk_data、_s 宏及未登记的局部字符串实现。覆盖汇编、递归调试类型、
  覆盖率/回溯生成源码、驱动参数/路径和诊断。跳转前清理当前边界，保留外层源码；
  正常收尾与末次关闭配套，不访问失效的 C 局部地址。公共格式化辅助返回失败，
  诊断内分配失败使用常量消息，预处理特殊诊断避免递归进入自身。
  新增普通/故障对象测试、七种真实编译故障场景、嵌套汇编差分与阶段再生验证。
- P04：tokens 复用宏流分配域管理延迟内联记录、复制文件名和内嵌 body；
  SAVE_INLINE/FIRST_INLINE/RESET_INLINES 经 VO。链保持原声明顺序，引用检查、
  迭代和延迟生成算法保留；宏栈以 alloc=0 借用 body，不承担释放。
  收尾在调试清理后先退出全部宏流，再清理内联和符号；完整 reset/关闭也兜底释放。
  原 InlineFunc、会话数组/数量与 free_inline_functions 删除。
  新增普通/故障资源测试、三种调试配置的目标字节、多文件语义、三类错误差分，
  以及五类会话失败/恢复检查；两个会话在多个翻译单元复用同名 inline 后实际运行。
- P05.1：包含栈、ifdef 栈、pack 栈和游标归已有 preprocessor 工作区；open/reset
  初始化全部栈，删除会话字段与旧深度宏。源缓冲只借用条件栈文件边界，包含栈只借用
  源缓冲；先弹出源再 reset，正常及错误收尾清除借用。诊断、预处理行标记、详细输出
  和类型布局同步使用该工作区；TinyCC 原有限制与 pack 语义保持不变。
  对象回归增加栈隔离/共享打开/reset，session 增加 pack 跨翻译单元重置、条件/pack/
  包含递归越界和其他控制诊断，故障回归统一核对空栈。预处理样例覆盖嵌套 pack 布局，
  递归包含错误保留完整包含链并与基线比较。
- P05.2：preprocessor 拥有包含缓存记录/文件名及跨翻译单元保留的 pragma 库与依赖
  结果。缓存经 VO 查找/创建，沿用路径比较与 hash；桶链替代旧索引数组，RESET 释放。
  两个结果列表经 VO 复制追加、只读借用；保留顺序/重复项，失败不发布新内容，RESET
  保留结果，末次关闭统一释放。旧 CachedInclude、会话数组/数量及清理路径已删除。
  主输入依赖失败关闭拥有的 fd，包含失败在 VO 返回后诊断；pragma 文本经 temporaries
  暂存，右括号等诊断跳转也清理。新增两组对象回归、九类会话失败/恢复及多文件样例，
  验证 once 别名、保护宏重入、依赖输出与库链接；生成构造与阶段对象测试同步。
- P06.1：公共字符串增长/格式化、Unicode 编码、token 驻留/拼写、流分配/追加及
  宏流入栈/退栈均显式接收会话，删除旧签名并同步语法、汇编、调试、链接和驱动调用。
  致命诊断及 error1 显式接收所属会话，VO 失败在访问撤销后诊断；纯 reset/初始化/
  载荷解码不添加上下文。尚未迁移的算法在边界传入当前会话，不新增隐式定位器。
  既有会话回归新增全局入口为空/指向另一会话的直接接口隔离、嵌套流恢复和六类失败
  诊断/分配数检查；根目标仍二十六组。PP_DEBUG 分支由 GCC 构建并与基线完整跟踪比较。
  预处理表达式特殊诊断重放及词法/宏/指令链仍依赖原全局入口，归 P06.2；其他模块
  的算法与诊断入口按后续阶段迁移。
- 不含 tinycc/、不带既有缓存的干净副本已验证构建、二十六组回归和第二/三阶段自举。
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

2026-10-24，Debug 和 Release 两条统一验证命令均完整通过。包含二十六组 C 回归、
六组编译集成检查（含 `scope`、`functions`、`switches`、`initializers`、`temporaries` 和 `translation` 语义样例、120 个 TinyCC 输入差分）、NeoCC 编译并运行
NEOioic 健壮性/分派/菱形及源对象/token 对象/预处理对象/符号对象/表达式对象/作用域对象/函数对象/switch 对象/初始化器对象/临时字符串对象/翻译单元对象/目录测试。
未纳入当前平台的输入及原因保留在脚本和结果 JSON，不把共同失败当作 C 功能正确的证据。

第二、第三阶段从源码重建 NeoCC、NEOioic、OpenOICC 和运行库：26 个目标文件及
两个可执行文件共 28 对逐字节一致，运行库逐阶段比较。阶段 OpenOICC 两次再生后
source/tokens/preprocessor/symbols/expressions/scopes/functions/switches/initializers/temporaries/translation 构造区无变化，并使用该阶段 NeoCC 重新编译运行
十一个对象测试及延迟内联、预处理资源测试、C11 语义样例、作用域、函数、switch、初始化器、临时字符串、翻译单元及多文件内联样例。统一入口还分别对 GCC 构建和第三阶段
构建的 OpenOICC 运行 39 项回归。
结果记录在对应构建目录的 regression/results.json、bootstrap/results.json；
生成器测试辅助 C 程序仍由 GCC 编译。两次验证分别写入 Debug/Release 的
verification.json 成功记录；120 个差分输入及 21 个排除项见脚本和结果文件。
这些 build/ 结果受忽略，不要求随源码交付，新环境需重新生成，不能当作后续修改的验证结果。
tokens 构造区经仓库 OpenOICC 预览并显式再生，无代码差异；阶段工具重复再生稳定。
来源清单与 UTF-8/LF 随迁移复核；Git 由开发者管理，提交状态应在接手时重新检查。

## 待迁移与限制

1. 逐步显式传递预处理算法协作上下文，移除对全局 tcc_state 入口的依赖；
   token/值、源游标、宏展开工作区和宏定义栈已有独立所有者，不宣称预处理器全部迁移完成。
2. 迁移预处理与语法算法的隐式协作，保持直接代码生成；缓存及结果资源已对象化。
3. 分别迁移目标代码生成、链接输出和装载资源，删除被替代的活跃路径。
4. 增加 Linux/其他目标的配置、ABI、运行库和差分验证。
5. 扩展 TLS/pthread、回溯/边界检查和复杂 DLL/ELF 场景，当前尚未覆盖。

当前仍有 tcc_state、目标后端专有全局状态及算法间隐式协作，
尚未完成整个 TinyCC 的对象化重写；不宣称重入、并行或完整 C11 支持。禁止把所有状态
塞进一个对象外壳代替职责迁移。内部基线函数/类型和部分诊断名称按职责迁移逐步更新。

本阶段完成 P06.1 公共字符串、token 与重放流的显式上下文，Debug/Release 全量验证通过。
本阶段停下供开发者提交备份；后续从 P06.2 的词法、宏展开与指令调用链继续。

## 下一阶段入口与注意事项

按 [阶段计划](plan.md) 从 P06.2 开始；开发者指定其他编号时先核对其前置阶段。
优先阅读 `tccpp.c` 的词法、宏展开、指令和表达式调用链及语法/汇编/诊断调用点，
逐步显式传递所属会话与职责对象，移除这些算法对全局 `tcc_state` 的查询。
现有控制栈、缓存和结果所有权不重复迁移；保留正常/错误清理及跨翻译单元结果有效期。
P06.1 公共接口已要求显式会话，后续调用链直接使用其参数，不重新引入隐式签名；
`_tcc_error` 已要求会话，非致命诊断和 warning 尚从旧入口取会话。特殊表达式诊断
`pp_error` 仍需与 `next` 词法/展开链一同迁移，不能只改其外层签名便宣称完成。

- P05.2 契约在 `include/neocc/preprocessor.h`；INCLUDE 返回缓存借用，RESET 后失效。
  RESULTS 返回只读结果视图，RESET 保留列表，末次关闭结束借用；成功追加可能改变
  items 数组，不能跨追加保留该数组。平台路径纯辅助位于 `src/support/include_path.h`，
  再生副本必须携带此头，不能向待扫描对象 C 引入条件预处理。

- P04 契约在 `include/neocc/tokens.h`，记录及流缓冲由 tokens 统一拥有。
  新记录先通过 VO 登记，再填充内嵌 body；内联重放只能借用（alloc=0），禁止
  用 tok_str_free 释放内嵌描述符。生成时可清除 symbol 借用，next/文件名不改动。
  必须先退出宏栈并清除恢复游标，再 RESET_INLINES，随后释放符号存储。

- P03 的 `translation` 对象经 VO 借用为 `TCCState.unit`，拥有匿名编号、指令游标、
  抑制位、调试模式及代码节借用；原 `anon_sym`、`ind`、`nocode_wanted`、`debug_modes`
  共享定义和 `cur_text_section` 字段/宏均删除。节仍由链接器拥有。
  初始化覆盖 C、预处理和汇编；收尾在调试/标签清理后 reset，先于作用域和符号池。
  独立汇编显式采用原零初始化函数起点，保持 STABS 行记录和重定位字节。
  对象回归覆盖共享打开、隔离、未对齐借用、reset 和部分关闭；会话诊断覆盖普通 C、
  STABS、DWARF、预处理表达式及汇编五条路径，检查零状态、VO、借用节与新会话恢复。

- 临时字符串契约在 `include/neocc/temporaries.h`，对象在 `src/objects/temporaries.c`。
  描述符由对象拥有，禁止重新登记 C 局部地址。ENTER 在主输入分配前建立边界，
  UNWIND 必须在诊断 longjmp 前执行，LEAVE 收尾；无边界 UNWIND 不释放外层源码。
  普通 POP 严格 LIFO，RESET/末次关闭释放全部；分配失败先返回 VO 再诊断。
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
