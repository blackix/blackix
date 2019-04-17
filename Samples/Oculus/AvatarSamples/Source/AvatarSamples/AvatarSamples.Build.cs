// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System;
using System.IO;

public class AvatarSamples : ModuleRules
{
	public AvatarSamples(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivatePCHHeaderFile = "AvatarSamples.h";

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Voice" });

		PrivateDependencyModuleNames.AddRange(new string[] {  "OculusAvatar", "OVRLipSync" });
    }
}
