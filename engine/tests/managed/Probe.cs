using System.Numerics;
using AllioEngine;

public sealed class Probe : Behaviour
{
    private static int count;
    public float speed = 1;

    public override void Awake() => Log($"awake:{++count}");
    public override void OnEnable() => Log("enable");
    public override void Start() => Log("start");
    public override void Update(float dt) => Position += new Vector3(speed * dt, 0, 0);
    public override void OnClick(string id) => Log($"click:{id}");
    public override void OnDisable() => Log("disable");
    public override void OnDestroy() => Log("destroy");
}
