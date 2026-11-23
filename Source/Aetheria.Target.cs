using UnrealBuildTool;
using System.Collections.Generic;

public class AetheriaTarget : TargetRules
{
    public AetheriaTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("Aetheria");
    }
}
