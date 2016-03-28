// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "HMDPrivatePCH.h"
#include "HeadMountedDisplayCommon.h"

FHMDSettings::FHMDSettings() :
	SavedScrPerc(100.f)
	, ScreenPercentage(100.f)
	, IdealScreenPercentage(100.f)
	, InterpupillaryDistance(0.064f)
	, WorldToMetersScale(100.f)
	, UserDistanceToScreenModifier(0.f)
	, HFOVInRadians(FMath::DegreesToRadians(90.f))
	, VFOVInRadians(FMath::DegreesToRadians(90.f))
	, NearClippingPlane(0)
	, FarClippingPlane(0)
	, MirrorWindowSize(0, 0)
	, BaseOffset(0, 0, 0)
	, BaseOrientation(FQuat::Identity)
	, PositionOffset(FVector::ZeroVector)
{
	Flags.Raw = 0;
	Flags.bHMDEnabled = true;
	Flags.bOverrideVSync = true;
	Flags.bVSync = true;
	Flags.bAllowFinishCurrentFrame = true;
	Flags.bHmdDistortion = true;
	Flags.bChromaAbCorrectionEnabled = true;
	Flags.bYawDriftCorrectionEnabled = true;
	Flags.bLowPersistenceMode = true;
	Flags.bUpdateOnRT = true;
	Flags.bMirrorToWindow = true;
	Flags.bFullscreenAllowed = false;
	Flags.bTimeWarp = true;
	Flags.bHmdPosTracking = true;
	Flags.bPlayerControllerFollowsHmd = true;
	Flags.bPlayerCameraManagerFollowsHmdOrientation = true;
	Flags.bPlayerCameraManagerFollowsHmdPosition = true;
	EyeRenderViewport[0] = EyeRenderViewport[1] = FIntRect(0, 0, 0, 0);

	CameraScale3D = FVector(1.0f, 1.0f, 1.0f);
	PositionScale3D = FVector(1.0f, 1.0f, 1.0f);
}

TSharedPtr<FHMDSettings, ESPMode::ThreadSafe> FHMDSettings::Clone() const
{
	TSharedPtr<FHMDSettings, ESPMode::ThreadSafe> NewSettings = MakeShareable(new FHMDSettings(*this));
	return NewSettings;
}

void FHMDSettings::SetEyeRenderViewport(int OneEyeVPw, int OneEyeVPh)
{
	EyeRenderViewport[0].Min = FIntPoint(0, 0);
	EyeRenderViewport[0].Max = FIntPoint(OneEyeVPw, OneEyeVPh);
	EyeRenderViewport[1].Min = FIntPoint(OneEyeVPw, 0);
	EyeRenderViewport[1].Max = FIntPoint(OneEyeVPw*2, OneEyeVPh);
}

FIntPoint FHMDSettings::GetTextureSize() const 
{ 
	return FIntPoint(EyeRenderViewport[1].Min.X + EyeRenderViewport[1].Size().X, 
					 EyeRenderViewport[1].Min.Y + EyeRenderViewport[1].Size().Y); 
}

float FHMDSettings::GetActualScreenPercentage() const
{
	return Flags.bOverrideScreenPercentage ? ScreenPercentage : IdealScreenPercentage;
}

//////////////////////////////////////////////////////////////////////////

FHMDGameFrame::FHMDGameFrame() :
	FrameNumber(0),
	WorldToMetersScale(100.f)
{
	LastHmdOrientation = FQuat::Identity;
	LastHmdPosition = FVector::ZeroVector;
	CameraScale3D = FVector(1.0f, 1.0f, 1.0f);
	ViewportSize = FIntPoint(0,0);
	PlayerLocation = FVector::ZeroVector;
	PlayerOrientation = FQuat::Identity;
	Flags.Raw = 0;
}

TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> FHMDGameFrame::Clone() const
{
	TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> NewFrame = MakeShareable(new FHMDGameFrame(*this));
	return NewFrame;
}

//////////////////////////////////////////////////////////////////////////
FHMDViewExtension::FHMDViewExtension(FHeadMountedDisplay* InDelegate)
	: Delegate(InDelegate)
{
}

FHMDViewExtension::~FHMDViewExtension()
{
}

void FHMDViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	check(IsInGameThread());

	Delegate->SetupViewFamily(InViewFamily);
}
void FHMDViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	check(IsInGameThread());

	Delegate->SetupView(InViewFamily, InView);
}

void FHMDViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	check(IsInGameThread());

	auto InRenderFrame = Delegate->PassRenderFrameOwnership();

	// saving only a pointer to the frame should be fine, since the game thread doesn't suppose to modify it anymore.
	RenderFrame = InRenderFrame;
}

void FHMDViewExtension::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	check(IsInRenderingThread());
}

void FHMDViewExtension::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
	check(IsInRenderingThread());
}

////////////////////////////////////////////////////////////////////////////
FHeadMountedDisplay::FHeadMountedDisplay()
{
	Flags.Raw = 0;
	Flags.bNeedUpdateStereoRenderingParams = true;
	DeltaControlRotation = FRotator::ZeroRotator;  // used from ApplyHmdRotation

#if !UE_BUILD_SHIPPING
	SideOfSingleCubeInMeters = 0;
	SeaOfCubesVolumeSizeInMeters = 0;
	NumberOfCubesInOneSide = 0;
	CenterOffsetInMeters = FVector::ZeroVector; // offset from the center of 'sea of cubes'
#endif

	CurrentFrameNumber.Set(1);
}

bool FHeadMountedDisplay::IsInitialized() const
{
	return (Settings->Flags.InitStatus & FHMDSettings::eInitialized) != 0;
}

TSharedPtr<ISceneViewExtension, ESPMode::ThreadSafe> FHeadMountedDisplay::GetViewExtension()
{
	TSharedPtr<FHMDViewExtension, ESPMode::ThreadSafe> Result(MakeShareable(new FHMDViewExtension(this)));
	return Result;
}

TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> FHeadMountedDisplay::PassRenderFrameOwnership()
{
	TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> RenFr = RenderFrame;
	RenderFrame = nullptr;
	return RenFr;
}

TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> FHeadMountedDisplay::CreateNewGameFrame() const
{
	TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> Result(MakeShareable(new FHMDGameFrame()));
	return Result;
}

TSharedPtr<FHMDSettings, ESPMode::ThreadSafe> FHeadMountedDisplay::CreateNewSettings() const
{
	TSharedPtr<FHMDSettings, ESPMode::ThreadSafe> Result(MakeShareable(new FHMDSettings()));
	return Result;
}

FHMDGameFrame* FHeadMountedDisplay::GetCurrentFrame() const
{
	//UE_LOG(LogHMD, Log, TEXT("Getting current frame, frame number %d"), int(CurrentFrameNumber.GetValue()));
	if (!Frame.IsValid())
	{
		//UE_LOG(LogHMD, Log, TEXT("		----- frame is null!!!"));
		return nullptr;
	}

	check(IsInGameThread());
	// A special case, when the game frame is over but rendering hasn't started yet
	if (Frame->Flags.bOutOfFrame && RenderFrame.IsValid() && RenderFrame->Flags.bOutOfFrame)
	{
		//UE_LOG(LogHMD, Log, TEXT("		+++++ render frame, %d"), RenderFrame->FrameNumber);
		return RenderFrame.Get();
	}
	// Technically speaking, we should return the frame only if frame counters are equal.
	// However, there are some calls UE makes from outside of the frame (for example, when 
	// switching to/from fullscreen), thus, returning the previous frame in this case.
	if (Frame->FrameNumber == CurrentFrameNumber.GetValue() || Frame->Flags.bOutOfFrame)
	{
		//UE_LOG(LogHMD, Log, TEXT("		+++++ game frame, %d"), Frame->FrameNumber);
		return Frame.Get();
	}
	//UE_LOG(LogHMD, Log, TEXT("		----- no frame found!"));
	return nullptr;
}

bool FHeadMountedDisplay::OnStartGameFrame(FWorldContext& WorldContext)
{
	check(IsInGameThread());

	if (!WorldContext.World() || !WorldContext.World()->IsGameWorld())
	{
		// ignore all non-game worlds
		return false;
	}

	Frame.Reset();
	Flags.bFrameStarted = true;

	bool bStereoEnabled = Settings->Flags.bStereoEnabled;
	bool bStereoDesired = Settings->Flags.bStereoEnforced ? bStereoEnabled : Settings->Flags.bStereoDesired;

	if(Flags.bNeedEnableStereo)
	{
		Settings->Flags.bStereoDesired = bStereoDesired = true;
	}

	if(bStereoDesired && (Flags.bNeedDisableStereo || !Settings->Flags.bHMDEnabled || !(bStereoEnabled ? IsHMDActive() : IsHMDConnected())))
	{
		bStereoDesired = false;
	}

	Flags.bNeedEnableStereo = false;
	Flags.bNeedDisableStereo = false;

	if(bStereoEnabled != bStereoDesired)
	{
		bStereoEnabled = DoEnableStereo(bStereoDesired, Flags.bEnableStereoToHmd);
	}

	// Keep trying to enable stereo until we succeed
	Flags.bNeedEnableStereo = bStereoDesired && !bStereoEnabled;

	if (!Settings->IsStereoEnabled() && !Settings->Flags.bHeadTrackingEnforced)
	{
		return false;
	}

	if (Flags.bNeedUpdateDistortionCaps)
	{
		UpdateDistortionCaps();
	}
	if (Flags.bNeedUpdateHmdCaps)
	{
		UpdateHmdCaps();
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

	auto CurrentFrame = CreateNewGameFrame();
	Frame = CurrentFrame;

	CurrentFrame->FrameNumber = (uint32)CurrentFrameNumber.Increment();
	CurrentFrame->Flags.bOutOfFrame = false;

	if (Settings->Flags.bWorldToMetersOverride)
	{
		CurrentFrame->WorldToMetersScale = Settings->WorldToMetersScale;
	}
	else
	{
		CurrentFrame->WorldToMetersScale = WorldContext.World()->GetWorldSettings()->WorldToMeters;
	}

	if (Settings->Flags.bCameraScale3DOverride)
	{
		CurrentFrame->CameraScale3D = Settings->CameraScale3D;
	}
	else
	{
		CurrentFrame->CameraScale3D = FVector(1.0f, 1.0f, 1.0f);
	}

	return true;
}

bool FHeadMountedDisplay::OnEndGameFrame(FWorldContext& WorldContext)
{
	check(IsInGameThread());

	Flags.bFrameStarted = false;

	FHMDGameFrame* const CurrentGameFrame = Frame.Get();

	if (!WorldContext.World() || !WorldContext.World()->IsGameWorld() || !CurrentGameFrame)
	{
		// ignore all non-game worlds
		return false;
	}

	CurrentGameFrame->Flags.bOutOfFrame = true;

	if (!CurrentGameFrame->Settings->IsStereoEnabled() && !CurrentGameFrame->Settings->Flags.bHeadTrackingEnforced)
	{
		return false;
	}
	// make sure there was only one game worlds.
	//	FGameFrame* CurrentRenderFrame = GetRenderFrame();
	//@todo hmd	check(CurrentRenderFrame->FrameNumber != CurrentGameFrame->FrameNumber);

	// make sure end frame matches the start
	check(CurrentFrameNumber.GetValue() == CurrentGameFrame->FrameNumber);

	if (CurrentFrameNumber.GetValue() == CurrentGameFrame->FrameNumber)
	{
		RenderFrame = Frame;
	}
	else
	{
		RenderFrame.Reset();
	}
	return true;
}

bool FHeadMountedDisplay::IsHMDEnabled() const
{
	return (Settings->Flags.bHMDEnabled);
}

void FHeadMountedDisplay::EnableHMD(bool enable)
{
	Settings->Flags.bHMDEnabled = enable;
	if (!Settings->Flags.bHMDEnabled)
	{
		EnableStereo(false);
	}
}

bool FHeadMountedDisplay::DoesSupportPositionalTracking() const
{
	return false;
}

bool FHeadMountedDisplay::HasValidTrackingPosition()
{
	return false;
}

void FHeadMountedDisplay::GetPositionalTrackingCameraProperties(FVector& OutOrigin, FQuat& OutOrientation, float& OutHFOV, float& OutVFOV, float& OutCameraDistance, float& OutNearPlane, float& OutFarPlane) const
{
}

void FHeadMountedDisplay::RebaseObjectOrientationAndPosition(FVector& OutPosition, FQuat& OutOrientation) const
{
}

bool FHeadMountedDisplay::IsInLowPersistenceMode() const
{
	const auto frame = GetCurrentFrame();
	if (frame)
	{
		const auto FrameSettings = frame->Settings;
		return FrameSettings->Flags.bLowPersistenceMode;
	}
	return false;
}

void FHeadMountedDisplay::EnableLowPersistenceMode(bool Enable)
{
	Settings->Flags.bLowPersistenceMode = Enable;
}

float FHeadMountedDisplay::GetInterpupillaryDistance() const
{
	return Settings->InterpupillaryDistance;
}

void FHeadMountedDisplay::SetInterpupillaryDistance(float NewInterpupillaryDistance)
{
	Settings->InterpupillaryDistance = NewInterpupillaryDistance;
	UpdateStereoRenderingParams();
}

void FHeadMountedDisplay::GetFieldOfView(float& InOutHFOVInDegrees, float& InOutVFOVInDegrees) const
{
	const auto frame = GetCurrentFrame();
	if (frame)
	{
		InOutHFOVInDegrees = FMath::RadiansToDegrees(frame->Settings->HFOVInRadians);
		InOutVFOVInDegrees = FMath::RadiansToDegrees(frame->Settings->VFOVInRadians);
	}
}

bool FHeadMountedDisplay::IsChromaAbCorrectionEnabled() const
{
	const auto frame = GetCurrentFrame();
	return (frame && frame->Settings->Flags.bChromaAbCorrectionEnabled);
}

void FHeadMountedDisplay::OnScreenModeChange(EWindowMode::Type WindowMode)
{
	EnableStereo(WindowMode != EWindowMode::Windowed);
	UpdateStereoRenderingParams();
}

bool FHeadMountedDisplay::IsPositionalTrackingEnabled() const
{
	const auto frame = GetCurrentFrame();
	return (frame && frame->Settings->Flags.bHmdPosTracking);
}

bool FHeadMountedDisplay::EnablePositionalTracking(bool enable)
{
	Settings->Flags.bHmdPosTracking = enable;
	return enable;
}

bool FHeadMountedDisplay::EnableStereo(bool bStereo)
{
	Settings->Flags.bStereoDesired = bStereo;
	Settings->Flags.bStereoEnforced = false;
	return DoEnableStereo(bStereo, true);
}

bool FHeadMountedDisplay::IsStereoEnabled() const
{
	if (IsInGameThread())
	{
		const auto frame = GetCurrentFrame();
		if (frame && frame->Settings.IsValid())
		{
			return (frame->Settings->IsStereoEnabled());
		}
		else
		{
			// If IsStereoEnabled is called when a game frame hasn't started, then always return false.
			// In the case when you need to check if stereo is GOING TO be enabled in next frame,
			// use explicit call to Settings->IsStereoEnabled()
			return false;
		}
	}
	else
	{
		check(0);
	}
	return false;
}

bool FHeadMountedDisplay::IsStereoEnabledOnNextFrame() const
{
	return Settings->IsStereoEnabled();
}

void FHeadMountedDisplay::AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	SizeX = SizeX / 2;
	if (StereoPass == eSSP_RIGHT_EYE)
	{
		X += SizeX;
	}
}

void FHeadMountedDisplay::SetClippingPlanes(float NCP, float FCP)
{
	Settings->NearClippingPlane = NCP;
	Settings->FarClippingPlane = FCP;
	Settings->Flags.bClippingPlanesOverride = false; // prevents from saving in .ini file
}

void FHeadMountedDisplay::SetBaseRotation(const FRotator& BaseRot)
{
	SetBaseOrientation(BaseRot.Quaternion());
}

FRotator FHeadMountedDisplay::GetBaseRotation() const
{
	return GetBaseOrientation().Rotator();
}

void FHeadMountedDisplay::SetBaseOrientation(const FQuat& BaseOrient)
{
	Settings->BaseOrientation = BaseOrient;
}

FQuat FHeadMountedDisplay::GetBaseOrientation() const
{
	return Settings->BaseOrientation;
}

void FHeadMountedDisplay::SetPositionScale3D(FVector PosScale3D)
{
	Settings->PositionScale3D = PosScale3D;
}

FVector FHeadMountedDisplay::GetPositionScale3D() const
{
	return Settings->PositionScale3D;
}

void FHeadMountedDisplay::SetBaseOffsetInMeters(const FVector& BaseOffset)
{
	Settings->BaseOffset = BaseOffset;
}

FVector FHeadMountedDisplay::GetBaseOffsetInMeters() const
{
	return Settings->BaseOffset;
}

bool FHeadMountedDisplay::IsHeadTrackingAllowed() const
{
	const auto frame = GetCurrentFrame();
	return (frame && (frame->Settings->Flags.bHeadTrackingEnforced || GEngine->IsStereoscopic3D()));
}

void FHeadMountedDisplay::ResetStereoRenderingParams()
{
	Flags.bNeedUpdateStereoRenderingParams = true;
	Settings->Flags.bOverrideStereo = false;
	Settings->Flags.bOverrideIPD = false;
	Settings->Flags.bWorldToMetersOverride = false;
	Settings->NearClippingPlane = Settings->FarClippingPlane = 0.f;
	Settings->Flags.bClippingPlanesOverride = true; // forces zeros to be written to ini file to use default values next run
}

bool FHeadMountedDisplay::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	auto frame = GetCurrentFrame();

	if (FParse::Command(&Cmd, TEXT("STEREO")))
	{
		if (FParse::Command(&Cmd, TEXT("OFF")))
		{
			Flags.bNeedDisableStereo = true;
			Settings->Flags.bStereoEnforced = true;
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("RESET")))
		{
			Settings->Flags.bStereoEnforced = false;
			ResetStereoRenderingParams();
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("SHOW")))
		{
			Ar.Logf(TEXT("stereo ipd=%.4f hfov=%.3f vfov=%.3f\n nearPlane=%.4f farPlane=%.4f"), GetInterpupillaryDistance(),
				FMath::RadiansToDegrees(Settings->HFOVInRadians), FMath::RadiansToDegrees(Settings->VFOVInRadians),
				(Settings->NearClippingPlane) ? Settings->NearClippingPlane : GNearClippingPlane, Settings->FarClippingPlane);
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
				if (!Settings->Flags.bHMDEnabled)
				{
					Ar.Logf(TEXT("HMD is disabled. Use 'hmd enable' to re-enable it."));
				}
				Flags.bEnableStereoToHmd = hmd;
				EnableStereo(true);
				Settings->Flags.bStereoEnforced = true;
				return true;
			}
		}

		// normal configuration
		float val;
		if (FParse::Value(Cmd, TEXT("E="), val))
		{
			SetInterpupillaryDistance(val);
			Settings->Flags.bOverrideIPD = true;
			Flags.bNeedUpdateStereoRenderingParams = true;
		}
		if (FParse::Value(Cmd, TEXT("FCP="), val)) // far clipping plane override
		{
			Settings->FarClippingPlane = val;
			Settings->Flags.bClippingPlanesOverride = true;
		}
		if (FParse::Value(Cmd, TEXT("NCP="), val)) // near clipping plane override
		{
			Settings->NearClippingPlane = val;
			Settings->Flags.bClippingPlanesOverride = true;
		}
		if (FParse::Value(Cmd, TEXT("W2M="), val))
		{
			Settings->WorldToMetersScale = val;
			Settings->Flags.bWorldToMetersOverride = true;
		}
		if (FParse::Value(Cmd, TEXT("CS="), val))
		{
			Settings->CameraScale3D = FVector(val, val, val);
			Settings->Flags.bCameraScale3DOverride = true;
		}
		if (FParse::Value(Cmd, TEXT("PS="), val))
		{
			Settings->PositionScale3D = FVector(val, val, val);
		}

		// debug configuration
		if (Settings->Flags.bDevSettingsEnabled)
		{
			float fov;
			if (FParse::Value(Cmd, TEXT("HFOV="), fov))
			{
				Settings->HFOVInRadians = FMath::DegreesToRadians(fov);
				Settings->Flags.bOverrideStereo = true;
			}
			else if (FParse::Value(Cmd, TEXT("VFOV="), fov))
			{
				Settings->VFOVInRadians = FMath::DegreesToRadians(fov);
				Settings->Flags.bOverrideStereo = true;
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
			if (FParse::Command(&Cmd, TEXT("RESET")))
			{
				if (Settings->Flags.bStereoEnabled)
				{
					Settings->Flags.bVSync = Settings->Flags.bSavedVSync;
					Flags.bApplySystemOverridesOnStereo = true;
					Flags.bNeedUpdateHmdCaps = true;
				}
				Settings->Flags.bOverrideVSync = false;
				return true;
			}
			else
			{
				if (FParse::Command(&Cmd, TEXT("ON")) || FParse::Command(&Cmd, TEXT("1")))
				{
					Settings->Flags.bVSync = true;
					Settings->Flags.bOverrideVSync = true;
					Flags.bApplySystemOverridesOnStereo = true;
					Flags.bNeedUpdateHmdCaps = true;
					return true;
				}
				else if (FParse::Command(&Cmd, TEXT("OFF")) || FParse::Command(&Cmd, TEXT("0")))
				{
					Settings->Flags.bVSync = false;
					Settings->Flags.bOverrideVSync = true;
					Flags.bApplySystemOverridesOnStereo = true;
					Flags.bNeedUpdateHmdCaps = true;
					return true;
				}
				else if (FParse::Command(&Cmd, TEXT("TOGGLE")) || FParse::Command(&Cmd, TEXT("")))
				{
					Settings->Flags.bVSync = !Settings->Flags.bVSync;
					Settings->Flags.bOverrideVSync = true;
					Flags.bApplySystemOverridesOnStereo = true;
					Flags.bNeedUpdateHmdCaps = true;
					Ar.Logf(TEXT("VSync is currently %s"), (Settings->Flags.bVSync) ? TEXT("ON") : TEXT("OFF"));
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
				Settings->Flags.bOverrideScreenPercentage = false;
				Flags.bApplySystemOverridesOnStereo = true;
			}
			else
			{
				float sp = FCString::Atof(*CmdName);
				if (sp >= 30 && sp <= 300)
				{
					Settings->Flags.bOverrideScreenPercentage = true;
					Settings->ScreenPercentage = sp;
					Flags.bApplySystemOverridesOnStereo = true;
				}
				else
				{
					Ar.Logf(TEXT("Value is out of range [30..300]"));
				}
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
					Settings->Flags.bUpdateOnRT = true;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("OFF")))
				{
					Settings->Flags.bUpdateOnRT = false;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("TOGGLE")))
				{
					Settings->Flags.bUpdateOnRT = !Settings->Flags.bUpdateOnRT;
				}
				else
				{
					return false;
				}
			}
			else
			{
				Settings->Flags.bUpdateOnRT = !Settings->Flags.bUpdateOnRT;
			}
			Ar.Logf(TEXT("Update on render thread is currently %s"), (Settings->Flags.bUpdateOnRT) ? TEXT("ON") : TEXT("OFF"));
			return true;
		}
#if !UE_BUILD_SHIPPING
		else if (FParse::Command(&Cmd, TEXT("UPDATEONGT"))) // update on game thread
		{
			FString CmdName = FParse::Token(Cmd, 0);
			if (!CmdName.IsEmpty())
			{
				if (!FCString::Stricmp(*CmdName, TEXT("ON")))
				{
					Settings->Flags.bDoNotUpdateOnGT = false;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("OFF")))
				{
					Settings->Flags.bDoNotUpdateOnGT = true;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("TOGGLE")))
				{
					Settings->Flags.bDoNotUpdateOnGT = !Settings->Flags.bDoNotUpdateOnGT;
				}
				else
				{
					return false;
				}
			}
			else
			{
				Settings->Flags.bDoNotUpdateOnGT = !Settings->Flags.bDoNotUpdateOnGT;
			}
			Ar.Logf(TEXT("Update on game thread is currently %s"), (!Settings->Flags.bDoNotUpdateOnGT) ? TEXT("ON") : TEXT("OFF"));
			return true;
		}
#endif //UE_BUILD_SHIPPING
	}
#if !UE_BUILD_SHIPPING
	else if (FParse::Command(&Cmd, TEXT("HMDDBG")))
	{
		if (FParse::Command(&Cmd, TEXT("SHOWCAMERA")))
		{
			if (FParse::Command(&Cmd, TEXT("OFF")))
			{
				Settings->Flags.bDrawSensorFrustum = false;
				return true;
			}
			if (FParse::Command(&Cmd, TEXT("ON")))
			{
				Settings->Flags.bDrawSensorFrustum = true;
				return true;
			}
			else
			{
				Settings->Flags.bDrawSensorFrustum = !Settings->Flags.bDrawSensorFrustum;
				return true;
			}
		}
		else if (FParse::Command(&Cmd, TEXT("CUBES")))
		{
			if (FParse::Command(&Cmd, TEXT("OFF")))
			{
				Settings->Flags.bDrawCubes = false;
				return true;
			}
			if (FParse::Command(&Cmd, TEXT("ON")))
			{
				Settings->Flags.bDrawCubes = true;
				return true;
			}
			else
			{
				Settings->Flags.bDrawCubes = !Settings->Flags.bDrawCubes;
				return true;
			}
		}
		else if (FParse::Command(&Cmd, TEXT("POSOFF")))
		{
			FString StrX = FParse::Token(Cmd, 0);
			FString StrY = FParse::Token(Cmd, 0);
			FString StrZ = FParse::Token(Cmd, 0);
			Settings->PositionOffset.X = FCString::Atof(*StrX);
			Settings->PositionOffset.Y = FCString::Atof(*StrY);
			Settings->PositionOffset.Z = FCString::Atof(*StrZ);
		}
	}
#endif //!UE_BUILD_SHIPPING
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
			Settings->Flags.bHeadTrackingEnforced = false;
			ResetOrientationAndPosition(yaw);
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("RESETPOS")))
		{
			Settings->Flags.bHeadTrackingEnforced = false;
			ResetPosition();
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("RESETROT")))
		{
			FString YawStr = FParse::Token(Cmd, 0);
			float yaw = 0.f;
			if (!YawStr.IsEmpty())
			{
				yaw = FCString::Atof(*YawStr);
			}
			Settings->Flags.bHeadTrackingEnforced = false;
			ResetOrientation(yaw);
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("ON")) || FParse::Command(&Cmd, TEXT("ENABLE")))
		{
			Settings->Flags.bHmdPosTracking = true;
			Flags.bNeedUpdateHmdCaps = true;
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("OFF")) || FParse::Command(&Cmd, TEXT("DISABLE")))
		{
			Settings->Flags.bHmdPosTracking = false;
			Flags.bNeedUpdateHmdCaps = true;
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("TOGGLE")))
		{
			Settings->Flags.bHmdPosTracking = !Settings->Flags.bHmdPosTracking;
			Flags.bNeedUpdateHmdCaps = true;
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("ENFORCE")))
		{
			FString CmdName = FParse::Token(Cmd, 0);
			if (CmdName.IsEmpty())
				return false;

			if (!FCString::Stricmp(*CmdName, TEXT("OFF")))
			{
				Settings->Flags.bHeadTrackingEnforced = false;
				return true;
			}
			else if (!FCString::Stricmp(*CmdName, TEXT("ON")))
			{
				Settings->Flags.bHeadTrackingEnforced = !Settings->Flags.bHeadTrackingEnforced;
				if (!Settings->Flags.bHeadTrackingEnforced)
				{
					ResetControlRotation();
				}
				return true;
			}
			return false;
		}
		else if (FParse::Command(&Cmd, TEXT("SHOW")))
		{
			Ar.Logf(TEXT("hmdpos is %s, vision='%s'"),
				(Settings->Flags.bHmdPosTracking ? TEXT("enabled") : TEXT("disabled")),
				((frame && frame->Flags.bHaveVisionTracking) ? TEXT("active") : TEXT("lost")));
			return true;
		}
	}
	else if (FParse::Command(&Cmd, TEXT("SETFINISHFRAME")))
	{
		static IConsoleVariable* CFinishFrameVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FinishCurrentFrame"));

		if (FParse::Command(&Cmd, TEXT("ON")))
		{
			Settings->Flags.bAllowFinishCurrentFrame = true;
			if (Settings->Flags.bStereoEnabled)
			{
				CFinishFrameVar->Set(Settings->Flags.bAllowFinishCurrentFrame != 0);
			}
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("OFF")))
		{
			Settings->Flags.bAllowFinishCurrentFrame = false;
			if (Settings->Flags.bStereoEnabled)
			{
				CFinishFrameVar->Set(Settings->Flags.bAllowFinishCurrentFrame != 0);
			}
			return true;
		}
		return false;
	}
	else if (FParse::Command(&Cmd, TEXT("HMDDEV")))
	{
		if (FParse::Command(&Cmd, TEXT("ON")))
		{
			Settings->Flags.bDevSettingsEnabled = true;
		}
		else if (FParse::Command(&Cmd, TEXT("OFF")))
		{
			Settings->Flags.bDevSettingsEnabled = false;
		}
		UpdateStereoRenderingParams();
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("UNCAPFPS")))
	{
		GEngine->bSmoothFrameRate = false;
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("HMDVERSION")))
	{
		Ar.Logf(*GetVersionString());
		return true;
	}
	return false;
}

void FHeadMountedDisplay::SetScreenPercentage(float InScreenPercentage)
{
	Settings->ScreenPercentage = InScreenPercentage;
	if (InScreenPercentage == 0.0f)
	{
		Settings->Flags.bOverrideScreenPercentage = false;
		Flags.bApplySystemOverridesOnStereo = true;
	}
	else
	{
		Settings->Flags.bOverrideScreenPercentage = true;
		Settings->ScreenPercentage = FMath::Clamp(InScreenPercentage, 30.f, 300.f);
		Flags.bApplySystemOverridesOnStereo = true;
	}
}

float FHeadMountedDisplay::GetScreenPercentage() const
{
	return (Settings->Flags.bOverrideScreenPercentage) ? Settings->ScreenPercentage : 0.0f;
}

void FHeadMountedDisplay::ResetControlRotation() const
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

void FHeadMountedDisplay::GetCurrentHMDPose(FQuat& CurrentOrientation, FVector& CurrentPosition,
	bool bUseOrienationForPlayerCamera, bool bUsePositionForPlayerCamera, const FVector& PositionScale)
{
	// only supposed to be used from the game thread
	check(IsInGameThread());
	auto frame = GetCurrentFrame();
	if (!frame)
	{
		CurrentOrientation = FQuat::Identity;
		CurrentPosition = FVector::ZeroVector;
		return;
	}
	if (PositionScale != FVector::ZeroVector)
	{
		frame->CameraScale3D = PositionScale;
		frame->Flags.bCameraScale3DAlreadySet = true;
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
	}
}

void FHeadMountedDisplay::GetCurrentOrientationAndPosition(FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	GetCurrentHMDPose(CurrentOrientation, CurrentPosition, false, false, FVector::ZeroVector);
}

FVector FHeadMountedDisplay::GetNeckPosition(const FQuat& CurrentOrientation, const FVector& CurrentPosition, const FVector& PositionScale)
{
	const auto frame = GetCurrentFrame();
	if (!frame)
	{
		return FVector::ZeroVector;
	}
	FVector UnrotatedPos = CurrentOrientation.Inverse().RotateVector(CurrentPosition);
	UnrotatedPos.X -= frame->Settings->NeckToEyeInMeters.X * frame->WorldToMetersScale;
	UnrotatedPos.Z -= frame->Settings->NeckToEyeInMeters.Y * frame->WorldToMetersScale;
	return UnrotatedPos;
}

void FHeadMountedDisplay::ApplyHmdRotation(APlayerController* PC, FRotator& ViewRotation)
{
	auto frame = GetCurrentFrame();
	if (!frame)
	{
		return;
	}
	if (!frame->Settings->Flags.bPlayerControllerFollowsHmd)
	{
		return;
	}
#if !UE_BUILD_SHIPPING
	if (frame->Settings->Flags.bDoNotUpdateOnGT)
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
	const FQuat DeltaControlOrientation = DeltaControlRotation.Quaternion();

	ViewRotation = FRotator(DeltaControlOrientation * CurHmdOrientation);

	frame->Flags.bPlayerControllerFollowsHmd = true;
	frame->Flags.bOrientationChanged = true;
	frame->Flags.bPositionChanged = true;
}

bool FHeadMountedDisplay::UpdatePlayerCamera(FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	auto frame = GetCurrentFrame();
	if (!frame)
	{
		return false;
	}

#if !UE_BUILD_SHIPPING
	if (frame->Settings->Flags.bDoNotUpdateOnGT)
	{
		frame->Flags.bOrientationChanged = frame->Settings->Flags.bPlayerCameraManagerFollowsHmdOrientation;// POV.bFollowHmdOrientation;
		frame->Flags.bPositionChanged = frame->Settings->Flags.bPlayerCameraManagerFollowsHmdPosition;//POV.bFollowHmdPosition;
		return false;
	}
#endif
	GetCurrentPose(CurrentOrientation, CurrentPosition, frame->Settings->Flags.bPlayerCameraManagerFollowsHmdOrientation, frame->Settings->Flags.bPlayerCameraManagerFollowsHmdPosition);

	if (frame->Settings->Flags.bPlayerCameraManagerFollowsHmdOrientation) //POV.bFollowHmdOrientation
	{
		frame->LastHmdOrientation = CurrentOrientation;
		frame->Flags.bOrientationChanged = true;
	}

	if (frame->Settings->Flags.bPlayerCameraManagerFollowsHmdPosition) // POV.bFollowHmdPosition
	{
		frame->LastHmdPosition = CurrentPosition;
		frame->Flags.bPositionChanged = true;
	}

	return true;
}

float FHeadMountedDisplay::GetActualScreenPercentage() const
{
	const auto frame = GetCurrentFrame();
	if (frame)
	{
		return frame->Settings->GetActualScreenPercentage();
	}
	return 0.0f;
}

#if !UE_BUILD_SHIPPING
void FHeadMountedDisplay::DrawDebugTrackingCameraFrustum(UWorld* World, const FRotator& ViewRotation, const FVector& ViewLocation)
{
	const auto frame = GetCurrentFrame();
	if (!frame)
	{
		return;
	}
	const FColor c = (HasValidTrackingPosition() ? FColor::Green : FColor::Red);
	FVector origin;
	FQuat orient;
	float hfovDeg, vfovDeg, nearPlane, farPlane, cameraDist;
	GetPositionalTrackingCameraProperties(origin, orient, hfovDeg, vfovDeg, cameraDist, nearPlane, farPlane);

	FVector HeadPosition;
	FQuat HeadOrient;
	GetCurrentPose(HeadOrient, HeadPosition, false, false);
	const FQuat DeltaControlOrientation = ViewRotation.Quaternion() * HeadOrient.Inverse();

	orient = DeltaControlOrientation * orient;
	if (frame->Flags.bPositionChanged && !frame->Flags.bPlayerControllerFollowsHmd)
	{
		origin = origin - HeadPosition;
	}
	origin = DeltaControlOrientation.RotateVector(origin);

	// Level line
	//DrawDebugLine(World, ViewLocation, FVector(ViewLocation.X + 1000, ViewLocation.Y, ViewLocation.Z), FColor::Blue);

	const float hfov = FMath::DegreesToRadians(hfovDeg * 0.5f);
	const float vfov = FMath::DegreesToRadians(vfovDeg * 0.5f);
	FVector coneTop(0, 0, 0);
	FVector coneBase1(-farPlane, farPlane * FMath::Tan(hfov), farPlane * FMath::Tan(vfov));
	FVector coneBase2(-farPlane, -farPlane * FMath::Tan(hfov), farPlane * FMath::Tan(vfov));
	FVector coneBase3(-farPlane, -farPlane * FMath::Tan(hfov), -farPlane * FMath::Tan(vfov));
	FVector coneBase4(-farPlane, farPlane * FMath::Tan(hfov), -farPlane * FMath::Tan(vfov));
	FMatrix m(FMatrix::Identity);
	m = orient * m;
	m *= FScaleMatrix(frame->CameraScale3D);
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
	FVector coneNear2(-nearPlane, -nearPlane * FMath::Tan(hfov), nearPlane * FMath::Tan(vfov));
	FVector coneNear3(-nearPlane, -nearPlane * FMath::Tan(hfov), -nearPlane * FMath::Tan(vfov));
	FVector coneNear4(-nearPlane, nearPlane * FMath::Tan(hfov), -nearPlane * FMath::Tan(vfov));
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

void FHeadMountedDisplay::DrawSeaOfCubes(UWorld* World, FVector ViewLocation)
{
	const auto frame = GetCurrentFrame();
	if (!frame)
	{
		return;
	}
	if (frame->Settings->Flags.bDrawCubes && !SeaOfCubesActorPtr.IsValid())
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnInfo.bNoFail = true;
		SpawnInfo.ObjectFlags = RF_Transient;
		AStaticMeshActor* SeaOfCubesActor;
		SeaOfCubesActorPtr = SeaOfCubesActor = World->SpawnActor<AStaticMeshActor>(SpawnInfo);

		const float cube_side_in_meters = (SideOfSingleCubeInMeters == 0) ? 0.12f : SideOfSingleCubeInMeters;
		const float cube_side = cube_side_in_meters * frame->Settings->WorldToMetersScale;
		const float cubes_volume_in_meters = (SeaOfCubesVolumeSizeInMeters == 0) ? 3.2f : SeaOfCubesVolumeSizeInMeters;
		const float cubes_volume = cubes_volume_in_meters * frame->Settings->WorldToMetersScale;
		const int num_of_cubes = (NumberOfCubesInOneSide == 0) ? 11 : NumberOfCubesInOneSide;
		const TCHAR* const cube_resource_name = (CubeMeshName.IsEmpty()) ? TEXT("/Engine/BasicShapes/Cube.Cube") : *CubeMeshName;
		const TCHAR* const cube_material_name = (CubeMaterialName.IsEmpty()) ? TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial") : *CubeMaterialName;

		UStaticMesh* const BoxMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), NULL,
			cube_resource_name, NULL, LOAD_None, NULL));

		UMaterial* const BoxMat = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), NULL,
			cube_material_name, NULL, LOAD_None, NULL));

		if (!BoxMesh)
		{
			SeaOfCubesActorPtr = nullptr;
			UE_LOG(LogHMD, Log, TEXT("DrawSeaOfCubes: Can't find cube  resource: %s"), cube_resource_name);
			return;
		}
		if (!BoxMat)
		{
			UE_LOG(LogHMD, Log, TEXT("DrawSeaOfCubes: Can't find cube material: %s"), cube_material_name);
			// cubes still can be drawn w/o material, so proceed.
		}
		const FBoxSphereBounds bounds = BoxMesh->GetBounds();

		// Calc dimensions to calculate proper scale to maintain proper size of cubes
		UE_LOG(LogHMD, Log, TEXT("Cube %s: Box extent %s, sphere rad %f"), cube_resource_name,
			*bounds.BoxExtent.ToString(), bounds.SphereRadius);

		const FVector box_scale(cube_side / (bounds.BoxExtent.X * 2), cube_side / (bounds.BoxExtent.Y * 2), cube_side / (bounds.BoxExtent.Z * 2));

		const FVector SeaOfCubesOrigin = ViewLocation + (CenterOffsetInMeters * frame->Settings->WorldToMetersScale);

		UInstancedStaticMeshComponent* ISMComponent = NewObject<UInstancedStaticMeshComponent>();
		ISMComponent->AttachTo(SeaOfCubesActor->GetStaticMeshComponent());
		ISMComponent->SetStaticMesh(BoxMesh);
		ISMComponent->SetMaterial(0, BoxMat);
		ISMComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISMComponent->SetCastShadow(false);
		ISMComponent->SetEnableGravity(false);
		ISMComponent->SetStaticLightingMapping(false, 0);
		ISMComponent->InvalidateLightingCache();
		ISMComponent->RegisterComponentWithWorld(World);

		float X = SeaOfCubesOrigin.X - cubes_volume / 2;
		for (int xn = 0; xn < num_of_cubes; ++xn, X += cubes_volume / num_of_cubes)
		{
			float Y = SeaOfCubesOrigin.Y - cubes_volume / 2;
			for (int yn = 0; yn < num_of_cubes; ++yn, Y += cubes_volume / num_of_cubes)
			{
				float Z = SeaOfCubesOrigin.Z - cubes_volume / 2;
				for (int zn = 0; zn < num_of_cubes; ++zn, Z += cubes_volume / num_of_cubes)
				{
					FTransform Transf(FQuat::Identity, FVector(X, Y, Z), box_scale);
					ISMComponent->AddInstanceWorldSpace(Transf);
				}
			}
		}
	}
	else if (!frame->Settings->Flags.bDrawCubes && SeaOfCubesActorPtr.IsValid())
	{
		SeaOfCubesActorPtr.Get()->GetStaticMeshComponent()->SetVisibility(false, true);
		World->DestroyActor(SeaOfCubesActorPtr.Get());
		SeaOfCubesActorPtr = nullptr;
	}
}
#endif // #if !UE_BUILD_SHIPPING

uint32 FHeadMountedDisplay::CreateLayerEx(UTexture2D* InTexture, int32 InPrioirity, FHMDLayerManager::LayerOriginType InLayerOriginType)
{
	FHMDLayerManager* pLayerMgr = GetLayerManager();
	if (pLayerMgr)
	{
		if (InTexture)
		{
			uint32 id;
			TSharedPtr<FHMDLayerDesc> layer = pLayerMgr->AddLayer(FHMDLayerDesc::Quad, InPrioirity, InLayerOriginType, id);
			layer->SetTexture(InTexture);
			return id;
		}
		else
		{
			// non quad/texture layers are not supported yet
			check(0);
		}
	}
	return 0;
}

uint32 FHeadMountedDisplay::CreateLayer(UTexture2D* InTexture, int32 InPrioirity, bool bInHeadLocked)
{
	return CreateLayerEx(InTexture, InPrioirity, (bInHeadLocked) ? FHMDLayerManager::Layer_HeadLocked : FHMDLayerManager::Layer_WorldLocked);
}

void FHeadMountedDisplay::DestroyLayer(uint32 LayerId)
{
	if (LayerId > 0)
	{
		FHMDLayerManager* pLayerMgr = GetLayerManager();
		if (pLayerMgr)
		{
			pLayerMgr->RemoveLayer(LayerId);
		}
	}
}

void FHeadMountedDisplay::SetTransform(uint32 LayerId, const FTransform& InTransform)
{
	if (LayerId > 0)
	{
		FHMDLayerManager* pLayerMgr = GetLayerManager();
		if (pLayerMgr)
		{
			const FHMDLayerDesc* pLayer = pLayerMgr->GetLayerDesc(LayerId);
			if (pLayer && pLayer->GetType() == FHMDLayerDesc::Quad)
			{
				FHMDLayerDesc Layer = *pLayer;
				Layer.SetTransform(InTransform);
				pLayerMgr->UpdateLayer(Layer);
			}
		}
	}
}

void FHeadMountedDisplay::SetQuadSize(uint32 LayerId, const FVector2D& InSize)
{
	if (LayerId > 0)
	{
		FHMDLayerManager* pLayerMgr = GetLayerManager();
		if (pLayerMgr)
		{
			const FHMDLayerDesc* pLayer = pLayerMgr->GetLayerDesc(LayerId);
			if (pLayer && pLayer->GetType() == FHMDLayerDesc::Quad)
			{
				FHMDLayerDesc Layer = *pLayer;
				Layer.SetQuadSize(InSize);
				pLayerMgr->UpdateLayer(Layer);
			}
		}
	}
}

void FHeadMountedDisplay::SetTextureViewport(uint32 LayerId, const FBox2D& UVRect)
{
	if (LayerId > 0)
	{
		FHMDLayerManager* pLayerMgr = GetLayerManager();
		if (pLayerMgr)
		{
			const FHMDLayerDesc* pLayer = pLayerMgr->GetLayerDesc(LayerId);
			if (pLayer && pLayer->GetType() == FHMDLayerDesc::Quad)
			{
				FHMDLayerDesc Layer = *pLayer;
				Layer.SetTextureViewport(UVRect);
				pLayerMgr->UpdateLayer(Layer);
			}
		}
	}
}

void FHeadMountedDisplay::QuantizeBufferSize(int32& InOutBufferSizeX, int32& InOutBufferSizeY, uint32 DividableBy)
{
	const uint32 Mask = ~(DividableBy - 1);
	InOutBufferSizeX = (InOutBufferSizeX + DividableBy - 1) & Mask;
	InOutBufferSizeY = (InOutBufferSizeY + DividableBy - 1) & Mask;
}

/************************************************************************/
/* Layers                                                               */
/************************************************************************/
FHMDLayerDesc::FHMDLayerDesc(class FHMDLayerManager& InLayerMgr, ELayerTypeMask InType, uint32 InPriority, uint32 InID) :
	LayerManager(InLayerMgr)
	, Id(InID | InType)
	, Texture(nullptr)
	, TextureUV(ForceInit)
	, QuadSize(FVector2D::ZeroVector)
	, Priority(InPriority & IdMask)
	, bHighQuality(true)
	, bHeadLocked(false)
	, bTorsoLocked(false)
	, bTextureHasChanged(true)
	, bTransformHasChanged(true)
	, bNewLayer(true)
	, bAlreadyAdded(false)
{
	TextureUV.Min = FVector2D(0, 0);
	TextureUV.Max = FVector2D(1, 1);
}

void FHMDLayerDesc::SetTransform(const FTransform& InTrn)
{
	Transform = InTrn;
	bTransformHasChanged = true;
	LayerManager.SetDirty();
}

void FHMDLayerDesc::SetQuadSize(const FVector2D& InSize)
{
	QuadSize = InSize;
	bTransformHasChanged = true;
	LayerManager.SetDirty();
}

void FHMDLayerDesc::SetTexture(UTexture* InTexture)
{
	Texture = InTexture;
	bTextureHasChanged = true;
	LayerManager.SetDirty();
}

void FHMDLayerDesc::SetTextureSet(FTextureSetProxyParamRef InTextureSet)
{
	TextureSet = InTextureSet;
	bTextureHasChanged = true;
	LayerManager.SetDirty();
}

void FHMDLayerDesc::SetTextureViewport(const FBox2D& InUVRect)
{
	TextureUV = InUVRect;
	bTransformHasChanged = true;
	LayerManager.SetDirty();
}

FHMDLayerDesc& FHMDLayerDesc::operator=(const FHMDLayerDesc& InSrc)
{
	check(&LayerManager == &InSrc.LayerManager);
	
	if (Id != InSrc.Id || Priority != InSrc.Priority || bHighQuality != InSrc.bHighQuality || bHeadLocked != InSrc.bHeadLocked || bTorsoLocked != InSrc.bTorsoLocked)
	{
		bNewLayer = true;
		Id = InSrc.Id;
		Priority = InSrc.Priority;
		bHighQuality = InSrc.bHighQuality;
		bHeadLocked = InSrc.bHeadLocked;
		bTorsoLocked = InSrc.bTorsoLocked;
	}
	if (Texture != InSrc.Texture)
	{
		bTextureHasChanged = true;
		Texture = InSrc.Texture;
	}
	if (TextureSet.Get() != InSrc.TextureSet.Get())
	{
		bTextureHasChanged = true;
		TextureSet = InSrc.TextureSet;
	}
	if (!(TextureUV == InSrc.TextureUV) || QuadSize != InSrc.QuadSize || !Transform.Equals(InSrc.Transform))
	{
		bTransformHasChanged = true;
		TextureUV = InSrc.TextureUV;
		QuadSize = InSrc.QuadSize;
		Transform = InSrc.Transform;
	}
	LayerManager.SetDirty();
	return *this;
}

//////////////////////////////////////////////////////////////////////////
FHMDRenderLayer::FHMDRenderLayer(FHMDLayerDesc& InLayerInfo) :
	LayerInfo(InLayerInfo)
	, bOwnsTextureSet(true)
{
	if (InLayerInfo.HasTextureSet())
	{
		TransferTextureSet(InLayerInfo);
	}
}

TSharedPtr<FHMDRenderLayer> FHMDRenderLayer::Clone() const
{
	TSharedPtr<FHMDRenderLayer> NewLayer = MakeShareable(new FHMDRenderLayer(*this));
	return NewLayer;
}

void FHMDRenderLayer::ReleaseResources()
{
	if (bOwnsTextureSet && TextureSet.IsValid())
	{
		TextureSet->ReleaseResources();
	}
}

//////////////////////////////////////////////////////////////////////////
FHMDLayerManager::FHMDLayerManager() : 
	CurrentId(0)
	, bLayersChanged(false)
{
}

FHMDLayerManager::~FHMDLayerManager()
{
}

void FHMDLayerManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	//Collector.AddReferencedObject(WidgetVertexColorMaterial);
	for (uint32 i = 0, n = EyeLayers.Num(); i < n; ++i)
	{
		if (EyeLayers[i].IsValid() && EyeLayers[i]->HasTexture())
		{
			Collector.AddReferencedObject(EyeLayers[i]->GetUTextureRef());
		}
	}
	for (uint32 i = 0, n = QuadLayers.Num(); i < n; ++i)
	{
		if (QuadLayers[i].IsValid() && QuadLayers[i]->HasTexture())
		{
			Collector.AddReferencedObject(QuadLayers[i]->GetUTextureRef());
		}
	}
	for (uint32 i = 0, n = DebugLayers.Num(); i < n; ++i)
	{
		if (DebugLayers[i].IsValid() && DebugLayers[i]->HasTexture())
		{
			Collector.AddReferencedObject(DebugLayers[i]->GetUTextureRef());
		}
	}
}

void FHMDLayerManager::Startup()
{
}

void FHMDLayerManager::Shutdown()
{
	{
		FScopeLock ScopeLock(&LayersLock);
		RemoveAllLayers();

		LayersToRender.SetNum(0);

		CurrentId = 0;
		bLayersChanged = false;
	}
}

TSharedPtr<FHMDLayerDesc> 
FHMDLayerManager::AddLayer(FHMDLayerDesc::ELayerTypeMask InType, uint32 InPriority, LayerOriginType InLayerOriginType, uint32& OutLayerId)
{
	TSharedPtr<FHMDLayerDesc> NewLayerDesc = MakeShareable(new FHMDLayerDesc(*this, InType, InPriority, CurrentId++));

	switch (InLayerOriginType)
	{
	case Layer_HeadLocked:
		NewLayerDesc->bHeadLocked = true;
		break;

	case Layer_TorsoLocked:
		NewLayerDesc->bTorsoLocked = true;
		break;

	default: // default is WorldLocked
		break;
	}

	OutLayerId = NewLayerDesc->GetId();

	FScopeLock ScopeLock(&LayersLock);
	TArray<TSharedPtr<FHMDLayerDesc> >& Layers = GetLayersArrayById(OutLayerId);
	Layers.Add(NewLayerDesc);

	bLayersChanged = true;
	return NewLayerDesc;
}

void FHMDLayerManager::RemoveLayer(uint32 LayerId)
{
	FScopeLock ScopeLock(&LayersLock);
	TArray<TSharedPtr<FHMDLayerDesc> >& Layers = GetLayersArrayById(LayerId);
	uint32 idx = FindLayerIndex(Layers, LayerId);
	if (idx != ~0u)
	{
		Layers.RemoveAt(idx);
	}
	bLayersChanged = true;
}

void FHMDLayerManager::RemoveAllLayers()
{
	FScopeLock ScopeLock(&LayersLock);
	EyeLayers.SetNum(0);
	QuadLayers.SetNum(0);
	DebugLayers.SetNum(0);
	bLayersChanged = true;
}

const FHMDLayerDesc* FHMDLayerManager::GetLayerDesc(uint32 LayerId) const
{
	FScopeLock ScopeLock(&LayersLock);
	return FindLayer_NoLock(LayerId).Get();
}

const TArray<TSharedPtr<FHMDLayerDesc> >& FHMDLayerManager::GetLayersArrayById(uint32 LayerId) const
{
	switch (LayerId & FHMDLayerDesc::TypeMask)
	{
	case FHMDLayerDesc::Eye:
		return EyeLayers;
		break;
	case FHMDLayerDesc::Quad:
		return QuadLayers;
		break;
	case FHMDLayerDesc::Debug:
		return DebugLayers;
		break;
	default:
		check(0);
	}
	return EyeLayers;
}

TArray<TSharedPtr<FHMDLayerDesc> >& FHMDLayerManager::GetLayersArrayById(uint32 LayerId) 
{
	switch (LayerId & FHMDLayerDesc::TypeMask)
	{
	case FHMDLayerDesc::Eye:
		return EyeLayers;
		break;
	case FHMDLayerDesc::Quad:
		return QuadLayers;
		break;
	case FHMDLayerDesc::Debug:
		return DebugLayers;
		break;
	default:
		check(0);
	}
	return EyeLayers;
}

// returns ~0u if not found
uint32 FHMDLayerManager::FindLayerIndex(const TArray<TSharedPtr<FHMDLayerDesc> >& Layers, uint32 LayerId)
{
	for (uint32 i = 0, n = Layers.Num(); i < n; ++i)
	{
		if (Layers[i].IsValid() && Layers[i]->GetId() == LayerId)
		{
			return i;
		}
	}
	return ~0u;
}

TSharedPtr<FHMDLayerDesc> FHMDLayerManager::FindLayer_NoLock(uint32 LayerId) const
{
	const TArray<TSharedPtr<FHMDLayerDesc> >& Layers = GetLayersArrayById(LayerId);
		uint32 idx = FindLayerIndex(Layers, LayerId);
	if (idx != ~0u)
	{
		return Layers[idx];
	}
	return nullptr;
}

void FHMDLayerManager::UpdateLayer(const FHMDLayerDesc& InLayerDesc)
{
	FScopeLock ScopeLock(&LayersLock);
	TArray<TSharedPtr<FHMDLayerDesc> >& Layers = GetLayersArrayById(InLayerDesc.GetId());
	uint32 idx = FindLayerIndex(Layers, InLayerDesc.GetId());
	if (idx != ~0u)
	{
		*Layers[idx].Get() = InLayerDesc;
		SetDirty();
	}
}

TSharedPtr<FHMDRenderLayer> FHMDLayerManager::CreateRenderLayer_RenderThread(FHMDLayerDesc& InDesc)
{
	TSharedPtr<FHMDRenderLayer> NewLayer = MakeShareable(new FHMDRenderLayer(InDesc));
	return NewLayer;
}

FHMDRenderLayer* FHMDLayerManager::GetRenderLayer_RenderThread_NoLock(uint32 LayerId)
{
	for (uint32 i = 0, n = LayersToRender.Num(); i < n; ++i)
	{
		auto RenderLayer = LayersToRender[i];
		if (RenderLayer->GetLayerDesc().GetId() == LayerId)
		{
			return RenderLayer.Get();
		}
	}
	return nullptr;
}

void FHMDLayerManager::ReleaseTextureSetsInArray_RenderThread_NoLock(TArray<TSharedPtr<FHMDLayerDesc> >& Layers)
{
	for (int32 i = 0; i < Layers.Num(); ++i)
	{
		if (Layers[i].IsValid())
		{
			FTextureSetProxyRef TexSet = Layers[i]->GetTextureSet();
			if (TexSet.IsValid())
			{
				TexSet->ReleaseResources();
			}
			Layers[i]->SetTextureSet(nullptr);
		}
	}
}

void FHMDLayerManager::ReleaseTextureSets_RenderThread_NoLock()
{
	ReleaseTextureSetsInArray_RenderThread_NoLock(EyeLayers);
	ReleaseTextureSetsInArray_RenderThread_NoLock(QuadLayers);
	ReleaseTextureSetsInArray_RenderThread_NoLock(DebugLayers);
}

void FHMDLayerManager::PreSubmitUpdate_RenderThread(FRHICommandListImmediate& RHICmdList, const FHMDGameFrame* CurrentFrame, bool ShowFlagsRendering)
{
	check(IsInRenderingThread());

	if (bLayersChanged)
	{
		// If layers were changed then make a new snapshot of layers for rendering.
		// Then sort the array by priority.
		FScopeLock ScopeLock(&LayersLock);

		// go through render layers and check whether the layer is modified or removed
		uint32 NumOfAlreadyAdded = 0;
		for (uint32 i = 0, n = LayersToRender.Num(); i < n; ++i)
		{
			auto RenderLayer = LayersToRender[i];
			TSharedPtr<FHMDLayerDesc> LayerDesc = FindLayer_NoLock(RenderLayer->GetLayerDesc().GetId());
			if (LayerDesc.IsValid() && !LayerDesc->IsNewLayer())
			{
				// the layer has changed. modify the render counterpart accordingly.
				if (LayerDesc->IsTextureChanged())
				{
					RenderLayer->ReleaseResources();
					if (LayerDesc->HasTextureSet())
					{
						check(!LayerDesc->HasTexture());
						RenderLayer->TransferTextureSet(*LayerDesc.Get());
					}
					else if (LayerDesc->HasTexture())
					{
						check(!LayerDesc->HasTextureSet());
					}
				}
				if (LayerDesc->IsTransformChanged())
				{
					RenderLayer->SetLayerDesc(*LayerDesc.Get());
				}
				LayerDesc->bAlreadyAdded = true;
				++NumOfAlreadyAdded;
			}
			else
			{
				// layer desc is not found, deleted, or just added. Release resources, kill the renderlayer.
				RenderLayer->ReleaseResources();
				LayersToRender[i] = nullptr;
			}
		}

 		LayersToRender.SetNum(EyeLayers.Num() + QuadLayers.Num() + DebugLayers.Num() + LayersToRender.Num() - NumOfAlreadyAdded);

		for (uint32 i = 0, n = EyeLayers.Num(); i < n; ++i)
		{
			TSharedPtr<FHMDLayerDesc> pLayer = EyeLayers[i];
			check(pLayer.IsValid());
			if (!pLayer->bAlreadyAdded) // add only new layers, not already added to the LayersToRender array
			{
				LayersToRender.Add(CreateRenderLayer_RenderThread(*pLayer.Get()));
			}
			pLayer->ResetChangedFlags();
		}
		for (uint32 i = 0, n = QuadLayers.Num(); i < n; ++i)
		{
			TSharedPtr<FHMDLayerDesc> pLayer = QuadLayers[i];
			check(pLayer.IsValid());
			if (!pLayer->bAlreadyAdded) // add only new layers, not already added to the LayersToRender array
			{
				LayersToRender.Add(CreateRenderLayer_RenderThread(*pLayer.Get()));
			}
			pLayer->ResetChangedFlags();
		}
		for (uint32 i = 0, n = DebugLayers.Num(); i < n; ++i)
		{
			TSharedPtr<FHMDLayerDesc> pLayer = DebugLayers[i];
			check(pLayer.IsValid());
			if (!pLayer->bAlreadyAdded) // add only new layers, not already added to the LayersToRender array
			{
				LayersToRender.Add(CreateRenderLayer_RenderThread(*pLayer.Get()));
			}
			pLayer->ResetChangedFlags();
		}

		struct Comparator
		{
			bool operator()(const TSharedPtr<FHMDRenderLayer>& l1,
							const TSharedPtr<FHMDRenderLayer>& l2) const
			{
				if (!l1.IsValid())
				{
					return false;
				}
				if (!l2.IsValid())
				{
					return true;
				}
				auto LayerDesc1 = l1->GetLayerDesc();
				auto LayerDesc2 = l2->GetLayerDesc();
				return (LayerDesc1.GetPriority() | LayerDesc1.GetType()) < (LayerDesc2.GetPriority() | LayerDesc2.GetType());
			}
		};
		LayersToRender.Sort(Comparator());
		// all empty (nullptr) entries should be at the end of the array. 
		// The total number of render layers should be equal to sum of all layers.
		LayersToRender.SetNum(EyeLayers.Num() + QuadLayers.Num() + DebugLayers.Num());
		bLayersChanged = false;
	}
}




