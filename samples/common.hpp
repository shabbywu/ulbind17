#pragma once
#include <ulbind17/setup.hpp>
#include <ulbind17/ulbind17.hpp>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace sample {
namespace js = ulbind17::js;
using ultralight::RefPtr;
using ultralight::Renderer;
using ultralight::View;
using ultralight::ViewConfig;

template <typename T> T take(js::Result<T> result) {
    if (!result)
        throw std::runtime_error(result.error().message());
    return std::move(result).value();
}

inline void check(bool condition, const std::string &message) {
    if (!condition)
        throw std::runtime_error(message);
}

struct Fixture {
    RefPtr<Renderer> renderer = Renderer::Create();
    RefPtr<View> view;

    Fixture() {
        ViewConfig config;
        config.is_accelerated = false;
        view = renderer->CreateView(320, 180, config, nullptr);
        check(view.get() != nullptr, "CreateView failed");
        // The initial opaque document is not allowed to receive a native API.
        // Load our own local page before evaluating scripts against the bridge.
        struct Ready : ultralight::LoadListener {
            bool ready = false;
            void OnDOMReady(View *, std::uint64_t, bool main_frame, const ultralight::String &) override {
                if (main_frame)
                    ready = true;
            }
        } listener;
        view->set_load_listener(&listener);
        view->LoadHTML("<!doctype html><body></body>");
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!listener.ready && std::chrono::steady_clock::now() < deadline) {
            renderer->Update();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        view->set_load_listener(nullptr);
        check(listener.ready, "local page did not reach DOMReady");
    }
};

template <typename F> int run(F &&body, bool chinese = false) {
    try {
        if (chinese)
            ulbind17::setup_ultralight_platform_with_chinese_font();
        else
            ulbind17::setup_ultralight_platform();
        std::forward<F>(body)();
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
} // namespace sample
