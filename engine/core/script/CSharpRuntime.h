#pragma once

#include "Export.h"
#include "ecs/Entity.h"
#include <filesystem>
#include <string>

namespace jasperengine {
    class Scene;
    struct ScriptEntry;

    class JASPERENGINE_API CSharpRuntime {
    public:
        // Call before a project's first scene wakes. The desktop player defaults to managed/
        // next to its executable. Assemblies and static fields live until the end of Play.
        static void setAssemblyDirectory(const std::filesystem::path& directory);
        static bool verifyAssembly(const std::filesystem::path& directory);
        static std::string inspect(const ScriptEntry& entry);
        static void create(Scene* scene, Entity entity, ScriptEntry& entry);
        static void release(ScriptEntry& entry);
        static void releaseScene(Scene* scene);
        static void endPlay();
    };
}
