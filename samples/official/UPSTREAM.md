# 上游来源与许可

前端资源来自官方 Ultralight SDK **2.0.0-beta.2 / macOS ARM64 / free** 分发包的 samples：

- `02-basic-app/assets`
- `05-javascript-bridge/assets`
- `07-file-loading/assets`
- `13-game-ui/assets`
- `11-multi-window/assets`

除下述修改外，这些资源按原样复制。C++ 与构建文件为本项目适配实现。

`11-multi-window/assets/editor.html` 保留初始示例文档，使用原生 textarea 替换 Monaco，
通过 input 事件与 200 ms 防抖更新预览。Monaco 分发文件已移除。
上游分发包的许可文本保存在 [licenses](licenses/)；使用及分发 SDK 仍须遵守其许可条件。
现有资源附带的第三方许可保留在原目录。
此记录不授予额外的资源或 SDK 使用权。
