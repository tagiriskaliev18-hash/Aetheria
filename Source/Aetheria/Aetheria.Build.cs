using UnrealBuildTool;

public class Aetheria : ModuleRules
{
    public Aetheria(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "NetCore",
            "UMG",
            "Slate",
            "SlateCore",
            "AIModule",
            "ImageWrapper",
            "Json",
            "AnimGraphRuntime",
            "NavigationSystem",
            "AssetRegistry"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "RenderCore",
            "RHI",
            "Niagara"
        });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "MaterialEditor",
                "AssetRegistry",
                "AssetTools"
            });
        }
    }
}
