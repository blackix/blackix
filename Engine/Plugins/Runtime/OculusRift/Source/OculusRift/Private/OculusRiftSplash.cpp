// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
//
#include "HMDPrivatePCH.h"
#include "OculusRiftHMD.h"
#include "OculusRiftSplash.h"

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

FOculusRiftSplash::FOculusRiftSplash(FOculusRiftHMD* InPlugin) : 
	LayerMgr(InPlugin->GetCustomPresent_Internal())
	, pPlugin(InPlugin)
	, SplashLID(~0u)
	, SplashLID_RenderThread(~0u)
	, DisplayRefreshRate(1/90.f)
	, CurrentAngle(0.f)
{
	QuadCenterDistanceInMeters = FVector::ZeroVector;
	QuadSizeInMeters = FVector2D::ZeroVector;
	RotationDeltaInDeg = 0;
	RotationAxis = FVector::ZeroVector;

	const TCHAR* SplashSettings = TEXT("Oculus.Splash.Settings");
	float f;
	FVector vec;
	FVector2D vec2d;
	FString s;
	if (GConfig->GetString(SplashSettings, TEXT("TexturePath"), s, GEngineIni))
	{
		TexturePath = s;
	}
	if (GConfig->GetVector(SplashSettings, TEXT("DistanceInMeters"), vec, GEngineIni))
	{
		QuadCenterDistanceInMeters = vec;
	}
	if (GConfig->GetVector2D(SplashSettings, TEXT("SizeInMeters"), vec2d, GEngineIni))
	{
		QuadSizeInMeters = vec2d;
	}
	if (GConfig->GetFloat(SplashSettings, TEXT("RotationDeltaInDegrees"), f, GEngineIni))
	{
		RotationDeltaInDeg = f;
	}
	if (GConfig->GetVector(SplashSettings, TEXT("RotationAxis"), vec, GEngineIni))
	{
		RotationAxis = vec;
	}
}

void FOculusRiftSplash::Startup()
{
	FAsyncLoadingSplash::Startup();

	ovrHmdDesc Desc = ovr_GetHmdDesc(nullptr);
	DisplayRefreshRate = Desc.DisplayRefreshRate;
	LayerMgr.Startup();
}

void FOculusRiftSplash::Shutdown()
{
	LayerMgr.RemoveAllLayers();

	FCustomPresent* pCustomPresent = pPlugin->GetCustomPresent_Internal();
	if (SplashLID != ~0u && pCustomPresent)
	{
		pCustomPresent->UnlockSubmitFrame();
	}
	SplashLID = ~0u;
	ShowingBlack.Set(0);
	LayerMgr.Shutdown();

	FAsyncLoadingSplash::Shutdown();
}

void FOculusRiftSplash::ReleaseResources()
{
	if (SplashLID != ~0u)
	{
		LayerMgr.RemoveLayer(SplashLID);
		SplashLID = ~0u;

		FCustomPresent* pCustomPresent = pPlugin->GetCustomPresent_Internal();
		if (pCustomPresent)
		{
			pCustomPresent->UnlockSubmitFrame();
		}
	}
	ShowingBlack.Set(0);
	LayerMgr.ReleaseTextureSets();
}

void FOculusRiftSplash::Tick(float DeltaTime)
{
	FCustomPresent* pCustomPresent = pPlugin->GetCustomPresent_Internal();
	FGameFrame* pCurrentFrame = (FGameFrame*)RenderFrame.Get();
	if (pCustomPresent && pCurrentFrame && pCustomPresent->GetSession())
	{
		static double LastHighFreqTime = FPlatformTime::Seconds();
		double CurTime = FPlatformTime::Seconds();
		double DeltaSecondsHighFreq = CurTime - LastHighFreqTime;

		// Let update only each 10 fps
		if ((RotationDeltaInDeg != 0.f && DeltaSecondsHighFreq > 2.f / DisplayRefreshRate) ||
			DeltaSecondsHighFreq > 30.f / DisplayRefreshRate)
		{
			const FHMDLayerDesc* pLayerDesc = LayerMgr.GetLayerDesc(SplashLID_RenderThread);
			if (pLayerDesc)
			{
				FHMDLayerDesc layerDesc = *pLayerDesc;
				FTransform transform(layerDesc.GetTransform());
				FQuat quat(RotationAxis, CurrentAngle);
				transform.SetRotation(quat);
				layerDesc.SetTransform(transform);
				LayerMgr.UpdateLayer(layerDesc);
				CurrentAngle += FMath::DegreesToRadians(RotationDeltaInDeg);
			}

			LayerMgr.PreSubmitUpdate_RenderThread(FRHICommandListExecutor::GetImmediateCommandList(), pCurrentFrame, false);
			check(pCustomPresent->GetSession());
			LayerMgr.SubmitFrame_RenderThread(pCustomPresent->GetSession(), pCurrentFrame, false);

			if (DeltaSecondsHighFreq > 0.5)
			{
				UE_LOG(LogHMD, Log, TEXT("DELTA > 0.5f, ie: %.4f %.4f"), DeltaTime, float(DeltaSecondsHighFreq));
			}
			LastHighFreqTime = CurTime;
		}
	}
}

bool FOculusRiftSplash::IsTickable() const
{
	return IsLoadingStarted() && !IsDone() && ShowingBlack.GetValue() != 1;
}

void FOculusRiftSplash::Show()
{
	FCustomPresent* pCustomPresent = pPlugin->GetCustomPresent_Internal();
	if (pCustomPresent)
	{
		if (!TexturePath.IsEmpty())
		{
			LoadTexture(TexturePath);
		}
		if (LoadingTexture->IsValidLowLevel())
		{
			if (SplashLID != ~0u)
			{
				LayerMgr.RemoveLayer(SplashLID);
				SplashLID = ~0u;
			}

			TSharedPtr<FHMDLayerDesc> layer = LayerMgr.AddLayer(FHMDLayerDesc::Quad, 10, FHMDLayerManager::Layer_TorsoLocked, SplashLID);
			check(layer.IsValid());
			layer->SetTexture(LoadingTexture);
			FTransform tr(QuadCenterDistanceInMeters);
			layer->SetTransform(tr);
			layer->SetQuadSize(QuadSizeInMeters);

			// this will push black frame, if texture is not loaded
			pPlugin->InitDevice();

			ShowingBlack.Set(0);
			CurrentAngle = 0.f;
			PushFrame();
		}
		else
		{
			PushBlackFrame();
		}
		pCustomPresent->LockSubmitFrame();
	}
}

struct FSplashRenParams 
{
	FCustomPresent*		pCustomPresent;
	FHMDGameFrameRef	CurrentFrame;
	FHMDGameFrameRef*	RenderFrameRef;
	uint32				SplashLID;
	uint32*				pSplashLID_RenderThread;
};

void FOculusRiftSplash::PushFrame()
{
	FCustomPresent* pCustomPresent = pPlugin->GetCustomPresent_Internal();
	check(pCustomPresent);
	if (pCustomPresent && pCustomPresent->GetSession())
	{
		// Create a fake frame to pass it to layer manager
		TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> CurrentFrame = pPlugin->CreateNewGameFrame();
		CurrentFrame->Settings = pPlugin->GetSettings()->Clone();
		CurrentFrame->FrameNumber = GFrameCounter + 1; // make sure no 0 frame is used.
		// keep units in meters rather than UU (because UU make not much sense).
		CurrentFrame->WorldToMetersScale = CurrentFrame->Settings->WorldToMetersScale = 1.0f;

		ovr_GetPredictedDisplayTime(pCustomPresent->GetSession(), CurrentFrame->FrameNumber);

		FSplashRenParams params;
		params.pCustomPresent = pCustomPresent;
		params.CurrentFrame = CurrentFrame;
		params.RenderFrameRef = &RenderFrame;
		params.SplashLID = SplashLID;
		params.pSplashLID_RenderThread = &SplashLID_RenderThread;

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(SubmitSplash,
		const FSplashRenParams&, Params, params,
		FLayerManager&, LayerMgr, LayerMgr,
		{
			*Params.RenderFrameRef			= Params.CurrentFrame;
			*Params.pSplashLID_RenderThread = Params.SplashLID;

			auto pCurrentFrame = (FGameFrame*)Params.CurrentFrame.Get();
			check(Params.pCustomPresent->GetSession());
			ovr_GetPredictedDisplayTime(Params.pCustomPresent->GetSession(), pCurrentFrame->FrameNumber);
			LayerMgr.PreSubmitUpdate_RenderThread(RHICmdList, pCurrentFrame, false);
			LayerMgr.SubmitFrame_RenderThread(Params.pCustomPresent->GetSession(), pCurrentFrame, false);
		});
		FlushRenderingCommands();
	}
}

void FOculusRiftSplash::PushBlackFrame()
{
	ShowingBlack.Set(1);
	if (SplashLID != ~0u)
	{
		LayerMgr.RemoveLayer(SplashLID);
		SplashLID = ~0u;
	}

	// create an empty quad layer with no texture
	TSharedPtr<FHMDLayerDesc> layer = LayerMgr.AddLayer(FHMDLayerDesc::Quad, 10, FHMDLayerManager::Layer_TorsoLocked, SplashLID);
	check(layer.IsValid());
	layer->SetQuadSize(FVector2D(0.01f, 0.01f));
	PushFrame();
}

void FOculusRiftSplash::Hide()
{
	if (SplashLID != ~0u)
	{
		LayerMgr.RemoveLayer(SplashLID);
		SplashLID = ~0u;
	}
	PushBlackFrame();
	UnloadTexture();

	FCustomPresent* pCustomPresent = pPlugin->GetCustomPresent_Internal();
	if (pCustomPresent)
	{
		pCustomPresent->UnlockSubmitFrame();
	}
}

void FOculusRiftSplash::OnLoadingBegins()
{
	FAsyncLoadingSplash::OnLoadingBegins();
	Show();
}

void FOculusRiftSplash::OnLoadingEnds()
{
	FAsyncLoadingSplash::OnLoadingEnds();
	Hide();
}


#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS

