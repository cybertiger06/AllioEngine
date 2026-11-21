#pragma once

#include <filesystem>

namespace jasperengine::managed {
    // Keep the layout and cdecl calls in sync with Allio.Managed/Bootstrap.cs.
    struct NativeApi {
        void (*log)(int error, const char* text);
        void (*getPosition)(void*, float*, float*, float*);
        void (*setPosition)(void*, float, float, float);
        void (*rotate)(void*, float, float, float);
        int (*key)(void*, int, int);
        void (*setClass)(void*, const char*, const char*, int);
        void (*focus)(void*, const char*);
    };

    struct ManagedApi {
        int (*load)(const char* gameAssembly);
        void* (*create)(const char* className, void* nativeInstance, int* features);
        void (*notify)(void*, int event, float deltaTime, const char* text);
        void (*release)(void*);
        void (*unload)();
        void (*property)(void*, const char* name, const char* value);
        void (*inspect)(void*, void (*receive)(const char*, void*), void* output);
    };

    bool initialize(const std::filesystem::path& directory, const NativeApi& native, ManagedApi& managed);
    std::filesystem::path defaultAssemblyDirectory();
}
