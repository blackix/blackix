// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CoreUObject : ModuleRules
{
	public CoreUObject(TargetInfo Target)
	{
		SharedPCHHeaderFile = "Runtime/CoreUObject/Public/CoreUObject.h";

		PrivateIncludePaths.Add("Runtime/CoreUObject/Private");

		PrivateIncludePathModuleNames.Add("TargetPlatform");

		PublicDependencyModuleNames.Add("Core");

		PrivateDependencyModuleNames.Add("Projects");
	}
}
