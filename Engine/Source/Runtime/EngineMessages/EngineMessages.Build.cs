// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class EngineMessages : ModuleRules
	{
		public EngineMessages(TargetInfo Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[]
				{
					"Editor/EngineMessages/Private",
				}
			);
		}
	}
}
