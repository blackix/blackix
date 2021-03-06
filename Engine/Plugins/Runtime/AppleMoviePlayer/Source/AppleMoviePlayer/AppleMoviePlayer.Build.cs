// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AppleMoviePlayer : ModuleRules
	{
        public AppleMoviePlayer(TargetInfo Target)
		{
			PrivateIncludePaths.Add("Runtime/AppleMoviePlayer/Private");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				    "CoreUObject",
				    "Engine",
                    "MoviePlayer",
                    "RenderCore",
                    "RHI",
                    "Slate"
				}
				);
				
			if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PublicFrameworks.AddRange(new string[] { "QuartzCore" });
			}
		}
	}
}
