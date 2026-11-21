#include "CSharpHost.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32) || (defined(__APPLE__) && !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)) || (defined(__linux__) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__))
#define ALLIO_DESKTOP_CLR 1
#include "../../libs/dotnet-host/coreclr_delegates.h"
#include "../../libs/dotnet-host/hostfxr.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define ALLIO_CLR_TEXT(s) L##s
#else
#include <dlfcn.h>
#define ALLIO_CLR_TEXT(s) s
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif
#endif

namespace jasperengine::managed {
namespace fs = std::filesystem;

#if ALLIO_DESKTOP_CLR
namespace {
#ifdef _WIN32
    constexpr auto hostName = L"hostfxr.dll";
    constexpr auto dotnetName = L"dotnet.exe";
    constexpr auto pathSeparator = L';';
    fs::path environment(const wchar_t* key) {
        const wchar_t* value = _wgetenv(key);
        return value ? fs::path(value) : fs::path{};
    }
    using Library = HMODULE;
    Library open(const fs::path& path) { return LoadLibraryW(path.c_str()); }
    auto symbol(Library lib, const char* name) { return GetProcAddress(lib, name); }
#else
#ifdef __APPLE__
    constexpr auto hostName = "libhostfxr.dylib";
#else
    constexpr auto hostName = "libhostfxr.so";
#endif
    constexpr auto dotnetName = "dotnet";
    constexpr auto pathSeparator = ':';
    fs::path environment(const char* key) {
        const char* value = std::getenv(key);
        return value ? fs::path(value) : fs::path{};
    }
    using Library = void*;
    Library open(const fs::path& path) { return dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL); }
    auto symbol(Library lib, const char* name) { return dlsym(lib, name); }
#endif

    fs::path findHost() {
        std::vector<fs::path> roots{environment(ALLIO_CLR_TEXT("DOTNET_ROOT"))};
        std::basic_istringstream<fs::path::value_type> pathList(environment(ALLIO_CLR_TEXT("PATH")).native());
        fs::path::string_type part;
        std::error_code ec;
        while (std::getline(pathList, part, pathSeparator)) {
            if (part.empty()) continue;
            fs::path dotnet = fs::canonical(fs::path(part) / dotnetName, ec);
            if (!ec) roots.push_back(dotnet.parent_path());
        }
#ifdef _WIN32
        fs::path programFiles = environment(L"ProgramFiles");
        if (!programFiles.empty()) roots.push_back(programFiles / L"dotnet");
#else
        roots.emplace_back("/usr/share/dotnet");
        roots.emplace_back("/usr/local/share/dotnet");
#endif
        for (const auto& root : roots) {
            if (root.empty()) continue;
            std::array<int, 3> bestVersion{};
            fs::path best;
            for (const auto& entry : fs::directory_iterator(root / "host" / "fxr", ec)) {
                std::istringstream version(entry.path().filename().string());
                std::array<int, 3> numbers{};
                char first, second;
                if (!(version >> numbers[0] >> first >> numbers[1] >> second >> numbers[2]) ||
                    first != '.' || second != '.' || numbers[0] < 10 || numbers < bestVersion) continue;
                const fs::path candidate = entry.path() / hostName;
                if (fs::is_regular_file(candidate, ec)) {
                    best = candidate;
                    bestVersion = numbers;
                }
            }
            if (!best.empty()) return best;
        }
        return {};
    }
}
#endif

fs::path defaultAssemblyDirectory() {
    fs::path executable;
#if defined(_WIN32)
    std::wstring path(32768, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length && length < path.size()) executable = path.substr(0, length);
#elif defined(__APPLE__) && ALLIO_DESKTOP_CLR
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> path(size);
    if (!_NSGetExecutablePath(path.data(), &size)) executable = path.data();
#elif defined(__linux__) && ALLIO_DESKTOP_CLR
    std::error_code ec;
    executable = fs::read_symlink("/proc/self/exe", ec);
#endif
    return executable.parent_path() / "managed";
}

bool initialize(const fs::path& directory, const NativeApi& native, ManagedApi& managed) {
#if ALLIO_DESKTOP_CLR
    using Start = int (*)(const NativeApi*, ManagedApi*);
    const fs::path config = fs::absolute(directory / "Allio.Managed.runtimeconfig.json");
    const fs::path assembly = fs::absolute(directory / "Allio.Managed.dll");
    auto utf8 = [](const fs::path& path) {
        auto text = path.u8string();
        return std::string(text.begin(), text.end());
    };
    auto fail = [&](const std::string& action, int code) {
        std::ostringstream message;
        message << "C# scripting: " << action << " (0x" << std::hex << code << ").";
        native.log(1, message.str().c_str());
        return false;
    };
    // CoreCLR and its host stay in this process. Only game assemblies are collectible.
    static Library library = nullptr;
    if (!library) {
        auto host = findHost();
        if (!host.empty()) library = open(host);
    }
    if (!library) {
        native.log(1, "C# scripting requires a matching .NET 10 desktop runtime. Set DOTNET_ROOT or add dotnet to PATH.");
        return false;
    }
    auto init = reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>(symbol(library, "hostfxr_initialize_for_runtime_config"));
    auto get = reinterpret_cast<hostfxr_get_runtime_delegate_fn>(symbol(library, "hostfxr_get_runtime_delegate"));
    auto close = reinterpret_cast<hostfxr_close_fn>(symbol(library, "hostfxr_close"));
    if (!init || !get || !close) return fail("incompatible hostfxr", -1);
    hostfxr_handle context = nullptr;
    int rc = init(config.c_str(), nullptr, &context);
    if (rc < 0 || !context) return fail("cannot initialize " + utf8(config), rc);
    void* delegate = nullptr;
    rc = get(context, hdt_load_assembly_and_get_function_pointer, &delegate);
    close(context);
    if (rc < 0 || !delegate) return fail("cannot obtain CLR assembly loader", rc);
    auto load = reinterpret_cast<load_assembly_and_get_function_pointer_fn>(delegate);
    void* entry = nullptr;
    rc = load(assembly.c_str(), ALLIO_CLR_TEXT("AllioEngine.Bootstrap, Allio.Managed"),
              ALLIO_CLR_TEXT("Initialize"), UNMANAGEDCALLERSONLY_METHOD, nullptr, &entry);
    if (rc < 0 || !entry) return fail("cannot load " + utf8(assembly), rc);
    return reinterpret_cast<Start>(entry)(&native, &managed) == 0;
#else
    native.log(1, "C# scripting requires a Windows, Linux or macOS desktop runtime.");
    return false;
#endif
}
}
