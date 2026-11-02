using UnrealBuildTool;
public class EchoesOfTheArchive : ModuleRules
{
    public EchoesOfTheArchive(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {"Core", "CoreUObject", "Engine", "InputCore"});
    }
}
