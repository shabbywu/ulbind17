# Ultralight SDK 示例

这些示例基于 Ultralight 2.0.0-beta.2，使用 ulbind17 的 Runtime、ManagedView 和 Bridge。
页面、图片和字体来自 SDK 分发包；资源来源及修改记录见下文。
来源和许可见 [UPSTREAM.md](UPSTREAM.md)。

| 样例 | Target | 验证内容 |
| --- | --- | --- |
| 02 Basic App | `Official02BasicApp` | AppCore 窗口与 ManagedView 托管 |
| 05 JavaScript Bridge | `Official05JavaScriptBridge` | 参数转换、原生调用、tick 回调、导航后重新绑定 |
| 07 File Loading | `Official07FileLoading` | 本地 HTML、背景图片、日期和时钟 |
| 13 Game UI | `Official13GameUI` | 选择页和六个游戏场景、动画、窗口展示 |
| 11 Multi Window | `Official11MultiWindow` | HTML 编辑器通过 Bridge 实时更新另一窗口 |

## macOS ARM64 构建与运行

```sh
cmake -S . -B build/appcore \
  -DULTRALIGHT_SDK_VERSION=2.0.0-beta.2 \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DULBIND17_ENABLE_APPCORE=ON \
  -DULBIND17_BUILD_OFFICIAL_SAMPLES=ON \
  -DULBIND17_BUILD_GAME_UI_HOST_SAMPLE=ON
cmake --build build/appcore -j
./build/appcore/samples/official/02-basic-app/Official02BasicApp.app/Contents/MacOS/Official02BasicApp
ctest --test-dir build/appcore -R 'Official|GameUIHostAdapter' --output-on-failure
```

其他桌面样例的可执行文件位于各自目录中的同名 `.app/Contents/MacOS/` 下。
支持 `--smoke-test` 自动验证并关闭、`--cpu` 使用 CPU 展示；13 还支持 `--scene NAME`。
所有桌面样例只由 `Application::RunOnce()` 驱动，窗口关闭触发统一关闭流程。
05 和 11 使用独立 API 注册表，在加载前 AttachTo ManagedView，导航后由 SDK 自动重新注入。
其他文档级 Bridge 操作继续使用 WithBridge，保留句柄在导航后按现有规则失效。

样例各自复制动态库、页面与 SDK 数据资源，避免不同样例的 `assets/app.html` 冲突。
构建时会重新复制资源，修改 HTML 后再次构建即可。桌面测试需要原生桌面会话。
真实跨屏 DPI、窗口拖动和中文 IME 可在已有 `AppCoreExample` 中交互验证。

## Game UI 离屏示例

`GameUIHostAdapter` 只链接核心 target，不依赖 AppCore。可单独启用：

```sh
cmake -S . -B build/host \
  -DULBIND17_ENABLE_APPCORE=OFF \
  -DULBIND17_BUILD_GAME_UI_HOST_SAMPLE=ON
cmake --build build/host --target GameUIHostAdapter -j
cd build/host/samples/official/13-game-ui-host
./GameUIHostAdapter --scene flight-hud --output frame.ppm
```

场景名称：`flight-hud`、`overdrive`、`lightfingers`、`ashen-reliquary`、`wind-and-ink`、`tidepool`。
省略 `--scene` 渲染选择页；`--assets DIR` 指定页面根目录。
程序驱动 Update → RefreshDisplay → Render，等待页面与动画就绪，复制 800×450 CPU 帧后退出。
`--output` 导出 RGB PPM；内部 CopyPixels 返回有独立所有权的 BGRA8 预乘 alpha 数据，读取时使用 row_bytes。
场景 smoke 测试还发送 Escape 返回选择页，并检查导航后旧 Bridge 句柄失效。

接入持续运行的宿主循环时，在每帧驱动 Runtime，并将读取的像素上传至宿主纹理。
宿主负责输入转发、View 尺寸、坐标/DPI、焦点和线程约束；纹理上传须匹配 BGRA8 预乘 alpha 格式。
GPU 渲染需要宿主提供 GPUDriver 并处理 render target。
自定义 FileSystem 必须在创建 Renderer 前注册，并持续存活到 Renderer 释放。

11 使用原生 textarea 编辑 HTML，无需额外编辑器依赖。保留初始预览和 200 ms 输入防抖；不提供语法高亮或代码补全。
