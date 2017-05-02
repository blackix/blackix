// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_TextureSetProxy.h"
#include "OculusHMDPrivateRHI.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS_OPENGL
#include "OculusHMD_CustomPresent.h"

#if PLATFORM_WINDOWS
#ifndef WINDOWS_PLATFORM_TYPES_GUARD
#include "AllowWindowsPlatformTypes.h"
#endif
#endif

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FOpenGLTexture2DSet
//-------------------------------------------------------------------------------------------------

class FOpenGLTexture2DSet : public FOpenGLTexture2D
{
protected:
	FOpenGLTexture2DSet(FOpenGLDynamicRHI* InGLRHI, GLuint InResource, GLenum InTarget, GLenum InAttachment,
		uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, uint32 InArraySize,
		EPixelFormat InFormat, bool bInCubemap, bool bInAllocatedStorage, uint32 InFlags, uint8* InTextureRange);

	void AddTexture(GLuint InTexture);

public:
	static FOpenGLTexture2DSet* CreateTextureSet_RenderThread(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, uint32 InArraySize, const TArray<ovrpTextureHandle>& InTextures);

	void AliasResources_RHIThread(uint32 SwapChainIndex);
	void ReleaseResources_RHIThread();

protected:
	struct SwapChainElement
	{
		GLuint Texture;
	};

	TArray<SwapChainElement> SwapChainElements;
};


FOpenGLTexture2DSet::FOpenGLTexture2DSet(FOpenGLDynamicRHI* InGLRHI, GLuint InResource, GLenum InTarget, GLenum InAttachment,
	uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, uint32 InArraySize,
	EPixelFormat InFormat, bool bInCubemap, bool bInAllocatedStorage, uint32 InFlags, uint8* InTextureRange)

	: FOpenGLTexture2D(InGLRHI, InResource, InTarget, InAttachment, InSizeX, InSizeY, InSizeZ, InNumMips,
	InNumSamples, InNumSamplesTileMem, InArraySize, InFormat, bInCubemap, bInAllocatedStorage, InFlags, InTextureRange, FClearValueBinding())
{
}


void FOpenGLTexture2DSet::AddTexture(GLuint InTexture)
{
	SwapChainElement element;
	element.Texture = InTexture;
	SwapChainElements.Push(element);
}


FOpenGLTexture2DSet* FOpenGLTexture2DSet::CreateTextureSet_RenderThread(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, uint32 InArraySize, const TArray<ovrpTextureHandle>& InTextures)
{
	CheckInRenderThread();

	if (!InTextures.Num() || !(GLuint) InTextures[0])
	{
		return nullptr;
	}

	uint32 TexCreateFlags = TexCreate_ShaderResource | TexCreate_RenderTargetable;

	FOpenGLDynamicRHI* GLRHI = (FOpenGLDynamicRHI*) GDynamicRHI;

	TexCreateFlags |= TexCreate_SRGB;

	FOpenGLTexture2DSet* NewTextureSet = new FOpenGLTexture2DSet(
		GLRHI,
		0,
		InArraySize > 1 ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D,
		GL_NONE,
		InSizeX,
		InSizeY,
		0,
		InNumMips,
		InNumSamples,
		InNumSamplesTileMem,
		InArraySize,
		InFormat,
		false,
		false,
		TexCreateFlags,
		nullptr);

	OpenGLTextureAllocated(NewTextureSet, TexCreateFlags);

	for (int32 TextureIndex = 0; TextureIndex < InTextures.Num(); ++TextureIndex)
	{
		NewTextureSet->AddTexture((GLuint) InTextures[TextureIndex]);
	}

	ExecuteOnRHIThread([&]()
	{
		NewTextureSet->AliasResources_RHIThread(0);
	});

	return NewTextureSet;
}


void FOpenGLTexture2DSet::AliasResources_RHIThread(uint32 SwapChainIndex)
{
	CheckInRHIThread();

	Resource = SwapChainElements[SwapChainIndex].Texture;
}


void FOpenGLTexture2DSet::ReleaseResources_RHIThread()
{
	CheckInRHIThread();

	SwapChainElements.Empty(0);
}


//-------------------------------------------------------------------------------------------------
// FOpenGLTextureSetProxy
//-------------------------------------------------------------------------------------------------

class FOpenGLTextureSetProxy : public FTextureSetProxy
{
public:
	FOpenGLTextureSetProxy(FTextureRHIRef InTexture, uint32 InSwapChainLength)
		: FTextureSetProxy(InTexture, InSwapChainLength) {}

	virtual ~FOpenGLTextureSetProxy()
	{
		auto GLTS = static_cast<FOpenGLTexture2DSet*>(RHITexture->GetTexture2D());

		if (InRenderThread())
		{
			ExecuteOnRHIThread([&]()
			{
				GLTS->ReleaseResources_RHIThread();
			});
		}
		else
		{
			GLTS->ReleaseResources_RHIThread();
		}

		RHITexture = nullptr;
	}

protected:
	virtual void AliasResources_RHIThread() override
	{
		auto GLTS = static_cast<FOpenGLTexture2DSet*>(RHITexture->GetTexture2D());
		GLTS->AliasResources_RHIThread(SwapChainIndex_RHIThread);
	}
};


//-------------------------------------------------------------------------------------------------
// APIs
//-------------------------------------------------------------------------------------------------

FTextureSetProxyPtr CreateTextureSetProxy_OpenGL(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, uint32 InArraySize, const TArray<ovrpTextureHandle>& InTextures)
{
	TRefCountPtr<FOpenGLTexture2DSet> TextureSet = FOpenGLTexture2DSet::CreateTextureSet_RenderThread(InSizeX, InSizeY, InFormat, InNumMips, InNumSamples, InNumSamplesTileMem, InArraySize, InTextures);

	if (TextureSet)
	{
		return MakeShareable(new FOpenGLTextureSetProxy(TextureSet->GetTexture2D(), InTextures.Num()));
	}

	return nullptr;
}


} // namespace OculusHMD

#if PLATFORM_WINDOWS
#undef WINDOWS_PLATFORM_TYPES_GUARD
#endif

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS_OPENGL
