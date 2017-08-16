// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OculusMRModule.h"
#include "OculusHMDModule.h"

#define LOCTEXT_NAMESPACE "OculusMR"

FOculusMRModule::FOculusMRModule()
{
	bInitialized = false;
}

void FOculusMRModule::StartupModule()
{
#if OCULUS_MR_SUPPORTED_PLATFORMS
	if (FOculusHMDModule::Get().PreInit() && OVRP_SUCCESS(ovrp_InitializeMixedReality()))
	{
		bInitialized = true;
	}
#endif
}

void FOculusMRModule::ShutdownModule()
{
#if OCULUS_MR_SUPPORTED_PLATFORMS
	if (bInitialized)
	{
		ovrp_ShutdownMixedReality();
	}
#endif
}

IMPLEMENT_MODULE( FOculusMRModule, OculusMR )

#undef LOCTEXT_NAMESPACE
