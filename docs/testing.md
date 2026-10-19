# 验证范围

验证平台为 Windows x86-64、MinGW-w64 GCC 15.1.0、xmake 2.9.6。
Debug/Release 均使用 C11；测试检查不依赖 `assert`，Release 仍执行测试动作。
以下记录只覆盖所列组合和输入，不表示完整 C11、其他平台或全部 TinyCC 功能通过。

2026-10-19，`python tests/verify.py --mode debug` 和 `--mode release` 均完整通过：
各九组根 C 回归、六组编译集成检查（含 120 个 TinyCC 输入差分）、19 对阶段产物
一致性及两种构建方式下各 39 项 OpenOICC 回归。21 个语料排除项见下文。
两种模式均生成 `verification.json` 成功记录。

## 根 C 回归

`xmake test -P . -v` 执行九组：

- `runtime_dispatch_test`：七个接力接口的结果、访问撤销和席位耗尽。
- `runtime_robustness_test`：原 NEOioic 的队列随机操作、模式、引用、共享祖先、
  构造失败、OID 边界、动态/外部存储和部分关闭。
- `source_test`：未对齐消息、长名称/长度、EOB、257 层缓冲、真实文件名、借用
  stdin、拥有句柄、共享打开、对象隔离、1000 次 OID 回收及部分关闭重试。
- `source_fault_test`：构造区与两队列分配失败、缓冲失败保留句柄所有权、文件名
  分配失败保留原名，并记录释放数量。
- `session_test`：64 轮交替双会话编译、宏隔离、重定位后实际调用、带行号的诊断、
  错误后新会话编译和句柄计数。
- `session_fault_test`：三处源缓冲失败，核对 `longjmp` 后资源、访问记录及后续编译。
- `tokens_test`：未对齐消息、2000 个名称及索引扩容、驻留稳定性、对象隔离、对齐、
  跨对象指针拒绝、尺寸溢出、宏流增长/缩小/释放、共享打开和 reset/关闭后的重新编号。
- `tokens_fault_test`：索引首次分配、扩容和符号块分配失败，不发布 token；宏流扩展
  分配失败保留原内容；最终释放数量与启动时一致。
- `directory_test`：Windows 空目录、缺失目录、文件作为目录、含空格名称、回调失败
  的 1000 次枚举与句柄清理。

宿主故障注入依赖 GNU ld `--wrap`；不是 NeoCC 生产开关。
`session_fault_test` 另注入两个对象的构造失败、宏流和标识符两个 arena 的分配失败，检查输入清理、token
对象 VO 记录撤销，以及之后新会话的编译；原有会话错误计数累积语义保持不变。

## 纯 C 与 NEOioic 编译差分

`tests/regression.py` 包含六组集成检查：

- 自有 C11 程序在 GCC、TinyCC 基线和 NeoCC 下编译运行，比较输出与退出码；覆盖
  类型、作用域、结构体/浮点/可变参数 ABI、VLA、对齐、指针及跨文件重定位。
- 对同一输入比较 TinyCC/NeoCC 预处理文本和 ELF 目标字节。
- 包含链与 `#line` 诊断、缺失文件、输出失败，比较错误退出和诊断内容。
- NeoCC 编译并实际运行 NEOioic 分派、健壮性、菱形继承和本项目源对象/token 对象/目录测试。
  NEOioic 故障注入通过测试翻译单元的编译期函数替换实现，真实分配器另编译；
  不要求 NeoCC PE 链接器支持 GNU `--wrap`。
- TinyCC `tests/tests2` 的 120 个输入逐项差分；21 个平台特定或需要本阶段未启用运行库的输入明确排除，
  完整名单和原因保留在脚本及运行后的 `regression/results.json`。

语料差分同时比较结果和退出码；共同失败只能证明未产生迁移差异，不能作为其 C
功能正确的证据。`34_array_assignment` 为上游已排除的非标准扩展；i386/ARM/RISC-V、
pthread/TLS、多 DLL 边界检查与回溯、ELF 动态链接案例未纳入当前覆盖。
没有把这些案例改成宽松的“通过”。GCC/新建程序提供额外正向结果依据。

## 自举及构造再生

`tests/bootstrap.py` 从 GCC 构建的第一阶段 NeoCC 开始，以普通 C 源码独立编译
CLI、宿主、源对象、所有当前后端/编译模块和 NEOioic，链接第二阶段；再用第二阶段
构建第三阶段。各阶段还重新编译运行库和 OpenOICC。

第二/三阶段的 17 个 NeoCC/NEOioic 目标文件、NeoCC 可执行文件和 OpenOICC
可执行文件共 19 对逐字节一致；`libtcc1.a`、`runmain.o` 也逐阶段比较。
不启用调试路径或时间戳等可变字段，不以修改预期哈希掩盖差异。
`bootstrap/results.json` 记录本次产物 SHA-256，不提交本机绝对路径。

第二、第三阶段 OpenOICC 各在构建目录副本中预览、显式再生两次，源构造区均无变化，
再用该阶段 NeoCC 编译并运行源对象及 token 对象测试；阶段工具还编译运行 C11 语义样例。
这证明当前源码/目标组合的阶段稳定性，不代表整个编译器对象化完成。

`tests/verify.py` 还对 GCC 构建和第三阶段 NeoCC 构建的 OpenOICC 分别运行全部
39 项集成回归，覆盖随机图、故障分配、布局、区外字节、幂等再生和再编译。
测试辅助 C 程序仍由 GCC 构建；不得把此结果描述为其所有辅助程序都由 NeoCC 编译。

## 不含 TinyCC 参考树的验证

`tests/independent.py --mode debug`（或 `release`）每次建立全新的仓库内源码副本，
不复制 `tinycc/`、现有产物或缓存。在副本中用 GCC/xmake 构建，执行九组根回归，
再以 NeoCC 完成第二/三阶段编译、运行库重建、19 对输出比较和两个对象的构造再生。
两种模式的独立性结果位于 `build/independent/debug.json` 和 `release.json`。
完整验证入口包含该检查；独立性副本不运行需要参考语料的 TinyCC 差分。
它证明构建和运行不依赖参考树，不能证明全套对象化重写已经完成。
