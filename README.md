# ulbind17

为 **Ultralight 2.x / C++20** 提供仅含头文件的便捷接口，以及内嵌资源的平台服务。
项目保留 ulbind17 名称，但不再支持 C++17 和旧的 JavaScriptCore 封装 API。
首个完成验证的 SDK 版本为 **2.0.0-beta.2 Free**。

## 构建

支持 Windows x64、Linux x64 和 macOS arm64。需要支持 C++20 的编译器
（VS 2022+、GCC 11+ 或兼容的 Clang）和 CMake 3.22+。

使用已解压的 SDK 离线构建：

```sh
cmake -S . -B build/local -DULTRALIGHT_SDK_ROOT=/path/to/ultralight-sdk
cmake --build build/local --parallel 2
ctest --test-dir build/local --output-on-failure
```

也可以通过 `CMAKE_PREFIX_PATH` 或 `Ultralight_DIR` 指定 SDK 的 CMake 包。
没有找到本地 SDK 时，配置阶段会通过
[官方下载 API](https://ultralig.ht/docs/2.0/getting-the-sdk) 下载 Free SDK，校验 SHA-256，
并将压缩包和解压结果缓存在构建目录中。
`ULTRALIGHT_SDK_VERSION` 默认为 `latest`；设置为 `2.0.0-beta.2` 可固定版本。
自动下载模式下，每次配置都会重新查询版本信息；选择 `latest` 时会获取当前最新版，
匹配的缓存压缩包会被复用。离线配置需要显式指定本地 SDK 路径。
旧版 SDK 或不支持的架构会导致配置失败，不会回退到 1.x。

配置选项：

| 选项 | 默认值 | 用途 |
| --- | --- | --- |
| `ULTRALIGHT_SDK_ROOT` | 空 | 已解压的 SDK 路径；优先于包查找和自动下载 |
| `ULTRALIGHT_SDK_VERSION` | `latest` | 自动下载的 SDK 版本 |
| `BUILD_TESTING` | 作为顶层项目时为 ON | 构建控制台示例和回归测试 |
| `ENABLE_TEST` | OFF | 构建全部五个示例 |
| `ULBIND17_BUILD_RENDERING_SAMPLE` | `ENABLE_TEST` 的值 | 单独控制 GLFW/GLAD 渲染示例的构建 |
| `ULBIND17_ALLOW_UNAVAILABLE_GRAPHICS` | OFF | 图形环境不可用时，允许 CTest 将渲染冒烟测试标记为跳过 |

控制台测试不依赖 GLFW 或 GLAD。构建 Sample5 时，可以设置 `VCPKG_ROOT` 后使用项目的
vcpkg 预设，也可以自行提供已安装的 GLFW/GLAD CMake 包：

```sh
cmake --preset default -DENABLE_TEST=ON -DULTRALIGHT_SDK_ROOT=/path/to/ultralight-sdk
cmake --build --preset default
ctest --test-dir build/Darwin -C Release --output-on-failure
```

预设构建目录使用宿主系统名称（`Darwin`、`Linux` 或 `Windows`）。
Sample5 提供带超时限制的 `--smoke-test`，通过隐藏窗口检查 OpenGL 像素颜色，
并验证鼠标点击能触发 C++ 回调。运行它需要桌面会话；Linux 上可以使用
`xvfb-run -a ctest --test-dir build/Linux --output-on-failure` 运行包含图形示例的测试。
macOS CI 启用 `ULBIND17_ALLOW_UNAVAILABLE_GRAPHICS`：若 GLFW 无法初始化图形环境或创建
OpenGL 上下文，测试输出具体错误并以退出码 77 标记为 `Skipped`。默认本地测试仍要求图形环境可用；
也可显式运行 `Sample5 --smoke-test --allow-unavailable-graphics` 使用相同行为。
上下文创建后的加载器、像素检查和点击回调错误仍会导致测试失败。Linux CI 在 Xvfb 下执行实际渲染检查。
Windows CI 编译 Sample5，并在运行 CTest 时排除 `Sample5.smoke`；本地仍可手动运行该测试。

## 在其他 CMake 项目中使用

```cmake
add_subdirectory(path/to/ulbind17 ulbind17-build)
target_link_libraries(my_app PRIVATE ulbind17::ulbind17)
ulbind17_copy_runtime_files(my_app)
```

`ulbind17::header` 提供绑定和平台服务头文件，以及 SDK 链接依赖；
`ulbind17::ulbind17` 还会链接内嵌资源。两个目标都会向使用方传递 C++20 要求。
现有的 `ultralight-sdk` 和 `ultralight-sdk-fullstack` 目标分别封装
`Ultralight::Ultralight` 和 `Ultralight::AppCore`。
`ulbind17_copy_runtime_files()` 将动态库复制到可执行文件旁，并设置 rpath；
使用内嵌资源时，不需要再复制外部 ICU 数据或证书包。
自行提供文件系统和资源的应用也可以使用 SDK 的 `ultralight_copy_runtime_files()` 辅助函数。

Python 扩展、Godot 插件等共享库或模块库也可以调用相同的复制函数：

```cmake
target_link_libraries(my_extension PRIVATE ulbind17::ulbind17)
ulbind17_copy_runtime_files(my_extension INCLUDE_APPCORE OFF)
```

`INCLUDE_APPCORE` 默认 ON，保留原调用行为；不使用 AppCore 的宿主可关闭它。
macOS 的库从 `@loader_path/` 查找同目录依赖，Linux 使用 `$ORIGIN`。
复制到可执行文件、SHARED_LIBRARY 或 MODULE_LIBRARY 目标的输出目录；该函数不负责安装或 wheel 打包。

## 通用宿主运行时

`<ulbind17/runtime.hpp>` 提供可选的 `Runtime`、`ManagedView` 和 CPU 像素接口。
实现仍为头文件，不依赖 Python、Godot、窗口库或图片编码器。

```cpp
#include <ulbind17/setup.hpp>
#include <ulbind17/runtime.hpp>

ulbind17::setup_ultralight_platform();
auto runtime = ulbind17::Runtime::Create();
ultralight::ViewConfig config;
config.is_accelerated = false;
auto view = runtime.CreateView(800, 450, config);

ulbind17::ViewCallbacks callbacks;
callbacks.window_ready = [&](const ulbind17::FrameEvent &event) {
    if (!event.is_main_frame) return;
    view.WithBridge([](ulbind17::jsc::Bridge &bridge) {
        bridge.bindFunc("echo", [](JSContextRef context, JSObjectRef,
                                  std::span<const JSValueRef> args) {
            return args.empty() ? JSValueMakeUndefined(context) : args[0];
        });
    });
};
callbacks.dom_ready = [](const ulbind17::FrameEvent &event) {
    // 在主文档就绪后执行依赖 DOM 的操作。
};
view.SetCallbacks(std::move(callbacks));
view.LoadHTML("<script>console.log(echo('hello'))</script>");

// 由宿主循环重复调用；Runtime 不运行后台循环。
runtime.Update();
runtime.RefreshDisplay(0);
runtime.Render();

auto pixels = view.CopyPixels();
view.ClearDirtyBounds(); // 成功消费当前帧后，再确认脏区域。
runtime.Close();
```

完整、带超时的宿主适配示例见 [samples/host_adapter.cpp](samples/host_adapter.cpp)，
构建后运行 `HostAdapter`，或执行 `ctest -R HostAdapter`。

宿主已有 Renderer 时，在它的创建线程调用 `Runtime::FromExisting(renderer)`。
它持有引用，不重设平台服务。文件系统、字体、日志、SurfaceFactory 和 GPUDriver
仍由宿主通过 SDK Platform 配置，并保持存活至 Renderer 的最后一个引用释放。
独立宿主使用 `Create()`，此前必须注册 FileSystem 和 FontLoader。一个程序只创建一个 Renderer：
同一二进制内重复 `Create()` 返回共享运行时；关闭后不会重新创建 Renderer。
不同扩展模块的头文件状态不构成进程级初始化协调，已有宿主应显式传入同一个 Renderer。

### 生命周期、回调与线程

- Runtime 和 ManagedView 是共享状态的可复制句柄。最后一个 Runtime 句柄释放时，仍存活的
  ManagedView 会继续持有运行时；显式 `Runtime::Close()` 关闭所有受管理页面。
- `ManagedView::Close()` 和 `Runtime::Close()` 可重复调用。页面先停止回调，再解除文档 Bridge、
  取消 Promise 观察、清理宿主回调并释放引用。宿主另行持有的 SDK Renderer 引用不会被强制销毁。
- 主框架开始导航或报告新窗口对象时，使旧 Bridge 失效。子框架事件携带 frame ID 和
  `is_main_frame`，不会替换主框架 Bridge。页面句柄不可跨导航使用。
- `SetCallbacks()` 替换整组回调。回调中允许关闭页面、发起导航或替换回调；渲染器的 Update、
  RefreshDisplay 和 Render 不允许嵌套。加载结束不代表成功，应单独处理 `fail_loading`。
- ManagedView 独占 SDK 的 LoadListener 和 ViewListener 槽位。`native()` 返回借用指针，
  用于输入、Session、GPU render target 等 SDK 扩展操作；不要替换受管理的两类监听器。
  其他宿主监听器仍由宿主管理。
- `WithBridge()` 在 JS 上下文锁有效期间调用回调。返回宿主值或 `Bridge::Handle`，不要返回借用的
  JSC 指针或 Bridge 引用。原生绑定不自动跨导航重放；在每次主框架 `window_ready` 中重新注册。
- View/Runtime 操作、Close 和最终析构在 Renderer 创建线程执行。跨线程使用 `PostTask()`；
  它只在后续 Update 中执行，Runtime 关闭后抑制尚未执行的任务。任务捕获对象须能安全析构。
  同一 Bridge::Handle 的析构可在工作线程执行，但读取它仍须回到 Renderer 线程。
- 回调不要按值捕获自己所在的 ManagedView/Runtime，以免形成引用环；使用宿主持有的对象、弱引用，
  或保证在宿主销毁前显式 Close。线程终结器须在最后一次 Update 停止前安排 Close。

`SetErrorHandler()` 接收页面事件和已投递任务的宿主异常；未设置时发送到 SDK Logger。
错误处理器自身抛出的异常也会被隔离。`ulbind17::Error` 继承 `std::runtime_error`，
提供 `ErrorCode`、消息以及可取得的 JS stack/source_url/line/column，诊断字符串独立于文档寿命。
原有类型化接口继续返回 `js::Result`，原有 JSC 验证接口保留异常类别。

### CPU 像素

`WithPixels(callback)` 提供只在回调期间有效的 `PixelView`；`CopyPixels()` 返回独立拥有内存的
`PixelBuffer`，后者可跨线程使用或在 View 关闭后保留。
两者提供 width、height、row_bytes、dirty_bounds、format 和 data。
原生格式是 **BGRA8、预乘 alpha**；row_bytes 可能包含填充，不保证等于 width × 4。
透明像素须按该格式消费，外部项目自行实现 RGBA/非预乘转换和图片编码。

使用任何自定义 Surface 都不需要强制转换成 BitmapSurface。读取不清除脏区域，
上传成功后调用 `ClearDirtyBounds()`。像素锁期间拒绝驱动 Renderer、导航、Resize、再次读取和 Close；
回调抛异常也会解锁。不要从渲染器回调中读取像素，或通过 `native()` 绕过锁限制。
GPU View 没有 CPU Surface，读取时报 `ErrorCode::Unsupported`，宿主直接使用其 render target。

## 动态 JavaScriptCore 桥接

`<ulbind17/jsc/Bridge.hpp>` 面向运行时类型的宿主适配器；Godot Variant、Python 对象和引用计数
由各自的外部项目转换。JSC 的原始句柄与类型化 SDK 的 `js::Value::raw()` 是不同类型，不能混用。
手工创建 Bridge 时须在 View 的 JS 上下文锁内调用 `Bridge::create(lock->ctx(), view)`，
并在导航和销毁时主动 `invalidate()`；ManagedView 自动处理这一步。

| 能力 | 接口 |
| --- | --- |
| 属性与索引 | `get`、`set`、`has`、`at`、`setAt` |
| 数组与对象 | `length`、`keys`、`makeArray`、`makePlainObject` |
| 基础值 | `makeBoolean`、`makeNumber`、`makeString`、`makeNull`、`makeUndefined` |
| 严格读取 | `readBoolean`、`readNumber`、`readString` |
| 带所有权操作 | `evaluateRetained`、`getRetained`、`callRetained`、`retain` |
| 动态宿主对象 | `makeObject(payload, ObjectHooks)`、`unwrap<T>` |
| Promise | `then(promise, settled, cancelled)` |

`keys()` 只返回自身可枚举的字符串键，不读取属性值；`has()` 也检查继承属性。
数组空洞读取为 undefined，length 包含空洞。严格读取不会将 `"7"` 转为数字。
`callRetained()` 检查函数、参数及 receiver 的 Bridge 归属，并保留 receiver 的 `this`。
句柄属于某个 Bridge，不能直接在另一 Bridge 中使用，即使两个 Bridge 对应同一个页面。

原始值是借用值，须属于当前文档；在后续分配或执行脚本前，使用 `Root`/`retain()` 保持它们存活。
JSC 原始指针不携带来源元数据，原始接口无法替宿主检查跨上下文误用；面向外部语言应使用保留句柄。
Unicode 字符串和键支持内嵌 NUL，传递 `std::string_view` 时须保留实际长度。

`ObjectHooks` 提供 get/set/has/keys。get 返回 nullptr 表示交由普通属性查找处理，set 返回 false
表示未处理该写入。只提供 keys 时，has 从 keys 判断属性存在性，不读取 getter。
旧的 getter/setter 重载仍保留原来的查找语义。
JSC 对 has/keys 钩子没有异常输出槽，这两类钩子应不抛异常；意外异常会记录到 Logger，
并返回 false/空键集合。get、set 和函数回调的异常会转成页面的 JS Error。

递归数据转换可使用 `<ulbind17/jsc/Traversal.hpp>` 的 `Traversal::enter(identity)`，
其作用域 Guard 默认限制 64 层并检测当前路径的循环引用；重复的兄弟节点允许再次转换。
容器映射、整数精度、null/undefined 映射和宿主对象有效性仍由适配器决定。
低层 JSC 手工绑定由宿主决定哪些页面能访问，不自动应用 `js::API::AttachTo()` 的来源规则。

## 绑定 C++ 函数和类

`<ulbind17/ulbind17.hpp>` 通过 `ulbind17::js` 暴露官方的类型化 JavaScript 绑定接口。

```cpp
#include <ulbind17/setup.hpp>
#include <ulbind17/ulbind17.hpp>
#include <stdexcept>

namespace js = ulbind17::js;

ulbind17::setup_ultralight_platform();
auto renderer = ultralight::Renderer::Create();
ultralight::ViewConfig config;
config.is_accelerated = false;
auto view = renderer->CreateView(800, 450, config, nullptr);

js::API api("app");
api["hello"] = [](std::string who) { return "hello " + who; };
if (!api.AttachTo(view.get()))
    throw std::runtime_error("无法将 app API 绑定到页面");

view->LoadHTML("<body><script>document.body.textContent = app.hello('world')</script>");
```

页面使用这些绑定期间，必须保持 `api` 及回调捕获的 C++ 对象存活。
在宿主循环中调用 `RefreshDisplay(0)`、`Update()` 和 `Render()`。
新建 View 的初始文档具有不透明来源，默认来源策略不会向其中注入绑定；
需要先加载自己的 HTML 或本地页面。应在加载实际 UI 前绑定 API，页面导航后会重新注入绑定。
向远程页面绑定 API 时，应显式配置允许的来源规则。

操作页面时，在页面就绪后创建 `js::Context(view.get())`。
Context 和 Value 句柄属于当前页面；导航后需要重新获取句柄。

```cpp
js::Context context(view.get());
auto object = context.MakeObject();
object["score"] = 7;
auto score = ulbind17::get<int>(object, "score");
if (!score) {
    std::cerr << score.error().message() << '\n';
} else {
    std::cout << score.value() << '\n';
}
```

所有便捷函数都返回需要检查成功或失败的 `js::Result`：

- `get<T>(value, key)`：按字符串或整数属性键读取值，使用 SDK 的严格类型转换。
- `keys(value)`：按 `Object.keys` 顺序返回对象自身可枚举的字符串键，不读取属性值，也不触发 getter。
- `size(value)`：返回数组长度（包含空洞），或对象自身可枚举的键数量。

失败时读取 `error()`，仅在成功时调用 `value()`。
对失败的 Result 调用官方 SDK 的 `value()` 会产生未定义行为。
缺失值、异常、空句柄和已失效的页面句柄都会保留错误，不会被悄悄转换为默认值。

使用 SDK 的类定义接口绑定 C++ 类：

```cpp
api.DefineClass<Player>("Player")
    .Constructor<>()
    .Field("score", &Player::score)
    .Method("add", &Player::Add);
```

通过绑定的构造函数创建的实例由 SDK 管理所有权。返回 `Player*` 时，JavaScript 包装对象
仅借用该实例；需要保持实例存活，或在销毁前对每个页面中的包装对象调用
`js::Detach<Player>(wrapper)`。官方类定义在每个进程中按 C++ 类型注册一次。
销毁最后一个持有所有权的 API 句柄时，其绑定会被解除。

## 迁移旧代码

| 旧版 ulbind17 接口 | 替代方式 |
| --- | --- |
| `Object::GetGlobalObject(raw_context)` | `js::Context(view).GlobalObject()` |
| `Array(raw_context)` / `Object(raw_context)` | `context.MakeArray({})` / `context.MakeObject()` |
| `object.get<T>(key)` | `ulbind17::get<T>(object, key)` |
| `object.set(key, value)` | `object[key] = value`，或检查 `SetProperty()` 的返回结果 |
| `object.size()` / `object.keys()` | `ulbind17::size(object)` / `ulbind17::keys(object)` |
| `window.bindFunc(name, fn)` | `api[name] = fn`；页面通过 `app.name()` 调用 |
| 对象上的 `bindFunc()` | `object[name] = context.MakeFunction(name, fn)` |
| `Script(raw_context, code).Evaluate<T>()` | `context.Evaluate<T>(code)`，返回 Result |
| `ClassDef<T>().defProperty().bindFunc().end()` | `api.DefineClass<T>().Field().Method()` |
| `Null` / `Undefined` / 空指针值 | `js::null` / `js::undefined` |

类型转换采用严格规则：`"7"` 不会自动转换为整数，`3.5` 不会被截断为 `3`。
需要 JavaScript 隐式转换语义时，应显式调用 `ToNumber()` / `ToString()`。
脚本求值返回执行完成时的值；顶层 `return` 不是合法的 JavaScript。
旧版 `ClassDef<C, Base>` 的原型继承不再保留，需要显式注册 C++ 基类成员。
`raw()` 返回的 SDK 句柄不是 JavaScriptCore 的 `JSValueRef` / `JSContextRef`。
需要直接访问 JavaScriptCore 时，仍可通过 SDK 的 `View::LockJSContext()` 获取上下文。

## 内嵌资源与验证

两个初始化函数保留原名：
`setup_ultralight_platform()` 和 `setup_ultralight_platform_with_chinese_font()`。
平台服务在整个进程中保持存活；首次调用决定使用的配置和字体。
ICU 数据和证书从所选 SDK 生成到构建目录中，原有的内嵌字体继续保留。
运行时资源查找遵循 `Config::resource_path_prefix`，不需要外部 `assets/resources` 目录。

自定义文件系统可显式选择两种 SDK 资源来源，二者都提供 `FileExists()` 和 `OpenFile()`：

```cpp
#include <ulbind17/resources/embedded/SDKResources.hpp>
#include <ulbind17/resources/filesystem/SDKResources.hpp>

// 内嵌模式：只访问所选 SDK 的静态字节，Buffer 不复制数据，也不访问宿主文件系统。
ulbind17::resources::embedded::SDKResources embedded("resources/");
auto memory = embedded.OpenFile("resources/cacert.pem");

// 文件模式：从 rootdir/resource_dir 读取，只需要链接 ulbind17::header。
ulbind17::resources::filesystem::SDKResources files("./assets", "resources/");
auto disk = files.OpenFile("resources/cacert.pem");
```

内嵌模式链接 `ulbind17::ulbind17`，文件模式可只链接 `ulbind17::header`。
两种模式仅识别 ICU 和证书文件，匹配时规范化资源前缀与请求路径；未匹配的请求返回 false/nullptr，交由调用方的文件系统处理。
Godot 插件应选择内嵌模式，普通项目内容继续使用 Godot `FileAccess`。

使用 `FileSystemOverlay` 可以复用资源优先、宿主文件系统兜底的组合逻辑：

```cpp
#include <ulbind17/resources/FileSystemOverlay.hpp>
#include <ulbind17/resources/embedded/SDKResources.hpp>

auto files = std::make_shared<MyHostFileSystem>();
auto overlay = std::make_shared<ulbind17::resources::FileSystemOverlay<
    ulbind17::resources::embedded::SDKResources>>(
        ulbind17::resources::embedded::SDKResources(config.resource_path_prefix.utf8().data()), files);
ultralight::Platform::instance().set_file_system(overlay.get());
// 在创建 Renderer 前注册，并保持 overlay 存活至最后一个 Renderer 引用释放。
```

模板也接受文件式 SDKResources。成功读取 SDK 资源时不访问宿主文件系统；未命中或资源无法读取时
委托宿主。Mime-type 和 charset 始终由宿主提供。Overlay 拥有 provider 和宿主的共享引用，
两者须支持 SDK 的并发文件读取调用。

回归测试覆盖绑定接口的类型转换、稀疏数组、Unicode 键、C++ 对象所有权与解除绑定、
页面导航、两种字体、内嵌资源与所选 SDK 的字节一致性，以及 SDK 下载完整性和缓存错误。
GLFW 示例继续显示 CPU 渲染的 BitmapSurface，没有实现新版 Ultralight GPUDriver 接口。

字体度量、内存默认值、WebAssembly 等引擎行为变化，参见
[Ultralight 官方迁移指南](https://ultralig.ht/docs/2.0/migrating-from-1-4-to-2-0)。

## ManagedView 输入转发

`ManagedView` 支持 `FireMouseEvent`、`FireScrollEvent`、`FireKeyEvent`、`Focus`、
`Unfocus`、`HasFocus` 和 `HasInputFocus`，直接使用 SDK 的通用事件类型。
坐标和滚动量遵循 SDK 的逻辑像素约定，文本输入使用 `KeyEvent::kType_Char`，
物理按键推荐 `kType_RawKeyDown` / `kType_KeyUp`。

调用必须位于 Renderer 线程，关闭后返回 `Closed`；像素锁定、Renderer 调用、
输入分发或 `WithBridge` 操作期间再次发送输入返回 `Busy`。
输入期间保活原生 View 和 Renderer，因此回调可以关闭宿主包装对象。
回调可通过 `WithBridge` 读取页面，但应在当前调用返回后再发送后续输入。
`HasFocus` / `HasInputFocus` 仅查询状态，不触发输入。

事件分发不自动驱动渲染。宿主应继续执行 `Update`、`RefreshDisplay`、`Render`；
某些滚动效果需要完整的帧循环才能推进。
