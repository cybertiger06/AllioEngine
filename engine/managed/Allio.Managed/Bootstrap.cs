using System.Globalization;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Loader;

namespace AllioEngine;

// C ABI shared with engine/core/script/CSharpRuntime.cpp. Exceptions must never cross it.
[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeApi
{
    internal delegate* unmanaged[Cdecl]<int, nint, void> Log;
    internal delegate* unmanaged[Cdecl]<nint, float*, float*, float*, void> GetPosition;
    internal delegate* unmanaged[Cdecl]<nint, float, float, float, void> SetPosition;
    internal delegate* unmanaged[Cdecl]<nint, float, float, float, void> Rotate;
    internal delegate* unmanaged[Cdecl]<nint, int, int, int> Key;
    internal delegate* unmanaged[Cdecl]<nint, nint, nint, int, void> SetClass;
    internal delegate* unmanaged[Cdecl]<nint, nint, void> Focus;
}

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct ManagedApi
{
    internal delegate* unmanaged[Cdecl]<nint, int> Load;
    internal delegate* unmanaged[Cdecl]<nint, nint, int*, nint> Create;
    internal delegate* unmanaged[Cdecl]<nint, int, float, nint, void> Notify;
    internal delegate* unmanaged[Cdecl]<nint, void> Release;
    internal delegate* unmanaged[Cdecl]<void> Unload;
    internal delegate* unmanaged[Cdecl]<nint, nint, nint, void> Property;
    internal delegate* unmanaged[Cdecl]<nint, delegate* unmanaged[Cdecl]<nint, nint, void>, nint, void> Inspect;
}

internal sealed class GameContext(string assemblyPath) : AssemblyLoadContext(isCollectible: true)
{
    private readonly AssemblyDependencyResolver resolver = new(assemblyPath);
    protected override Assembly? Load(AssemblyName name)
    {
        if (name.Name == typeof(Behaviour).Assembly.GetName().Name) return typeof(Behaviour).Assembly;
        string? path = resolver.ResolveAssemblyToPath(name);
        if (path == null) return null;
        using var stream = File.OpenRead(path);
        return LoadFromStream(stream);
    }
    protected override nint LoadUnmanagedDll(string name) =>
        resolver.ResolveUnmanagedDllToPath(name) is string path ? LoadUnmanagedDllFromPath(path) : 0;
}

internal static unsafe class Bootstrap
{
    internal static NativeApi Native;
    private static GameContext? context;
    private static Assembly? game;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static int Initialize(NativeApi* native, ManagedApi* api)
    {
        Native = *native;
        *api = new() { Load = &Load, Create = &Create, Notify = &Notify, Release = &Release, Unload = &Unload, Property = &SetProperty, Inspect = &Inspect };
        return 0;
    }

    private static string Utf8(nint p) => Marshal.PtrToStringUTF8(p) ?? "";
    internal static void WriteLog(bool error, string message)
    {
        nint text = Marshal.StringToCoTaskMemUTF8(message);
        try { Native.Log(error ? 1 : 0, text); }
        finally { Marshal.FreeCoTaskMem(text); }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static int Load(nint path)
    {
        try
        {
            string file = Utf8(path);
            context = new GameContext(file);
            using var stream = File.OpenRead(file);
            game = context.LoadFromStream(stream);
            return 1;
        }
        catch (Exception e) { WriteLog(true, e.ToString()); ClearGame(); return 0; }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static nint Create(nint className, nint native, int* features)
    {
        try
        {
            string name = Utf8(className);
            Type? type = game!.GetType(name) ?? game.GetTypes().SingleOrDefault(t => t.Name == name && typeof(Behaviour).IsAssignableFrom(t));
            if (type == null || type.IsAbstract || !typeof(Behaviour).IsAssignableFrom(type))
                throw new TypeLoadException($"Cannot find concrete AllioEngine.Behaviour '{name}' in Allio.Game.dll.");
            var behaviour = (Behaviour)Activator.CreateInstance(type)!;
            bool Overrides(string method, Type parameter)
            {
                var actual = type.GetMethod(method, [parameter]);
                return actual?.GetBaseDefinition().DeclaringType == typeof(Behaviour) &&
                       actual.DeclaringType != typeof(Behaviour);
            }
            *features = (Overrides(nameof(Behaviour.Update), typeof(float)) ? 1 : 0) |
                        (Overrides(nameof(Behaviour.OnClick), typeof(string)) ? 2 : 0);
            behaviour.NativeInstance = native;
            behaviour.OwnerThread = Environment.CurrentManagedThreadId;
            return GCHandle.ToIntPtr(GCHandle.Alloc(behaviour));
        }
        catch (Exception e) { WriteLog(true, e.ToString()); return 0; }
    }

    private static Behaviour Object(nint id) => (Behaviour)GCHandle.FromIntPtr(id).Target!;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void Notify(nint id, int message, float dt, nint text)
    {
        try
        {
            var obj = Object(id);
            switch (message)
            {
                case 0: obj.Awake(); break;
                case 1: obj.OnEnable(); break;
                case 2: obj.Start(); break;
                case 3: obj.Update(dt); break;
                case 4: obj.OnDisable(); break;
                case 5: obj.OnDestroy(); break;
                case 6: obj.OnClick(Utf8(text)); break;
            }
        }
        catch (Exception e) { WriteLog(true, e.ToString()); }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void SetProperty(nint id, nint name, nint value)
    {
        try
        {
            var obj = Object(id);
            string member = Utf8(name), text = Utf8(value);
            var field = obj.GetType().GetField(member, BindingFlags.Public | BindingFlags.Instance);
            if (field != null && !field.IsInitOnly)
                field.SetValue(obj, Convert.ChangeType(text, field.FieldType, CultureInfo.InvariantCulture));
        }
        catch (Exception e) { WriteLog(true, e.ToString()); }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void Release(nint id)
    {
        var handle = GCHandle.FromIntPtr(id);
        ((Behaviour)handle.Target!).NativeInstance = 0;
        handle.Free();
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void Inspect(nint id, delegate* unmanaged[Cdecl]<nint, nint, void> receive, nint output)
    {
        try
        {
            var obj = Object(id);
            var fields = new Dictionary<string, object?>();
            foreach (var field in obj.GetType().GetFields(BindingFlags.Public | BindingFlags.Instance))
            {
                var type = field.FieldType;
                if (type != typeof(bool) && type != typeof(int) && type != typeof(float) && type != typeof(string)) continue;
                object? value = field.GetValue(obj);
                if (value is float f && !float.IsFinite(f)) value = f.ToString(CultureInfo.InvariantCulture);
                if (value is string s && s.Length > 4096) value = s[..4096];
                fields[field.Name] = value;
                if (fields.Count == 100) break;
            }
            string json = System.Text.Json.JsonSerializer.Serialize(fields);
            nint text = Marshal.StringToCoTaskMemUTF8(json);
            try { receive(text, output); }
            finally { Marshal.FreeCoTaskMem(text); }
        }
        catch (Exception e) { WriteLog(true, e.ToString()); }
    }

    private static void ClearGame()
    {
        game = null;
        context?.Unload();
        context = null;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void Unload() => ClearGame();
}
