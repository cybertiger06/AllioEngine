using AllioEngine;

public sealed class MainMenuUI : Behaviour
{
    private readonly string[] buttons = { "play", "settings", "credits", "quit" };
    private int focused = -1;
    private bool settingsOpen;

    public override void OnClick(string elementId)
    {
        if (elementId == "settings-back")
        {
            ShowSettings(false);
            return;
        }

        int index = System.Array.IndexOf(buttons, elementId);
        if (index < 0) return;
        focused = index;
        Log("main_menu: " + elementId);
        if (elementId == "settings") ShowSettings(true);
    }

    public override void Update(float deltaTime)
    {
        if (settingsOpen)
        {
            if (IsKeyPressed(Key.Escape)) ShowSettings(false);
            return;
        }

        int step = IsKeyPressed(Key.Down) ? 1 : IsKeyPressed(Key.Up) ? -1 : 0;
        if (step != 0)
        {
            focused = focused < 0 ? 0 : (focused + step + buttons.Length) % buttons.Length;
            FocusElement(buttons[focused]);
        }
        else if (focused < 0 && IsKeyPressed(Key.Enter))
        {
            OnClick("play");
        }
    }

    private void ShowSettings(bool open)
    {
        settingsOpen = open;
        SetElementClass("settings-panel", "open", open);
        FocusElement(open ? "settings-back" : "settings");
    }
}
