# OpenOICC 0.1 命令与生成契约

OpenOICC 是使用 C11 与 NEOioic 实现的控制台工具。应用宿主管理 OID；项目对象通过 `vo_open`、`vo_interact2`、`vo_close` 协调源码文档、标记解析、继承分析、构造生成和文件提交。文档与节点模型由项目独占，算法使用同一模型，不为每个纯计算步骤创建对象。

工具专为 NEOioic 生成代码。NEOioic 由 OIOIC 发展而来，未来可能有不兼容的 API 更新；本契约只对应仓库内固定的 NEOioic 快照，升级时需同步适配生成器与构造辅助实现，并重新执行行为验证。

## 命令

首次使用请先阅读 [用户指南](openoicc-user-guide.md)，其中包含可编译的最小示例与常见问题。正式名称为 OpenOICC，可执行文件名为 `openoicc`，输入语法仍使用 `<oicc>` 标记。

在仓库根目录构建后运行：

```sh
xmake -P . openoicc
xmake run -P . openoicc --help
xmake run -P . openoicc mo Demo build
xmake run -P . openoicc go car build/mo/Demo
xmake run -P . openoicc build/mo/Demo
xmake run -P . openoicc new boat path/to/boat.c
xmake run -P . openoicc new amphibian path/to/amphibian.c "car, ship"
xmake run -P . openoicc check path/to/car.c path/to/ship.c path/to/amphibian.c
xmake run -P . openoicc generate path/to/car.c path/to/ship.c path/to/amphibian.c
xmake run -P . openoicc generate --write path/to/car.c path/to/ship.c path/to/amphibian.c
```

`-h`、`-help`、`--help` 显示同一帮助。`openoicc go|ao|co|ro|mo NAME [DIRECTORY]` 创建分类节点，类型不区分大小写，目录默认当前目录。`co/ro/mo` 在 `DIRECTORY/类型/名称` 创建宿主；目录末项已经是该类型时不再重复。宿主包含同名节点、`ho.c/ho.h`、`se`、`so/go`、`so/ao`；`mo` 另有 `so/ao/Main` 入口。`go/ao` 在指定宿主的 `so/类型/名称` 创建节点。节点初始继承 `NEOioic`，提供九接口、头文件和空构造区。只生成有职责的源码及分组头文件，不生成空分组 `.c` 或历史模板注释。

分类创建保留 `go/ao/co/ro/mo/ho/so/se/ce/re/OIOIC/NEOioic` 的全部大小写变体，同一宿主的 go/ao 节点名称不得相互冲突，也不得与宿主同名。不覆盖已有内容；创建失败清理本次新建的文件与目录，清理失败报告路径。宿主骨架需要用户补充 OID、模型编号、成员头文件、构造器声明与创建分支，才能编译运行。

`openoicc [DIRECTORY]` 进入目录构造模式，默认当前目录，**直接写入**。读取宿主 `ho.c`（不存在时尝试 `host.c`），递归收集含注释 `<oicc>` 标记的 `.c` 文件，校验完整继承图。只为 `host_create_object` 函数体中直接调用的 `crt_name(...)` 生成构造器，清空其他节点的 `<crt>` 内容；这些节点仍可作为基节点参与派生对象构造。宿主创建分支由用户维护，工具不修改宿主。函数中的局部 `extern int crt_name(...)` 声明、注释、字符串及其他函数里的调用均不选择目标。构造器调用须直接书写，不支持宏展开、函数指针或间接选择；条件编译和不明确的宿主定义报错。扫描包含子目录中的全部 `.c`，请将构建副本放在宿主目录外；不跟随目录链接，无标记文件仍须符合支持的 C 文本词法规则。

`new` 创建 UTF-8、LF、无自有成员的单文件九接口骨架，默认直接基节点为 `NEOioic`。目标目录须已存在，文件名须与节点名一致，不覆盖已有文件。继承列表作为一个逗号分隔的参数传入；创建时校验语法，基节点是否存在由项目校验负责。节点名采用 ASCII 字母开头、随后字母/数字/下划线的名称，根名 `NEOioic` 的所有大小写变体保留。

`check` 解析并校验全部输入，计算构造输出，报告会改变的文件，不写入。`generate` 默认同样只报告计划，只有 `--write` 提交更新。这两个显式文件命令为每个输入生成构造器，所有非根基节点的 `.c` 必须显式列出，参数顺序不影响同一节点的输出。每个文件对应一个节点；文件名去掉 `.c` 后与 `tog_name` 的后缀须不区分大小写相同，实际节点名取绑定后缀的拼写（例如 `Main.c` 中的 `tog_main`）。同名节点和大写后冲突的 `MSN_*` 名称都报错。

退出码：0 成功，1 输入/构造/文件操作失败，2 命令用法错误。源码校验诊断通常包含文件、行、列及原因；部分名称和图诊断指向文件起点，命令或资源错误可能只有原因。成功校验并不替代 C 编译和业务行为验证。

## 支持的源文件

一个源文件包含且仅包含一个完整区域：

```c
/* <oicc> */
/* <ibn> car, ship </ibn> */
/* <crt> */
/* 工具只替换这两个 crt 注释之间的字节。 */
/* </crt> */
/* </oicc> */
```

标记只在 C 注释中识别，字符串和字符字面量中的标记忽略。`<ibn>` 在同一条注释内闭合；两个 `<crt>` 边界必须各自位于独立的块注释中，且位于文件作用域。重复、错序、未闭合标记以及损坏的 C 注释/字面量、区外花括号不配对会失败。根节点无输入文件，直接基节点列表必须非空（无其他基节点时写 `NEOioic`）。

业务代码提供唯一的 `void tog_name(struct object *)` 定义。自有成员可定义为 `struct em_name { ... };`，放在本文件构造区外或同目录同名 `.h` 的文件作用域中。没有该结构定义时不分配自有成员；重复、局部定义及引用了未直接定义的自有成员类型会拒绝。工具不解析整个 include 树，也不展开宏、不解析 typedef 或属性修饰的成员结构声明；成员定义须直接可见，源文件内的条件预处理拒绝，成员头文件只允许一个常规外层 `#ifndef` 守卫。禁止 C 行拼接、三字符替换和双字符预处理指令，输入必须是合法 UTF-8 且不含 NUL，允许 UTF-8 BOM。成员布局的最终有效性由 C 编译及构造器验证。

源文件必须让该构造器用到的所有 `struct em_*` 定义可见，并提供宿主 `MSN_NAME` 宏；模型编号必须非零、互不相同且可由 `unsigned short` 表示。`new` 通过 `host.h` 引入应用契约；请在该应用头文件中管理模型编号和必要的成员头文件。不能把不同节点的编号都设为同一个默认值。

## 输出与运行时

生成的 `crt_name` 接口与仓库当前构造器一致。生成代码包含 `openoicc/construct.h`；编译需把仓库 `include/` 及 NEOioic `include/` 加入头文件搜索路径，链接当前 NEOioic 及所有节点业务实现。`construct.h` 是不依赖 openoicc 可执行程序的 C11 头文件实现，可随生成源码一同分发并保留 MIT 许可。

节点按有序深度优先前序遍历去重，最终派生节点位于首项；直接基节点保持 `<ibn>` 顺序，祖先集合使用同一遍历顺序。共享祖先只分配一份。每个节点的席位倍数为从构造目标到该节点的最长继承路径长度加一。该规则为逐层向基节点的嵌套委托保留外部调用者与各层委托者记录；普通 OBS/SBO 沿用原上下文。一个委托 VO 返回时清除对应祖先上下文，因此先后访问不同分支不累加同时在用席位。此规则覆盖单线程、沿继承方向委托的模型；任意跨分支委托、循环重入及并发调用不在这一容量保证内。

菱形 amphicar → car / ship → vehicle → NEOioic 得到节点顺序 amphicar、car、vehicle、NEOioic、ship，倍数 1、2、3、4、2。回归通过真实 OBS 与嵌套 VO 委托验证这些容量被使用及释放，而非仅对比生成文本。

布局逐块检查尺寸加乘溢出和对齐，自有成员使用 `NEOIOIC_ALIGNED_SIZE`；超过 `struct object` 对齐的成员明确返回 `RN_INVALIDARG`。`caller_capacity` 必须为正，乘席位倍数后不超过 `USHRT_MAX`。模型编号有编译期范围检查，重复编号在构造时拒绝。输入/输出队列独立分配，容量零禁用，空队列头尾为 NULL。

构造失败不发布动态对象、不消耗 OID，释放本次分配的队列和本体。参数和布局校验失败不会写外部存储；开始初始化后的队列分配失败会清零本次构造区。成功才推进 OID，低 16 位不允许跨入宿主编号。openoicc 宿主可回收并重用节点 OID；生成函数的 OID 来源由目标应用宿主管理。

外部存储的容量和对齐仍由调用者保证，构造接口没有容量参数；不得用 `destroy_object` 释放外部本体。关闭返回 `RN_CLOSEPARTIAL` 时只允许重试关闭，成功清理后再销毁。生成器保留业务接口代码及运行时 OBS/SBO 语义。

## 写入和再生

全部输入先完成解析、图校验和生成；需要改变的文件先写入同目录独占临时文件，刷新后再次核对全部原文件。然后逐个使用平台替换操作提交。无变化文件不写入；区外字节逐字保留，LF/CRLF 生成区域按输入约定输出。

仓库源码统一保存为 LF；工具仍可处理外部 CRLF 输入并保留其约定。孤立的 CR 换行会明确拒绝，避免注释边界与行位置解释不一致。

多文件替换不构成事务：校验或准备失败不提交任何文件；提交阶段失败时已经报告 `committed` 的文件保留更新，未提交临时文件在关闭项目时清理。修复错误后重新运行即可继续。原文件核对降低普通并发编辑覆盖风险，但不是文件锁或恶意并发修改防护。只接受普通源文件，拒绝文件符号链接/Windows reparse point。Windows 文件路径当前使用系统窄字符接口；空格路径已验证，不能宣称任意 Unicode 路径已支持。

工具自身构造代码已纳入版本管理，常规构建直接编译，不隐式再生：

```sh
xmake run -P . openoicc generate --write src/objects/project.c
xmake -P . openoicc
xmake run -P . openoicc generate --write src/objects/project.c
```

第二次应没有变更。测试还会在 `build/tests/` 中复制工具源码、再生、重新编译并再次检查稳定性。

## 原工具一致性边界

已按原工具帮助、节点标记及宿主创建函数核对并补齐分类创建、默认目录构造与所需构造器选择。内部名称采用当前 NEOioic API，不提供旧 API 兼容层；安全报告涉及的队列、布局、容量与生命周期差异保留 NEOioic 的修复契约。核对证据、安全条目对应关系及尚未完成的原二进制差分验证见 [原工具功能核对](oicc-comparison.md)。不能据此宣称输出字节或所有行为完全一致。旧二进制不分发、不参与构建、自举或常规测试。
