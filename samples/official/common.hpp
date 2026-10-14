#pragma once
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <ulbind17/appcore.hpp>
#include <vector>

namespace official {
using Bridge = ulbind17::jsc::Bridge;
using Clock = std::chrono::steady_clock;
inline void Check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
struct Options {
    bool smoke = false, cpu = false;
    std::string scene;
    static Options Parse(int argc, char **argv) {
        Options result;
        for (int i = 1; i < argc; ++i) {
            std::string_view argument(argv[i]);
            if (argument == "--smoke-test")
                result.smoke = true;
            else if (argument == "--cpu")
                result.cpu = true;
            else if (argument == "--scene" && i + 1 < argc)
                result.scene = argv[++i];
            else
                throw std::runtime_error("Usage: sample [--smoke-test] [--cpu] [--scene NAME]");
        }
        return result;
    }
};
struct Page {
    ulbind17::appcore::Window window;
    ulbind17::ManagedView view;
    ultralight::RefPtr<ultralight::Panel> panel;
    bool ready = false;
    unsigned documents = 0;
    std::function<void(Bridge &)> bind;
    Page(ulbind17::appcore::Window value, const ulbind17::Runtime &runtime)
        : window(std::move(value)),
          view(runtime.CreateView(window.native()->device_width(), window.native()->device_height(),
                                  window.native()->default_view_config())),
          panel(window.AdoptPanel(view)) {
    }
    bool Boolean(std::string_view expression) const {
        return view.WithBridge(
            [&](auto &bridge) { return bridge.readBoolean(bridge.evaluateRetained(expression)->value()); });
    }
    std::string String(std::string_view expression) const {
        return view.WithBridge(
            [&](auto &bridge) { return bridge.readString(bridge.evaluateRetained(expression)->value()); });
    }
    void Execute(std::string_view source) const {
        view.WithBridge([&](auto &bridge) { bridge.evaluateRetained(source); });
    }
    void Load(std::string_view url) {
        ready = false;
        view.LoadURL(url);
        window.native()->ShowWhenReady();
    }
};
class Host {
    std::string error_;
    std::vector<std::unique_ptr<Page>> pages_;

  public:
    Options options;
    ulbind17::appcore::Application app;
    ulbind17::Runtime runtime;
    explicit Host(Options value, const char *name)
        : options(std::move(value)), app(Create(options, name)), runtime(app.runtime()) {
        runtime.SetErrorHandler([this](const auto &error) { error_ = error.what(); });
    }
    ~Host() {
        app.Close();
        pages_.clear();
    }
    static ulbind17::appcore::Application Create(const Options &options, const char *name) {
        ultralight::Settings settings;
        settings.developer_name = "ulbind17";
        settings.app_name = name;
        settings.force_cpu_renderer = options.cpu;
        return ulbind17::appcore::Application::Create(settings);
    }
    Page &CreatePage(const char *title, double width, double height) {
        auto window = app.CreateWindow(width, height, false,
                                       ultralight::WindowFlags::Titled | ultralight::WindowFlags::Resizable |
                                           ultralight::WindowFlags::Hidden);
        auto page = std::make_unique<Page>(std::move(window), runtime);
        auto *state = page.get();
        state->window.native()->SetTitle(title);
        state->window.SetCallbacks({[this] { app.Close(); }, {}});
        ulbind17::ViewCallbacks callbacks;
        callbacks.begin_loading = [state](const auto &event) {
            if (event.is_main_frame)
                state->ready = false;
        };
        callbacks.window_ready = [state](const auto &event) {
            if (!event.is_main_frame)
                return;
            ++state->documents;
            state->view.WithBridge([&](auto &bridge) {
                bridge.evaluateRetained(R"JS(
                    globalThis.__sampleErrors = [];
                    addEventListener('error', e => __sampleErrors.push(String(e.message)));
                    addEventListener('unhandledrejection', e => __sampleErrors.push(String(e.reason)));
                )JS");
                if (state->bind)
                    state->bind(bridge);
            });
        };
        callbacks.dom_ready = [state](const auto &event) {
            if (event.is_main_frame)
                state->ready = true;
        };
        callbacks.fail_loading = [this](const auto &failure) { error_ = failure.description; };
        callbacks.request_close = [state] { state->window.Close(); };
        state->view.SetCallbacks(std::move(callbacks));
        pages_.push_back(std::move(page));
        return *state;
    }
    void Run(std::function<void()> update = {}, std::function<bool()> verify = {}) {
        auto deadline = Clock::now() + std::chrono::seconds(30);
        bool validated = false;
        while (app.RunOnce(0.002)) {
            if (!error_.empty())
                throw std::runtime_error(error_);
            if (update)
                update();
            if (!options.smoke)
                continue;
            Check(Clock::now() < deadline, "Official sample smoke timed out");
            bool ready = true;
            for (auto &page : pages_) {
                if (!page->ready || page->panel->bounds().height() <= 0) {
                    ready = false;
                    continue;
                }
                auto errors = page->String("JSON.stringify(__sampleErrors)");
                if (errors != "[]")
                    throw std::runtime_error(errors);
            }
            if (ready && (!verify || verify())) {
                validated = true;
                std::cout << "Official sample smoke passed\n";
                app.Close();
            }
        }
        Check(!options.smoke || validated, "Window closed before smoke validation");
    }
};
template <typename F> int Main(F callback) {
    try {
        callback();
        return 0;
    }
    catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
} // namespace official
