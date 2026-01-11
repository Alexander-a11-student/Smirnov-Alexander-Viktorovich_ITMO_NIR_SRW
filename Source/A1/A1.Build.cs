using UnrealBuildTool;

public class A1 : ModuleRules
{
    public A1(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "UMG",        // <--- ÎÁßÇÀÒÅËÜÍÎ
            "Slate",      // <--- íóæåí UMG
            "SlateCore"   // <--- íóæåí UMG
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });
    }
}

