#include "../game_ui.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <ulbind17/resources/FileSystemOverlay.hpp>
#include <ulbind17/setup.hpp>

// This executable uses only the core target: no App, Window, Panel or AppCore drive calls.
int main(int argc, char **argv) {
    try {
        std::string scene, output;
        bool smoke = false;
        std::filesystem::path assets = "assets";
        for (int i = 1; i < argc; ++i) {
            std::string_view argument(argv[i]);
            if (argument == "--smoke-test") {
                smoke = true;
                continue;
            }
            if (argument == "--scene" && i + 1 < argc)
                scene = argv[++i];
            else if (argument == "--assets" && i + 1 < argc)
                assets = argv[++i];
            else if (argument == "--output" && i + 1 < argc)
                output = argv[++i];
            else
                throw std::runtime_error("Usage: GameUIHostAdapter [--scene NAME] [--assets DIR] [--output FRAME.ppm]");
        }
        auto url = game_ui::URL(scene);
        ulbind17::setup_ultralight_platform();
        using Overlay = ulbind17::resources::FileSystemOverlay<ulbind17::resources::embedded::SDKResources>;
        auto files = std::make_shared<Overlay>(ulbind17::resources::embedded::SDKResources("resources/"),
                                               std::make_shared<ulbind17::platform::FileSystem>(assets));
        ultralight::Platform::instance().set_file_system(files.get());
        auto runtime = ulbind17::Runtime::Create();
        std::string error;
        runtime.SetErrorHandler([&](const auto &value) { error = value.what(); });
        ultralight::ViewConfig config;
        config.is_accelerated = false;
        auto view = runtime.CreateView(800, 450, config);
        bool ready = false;
        ulbind17::ViewCallbacks callbacks;
        callbacks.begin_loading = [&](const auto &event) {
            if (event.is_main_frame)
                ready = false;
        };
        callbacks.dom_ready = [&](const auto &event) {
            if (event.is_main_frame)
                ready = true;
        };
        callbacks.fail_loading = [&](const auto &failure) { error = failure.description; };
        view.SetCallbacks(std::move(callbacks));
        view.LoadURL(url);
        view.Focus();
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (true) {
            runtime.Update();
            runtime.RefreshDisplay();
            runtime.Render();
            if (!error.empty())
                throw std::runtime_error(error);
            if (ready && game_ui::Ready(view, !scene.empty()))
                break;
            if (std::chrono::steady_clock::now() > deadline)
                throw std::runtime_error("Game UI host timed out");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (!scene.empty())
            game_ui::CheckScene(view);
        auto pixels = view.CopyPixels();
        if (pixels.data.empty() || pixels.width != 800 || pixels.height != 450)
            throw std::runtime_error("Game UI host did not produce a CPU frame");
        view.ClearDirtyBounds();
        if (smoke && !scene.empty()) {
            auto previous = view.WithBridge([](auto &bridge) { return bridge.evaluateRetained("()=>42"); });
            ultralight::KeyEvent escape;
            escape.type = ultralight::KeyEvent::kType_RawKeyDown;
            escape.virtual_key_code = 27;
            escape.key_identifier = "U+001B";
            view.Focus();
            view.FireKeyEvent(escape); // The original harness handles Esc and navigates back to the chooser.
            while (!ready || !game_ui::Ready(view, false)) {
                runtime.Update();
                runtime.RefreshDisplay();
                runtime.Render();
                if (!error.empty())
                    throw std::runtime_error(error);
                if (std::chrono::steady_clock::now() > deadline)
                    throw std::runtime_error("Game UI Escape navigation timed out");
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            bool expired = false;
            try {
                previous->value();
            }
            catch (const ulbind17::Error &value) {
                expired = value.code == ulbind17::ErrorCode::PageGone;
            }
            if (!expired)
                throw std::runtime_error("Game UI navigation retained the old Bridge");
        }
        if (!output.empty()) {
            std::ofstream file(output, std::ios::binary);
            file << "P6\n" << pixels.width << ' ' << pixels.height << "\n255\n";
            for (uint32_t y = 0; y < pixels.height; ++y) {
                auto row = pixels.data.data() + y * pixels.row_bytes;
                for (uint32_t x = 0; x < pixels.width; ++x) {
                    const char rgb[] = {char(row[x * 4 + 2]), char(row[x * 4 + 1]), char(row[x * 4])};
                    file.write(rgb, 3);
                }
            }
            if (!file)
                throw std::runtime_error("Could not write CPU frame");
        }
        runtime.Close();
        std::cout << "Game UI host passed: BGRA8 premultiplied " << pixels.width << 'x' << pixels.height
                  << " row_bytes=" << pixels.row_bytes << '\n';
        return 0;
    }
    catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
