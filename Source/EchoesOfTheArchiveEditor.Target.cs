using UnrealBuildTool;
using System.Collections.Generic;
public class EchoesOfTheArchiveEditorTarget : TargetRules
{
    public EchoesOfTheArchiveEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V2;
        ExtraModuleNames.Add("EchoesOfTheArchive");
        if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            bOverrideBuildEnvironment = true;
            bUseSharedPCHs = false;
            AdditionalCompilerArguments = " -Wno-error -Wno-deprecated-builtins -Wno-deprecated-declarations -fno-access-control";
            AdditionalLinkerArguments = " -rpath /usr/lib/swift";
        }
    }
}
