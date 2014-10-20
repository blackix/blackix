// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NiagaraEditor : ModuleRules
{
	public NiagaraEditor(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Engine", 
				"Core", 
				"CoreUObject", 
                "InputCore",
				"RenderCore",
				"Slate", 
				"SlateCore",
				"Kismet",
                "EditorStyle",
				"UnrealEd", 
				"GraphEditor", 
				"VectorVM"
			}
		);

        PublicDependencyModuleNames.AddRange(
            new string[] {
				"Engine"
            }
        );

        PublicIncludePathModuleNames.AddRange(
            new string[] {
				"Engine", 
				"Messaging", 
				"GraphEditor", 
				"LevelEditor"}
                );

		DynamicallyLoadedModuleNames.Add("WorkspaceMenuStructure");
	}
}
