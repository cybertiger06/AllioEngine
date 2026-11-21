#include "../../core/script/CSharpHost.h"

#include <array>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace jasperengine::managed;

namespace {
    std::vector<std::string> messages;
    bool failed = false;
    const NativeApi native{
        [](int error, const char* message) {
            if (error) {
                failed = true;
                std::cerr << message << '\n';
            } else messages.emplace_back(message);
        },
        [](void* instance, float* x, float* y, float* z) {
            const auto& p = *static_cast<std::array<float, 3>*>(instance);
            *x = p[0]; *y = p[1]; *z = p[2];
        },
        [](void* instance, float x, float y, float z) {
            *static_cast<std::array<float, 3>*>(instance) = {x, y, z};
        },
        nullptr, nullptr, nullptr, nullptr
    };
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    if (argc != 2) return 1;
    const fs::path root = fs::absolute(argv[1]);
    ManagedApi api{};
    if (!initialize(root, native, api)) return 1;
    const auto game = root / "Allio.Game.dll";
    const auto path = game.u8string();
    for (int play = 0; play != 2; ++play) {
        if (!api.load(reinterpret_cast<const char*>(path.c_str()))) return 1;
        std::array<float, 3> position = {1.f, 2.f, 3.f};
        int features = 0;
        void* instance = api.create("Probe", &position, &features);
        if (!instance || features != 3) return 1;
        api.property(instance, "speed", "10");
        for (int event : {0, 1, 2, 3, 6, 4, 5})
            api.notify(instance, event, .25f, "settings");
        std::string fields;
        api.inspect(instance, [](const char* json, void* output) {
            *static_cast<std::string*>(output) = json;
        }, &fields);
        if (fields != R"({"speed":10})") return 1;
        api.release(instance);
        api.unload();
        if (position != std::array<float, 3>{3.5f, 2.f, 3.f} || failed ||
            messages != std::vector<std::string>{"awake:1", "enable", "start", "click:settings", "disable", "destroy"}) {
            std::cerr << "Callbacks, property binding, or per-Play state did not match." << '\n';
            return 1;
        }
        messages.clear();
        // Editor rebuilds replace this DLL at the same path on the next Play.
        fs::rename(game, root / "previous.dll");
        fs::rename(root / "previous.dll", game);
    }
}
