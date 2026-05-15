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
            "UMG",
            "Slate",
            "SlateCore",
            "NavigationSystem" // <--- днаюбэ щрн дкъ наундю ярем
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });
    }
}