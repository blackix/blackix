// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Voice : ModuleRules
{
	public Voice(TargetInfo Target)
	{
		Definitions.Add("VOICE_PACKAGE=1");

		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/Online/Voice/Private",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core"
			}
			);

		if (Target.Platform == UnrealTargetPlatform.Win32 ||
			Target.Platform == UnrealTargetPlatform.Win64)
		{
			AddThirdPartyPrivateStaticDependencies(Target, "DirectSound");
		}

		AddThirdPartyPrivateStaticDependencies(Target, "libOpus");
    }
}


		



		
