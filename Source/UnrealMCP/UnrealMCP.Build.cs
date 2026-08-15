using UnrealBuildTool;

public class UnrealMCP : ModuleRules
{
    public UnrealMCP(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[] {
            }
        );

        PrivateIncludePaths.AddRange(
            new string[] {
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "Networking",
                "Sockets",
                "Json",
                "JsonUtilities"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Projects",
                "Slate",
                "SlateCore",
                "UMG",
                "RenderCore",
                "CinematicCamera",
                "ImageWrapper"
            }
        );

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Kismet",
                    "BlueprintGraph",
                    "MaterialEditor",
                    "UnrealEd",
                    "LevelEditor",
                    "EditorSubsystem",
                    "EditorScriptingUtilities",
                    "AssetRegistry",
                    "AssetTools",
                    "UMGEditor",
                    "MovieScene",
                    "MovieSceneTracks",
                    "Hexagon",
                    "Niagara",
                    "NiagaraCore"
                }
            );
        }

        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
            }
        );
    }
}
