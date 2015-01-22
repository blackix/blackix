// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "HeadMountedDisplayFunctionLibrary.generated.h"

UENUM()
namespace EOrientPositionSelector
{
	enum Type
	{
		Orientation UMETA(DisplayName = "Orientation"),
		Position UMETA(DisplayName = "Position"),
		OrientationAndPosition UMETA(DisplayName = "Orientation and Position")
	};
}

USTRUCT(BlueprintType, meta = (DisplayName = "HMD User Profile Data Field"))
struct FHmdUserProfileField
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	FString FieldName;

	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	FString FieldValue;

	FHmdUserProfileField() {}
	FHmdUserProfileField(const FString& Name, const FString& Value) :
		FieldName(Name), FieldValue(Value) {}
};

USTRUCT(BlueprintType, meta = (DisplayName = "HMD User Profile Data"))
struct FHmdUserProfile
{
	GENERATED_USTRUCT_BODY()

	/** Name of the user's profile. */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	FString Name;

	/** Gender of the user ("male", "female", etc). */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	FString Gender;

	/** Height of the player, in meters */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	float PlayerHeight;

	/** Height of the player, in meters */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	float EyeHeight;

	/** Interpupillary distance of the player, in meters */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	float IPD;

	/** Eye-to-neck distance, in meters. X - horizontal, Y - vertical. */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	FVector2D EyeToNeckDistance;

	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	TArray<FHmdUserProfileField> ExtraFields;

	FHmdUserProfile() : 
		PlayerHeight(0.f), EyeHeight(0.f), IPD(0.f), EyeToNeckDistance(FVector2D::ZeroVector) {}
};

UCLASS()
class UHeadMountedDisplayFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/**
	 * Returns whether or not we are currently using the head mounted display.
	 *
	 * @return (Boolean)  status of HMD
	 */
	UFUNCTION(BlueprintPure, Category="Input|HeadMountedDisplay")
	static bool IsHeadMountedDisplayEnabled();

	/**
	 * Switches to/from using HMD and stereo rendering.
	 *
	 * @param bEnable			(in) 'true' to enable HMD / stereo; 'false' otherwise
	 * @return (Boolean)		True, if the request was successful.
	 */
	UFUNCTION(BlueprintPure, Category="Input|HeadMountedDisplay")
	static bool EnableHMD(bool bEnable);

	/**
	 * Grabs the current orientation and position for the HMD.  If positional tracking is not available, DevicePosition will be a zero vector
	 *
	 * @param DeviceRotation	(out) The device's current rotation
	 * @param DevicePosition	(out) The device's current position, in its own tracking space
	 * @param bUseOrienationForPlayerCamera	(in) Should be set to 'true' if the orientation is going to be used to update orientation of the camera manually.
	 * @param bUsePositionForPlayerCamera	(in) Should be set to 'true' if the position is going to be used to update position of the camera manually.
	 * @param PositionScale		(in) The 3D scale that will be applied to position in the case if bUsePositionForPlayerCamera is set to true.
	 */
	UFUNCTION(BlueprintPure, Category="Input|HeadMountedDisplay")
	static void GetOrientationAndPosition(FRotator& DeviceRotation, FVector& DevicePosition, bool bUseOrienationForPlayerCamera = false, bool bUsePositionForPlayerCamera = false, const FVector PositionScale = FVector(1.0f, 1.0f, 1.0f));

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
	 * @param bEnable			(in) 'true' to enable low persistence mode; 'false' otherwise
	 */
	UFUNCTION(BlueprintCallable, Category="Input|HeadMountedDisplay")
	static void EnableLowPersistenceMode(bool bEnable);

	/** 
	 * Resets orientation by setting roll and pitch to 0, assuming that current yaw is forward direction and assuming
	 * current position as a 'zero-point' (for positional tracking). 
	 *
	 * @param Yaw				(in) the desired yaw to be set after orientation reset.
	 * @param Options			(in) specifies either position, orientation or both should be reset.
	 */
	UFUNCTION(BlueprintCallable, Category="Input|HeadMountedDisplay")
	static void ResetOrientationAndPosition(float Yaw = 0.f, EOrientPositionSelector::Type Options = EOrientPositionSelector::OrientationAndPosition);

	/** 
	 * Sets near and far clipping planes (NCP and FCP) for stereo rendering. Similar to 'stereo ncp= fcp' console command, but NCP and FCP set by this
	 * call won't be saved in .ini file.
	 *
	 * @param Near				(in) Near clipping plane, in centimeters
	 * @param Far				(in) Far clipping plane, in centimeters
	 */
	UFUNCTION(BlueprintCallable, Category="Input|HeadMountedDisplay")
	static void SetClippingPlanes(float Near, float Far);

	/**
	 * Sets 'base rotation' - the rotation that will be subtracted from
	 * the actual HMD orientation.
	 * The position offset might be added to current HMD position,
	 * effectively moving the virtual camera by the specified offset. The addition
	 * occurs after the HMD orientation and position are applied.
	 *
	 * @param Rotation			(in) Rotator object with base rotation
	 * @param PositionOffset	(in) the vector to be added to HMD position before applying the base rotation.
	 * @param Options			(in) specifies either position, orientation or both should be set.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|HeadMountedDisplay")
	static void SetBaseRotationAndPositionOffset(const FRotator& Rotation, const FVector& PositionOffset, EOrientPositionSelector::Type Options);

	/**
	 * Returns current base rotation and position offset.
	 *
	 * @param Rotation			(out) Rotator object with base rotation
	 * @param PositionOffset	(out) Base position (the position before ResetPosition is called).
	 */
	UFUNCTION(BlueprintPure, Category = "Input|HeadMountedDisplay")
	static void GetBaseRotationAndPositionOffset(FRotator& Rotation, FVector& PositionOffset);

	/**
	 * Reports raw sensor data. If HMD doesn't support any of the parameters then it will be set to zero.
	 *
	 * @param Accelerometer	(out) Acceleration reading in m/s^2.
	 * @param Gyro			(out) Rotation rate in rad/s.
	 * @param Magnetometer	(out) Magnetic field in Gauss.
	 * @param Temperature	(out) Temperature of the sensor in degrees Celsius.
	 * @param TimeInSeconds	(out) Time when the reported IMU reading took place, in seconds.
	 */
	UFUNCTION(BlueprintPure, Category = "Input|HeadMountedDisplay")
	static void GetRawSensorData(FVector& Accelerometer, FVector& Gyro, FVector& Magnetometer, float& Temperature, float& TimeInSeconds);

	/**
	 * Returns current user profile.
	 *
	 * @param Profile		(out) Structure to hold current user profile.
	 * @return (boolean)	True, if user profile was acquired.
	 */
	UFUNCTION(BlueprintPure, Category = "Input|HeadMountedDisplay")
	static bool GetUserProfile(FHmdUserProfile& Profile);

	/** 
	 * Sets screen percentage to be used in VR mode.
	 *
	 * @param ScreenPercentage	(in) Specifies the screen percentage to be used in VR mode. Use 0.0f value to reset to default value.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|HeadMountedDisplay")
	static void SetScreenPercentage(float ScreenPercentage = 0.0f);

	/** 
	 * Returns screen percentage to be used in VR mode.
	 *
	 * @return (float)	The screen percentage to be used in VR mode.
	 */
	UFUNCTION(BlueprintPure, Category = "Input|HeadMountedDisplay")
	static float GetScreenPercentage();
};
