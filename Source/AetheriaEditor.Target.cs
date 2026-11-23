using UnrealBuildTool;
using System.Collections.Generic;

public class AetheriaEditorTarget : TargetRules
{
    public AetheriaEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("Aetheria");
    }
}
