using AllioEngine;

public sealed class Rotator : Behaviour
{
    public float degreesPerSecond = 45f;

    public override void Update(float deltaTime)
    {
        Rotate(0f, degreesPerSecond * deltaTime, 0f);
    }
}
