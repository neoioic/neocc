# NEOioic 对象模型与生成契约

本文件说明 OpenOICC 面向的 [NEOioic](https://github.com/neoioic/neoioic) 对象模型与生成契约，供开发者在本仓库内阅读。来源及 MIT 许可见 [第三方声明](../THIRD_PARTY_NOTICES.md)。接口行为以 [公共头文件](../third_party/neoioic/include/object.h) 和仓库内运行时源码为准；历史 OIOIC 资料不构成兼容承诺。

## 模块边界与节点

运行时管理对象结构、接口分派、继承节点查询、访问登记及字节队列；应用宿主管理对象创建选择、OID 分配回收和应用入口。`struct object` 是节点，`model_id` 是对象模编号，`oid` 是对象标识，`members` 指向节点自有的 `struct em_xxx` 状态。

| 原模型术语 | 当前成员 | 含义 |
| --- | --- | --- |
| AND | `nodes` | 对象图的节点数组，最终派生节点位于首项 |
| AIB | `direct_bases` | 有序直接基节点数组，决定基节点访问顺序 |
| ABN | `bases` | 全部基节点数组，不包含自身 |
| ACS | `callers` | `struct vr` 访问记录席位 |

NEOioic 是唯一根节点，成员 `struct em_neoioic` 保存输入/输出队列和模式位。`get_base_node`、`get_base_members` 只查询 ABN；自身状态用 `self->members`。根节点自身不能通过基节点查询查到自己。

## 继承顺序与席位

[参考构造器](../examples/oioic_reference/so/go/amphicar/amphicar.c) 中，amphicar 的直接基节点顺序是 car、ship，节点数组顺序是 amphicar、car、vehicle、NEOioic、ship。这是按声明顺序先访问节点自身、再递归基节点、首次出现时加入数组的去重遍历。为每个节点建立有序祖先集合，不能将共享祖先按分支重复分配，也不能重新排序直接基节点。

ACS 为同一调用者的不同委托者提供记录。摘选构造器为一次调用配置如下席位倍数：

| 节点 | 席位倍数 |
| --- | --- |
| amphicar | 1 |
| car | 2 |
| vehicle | 3 |
| NEOioic | 4 |
| ship | 2 |

仓库未收录旧生成器及其席位算法源码。当前 OpenOICC 使用从构造目标到节点的最长继承路径长度加一作为席位倍数，并结合 `register_caller`、委托访问和真实生成代码行为验证。不能把节点去重、祖先数量或路径数量直接等同于席位算法；该规则的跨分支委托、循环重入和并发限制见 [生成契约](openoicc.md)。

摘选 amphicar 构造器的节点数为 5、ABN 数量总和为 9、AIB 数量总和为 5、ACS 倍数总和为 12、最大节点倍数为 4。这些是参考实例数据，生成器必须从任意受支持继承图计算，不硬编码示例常数。

当前 openoicc 初版使用从构造目标到节点的最长继承路径长度加一作为席位倍数。在沿继承方向逐层委托的单线程模型下，该规则覆盖同时存在的外部上下文与委托链；普通继承遍历沿用上下文，委托 VO 返回清除自己的祖先上下文。`tests/delegation.c` 在生成的菱形对象上实际占满上述 1、2、3、4、2 个席位并检查释放。该规则不保证任意跨分支委托、循环重入或并发容量，完整边界见 [生成契约](openoicc.md)。

## 构造布局与所有权

`crt_xxx` 构造节点图并布置内存，`tog_xxx` 绑定九个函数指针。构造区保存节点数组、基节点指针、访问席位和扩展成员；队列缓冲单独分配。所有节点共享一个构造区，不能独立释放基节点。

尺寸和偏移使用 `size_t`，成员块使用 `NEOIOIC_ALIGNED_SIZE(struct em_xxx)`。当前构造辅助实现检查尺寸加乘溢出和字段窄化，并拒绝超过 `struct object` 对齐要求的成员类型。

`caller_capacity` 必须为正，按每个节点最大席位倍数检查 `USHRT_MAX` 边界，再转为 `size_t` 计算总量。外部存储构造会清零整个构造区，调用者必须保证空间容量、对齐和有效期；当前构造接口没有外部容量参数。

动态对象通过 `create_object` 创建，`destroy_object` 宏仅在宿主销毁成功后释放本体并置空指针。外部存储对象仅用 `host_destroy_object` 清理队列和标识，由存储拥有者管理本体。宿主销毁不代替 `vo_close`，也不自动验证对象是否仍打开。

OID 是 `uint64_t`，高 16 位为 OS、中 32 位为宿主、低 16 位为节点。耗尽后的回滚和资源清理由宿主处理。分配失败时释放本次已取得的资源，不把部分构造区作为有效对象发布。

## 接口、访问和结果

五类统一接口展开为 open、input、output、io_put、interact0、interact1、interact2、interact3、close。interact0 无流，interact1 仅输出，interact2 仅输入，interact3 输入输出；应用行为从对应 `AC0_`～`AC3_` 扩展，明确数据尺寸与输出数量。

业务通过 `vo_*` 调用，直接调用接口指针会绕过访问登记。`struct vr` 中 `cr` 为调用者、`mr` 为委托者，两者非零；以 `(cr, mr)` 去重。转委托保留 `cr` 并按职责切换 `mr`。VO 结束后清除当前节点及祖先访问记录。

OBS 先基节点后自身，SBO 先自身后基节点；专用中性结果表示继续分派，不能改成普通成功/失败判断。接力 SBO 单继承传递基节点结果，多继承返回 `RO__SBOOIX`。当前 `INLINE_VOFUNCTION` 为 0，使用 `src/object.c` 的 VO；改动时还须核对头文件备用内联实现。

统一返回 `int`，用 `RN/RO/RP` 判断否定、中性、肯定结果，不使用旧 `IRESULT`、`FRESULT` 别名。调用者记录、引用计数、队列和宿主 OID 不自动提供线程同步。

## 生命周期与队列

创建、打开、关闭、销毁是不同阶段。`RN_CLOSEPARTIAL` 表示部分资源已释放，引用仅用于重试关闭，禁止继续业务访问、再次打开或直接销毁。打开失败后的清理也可能返回此结果。当前没有独立状态位自动拦截非法继续访问，应用必须遵守协议。

根节点关闭归零返回 `R_P`，派生示例通常返回 `RP_RCZERO`，不能要求所有节点的肯定结果相同。必要业务和参数校验不能依赖 Release 中禁用的断言。

空队列 `front/rear` 为 NULL；非空指向数组内首尾字节。禁止 `data - 1` 哨兵。容量零禁用队列；扩容失败保留原内容。容量命令通过 `memcpy` 读写本机 `unsigned int`，检查字节流长度，不假设整数对齐。

`enqueue` 或 `dequeue` 返回 `RN_LOST` 时仍可能处理部分数据，输出以 `output_quantity` 为准。空队列出列返回 `RO_VAINLY`。模式位为零表示共享和阻断，不能按名称直觉反转位语义。

## 生成标记与当前命名

`<ibn>` 标记声明有序直接基节点，`<crt>` 区域保存生成的构造函数。生成器只能替换该区域，业务代码保留在区外。无自有成员的节点可没有 `struct em_xxx`，不能凭名字伪造成员类型。

| 历史名称 | 当前名称 |
| --- | --- |
| OBJECT、QUEUE、VR | `struct object`、`struct queue`、`struct vr` |
| CRT_xxx、TOG_xxx | `crt_xxx`、`tog_xxx` |
| VO_Open、VO_Close | `vo_open`、`vo_close` |
| CreateObject、DestroyObject | `create_object`、`destroy_object` |
| GetBN、GetEMofBN | `get_base_node`、`get_base_members` |
| InitAIBofND、InitABNofND | `init_direct_bases`、`init_base_nodes` |

旧工具不能自动满足当前命名、对齐、席位边界、外部存储初始化及失败清理约束。OpenOICC 直接生成当前规范，不提供历史 API 兼容层。分类创建、标记与宿主选择职责的核对，以及安全报告覆盖的契约取舍，见 [原工具功能核对](oicc-comparison.md)。历史教程和预编译工具不参与构建或自举。
