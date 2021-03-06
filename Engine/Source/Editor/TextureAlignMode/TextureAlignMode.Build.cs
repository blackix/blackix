// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TextureAlignMode : ModuleRules
{
	public TextureAlignMode(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"UnrealEd",
				"RenderCore",
				"LevelEditor"
			}
			);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "BspMode"
			}
        );
	}
}
