// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OculusRiftPrivate.h"
#include "OculusRiftHMD.h"
#include "EngineAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "SceneViewport.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Classes/Editor/EditorEngine.h"
#endif

#if !UE_BUILD_SHIPPING
// Should be changed to CAPI when available.
#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif
#include "../Src/Kernel/OVR_Log.h"
#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif
#endif // #if !UE_BUILD_SHIPPING

//---------------------------------------------------
// Oculus Rift Plugin Implementation
//---------------------------------------------------

class FOculusRiftPlugin : public IOculusRiftPlugin
{
	/** IHeadMountedDisplayModule implementation */
	virtual TSharedPtr< class IHeadMountedDisplay > CreateHeadMountedDisplay() override;

	// Pre-init the HMD module (optional).
	virtual void PreInit() override;

	FString GetModulePriorityKeyName() const
	{
		return FString(TEXT("OculusRift"));
	}
};

IMPLEMENT_MODULE( FOculusRiftPlugin, OculusRift )

TSharedPtr< class IHeadMountedDisplay > FOculusRiftPlugin::CreateHeadMountedDisplay()
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	TSharedPtr< FOculusRiftHMD > OculusRiftHMD( new FOculusRiftHMD() );
	if( OculusRiftHMD->IsInitialized() )
	{
		return OculusRiftHMD;
	}
#endif//OCULUS_RIFT_SUPPORTED_PLATFORMS
	return NULL;
}

void FOculusRiftPlugin::PreInit()
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	FOculusRiftHMD::PreInit();
#endif//OCULUS_RIFT_SUPPORTED_PLATFORMS
}

//---------------------------------------------------
// Oculus Rift IHeadMountedDisplay Implementation
//---------------------------------------------------

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#if !UE_BUILD_SHIPPING
//////////////////////////////////////////////////////////////////////////
class OculusLog : public OVR::Log
{
public:
	OculusLog()
	{
		SetLoggingMask(OVR::LogMask_Debug | OVR::LogMask_Regular);
	}

	// This virtual function receives all the messages,
	// developers should override this function in order to do custom logging
	virtual void    LogMessageVarg(LogMessageType messageType, const char* fmt, va_list argList)
	{
		if ((messageType & GetLoggingMask()) == 0)
			return;

		ANSICHAR buf[1024];
		int32 len = FCStringAnsi::GetVarArgs(buf, sizeof(buf), sizeof(buf) / sizeof(ANSICHAR), fmt, argList);
		if (len > 0 && buf[len - 1] == '\n') // truncate the trailing new-line char, since Logf adds its own
			buf[len - 1] = '\0';
		TCHAR* tbuf = ANSI_TO_TCHAR(buf);
		GLog->Logf(TEXT("OCULUS: %s"), tbuf);
	}
};

#endif

//////////////////////////////////////////////////////////////////////////
FSettings::FSettings() :
	SavedScrPerc(100.f)
	, ScreenPercentage(100.f)
	, InterpupillaryDistance(OVR_DEFAULT_IPD)
	, WorldToMetersScale(100.f)
	, UserDistanceToScreenModifier(0.f)
	, HFOVInRadians(FMath::DegreesToRadians(90.f))
	, VFOVInRadians(FMath::DegreesToRadians(90.f))
	, HudOffset(0.f)
	, CanvasCenterOffset(0.f)
	, MirrorWindowSize(0, 0)
	, NearClippingPlane(0)
	, FarClippingPlane(0)
	, BaseOffset(0, 0, 0)
	, BaseOrientation(FQuat::Identity)
	, PositionOffset(0,0,0)
{
	Flags.Raw = 0;
	Flags.bHMDEnabled = true;
	Flags.bOverrideVSync = true;
	Flags.bVSync = true;
	Flags.bAllowFinishCurrentFrame = true;
	Flags.bHmdDistortion = true;
	Flags.bChromaAbCorrectionEnabled = true;
	Flags.bYawDriftCorrectionEnabled = true;
	Flags.bLowPersistenceMode = true; // on by default (DK2+ only)
	Flags.bUpdateOnRT = true;
	Flags.bOverdrive = true;
	Flags.bMirrorToWindow = true;
	Flags.bTimeWarp = true;
#ifdef OVR_VISION_ENABLED
	Flags.bHmdPosTracking = true;
#endif
#ifndef OVR_SDK_RENDERING
	Flags.bTimeWarp = false;
#else
#endif
	FMemory::MemSet(EyeRenderDesc, 0);
	FMemory::MemSet(EyeProjectionMatrices, 0);
	FMemory::MemSet(EyeFov, 0);
	FMemory::MemSet(EyeRenderViewport, 0);

	SupportedTrackingCaps = SupportedDistortionCaps = SupportedHmdCaps = 0;
	TrackingCaps = DistortionCaps = HmdCaps = 0;

#ifndef OVR_SDK_RENDERING
	FMemory::MemSet(UVScaleOffset, 0);
#endif
}

void FSettings::SetViewportSize(int w, int h)
{
	EyeRenderViewport[0].Pos = Vector2i(0, 0);
	EyeRenderViewport[0].Size = Sizei(w, h);
	EyeRenderViewport[1].Pos = Vector2i(w, 0);
	EyeRenderViewport[1].Size = EyeRenderViewport[0].Size;
}

void FSettings::Reset()
{
#ifndef OVR_SDK_RENDERING
	for (unsigned i = 0; i < sizeof(pDistortionMesh) / sizeof(pDistortionMesh[0]); ++i)
	{
		pDistortionMesh[i] = nullptr;
	}
#endif
}


void FGameFrame::Reset()
{
	Settings.Reset();
	FMemory::MemSet(*this, 0);
	DeltaControlOrientation = FQuat::Identity;
	LastHmdOrientation = FQuat::Identity;
	LastHmdPosition = FVector::ZeroVector;
	LastFrameNumber = 0;
	CameraScale3D = FVector(1.0f, 1.0f, 1.0f);
}


void FOculusRiftHMD::PreInit()
{
	ovr_Initialize();
}

void FOculusRiftHMD::OnStartGameFrame()
{
	Frame.Reset();
	Frame.Flags.bFrameStarted = true;
	if (!Settings.IsStereoEnabled() && !Settings.Flags.bHeadTrackingEnforced)
	{
		return;
	}

	if (Flags.bNeedUpdateDistortionCaps)
	{
		UpdateDistortionCaps();
	}
	if (Flags.bNeedUpdateHmdCaps)
	{
		UpdateHmdCaps();
	}

	if (Flags.bNeedDisableStereo)
	{
		DoEnableStereo(false, true);
		Flags.bNeedDisableStereo = false;
	}
	if (Flags.bApplySystemOverridesOnStereo)
	{
		ApplySystemOverridesOnStereo();
		Flags.bApplySystemOverridesOnStereo = false;
	}

	if (Flags.bNeedUpdateStereoRenderingParams)
	{
		UpdateStereoRenderingParams();
	}

	Frame.FrameNumber = GFrameCounter;
	Frame.Settings = Settings;
	Frame.Flags.bOutOfFrame = false;
	Frame.WorldToMetersScale = -1000;

#ifdef OVR_VISION_ENABLED
	if (Hmd && Frame.Settings.Flags.bHmdPosTracking)
	{
		const ovrTrackingState ts = ovrHmd_GetTrackingState(Hmd, ovr_GetTimeInSeconds());
		Frame.Flags.bHaveVisionTracking = (ts.StatusFlags & ovrStatus_PositionTracked) != 0;
		if (Frame.Flags.bHaveVisionTracking && !Flags.bHadVisionTracking)
		{
			UE_LOG(LogHMD, Warning, TEXT("Vision Tracking Acquired"));
		}
		if (!Frame.Flags.bHaveVisionTracking && Flags.bHadVisionTracking)
		{
			UE_LOG(LogHMD, Warning, TEXT("Lost Vision Tracking"));
		}
		Flags.bHadVisionTracking = Frame.Flags.bHaveVisionTracking;
	}
#endif // OVR_VISION_ENABLED
}

void FOculusRiftHMD::OnBeginRenderingViewFamily(FCanvas*, FSceneViewFamily*)
{
	check(IsInGameThread());
	if (!Frame.Settings.IsStereoEnabled() && !Frame.Settings.Flags.bHeadTrackingEnforced)
	{
		return;
	}

	Lock::Locker lock(&UpdateOnRTLock);
	if (GFrameCounter == Frame.FrameNumber)
	{
		RenderFrame = Frame;
	}
	else
	{
		RenderFrame.Reset();
	}
}

void FOculusRiftHMD::OnEndGameFrame()
{
	check(IsInGameThread());
	if (!Frame.Settings.IsStereoEnabled() && !Frame.Settings.Flags.bHeadTrackingEnforced)
	{
		return;
	}
	check(GFrameCounter == Frame.FrameNumber);
	//Frame.FrameNumber = 0;

	Frame.Flags.bOutOfFrame = true;
	Frame.Flags.bFrameStarted = false;
}

void FOculusRiftHMD::OnWorldTick()
{
	check(IsInGameThread());
	if (!Frame.Settings.IsStereoEnabled() && !Frame.Settings.Flags.bHeadTrackingEnforced)
	{
		return;
	}
	check(GWorld);

	auto frame = GetFrame();
	check(frame);

	if (frame->Settings.Flags.bWorldToMetersOverride)
	{
		frame->WorldToMetersScale = frame->Settings.WorldToMetersScale;
	}
	else
	{
		frame->WorldToMetersScale = GWorld->GetWorldSettings()->WorldToMeters;
	}
}

bool FOculusRiftHMD::IsHMDConnected()
{
	if (Settings.Flags.bHMDEnabled)
	{
		InitDevice();
		return Hmd != nullptr;
	}
	return false;
}

FGameFrame* FOculusRiftHMD::GetFrame()
{
	check(IsInGameThread());
	// Technically speaking, we should return the frame only if frame counters are equal.
	// However, there are some calls UE makes from outside of the frame (for example, when 
	// switching to/from fullscreen), thus, returning the previous frame in this case.
	if (Frame.FrameNumber == GFrameCounter || Frame.Flags.bOutOfFrame)
	{
		return &Frame;
	}
	return nullptr;
}

const FGameFrame* FOculusRiftHMD::GetFrame() const
{
	check(IsInGameThread());
	// Technically speaking, we should return the frame only if frame counters are equal.
	// However, there are some calls UE makes from outside of the frame (for example, when 
	// switching to/from fullscreen), thus, returning the previous frame in this case.
	if (Frame.FrameNumber == GFrameCounter || Frame.Flags.bOutOfFrame)
	{
		return &Frame;
	}
	return nullptr;
}

bool FOculusRiftHMD::IsHMDEnabled() const
{
	return Settings.Flags.bHMDEnabled;
}

void FOculusRiftHMD::EnableHMD(bool enable)
{
	Settings.Flags.bHMDEnabled = enable;
	if (!Settings.Flags.bHMDEnabled)
	{
		EnableStereo(false);
	}
}

EHMDDeviceType::Type FOculusRiftHMD::GetHMDDeviceType() const
{
	return EHMDDeviceType::DT_OculusRift;
}

bool FOculusRiftHMD::GetHMDMonitorInfo(MonitorInfo& MonitorDesc)
{
	if (IsInitialized())
	{
		InitDevice();
	}
	if (Hmd)
	{
		MonitorDesc.MonitorName = Hmd->DisplayDeviceName;
		MonitorDesc.MonitorId	= Hmd->DisplayId;
		MonitorDesc.DesktopX	= Hmd->WindowsPos.x;
		MonitorDesc.DesktopY	= Hmd->WindowsPos.y;
		MonitorDesc.ResolutionX = Hmd->Resolution.w;
		MonitorDesc.ResolutionY = Hmd->Resolution.h;
		MonitorDesc.WindowSizeX = Settings.MirrorWindowSize.X;
		MonitorDesc.WindowSizeY = Settings.MirrorWindowSize.Y;
		return true;
	}
	else
	{
		MonitorDesc.MonitorName = "";
		MonitorDesc.MonitorId = 0;
		MonitorDesc.DesktopX = MonitorDesc.DesktopY = MonitorDesc.ResolutionX = MonitorDesc.ResolutionY = 0;
		MonitorDesc.WindowSizeX = MonitorDesc.WindowSizeY = 0;
	}
	return false;
}

bool FOculusRiftHMD::IsFullscreenAllowed()
{
	InitDevice();	
	return ((Hmd && (Hmd->HmdCaps & ovrHmdCap_ExtendDesktop) != 0) || !Hmd) ? true : false;
}

bool FOculusRiftHMD::DoesSupportPositionalTracking() const
{
#ifdef OVR_VISION_ENABLED
	auto frame = GetFrame();
	return (frame && frame->Settings.Flags.bHmdPosTracking && (Settings.SupportedTrackingCaps & ovrTrackingCap_Position) != 0);
#else
	return false;
#endif //OVR_VISION_ENABLED
}

bool FOculusRiftHMD::HasValidTrackingPosition()
{
#ifdef OVR_VISION_ENABLED
	auto frame = GetFrame();
	return (frame && frame->Settings.Flags.bHmdPosTracking && frame->Flags.bHaveVisionTracking);
#else
	return false;
#endif //OVR_VISION_ENABLED
}

#define TRACKER_FOCAL_DISTANCE			1.00f // meters (focal point to origin for position)

void FOculusRiftHMD::GetPositionalTrackingCameraProperties(FVector& OutOrigin, FRotator& OutOrientation, float& OutHFOV, float& OutVFOV, float& OutCameraDistance, float& OutNearPlane, float& OutFarPlane) const
{
	auto frame = GetFrame();
	if (!frame)
	{
		return;
	}
	OutOrigin = FVector::ZeroVector;
	OutOrientation = FRotator::ZeroRotator;
	OutHFOV = OutVFOV = OutCameraDistance = OutNearPlane = OutFarPlane = 0;

	if (!Hmd)
	{
		return;
	}

	check(frame->WorldToMetersScale >= 0);
	OutCameraDistance = TRACKER_FOCAL_DISTANCE * frame->WorldToMetersScale;
	OutHFOV = FMath::RadiansToDegrees(Hmd->CameraFrustumHFovInRadians);
	OutVFOV = FMath::RadiansToDegrees(Hmd->CameraFrustumVFovInRadians);
	OutNearPlane = Hmd->CameraFrustumNearZInMeters * frame->WorldToMetersScale;
	OutFarPlane = Hmd->CameraFrustumFarZInMeters * frame->WorldToMetersScale;

	// Read the camera pose
	const ovrTrackingState ss = ovrHmd_GetTrackingState(Hmd, ovr_GetTimeInSeconds());
	if (!(ss.StatusFlags & ovrStatus_CameraPoseTracked))
	{
		return;
	}
	const ovrPosef& cameraPose = ss.CameraPose;

	FQuat Orient;
	FVector Pos;
	PoseToOrientationAndPosition(cameraPose, Orient, Pos, *frame);
	OutOrientation = (frame->DeltaControlOrientation * Orient).Rotator();
	OutOrigin = frame->DeltaControlOrientation.RotateVector(Pos) + frame->Settings.PositionOffset;
}

bool FOculusRiftHMD::IsInLowPersistenceMode() const
{
	auto frame = GetFrame();
	return (frame && frame->Settings.Flags.bLowPersistenceMode && (Settings.SupportedHmdCaps & ovrHmdCap_LowPersistence) != 0);
}

void FOculusRiftHMD::EnableLowPersistenceMode(bool Enable)
{
	Settings.Flags.bLowPersistenceMode = Enable;
	Flags.bNeedUpdateHmdCaps = true;
}

float FOculusRiftHMD::GetInterpupillaryDistance() const
{
	return Settings.InterpupillaryDistance;
}

void FOculusRiftHMD::SetInterpupillaryDistance(float NewInterpupillaryDistance)
{
	Settings.InterpupillaryDistance = NewInterpupillaryDistance;
 	UpdateStereoRenderingParams();
}

void FOculusRiftHMD::GetFieldOfView(float& InOutHFOVInDegrees, float& InOutVFOVInDegrees) const
{
	auto frame = GetFrame();
	if (frame)
	{
		InOutHFOVInDegrees = FMath::RadiansToDegrees(frame->Settings.HFOVInRadians);
		InOutVFOVInDegrees = FMath::RadiansToDegrees(frame->Settings.VFOVInRadians);
	}
}

void FOculusRiftHMD::PoseToOrientationAndPosition(const ovrPosef& InPose, FQuat& OutOrientation, FVector& OutPosition, const FGameFrame& InFrame) const
{
	OutOrientation = ToFQuat(InPose.Orientation);

	check(InFrame.WorldToMetersScale >= 0);
	// correct position according to BaseOrientation and BaseOffset. 
	const FVector Pos = ToFVector_M2U(Vector3f(InPose.Position) - InFrame.Settings.BaseOffset, InFrame.WorldToMetersScale) * InFrame.CameraScale3D;
	OutPosition = InFrame.Settings.BaseOrientation.Inverse().RotateVector(Pos);

	// apply base orientation correction to OutOrientation
	OutOrientation = InFrame.Settings.BaseOrientation.Inverse() * OutOrientation;
	OutOrientation.Normalize();
}

void FOculusRiftHMD::GetCurrentOrientationAndPosition(FQuat& CurrentOrientation, FVector& CurrentPosition, 
	bool bUseOrienationForPlayerCamera, bool bUsePositionForPlayerCamera, const FVector& PositionScale)
{
	// only supposed to be used from the game thread
	checkf(IsInGameThread());
	auto frame = GetFrame();
	if (!frame)
	{
		CurrentOrientation = FQuat::Identity;
		CurrentPosition = FVector::ZeroVector;
		return;
	}
	GetCurrentPose(CurrentOrientation, CurrentPosition, bUseOrienationForPlayerCamera, bUsePositionForPlayerCamera);
	if (bUseOrienationForPlayerCamera)
	{
		frame->LastHmdOrientation = CurrentOrientation;
		frame->Flags.bOrientationChanged = bUseOrienationForPlayerCamera;
	}
	if (bUsePositionForPlayerCamera)
	{
		frame->LastHmdPosition = CurrentPosition;
		frame->Flags.bPositionChanged = bUsePositionForPlayerCamera;
		frame->CameraScale3D = PositionScale;
	}
}

void FOculusRiftHMD::GetCurrentPose(FQuat& CurrentHmdOrientation, FVector& CurrentHmdPosition, bool bUseOrienationForPlayerCamera, bool bUsePositionForPlayerCamera)
{
	check(IsInGameThread());
	check(Hmd);

	auto frame = GetFrame();
	check(frame);

	// Save eye poses
	ovrTrackingState ts;
	ovrVector3f hmdToEyeViewOffset[2] = { frame->Settings.EyeRenderDesc[0].HmdToEyeViewOffset, frame->Settings.EyeRenderDesc[1].HmdToEyeViewOffset };
	ovrPosef CurEyeRenderPose[2];
	ovrHmd_GetEyePoses(Hmd, frame->FrameNumber, hmdToEyeViewOffset, CurEyeRenderPose, &ts);
	if (bUseOrienationForPlayerCamera || bUsePositionForPlayerCamera)
	{
		// if this pose is going to be used for camera update then save it.
		// This matters only if bUpdateOnRT is OFF.
		frame->EyeRenderPose[0] = CurEyeRenderPose[0];
		frame->EyeRenderPose[1] = CurEyeRenderPose[1];
		frame->HeadPose = ts.HeadPose.ThePose;
	}

	PoseToOrientationAndPosition(ts.HeadPose.ThePose, CurrentHmdOrientation, CurrentHmdPosition, *frame);
	//UE_LOG(LogHMD, Log, TEXT("P: %.3f %.3f %.3f"), CurrentHmdPosition.X, CurrentHmdPosition.Y, CurrentHmdPosition.Y);

	frame->LastFrameNumber = frame->FrameNumber;
}

void FOculusRiftHMD::ApplyHmdRotation(APlayerController* PC, FRotator& ViewRotation)
{
	auto frame = GetFrame();
	if (!frame)
	{
		return;
	}
#if !UE_BUILD_SHIPPING
 	if (frame->Settings.Flags.bDoNotUpdateOnGT)
		return;
#endif

	ViewRotation.Normalize();

	FQuat CurHmdOrientation;
	FVector CurHmdPosition;
	GetCurrentPose(CurHmdOrientation, CurHmdPosition, true, true);
	frame->LastHmdOrientation = CurHmdOrientation;

	const FRotator DeltaRot = ViewRotation - PC->GetControlRotation();
	DeltaControlRotation = (DeltaControlRotation + DeltaRot).GetNormalized();

	// Pitch from other sources is never good, because there is an absolute up and down that must be respected to avoid motion sickness.
	// Same with roll.
	DeltaControlRotation.Pitch = 0;
	DeltaControlRotation.Roll = 0;
	frame->DeltaControlOrientation = DeltaControlRotation.Quaternion();

	ViewRotation = FRotator(frame->DeltaControlOrientation * CurHmdOrientation);

	frame->Flags.bPlayerControllerFollowsHmd = true;
	frame->Flags.bOrientationChanged = true;
	frame->Flags.bPositionChanged = true;
#if !UE_BUILD_SHIPPING
	if (frame->Settings.Flags.bDrawTrackingCameraFrustum && PC->GetPawnOrSpectator())
	{
		DrawDebugTrackingCameraFrustum(PC->GetWorld(), PC->GetPawnOrSpectator()->GetPawnViewLocation());
	}
#endif
}

void FOculusRiftHMD::UpdatePlayerCamera(APlayerCameraManager* Camera, struct FMinimalViewInfo& POV)
{
	auto frame = GetFrame();
	if (!frame)
	{
		return;
	}
	frame->LastHmdOrientation = FQuat::Identity;
	frame->LastHmdPosition = FVector::ZeroVector;
	frame->CameraScale3D = POV.Scale3D;

#if !UE_BUILD_SHIPPING
	if (frame->Settings.Flags.bDoNotUpdateOnGT)
		return;
#endif
	FQuat	CurHmdOrientation;
	FVector CurHmdPosition;
	GetCurrentPose(CurHmdOrientation, CurHmdPosition, POV.bFollowHmdOrientation, POV.bFollowHmdPosition);

	DeltaControlRotation = FRotator::ZeroRotator;
	frame->DeltaControlOrientation = POV.Rotation.Quaternion();

	if (POV.bFollowHmdOrientation)
	{
		// Apply HMD orientation to camera rotation.
		POV.Rotation = FRotator(POV.Rotation.Quaternion() * CurHmdOrientation);
		frame->LastHmdOrientation = CurHmdOrientation;
	}
	
	frame->Flags.bOrientationChanged = POV.bFollowHmdOrientation;
	
	if (POV.bFollowHmdPosition)
	{
		const FVector vEyePosition = frame->DeltaControlOrientation.RotateVector(CurHmdPosition);
		POV.Location += vEyePosition;
		//UE_LOG(LogHMD, Log, TEXT("!!!! %d"), GFrameNumber);
		frame->LastHmdPosition = CurHmdPosition;
	}
	frame->Flags.bPositionChanged = POV.bFollowHmdPosition;

#if !UE_BUILD_SHIPPING
	if (frame->Settings.Flags.bDrawTrackingCameraFrustum)
	{
		DrawDebugTrackingCameraFrustum(Camera->GetWorld(), POV.Location);
	}
#endif
}

#if !UE_BUILD_SHIPPING
void FOculusRiftHMD::DrawDebugTrackingCameraFrustum(UWorld* World, const FVector& ViewLocation)
{
	const FColor c = (HasValidTrackingPosition() ? FColor::Green : FColor::Red);
	FVector origin;
	FRotator rotation;
	float hfovDeg, vfovDeg, nearPlane, farPlane, cameraDist;
	GetPositionalTrackingCameraProperties(origin, rotation, hfovDeg, vfovDeg, cameraDist, nearPlane, farPlane);

	// Level line
	//DrawDebugLine(World, ViewLocation, FVector(ViewLocation.X + 1000, ViewLocation.Y, ViewLocation.Z), FColor::Blue);

	const float hfov = FMath::DegreesToRadians(hfovDeg * 0.5f);
	const float vfov = FMath::DegreesToRadians(vfovDeg * 0.5f);
	FVector coneTop(0, 0, 0);
	FVector coneBase1(-farPlane, farPlane * FMath::Tan(hfov), farPlane * FMath::Tan(vfov));
	FVector coneBase2(-farPlane,-farPlane * FMath::Tan(hfov), farPlane * FMath::Tan(vfov));
	FVector coneBase3(-farPlane,-farPlane * FMath::Tan(hfov),-farPlane * FMath::Tan(vfov));
	FVector coneBase4(-farPlane, farPlane * FMath::Tan(hfov),-farPlane * FMath::Tan(vfov));
	FMatrix m(FMatrix::Identity);
	m *= FRotationMatrix(rotation);
	m *= FTranslationMatrix(origin);
	m *= FTranslationMatrix(ViewLocation); // to location of pawn
	coneTop = m.TransformPosition(coneTop);
	coneBase1 = m.TransformPosition(coneBase1);
	coneBase2 = m.TransformPosition(coneBase2);
	coneBase3 = m.TransformPosition(coneBase3);
	coneBase4 = m.TransformPosition(coneBase4);

	// draw a point at the camera pos
	DrawDebugPoint(World, coneTop, 5, c);

	// draw main pyramid, from top to base
	DrawDebugLine(World, coneTop, coneBase1, c);
	DrawDebugLine(World, coneTop, coneBase2, c);
	DrawDebugLine(World, coneTop, coneBase3, c);
	DrawDebugLine(World, coneTop, coneBase4, c);
											  
	// draw base (far plane)				  
	DrawDebugLine(World, coneBase1, coneBase2, c);
	DrawDebugLine(World, coneBase2, coneBase3, c);
	DrawDebugLine(World, coneBase3, coneBase4, c);
	DrawDebugLine(World, coneBase4, coneBase1, c);

	// draw near plane
	FVector coneNear1(-nearPlane, nearPlane * FMath::Tan(hfov), nearPlane * FMath::Tan(vfov));
	FVector coneNear2(-nearPlane,-nearPlane * FMath::Tan(hfov), nearPlane * FMath::Tan(vfov));
	FVector coneNear3(-nearPlane,-nearPlane * FMath::Tan(hfov),-nearPlane * FMath::Tan(vfov));
	FVector coneNear4(-nearPlane, nearPlane * FMath::Tan(hfov),-nearPlane * FMath::Tan(vfov));
	coneNear1 = m.TransformPosition(coneNear1);
	coneNear2 = m.TransformPosition(coneNear2);
	coneNear3 = m.TransformPosition(coneNear3);
	coneNear4 = m.TransformPosition(coneNear4);
	DrawDebugLine(World, coneNear1, coneNear2, c);
	DrawDebugLine(World, coneNear2, coneNear3, c);
	DrawDebugLine(World, coneNear3, coneNear4, c);
	DrawDebugLine(World, coneNear4, coneNear1, c);

	// center line
	FVector centerLine(-cameraDist, 0, 0);
	centerLine = m.TransformPosition(centerLine);
	DrawDebugLine(World, coneTop, centerLine, FColor::Yellow);
	DrawDebugPoint(World, centerLine, 5, FColor::Yellow);
}
#endif // #if !UE_BUILD_SHIPPING

bool FOculusRiftHMD::IsChromaAbCorrectionEnabled() const
{
	auto frame = GetFrame();
	return (frame && frame->Settings.Flags.bChromaAbCorrectionEnabled);
}

ISceneViewExtension* FOculusRiftHMD::GetViewExtension()
{
	return this;
}

bool FOculusRiftHMD::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	auto frame = GetFrame();

	if (FParse::Command( &Cmd, TEXT("STEREO") ))
	{
		if (FParse::Command(&Cmd, TEXT("OFF")))
		{
			Flags.bNeedDisableStereo = true;
			return true;
		}
		else if (FParse::Command( &Cmd, TEXT("RESET") ))
		{
			Flags.bNeedUpdateStereoRenderingParams = true;
			Settings.Flags.bOverrideStereo = false;
			Settings.Flags.bOverrideIPD = false;
			Settings.Flags.bWorldToMetersOverride = false;
			Settings.NearClippingPlane = Settings.FarClippingPlane = 0.f;
			Settings.Flags.bClippingPlanesOverride = true; // forces zeros to be written to ini file to use default values next run
			Settings.InterpupillaryDistance = ovrHmd_GetFloat(Hmd, OVR_KEY_IPD, OVR_DEFAULT_IPD);
			//UpdateStereoRenderingParams();
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("SHOW")))
		{
			Ar.Logf(TEXT("stereo ipd=%.4f hfov=%.3f vfov=%.3f\n nearPlane=%.4f farPlane=%.4f"), GetInterpupillaryDistance(),
				FMath::RadiansToDegrees(Settings.HFOVInRadians), FMath::RadiansToDegrees(Settings.VFOVInRadians),
				(Settings.NearClippingPlane) ? Settings.NearClippingPlane : GNearClippingPlane, Settings.FarClippingPlane);
		}
		else
		{
			bool on, hmd = false;
			on = FParse::Command(&Cmd, TEXT("ON"));
			if (!on)
			{
				hmd = FParse::Command(&Cmd, TEXT("HMD"));
			}
			if (on || hmd)
			{
				if (!Settings.Flags.bHMDEnabled)
				{
					Ar.Logf(TEXT("HMD is disabled. Use 'hmd enable' to re-enable it."));
				}
				DoEnableStereo(true, hmd);
				return true;
			}
		}

		// normal configuration
		float val;
		if (FParse::Value( Cmd, TEXT("E="), val))
		{
			SetInterpupillaryDistance( val );
			Settings.Flags.bOverrideIPD = true;
			Flags.bNeedUpdateStereoRenderingParams = true;
		}
		if (FParse::Value(Cmd, TEXT("FCP="), val)) // far clipping plane override
		{
			Settings.FarClippingPlane = val;
			Settings.Flags.bClippingPlanesOverride = true;
		}
		if (FParse::Value(Cmd, TEXT("NCP="), val)) // near clipping plane override
		{
			Settings.NearClippingPlane = val;
			Settings.Flags.bClippingPlanesOverride = true;
		}
		if (FParse::Value(Cmd, TEXT("W2M="), val))
		{
			Settings.WorldToMetersScale = val;
			Settings.Flags.bWorldToMetersOverride = true;
		}

		// debug configuration
		if (Settings.Flags.bDevSettingsEnabled)
		{
			float fov;
			if (FParse::Value(Cmd, TEXT("HFOV="), fov))
			{
				Settings.HFOVInRadians = FMath::DegreesToRadians(fov);
				Settings.Flags.bOverrideStereo = true;
			}
			else if (FParse::Value(Cmd, TEXT("VFOV="), fov))
			{
				Settings.VFOVInRadians = FMath::DegreesToRadians(fov);
				Settings.Flags.bOverrideStereo = true;
			}
		}
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("HMD")))
	{
		if (FParse::Command(&Cmd, TEXT("ENABLE")))
		{
			EnableHMD(true);
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("DISABLE")))
		{
			EnableHMD(false);
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("VSYNC")))
		{
			if (FParse::Command( &Cmd, TEXT("RESET") ))
			{
				if (Settings.Flags.bStereoEnabled)
				{
					Settings.Flags.bVSync = Settings.Flags.bSavedVSync;
					Flags.bApplySystemOverridesOnStereo = true;
				}
				Settings.Flags.bOverrideVSync = false;
				return true;
			}
			else
			{
				if (FParse::Command(&Cmd, TEXT("ON")) || FParse::Command(&Cmd, TEXT("1")))
				{
					Settings.Flags.bVSync = true;
					Settings.Flags.bOverrideVSync = true;
					Flags.bApplySystemOverridesOnStereo = true;
					return true;
				}
				else if (FParse::Command(&Cmd, TEXT("OFF")) || FParse::Command(&Cmd, TEXT("0")))
				{
					Settings.Flags.bVSync = false;
					Settings.Flags.bOverrideVSync = true;
					Flags.bApplySystemOverridesOnStereo = true;
					return true;
				}
				else if (FParse::Command(&Cmd, TEXT("TOGGLE")) || FParse::Command(&Cmd, TEXT("")))
				{
					Settings.Flags.bVSync = !Settings.Flags.bVSync;
					Settings.Flags.bOverrideVSync = true;
					Flags.bApplySystemOverridesOnStereo = true;
					Ar.Logf(TEXT("VSync is currently %s"), (Settings.Flags.bVSync) ? TEXT("ON") : TEXT("OFF"));
					return true;
				}
			}
			return false;
		}
		else if (FParse::Command(&Cmd, TEXT("SP")) || 
				 FParse::Command(&Cmd, TEXT("SCREENPERCENTAGE")))
		{
			FString CmdName = FParse::Token(Cmd, 0);
			if (CmdName.IsEmpty())
				return false;

			if (!FCString::Stricmp(*CmdName, TEXT("RESET")))
			{
				Settings.Flags.bOverrideScreenPercentage = false;
				Flags.bApplySystemOverridesOnStereo = true;
			}
			else
			{
				float sp = FCString::Atof(*CmdName);
				if (sp >= 30 && sp <= 300)
				{
					Settings.Flags.bOverrideScreenPercentage = true;
					Settings.ScreenPercentage = sp;
					Flags.bApplySystemOverridesOnStereo = true;
				}
				else
				{
					Ar.Logf(TEXT("Value is out of range [30..300]"));
				}
			}
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("LP"))) // low persistence mode
		{
			FString CmdName = FParse::Token(Cmd, 0);
			if (!CmdName.IsEmpty())
			{
				if (!FCString::Stricmp(*CmdName, TEXT("ON")))
				{
					Settings.Flags.bLowPersistenceMode = true;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("OFF")))
				{
					Settings.Flags.bLowPersistenceMode = false;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("TOGGLE")))
				{
					Settings.Flags.bLowPersistenceMode = !Settings.Flags.bLowPersistenceMode;
				}
				else
				{
					return false;
				}
			}
			else
			{
				Settings.Flags.bLowPersistenceMode = !Settings.Flags.bLowPersistenceMode;
			}
			Flags.bNeedUpdateHmdCaps = true;
			Ar.Logf(TEXT("Low Persistence is currently %s"), (Settings.Flags.bLowPersistenceMode) ? TEXT("ON") : TEXT("OFF"));
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("MIRROR"))) // to mirror or not to mirror?...
		{
			FString CmdName = FParse::Token(Cmd, 0);
			if (!CmdName.IsEmpty())
			{
				if (!FCString::Stricmp(*CmdName, TEXT("ON")))
				{
					Settings.Flags.bMirrorToWindow = true;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("OFF")))
				{
					Settings.Flags.bMirrorToWindow = false;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("TOGGLE"))) 
				{
					Settings.Flags.bMirrorToWindow = !Settings.Flags.bMirrorToWindow;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("RESET")))
				{
					Settings.Flags.bMirrorToWindow = true;
					Settings.MirrorWindowSize.X = Settings.MirrorWindowSize.Y = 0;
				}
				else
				{
					int32 X = FCString::Atoi(*CmdName);
					const TCHAR* CmdTemp = FCString::Strchr(*CmdName, 'x') ? FCString::Strchr(*CmdName, 'x') + 1 : FCString::Strchr(*CmdName, 'X') ? FCString::Strchr(*CmdName, 'X') + 1 : TEXT("");
					int32 Y = FCString::Atoi(CmdTemp);

					Settings.MirrorWindowSize.X = X;
					Settings.MirrorWindowSize.Y = Y; 
				}
			}
			else
			{
				Settings.Flags.bMirrorToWindow = !Settings.Flags.bMirrorToWindow;
			}
			Flags.bNeedUpdateHmdCaps = true;
			Ar.Logf(TEXT("Mirroring is currently %s"), (Settings.Flags.bMirrorToWindow) ? TEXT("ON") : TEXT("OFF"));
			if (Settings.Flags.bMirrorToWindow && (Settings.MirrorWindowSize.X != 0 || Settings.MirrorWindowSize.Y != 0))
			{
				Ar.Logf(TEXT("Mirror window size is %d x %d"), Settings.MirrorWindowSize.X, Settings.MirrorWindowSize.Y);
			}
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("UPDATEONRT"))) // update on renderthread
		{
			FString CmdName = FParse::Token(Cmd, 0);
			if (!CmdName.IsEmpty())
			{
				if (!FCString::Stricmp(*CmdName, TEXT("ON")))
				{
					Settings.Flags.bUpdateOnRT = true;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("OFF")))
				{
					Settings.Flags.bUpdateOnRT = false;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("TOGGLE")))
				{
					Settings.Flags.bUpdateOnRT = !Settings.Flags.bUpdateOnRT;
				}
				else
				{
					return false;
				}
			}
			else
			{
				Settings.Flags.bUpdateOnRT = !Settings.Flags.bUpdateOnRT;
			}
			Ar.Logf(TEXT("Update on render thread is currently %s"), (Settings.Flags.bUpdateOnRT) ? TEXT("ON") : TEXT("OFF"));
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("OVERDRIVE"))) // 2 frame raise overdrive
		{
			FString CmdName = FParse::Token(Cmd, 0);
			if (!CmdName.IsEmpty())
			{
				if (!FCString::Stricmp(*CmdName, TEXT("ON")))
				{
					Settings.Flags.bOverdrive = true;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("OFF")))
				{
					Settings.Flags.bOverdrive = false;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("TOGGLE")))
				{
					Settings.Flags.bOverdrive = !Settings.Flags.bOverdrive;
				}
				else
				{
					return false;
				}
			}
			else
			{
				Settings.Flags.bOverdrive = !Settings.Flags.bOverdrive;
			}
			Flags.bNeedUpdateDistortionCaps = true;
			Ar.Logf(TEXT("Overdrive is currently %s"), (Settings.Flags.bOverdrive) ? TEXT("ON") : TEXT("OFF"));
			return true;
		}
#ifdef OVR_SDK_RENDERING
		else if (FParse::Command(&Cmd, TEXT("TIMEWARP"))) 
		{
			FString CmdName = FParse::Token(Cmd, 0);
			if (!CmdName.IsEmpty())
			{
				if (!FCString::Stricmp(*CmdName, TEXT("ON")))
				{
					Settings.Flags.bTimeWarp = true;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("OFF")))
				{
					Settings.Flags.bTimeWarp = false;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("TOGGLE")))
				{
					Settings.Flags.bTimeWarp = !Settings.Flags.bTimeWarp;
				}
				else
				{
					return false;
				}
			}
			else
			{
				Settings.Flags.bTimeWarp = !Settings.Flags.bTimeWarp;
			}
			Flags.bNeedUpdateDistortionCaps = true;
			Ar.Logf(TEXT("TimeWarp is currently %s"), (Settings.Flags.bTimeWarp) ? TEXT("ON") : TEXT("OFF"));
			return true;
		}
#endif // #ifdef OVR_SDK_RENDERING
#if !UE_BUILD_SHIPPING
		else if (FParse::Command(&Cmd, TEXT("UPDATEONGT"))) // update on game thread
		{
			FString CmdName = FParse::Token(Cmd, 0);
			if (!CmdName.IsEmpty())
			{
				if (!FCString::Stricmp(*CmdName, TEXT("ON")))
				{
					Settings.Flags.bDoNotUpdateOnGT = false;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("OFF")))
				{
					Settings.Flags.bDoNotUpdateOnGT = true;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("TOGGLE")))
				{
					Settings.Flags.bDoNotUpdateOnGT = !Settings.Flags.bDoNotUpdateOnGT;
				}
				else
				{
					return false;
				}
			}
			else
			{
				Settings.Flags.bDoNotUpdateOnGT = !Settings.Flags.bDoNotUpdateOnGT;
			}
			Ar.Logf(TEXT("Update on game thread is currently %s"), (!Settings.Flags.bDoNotUpdateOnGT) ? TEXT("ON") : TEXT("OFF"));
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("STATS"))) // status / statistics
		{
			Settings.Flags.bShowStats = !Settings.Flags.bShowStats;
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("GRID"))) // grid
		{
			Settings.Flags.bDrawGrid = !Settings.Flags.bDrawGrid;
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("PROFILE"))) // profile
		{
			Settings.Flags.bProfiling = !Settings.Flags.bProfiling;
			Flags.bNeedUpdateDistortionCaps = true;
			Ar.Logf(TEXT("Profiling mode is currently %s"), (Settings.Flags.bProfiling) ? TEXT("ON") : TEXT("OFF"));
			return true;
		}
#endif //UE_BUILD_SHIPPING
	}
	else if (FParse::Command(&Cmd, TEXT("HMDMAG")))
	{
		if (FParse::Command(&Cmd, TEXT("ON")))
		{
			Settings.Flags.bYawDriftCorrectionEnabled = true;
			Flags.bNeedUpdateHmdCaps = true;
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("OFF")))
		{
			Settings.Flags.bYawDriftCorrectionEnabled = false;
			Flags.bNeedUpdateHmdCaps = true;
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("SHOW")))
		{
			Ar.Logf(TEXT("mag %s"), Settings.Flags.bYawDriftCorrectionEnabled ? 
				TEXT("on") : TEXT("off"));
			return true;
		}
		return false;
	}
	else if (FParse::Command(&Cmd, TEXT("HMDWARP")))
    {
#ifndef OVR_SDK_RENDERING
        if (FParse::Command( &Cmd, TEXT("ON") ))
        {
            Settings.Flags.bHmdDistortion = true;
            return true;
        }
        else if (FParse::Command( &Cmd, TEXT("OFF") ))
        {
            Settings.Flags.bHmdDistortion = false;
            return true;
        }
#endif //OVR_SDK_RENDERING
		if (FParse::Command(&Cmd, TEXT("CHA")))
		{
			Settings.Flags.bChromaAbCorrectionEnabled = true;
			Flags.bNeedUpdateDistortionCaps = true;
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("NOCHA")))
		{
			Settings.Flags.bChromaAbCorrectionEnabled = false;
			Flags.bNeedUpdateDistortionCaps = true;
			return true;
		}
		else if (FParse::Command( &Cmd, TEXT("HQ") ))
		{
			// High quality distortion
			if (FParse::Command( &Cmd, TEXT("ON") ))
			{
				Settings.Flags.bHQDistortion = true;
			}
			else if (FParse::Command(&Cmd, TEXT("OFF")))
			{
				Settings.Flags.bHQDistortion = false;
			}
			else
			{
				Settings.Flags.bHQDistortion = !Settings.Flags.bHQDistortion;
			}
			Ar.Logf(TEXT("High quality distortion is currently %s"), (Settings.Flags.bHQDistortion) ? TEXT("ON") : TEXT("OFF"));
			Flags.bNeedUpdateDistortionCaps = true;
			return true;
		}

		if (FParse::Command(&Cmd, TEXT("SHOW")))
		{
			Ar.Logf(TEXT("hmdwarp %s sc=%f %s"), (Settings.Flags.bHmdDistortion ? TEXT("on") : TEXT("off"))
				, Settings.IdealScreenPercentage / 100.f
				, (Settings.Flags.bChromaAbCorrectionEnabled ? TEXT("cha") : TEXT("nocha")));
		}
		return true;
    }
	else if (FParse::Command(&Cmd, TEXT("HMDPOS")))
	{
		if (FParse::Command(&Cmd, TEXT("RESET")))
		{
			FString YawStr = FParse::Token(Cmd, 0);
			float yaw = 0.f;
			if (!YawStr.IsEmpty())
			{
				yaw = FCString::Atof(*YawStr);
			}
			ResetOrientationAndPosition(yaw);
			return true;
		}
#ifdef OVR_VISION_ENABLED
		else if (FParse::Command(&Cmd, TEXT("ON")) || FParse::Command(&Cmd, TEXT("ENABLE")))
		{
			Settings.Flags.bHmdPosTracking = true;
			Flags.bNeedUpdateHmdCaps = true;
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("OFF")) || FParse::Command(&Cmd, TEXT("DISABLE")))
		{
			Settings.Flags.bHmdPosTracking = false;
			Flags.bNeedUpdateHmdCaps = true;
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("TOGGLE")))
		{
			Settings.Flags.bHmdPosTracking = !Settings.Flags.bHmdPosTracking;
			Flags.bNeedUpdateHmdCaps = true;
			return true;
		}
#if !UE_BUILD_SHIPPING
		else if (FParse::Command(&Cmd, TEXT("SHOWCAMERA")))
		{
			if (FParse::Command(&Cmd, TEXT("OFF")))
			{
				Settings.Flags.bDrawTrackingCameraFrustum = false;
				return true;
			}
			if (FParse::Command(&Cmd, TEXT("ON")))
			{
				Settings.Flags.bDrawTrackingCameraFrustum = true;
				return true;
			}
			else 
			{
				Settings.Flags.bDrawTrackingCameraFrustum = !Settings.Flags.bDrawTrackingCameraFrustum;
				return true;
			}
		}
#endif // #if !UE_BUILD_SHIPPING
		else if (FParse::Command(&Cmd, TEXT("SHOW")))
		{
			Ar.Logf(TEXT("hmdpos is %s, vision='%s'"), 
				(Settings.Flags.bHmdPosTracking ? TEXT("enabled") : TEXT("disabled")),
				((frame && frame->Flags.bHaveVisionTracking) ? TEXT("active") : TEXT("lost")));
			return true;
		}
#endif // #ifdef OVR_VISION_ENABLED
	}
	else if (FParse::Command(&Cmd, TEXT("OCULUSDEV")))
	{
		if (FParse::Command(&Cmd, TEXT("ON")))
		{
			Settings.Flags.bDevSettingsEnabled = true;
		}
		else if (FParse::Command(&Cmd, TEXT("OFF")))
		{
			Settings.Flags.bDevSettingsEnabled = false;
		}
		UpdateStereoRenderingParams();
		return true;
	}
	if (FParse::Command(&Cmd, TEXT("MOTION")))
	{
		FString CmdName = FParse::Token(Cmd, 0);
		if (CmdName.IsEmpty())
			return false;

		if (!FCString::Stricmp(*CmdName, TEXT("ON")))
		{
			Settings.Flags.bHeadTrackingEnforced = false;
			return true;
		}
		else if (!FCString::Stricmp(*CmdName, TEXT("ENFORCE")))
		{
			Settings.Flags.bHeadTrackingEnforced = !Settings.Flags.bHeadTrackingEnforced;
			if (!Settings.Flags.bHeadTrackingEnforced)
			{
				ResetControlRotation();
			}
			return true;
		}
		else if (!FCString::Stricmp(*CmdName, TEXT("RESET")))
		{
			Settings.Flags.bHeadTrackingEnforced = false;
			ResetControlRotation();
			return true;
		}
		return false;
	}
#ifndef OVR_SDK_RENDERING
	else if (FParse::Command(&Cmd, TEXT("SETFINISHFRAME")))
	{
		static IConsoleVariable* CFinishFrameVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FinishCurrentFrame"));

		if (FParse::Command(&Cmd, TEXT("ON")))
		{
			Settings.Flags.bAllowFinishCurrentFrame = true;
			if (Settings.Flags.bStereoEnabled)
			{
				CFinishFrameVar->Set(Settings.Flags.bAllowFinishCurrentFrame != 0);
			}
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("OFF")))
		{
			Settings.Flags.bAllowFinishCurrentFrame = false;
			if (Settings.Flags.bStereoEnabled)
			{
				CFinishFrameVar->Set(Settings.Flags.bAllowFinishCurrentFrame != 0);
			}
			return true;
		}
		return false;
	}
#endif
	else if (FParse::Command(&Cmd, TEXT("UNCAPFPS")))
	{
		GEngine->bSmoothFrameRate = false;
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("OVRVERSION")))
	{
		Ar.Logf(*GetVersionString());
		return true;
	}

	return false;
}

void FOculusRiftHMD::OnScreenModeChange(EWindowMode::Type WindowMode)
{
	EnableStereo(WindowMode != EWindowMode::Windowed);
	UpdateStereoRenderingParams();
}

FString FOculusRiftHMD::GetVersionString() const
{
	const char* Results = ovr_GetVersionString();
	FString s = FString::Printf(TEXT("%s, LibOVR: %s, built %s, %s"), *GEngineVersion.ToString(), UTF8_TO_TCHAR(Results),
		UTF8_TO_TCHAR(__DATE__), UTF8_TO_TCHAR(__TIME__));
	return s;
}

void FOculusRiftHMD::RecordAnalytics()
{
	if (FEngineAnalytics::IsAvailable())
	{
		// prepare and send analytics data
		TArray<FAnalyticsEventAttribute> EventAttributes;

		IHeadMountedDisplay::MonitorInfo MonitorInfo;
		GetHMDMonitorInfo(MonitorInfo);
		if (Hmd)
		{
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DeviceName"), FString::Printf(TEXT("%s - %s"), ANSI_TO_TCHAR(Hmd->Manufacturer), ANSI_TO_TCHAR(Hmd->ProductName))));
		}
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DisplayDeviceName"), MonitorInfo.MonitorName));
#if PLATFORM_MAC // On OS X MonitorId is the CGDirectDisplayID aka uint64, not a string
		FString DisplayId(FString::Printf(TEXT("%llu"), MonitorInfo.MonitorId));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DisplayId"), DisplayId));
#else
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DisplayId"), MonitorInfo.MonitorId));
#endif
		FString MonResolution(FString::Printf(TEXT("(%d, %d)"), MonitorInfo.ResolutionX, MonitorInfo.ResolutionY));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Resolution"), MonResolution));

		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ChromaAbCorrectionEnabled"), Settings.Flags.bChromaAbCorrectionEnabled));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("MagEnabled"), Settings.Flags.bYawDriftCorrectionEnabled));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DevSettingsEnabled"), Settings.Flags.bDevSettingsEnabled));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("OverrideInterpupillaryDistance"), Settings.Flags.bOverrideIPD));
		if (Settings.Flags.bOverrideIPD)
		{
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("InterpupillaryDistance"), GetInterpupillaryDistance()));
		}
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("OverrideStereo"), Settings.Flags.bOverrideStereo));
		if (Settings.Flags.bOverrideStereo)
		{
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("HFOV"), Settings.HFOVInRadians));
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("VFOV"), Settings.VFOVInRadians));
		}
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("OverrideVSync"), Settings.Flags.bOverrideVSync));
		if (Settings.Flags.bOverrideVSync)
		{
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("VSync"), Settings.Flags.bVSync));
		}
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("OverrideScreenPercentage"), Settings.Flags.bOverrideScreenPercentage));
		if (Settings.Flags.bOverrideScreenPercentage)
		{
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ScreenPercentage"), Settings.ScreenPercentage));
		}
		if (Settings.Flags.bWorldToMetersOverride)
		{
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("WorldToMetersScale"), Settings.WorldToMetersScale));
		}
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("InterpupillaryDistance"), Settings.InterpupillaryDistance));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("TimeWarp"), Settings.Flags.bTimeWarp));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("AllowFinishCurrentFrame"), Settings.Flags.bAllowFinishCurrentFrame));
#ifdef OVR_VISION_ENABLED
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("HmdPosTracking"), Settings.Flags.bHmdPosTracking));
#endif
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("LowPersistenceMode"), Settings.Flags.bLowPersistenceMode));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("UpdateOnRT"), Settings.Flags.bUpdateOnRT));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Overdrive"), Settings.Flags.bOverdrive));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("MirrorToWindow"), Settings.Flags.bMirrorToWindow));

		FString OutStr(TEXT("Editor.VR.DeviceInitialised"));
		FEngineAnalytics::GetProvider().RecordEvent(OutStr, EventAttributes);
	}
}

bool FOculusRiftHMD::IsPositionalTrackingEnabled() const
{
#ifdef OVR_VISION_ENABLED
	auto frame = GetFrame();
	return (frame && frame->Settings.Flags.bHmdPosTracking);
#else
	return false;
#endif // #ifdef OVR_VISION_ENABLED
}

bool FOculusRiftHMD::EnablePositionalTracking(bool enable)
{
#ifdef OVR_VISION_ENABLED
	Settings.Flags.bHmdPosTracking = enable;
	return enable;
#else
	OVR_UNUSED(enable);
	return false;
#endif
}

class FSceneViewport* FOculusRiftHMD::FindSceneViewport()
{
	if (!GIsEditor)
	{
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		return GameEngine->SceneViewport.Get();
	}
#if WITH_EDITOR
	else
	{
		UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
		return (FSceneViewport*)(EditorEngine->GetPIEViewport());
	}
#endif
	return nullptr;
}

//---------------------------------------------------
// Oculus Rift IStereoRendering Implementation
//---------------------------------------------------
bool FOculusRiftHMD::IsStereoEnabled() const
{
	if (IsInGameThread())
	{
		auto frame = GetFrame();
		if (frame)
		{
			return (frame->Settings.IsStereoEnabled());
		}
		else
		{
			return (!Frame.Flags.bFrameStarted && Settings.IsStereoEnabled());
		}
	}
	else if (IsInRenderingThread())
	{
		return RenderParams.Frame.Settings.IsStereoEnabled();
	}
	else
	{
		check(0);
	}
	return false;
}

bool FOculusRiftHMD::EnableStereo(bool bStereo)
{
	return DoEnableStereo(bStereo, true);
}

bool FOculusRiftHMD::DoEnableStereo(bool bStereo, bool bApplyToHmd)
{
	check(IsInGameThread());

	FSceneViewport* SceneVP = FindSceneViewport();
	if (bStereo && (!SceneVP || !SceneVP->IsStereoRenderingAllowed()))
	{
		return false;
	}

	bool stereoEnabled = (Settings.Flags.bHMDEnabled) ? bStereo : false;

	if ((Settings.Flags.bStereoEnabled && stereoEnabled) || (!Settings.Flags.bStereoEnabled && !stereoEnabled))
	{
		// already in the desired mode
		return Settings.Flags.bStereoEnabled;
	}
	if (!stereoEnabled)
	{
		Frame.Settings.Flags.bStereoEnabled = false;
	}

	bool wasFullscreenAllowed = IsFullscreenAllowed();
	if (OnOculusStateChange(stereoEnabled))
	{
		Settings.Flags.bStereoEnabled = stereoEnabled;

		if (SceneVP)
		{
			if (!IsFullscreenAllowed() && stereoEnabled)
			{
				if (Hmd)
				{
					// keep window size, but set viewport size to Rift resolution
					SceneVP->SetViewportSize(Hmd->Resolution.w, Hmd->Resolution.h);
				}
			}
			else if ((!wasFullscreenAllowed && !stereoEnabled))
			{
				// restoring original viewport size (to be equal to window size).
				TSharedPtr<SWindow> Window = SceneVP->FindWindow();
				if (Window.IsValid())
				{
					FVector2D size = Window->GetSizeInScreen();
					SceneVP->SetViewportSize(size.X, size.Y);
					Window->SetViewportSizeDrivenByWindow(true);
				}
			}

			if (SceneVP)
			{
				TSharedPtr<SWindow> Window = SceneVP->FindWindow();
				if (Window.IsValid())
				{
					FVector2D size = Window->GetSizeInScreen();

					if (bApplyToHmd && IsFullscreenAllowed())
					{
						SceneVP->SetViewportSize(size.X, size.Y);
						Window->SetViewportSizeDrivenByWindow(true);

						if (stereoEnabled)
						{
							EWindowMode::Type wm = (!GIsEditor) ? EWindowMode::Fullscreen : EWindowMode::WindowedFullscreen;
							FVector2D size = Window->GetSizeInScreen();
							SceneVP->ResizeFrame(size.X, size.Y, wm, 0, 0);
						}
						else
						{
							// In Editor we cannot use ResizeFrame trick since it is called too late and App::IsGame
							// returns false.
							if (GIsEditor)
							{
								FSlateRect PreFullScreenRect;
								PopPreFullScreenRect(PreFullScreenRect);
								if (PreFullScreenRect.GetSize().X > 0 && PreFullScreenRect.GetSize().Y > 0 && IsFullscreenAllowed())
								{
									Window->MoveWindowTo(FVector2D(PreFullScreenRect.Left, PreFullScreenRect.Top));
								}
							}
							else
							{
								FVector2D size = Window->GetSizeInScreen();
								SceneVP->ResizeFrame(size.X, size.Y, EWindowMode::Windowed, 0, 0);
							}
						}
					}
					else if (!IsFullscreenAllowed())
					{
						// a special case when 'stereo on' or 'stereo hmd' is used in Direct mode. We must set proper window mode, otherwise
						// it will be lost once window loses and regains the focus.
						FSystemResolution::RequestResolutionChange(size.X, size.Y, (stereoEnabled) ? EWindowMode::WindowedMirror : EWindowMode::Windowed);
					}
				}
			}
		}
	}
	return Settings.Flags.bStereoEnabled;
}

void FOculusRiftHMD::ResetControlRotation() const
{
	// Switching back to non-stereo mode: reset player rotation and aim.
	// Should we go through all playercontrollers here?
	APlayerController* pc = GEngine->GetFirstLocalPlayerController(GWorld);
	if (pc)
	{
		// Reset Aim? @todo
		FRotator r = pc->GetControlRotation();
		r.Normalize();
		// Reset roll and pitch of the player
		r.Roll = 0;
		r.Pitch = 0;
		pc->SetControlRotation(r);
	}
}

bool FOculusRiftHMD::OnOculusStateChange(bool bIsEnabledNow)
{
	Settings.Flags.bHmdDistortion = bIsEnabledNow;
	if (!bIsEnabledNow)
	{
		// Switching from stereo
		ReleaseDevice();

		ResetControlRotation();
		RestoreSystemValues();
		return true;
	}
	else
	{
		// Switching to stereo
		InitDevice();

		if (Hmd)
		{
			SaveSystemValues();
			Flags.bApplySystemOverridesOnStereo = true;

			UpdateStereoRenderingParams();
			return true;
		}
	}
	return false;
}

void FOculusRiftHMD::ApplySystemOverridesOnStereo(bool bForce)
{
	if (Settings.Flags.bStereoEnabled || bForce)
	{
		// Set the current VSync state
		if (Settings.Flags.bOverrideVSync)
		{
			static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
			CVSyncVar->Set(Settings.Flags.bVSync != 0);
		}
		else
		{
			static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
			Settings.Flags.bVSync = CVSyncVar->GetInt() != 0;
		}
		UpdateHmdCaps();

#ifndef OVR_SDK_RENDERING
		static IConsoleVariable* CFinishFrameVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FinishCurrentFrame"));
		CFinishFrameVar->Set(Settings.Flags.bAllowFinishCurrentFrame != 0);
#endif
	}
}

void FOculusRiftHMD::SaveSystemValues()
{
	static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
	Settings.Flags.bSavedVSync = CVSyncVar->GetInt() != 0;

	static IConsoleVariable* CScrPercVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	Settings.SavedScrPerc = CScrPercVar->GetFloat();
}

void FOculusRiftHMD::RestoreSystemValues()
{
	static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
	CVSyncVar->Set(Settings.Flags.bSavedVSync != 0);

	static IConsoleVariable* CScrPercVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	CScrPercVar->Set(Settings.SavedScrPerc);

	static IConsoleVariable* CFinishFrameVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FinishCurrentFrame"));
	CFinishFrameVar->Set(false);
}

void FOculusRiftHMD::UpdateScreenSettings(const FViewport*)
{
	auto frame = GetFrame();
	if (frame && frame->Flags.bScreenPercentageEnabled)
	{
		// Set the current ScreenPercentage state
		static IConsoleVariable* CScrPercVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
		float DesiredSceeenPercentage;
		if (frame->Settings.Flags.bOverrideScreenPercentage)
		{
			DesiredSceeenPercentage = frame->Settings.ScreenPercentage;
		}
		else
		{
			DesiredSceeenPercentage = frame->Settings.IdealScreenPercentage;
		}
		if (FMath::RoundToInt(CScrPercVar->GetFloat()) != FMath::RoundToInt(DesiredSceeenPercentage))
		{
			CScrPercVar->Set(DesiredSceeenPercentage);
		}
	}
}

void FOculusRiftHMD::AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
    SizeX = SizeX / 2;
    if( StereoPass == eSSP_RIGHT_EYE )
    {
        X += SizeX;
    }
}

void FOculusRiftHMD::CalculateStereoViewOffset(const EStereoscopicPass StereoPassType, const FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation)
{
	check(WorldToMeters != 0.f);
	check(Hmd);

	const int idx = (StereoPassType == eSSP_LEFT_EYE) ? 0 : 1;

	if (IsInGameThread())
	{
		auto frame = GetFrame();
		if (!frame)
		{
			return;
		}

		// This method is called from GetProjectionData on a game thread.
		// The modified ViewLocation is used ONLY for ViewMatrix composition, it is not
		// stored modified in the ViewInfo. ViewInfo.ViewLocation remains unmodified.

		if( StereoPassType != eSSP_FULL || frame->Settings.Flags.bHeadTrackingEnforced )
		{
			if (!frame->Flags.bOrientationChanged)
			{
				UE_LOG(LogHMD, Log, TEXT("Orientation wasn't applied to a camera in frame %d"), GFrameCounter);
			}

			FVector CurEyePosition;
			FQuat CurEyeOrient;
			PoseToOrientationAndPosition(frame->EyeRenderPose[idx], CurEyeOrient, CurEyePosition, *frame);

			FVector HeadPosition = FVector::ZeroVector;
			// If we use PlayerController->bFollowHmd then we must apply full EyePosition (HeadPosition == 0).
			// Otherwise, we will apply only a difference between EyePosition and HeadPosition, since
			// HeadPosition is supposedly already applied.
			if (!frame->Flags.bPlayerControllerFollowsHmd)
			{
				FQuat HeadOrient;
				PoseToOrientationAndPosition(frame->HeadPose, HeadOrient, HeadPosition, *frame);
			}

			FVector HmdToEyeOffset;
			// apply stereo disparity to ViewLocation. Note, ViewLocation already contains HeadPose.Position, thus
			// we just need to apply delta between EyeRenderPose.Position and the HeadPose.Position. 
			// EyeRenderPose and HeadPose are captured by the same call to GetEyePoses.
			HmdToEyeOffset = CurEyePosition - HeadPosition;

			// The HMDPosition already has HMD orientation applied.
			// Apply rotational difference between HMD orientation and ViewRotation
			// to HMDPosition vector. 
			const FVector vEyePosition = frame->DeltaControlOrientation.RotateVector(HmdToEyeOffset);
			ViewLocation += vEyePosition;
		}
	}
}

void FOculusRiftHMD::ResetOrientationAndPosition(float yaw)
{
	ResetOrientation(yaw);
	ResetPosition();
}

void FOculusRiftHMD::ResetOrientation(float yaw)
{
	const ovrTrackingState ss = ovrHmd_GetTrackingState(Hmd, ovr_GetTimeInSeconds());
	const ovrPosef& pose = ss.HeadPose.ThePose;
	const OVR::Quatf orientation = OVR::Quatf(pose.Orientation);

	FRotator ViewRotation;
	ViewRotation = FRotator(ToFQuat(orientation));
	ViewRotation.Pitch = 0;
	ViewRotation.Roll = 0;

	if (yaw != 0.f)
	{
		// apply optional yaw offset
		ViewRotation.Yaw -= yaw;
		ViewRotation.Normalize();
	}

	Settings.BaseOrientation = ViewRotation.Quaternion();
}

void FOculusRiftHMD::ResetPosition()
{
	// Reset position
#ifdef OVR_VISION_ENABLED
	const ovrTrackingState ss = ovrHmd_GetTrackingState(Hmd, ovr_GetTimeInSeconds());
	const ovrPosef& pose = ss.HeadPose.ThePose;
	const OVR::Quatf orientation = OVR::Quatf(pose.Orientation);

	Settings.BaseOffset = pose.Position;
#else
	Settings.BaseOffset = OVR::Vector3f(0, 0, 0);
#endif // #ifdef OVR_VISION_ENABLED
}

void FOculusRiftHMD::SetClippingPlanes(float NCP, float FCP)
{
	Settings.NearClippingPlane = NCP;
	Settings.FarClippingPlane = FCP;
	Settings.Flags.bClippingPlanesOverride = false; // prevents from saving in .ini file
}

void FOculusRiftHMD::SetBaseRotation(const FRotator& BaseRot)
{
	SetBaseOrientation(BaseRot.Quaternion());
}

FRotator FOculusRiftHMD::GetBaseRotation() const
{
	return GetBaseOrientation().Rotator();
}

void FOculusRiftHMD::SetBaseOrientation(const FQuat& BaseOrient)
{
	Settings.BaseOrientation = BaseOrient;
}

FQuat FOculusRiftHMD::GetBaseOrientation() const
{
	return Settings.BaseOrientation;
}

void FOculusRiftHMD::SetPositionOffset(const FVector& PosOff)
{
	Settings.PositionOffset = PosOff;
}

FVector FOculusRiftHMD::GetPositionOffset() const
{
	return Settings.PositionOffset;
}

FMatrix FOculusRiftHMD::GetStereoProjectionMatrix(EStereoscopicPass StereoPassType, const float FOV) const
{
	auto frame = GetFrame();
	check(frame);
	check(IsStereoEnabled());

	const int idx = (StereoPassType == eSSP_LEFT_EYE) ? 0 : 1;

	FMatrix proj = ToFMatrix(frame->Settings.EyeProjectionMatrices[idx]);

	// correct far and near planes for reversed-Z projection matrix
	const float InNearZ = (frame->Settings.NearClippingPlane) ? frame->Settings.NearClippingPlane : GNearClippingPlane;
	const float InFarZ = (frame->Settings.FarClippingPlane) ? frame->Settings.FarClippingPlane : GNearClippingPlane;
	proj.M[3][3] = 0.0f;
	proj.M[2][3] = 1.0f;

	proj.M[2][2] = (InNearZ == InFarZ) ? 0.0f    : InNearZ / (InNearZ - InFarZ);
	proj.M[3][2] = (InNearZ == InFarZ) ? InNearZ : -InFarZ * InNearZ / (InNearZ - InFarZ);

	return proj;
}

void FOculusRiftHMD::InitCanvasFromView(FSceneView* InView, UCanvas* Canvas)
{
	// This is used for placing small HUDs (with names)
	// over other players (for example, in Capture Flag).
	// HmdOrientation should be initialized by GetCurrentOrientation (or
	// user's own value).
	FSceneView HmdView(*InView);

	const FQuat DeltaOrient = HmdView.BaseHmdOrientation.Inverse() * Canvas->HmdOrientation;
	HmdView.ViewRotation = FRotator(HmdView.ViewRotation.Quaternion() * DeltaOrient);

	HmdView.UpdateViewMatrix();
	Canvas->ViewProjectionMatrix = HmdView.ViewProjectionMatrix;
}

void FOculusRiftHMD::PushViewportCanvas(EStereoscopicPass StereoPass, FCanvas *InCanvas, UCanvas *InCanvasObject, FViewport *InViewport) const
{
	auto frame = GetFrame();
	check(frame);
	if (StereoPass != eSSP_FULL)
	{
		int32 SideSizeX = FMath::TruncToInt(InViewport->GetSizeXY().X * 0.5);

		// !AB: temporarily assuming all canvases are at Z = 1.0f and calculating
		// stereo disparity right here. Stereo disparity should be calculated for each
		// element separately, considering its actual Z-depth.
		const float Z = 1.0f;
		float Disparity = Z * frame->Settings.HudOffset + Z * frame->Settings.CanvasCenterOffset;
		if (StereoPass == eSSP_RIGHT_EYE)
			Disparity = -Disparity;

		if (InCanvasObject)
		{
			//InCanvasObject->Init();
			InCanvasObject->SizeX = SideSizeX;
			InCanvasObject->SizeY = InViewport->GetSizeXY().Y;
			InCanvasObject->SetView(NULL);
			InCanvasObject->Update();
		}

		float ScaleFactor = 1.0f;
		FScaleMatrix m(ScaleFactor);

		InCanvas->PushAbsoluteTransform(FTranslationMatrix(
			FVector(((StereoPass == eSSP_RIGHT_EYE) ? SideSizeX : 0) + Disparity, 0, 0))*m);
	}
	else
	{
		FMatrix m;
		m.SetIdentity();
		InCanvas->PushAbsoluteTransform(m);
	}
}

void FOculusRiftHMD::PushViewCanvas(EStereoscopicPass StereoPass, FCanvas *InCanvas, UCanvas *InCanvasObject, FSceneView *InView) const
{
	if (StereoPass != eSSP_FULL)
	{
		if (InCanvasObject)
		{
			//InCanvasObject->Init();
			InCanvasObject->SizeX = InView->ViewRect.Width();
			InCanvasObject->SizeY = InView->ViewRect.Height();
			InCanvasObject->SetView(InView);
			InCanvasObject->Update();
		}

		InCanvas->PushAbsoluteTransform(FTranslationMatrix(FVector(InView->ViewRect.Min.X, InView->ViewRect.Min.Y, 0)));
	}
	else
	{
		FMatrix m;
		m.SetIdentity();
		InCanvas->PushAbsoluteTransform(m);
	}
}


//---------------------------------------------------
// Oculus Rift ISceneViewExtension Implementation
//---------------------------------------------------

void FOculusRiftHMD::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	auto frame = GetFrame();
	check(frame);

	InViewFamily.EngineShowFlags.MotionBlur = 0;
#ifndef OVR_SDK_RENDERING
	InViewFamily.EngineShowFlags.HMDDistortion = frame->Settings.Flags.bHmdDistortion;
#else
	InViewFamily.EngineShowFlags.HMDDistortion = false;
#endif
	InViewFamily.EngineShowFlags.StereoRendering = IsStereoEnabled();

	frame->Flags.bScreenPercentageEnabled = InViewFamily.EngineShowFlags.ScreenPercentage;
}

void FOculusRiftHMD::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	auto frame = GetFrame();
	check(frame);

	InView.BaseHmdOrientation = frame->LastHmdOrientation;
	InView.BaseHmdLocation = frame->LastHmdPosition;

#ifndef OVR_SDK_RENDERING
	InViewFamily.bUseSeparateRenderTarget = false;

	// check and save texture size. 
	if (InView.StereoPass == eSSP_LEFT_EYE)
	{
		if (Settings.GetViewportSize() != InView.ViewRect.Size())
		{
			Settings.SetViewportSize(InView.ViewRect.Size().X, InView.ViewRect.Size().Y);
			Flags.bNeedUpdateStereoRenderingParams = true;
		}
	}
#else
	InViewFamily.bUseSeparateRenderTarget = ShouldUseSeparateRenderTarget();
#endif
}

bool FOculusRiftHMD::IsHeadTrackingAllowed() const
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		UEditorEngine* EdEngine = Cast<UEditorEngine>(GEngine);
		return Hmd && (!EdEngine || EdEngine->bUseVRPreviewForPlayWorld) &&	(Settings.Flags.bHeadTrackingEnforced || GEngine->IsStereoscopic3D());
	}
#endif//WITH_EDITOR
	auto frame = GetFrame();
	return (frame && Hmd && (frame->Settings.Flags.bHeadTrackingEnforced || GEngine->IsStereoscopic3D()));
}

//---------------------------------------------------
// Oculus Rift Specific
//---------------------------------------------------

FOculusRiftHMD::FOculusRiftHMD()
	:
	  Hmd(nullptr)
	, RenderParams(getThis())
{
	Flags.Raw = 0;
	Flags.bNeedUpdateStereoRenderingParams = true;
	Frame.Reset();
	DeltaControlRotation = FRotator::ZeroRotator;

	if (GIsEditor)
	{
		Settings.Flags.bOverrideScreenPercentage = true;
		Settings.ScreenPercentage = 100;
	}
	OSWindowHandle = nullptr;
	Startup();
}

FOculusRiftHMD::~FOculusRiftHMD()
{
	Shutdown();
}

bool FOculusRiftHMD::IsInitialized() const
{
	return (Settings.Flags.InitStatus & FSettings::eInitialized) != 0;
}

void FOculusRiftHMD::Startup()
{
	if ((!IsRunningGame() && !GIsEditor) || (Settings.Flags.InitStatus & FSettings::eStartupExecuted) != 0)
	{
		// do not initialize plugin for server or if it was already initialized
		return;
	}
	Settings.Flags.InitStatus |= FSettings::eStartupExecuted;

	// Initializes LibOVR. This LogMask_All enables maximum logging.
	// Custom allocator can also be specified here.
	// Actually, most likely, the ovr_Initialize is already called from PreInit.
	ovr_Initialize();

#if !UE_BUILD_SHIPPING
	// Should be changed to CAPI when available.
	static OculusLog OcLog;
	OVR::Log::SetGlobalLog(&OcLog);
#endif //#if !UE_BUILD_SHIPPING

	// Uncap fps to enable FPS higher than 62
	GEngine->bSmoothFrameRate = false;

	SaveSystemValues();

#ifdef OVR_SDK_RENDERING
#if defined(OVR_D3D_VERSION) && (OVR_D3D_VERSION == 11)
	if (IsPCPlatform(GMaxRHIShaderPlatform) && !IsOpenGLPlatform(GMaxRHIShaderPlatform))
	{
		pD3D11Bridge = new D3D11Bridge(this);
	}
#endif
#if defined(OVR_GL)
	if (IsOpenGLPlatform(GMaxRHIShaderPlatform))
	{
		pOGLBridge = new OGLBridge(this);
	}
#endif
#endif // #ifdef OVR_SDK_RENDERING

	if (GIsEditor)
	{
		Settings.Flags.bHeadTrackingEnforced = true;
		//AlternateFrameRateDivider = 2;
	}

	bool forced = true;
	if (!FParse::Param(FCommandLine::Get(), TEXT("forcedrift")))
	{
		InitDevice();
		forced = false;
	}

	if (forced || Hmd)
	{
		Settings.Flags.InitStatus |= FSettings::eInitialized;

		UE_LOG(LogHMD, Log, TEXT("Oculus plugin initialized. Version: %s"), *GetVersionString());
	}
}

void FOculusRiftHMD::Shutdown()
{
	if (!(Settings.Flags.InitStatus & FSettings::eStartupExecuted))
	{
		return;
	}

	RestoreSystemValues();

#ifdef OVR_SDK_RENDERING
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(ShutdownRen,
	FOculusRiftHMD*, Plugin, this,
	{
		Plugin->ShutdownRendering();
	});
#endif // OVR_SDK_RENDERING
	ReleaseDevice();
	
	Settings.Reset();
	Frame.Reset();
	RenderFrame.Reset();

	{
		OVR::Lock::Locker lock(&UpdateOnRTLock);
		RenderParams.Reset();
	}
	ovr_Shutdown();
	Settings.Flags.InitStatus = 0;
	UE_LOG(LogHMD, Log, TEXT("Oculus shutdown."));
}

bool FOculusRiftHMD::InitDevice()
{
	if (Hmd)
	{
		const ovrTrackingState ss = ovrHmd_GetTrackingState(Hmd, ovr_GetTimeInSeconds());
		if (!(ss.StatusFlags & ovrStatus_HmdConnected))
		{
			ReleaseDevice();
		}
		else
		{
			return true; // already created and present
		}
	}
	check(!Hmd);

	Hmd = ovrHmd_Create(0);
	if (Hmd)
	{
		Settings.SupportedDistortionCaps = Hmd->DistortionCaps;
		Settings.SupportedHmdCaps = Hmd->HmdCaps;
		Settings.SupportedTrackingCaps = Hmd->TrackingCaps;

#ifndef OVR_SDK_RENDERING
		Settings.SupportedDistortionCaps &= ~ovrDistortionCap_Overdrive;
#endif
#ifndef OVR_VISION_ENABLED
		Settings.SupportedTrackingCaps &= ~ovrTrackingCap_Position;
#endif

		Settings.DistortionCaps = Settings.SupportedDistortionCaps & (ovrDistortionCap_Chromatic | ovrDistortionCap_TimeWarp | ovrDistortionCap_Vignette | ovrDistortionCap_Overdrive);
		Settings.TrackingCaps = Settings.SupportedTrackingCaps & (ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection | ovrTrackingCap_Position);
		Settings.HmdCaps = Settings.SupportedHmdCaps & (ovrHmdCap_DynamicPrediction | ovrHmdCap_LowPersistence);
		Settings.HmdCaps |= (Settings.Flags.bVSync ? 0 : ovrHmdCap_NoVSync);

		if (!(Settings.SupportedDistortionCaps & ovrDistortionCap_TimeWarp))
		{
			Settings.Flags.bTimeWarp = false;
		}

		Settings.Flags.bHmdPosTracking = (Settings.SupportedTrackingCaps & ovrTrackingCap_Position) != 0;

		LoadFromIni();

		UpdateDistortionCaps();
		UpdateHmdRenderInfo();
		UpdateStereoRenderingParams();
		UpdateHmdCaps();
	}

	return Hmd != nullptr;
}

void FOculusRiftHMD::ReleaseDevice()
{
	if (Hmd)
	{
		SaveToIni();

		ovrHmd_AttachToWindow(Hmd, NULL, NULL, NULL);

		// Wait for all resources to be released
#ifdef OVR_SDK_RENDERING
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(ResetRen,
		FOculusRiftHMD*, Plugin, this,
		{
			if (Plugin->GetActiveRHIBridgeImpl())
			{
				Plugin->GetActiveRHIBridgeImpl()->Reset();
			}
		});
		// Wait for all resources to be released
		FlushRenderingCommands();
#endif 

		ovrHmd_Destroy(Hmd);
		Hmd = nullptr;
	}
}

void FOculusRiftHMD::UpdateDistortionCaps()
{
	if (IsOpenGLPlatform(GMaxRHIShaderPlatform))
	{
		Settings.DistortionCaps &= ~ovrDistortionCap_SRGB;
		Settings.DistortionCaps |= ovrDistortionCap_FlipInput;
	}
	(Settings.Flags.bTimeWarp) ? Settings.DistortionCaps |= ovrDistortionCap_TimeWarp : Settings.DistortionCaps &= ~ovrDistortionCap_TimeWarp;
	(Settings.Flags.bOverdrive) ? Settings.DistortionCaps |= ovrDistortionCap_Overdrive : Settings.DistortionCaps &= ~ovrDistortionCap_Overdrive;
	(Settings.Flags.bHQDistortion) ? Settings.DistortionCaps |= ovrDistortionCap_HqDistortion : Settings.DistortionCaps &= ~ovrDistortionCap_HqDistortion;
	(Settings.Flags.bChromaAbCorrectionEnabled) ? Settings.DistortionCaps |= ovrDistortionCap_Chromatic : Settings.DistortionCaps &= ~ovrDistortionCap_Chromatic;
#if !UE_BUILD_SHIPPING
	(Settings.Flags.bProfiling) ? Settings.DistortionCaps |= ovrDistortionCap_ProfileNoTimewarpSpinWaits : Settings.DistortionCaps &= ~ovrDistortionCap_ProfileNoTimewarpSpinWaits;
#endif // #if !UE_BUILD_SHIPPING

#ifdef OVR_SDK_RENDERING 
	if (GetActiveRHIBridgeImpl())
	{
		GetActiveRHIBridgeImpl()->SetNeedReinitRendererAPI();
	}
#endif // OVR_SDK_RENDERING
	Flags.bNeedUpdateDistortionCaps = false;
}

void FOculusRiftHMD::UpdateHmdCaps()
{
	if (Hmd)
	{
		Settings.TrackingCaps = ovrTrackingCap_Orientation;
		if (Settings.Flags.bYawDriftCorrectionEnabled)
		{
			Settings.TrackingCaps |= ovrTrackingCap_MagYawCorrection;
		}
		else
		{
			Settings.TrackingCaps &= ~ovrTrackingCap_MagYawCorrection;
		}
		if (Settings.Flags.bHmdPosTracking)
		{
			Settings.TrackingCaps |= ovrTrackingCap_Position;
		}
		else
		{
			Settings.TrackingCaps &= ~ovrTrackingCap_Position;
		}

		if (Settings.Flags.bLowPersistenceMode)
		{
			Settings.HmdCaps |= ovrHmdCap_LowPersistence;
		}
		else
		{
			Settings.HmdCaps &= ~ovrHmdCap_LowPersistence;
		}

		if (Settings.Flags.bVSync)
		{
			Settings.HmdCaps &= ~ovrHmdCap_NoVSync;
		}
		else
		{
			Settings.HmdCaps |= ovrHmdCap_NoVSync;
		}

		if (Settings.Flags.bMirrorToWindow)
		{
			Settings.HmdCaps &= ~ovrHmdCap_NoMirrorToWindow;
		}
		else
		{
			Settings.HmdCaps |= ovrHmdCap_NoMirrorToWindow;
		}
		ovrHmd_SetEnabledCaps(Hmd, Settings.HmdCaps);

		ovrHmd_ConfigureTracking(Hmd, Settings.TrackingCaps, 0);
		Flags.bNeedUpdateHmdCaps = false;
	}
}

FORCEINLINE static float GetVerticalFovRadians(const ovrFovPort& fov)
{
	return FMath::Atan(fov.UpTan) + FMath::Atan(fov.DownTan);
}

FORCEINLINE static float GetHorizontalFovRadians(const ovrFovPort& fov)
{
	return FMath::Atan(fov.LeftTan) + FMath::Atan(fov.RightTan);
}

void FOculusRiftHMD::UpdateHmdRenderInfo()
{
	check(Hmd);

	UE_LOG(LogHMD, Warning, TEXT("HMD %s, Monitor %s, res = %d x %d, windowPos = {%d, %d}"), ANSI_TO_TCHAR(Hmd->ProductName), 
		ANSI_TO_TCHAR(Hmd->DisplayDeviceName), Hmd->Resolution.w, Hmd->Resolution.h, Hmd->WindowsPos.x, Hmd->WindowsPos.y); 

	// Calc FOV
	if (!Settings.Flags.bOverrideFOV)
	{
		// Calc FOV, symmetrical, for each eye. 
		Settings.EyeFov[0] = Hmd->DefaultEyeFov[0];
		Settings.EyeFov[1] = Hmd->DefaultEyeFov[1];

		// Calc FOV in radians
		Settings.VFOVInRadians = FMath::Max(GetVerticalFovRadians(Settings.EyeFov[0]), GetVerticalFovRadians(Settings.EyeFov[1]));
		Settings.HFOVInRadians = FMath::Max(GetHorizontalFovRadians(Settings.EyeFov[0]), GetHorizontalFovRadians(Settings.EyeFov[1]));
	}

	const Sizei recommenedTex0Size = ovrHmd_GetFovTextureSize(Hmd, ovrEye_Left, Settings.EyeFov[0], 1.0f);
	const Sizei recommenedTex1Size = ovrHmd_GetFovTextureSize(Hmd, ovrEye_Right, Settings.EyeFov[1], 1.0f);

	Sizei idealRenderTargetSize;
	idealRenderTargetSize.w = recommenedTex0Size.w + recommenedTex1Size.w;
	idealRenderTargetSize.h = FMath::Max(recommenedTex0Size.h, recommenedTex1Size.h);

	Settings.IdealScreenPercentage = FMath::Max(float(idealRenderTargetSize.w) / float(Hmd->Resolution.w) * 100.f,
												float(idealRenderTargetSize.h) / float(Hmd->Resolution.h) * 100.f);

	// Override eye distance by the value from HMDInfo (stored in Profile).
	if (!Settings.Flags.bOverrideIPD)
	{
		Settings.InterpupillaryDistance = ovrHmd_GetFloat(Hmd, OVR_KEY_IPD, OVR_DEFAULT_IPD);
	}

	// Default texture size (per eye) is equal to half of W x H resolution. Will be overridden in SetupView.
	Settings.SetViewportSize(Hmd->Resolution.w / 2, Hmd->Resolution.h);	

	Flags.bNeedUpdateStereoRenderingParams = true;
}

void FOculusRiftHMD::UpdateStereoRenderingParams()
{
	check(IsInGameThread());

	if ((!Settings.IsStereoEnabled() && !Settings.Flags.bHeadTrackingEnforced))
	{
		return;
	}
	if (IsInitialized() && Hmd)
	{
		//!AB: note, for Direct Rendering EyeRenderDesc is calculated twice, once
		// here and another time in BeginRendering_RenderThread. I need to have EyeRenderDesc
		// on a game thread for ViewAdjust (for StereoViewOffset calculation).
		Settings.EyeRenderDesc[0] = ovrHmd_GetRenderDesc(Hmd, ovrEye_Left, Settings.EyeFov[0]);
		Settings.EyeRenderDesc[1] = ovrHmd_GetRenderDesc(Hmd, ovrEye_Right, Settings.EyeFov[1]);
		if (Settings.Flags.bOverrideIPD)
		{
			Settings.EyeRenderDesc[0].HmdToEyeViewOffset.x = Settings.InterpupillaryDistance * 0.5f;
			Settings.EyeRenderDesc[1].HmdToEyeViewOffset.x = -Settings.InterpupillaryDistance * 0.5f;
		}

		const bool bRightHanded = false;
		// Far and Near clipping planes will be modified in GetStereoProjectionMatrix()
		Settings.EyeProjectionMatrices[0] = ovrMatrix4f_Projection(Settings.EyeFov[0], 0.01f, 10000.0f, bRightHanded);
		Settings.EyeProjectionMatrices[1] = ovrMatrix4f_Projection(Settings.EyeFov[1], 0.01f, 10000.0f, bRightHanded);

		// 2D elements offset
		if (!Settings.Flags.bOverride2D)
		{
			float ScreenSizeInMeters[2]; // 0 - width, 1 - height
			float LensSeparationInMeters;
			LensSeparationInMeters = ovrHmd_GetFloat(Hmd, "LensSeparation", 0);
			ovrHmd_GetFloatArray(Hmd, "ScreenSize", ScreenSizeInMeters, 2);

			// Recenter projection (meters)
			const float LeftProjCenterM = ScreenSizeInMeters[0] * 0.25f;
			const float LensRecenterM = LeftProjCenterM - LensSeparationInMeters * 0.5f;

			// Recenter projection (normalized)
			const float LensRecenter = 4.0f * LensRecenterM / ScreenSizeInMeters[0];

			Settings.HudOffset = 0.25f * Settings.InterpupillaryDistance * (Hmd->Resolution.w / ScreenSizeInMeters[0]) / 15.0f;
			Settings.CanvasCenterOffset = (0.25f * LensRecenter) * Hmd->Resolution.w;
		}

		PrecalculatePostProcess_NoLock();
#ifdef OVR_SDK_RENDERING 
		GetActiveRHIBridgeImpl()->SetNeedReinitRendererAPI();
#endif // OVR_SDK_RENDERING
		Flags.bNeedUpdateStereoRenderingParams = false;
	}
	else
	{
		Settings.CanvasCenterOffset = 0.f;
	}
}

void FOculusRiftHMD::LoadFromIni()
{
	const TCHAR* OculusSettings = TEXT("Oculus.Settings");
	bool v;
	float f;
	if (GConfig->GetBool(OculusSettings, TEXT("bChromaAbCorrectionEnabled"), v, GEngineIni))
	{
		Settings.Flags.bChromaAbCorrectionEnabled = v;
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bYawDriftCorrectionEnabled"), v, GEngineIni))
	{
		Settings.Flags.bYawDriftCorrectionEnabled = v;
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bDevSettingsEnabled"), v, GEngineIni))
	{
		Settings.Flags.bDevSettingsEnabled = v;
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bOverrideIPD"), v, GEngineIni))
	{
		Settings.Flags.bOverrideIPD = v;
		if (Settings.Flags.bOverrideIPD)
		{
			if (GConfig->GetFloat(OculusSettings, TEXT("IPD"), f, GEngineIni))
			{
				SetInterpupillaryDistance(f);
			}
		}
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bOverrideStereo"), v, GEngineIni))
	{
		Settings.Flags.bOverrideStereo = v;
		if (Settings.Flags.bOverrideStereo)
		{
			if (GConfig->GetFloat(OculusSettings, TEXT("HFOV"), f, GEngineIni))
			{
				Settings.HFOVInRadians = f;
			}
			if (GConfig->GetFloat(OculusSettings, TEXT("VFOV"), f, GEngineIni))
			{
				Settings.VFOVInRadians = f;
			}
		}
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bOverrideVSync"), v, GEngineIni))
	{
		Settings.Flags.bOverrideVSync = v;
		if (GConfig->GetBool(OculusSettings, TEXT("bVSync"), v, GEngineIni))
		{
			Settings.Flags.bVSync = v;
		}
	}
	if (!GIsEditor)
	{
		if (GConfig->GetBool(OculusSettings, TEXT("bOverrideScreenPercentage"), v, GEngineIni))
		{
			Settings.Flags.bOverrideScreenPercentage = v;
			if (GConfig->GetFloat(OculusSettings, TEXT("ScreenPercentage"), f, GEngineIni))
			{
				Settings.ScreenPercentage = f;
			}
		}
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bAllowFinishCurrentFrame"), v, GEngineIni))
	{
		Settings.Flags.bAllowFinishCurrentFrame = v;
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bLowPersistenceMode"), v, GEngineIni))
	{
		Settings.Flags.bLowPersistenceMode = v;
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bUpdateOnRT"), v, GEngineIni))
	{
		Settings.Flags.bUpdateOnRT = v;
	}
	if (GConfig->GetFloat(OculusSettings, TEXT("FarClippingPlane"), f, GEngineIni))
	{
		Settings.FarClippingPlane = f;
	}
	if (GConfig->GetFloat(OculusSettings, TEXT("NearClippingPlane"), f, GEngineIni))
	{
		Settings.NearClippingPlane = f;
	}
}

void FOculusRiftHMD::SaveToIni()
{
	const TCHAR* OculusSettings = TEXT("Oculus.Settings");
	GConfig->SetBool(OculusSettings, TEXT("bChromaAbCorrectionEnabled"), Settings.Flags.bChromaAbCorrectionEnabled, GEngineIni);
	GConfig->SetBool(OculusSettings, TEXT("bYawDriftCorrectionEnabled"), Settings.Flags.bYawDriftCorrectionEnabled, GEngineIni);
	GConfig->SetBool(OculusSettings, TEXT("bDevSettingsEnabled"), Settings.Flags.bDevSettingsEnabled, GEngineIni);

	GConfig->SetBool(OculusSettings, TEXT("bOverrideIPD"), Settings.Flags.bOverrideIPD, GEngineIni);
	if (Settings.Flags.bOverrideIPD)
	{
		GConfig->SetFloat(OculusSettings, TEXT("IPD"), GetInterpupillaryDistance(), GEngineIni);
	}
	GConfig->SetBool(OculusSettings, TEXT("bOverrideStereo"), Settings.Flags.bOverrideStereo, GEngineIni);
	if (Settings.Flags.bOverrideStereo)
	{
		GConfig->SetFloat(OculusSettings, TEXT("HFOV"), Settings.HFOVInRadians, GEngineIni);
		GConfig->SetFloat(OculusSettings, TEXT("VFOV"), Settings.VFOVInRadians, GEngineIni);
	}

	GConfig->SetBool(OculusSettings, TEXT("bOverrideVSync"), Settings.Flags.bOverrideVSync, GEngineIni);
	if (Settings.Flags.bOverrideVSync)
	{
		GConfig->SetBool(OculusSettings, TEXT("VSync"), Settings.Flags.bVSync, GEngineIni);
	}

	if (!GIsEditor)
	{
		GConfig->SetBool(OculusSettings, TEXT("bOverrideScreenPercentage"), Settings.Flags.bOverrideScreenPercentage, GEngineIni);
		if (Settings.Flags.bOverrideScreenPercentage)
		{
			// Save the current ScreenPercentage state
			GConfig->SetFloat(OculusSettings, TEXT("ScreenPercentage"), Settings.ScreenPercentage, GEngineIni);
		}
	}
	GConfig->SetBool(OculusSettings, TEXT("bAllowFinishCurrentFrame"), Settings.Flags.bAllowFinishCurrentFrame, GEngineIni);

	GConfig->SetBool(OculusSettings, TEXT("bLowPersistenceMode"), Settings.Flags.bLowPersistenceMode, GEngineIni);

	GConfig->SetBool(OculusSettings, TEXT("bUpdateOnRT"), Settings.Flags.bUpdateOnRT, GEngineIni);

	if (Settings.Flags.bClippingPlanesOverride)
	{
		GConfig->SetFloat(OculusSettings, TEXT("FarClippingPlane"), Settings.FarClippingPlane, GEngineIni);
		GConfig->SetFloat(OculusSettings, TEXT("NearClippingPlane"), Settings.NearClippingPlane, GEngineIni);
	}
}

bool FOculusRiftHMD::HandleInputKey(UPlayerInput* pPlayerInput,
	const FKey& Key, EInputEvent EventType, float AmountDepressed, bool bGamepad)
{
	if (Hmd && EventType == IE_Pressed && Settings.IsStereoEnabled())
	{
		if (!Key.IsMouseButton())
		{
			ovrHmd_DismissHSWDisplay(Hmd);
		}
	}
	return false;
}

void FOculusRiftHMD::OnBeginPlay()
{
	// @TODO: add more values here.
	// This call make sense when 'Play' is used from the Editor;
	if (GIsEditor)
	{
		DeltaControlRotation = FRotator::ZeroRotator;
		Settings.PositionOffset = FVector::ZeroVector;
		Settings.BaseOrientation = FQuat::Identity;
		Settings.BaseOffset = OVR::Vector3f(0, 0, 0);
		Settings.WorldToMetersScale = 100.f;
		Settings.Flags.bWorldToMetersOverride = false;
		InitDevice();
	}
}

void FOculusRiftHMD::OnEndPlay()
{
	if (GIsEditor)
	{
		EnableStereo(false);
		ReleaseDevice();
	}
}

#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS

