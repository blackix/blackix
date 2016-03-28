// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
//
#include "HMDPrivatePCH.h"
#define OVR_CPP_INCLUDES_D3D12
#include "OculusRiftHMD.h"

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#if defined(OVR_D3D)

#include "D3D12RHIPrivate.h"
#include "D3D12Util.h"

#ifndef WINDOWS_PLATFORM_TYPES_GUARD
#include "AllowWindowsPlatformTypes.h"
#endif

#include "OVR_CAPI_D3D.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "PostProcess/PostProcessHMD.h"
#include "ScreenRendering.h"
#include "SlateBasics.h"

//-------------------------------------------------------------------------------------------------
// FD3D12Texture2DSetProxy 
//-------------------------------------------------------------------------------------------------

class FD3D12Texture2DSetProxy : public FTexture2DSetProxy
{
public:
	FD3D12Texture2DSetProxy(FOvrSessionSharedParamRef InOvrSession, ovrTextureSwapChain InOvrTextureSwapChain, FTexture2DRHIParamRef InRHITexture, const TArray<FTexture2DRHIRef>& InRHITextureSwapChain, EPixelFormat SrcFormat) : 
		FTexture2DSetProxy(InOvrSession, InRHITexture, InRHITexture->GetSizeX(), InRHITexture->GetSizeY(), SrcFormat, InRHITexture->GetNumMips()),
		OvrTextureSwapChain(InOvrTextureSwapChain), RHITextureSwapChain(InRHITextureSwapChain) {}

	virtual ovrTextureSwapChain GetSwapTextureSet() const override
	{
		return OvrTextureSwapChain;
	}

	virtual void SwitchToNextElement() override
	{
		if (RHITexture.IsValid())
		{
			FOvrSessionShared::AutoSession OvrSession(Session);
			int CurrentIndex;
			ovr_GetTextureSwapChainCurrentIndex(OvrSession, OvrTextureSwapChain, &CurrentIndex);

			FD3D12DynamicRHI* DynamicRHI = static_cast<FD3D12DynamicRHI*>(GDynamicRHI);

			DynamicRHI->RHIAliasTexture2DResources(RHITexture->GetTexture2D(), RHITextureSwapChain[CurrentIndex]);
		}
	}

	virtual void ReleaseResources() override
	{
		RHITexture = nullptr;
		RHITextureSwapChain.Empty();

		if (OvrTextureSwapChain)
		{
			FOvrSessionShared::AutoSession OvrSession(Session);
			ovr_DestroyTextureSwapChain(OvrSession, OvrTextureSwapChain);
			OvrTextureSwapChain = nullptr;
		}
	}

protected:
	ovrTextureSwapChain OvrTextureSwapChain;
	TArray<FTexture2DRHIRef> RHITextureSwapChain;
};


//-------------------------------------------------------------------------------------------------
// FOculusRiftHMD::D3D12Bridge
//-------------------------------------------------------------------------------------------------

FOculusRiftHMD::D3D12Bridge::D3D12Bridge(FOvrSessionSharedParamRef InOvrSession)
	: FCustomPresent(InOvrSession)
{
	check(IsInGameThread());

	// Disable RHI thread
	if(GRHISupportsRHIThread && GIsThreadedRendering && GUseRHIThread)
	{
		FSuspendRenderingThread SuspendRenderingThread(true);
		GUseRHIThread = false;
	}
}

bool FOculusRiftHMD::D3D12Bridge::IsUsingGraphicsAdapter(const ovrGraphicsLuid& luid)
{
	TRefCountPtr<ID3D12Device> D3DDevice;

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		GetNativeDevice,
		TRefCountPtr<ID3D12Device>&, D3DDeviceRef, D3DDevice,
		{
			D3DDeviceRef = (ID3D12Device*) RHIGetNativeDevice();
		});

	FlushRenderingCommands();

	if(D3DDevice)
	{
		LUID AdapterLuid = D3DDevice->GetAdapterLuid();
		return !FMemory::Memcmp(&luid, &AdapterLuid, sizeof(LUID));
	}

	// Not enough information.  Assume that we are using the correct adapter.
	return true;
}

void FOculusRiftHMD::D3D12Bridge::BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT)
{
	SCOPE_CYCLE_COUNTER(STAT_BeginRendering);

	check(IsInRenderingThread());

	SetRenderContext(&InRenderContext);

	FGameFrame* CurrentFrame = GetRenderFrame();
	check(CurrentFrame);
	FSettings* FrameSettings = CurrentFrame->GetSettings();
	check(FrameSettings);

	const uint32 RTSizeX = RT->GetSizeX();
	const uint32 RTSizeY = RT->GetSizeY();

	const FVector2D ActualMirrorWindowSize = CurrentFrame->WindowSize;
	// detect if mirror texture needs to be re-allocated or freed
	FOvrSessionShared::AutoSession OvrSession(Session);
	if (Session->IsActive() && MirrorTextureRHI && (bNeedReAllocateMirrorTexture || 
		(FrameSettings->Flags.bMirrorToWindow && (
		FrameSettings->MirrorWindowMode != FSettings::eMirrorWindow_Distorted ||
		ActualMirrorWindowSize != FVector2D(MirrorTextureRHI->GetSizeX(), MirrorTextureRHI->GetSizeY()))) ||
		!FrameSettings->Flags.bMirrorToWindow ))
	{
		check(MirrorTexture);
		ovr_DestroyMirrorTexture(OvrSession, MirrorTexture);
		MirrorTexture = nullptr;
		MirrorTextureRHI = nullptr;
		bNeedReAllocateMirrorTexture = false;
	}

	// need to allocate a mirror texture?
	if (FrameSettings->Flags.bMirrorToWindow && FrameSettings->MirrorWindowMode == FSettings::eMirrorWindow_Distorted && !MirrorTextureRHI &&
		ActualMirrorWindowSize.X != 0 && ActualMirrorWindowSize.Y != 0)
	{
		ovrMirrorTextureDesc desc {};
		// Override the format to be sRGB so that the compositor always treats eye buffers
		// as if they're sRGB even if we are sending in linear format textures
		desc.Format = OVR_FORMAT_B8G8R8A8_UNORM_SRGB;
		desc.Width = (int)ActualMirrorWindowSize.X;
		desc.Height = (int)ActualMirrorWindowSize.Y;
		desc.MiscFlags = ovrTextureMisc_DX_Typeless;

		FD3D12DynamicRHI* DynamicRHI = static_cast<FD3D12DynamicRHI*>(GDynamicRHI);

		ovrResult res = ovr_CreateMirrorTextureDX(OvrSession, DynamicRHI->RHIGetD3DCommandQueue(), &desc, &MirrorTexture);
		if (!MirrorTexture || !OVR_SUCCESS(res))
		{
			UE_LOG(LogHMD, Error, TEXT("Can't create a mirror texture, error = %d"), res);
			return;
		}
		bReady = true;

		UE_LOG(LogHMD, Log, TEXT("Allocated a new mirror texture (size %d x %d)"), desc.Width, desc.Height);

		TRefCountPtr<ID3D12Resource> pD3DResource;
		res = ovr_GetMirrorTextureBufferDX(OvrSession, MirrorTexture, IID_PPV_ARGS(pD3DResource.GetInitReference()));
		if (!OVR_SUCCESS(res))
		{
			UE_LOG(LogHMD, Error, TEXT("ovr_GetMirrorTextureBufferDX failed, error = %d"), int(res));
			return;
		}

		MirrorTextureRHI = DynamicRHI->RHICreateTexture2DFromD3D12Resource(
			PF_B8G8R8A8, 
			TexCreate_ShaderResource, 
			FClearValueBinding::None, 
			pD3DResource);

		bNeedReAllocateMirrorTexture = false;
	}
}

FTexture2DSetProxyRef FOculusRiftHMD::D3D12Bridge::CreateTextureSet(const uint32 InSizeX, const uint32 InSizeY, const EPixelFormat InSrcFormat, const uint32 InNumMips, uint32 InCreateTexFlags)
{
	const EPixelFormat Format = PF_B8G8R8A8;
	const DXGI_FORMAT PlatformResourceFormat = (DXGI_FORMAT)GPixelFormats[Format].PlatformFormat;

	const uint32 TexCreate_Flags = (((InCreateTexFlags & ShaderResource) ? TexCreate_ShaderResource : 0)|
									((InCreateTexFlags & RenderTargetable) ? TexCreate_RenderTargetable : 0));

	ovrTextureSwapChainDesc desc {};
	desc.Type = ovrTexture_2D;
	desc.ArraySize = 1;
	desc.MipLevels = (InNumMips == 0) ? GetNumMipLevels(InSizeX, InSizeY, InCreateTexFlags) : InNumMips;
	check(desc.MipLevels > 0);
	desc.SampleCount = 1;
	desc.StaticImage = (InCreateTexFlags & StaticImage) ? ovrTrue : ovrFalse;
	desc.Width = InSizeX;
	desc.Height = InSizeY;
	// Override the format to be sRGB so that the compositor always treats eye buffers
	// as if they're sRGB even if we are sending in linear formatted textures
	desc.Format = OVR_FORMAT_B8G8R8A8_UNORM_SRGB;
	desc.MiscFlags = ovrTextureMisc_DX_Typeless;

	// just make sure the proper format is used; if format is different then we might
	// need to make some changes here.
	check(PlatformResourceFormat == DXGI_FORMAT_B8G8R8A8_TYPELESS);

	desc.BindFlags = ovrTextureBind_DX_RenderTarget;
	if (desc.MipLevels != 1)
	{
		desc.MiscFlags |= ovrTextureMisc_AllowGenerateMips;
	}

	FD3D12DynamicRHI* DynamicRHI = static_cast<FD3D12DynamicRHI*>(GDynamicRHI);

	ovrTextureSwapChain OvrTextureSwapChain;
	FOvrSessionShared::AutoSession OvrSession(Session);
	ovrResult res = ovr_CreateTextureSwapChainDX(OvrSession, DynamicRHI->RHIGetD3DCommandQueue(), &desc, &OvrTextureSwapChain);

	if (!OvrTextureSwapChain || !OVR_SUCCESS(res))
	{
		UE_LOG(LogHMD, Error, TEXT("ovr_CreateTextureSwapChainDX failed (size %d x %d), error = %d"), desc.Width, desc.Height, res);
		if (res == ovrError_DisplayLost)
		{
			bNeedReAllocateMirrorTexture = bNeedReAllocateTextureSet = true;
			FPlatformAtomics::InterlockedExchange(&NeedToKillHmd, 1);
		}
		return nullptr;
	}
	bReady = true;

	FTexture2DRHIRef RHITexture;
	{
		TRefCountPtr<ID3D12Resource> pD3DResource;
		res = ovr_GetTextureSwapChainBufferDX(OvrSession, OvrTextureSwapChain, 0, IID_PPV_ARGS(pD3DResource.GetInitReference()));
		if (!OVR_SUCCESS(res))
		{
			UE_LOG(LogHMD, Error, TEXT("ovr_GetTextureSwapChainBufferDX failed, error = %d"), int(res));
			return nullptr;
		}

		RHITexture = DynamicRHI->RHICreateTexture2DFromD3D12Resource(
			Format, 
			TexCreate_ShaderResource, 
			FClearValueBinding::None, 
			pD3DResource);
	}

	TArray<FTexture2DRHIRef> RHITextureSwapChain;
	{
		int OvrTextureSwapChainLength;
		ovr_GetTextureSwapChainLength(OvrSession, OvrTextureSwapChain, &OvrTextureSwapChainLength);

		for (int32 i = 0; i < OvrTextureSwapChainLength; ++i)
		{
			TRefCountPtr<ID3D12Resource> pD3DResource;
			res = ovr_GetTextureSwapChainBufferDX(OvrSession, OvrTextureSwapChain, i, IID_PPV_ARGS(pD3DResource.GetInitReference()));
			if (!OVR_SUCCESS(res))
			{
				UE_LOG(LogHMD, Error, TEXT("ovr_GetTextureSwapChainBufferDX failed, error = %d"), int(res));
				return nullptr;
			}

			RHITextureSwapChain.Add(DynamicRHI->RHICreateTexture2DFromD3D12Resource(
				PF_B8G8R8A8, 
				TexCreate_ShaderResource, 
				FClearValueBinding::None, 
				pD3DResource));
		}
	}

	return MakeShareable(new FD3D12Texture2DSetProxy(Session, OvrTextureSwapChain, RHITexture, RHITextureSwapChain, InSrcFormat));
}

#if PLATFORM_WINDOWS
	// It is required to undef WINDOWS_PLATFORM_TYPES_GUARD for any further D3D12 / GL private includes
	#undef WINDOWS_PLATFORM_TYPES_GUARD
#endif

#endif // #ifdef OVR_D3D

#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS
