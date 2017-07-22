// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_TextureSetProxy.h"
#include "OculusHMDPrivateRHI.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS_VULKAN
#include "OculusHMD_CustomPresent.h"

#if PLATFORM_WINDOWS
#ifndef WINDOWS_PLATFORM_TYPES_GUARD
#include "AllowWindowsPlatformTypes.h"
#endif
#endif

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FVulkanTextureSetProxy
//-------------------------------------------------------------------------------------------------

class FVulkanTextureSetProxy : public FTextureSetProxy
{
public:
	FVulkanTextureSetProxy(FTextureRHIParamRef InRHITexture, const TArray<FTextureRHIRef>& InRHITextureSwapChain) :
		FTextureSetProxy(InRHITexture, InRHITextureSwapChain.Num()),
		RHITextureSwapChain(InRHITextureSwapChain) {}

	virtual ~FVulkanTextureSetProxy()
	{
		if (InRenderThread())
		{
			ExecuteOnRHIThread([this]()
			{
				ReleaseResources_RHIThread();
			});
		}
		else
		{
			ReleaseResources_RHIThread();
		}
	}

protected:
	virtual void AliasResources_RHIThread() override
	{
		CheckInRHIThread();

		FVulkanDynamicRHI* DynamicRHI = static_cast<FVulkanDynamicRHI*>(GDynamicRHI);
		DynamicRHI->RHIAliasTextureResources(RHITexture, RHITextureSwapChain[SwapChainIndex_RHIThread]);
	}

	void ReleaseResources_RHIThread()
	{
		CheckInRHIThread();

		RHITexture = nullptr;
		RHITextureSwapChain.Empty();
	}

protected:
	TArray<FTextureRHIRef> RHITextureSwapChain;
};


//-------------------------------------------------------------------------------------------------
// APIs
//-------------------------------------------------------------------------------------------------

FTextureSetProxyPtr CreateTextureSetProxy_Vulkan(FTextureRHIParamRef InRHITexture, const TArray<FTextureRHIRef>& InRHITextureSwapChain)
{
	return MakeShareable(new FVulkanTextureSetProxy(InRHITexture, InRHITextureSwapChain));
}


} // namespace OculusHMD

#if PLATFORM_WINDOWS
#undef WINDOWS_PLATFORM_TYPES_GUARD
#endif

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS_VULKAN
