# C# gameplay scripts

Use **Assets > Create > C# Script** to create a file. Drag it onto a GameObject, or use **Inspector > Add Script** to create and attach it in one step. From MCP, call:

```json
{"name":"create_script","arguments":{"kind":"csharp","name":"Spinner","directory":"scripts","entity":"Cube"}}
```

The class in `Spinner.cs` derives from `AllioEngine.Behaviour`. Public scalar fields with simple literal defaults are editable on each instance in the Inspector.

```csharp
using AllioEngine;

public sealed class Spinner : Behaviour
{
    public float degreesPerSecond = 45f;

    public override void Update(float deltaTime)
    {
        Rotate(0f, degreesPerSecond * deltaTime, 0f);
    }
}
```

**Play** compiles the project with the .NET 10 SDK. The Console reports compilation and script errors. Stop and Play again after editing source. Classes can also override `Awake()`, `OnEnable()`, `Start()`, `OnDisable()` and `OnDestroy()`. Local `Position` uses `System.Numerics.Vector3`. Check held keys with `IsKeyDown(Key.Left)` and press events with `IsKeyPressed(Key.Space)`.

For an HTML UI, attach the behaviour to the GameObject that owns the UI Document. In `OnClick(string elementId)`, match the HTML element's id. For example, a Settings button can call `SetElementClass("settings-panel", "open", true)` and `FocusElement("settings-back")`. The UI project template contains a complete example.

C# builds target desktop. Install the matching .NET 10 runtime on the player's computer and keep the exported managed directory beside the executable. Existing Lua assets use the [legacy Lua guide](lua-scripting.md).
