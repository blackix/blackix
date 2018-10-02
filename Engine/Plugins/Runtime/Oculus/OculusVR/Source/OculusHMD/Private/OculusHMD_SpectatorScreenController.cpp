// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_SpectatorScreenController.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "OculusHMD.h"

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FSpectatorScreenController
//-------------------------------------------------------------------------------------------------

FSpectatorScreenController::FSpectatorScreenController(FOculusHMD* InOculusHMD)
	: FDefaultSpectatorScreenController(InOculusHMD)
	, OculusHMD(InOculusHMD)
{
}

#if WITH_OCULUS_PRIVATE_CODE
void FSpectatorScreenController::UpdateSpectatorScreenMode_RenderThread()
{
	// when there is a casting viewport projecting to mirror window, we need to bind the Distorted function
	if ((!IsInRenderingThread() && OculusHMD->CastingViewportRenderTexture) ||
		(IsInRenderingThread() && OculusHMD->CastingViewportRenderTexture_RenderThread))
	{
		ESpectatorScreenMode OldNewMode;
		{
			FScopeLock FrameLock(&NewSpectatorScreenModeLock);

			OldNewMode = NewSpectatorScreenMode;
			NewSpectatorScreenMode = ESpectatorScreenMode::Undistorted;
			FDefaultSpectatorScreenController::UpdateSpectatorScreenMode_RenderThread();
			NewSpectatorScreenMode = OldNewMode;
		}
	}
	else
	{
		FDefaultSpectatorScreenController::UpdateSpectatorScreenMode_RenderThread();
	}
}
#endif


void FSpectatorScreenController::RenderSpectatorScreen_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* BackBuffer, FTexture2DRHIRef RenderTexture, FVector2D WindowSize) const
{
	if (OculusHMD->GetCustomPresent_Internal())
	{
		FDefaultSpectatorScreenController::RenderSpectatorScreen_RenderThread(RHICmdList, BackBuffer, RenderTexture, WindowSize);
	}
}

void FSpectatorScreenController::RenderSpectatorModeUndistorted(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize)
{
	CheckInRenderThread();

#if WITH_OCULUS_PRIVATE_CODE
	if (OculusHMD->CastingViewportRenderTexture_RenderThread)
	{
		// when there is a casting viewport projecting to mirror window, stretch the output to use all pixels efficiently
		const FIntRect SrcRect(0, 0, EyeTexture->GetSizeX(), EyeTexture->GetSizeY());
		const FIntRect DstRect(0, 0, TargetTexture->GetSizeX(), TargetTexture->GetSizeY());

		OculusHMD->CopyTexture_RenderThread(RHICmdList, EyeTexture, SrcRect, TargetTexture, DstRect, false);
		return;
	}
#endif

	FSettings* Settings = OculusHMD->GetSettings_RenderThread();
	FIntRect DestRect(0, 0, TargetTexture->GetSizeX() / 2, TargetTexture->GetSizeY());
	for (int i = 0; i < 2; ++i)
	{
		OculusHMD->CopyTexture_RenderThread(RHICmdList, EyeTexture, Settings->EyeRenderViewport[i], TargetTexture, DestRect, false);
		DestRect.Min.X += TargetTexture->GetSizeX() / 2;
		DestRect.Max.X += TargetTexture->GetSizeX() / 2;
	}
}

void FSpectatorScreenController::RenderSpectatorModeDistorted(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize)
{
	CheckInRenderThread();
	FCustomPresent* CustomPresent = OculusHMD->GetCustomPresent_Internal();
	FTexture2DRHIRef MirrorTexture = CustomPresent->GetMirrorTexture();
	if (MirrorTexture)
	{
		FIntRect SrcRect(0, 0, MirrorTexture->GetSizeX(), MirrorTexture->GetSizeY());
		FIntRect DestRect(0, 0, TargetTexture->GetSizeX(), TargetTexture->GetSizeY());
		OculusHMD->CopyTexture_RenderThread(RHICmdList, MirrorTexture, SrcRect, TargetTexture, DestRect, false);
	}
}

void FSpectatorScreenController::RenderSpectatorModeSingleEye(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize)
{
	CheckInRenderThread();
	FSettings* Settings = OculusHMD->GetSettings_RenderThread();
	const FIntRect SrcRect= Settings->EyeRenderViewport[0];
	const FIntRect DstRect(0, 0, TargetTexture->GetSizeX(), TargetTexture->GetSizeY());

	OculusHMD->CopyTexture_RenderThread(RHICmdList, EyeTexture, SrcRect, TargetTexture, DstRect, false);
}

} // namespace OculusHMD

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS