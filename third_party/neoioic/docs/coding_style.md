# C 代码风格

活跃源码采用 Linux 内核常用的 C 代码风格。项目仍是独立的用户态 C11 库和示例，构建、对象模型、接口分派与算法保持原有设计。

## 排版

- 使用 Tab 缩进，每个 Tab 为 8 列；通常在 80 列内换行。
- 函数的左大括号独占一行；`if`、`for`、`while`、`switch` 的左大括号跟随条件。
- `case` 与 `switch` 对齐；指针星号靠近变量名，运算符两侧留空格。
- 不把多条语句挤在一行；多行表达式按语义分组换行。
- 文件采用 UTF-8、LF，保留末尾换行。

仓库根目录的 `.clang-format` 与 `.editorconfig` 固定以上规则。可使用 clang-format 14 或更新版本整理修改过的 C 文件；不同版本可能在个别复杂表达式上产生排版差异。

## 命名

文件和目录采用小写下划线命名，扩展名使用小写；README.md、AGENTS.md、LICENSE 及工具规定的配置文件名保留惯例。示例按 `host.c`（宿主）、`objects/application/`（应用对象）、`objects/general/`（通用对象）和 `support/`（辅助功能）组织。项目正式名称为 NEOioic，代码名称与构建目标使用 `neoioic`，宏前缀使用 `NEOIOIC_`。历史教材、截图和上游二进制保留原始名称与旧 API。

自有函数、变量、结构体标签及成员采用小写下划线命名；宏常量、头文件保护宏及枚举常量使用大写下划线。模拟函数调用的宏沿用小写函数风格。Win32/OLE 类型、API、外部结构体成员和 `WinMain` 保留系统规定的名称。

直接使用 `struct object`、`struct queue`、`struct vr` 和节点的 `struct em_xxx`，不使用隐藏结构体的 typedef。标量直接使用原来别名对应的 C 类型；64 位标识使用 `uint64_t`。这种写法不改变成员顺序、宽度、布局或接口含义。

| 原名称 | 当前名称 |
| --- | --- |
| `OBJECT`、`QUEUE`、`VR` | `struct object`、`struct queue`、`struct vr` |
| `VO_Open`、`VO_Close` | `vo_open`、`vo_close` |
| `CreateObject`、`DestroyObject` | `create_object`、`destroy_object` |
| `__CreateObject__`、`__DestroyObject__` | `host_create_object`、`host_destroy_object` |
| `GetBN`、`GetEMofBN` | `get_base_node`、`get_base_members` |
| `InitAIBofND`、`InitABNofND` | `init_direct_bases`、`init_base_nodes` |
| `CallerCome`、`CallerLeave` | `register_caller`、`unregister_caller` |
| `CRT_car`、`TOG_car` | `crt_car`、`tog_car` |
| `This`、`pND`、`pCaller` | `self`、`node`、`caller_context` |

表中旧名称用于说明迁移关系；当前代码不提供兼容别名。AND、AIB、ABN、ACS、MSN、OID 等仍是架构概念，其对应代码成员以当前公共头文件为准。

## 注释与资料

注释优先说明约束、参数契约、资源所有权和实现原因。删除空白的需求编号、测试记录及维护历史模板，保留作者署名、版权声明、有效接口说明和 `<ibn>`、`<crt>` 构造标记。

当前 API 见公共头文件、[架构说明](architecture.md)和[开发教程](tutorial.md)。历史 HTML 教材及 WGI PDF 保留原始名称和截图，仅供设计背景参考。旧 oicc 工具生成的名称、类型和构造代码需要按当前规范人工审查与迁移。
