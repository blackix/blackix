// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "HeadMountedDisplayFunctionLibrary.generated.h"

UCLASS()
class UHeadMountedDisplayFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/**
	 * Returns whether or not we are currently using the head mounted display
	 */
	UFUNCTION(BlueprintPure, Category="Input|HeadMountedDisplay")
	static bool IsHeadMountedDisplayEnabled();

	/**
	 * Grabs the current orientation and position for the HMD.  If positional tracking is not available, DevicePosition will be a zero vector
	 *
	 * @param DeviceRotation	(out) The device's current rotation
	 * @param DevicePosition	(out) The device's current position, in its own tracking space
	 */
	UFUNCTION(BlueprintPure, Category="Input|HeadMountedDisplay")
	static void GetOrientationAndPosition(FRotator& DeviceRotation, FVector& DevicePosition);

	/**
	 * If the HMD supports positional tracking, whether or not we are currently being tracked	
	 */
 	UFUNCTION(BlueprintPure, Category="Input|HeadMountedDisplay")
	static bool HasValidTrackingPosition();

	/**
	 * If the HMD has a positional tracking camera, this will return the game-world location of the camera, as well as the parameters for the bounding region of tracking.
	 * This allows an in-game representation of the legal positional tracking range.  All values will be zeroed if the camera is not available or the HMD does not support it.
	 *
	 * @param CameraOrigin		(out) Origin, in world-space, of the tracking camera
	 * @param CameraOrientation (out) Rotation, in world-space, of the tracking camera
	 * @param HFOV				(out) Field-of-view, horizontal, in degrees, of the valid tracking zone of the camera
	 * @param VFOV				(out) Field-of-view, vertical, in degrees, of the valid tracking zone of the camera
	 * @param CameraDistance	(out) Nominal distance to camera, in world-space
	 * @param NearPlane			(out) Near plane distance of the tracking volume, in world-space
	 * @param FarPlane			(out) Far plane distance of the tracking volume, in world-space
	 */
	UFUNCTION(BlueprintPure, Category="Input|HeadMountedDisplay")
	static void GetPositionalTrackingCameraParameters(FVector& CameraOrigin, FRotator& CameraOrientation, float& HFOV, float& VFOV, float& CameraDistance, float& NearPlane, float&FarPlane);

	/**
	 * Returns true, if HMD is in low persistence mode. 'false' otherwise.
	 */
	UFUNCTION(BlueprintPure, Category="Input|HeadMountedDisplay")
	static bool IsInLowPersistenceMode();

	/**
	 * Switches between low and full persistence modes.
	 *
	 * @param Enable			(in) 'true' to enable low persistence mode; 'false' otherwise
	 */
	UFUNCTION(BlueprintCallable, Category="Input|HeadMountedDisplay")
	static void EnableLowPersistenceMode(bool Enable);

	/** 
	 * Resets orientation by setting roll and pitch to 0, assuming that current yaw is forward direction and assuming
	 * current position as a 'zero-point' (for positional tracking). 
	 *
	 * @param Yaw				(in) the desired yaw to be set after orientation reset.
	 */
	UFUNCTION(BlueprintCallable, Category="Input|HeadMountedDisplay")
	static void ResetOrientationAndPosition(float Yaw = 0.f);

	/** 
	 * Resets orientation by setting roll and pitch to 0, assuming that current yaw is forward direction. Position is not changed. 
	 *
	 * @param Yaw				(in) the desired yaw to be set after orientation reset.
	 */
	UFUNCTION(BlueprintCallable, Category="Input|HeadMountedDisplay")
	static void ResetOrientation(float Yaw = 0.f);

	/** 
	 * Resets position, assuming current position as a 'zero-point'.
	 */
	UFUNCTION(BlueprintCallable, Category="Input|HeadMountedDisplay")
	static void ResetPosition();

	/** 
	 * Sets near and far clipping planes (NCP and FCP) for stereo rendering. Similar to 'stereo ncp= fcp' console command, but NCP and FCP set by this
	 * call won't be saved in .ini file.
	 *
	 * @param NCP				(in) Near clipping plane, in centimeters
	 * @param FCP				(in) Far clipping plane, in centimeters
	 */
	UFUNCTION(BlueprintCallable, Category="Input|HeadMountedDisplay")
	static void SetClippingPlanes(float NCP, float FCP);

	/**
	 * Sets 'base rotation' - the rotation that will be subtracted from
	 * the actual HMD orientation.
	 *
	 * @param BaseRot			(in) Rotator object with base rotation
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|HeadMountedDisplay")
	static void SetBaseRotation(const FRotator& BaseRot);

	/**
	 * Returns current base rotation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|HeadMountedDisplay")
	static void GetBaseRotation(FRotator& OutRot);

	/**
	 * Sets 'base orientation' - the orientation that will be subtracted from
	 * the actual HMD orientation.
	 *
	 * @param BaseOrient		(in) Quaternion object with base orientation
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|HeadMountedDisplay")
	static void SetBaseOrientation(const FQuat& BaseOrient);

	/**
	 * Returns current base orientation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|HeadMountedDisplay")
	static void GetBaseOrientation(FQuat& OutOrient);

	/**
	 * Sets HMD position offset that will be added to current HMD position, 
	 * effectively moving the virtual camera by the specified offset. The addition
	 * occurs after the HMD orientation and position are applied.
	 *
	 * @param PosOffset			(in) the vector to be added to HMD position.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|HeadMountedDisplay")
	static void SetPositionOffset(const FVector& PosOffset);

	/**
	 * Returns the currently set position offset, previously set by the 
	 * SetPositionOffset call.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|HeadMountedDisplay")
	static void GetPositionOffset(FVector& OutPosOffset);
};
