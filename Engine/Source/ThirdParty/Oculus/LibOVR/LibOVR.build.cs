// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LibOVR : ModuleRules
{
	// Version of LibOVR
	public const int LibOVR_Major = 0;
	public const int LibOVR_Minor = 6;
	public const int LibOVR_Patch = 0;

	public LibOVR(TargetInfo Target)
	{
		Type = ModuleType.External;

// MAKE A SYMLINK OF THE 0.6 LibOVR from Oculus repository as X:\LibOVR_0_6_0, where 'X' is the same drive as the Unreal repo is located on.
// This will allow to use the latest 0.6 LibOVR every time automatically.
// Make sure the latest LibOVRRT64_0_6.dll is located in the PATH or in Engine/Binaries/Win64 directory.
// Alternatively, you may set the LIBOVR_DLL_DIR to the directory that contains the proper LibOVRRT64_0_6.dll.

        PublicIncludePaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "Oculus/LibOVR/"+
							   "/LibOVR_" + LibOVR_Major + "_" +
							   LibOVR_Minor + "_" + LibOVR_Patch+"/Include");
        PublicIncludePaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "Oculus/LibOVR/"+
							   "/LibOVR_" + LibOVR_Major + "_" +
							   LibOVR_Minor + "_" + LibOVR_Patch+"/Src");
	}
}
