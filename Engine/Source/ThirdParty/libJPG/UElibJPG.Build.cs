﻿// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UElibJPG : ModuleRules
{
	public UElibJPG(TargetInfo Target)
    {
        Type = ModuleType.External;

		string libJPGPath = UEBuildConfiguration.UEThirdPartyDirectory + "libJPG";
		PublicIncludePaths.Add(libJPGPath);

        // cpp files being used like header files in implementation
        PublicAdditionalShadowFiles.Add(libJPGPath + "/jpgd.cpp");
        PublicAdditionalShadowFiles.Add(libJPGPath + "/jpge.cpp");
    }
}

