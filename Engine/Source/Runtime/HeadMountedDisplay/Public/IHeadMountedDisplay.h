// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StereoRendering.h"
#include "Layout/SlateRect.h"

// depending on your kit and SDK, you may want to use this.
// new distortion handling still in development.

/**
 * The family of HMD device.  Register a new class of device here if you need to branch code for PostProcessing until 
 */
namespace EHMDDeviceType
{
	enum Type
	{
		DT_OculusRift,
		DT_Morpheus,
		DT_ES2GenericStereoMesh,
		DT_NoPost						// don't register any post passes
	};
}

/**
 * HMD device interface
 */

class HEADMOUNTEDDISPLAY_API IHeadMountedDisplay : public IModuleInterface, public IStereoRendering
{

public:
	IHeadMountedDisplay();

	/**
	 * Returns true if HMD is currently connected.
	 */
	virtual bool IsHMDConnected() = 0;

	/**
	 * Whether or not switching to stereo is enabled; if it is false, then EnableStereo(true) will do nothing.
	 */
	virtual bool IsHMDEnabled() const = 0;

	/**
	 * Enables or disables switching to stereo.
	 */
	virtual void EnableHMD(bool bEnable = true) = 0;

	/**
	 * Returns the family of HMD device implemented 
	 */
	virtual EHMDDeviceType::Type GetHMDDeviceType() const = 0;

	struct MonitorInfo
	{
		FString MonitorName;
		size_t  MonitorId;
		int		DesktopX, DesktopY;
		int		ResolutionX, ResolutionY;
		int		WindowSizeX, WindowSizeY;

		MonitorInfo() : MonitorId(0)
			, DesktopX(0)
			, DesktopY(0)
			, ResolutionX(0)
			, ResolutionY(0)
			, WindowSizeX(0)
			, WindowSizeY(0)
		{
		}

	};

    /**
     * Get the name or id of the display to output for this HMD. 
     */
	virtual bool	GetHMDMonitorInfo(MonitorInfo&) = 0;
	
    /**
	 * Calculates the FOV, based on the screen dimensions of the device. Original FOV is passed as params.
	 */
	virtual void	GetFieldOfView(float& InOutHFOVInDegrees, float& InOutVFOVInDegrees) const = 0;

	/**
	 * Whether or not the HMD supports positional tracking (either via camera or other means)
	 */
	virtual bool	DoesSupportPositionalTracking() const = 0;

	/**
	 * If the device has positional tracking, whether or not we currently have valid tracking
	 */
	virtual bool	HasValidTrackingPosition() = 0;

	/**
	 * If the HMD supports positional tracking via a camera, this returns the frustum properties (all in game-world space) of the tracking camera.
	 */
	virtual void	GetPositionalTrackingCameraProperties(FVector& OutOrigin, FQuat& OutOrientation, float& OutHFOV, float& OutVFOV, float& OutCameraDistance, float& OutNearPlane, float& OutFarPlane) const = 0;	

	/**
	 * Accessors to modify the interpupillary distance (meters)
	 */
	virtual void	SetInterpupillaryDistance(float NewInterpupillaryDistance) = 0;
	virtual float	GetInterpupillaryDistance() const = 0;

    /**
     * Get the current orientation and position reported by the HMD.
	 *
	 * @param CurrentOrientation	(out) The device's current rotation
	 * @param CurrentPosition		(out) The device's current position, in its own tracking space
	 * @param bUseOrienationForPlayerCamera	(in) Should be set to 'true' if the orientation is going to be used to update orientation of the camera manually.
	 * @param bUsePositionForPlayerCamera	(in) Should be set to 'true' if the position is going to be used to update position of the camera manually.
	 * @param PositionScale			(in) The 3D scale that will be applied to position.
	 */
    virtual void GetCurrentOrientationAndPosition(FQuat& CurrentOrientation, FVector& CurrentPosition, bool bUseOrienationForPlayerCamera = false, bool bUsePositionForPlayerCamera = false, const FVector& PositionScale = FVector::ZeroVector) = 0;

	/** 
	 * A helper function that calculates the estimated neck position using the specified orientation and position 
	 * (for example, reported by GetCurrentOrientationAndPosition()).
	 *
	 * @param CurrentOrientation	(in) The device's current rotation
	 * @param CurrentPosition		(in) The device's current position, in its own tracking space
	 * @param PositionScale			(in) The 3D scale that will be applied to position.
	 * @return			The estimated neck position, calculated using NeckToEye vector from User Profile. Same coordinate space as CurrentPosition.
	 */
	virtual FVector GetNeckPosition(const FQuat& CurrentOrientation, const FVector& CurrentPosition, const FVector& PositionScale = FVector::ZeroVector) { return FVector::ZeroVector; }

    /**
     * Get the ISceneViewExtension for this HMD, or none.
     */
    virtual TSharedPtr<class ISceneViewExtension> GetViewExtension() = 0;

	/**
     * Apply the orientation of the headset to the PC's rotation.
     * If this is not done then the PC will face differently than the camera,
     * which might be good (depending on the game).
     */
	virtual void ApplyHmdRotation(class APlayerController* PC, FRotator& ViewRotation) = 0;

	/**
	 * Apply the orientation and position of the headset to the Camera's rotation/location.
	 * This method is called for cameras with bFollowHmdOrientation set to 'true'.
	 */
	virtual void UpdatePlayerCamera(class APlayerCameraManager* Camera, struct FMinimalViewInfo& POV) = 0;

  	/**
	 * Gets the scaling factor, applied to the post process warping effect
	 */
	virtual float GetDistortionScalingFactor() const { return 0; }

	/**
	 * Gets the offset (in clip coordinates) from the center of the screen for the lens position
	 */
	virtual float GetLensCenterOffset() const { return 0; }

	/**
	 * Gets the barrel distortion shader warp values for the device
	 */
	virtual void GetDistortionWarpValues(FVector4& K) const  { }

	/**
	 * Returns 'false' if chromatic aberration correction is off.
	 */
	virtual bool IsChromaAbCorrectionEnabled() const = 0;

	/**
	 * Gets the chromatic aberration correction shader values for the device. 
	 * Returns 'false' if chromatic aberration correction is off.
	 */
	virtual bool GetChromaAbCorrectionValues(FVector4& K) const  { return false; }

	/**
	 * Exec handler to allow console commands to be passed through to the HMD for debugging
	 */
    virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) = 0;

	/**
	 * Returns true, if HMD allows fullscreen mode.
	 */
	virtual bool IsFullscreenAllowed() { return true; }

	/**
	 * Saves / loads pre-fullscreen rectangle. Could be used to store saved original window position 
	 * before switching to fullscreen mode.
	 */
	virtual void PushPreFullScreenRect(const FSlateRect& InPreFullScreenRect);
	virtual void PopPreFullScreenRect(FSlateRect& OutPreFullScreenRect);

	/**
	 * A callback that is called when screen mode is changed (fullscreen <-> window). 
	 */
	virtual void OnScreenModeChange(EWindowMode::Type WindowMode) = 0;

	/** Returns true if positional tracking enabled and working. */
	virtual bool IsPositionalTrackingEnabled() const = 0;

	/** 
	 * Tries to enable positional tracking.
	 * Returns the actual status of positional tracking. 
	 */
	virtual bool EnablePositionalTracking(bool enable) = 0;

	/**
	 * Returns true, if head tracking is allowed. Most common case: it returns true when GEngine->IsStereoscopic3D() is true,
	 * but some overrides are possible.
	 */
	virtual bool IsHeadTrackingAllowed() const = 0;

	/**
	 * Returns true, if HMD is in low persistence mode. 'false' otherwise.
	 */
	virtual bool IsInLowPersistenceMode() const = 0;

	/**
	 * Switches between low and full persistence modes.
	 *
	 * @param Enable			(in) 'true' to enable low persistence mode; 'false' otherwise
	 */
	virtual void EnableLowPersistenceMode(bool Enable = true) = 0;

	/** 
	 * Resets orientation by setting roll and pitch to 0, assuming that current yaw is forward direction and assuming
	 * current position as a 'zero-point' (for positional tracking). 
	 *
	 * @param Yaw				(in) the desired yaw to be set after orientation reset.
	 */
	virtual void ResetOrientationAndPosition(float Yaw = 0.f) = 0;

	/** 
	 * Resets orientation by setting roll and pitch to 0, assuming that current yaw is forward direction. Position is not changed. 
	 *
	 * @param Yaw				(in) the desired yaw to be set after orientation reset.
	 */
	virtual void ResetOrientation(float Yaw = 0.f) {}

	/** 
	 * Resets position, assuming current position as a 'zero-point'. 
	 */
	virtual void ResetPosition() {}

	/** 
	 * Sets base orientation by setting yaw, pitch, roll, assuming that this is forward direction. 
	 * Position is not changed. 
	 *
	 * @param BaseRot			(in) the desired orientation to be treated as a base orientation.
	 */
	virtual void SetBaseRotation(const FRotator& BaseRot) {}

	/**
	 * Returns current base orientation of HMD as yaw-pitch-roll combination.
	 */
	virtual FRotator GetBaseRotation() const { return FRotator::ZeroRotator; }

	/** 
	 * Sets base orientation, assuming that this is forward direction. 
	 * Position is not changed. 
	 *
	 * @param BaseOrient		(in) the desired orientation to be treated as a base orientation.
	 */
	virtual void SetBaseOrientation(const FQuat& BaseOrient) {}

	/**
	 * Returns current base orientation of HMD as a quaternion.
	 */
	virtual FQuat GetBaseOrientation() const { return FQuat::Identity; }

	/**
	 * Overrides HMD base offset. Base offset is added to current HMD position, 
	 * effectively moving the virtual camera by the specified offset. The addition
	 * occurs BEFORE the base orientations is applied.
	 *
	 * @param PosOffset			(in) the vector to be added to HMD position.
	 */
	virtual void SetBaseOffset(const FVector& PosOffset) {}

	/**
	 * Returns the currently used base offset. The base offset is the vector that is added to the position 
	 * BEFORE base orientation is applied. ResetOrientationAndPosition / ResetPosition methods sets the base
	 * offset to recently read position.
	 */
	virtual FVector GetBaseOffset() const { return FVector::ZeroVector; }

	virtual void DrawDistortionMesh_RenderThread(struct FRenderingCompositePassContext& Context, const FSceneView& View, const FIntPoint& TextureSize) {}

	/**
	 * This method is able to change screen settings right before any drawing occurs. 
	 * It is called at the beginning of UGameViewportClient::Draw() method.
	 * We might remove this one as UpdatePostProcessSettings should be able to capture all needed cases
	 */
	virtual void UpdateScreenSettings(const FViewport* InViewport) {}

	/**
	 * Allows to override the PostProcessSettings in the last moment e.g. allows up sampled 3D rendering
	 */
	virtual void UpdatePostProcessSettings(FPostProcessSettings*) {}

	/**
	 * Draw desired debug information related to the HMD system.
	 * @param Canvas The canvas on which to draw.
	 */
	virtual void DrawDebug(UCanvas* Canvas) {}

	/**
	 * Passing key events to HMD.
	 * If returns 'false' then key will be handled by PlayerController;
	 * otherwise, key won't be handled by the PlayerController.
	 */
	virtual bool HandleInputKey(class UPlayerInput*, const struct FKey& Key, enum EInputEvent EventType, float AmountDepressed, bool bGamepad) { return false; }

	/**
	 * This method is called when playing begins. Useful to reset all runtime values stored in the plugin.
	 */
	virtual void OnBeginPlay() {}

	/**
	 * This method is called when playing ends. Useful to reset all runtime values stored in the plugin.
	 */
	virtual void OnEndPlay() {}

	/**
	 * This method is called when new game frame begins (called on a game thread).
	 */
	virtual void OnStartGameFrame() {}

	/**
	 * This method is called when game frame ends (called on a game thread).
	 */
	virtual void OnEndGameFrame() {}

	/** 
	 * Additional optional distorion rendering parameters
	 * @todo:  Once we can move shaders into plugins, remove these!
	 */	
	virtual FTexture* GetDistortionTextureLeft() const {return NULL;}
	virtual FTexture* GetDistortionTextureRight() const {return NULL;}
	virtual FVector2D GetTextureOffsetLeft() const {return FVector2D::ZeroVector;}
	virtual FVector2D GetTextureOffsetRight() const {return FVector2D::ZeroVector;}
	virtual FVector2D GetTextureScaleLeft() const {return FVector2D::ZeroVector;}
	virtual FVector2D GetTextureScaleRight() const {return FVector2D::ZeroVector;}

	/**
	 * Record analytics
	 */
	virtual void RecordAnalytics() {}

	/* Raw sensor data structure. */
	struct SensorData
	{
		FVector Accelerometer;	// Acceleration reading in m/s^2.
		FVector Gyro;			// Rotation rate in rad/s.
		FVector Magnetometer;   // Magnetic field in Gauss.
		float Temperature;		// Temperature of the sensor in degrees Celsius.
		float TimeInSeconds;	// Time when the reported IMU reading took place, in seconds.
	};

	/**
	 * Reports raw sensor data. If HMD doesn't support any of the parameters then it should be set to zero.
	 *
	 * @param OutData	(out) SensorData structure to be filled in.
	 */
	virtual void GetRawSensorData(SensorData& OutData) { FMemory::MemSet(OutData, 0); }

	/**
	 * User profile structure.
	 */
	struct UserProfile
	{
		FString Name;
		FString Gender;
		float PlayerHeight;				// Height of the player, in meters
		float EyeHeight;				// Height of the player's eyes, in meters
		float IPD;						// Interpupillary distance, in meters
		FVector2D NeckToEyeDistance;	// Neck-to-eye distance, X - horizontal, Y - vertical, in meters
		TMap<FString, FString> ExtraFields; // extra fields in name / value pairs.
	};

	/** 
	 * Returns currently used user profile.
	 *
	 * @param OutProfile	(out) UserProfile structure to hold data from the user profile.
	 * @return (boolean)	True, if user profile was acquired. 
	 */
	virtual bool GetUserProfile(UserProfile& OutProfile) { return false; }

private:
	/** Stores the dimensions of the window before we moved into fullscreen mode, so they can be restored */
	FSlateRect PreFullScreenRect;
};
