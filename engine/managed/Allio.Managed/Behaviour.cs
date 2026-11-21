using System.Numerics;
using System.Runtime.InteropServices;

namespace AllioEngine;

public enum Key
{
    Space = 32, A = 65, D = 68, S = 83, W = 87,
    Escape = 256, Enter = 257, Right = 262, Left = 263, Down = 264, Up = 265
}

/// <summary>A script attached to one scene entity. Engine operations run on the game thread.</summary>
public abstract unsafe class Behaviour
{
    internal nint NativeInstance;
    internal int OwnerThread;
    private nint Instance => NativeInstance != 0 && OwnerThread == Environment.CurrentManagedThreadId
        ? NativeInstance : throw new InvalidOperationException("Access the entity from a live script callback on the game thread.");

    public virtual void Awake() {}
    public virtual void OnEnable() {}
    public virtual void Start() {}
    public virtual void Update(float deltaTime) {}
    public virtual void OnDisable() {}
    public virtual void OnDestroy() {}
    public virtual void OnClick(string elementId) {}

    public Vector3 Position
    {
        get
        {
            Vector3 value = default;
            Bootstrap.Native.GetPosition(Instance, &value.X, &value.Y, &value.Z);
            return value;
        }
        set => Bootstrap.Native.SetPosition(Instance, value.X, value.Y, value.Z);
    }

    /// <summary>Applies a local-space rotation in degrees.</summary>
    public void Rotate(float xDegrees, float yDegrees, float zDegrees) =>
        Bootstrap.Native.Rotate(Instance, xDegrees, yDegrees, zDegrees);

    /// <summary>True each frame the key is down.</summary>
    public bool IsKeyDown(Key key) => Bootstrap.Native.Key(Instance, (int)key, 0) != 0;
    /// <summary>True on the frame the key was pressed.</summary>
    public bool IsKeyPressed(Key key) => Bootstrap.Native.Key(Instance, (int)key, 1) != 0;
    public void Log(string message)
    {
        _ = Instance;
        Bootstrap.WriteLog(false, message);
    }

    public void SetElementClass(string id, string className, bool enabled)
    {
        var self = Instance;
        var element = Marshal.StringToCoTaskMemUTF8(id);
        try
        {
            var name = Marshal.StringToCoTaskMemUTF8(className);
            try { Bootstrap.Native.SetClass(self, element, name, enabled ? 1 : 0); }
            finally { Marshal.FreeCoTaskMem(name); }
        }
        finally { Marshal.FreeCoTaskMem(element); }
    }

    public void FocusElement(string id)
    {
        var self = Instance;
        var element = Marshal.StringToCoTaskMemUTF8(id);
        try { Bootstrap.Native.Focus(self, element); }
        finally { Marshal.FreeCoTaskMem(element); }
    }
}
