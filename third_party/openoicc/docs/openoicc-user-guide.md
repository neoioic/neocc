# OpenOICC 用户指南

OpenOICC 读取带有 NEOioic 标记的 C 节点文件，检查继承关系，并在指定区域生成 `crt_name` 构造函数。接口实现、成员定义和业务代码由节点作者维护。正式名称为 OpenOICC，命令、文件名和代码标识使用 `openoicc`；输入语法中的 `<oicc>` 标记保持不变。

## 构建与运行

需要 xmake 和支持 C11 的完整 GCC 或 Clang 工具链。Python 3 仅用于回归测试。NEOioic 运行时随仓库提供，不需要额外下载。

Windows 将完整 MinGW-w64 加入 PATH，然后在仓库根目录执行：

```powershell
xmake f -P . -p mingw -a x86_64 -m debug
xmake -P . openoicc
xmake run -P . openoicc --version
```

Linux 的配置命令为 `xmake f -P . --toolchain=gcc -m debug`，后续命令相同。Linux 和 Clang 是支持目标，当前实际验证平台及 Release 命令见 [构建与验证](build.md)。

构建不会自动将程序安装到 PATH。下文统一通过 `xmake run -P . openoicc` 调用，命令均从仓库根目录执行。也可以直接运行 `build/<plat>/<arch>/<mode>/openoicc`，Windows 文件名带 `.exe`。

## 分类节点与宿主目录构造

类型 `go/ao/co/ro/mo` 不区分大小写，目录参数省略时使用当前目录。例如在已有的 `build/` 中创建 MO 宿主，再添加 GO 节点：

```sh
xmake run -P . openoicc mo Demo build
xmake run -P . openoicc go boat build/mo/Demo
```

宿主位于 `build/mo/Demo`，含 `ho.c/ho.h`、Demo 节点、`se` 与 `so/go`、`so/ao`；MO 还包含 `so/ao/Main` 入口。CO、RO 的骨架没有 Main。GO、AO 位于指定宿主的 `so/类型/名称`。分类名称和 `ho/so/se/ce/re/OIOIC/NEOioic` 保留，不允许同一宿主内重名或覆盖。

在 `ho.h` 定义应用的 `OID_HO`、`OID_` 和各 `MSN_*`，并引入所需成员头文件；在 `ho.c` 的 `host_create_object` 中手工声明构造器、添加调用分支。例如 boat：

```c
/* 放在 host_create_object 函数内。 */
extern int crt_boat(uint64_t *, unsigned int, unsigned int, int,
		    struct object **, unsigned char *);
/* 放在 switch (model_id) 中。 */
case MSN_BOAT:
	result = crt_boat(&oid, input_capacity, output_capacity,
		caller_capacity, object_out, exotic_memory);
	break;
```

再运行目录构造：

```sh
xmake run -P . openoicc build/mo/Demo
```

该命令**直接更新源码**：递归读取节点注释标记，只生成宿主创建函数里直接调用的构造器，清空其他节点构造区；业务代码和宿主创建分支不变。只作为基节点使用的节点也要保留源文件和业务实现，但不必在宿主中添加独立创建分支。在宿主目录直接运行 `openoicc` 效果相同。宏、条件编译和间接构造器调用不在支持范围，完整约束见 [生成契约](openoicc.md)。编译时加入仓库 `include/`、NEOioic `include/`，并链接宿主、入口、节点与运行时源码。

## 单文件最小示例：创建、生成、编译

先创建空目录 `build/quickstart/`（PowerShell：`New-Item -ItemType Directory build/quickstart`；Linux：`mkdir -p build/quickstart`），再创建节点：

```sh
xmake run -P . openoicc new boat build/quickstart/boat.c
```

目标目录须存在，文件名须与节点名一致，`new` 不覆盖已有文件。节点名以 ASCII 字母开头，后接字母、数字或下划线；`NEOioic` 的任意大小写形式保留。默认直接基节点为 `NEOioic`。

生成的 `boat.c` 包含九个接口及完整绑定，构造区暂时为空。创建 `build/quickstart/host.h`：

```c
#ifndef QUICKSTART_HOST_H
#define QUICKSTART_HOST_H
#include "neoioic.h"
#define MSN_BOAT MSN_
#endif
```

应用负责提供模型编号和宿主契约；`new` 不自动创建这些头文件。检查、查看变更计划，再写入构造区：

```sh
xmake run -P . openoicc check build/quickstart/boat.c
xmake run -P . openoicc generate build/quickstart/boat.c
xmake run -P . openoicc generate --write build/quickstart/boat.c
```

`check` 和不带 `--write` 的 `generate` 都只报告哪些文件需要更新，不输出完整代码或差异。写入后检查 `boat.c` 的变化；再次运行生成命令应没有变更。

创建 `build/quickstart/main.c`，直接调用构造器验证这个无业务资源、禁用队列的节点：

```c
#include "host.h"
#include <stdio.h>

extern int crt_boat(uint64_t *, unsigned int, unsigned int, int,
		    struct object **, unsigned char *);

int main(void)
{
	uint64_t oid = OID_OS | UINT64_C(0x00200000);
	struct vr caller = { OID_OS | UINT64_C(0x00300000),
			     OID_OS | UINT64_C(0x00300000) };
	struct object *object = NULL;
	int result, closed;

	result = crt_boat(&oid, 0, 0, 1, &object, NULL);
	if (!RP(result))
		return 1;
	result = vo_open(object, &caller);
	if (RP(result))
		result = vo_interact0(object, ASK_SHARED, &caller);
	closed = vo_close(object, &caller);
	if (closed == RN_CLOSEPARTIAL)
		closed = vo_close(object, &caller);
	if (!RP(closed))
		return 1;
	/* 此例禁用队列，boat 无额外资源，关闭后只需释放本体。 */
	free(object);
	if (!RP(result))
		return 1;
	puts("boat lifecycle passed");
	return 0;
}
```

从仓库根目录编译，以下命令为一行：

```sh
gcc -std=c11 -Wall -Wextra -Iinclude -Ithird_party/neoioic/include -Ibuild/quickstart build/quickstart/main.c build/quickstart/boat.c third_party/neoioic/src/object.c third_party/neoioic/src/neoioic.c -o build/quickstart/boat-demo
```

Windows 运行 `./build/quickstart/boat-demo.exe`，Linux 运行 `./build/quickstart/boat-demo`，预期输出 `boat lifecycle passed`。正式应用需由宿主管理 OID 分配回收、创建选择及资源销毁，可参考 [仓库宿主](../src/host/host.c) 和 [继承示例](../examples/oioic_reference/README.md)。启用队列或业务资源后，需先清理相应资源，不能仅释放本体。

## 继承与自有成员

直接基节点列表作为一个逗号分隔参数传入，声明顺序决定分派顺序。在已存在的目标目录创建菱形继承：

```sh
xmake run -P . openoicc new vehicle build/quickstart/vehicle.c
xmake run -P . openoicc new car build/quickstart/car.c vehicle
xmake run -P . openoicc new ship build/quickstart/ship.c vehicle
xmake run -P . openoicc new amphibian build/quickstart/amphibian.c "car, ship"
xmake run -P . openoicc generate --write build/quickstart/vehicle.c build/quickstart/car.c build/quickstart/ship.c build/quickstart/amphibian.c
```

使用显式文件 `check/generate` 时，所有非根祖先的 `.c` 都须列出，包括间接祖先；这两个命令不自动搜索目录。编译前还需为各节点定义非零、唯一且不超过 `USHRT_MAX` 的 `MSN_*` 编号。共享祖先各只有一份；席位容量及单线程委托边界见 [生成契约](openoicc.md)。

如需 boat 的自有成员，可在 `boat.c` 构造区外的文件作用域定义：

```c
struct em_boat {
	unsigned int capacity;
};
```

也可放在同目录 `boat.h` 中，并通过应用头文件让所有相关构造器看到完整定义。工具只扫描节点 `.c` 和同名 `.h`，不遍历 include 树。成员头文件支持无保护或一个常规外层 `#ifndef` 守卫。使用直接的结构定义；宏生成、属性修饰、局部结构和任意条件编译不在支持范围。类型和布局仍须编译及运行验证。

## 标记与更新规则

保留 `new` 生成的九接口绑定和以下注释结构，业务代码放在构造区外：

```c
/* <oicc> */
/* <ibn> car, ship </ibn> */
/* <crt> */
/* 此处由工具维护。 */
/* </crt> */
/* </oicc> */
```

每个文件恰有一个完整区域及一个 `void tog_name(struct object *)` 定义。标记只在 C 注释中识别，字符串中的同名文本忽略。两个构造区边界须为文件作用域的独立块注释。重复、错序、未闭合标记及不支持的声明会报错。

全部输入先完成解析、继承校验和生成，再准备同目录独占临时文件、核对原文件并逐一替换。区外字节保留不变，生成区遵循输入的 LF/CRLF；仓库文本和新文件统一使用 UTF-8、LF。多文件写入不构成事务：校验或准备失败不提交，提交阶段失败时已报告 `committed` 的文件保留更新。修复原因后重新生成即可继续。

## 接入应用与常见问题

生成代码依赖 `openoicc/construct.h`、当前 NEOioic 头文件和运行时。NEOioic 由 OIOIC 发展而来，未来可能有不兼容更新；请使用与生成器配套的运行时快照，升级时重新核对 [生成契约](openoicc.md) 并验证生成代码。分发生成代码时需同时提供该辅助头文件和运行时，并保留 MIT 许可。应用还需链接所有祖先节点的业务实现，提供相应的模型编号和成员定义。

对象生命周期为构造 → `vo_open` → `vo_*` 业务调用 → `vo_close` → 宿主清理和销毁。用 `RN/RO/RP` 判断结果域；调用上下文 `cr`、`mr` 均须非零。`RN_CLOSEPARTIAL` 状态仅允许重试关闭。外部存储由调用者保证容量和对齐，并由存储拥有者释放本体；不得使用会释放本体的 `destroy_object` 宏。

| 现象 | 处理方式 |
| --- | --- |
| `unknown base node` | 补充该基节点及其全部祖先的 `.c` 参数；根 NEOioic 无须文件。 |
| 创建失败 | 检查目录、写权限、节点名与文件名；已有文件不会覆盖。路径含空格时整体加引号。 |
| 标记或绑定错误 | 检查注释顺序、作用域及唯一的 `tog_name` 定义，保留完整九接口绑定。 |
| 缺少 `host.h`、`MSN_*` 或成员类型 | 提供应用头文件，配置 include 路径，使完整定义可见。 |
| 链接缺少 `tog_*` / `vo_*` | 链接全部祖先节点实现及 NEOioic 运行时。 |
| 构造返回失败 | 检查模型编号、正数 caller_capacity、席位上限、对齐、OID 余量和内存分配。 |
| 写入失败 | 检查权限、文件占用与并发编辑；查看已提交文件记录，排除原因后重试。 |

退出码为 0（成功）、1（输入、生成或文件操作失败）和 2（命令用法错误）。源码校验诊断通常给出文件、行、列及原因。只处理普通文件，拒绝文件符号链接和 Windows reparse point。Windows 当前使用窄字符路径接口，不能保证任意 Unicode 路径。

OpenOICC 不执行完整 C 预处理，不替代 C 编译器，也不提供历史 oicc API 兼容层。分类创建和目录构造已依据原工具帮助、创建结果与教程核对；安全修复采用当前 NEOioic 契约。原程序不随项目分发，构造行为回归以仓库参考构造器和运行时为准，原二进制差分验证的限制见 [核对记录](oicc-comparison.md)。详细边界、自举再生和测试方式见 [生成契约](openoicc.md)、[测试说明](../tests/README.md)。
