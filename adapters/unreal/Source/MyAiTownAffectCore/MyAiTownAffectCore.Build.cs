using UnrealBuildTool;

public class MyAiTownAffectCore : ModuleRules
{
    public MyAiTownAffectCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "DeveloperSettings" });
    }
}
