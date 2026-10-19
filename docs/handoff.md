# NeoCC 迁移交接

2026-10-19 开始编译器实现。全程使用仓库内材料，没有访问外部项目，没有初始化、
暂存、提交 Git，未修改 TinyCC 基线。详细设计、命令和验证边界见
[架构](architecture.md)、[构建](build.md) 和 [测试](testing.md)。

## 已实现

- 根 xmake 构建 NeoCC、独立编译的 neocc_compiler、NEOioic 静态库、运行库及十组
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
- 不含 tinycc/、不带既有缓存的干净副本已验证构建、十组回归和第二/三阶段自举。
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

包含十组 C 回归、六组编译集成检查、TinyCC tests2 差分，NeoCC 编译并运行 NEOioic
健壮性/分派/菱形及源对象/token 对象/预处理对象/目录测试。未纳入当前平台的输入及原因保留在脚本和结果
JSON，不把共同失败当作 C 功能正确的证据。

第二、第三阶段从源码重建 NeoCC、NEOioic、OpenOICC 和运行库：18 个目标文件及
两个可执行文件共 20 对逐字节一致，运行库逐阶段比较。阶段 OpenOICC 两次再生后
source/tokens/preprocessor 构造区无变化，并使用该阶段 NeoCC 重新编译运行三个对象测试。
统一入口还分别对 GCC 构建和第三阶段构建的 OpenOICC 运行 39 项回归。
结果记录在对应构建目录的 regression/results.json、bootstrap/results.json；
生成器测试辅助 C 程序仍由 GCC 编译。构造再生、阶段稳定性与职责迁移分别记载。
Debug/Release 的上述统一命令已完整通过，分别写入 verification.json 成功记录。
来源清单与 UTF-8/LF 随迁移复核；Git 由开发者管理，本轮只提供工作区修改。
进入本阶段时工作区干净；未修改外部项目或 TinyCC 基线。

## 待迁移与限制

1. 逐步显式传递预处理算法协作上下文，移除对全局 tcc_state 入口的依赖；
   token/值、源游标和宏展开工作区已有独立所有者，宏定义栈仍随语义符号管理，
   不宣称预处理器全部迁移完成。
2. 按 tccgen.c 边界迁移类型、符号、作用域和表达式栈，保持直接代码生成。
3. 分别迁移目标代码生成、链接输出和装载资源，删除被替代的活跃路径。
4. 增加 Linux/其他目标的配置、ABI、运行库和差分验证。
5. 扩展 TLS/pthread、回溯/边界检查和复杂 DLL/ELF 场景，当前尚未覆盖。

当前仍有 tcc_state、宏定义/语义符号及代码生成全局状态，尚未完成整个 TinyCC 的
对象化重写；不宣称重入、并行或完整 C11 支持。禁止把所有状态塞进一个对象外壳
代替职责迁移。内部基线函数/类型和部分诊断名称按职责迁移逐步更新。

本阶段边界为当前 token/值、源游标和调试缩进的所有权迁移，以及工作区的对齐与生命周期。
完成阶段验证后停下供开发者提交备份；后续从语义状态和算法协作继续，不把本阶段等同于全套重写。

NEOioic 来源提交 f9d415a3728c461ac64ba4a760a8bd49b3a839a8，OpenOICC 来源提交
 ef33912af0088c784aae1a7fc1697671d9f0cdc0。TinyCC 0.9.28rc 的已有嵌套仓库提交为
 e23dabed154a8495f3ac1ff3cc9d04fa1c207893；不据此断言其原用户修改为空。
各快照 NEOCC_SOURCE.json 和本目录 tinycc_sources.json 记录来源、原始哈希及调整。
未创建根 LICENSE，TinyCC 衍生实现不重新标注 MIT。
