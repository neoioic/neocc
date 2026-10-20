# 下一阶段交接提示词

请先阅读 `AGENTS.md`、`docs/handoff.md`、`docs/architecture.md` 和相关源码、测试，
重新检查 Git 工作区，保留开发者已有修改。仅使用本仓库的 NEOioic、OpenOICC 快照，
不访问外部项目，不执行 Git 暂存、提交或推送。

继续 NeoCC 对 TinyCC 的职责迁移，每次只完成一个可独立审查、验证的阶段。
当前 source、tokens、preprocessor、symbols 及 expressions 已分别拥有所述资源；
表达式值栈、栈顶和四种常用类型已迁移，不能重复创建全局状态或兼容层。
从 `tccgen.c` 的作用域结构、清理/goto 链或函数状态中选择下一职责，
保留既有算法、直接代码生成和目标 ABI，不把剩余状态全部塞入一个对象。
具体边界、清理顺序和待办以交接记录及实际源码为准。

完成实现、删除被替代路径、补充必要回归并同步受影响文档。
串行运行 `python tests/verify.py --mode debug` 和
`python tests/verify.py --mode release`，修复失败并复验。
完成一个阶段后停下，中文说明改动、实际验证和剩余边界，给出一句简短中文提交信息，
由开发者提交备份后再继续下一阶段。
