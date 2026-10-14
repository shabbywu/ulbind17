#pragma once
#include <array>
#include <string>
#include <string_view>
#include <ulbind17/runtime.hpp>

namespace game_ui {
inline constexpr std::array<std::string_view, 6> scenes = {"flight-hud",      "overdrive",    "lightfingers",
                                                           "ashen-reliquary", "wind-and-ink", "tidepool"};
inline std::string URL(std::string_view scene) {
    if (scene.empty())
        return "file:///index.html";
    for (auto name : scenes)
        if (name == scene)
            return "file:///scenes/" + std::string(scene) + "/index.html";
    throw ulbind17::Error(ulbind17::ErrorCode::InvalidArgument, "Unknown game UI scene");
}
inline bool Ready(const ulbind17::ManagedView &view, bool scene) {
    return view.WithBridge([&](auto &bridge) {
        return bridge.readBoolean(
            bridge
                .evaluateRetained(scene ? "typeof window.scene === 'object' && JSON.parse(scene.info()).ready && "
                                          "JSON.parse(scene.perf()).frames >= 3 && "
                                          "(!document.getElementById('scene-veil') || "
                                          "+getComputedStyle(document.getElementById('scene-veil')).opacity < 0.05)"
                                        : "document.querySelectorAll('a.card').length === 6")
                ->value());
    });
}
inline void CheckScene(const ulbind17::ManagedView &view) {
    view.WithBridge([](auto &bridge) {
        auto error = bridge.readString(
            bridge
                .evaluateRetained(
                    "JSON.stringify({info: JSON.parse(scene.info()).error, perf: JSON.parse(scene.perf()).firstError})")
                ->value());
        if (error != "{\"info\":null,\"perf\":null}")
            throw std::runtime_error(error);
    });
}
} // namespace game_ui
