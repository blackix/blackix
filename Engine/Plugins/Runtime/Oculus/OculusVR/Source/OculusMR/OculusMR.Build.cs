// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OculusMR : ModuleRules
	{
		public OculusMR(ReadOnlyTargetRules Target) : base(Target)
        {
			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"InputDevice",			// For IInputDevice.h
					"HeadMountedDisplay",	// For IMotionController.h
					"ImageWrapper"
				});

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"InputCore",
                    "Slate",
                    "SlateCore",
                    "RHI",
                    "RenderCore",
                    "ShaderCore",
                    "MediaAssets",
                    "HeadMountedDisplay",
					"OculusHMD",
					"OculusInput",
                    "OVRPlugin"
                });

			PrivateIncludePaths.AddRange(
				new string[] {
					// Relative to Engine\Plugins\Runtime\Oculus\OculusVR\Source
					"OculusHMD/Private",
                    "OculusInput/Private",
                    "../../../../../Source/Runtime/Renderer/Private",
					"../../../../../Source/Runtime/Engine/Classes/Components",
                    "../../../../../Source/Runtime/MediaAssets/Private",
                });

            if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
            {
				PublicDelayLoadDLLs.Add("OVRPlugin.dll");
				RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/Oculus/OVRPlugin/OVRPlugin/" + Target.Platform.ToString() + "/OVRPlugin.dll"));			
			}
		}
	}
}
