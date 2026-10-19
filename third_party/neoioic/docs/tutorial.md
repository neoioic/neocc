# 当前开发教程

## 从最小程序开始

进入 `examples/hello_world`，按 [构建指南](build.md) 配置 GCC 并运行 `hello_world`，输出 `Hello world!`。项目中的 `objects/application/main/main.c` 放置程序入口，`host.c` 保留宿主创建/销毁模板，`host.h` 放置应用标识和行为定义。当前 `main` 只直接打印文本；`hello_world.c` 的构造区和宿主创建选择未填写，因此此示例验证构建与入口，不演示完整对象生命周期。完整创建、访问与销毁流程从单继承示例开始阅读。

核心库位于仓库 `src/`，公共头文件位于 `include/`，示例不复制核心源码。构建定义只声明当前应用及核心依赖。

## 单继承

`examples/single_inheritance/objects/general/vehicle` 定义重量，`car` 继承 vehicle 并增加最大载重、最大速度。`struct em_vehicle` 与 `struct em_car` 分别保存各节点的状态。

阅读 `car_open` 可看到 `get_base_members` 获取基节点状态，并在打开对象时初始化属性。`vehicle_interact1` 和 `car_interact1` 将属性写入调用者提供的输出缓冲区。`main` 创建、打开、查询、关闭并销毁汽车。

修改属性时要同步输出数据类型与缓冲区容量。修改扩展成员布局时还要同步 `crt_car` 的尺寸与偏移。`<crt>` 区域属于构造代码，不应放入业务逻辑。

## 多继承

`examples/multiple_inheritance` 增加 ship 与 amphicar。两栖车同时继承 car 和 ship，其节点图共享 vehicle 的重量状态。`get_base_members` 按 model_id 区分汽车和船的载重及速度；陆上与水上查询使用各自命名的行为码。

先阅读 `amphicar.c` 的构造函数，核对 AND、AIB、ABN 和扩展成员，再阅读它的打开与查询接口。运行程序核对共享重量以及两套性能参数。

## 新对象的约定

1. 在应用的 `objects/general` 中建立有明确职责的节点，定义必要的 `struct em_xxx` 状态与数据契约。
2. 在宿主头文件声明不冲突的 MSN 常量和对应 `AC0_`～`AC3_` 类别的行为码；参数 `model_id` 使用 `unsigned short`，`action` 使用 `unsigned int`。
3. 实现接口和 `tog_xxx`，明确基节点访问顺序及中性结果的继续分派规则。
4. 编写或生成 `crt_xxx`，保证尺寸、对齐、访问席位和失败清理正确，在宿主创建函数中登记。
5. 通过 `vo_*` 访问对象，并检查每个生命周期阶段的结果。

关闭若返回 `RN_CLOSEPARTIAL`，表示对象已经部分关闭，应停止业务访问，排除失败原因后重试 `vo_close`，完成关闭再销毁。打开失败也可能因清理失败返回此结果，处理方式相同。不要把保留的引用计数理解为业务状态已恢复。

预编译 `oicc` 可以辅助理解原始生成流程，但它不能自动满足当前迁移后的全部约定，使用前阅读 [工具说明](../tools/oicc/README.md)。

## Windows 界面

`examples/wgi` 展示可视对象 VObject、窗口对象 WObject、控件 CObject，以及按钮、编辑框、菜单和滚动条。业务示例 my_dialog/my_button 组合这些控件，WinMain 负责初始化 OLE、加载皮肤和消息循环。

资源通过 `skins/` 随构建复制。界面目前沿用原示例的交互实现，迁移 GCC 不表示所有控件行为已重新设计。[原 WGI 说明书](wgi_reference.pdf) 保留为历史设计参考，其版本与当前示例不同。

## 历史教材

[中文第二版](primer/zh/index.htm) 保留术语、模型解释和原始图示。其中的 1.0.6 API、VC6 截图与生成步骤是历史资料；旧下载与附录代码已移除，源码以当前 `include/`、`src/` 和 `examples/` 为准。当前 API 命名及与原始资料的对照见[代码风格](coding_style.md)。
