// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HeadMountedDisplayPrivate.h"

/**
* HMD device console vars
*/
static TAutoConsoleVariable<int32> CVarHiddenAreaMask(
	TEXT("vr.HiddenAreaMask"),
	1,
	TEXT("0 to disable hidden area mask, 1 to enable."),
	ECVF_Scalability | ECVF_RenderThreadSafe);


static void SetTrackingOrigin(const TArray<FString>& Args)
{
	int Origin = 0;
	if (Args.Num())
	{
		Origin = FCString::Atoi(*Args[0]);
		if (GEngine && GEngine->HMDDevice.IsValid())
		{
			GEngine->HMDDevice->SetTrackingOrigin(IHeadMountedDisplay::ETrackingOrigin(Origin));
		}
	}
	else
	{
		if (GEngine && GEngine->HMDDevice.IsValid())
		{
			Origin = GEngine->HMDDevice->GetTrackingOrigin();
		}
		if (GLog)
		{
			GLog->Logf(ELogVerbosity::Display, TEXT("Tracking orgin is set to %d"), Origin);
		}
	}
}

static FAutoConsoleCommand CTrackingOriginCmd(
	TEXT("vr.SetTrackingOrigin"),
	TEXT("0 - tracking origin is at the eyes/head, 1 - tracking origin is at the floor."),
	FConsoleCommandWithArgsDelegate::CreateStatic(SetTrackingOrigin));

class FHeadMountedDisplayModule : public IHeadMountedDisplayModule
{
	virtual TSharedPtr< class IHeadMountedDisplay, ESPMode::ThreadSafe > CreateHeadMountedDisplay()
	{
		TSharedPtr<IHeadMountedDisplay, ESPMode::ThreadSafe> DummyVal = NULL;
		return DummyVal;
	}

	FString GetModulePriorityKeyName() const
	{
		return FString(TEXT("Default"));
	}
};

IMPLEMENT_MODULE( FHeadMountedDisplayModule, HeadMountedDisplay );

IHeadMountedDisplay::IHeadMountedDisplay()
{
}

void IHeadMountedDisplay::SetTrackingOrigin(ETrackingOrigin InOrigin)
{
	if (GLog)
	{
		GLog->Log(ELogVerbosity::Display, TEXT("Not implemented IHeadMountedDisplay::SetTrackingOrigin is called"));
	}
}

IHeadMountedDisplay::ETrackingOrigin IHeadMountedDisplay::GetTrackingOrigin()
{
	if (GLog)
	{
		GLog->Log(ELogVerbosity::Display, TEXT("Not implemented IHeadMountedDisplay::GetTrackingOrigin is called"));
	}
	return Eye;
}

bool IHeadMountedDisplay::DoesAppUseVRFocus() const
{
	return FApp::UseVRFocus();
}

bool IHeadMountedDisplay::DoesAppHaveVRFocus() const
{
	return FApp::HasVRFocus();
}


