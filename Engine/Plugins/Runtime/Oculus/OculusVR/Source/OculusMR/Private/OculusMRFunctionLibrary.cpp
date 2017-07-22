// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OculusMRFunctionLibrary.h"
#include "OculusMRModule.h"
#include "OculusHMD.h"
#include "OculusHMDPrivate.h"

//-------------------------------------------------------------------------------------------------
// UOculusFunctionLibrary
//-------------------------------------------------------------------------------------------------

UOculusMRFunctionLibrary::UOculusMRFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UOculusMRFunctionLibrary::GetAllTrackedCamera(TArray<FTrackedCamera>& TrackedCameras, bool bCalibratedOnly)
{
	TrackedCameras.Empty();

	if (!FOculusMRModule::IsAvailable() || !FOculusMRModule::Get().IsInitialized() )
	{
		UE_LOG(LogMR, Error, TEXT("OculusMR not available"));
		return;
	}

	if (OVRP_FAILURE(ovrp_UpdateExternalCamera()))
	{
		UE_LOG(LogMR, Log, TEXT("ovrp_UpdateExternalCamera failure"));
		return;
	}

	int cameraCount = 0;
	if (OVRP_FAILURE(ovrp_GetExternalCameraCount(&cameraCount)))
	{
		UE_LOG(LogMR, Log, TEXT("ovrp_GetExternalCameraCount failure"));
		return;
	}

	for (int i = 0; i < cameraCount; ++i)
	{
		char cameraName[OVRP_EXTERNAL_CAMERA_NAME_SIZE];
		ovrpCameraIntrinsics cameraIntrinsics;
		ovrpCameraExtrinsics cameraExtrinsics;
		ovrp_GetExternalCameraName(i, cameraName);
		ovrp_GetExternalCameraIntrinsics(i, &cameraIntrinsics);
		ovrp_GetExternalCameraExtrinsics(i, &cameraExtrinsics);
		if ((bCalibratedOnly == false || cameraExtrinsics.CameraStatus == ovrpCameraStatus_Calibrated) && cameraIntrinsics.IsValid && cameraExtrinsics.IsValid)
		{
			FTrackedCamera camera;
			camera.Index = i;
			camera.Name = cameraName;
			camera.FieldOfView = FMath::RadiansToDegrees(FMath::Atan(cameraIntrinsics.FOVPort.LeftTan) + FMath::Atan(cameraIntrinsics.FOVPort.RightTan));
			camera.SizeX = cameraIntrinsics.ImageSensorPixelResolution.w;
			camera.SizeY = cameraIntrinsics.ImageSensorPixelResolution.h;
			camera.AttachedTrackedDevice = OculusHMD::ToETrackedDeviceType(cameraExtrinsics.AttachedToNode);
			OculusHMD::FPose Pose;
			GetOculusHMD()->ConvertPose(cameraExtrinsics.RelativePose, Pose);
			camera.CalibratedRotation = Pose.Orientation.Rotator();
			camera.CalibratedOffset = Pose.Position;
			camera.UserRotation = FRotator::ZeroRotator;
			camera.UserOffset = FVector::ZeroVector;
			TrackedCameras.Add(camera);
		}
	}
}

OculusHMD::FOculusHMD* UOculusMRFunctionLibrary::GetOculusHMD()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	if (GEngine && GEngine->HMDDevice.IsValid())
	{
		auto HMDType = GEngine->HMDDevice->GetHMDDeviceType();
		if (HMDType == EHMDDeviceType::DT_OculusRift || HMDType == EHMDDeviceType::DT_GearVR)
		{
			return static_cast<OculusHMD::FOculusHMD*>(GEngine->HMDDevice.Get());
		}
	}
#endif
	return nullptr;
}

