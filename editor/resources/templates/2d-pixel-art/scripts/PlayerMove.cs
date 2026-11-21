using AllioEngine;
using System.Numerics;

public sealed class PlayerMove : Behaviour
{
    public float speed = 300f;

    public override void Update(float deltaTime)
    {
        float x = 0f;
        float y = 0f;
        if (IsKeyDown(Key.Left) || IsKeyDown(Key.A)) x -= 1f;
        if (IsKeyDown(Key.Right) || IsKeyDown(Key.D)) x += 1f;
        if (IsKeyDown(Key.Up) || IsKeyDown(Key.W)) y += 1f;
        if (IsKeyDown(Key.Down) || IsKeyDown(Key.S)) y -= 1f;
        if (x != 0f || y != 0f)
            Position += new Vector3(x, y, 0f) * (speed * deltaTime);
    }
}
