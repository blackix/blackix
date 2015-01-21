// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Slate : ModuleRules
{
	public Slate(TargetInfo Target)
	{
		SharedPCHHeaderFile = "Runtime/Slate/Public/SlateBasics.h";

		PublicDependencyModuleNames.AddRange(
			new string[] { 
				"Core",
				"CoreUObject",
				"InputCore",
				"Json",
				"SlateCore",
			}
		);

		if (Target.Type.Value != TargetRules.TargetType.Server && UEBuildConfiguration.bCompileFreeType)
        {
			AddThirdPartyPrivateStaticDependencies(Target, "FreeType2");
			Definitions.Add("WITH_FREETYPE=1");
		}
		else
		{
			Definitions.Add("WITH_FREETYPE=0");
		}

		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/Slate/Private",
				"Runtime/Slate/Private/Framework",
				"Runtime/Slate/Private/Framework/Application",
				"Runtime/Slate/Private/Framework/Commands",
				"Runtime/Slate/Private/Framework/Docking",
				"Runtime/Slate/Private/Framework/Layout",
				"Runtime/Slate/Private/Framework/MultiBox",
				"Runtime/Slate/Private/Framework/Notifications",
				"Runtime/Slate/Private/Framework/Styling",
				"Runtime/Slate/Private/Framework/Testing",
				"Runtime/Slate/Private/Framework/Text",
				"Runtime/Slate/Private/Framework/Text/IOS",
				"Runtime/Slate/Private/Framework/Text/Tests",
				"Runtime/Slate/Private/Framework/Widgets",
				"Runtime/Slate/Private/Widgets/Colors",
				"Runtime/Slate/Private/Widgets/Docking",
				"Runtime/Slate/Private/Widgets/Images",
				"Runtime/Slate/Private/Widgets/Input",
				"Runtime/Slate/Private/Widgets/Layout",
				"Runtime/Slate/Private/Widgets/Navigation",
				"Runtime/Slate/Private/Widgets/Notifications",
				"Runtime/Slate/Private/Widgets/Testing",
				"Runtime/Slate/Private/Widgets/Text",
				"Runtime/Slate/Private/Widgets/Tutorials",
				"Runtime/Slate/Private/Widgets/Views",
			}
		);

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			AddThirdPartyPrivateStaticDependencies(Target, "XInput");
		}
	}
}
