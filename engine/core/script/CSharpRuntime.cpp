#include "CSharpRuntime.h"
#include "CSharpHost.h"
#include "ScriptRuntime.h"
#include "ScriptBase.h"
#include "Engine.h"
#include "Input.h"
#include "Log.h"
#include "util/CrashHook.h"
#include "util/NumberText.h"
#include "component/Transform.h"
#include "object/GameObject.h"
#if JASPERENGINE_HTMLUI
#include "component/UIDocumentComponent.h"
#include "object/ui/UIDocument.h"
#endif

#include <charconv>
#include <memory>
#include <numbers>
#include <unordered_set>

namespace jasperengine {
namespace csharp {
    namespace fs = std::filesystem;
    fs::path outputDirectory;
    managed::ManagedApi api{};
    bool gameLoaded = false;
    class ManagedScript;
    std::unordered_set<ManagedScript*> instances;

    enum Event { Awake, Enable, Start, Update, Disable, Destroy, Click };

    class ManagedScript final : public ScriptBase {
    public:
        using ScriptBase::ScriptBase;
        void* handle = nullptr;
        int features = 0;
#if JASPERENGINE_HTMLUI
        UIElement body;
        uint64_t listener = 0;

        void connectClick() {
            if (!(features & 2) || !scene->findComponent<UIDocumentComponent>(entity)) return;
            UIElement current = UIDocument(scene, entity).getBody();
            if (body == current) return;
            if (listener && body.isValid()) body.removeEventListener(listener);
            body = current;
            listener = body.isValid() ? body.addEventListener("click", [this](UIEvent& e) {
                if (!isActiveAndEnabled() || !ScriptRuntime::findEntry(scene, entity, this)) return;
                for (UIElement target = e.getTarget(); target.isValid(); target = target.getParentElement()) {
                    const std::string id = target.getId();
                    if (!id.empty()) {
                        api.notify(handle, Click, 0.f, id.c_str());
                        return;
                    }
                }
            }) : 0;
        }
#endif
        void init() override {
            if (features) REGISTER_ENGINE_EVENT(onUpdate);
            api.notify(handle, Awake, 0.f, nullptr);
        }
        void onEnable() override { api.notify(handle, Enable, 0.f, nullptr); }
        void start() override {
#if JASPERENGINE_HTMLUI
            connectClick();
#endif
            api.notify(handle, Start, 0.f, nullptr);
        }
        void onUpdate() {
            // The inspector can detach and undo a script while a scene is playing.
            if (!ScriptRuntime::findEntry(scene, entity, this)) return;
#if JASPERENGINE_HTMLUI
            connectClick();
#endif
            if (features & 1) api.notify(handle, Update, Engine::getScriptDeltatime(), nullptr);
        }
        void onDisable() override { api.notify(handle, Disable, 0.f, nullptr); }
        void onDestroy() override { api.notify(handle, Destroy, 0.f, nullptr); }

        ~ManagedScript() override {
            if (features) UNREGISTER_ENGINE_EVENT(onUpdate);
#if JASPERENGINE_HTMLUI
            if (listener && body.isValid()) body.removeEventListener(listener);
#endif
            if (handle) api.release(handle);
        }
    };

    template<typename Function>
    void transform(void* self, Function fn) {
        auto* script = static_cast<ManagedScript*>(self);
        Scene* scene = script->getScene();
        Entity entity = script->getEntity();
        if (scene->isEntityCreated(entity) && scene->findComponent<Transform>(entity)) {
            GameObject object(scene, entity);
            fn(object);
        }
    }

#if JASPERENGINE_HTMLUI
    UIElement element(void* self, const char* id) {
        auto* script = static_cast<ManagedScript*>(self);
        Scene* scene = script->getScene();
        Entity entity = script->getEntity();
        if (!id || !scene->isEntityCreated(entity) || !scene->findComponent<UIDocumentComponent>(entity)) return {};
        return UIDocument(scene, entity).getElementById(id);
    }
#endif

    const managed::NativeApi native{
        [](int error, const char* text) {
            // The editor pauses and stops a failed script on the main thread. Loader and
            // reflection diagnostics also come from the Play build worker and only go to the log.
            if (error == 2 && CrashHook::report("C# script callback", text)) return;
            if (error) Log::error("C# script error: %s", text);
            else Log::print("%s", text);
        },
        [](void* self, float* x, float* y, float* z) {
            transform(self, [&](GameObject& object) {
                Vector3 position = object.getPosition();
                *x = position.x;
                *y = position.y;
                *z = position.z;
            });
        },
        [](void* self, float x, float y, float z) {
            transform(self, [&](GameObject& object) { object.setPosition(x, y, z); });
        },
        [](void* self, float x, float y, float z) {
            transform(self, [&](GameObject& object) {
                const float scale = Engine::isUseDegrees() ? 1.f : std::numbers::pi_v<float> / 180.f;
                object.setRotation((object.getRotation() * Quaternion(x * scale, y * scale, z * scale)).normalized());
            });
        },
        [](void*, int key, int edge) -> int {
            return edge ? Input::getKeyDown(key) : Input::getKey(key);
        },
        [](void* self, const char* id, const char* name, int enabled) {
#if JASPERENGINE_HTMLUI
            UIElement target = element(self, id);
            if (name && target.isValid()) target.setClass(name, enabled != 0);
#endif
        },
        [](void* self, const char* id) {
#if JASPERENGINE_HTMLUI
            UIElement target = element(self, id);
            if (target.isValid()) target.focus();
#endif
        }
    };

    bool ensureLoaded() {
        if (gameLoaded) return true;
        const auto directory = outputDirectory.empty() ? managed::defaultAssemblyDirectory() : outputDirectory;
        if (!api.load && !managed::initialize(directory, native, api)) return false;
        const auto filename = fs::absolute(directory / "Allio.Game.dll").u8string();
        gameLoaded = api.load(reinterpret_cast<const char*>(filename.c_str())) != 0;
        return gameLoaded;
    }

    void unloadIfUnused() {
        if (gameLoaded && instances.empty()) {
            api.unload();
            gameLoaded = false;
        }
    }

    void setProperty(void* script, const ScriptProperty& prop) {
        if (auto value = std::get_if<std::string>(&prop.value)) {
            api.property(script, prop.name.c_str(), value->c_str());
        } else if (auto value = std::get_if<bool>(&prop.value)) {
            api.property(script, prop.name.c_str(), *value ? "true" : "false");
        } else if (auto value = std::get_if<int>(&prop.value)) {
            api.property(script, prop.name.c_str(), std::to_string(*value).c_str());
        } else if (auto value = std::get_if<float>(&prop.value)) {
#if (defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 130300) || \
    (defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ < 160300)
            const std::string text = NumberText::formatFloat(*value);
            api.property(script, prop.name.c_str(), text.c_str());
#else
            char buffer[64]{};
            auto result = std::to_chars(buffer, buffer + sizeof(buffer) - 1, *value);
            if (result.ec == std::errc{}) api.property(script, prop.name.c_str(), buffer);
#endif
        }
    }
}

void CSharpRuntime::setAssemblyDirectory(const std::filesystem::path& directory) {
    if (!csharp::instances.empty()) {
        Log::error("Stop all active C# scripts before changing the project's assembly directory.");
        return;
    }
    csharp::unloadIfUnused();
    csharp::outputDirectory = directory;
}

bool CSharpRuntime::verifyAssembly(const std::filesystem::path& directory) {
    if (!csharp::instances.empty()) return false;
    setAssemblyDirectory(directory);
    const bool ready = csharp::ensureLoaded();
    csharp::unloadIfUnused();
    return ready;
}

std::string CSharpRuntime::inspect(const ScriptEntry& entry) {
    std::string json;
    if (entry.type == ScriptType::CSHARP && entry.instance) {
        auto* script = static_cast<csharp::ManagedScript*>(entry.instance);
        csharp::api.inspect(script->handle, [](const char* text, void* out) {
            *static_cast<std::string*>(out) = text;
        }, &json);
    }
    return json;
}

void CSharpRuntime::create(Scene* scene, Entity entity, ScriptEntry& entry) {
    if (entry.instance || entry.type != ScriptType::CSHARP || !csharp::ensureLoaded()) return;
    auto script = std::make_unique<csharp::ManagedScript>(scene, entity);
    script->handle = csharp::api.create(entry.className.c_str(), script.get(), &script->features);
    if (!script->handle) return;
    for (const ScriptProperty& property : entry.properties) csharp::setProperty(script->handle, property);
    entry.cppMessageMask = scriptMessageBit(ScriptMessage::Init) | scriptMessageBit(ScriptMessage::Start) |
        scriptMessageBit(ScriptMessage::Enable) | scriptMessageBit(ScriptMessage::Disable) | scriptMessageBit(ScriptMessage::Destroy);
    entry.cppScript = script.get();
    csharp::instances.insert(script.get());
    entry.instance = script.release();
}

void CSharpRuntime::release(ScriptEntry& entry) {
    if (entry.type != ScriptType::CSHARP || !entry.instance) return;
    auto* script = static_cast<csharp::ManagedScript*>(entry.instance);
    if (csharp::instances.erase(script)) {
        ScriptRuntime::forgetInstance(script);
        delete script;
    }
    entry.instance = nullptr;
    entry.cppScript = nullptr;
    entry.cppMessageMask = 0;
}

void CSharpRuntime::releaseScene(Scene* scene) {
    // Commands can replace a ScriptComponent during Play. Release instances removed from
    // the component as well, before deleting the scene their native callbacks refer to.
    for (auto it = csharp::instances.begin(); it != csharp::instances.end();) {
        csharp::ManagedScript* script = *it;
        ++it;
        if (script->getScene() != scene) continue;
        if (ScriptEntry* entry = ScriptRuntime::findEntry(scene, script->getEntity(), script)) {
            release(*entry);
        } else {
            csharp::instances.erase(script);
            ScriptRuntime::forgetInstance(script);
            delete script;
        }
    }
}

void CSharpRuntime::endPlay() {
    csharp::unloadIfUnused();
}
}
