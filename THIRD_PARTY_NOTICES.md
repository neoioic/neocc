# 第三方材料声明

| 材料 | 范围与许可依据 |
| --- | --- |
| NEOioic | `third_party/neoioic/`，MIT，见其 `LICENSE` 和源码声明 |
| OpenOICC | `third_party/openoicc/`，MIT，见其 `LICENSE` 和源码声明；上游第三方说明只作来源背景，实际导入范围以 NeoCC 清单为准 |
| TinyCC | 开发者已有的 `tinycc/` 基线，见 `tinycc/COPYING` 和各文件声明；核心声明 LGPL 2 或更新版本，附带 LGPL 2.1 文本，支持文件以自身声明为准 |
| TinyCC 衍生实现与运行支持 | `src/main.c`、`src/compiler/`、`src/objects/source.c`、`src/objects/tokens.c`、`src/objects/preprocessor.c`、对应 `include/neocc/` 契约、`runtime/`；保留对应原文件声明。来源范围及 SHA-256 见 `docs/tinycc_sources.json`，适用许可仍以 `tinycc/COPYING` 及各文件为准；运行库存在附加链接例外和公共领域等不同声明 |

保留材料与衍生内容所需版权和许可。根格式配置摘自 OpenOICC。NeoCC 项目级许可尚未确定，本说明不把 TinyCC 衍生代码重新许可为 MIT。

本次摘选提交、原始及导入后校验值见各 `NEOCC_SOURCE.json`。不包含旧预编译 oicc，不依赖外部开发工作区。

迁移中的本地调整：OpenOICC 的 Windows 目录枚举使用原生 Win32 接口以支持 NeoCC
链接，不再依赖 MinGW dirent 实现；Windows 临时文件通过 `_open` 排他创建以支持
旧 MSVCRT，其 MIT 声明保留。NeoCC 的 kernel32 导入声明
增加 `GetProcessHandleCount` 供句柄泄漏回归使用。NEOioic 运行时源码保持固定快照。
