# C# gameplay scripts

Use **Assets > Create > C# Script** to create a file. Drag it onto a GameObject, or use **Inspector > Add Script** to create and attach it in one step. The same operation is available through MCP.

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

Use `check_script` with `{"path":"scripts/Spinner.cs"}` to compile all project scripts in the background using the .NET 10 SDK. For a proposed edit, pass the same path and a `code` string. To check an additional source, pass `code` and `"language":"csharp"`. The temporary compilation keeps the project's files and active Play session intact.

The result includes `ok`, `errors` and `warnings`. Each Roslyn finding includes its code, message, file path, line and column (`col`). Pass `"wait":false` to return immediately. If a check is still running after the default 15-second wait, it also returns a `job_id`. Poll `get_job` with that id until the status is `succeeded`, `failed` or `cancelled`. On success, read `result.ok` and `result.errors` to distinguish clean code from compiler diagnostics.

**Play** compiles the project with the .NET 10 SDK. The Console reports compilation and script errors. Stop and Play again after editing source. Classes can also override `Awake()`, `OnEnable()`, `Start()`, `OnDisable()` and `OnDestroy()`. Local `Position` uses `System.Numerics.Vector3`. Check held keys with `IsKeyDown(Key.Left)` and press events with `IsKeyPressed(Key.Space)`.

For an HTML UI, attach the behaviour to the GameObject that owns the UI Document. In `OnClick(string elementId)`, match the HTML element's id. For example, a Settings button can call `SetElementClass("settings-panel", "open", true)` and `FocusElement("settings-back")`. The UI project template contains a complete example.

C# builds target desktop. Install the matching .NET 10 runtime on the player's computer and keep the exported managed directory beside the executable. Existing Lua assets use the [legacy Lua guide](lua-scripting.md).
