// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "IHeadMountedDisplay.h"
#include "UObject/ObjectMacros.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "IOculusMRModule.h"
#include "OculusFunctionLibrary.h"
#include "OculusMRFunctionLibrary.generated.h"

class USceneComponent;

USTRUCT(BlueprintType)
struct FTrackedCamera
{
	GENERATED_USTRUCT_BODY()

	/** >=0: the index of the external camera
	  * -1: not bind to any external camera (and would be setup to match the manual CastingCameraActor placement)
	  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|OculusLibrary")
	int32 Index;

	/** The external camera name set through the CameraTool */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|OculusLibrary")
	FString Name;

	/** The horizontal FOV, in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|OculusLibrary", meta = (UIMin = "5.0", UIMax = "170", ClampMin = "0.001", ClampMax = "360.0", Units = deg))
	float FieldOfView;

	/** The resolution of the camera frame */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|OculusLibrary")
	int32 SizeX;

	/** The resolution of the camera frame */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|OculusLibrary")
	int32 SizeY;

	/** The tracking node the external camera is bound to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|OculusLibrary")
	ETrackedDeviceType AttachedTrackedDevice;

	/** The relative pose of the camera to the attached tracking device */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|OculusLibrary")
	FRotator CalibratedRotation;

	/** The relative pose of the camera to the attached tracking device */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|OculusLibrary")
	FVector CalibratedOffset;

	/** (optional) The user pose is provided to fine tuning the relative camera pose at the run-time */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|OculusLibrary")
	FRotator UserRotation;

	/** (optional) The user pose is provided to fine tuning the relative camera pose at the run-time */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|OculusLibrary")
	FVector UserOffset;

	FTrackedCamera()
		: Index(-1)
		, Name(TEXT("Unknown"))
		, FieldOfView(90.0f)
		, SizeX(1280)
		, SizeY(720)
		, AttachedTrackedDevice(ETrackedDeviceType::None)
		, CalibratedRotation(EForceInit::ForceInitToZero)
		, CalibratedOffset(EForceInit::ForceInitToZero)
		, UserRotation(EForceInit::ForceInitToZero)
		, UserOffset(EForceInit::ForceInitToZero)
	{}
};

UCLASS()
class OCULUSMR_API UOculusMRFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/** Retrieve an array of all (calibrated) tracked cameras which were calibrated through the CameraTool */
	UFUNCTION(BlueprintCallable, Category = "MR|OculusLibrary")
	static void GetAllTrackedCamera(TArray<FTrackedCamera>& TrackedCameras, bool bCalibratedOnly = true);

	/** Retrieve the outer boundary points in the world space */
	UFUNCTION(BlueprintCallable, Category = "MR|OculusLibrary")
	static TArray<FVector> GetOuterBoundaryPoints();

	/** Retrieve the play area points in the world space */
	UFUNCTION(BlueprintCallable, Category = "MR|OculusLibrary")
	static TArray<FVector> GetPlayAreaPoints();

public:
	static class OculusHMD::FOculusHMD* GetOculusHMD();
	static bool GetTrackingReferenceLocationAndRotationInWorldSpace(USceneComponent* TrackingReferenceComponent, FVector& TRLocation, FRotator& TRRotation);
};
