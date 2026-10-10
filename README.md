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
| `BUILD_TESTING` | 作为顶层项目时为 ON | 构建四个控制台示例和回归测试 |
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

回归测试覆盖绑定接口的类型转换、稀疏数组、Unicode 键、C++ 对象所有权与解除绑定、
页面导航、两种字体、内嵌资源与所选 SDK 的字节一致性，以及 SDK 下载完整性和缓存错误。
GLFW 示例继续显示 CPU 渲染的 BitmapSurface，没有实现新版 Ultralight GPUDriver 接口。

字体度量、内存默认值、WebAssembly 等引擎行为变化，参见
[Ultralight 官方迁移指南](https://ultralig.ht/docs/2.0/migrating-from-1-4-to-2-0)。
