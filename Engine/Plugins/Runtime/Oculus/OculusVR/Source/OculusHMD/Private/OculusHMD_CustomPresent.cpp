// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_CustomPresent.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "OculusHMD.h"

#if PLATFORM_ANDROID
#include "Android/AndroidJNI.h"
#include "Android/AndroidEGL.h"
#include "AndroidApplication.h"
#endif

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FCustomPresent
//-------------------------------------------------------------------------------------------------

FCustomPresent::FCustomPresent(FOculusHMD* InOculusHMD)
	: FRHICustomPresent(nullptr)
	, OculusHMD(InOculusHMD)
{
	// grab a pointer to the renderer module for displaying our mirror window
	static const FName RendererModuleName("Renderer");
	RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
}


void FCustomPresent::ReleaseResources_RHIThread()
{
	CheckInRHIThread();

	if (MirrorTextureRHI.IsValid())
	{
		ovrp_DestroyMirrorTexture2();
		MirrorTextureRHI = nullptr;
	}
}


void FCustomPresent::Shutdown()
{
	CheckInGameThread();

	// OculusHMD is going away, but this object can live on until viewport is destroyed
	ExecuteOnRenderThread([this]()
	{
		ExecuteOnRHIThread([this]()
		{
			OculusHMD = nullptr;
		});
	});
}

void FCustomPresent::UpdateViewport(FRHIViewport* InViewportRHI)
{
	CheckInGameThread();

	ViewportRHI = InViewportRHI;
	ViewportRHI->SetCustomPresent(this);
}

void FCustomPresent::OnBackBufferResize()
{
	// if we are in the middle of rendering: prevent from calling EndFrame
	ExecuteOnRenderThread([this]()
	{
		ExecuteOnRHIThread_DoNotWait([this]()
		{
			if (OculusHMD)
			{
				FGameFrame* Frame_RHIThread = OculusHMD->GetFrame_RHIThread();

				if (Frame_RHIThread)
				{
					Frame_RHIThread->ShowFlags.Rendering = 0;
				}
			}
		});
	});
}

bool FCustomPresent::Present(int32& SyncInterval)
{
	static const auto CVarMirrorMode = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.MirrorMode"));
	CheckInRHIThread();

	bool bHostPresent = true;

	if (OculusHMD && OculusHMD->GetFrame_RHIThread())
	{
		if (CVarMirrorMode)
		{
			bHostPresent = CVarMirrorMode->GetValueOnRenderThread() > 0; // !!!
		}
		SyncInterval = 0; // VSync off
		FinishRendering_RHIThread();
	}

	return bHostPresent;
}


void FCustomPresent::FinishRendering_RHIThread()
{
	SCOPE_CYCLE_COUNTER(STAT_FinishRendering);
	CheckInRHIThread();

	if (OculusHMD)
	{
		if (OculusHMD->GetFrame_RHIThread()->ShowFlags.Rendering)
		{
			// Update frame stats
#if STATS
			ovrpAppLatencyTimings AppLatencyTimings;			
			if(OVRP_SUCCESS(ovrp_GetAppLatencyTimings2(&AppLatencyTimings)))
			{
				SET_FLOAT_STAT(STAT_LatencyRender, AppLatencyTimings.LatencyRender * 1000.0f);
				SET_FLOAT_STAT(STAT_LatencyTimewarp, AppLatencyTimings.LatencyTimewarp * 1000.0f);
				SET_FLOAT_STAT(STAT_LatencyPostPresent, AppLatencyTimings.LatencyPostPresent * 1000.0f);
				SET_FLOAT_STAT(STAT_ErrorRender, AppLatencyTimings.ErrorRender * 1000.0f);
				SET_FLOAT_STAT(STAT_ErrorTimewarp, AppLatencyTimings.ErrorTimewarp * 1000.0f);
			}
#endif
		}
		else if (!OculusHMD->GetSettings_RHIThread()->Flags.bPauseRendering)
		{
			UE_LOG(LogHMD, Warning, TEXT("Skipping frame: FinishRendering called with no corresponding BeginRendering (was BackBuffer re-allocated?)"));
		}

		OculusHMD->FinishRHIFrame_RHIThread();
	}
}


EPixelFormat FCustomPresent::GetPixelFormat(EPixelFormat Format) const
{
	switch (Format)
	{
	case PF_B8G8R8A8:
	case PF_FloatRGBA:
	case PF_FloatR11G11B10:
	case PF_R8G8B8A8:
		return Format;
	}

	return GetDefaultPixelFormat();
}


EPixelFormat FCustomPresent::GetPixelFormat(ovrpTextureFormat Format) const
{
	switch(Format)
	{
		case ovrpTextureFormat_R8G8B8A8_sRGB:
		case ovrpTextureFormat_R8G8B8A8:
			return PF_R8G8B8A8;
		case ovrpTextureFormat_R16G16B16A16_FP:
			return PF_FloatRGBA;
		case ovrpTextureFormat_R11G11B10_FP:
			return PF_FloatR11G11B10;
		case ovrpTextureFormat_B8G8R8A8_sRGB:
		case ovrpTextureFormat_B8G8R8A8:
			return PF_B8G8R8A8;
	}

	return GetDefaultPixelFormat();
}

ovrpTextureFormat FCustomPresent::GetOvrpTextureFormat(EPixelFormat Format, bool bSRGB) const
{
	switch (Format)
	{
	case PF_B8G8R8A8:
		return bSRGB ? ovrpTextureFormat_B8G8R8A8_sRGB : ovrpTextureFormat_B8G8R8A8;
	case PF_FloatRGBA:
		return ovrpTextureFormat_R16G16B16A16_FP;
	case PF_FloatR11G11B10:
		return ovrpTextureFormat_R11G11B10_FP;
	case PF_R8G8B8A8:
		return bSRGB ? ovrpTextureFormat_R8G8B8A8_sRGB : ovrpTextureFormat_R8G8B8A8;
	}

	return GetOvrpTextureFormat(GetDefaultPixelFormat(), bSRGB);
}


void FCustomPresent::CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef DstTexture, FTextureRHIParamRef SrcTexture, int SrcSizeX, int SrcSizeY,
	FIntRect DstRect, FIntRect SrcRect, bool bAlphaPremultiply, bool bNoAlphaWrite) const
{
	CheckInRenderThread();

	if (DstRect.IsEmpty())
	{
		DstRect = FIntRect(0, 0, DstTexture->GetSizeX(), DstTexture->GetSizeY());
	}
	const uint32 ViewportWidth = DstRect.Width();
	const uint32 ViewportHeight = DstRect.Height();
	const FIntPoint TargetSize(ViewportWidth, ViewportHeight);

	const float SrcTextureWidth = SrcSizeX;
	const float SrcTextureHeight = SrcSizeY;
	float U = 0.f, V = 0.f, USize = 1.f, VSize = 1.f;
	if (!SrcRect.IsEmpty())
	{
		U = SrcRect.Min.X / SrcTextureWidth;
		V = SrcRect.Min.Y / SrcTextureHeight;
		USize = SrcRect.Width() / SrcTextureWidth;
		VSize = SrcRect.Height() / SrcTextureHeight;
	}

	FRHITexture* SrcTextureRHI = SrcTexture;
	RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, &SrcTextureRHI, 1);

	SetRenderTarget(RHICmdList, DstTexture, FTextureRHIRef());
	RHICmdList.SetViewport(DstRect.Min.X, DstRect.Min.Y, 0, DstRect.Max.X, DstRect.Max.Y, 1.0f);

	if (bAlphaPremultiply)
	{
		if (bNoAlphaWrite)
		{
			// for quads, write RGB, RGB = src.rgb * 1 + dst.rgb * 0
			RHICmdList.ClearColorTexture(DstTexture, FLinearColor(0.0f, 0.0f, 0.0f, 1.0f), FIntRect());
			RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI());
		}
		else
		{
			// for quads, write RGBA, RGB = src.rgb * src.a + dst.rgb * 0, A = src.a + dst.a * 0
			RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI());
		}
	}
	else
	{
		if (bNoAlphaWrite)
		{
			RHICmdList.ClearColorTexture(DstTexture, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), FIntRect());
			RHICmdList.SetBlendState(TStaticBlendState<CW_RGB>::GetRHI());
		}
		else
		{
			// for mirror window
			RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
		}
	}

	RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	const auto FeatureLevel = GMaxRHIFeatureLevel;
	auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

	TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
	TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

	static FGlobalBoundShaderState BoundShaderState;
	SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, RendererModule->GetFilterVertexDeclaration().VertexDeclarationRHI, *VertexShader, *PixelShader);

	PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SrcTextureRHI);

	RendererModule->DrawRectangle(
		RHICmdList,
		0, 0,
		ViewportWidth, ViewportHeight,
		U, V,
		USize, VSize,
		TargetSize,
		FIntPoint(1, 1),
		*VertexShader,
		EDRF_Default);
}

} // namespace OculusHMD

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS
