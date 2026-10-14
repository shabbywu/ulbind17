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
| `ULBIND17_ENABLE_APPCORE` | OFF | 提供可选的 AppCore 集成 target |
| `ULBIND17_BUILD_APPCORE_SAMPLE` | OFF | 构建 AppCore 字体、窗口与输入示例 |
| `ULBIND17_BUILD_OFFICIAL_SAMPLES` | OFF | 构建 SDK 桌面示例，需要启用 AppCore |
| `ULBIND17_BUILD_GAME_UI_HOST_SAMPLE` | OFF | 构建游戏 UI 离屏示例 |

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
Windows CI 编译全部示例，运行 CTest 时排除 Sample5 和 AppCore 桌面测试；本地可在桌面会话中手动运行。

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

共享库或模块库也可以调用相同的复制函数：

```cmake
target_link_libraries(my_extension PRIVATE ulbind17::ulbind17)
ulbind17_copy_runtime_files(my_extension INCLUDE_APPCORE OFF)
```

`INCLUDE_APPCORE` 默认 ON，保留原调用行为；不使用 AppCore 的宿主可关闭它。
macOS 的库从 `@loader_path/` 查找同目录依赖，Linux 使用 `$ORIGIN`。
复制到可执行文件、SHARED_LIBRARY 或 MODULE_LIBRARY 目标的输出目录；该函数不负责安装或 wheel 打包。

## 通用宿主运行时

`<ulbind17/runtime.hpp>` 提供可选的 `Runtime`、`ManagedView` 和 CPU 像素接口。
实现为头文件，支持自定义宿主循环与 CPU 像素读取。

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

`<ulbind17/jsc/Bridge.hpp>` 提供动态类型桥接接口；宿主负责自身对象的类型转换与引用管理。
JSC 的原始句柄与类型化 SDK 的 `js::Value::raw()` 是不同类型，不能混用。
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

`<ulbind17/API.hpp>` 提供独立于 View 的类型化 `ulbind17::API` 注册表，
复用官方 `js::API` 的函数、常量、类、事件与类型转换。先注册，再附着至一个或多个 ManagedView：

```cpp
#include <ulbind17/API.hpp>
#include <ulbind17/setup.hpp>

ulbind17::setup_ultralight_platform();
auto runtime = ulbind17::Runtime::Create();
ultralight::ViewConfig config;
config.is_accelerated = false;
auto view = runtime.CreateView(800, 450, config);
ulbind17::API api("app");
api.Bind("hello", [](std::string who) { return "hello " + who; });
api["version"] = "example";
api.DefineEvent<double>("tick");
if (!api.AttachTo(view))
    throw std::runtime_error("API attachment failed");
view.LoadHTML("<body><script>document.body.textContent = app.hello('world')</script>");
// 宿主持续驱动 Runtime；事件在后续 Update 中交付。
api.Emit("tick", 1.0);
```

`API` 是 SDK `js::API` 的值类型适配，不用于通过基类指针管理多态所有权。
支持 SDK 的 Bind、成员函数、DefineClass、属性、事件、诊断和 AttachOptions；
`ulbind17::js::API` 仍是原始 SDK 类型。旧 `Bindings` 封装已移除：改用 `API`，
将 `bindFunc()` 改为 `Bind()`，原 `.api()` 调用直接在 API 上进行。

`AttachTo(ManagedView, options)` 和 `DetachFrom(ManagedView)` 检查 Renderer 线程、关闭状态与像素锁，
操作期间保活原生对象。ManagedView 关闭时自动解除通过该入口记录的附着，停止后续注入与事件。
API 的拥有者由调用方保存，ManagedView 的 borrowed handle 不延长拥有者寿命；
销毁最后一个拥有者后，页面仍保留的函数按 SDK 规则报告 `ULJS_DETACHED`。
单独 DetachFrom 不撤销当前页面已有的函数，只停止后续注入与事件；这是 SDK 的原有语义。

导航后 SDK 自动重新注入 API，无需在 window_ready 中重新注册；动态 jsc::Bridge 的文档失效规则不变。
附着不会替换 LoadListener、ViewListener 或 editor listener。
重复 AttachTo 更新配置。注册在页面加载后发生变化时，当前页面保留旧绑定，下一次导航才使用新注册。
默认只注入主框架和本地 file:// / LoadHTML 内容，命名空间冻结；
`js::AllFrames` 开启子框架注入，显式 origin_rules 替换默认来源规则。
AttachTo 返回成功表示附着配置成功，并不表示当前来源允许注入；初始不透明文档可先附着，再加载实际页面。
ManagedView 入口的状态错误抛出 `ulbind17::Error`，SDK 的无效 API 或附着配置返回 false。
API 附着后的注册、分离与销毁在 Renderer 线程执行；Emit 可从工作线程调用。
数值损失与多余参数的诊断沿用 SDK 规则；需要将其作为调用错误时，使用 `js::Diagnostics::Strict`。

`<ulbind17/ulbind17.hpp>` 同时提供上述适配与原始 SDK 类型化接口。
直接使用原生 View 时，仍可使用 `ulbind17::js::API`：

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
可以先附着 API，再加载自己的 HTML 或本地页面；页面导航后会重新注入绑定。
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
需要将 SDK 数据随二进制分发时，可选择内嵌模式；页面资源可由自定义 FileSystem 提供。

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

## 可选 AppCore 桌面集成

`<ulbind17/appcore.hpp>` 提供 `ulbind17::appcore::Application` 和 `Window`。
开启 `ULBIND17_ENABLE_APPCORE` 后，链接 `ulbind17::appcore`；该 target 只依赖
`ulbind17::header` 和 `Ultralight::AppCore`，不强制链接内嵌资源。
原有核心 target 及独立 Renderer 的驱动方式保持不变。此集成已在 **2.0.0-beta.2** 上验证。

### 独立 Renderer 使用系统字体

```cpp
#include <ulbind17/appcore.hpp>

// 先安装宿主 FileSystem、Logger 和 Config，并确保服务的生命周期覆盖 Renderer。
// 如果需要原有内嵌资源，也可以先调用 setup_ultralight_platform()。
ulbind17::appcore::UseSystemFontLoader();
auto runtime = ulbind17::Runtime::Create();
// 后续仍由宿主调用 runtime.Update()/RefreshDisplay()/Render()。
```

`UseSystemFontLoader()` 显式选择 AppCore 的系统字体，只修改 FontLoader，保留自定义
FileSystem、Logger、GPUDriver、SurfaceFactory 等 handler。它必须在创建任何 Renderer/App 前调用；
对本二进制已经注册的 Renderer 或已有 App 调用会返回 `Busy`。原生 SDK Renderer 的创建绕过
本库注册表时，宿主仍须自行保证初始化顺序。

AppCore 的 `GetPlatformFontLoader()`、`GetPlatformFileSystem(baseDir)` 和
`GetDefaultLogger(logPath)` 返回 SDK 拥有的进程级单例，**不要 delete**。
FileSystem 和 Logger 的路径仅在首次获取时生效，后续参数不会重设路径。
直接调用 `GetPlatformFileSystem()` 时，相对目录以工作目录为基准；
`App::Create()` 的 `Settings::file_system_path` 则以可执行文件目录为基准，
macOS 上以 `.app/Contents/Resources/` 为基准。自定义 handler 的所有权仍属于宿主。

### 应用、窗口与 ManagedView

```cpp
#include <ulbind17/appcore.hpp>

namespace ac = ulbind17::appcore;
auto app = ac::Application::Create();
auto runtime = app.runtime(); // 内部使用 Runtime::FromExisting(app->renderer())。
auto window = app.CreateWindow(800, 540); // 逻辑像素；默认 Titled | Resizable。
window.SetCallbacks({[&] { app.Close(); }, {}});
auto view = runtime.CreateView(window.native()->device_width(),
                               window.native()->device_height(),
                               window.native()->default_view_config());
auto panel = window.AdoptPanel(view); // 默认填满窗口。
// 在 LoadHTML/LoadURL 前设置 ViewCallbacks；每次主框架 window_ready 中注册 Bridge。
view.LoadHTML("<body>系统字体与 AppCore 窗口</body>");
view.Focus(); // 自动转到 Panel::Focus()。
while (app.RunOnce(0.002)) {
    // 宿主工作；不再调用 Renderer 的 Update、RefreshDisplay 或 Render。
}
app.Close();
```

`Application::Create(Settings, Config)` 创建并管理一个 App；同一二进制内重复调用共享已有状态，
首次 Settings/Config 生效。关闭后不重新创建。先创建独立 Renderer 再创建 Application 会被拒绝。
AppCore 会保留已注册的 FileSystem、FontLoader、Logger 等服务，只补齐未设置的服务。
它会调整 SDK 规定的 Config 字段，包括 clockwise face winding、默认 cache path 和自动 font profile。
预先注册自定义 GPUDriver 或 SurfaceFactory 时，窗口封装返回 `Unsupported`，因为 AppCore 无法用它们
展示页面；这种宿主可以只复用系统字体服务。

`Application::CreateWindow(width, height, fullscreen, flags, monitor)` 可创建多个窗口，默认使用主显示器。
`Window::AdoptPanel(view, PanelOptions)` 只接受本 Application 的 Runtime 创建、且尚未托管的 View。
返回的 SDK Panel 句柄用于布局配置、隐藏/显示或查询；一个 View 只托管一次，关闭后不重用。
View 初始尺寸是**设备像素**。从 `default_view_config()` 取得字体、编辑行为、display ID、scale 和
GPU/CPU 配置；Panel 在后续布局和 DPI 改变时自动维护 View，不需要宿主重复 Resize。

托管不会替换 ManagedView 的 LoadListener/ViewListener，因此现有 Bridge、加载回调、异常处理及
主框架导航失效规则继续适用。**不要替换托管 View 的 editor listener**，AppCore 用它处理 IME。
`ManagedView::Focus()` 经由 Panel；`Unfocus()` 仅在本 Panel 正持有焦点时清除窗口焦点，
不会清除其他 Panel 的焦点。真实 OS 输入由 AppCore 自动发送，避免宿主再次转发相同事件。

`Window::SetCallbacks(WindowCallbacks)` 替换整组窗口回调，包含 `closed()` 和
`resized(double width, double height)`，后者使用逻辑像素。窗口回调的异常转交 Runtime 错误处理器，
错误处理器的异常也会被隔离。App/Window/Panel 的原生借用接口用于配置和查询；不要通过它们替换
监听器、移除受管理 Panel、直接驱动 Renderer 或销毁 App。使用 `Window::Close()` 和 `ManagedView::Close()`
管理托管关系。回调不要按值捕获自己所在的 Window/Application/ManagedView，以免形成引用环。

### 驱动与关闭

`RunOnce(max_wait_seconds = 0)` 处理系统事件、更新 Renderer，并按 SDK 的显示节奏渲染/展示。
传入 0 不主动等待，非负有限值控制等待上限；宿主应重复调用它。
macOS 的 `RunOnce()` 手动泵送事件，与 SDK `Run()` 的菜单快捷键分派可能有差异。
附着 AppCore 后，所有共享 Runtime 句柄的 `Update/RefreshDisplay/Render` 返回 `Unsupported`，
防止双重驱动；原生指针绕过封装时，互斥规则仍由宿主保证。
`Runtime::PostTask()` 继续可用，与 AppCore 共用 SDK 任务队列。

App、Window、View 操作和最终析构必须在 App/Renderer 的主线程进行。`RunOnce()` 不允许嵌套，
也不能在像素锁、输入分派或 `WithBridge()` 作用域中调用，违规返回 `Busy`。
在 SDK 更新期间触发的回调和 PostTask 中需要进一步发送输入时，安排在当前 RunOnce 返回后执行。

- Window 关闭会同时关闭它托管的所有 ManagedView。先停止页面回调、失效 Bridge 和取消 Promise，
  再移除 Panel、关闭原生窗口，最后发送窗口 `closed` 回调。
- 单独关闭 ManagedView 会移除它的 Panel，窗口仍存活。释放最后一个 Window 句柄也会关闭窗口；
  Window 持有托管 View，释放外部 ManagedView 句柄不会让页面从窗口消失。
- Application `Close()` 先停止循环，然后关闭所有窗口及 View、关闭 Runtime 并释放其 Renderer 引用，
  最后释放 App。直接调用附着的 Runtime `Close()` 会执行同一套关闭流程。
- Window、Runtime 和存活的 View 会保活 App；释放 Application 句柄不会使它们悬空。
  回调中允许关闭，当前原生调用的 App/Renderer/View 引用保留到调用返回。
- `Close()` 幂等；关闭后的 `RunOnce()` 返回 false，其他操作返回 `Closed`，旧 Bridge Handle 返回 `PageGone`。
  Application 关闭后尚未执行的 Runtime 任务被抑制。

系统服务的进程级单例由 SDK 保证；头文件句柄注册状态仍仅在同一二进制内共享。
跨扩展模块共享 Renderer 时，应由单一宿主传递同一 Renderer，并负责驱动和关闭。

### macOS ARM64 示例与验证

```sh
cmake -S . -B build/appcore \
  -DULTRALIGHT_SDK_VERSION=2.0.0-beta.2 \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DULBIND17_ENABLE_APPCORE=ON \
  -DULBIND17_BUILD_APPCORE_SAMPLE=ON
cmake --build build/appcore -j
./build/appcore/samples/AppCoreExample.app/Contents/MacOS/AppCoreExample
ctest --test-dir build/appcore -R 'appcore[.]|AppCoreExample' --output-on-failure
```

示例显示系统字体、中文、IME 输入框、Bridge 调用结果、页面尺寸与 devicePixelRatio。
拖动窗口边缘验证 resize，移动到不同 DPI 的显示器验证 scale 和布局更新。
`--smoke-test` 在页面与 Panel 布局就绪后检查 Bridge、窗口可见性和 DPI，并自动关闭；
`--cpu` 可验证 CPU 展示路径，默认跟随 AppCore 的 GPU/CPU 选择。

使用 AppCore 的应用可调用 `ulbind17_copy_runtime_files(target INCLUDE_APPCORE ON)` 复制动态库并设置 rpath。
该函数不复制页面和 SDK 数据资源。示例将所选 SDK 的资源复制到
`.app/Contents/Resources/assets/resources/`；其他平台复制到可执行文件旁的 `assets/resources/`，
与默认 `Settings::file_system_path` 和 `Config::resource_path_prefix` 对应。
宿主若使用内嵌 SDKResources/FileSystemOverlay，则自行注册该 FileSystem 并保持其存活，无需复制数据文件。

AppCore 测试覆盖系统字体及中文 fallback、首次路径规则、两种 scale、Panel 布局 resize 和 macOS 原生窗口 resize、
焦点/editor listener、首行 Bridge 绑定、主/子框架导航、加载失败、回调异常、Promise 取消、
关闭重入、共享句柄失效、像素锁和主线程约束。涉及 AppCore 窗口的测试需要原生桌面会话；
真实窗口拖动、跨屏 DPI 和中文 IME 仍需在交互示例中验证。

### SDK 示例

提供使用 Runtime、ManagedView 与 Bridge 的 Ultralight SDK 示例。
启用 `ULBIND17_BUILD_OFFICIAL_SAMPLES=ON`（需要 AppCore）构建桌面样例；
独立的 `ULBIND17_BUILD_GAME_UI_HOST_SAMPLE=ON` 提供不依赖 AppCore 的游戏 UI 离屏宿主示例，
展示自定义循环、输入转发和 CPU 帧读取。构建、验证及资源来源见 [samples/official/README.md](samples/official/README.md)。

### Release 示例附件

发布 GitHub Release 时，CI 构建全部示例并上传每个平台一个 ZIP：
`ulbind17-samples-windows-x64.zip`、`ulbind17-samples-linux-x64.zip`、
`ulbind17-samples-macos-arm64.zip`。压缩包包含示例程序、运行库、页面资源及许可文件，
每个示例位于独立目录。解压后，Linux/Windows 从示例目录运行程序；macOS 打开对应 `.app`。
普通 CI 与手动运行也会生成同名 workflow artifacts，供下载验证。
