// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Kismet/HeadMountedDisplayFunctionLibrary.h"
#include "HeadMountedDisplay.h"

UHeadMountedDisplayFunctionLibrary::UHeadMountedDisplayFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled()
{
	return GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed();
}

bool UHeadMountedDisplayFunctionLibrary::EnableHMD(bool bEnable)
{
	if (GEngine->HMDDevice.IsValid())
	{
		GEngine->HMDDevice->EnableHMD(bEnable);
		if (bEnable)
		{
			return GEngine->HMDDevice->EnableStereo(true);
		}
		else
		{
			GEngine->HMDDevice->EnableStereo(false);
			return true;
		}
	}
	return false;
}

void UHeadMountedDisplayFunctionLibrary::GetOrientationAndPosition(FRotator& DeviceRotation, FVector& DevicePosition, bool bUseOrienationForPlayerCamera, bool bUsePositionForPlayerCamera, const FVector PositionScale)
{
	if(GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		FQuat OrientationAsQuat;
		FVector Position(0.f);

		GEngine->HMDDevice->GetCurrentOrientationAndPosition(OrientationAsQuat, Position, bUseOrienationForPlayerCamera, bUsePositionForPlayerCamera, PositionScale);

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

void UHeadMountedDisplayFunctionLibrary::EnableLowPersistenceMode(bool bEnable)
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		GEngine->HMDDevice->EnableLowPersistenceMode(bEnable);
	}
}

void UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition(float Yaw, EOrientPositionSelector::Type Options)
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		switch (Options)
		{
		case EOrientPositionSelector::Orientation:
			GEngine->HMDDevice->ResetOrientation(Yaw);
			break;
		case EOrientPositionSelector::Position:
			GEngine->HMDDevice->ResetPosition();
			break;
		default:
			GEngine->HMDDevice->ResetOrientationAndPosition(Yaw);
		}
	}
}

void UHeadMountedDisplayFunctionLibrary::SetClippingPlanes(float Near, float Far)
{
	if (GEngine->HMDDevice.IsValid())
	{
		GEngine->HMDDevice->SetClippingPlanes(Near, Far);
	}
}

void UHeadMountedDisplayFunctionLibrary::SetBaseRotationAndPositionOffset(const FRotator& Rotation, const FVector& PositionOffset, EOrientPositionSelector::Type Options)
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		if (Options == EOrientPositionSelector::Orientation || EOrientPositionSelector::OrientationAndPosition)
		{
			GEngine->HMDDevice->SetBaseRotation(Rotation);
		}
		if (Options == EOrientPositionSelector::Position || EOrientPositionSelector::OrientationAndPosition)
		{
			GEngine->HMDDevice->SetBaseOffset(PositionOffset);
		}
	}
}

void UHeadMountedDisplayFunctionLibrary::GetBaseRotationAndPositionOffset(FRotator& Rotation, FVector& PositionOffset)
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		Rotation = GEngine->HMDDevice->GetBaseRotation();
		PositionOffset = GEngine->HMDDevice->GetBaseOffset();
	}
}

void UHeadMountedDisplayFunctionLibrary::GetRawSensorData(FVector& Accelerometer, FVector& Gyro, FVector& Magnetometer, float& Temperature, float& TimeInSeconds)
{
	if (GEngine->HMDDevice.IsValid())
	{
		IHeadMountedDisplay::SensorData Data;
		GEngine->HMDDevice->GetRawSensorData(Data);

		Accelerometer	= Data.Accelerometer;
		Gyro			= Data.Gyro;
		Magnetometer	= Data.Magnetometer;
		Temperature		= Data.Temperature;
		TimeInSeconds	= Data.TimeInSeconds;
	}
}

bool UHeadMountedDisplayFunctionLibrary::GetUserProfile(FHmdUserProfile& Profile)
{
	if (GEngine->HMDDevice.IsValid())
	{
		IHeadMountedDisplay::UserProfile Data;
		if (GEngine->HMDDevice->GetUserProfile(Data))
		{
			Profile.Name = Data.Name;
			Profile.Gender = Data.Gender;
			Profile.PlayerHeight = Data.PlayerHeight;
			Profile.EyeHeight = Data.EyeHeight;
			Profile.IPD = Data.IPD;
			Profile.EyeToNeckDistance = Data.EyeToNeckDistance;
			Profile.ExtraFields.Reserve(Data.ExtraFields.Num());
			for (TMap<FString, FString>::TIterator It(Data.ExtraFields); It; ++It)
			{
				Profile.ExtraFields.Add(FHmdUserProfileField(*It.Key(), *It.Value()));
			}
			return true;
		}
	}
	return false;
}

/** 
 * Sets screen percentage to be used in VR mode.
 *
 * @param ScreenPercentage	(in) Specifies the screen percentage to be used in VR mode. Use 0.0f value to reset to default value.
 */
void UHeadMountedDisplayFunctionLibrary::SetScreenPercentage(float ScreenPercentage)
{
	if (GEngine->StereoRenderingDevice.IsValid())
	{
		GEngine->StereoRenderingDevice->SetScreenPercentage(ScreenPercentage);
	}
}

/** 
 * Returns screen percentage to be used in VR mode.
 *
 * @return (float)	The screen percentage to be used in VR mode.
 */
float UHeadMountedDisplayFunctionLibrary::GetScreenPercentage()
{
	if (GEngine->StereoRenderingDevice.IsValid())
	{
		return GEngine->StereoRenderingDevice->GetScreenPercentage();
	}
	return 0.0f;
}
