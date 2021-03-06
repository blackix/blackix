﻿// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DirectSound : ModuleRules
{
	public DirectSound(TargetInfo Target)
	{
		Type = ModuleType.External;

		string DirectXSDKDir = UEBuildConfiguration.UEThirdPartyDirectory + "Windows/DirectX";
		PublicIncludePaths.Add( DirectXSDKDir + "/include");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicLibraryPaths.Add( DirectXSDKDir + "/Lib/x64");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicLibraryPaths.Add( DirectXSDKDir + "/Lib/x86");
		}

		PublicAdditionalLibraries.AddRange(
				new string[] {
 				"dxguid.lib",
 				"dsound.lib"
 				}
			);
	}
}

	