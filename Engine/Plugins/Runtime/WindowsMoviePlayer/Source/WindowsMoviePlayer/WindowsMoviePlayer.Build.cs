// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class WindowsMoviePlayer : ModuleRules
	{
        public WindowsMoviePlayer(TargetInfo Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/WindowsMoviePlayer/Private",
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject", // @todo Mac: for some reason CoreUObject and Engine are needed to link in debug on Mac
					"Engine", // @todo Mac: for some reason CoreUObject and Engine are needed to link in debug on Mac
                    "InputCore",
                    "MoviePlayer",
                    "RenderCore",
                    "RHI",
                    "Slate"
				}
				);

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
				}
				);

			if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
			{
				PublicDelayLoadDLLs.Add("shlwapi.dll");
				PublicDelayLoadDLLs.Add("mf.dll");
				PublicDelayLoadDLLs.Add("mfplat.dll");
				PublicDelayLoadDLLs.Add("mfplay.dll");
				PublicDelayLoadDLLs.Add("mfuuid.dll");
			}
		}
	}
}