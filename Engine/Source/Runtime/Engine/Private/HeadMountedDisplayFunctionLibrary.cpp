// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "HeadMountedDisplay.h"

UHeadMountedDisplayFunctionLibrary::UHeadMountedDisplayFunctionLibrary(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

bool UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled()
{
	return GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed();
}

void UHeadMountedDisplayFunctionLibrary::GetOrientationAndPosition(FRotator& DeviceRotation, FVector& DevicePosition)
{
	if(GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		FQuat OrientationAsQuat;
		FVector Position(0.f);

		GEngine->HMDDevice->GetCurrentOrientationAndPosition(OrientationAsQuat, Position);

		DeviceRotation = OrientationAsQuat.Rotator();
		DevicePosition = Position;
	}
	else
	{
		DeviceRotation = FRotator::ZeroRotator;
		DevicePosition = FVector::ZeroVector;
	}
}

bool UHeadMountedDisplayFunctionLibrary::HasValidTrackingPosition()
{
	if(GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		return GEngine->HMDDevice->HasValidTrackingPosition();
	}

	return false;
}

void UHeadMountedDisplayFunctionLibrary::GetPositionalTrackingCameraParameters(FVector& CameraOrigin, FRotator& CameraOrientation, float& HFOV, float& VFOV, float& CameraDistance, float& NearPlane, float&FarPlane)
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed() && GEngine->HMDDevice->DoesSupportPositionalTracking())
	{
		GEngine->HMDDevice->GetPositionalTrackingCameraProperties(CameraOrigin, CameraOrientation, HFOV, VFOV, CameraDistance, NearPlane, FarPlane);
	}
	else
	{
		// No HMD, zero the values
		CameraOrigin = FVector::ZeroVector;
		CameraOrientation = FRotator::ZeroRotator;
		HFOV = VFOV = 0.f;
		NearPlane = 0.f;
		FarPlane = 0.f;
		CameraDistance = 0.f;
	}
}

bool UHeadMountedDisplayFunctionLibrary::IsInLowPersistenceMode()
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		return GEngine->HMDDevice->IsInLowPersistenceMode();
	}
	else
	{
		return false;
	}
}

void UHeadMountedDisplayFunctionLibrary::EnableLowPersistenceMode(bool Enable)
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		GEngine->HMDDevice->EnableLowPersistenceMode(Enable);
	}
}

void UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition(float Yaw)
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		GEngine->HMDDevice->ResetOrientationAndPosition(Yaw);
	}
}

void UHeadMountedDisplayFunctionLibrary::ResetOrientation(float Yaw)
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		GEngine->HMDDevice->ResetOrientation(Yaw);
	}
}

void UHeadMountedDisplayFunctionLibrary::ResetPosition()
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		GEngine->HMDDevice->ResetPosition();
	}
}

void UHeadMountedDisplayFunctionLibrary::SetClippingPlanes(float NCP, float FCP)
{
	if (GEngine->HMDDevice.IsValid())
	{
		GEngine->HMDDevice->SetClippingPlanes(NCP, FCP);
	}
}

void UHeadMountedDisplayFunctionLibrary::SetBaseRotation(const FRotator& BaseRot)
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		GEngine->HMDDevice->SetBaseRotation(BaseRot);
	}
}

void UHeadMountedDisplayFunctionLibrary::GetBaseRotation(FRotator& OutRot)
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		OutRot = GEngine->HMDDevice->GetBaseRotation();
	}
}

void UHeadMountedDisplayFunctionLibrary::SetBaseOrientation(const FQuat& BaseOrient)
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		GEngine->HMDDevice->SetBaseOrientation(BaseOrient);
	}
}

void UHeadMountedDisplayFunctionLibrary::GetBaseOrientation(FQuat& OutOrient)
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		OutOrient = GEngine->HMDDevice->GetBaseOrientation();
	}
}

void UHeadMountedDisplayFunctionLibrary::SetPositionOffset(const FVector& PosOffset)
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		GEngine->HMDDevice->SetPositionOffset(PosOffset);
	}
}

void UHeadMountedDisplayFunctionLibrary::GetPositionOffset(FVector& OutPosOffset)
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		OutPosOffset = GEngine->HMDDevice->GetPositionOffset();
	}
}
