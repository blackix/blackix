// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OculusMRFunctionLibrary.h"
#include "OculusMRModule.h"
#include "OculusHMD.h"
#include "OculusHMDPrivate.h"

#include "GameFramework/PlayerController.h"

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

	if (ovrp_GetInitialized() == ovrpBool_False)
	{
		UE_LOG(LogMR, Error, TEXT("OVRPlugin not initialized"));
		return;
	}

	if (OVRP_FAILURE(ovrp_UpdateExternalCamera()))
	{
		UE_LOG(LogMR, Error, TEXT("ovrp_UpdateExternalCamera failure"));
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
	if (GEngine && GEngine->XRSystem.IsValid())
	{
		static const FName OculusSystemName(TEXT("OculusHMD"));
		if (GEngine->XRSystem->GetSystemName() == OculusSystemName)
		{
			return static_cast<OculusHMD::FOculusHMD*>(GEngine->XRSystem.Get());
		}
	}
#endif
	return nullptr;
}

/**
* Helper that gets geometry (3D points) of outer boundaries or play area (specified by BoundaryType)
* @param BoundaryType Must be ovrpBoundary_Outer or ovrpBoundary_PlayArea, specifies the type of boundary geometry to retrieve
* @return Array of 3D points in Unreal world coordinate space corresponding to boundary geometry.
*/
static TArray<FVector> UOculusMR_Internal_GetBoundaryPoints(ovrpBoundaryType BoundaryType)
{
	TArray<FVector> BoundaryPointList;

	OculusHMD::FOculusHMD* HMD = UOculusMRFunctionLibrary::GetOculusHMD();
	if (!HMD)
	{
		return BoundaryPointList;
	}


	if (FOculusHMDModule::Get().IsOVRPluginAvailable())
	{
		int NumPoints = 0;

		if (OVRP_SUCCESS(ovrp_GetBoundaryGeometry3(BoundaryType, NULL, &NumPoints)))
		{
			//allocate points
			const int BufferSize = NumPoints;
			ovrpVector3f* BoundaryPoints = new ovrpVector3f[BufferSize];

			if (OVRP_SUCCESS(ovrp_GetBoundaryGeometry3(BoundaryType, BoundaryPoints, &NumPoints)))
			{
				NumPoints = FMath::Min(BufferSize, NumPoints);
				check(NumPoints <= BufferSize); // For static analyzer
				BoundaryPointList.Reserve(NumPoints);

				for (int i = 0; i < NumPoints; i++)
				{
					ovrpPosef BoundaryPointPose = s_identityPose;
					BoundaryPointPose.Position = BoundaryPoints[i];
					OculusHMD::FPose PointPose;
					HMD->ConvertPose(BoundaryPointPose, PointPose);
					BoundaryPointList.Add(PointPose.Position);
				}
			}

			delete[] BoundaryPoints;
		}
	}
	return BoundaryPointList;
}

TArray<FVector> UOculusMRFunctionLibrary::GetOuterBoundaryPoints()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	return UOculusMR_Internal_GetBoundaryPoints(ovrpBoundary_Outer);
#else
	return TArray<FVector>();
#endif
}
TArray<FVector> UOculusMRFunctionLibrary::GetPlayAreaPoints()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	return UOculusMR_Internal_GetBoundaryPoints(ovrpBoundary_PlayArea);
#else
	return TArray<FVector>();
#endif
}

bool UOculusMRFunctionLibrary::GetTrackingReferenceLocationAndRotationInWorldSpace(USceneComponent* TrackingReferenceComponent, FVector& TRLocation, FRotator& TRRotation)
{
	if (!TrackingReferenceComponent)
	{
		APlayerController* PlayerController = GWorld->GetFirstPlayerController();
		if (!PlayerController)
		{
			return false;
		}
		APawn* Pawn = PlayerController->GetPawn();
		if (!Pawn)
		{
			return false;
		}
		TRLocation = Pawn->GetActorLocation();
		TRRotation = Pawn->GetActorRotation();
		return true;
	}
	else
	{
		TRLocation = TrackingReferenceComponent->GetComponentLocation();
		TRRotation = TrackingReferenceComponent->GetComponentRotation();
		return true;
	}
}
