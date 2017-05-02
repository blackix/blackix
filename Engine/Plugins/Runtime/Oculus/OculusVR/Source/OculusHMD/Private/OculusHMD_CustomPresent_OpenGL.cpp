// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_CustomPresent.h"
#include "OculusHMDPrivateRHI.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS_OPENGL
#include "OculusHMD.h"

#if PLATFORM_WINDOWS
#ifndef WINDOWS_PLATFORM_TYPES_GUARD
#include "AllowWindowsPlatformTypes.h"
#endif
#endif

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// OpenGLCreateTexture2DAlias
//-------------------------------------------------------------------------------------------------

static FOpenGLTexture2D* OpenGLCreateTexture2DAlias(
	FOpenGLDynamicRHI* InGLRHI,
	GLuint InResource,
	uint32 InSizeX,
	uint32 InSizeY,
	uint32 InSizeZ,
	uint32 InNumMips,
	uint32 InNumSamples,
	uint32 InNumSamplesTileMem,
	EPixelFormat InFormat,
	uint32 InFlags)
{
	GLenum Target = (InNumSamples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
	GLenum Attachment = GL_NONE;
	bool bAllocatedStorage = false;
	uint32 NumMips = 1;
	uint8* TextureRange = nullptr;

	FOpenGLTexture2D* NewTexture = new FOpenGLTexture2D(
		InGLRHI,
		InResource,
		Target,
		Attachment, InSizeX, InSizeY, 0, InNumMips, InNumSamples, InNumSamplesTileMem, 1, InFormat, false, bAllocatedStorage, InFlags, TextureRange, FClearValueBinding::None);

	OpenGLTextureAllocated(NewTexture, InFlags);
	return NewTexture;
}


//-------------------------------------------------------------------------------------------------
// FCustomPresentGL
//-------------------------------------------------------------------------------------------------

class FOpenGLCustomPresent : public FCustomPresent
{
public:
	FOpenGLCustomPresent(FOculusHMD* InOculusHMD);

	// Implementation of FCustomPresent, called by Plugin itself
	virtual ovrpRenderAPIType GetRenderAPI() const override;
	virtual bool IsUsingCorrectDisplayAdapter() override;
	virtual void UpdateMirrorTexture_RenderThread() override;

	virtual void* GetOvrpDevice() const override;
	virtual EPixelFormat GetDefaultPixelFormat() const override;
	virtual FTextureSetProxyPtr CreateTextureSet_RenderThread(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, uint32 InNumMips, uint32 InNumSamples, uint32 InArraySize, const TArray<ovrpTextureHandle>& InTextures) override;
};


FOpenGLCustomPresent::FOpenGLCustomPresent(FOculusHMD* InOculusHMD) :
	FCustomPresent(InOculusHMD)
{
}


ovrpRenderAPIType FOpenGLCustomPresent::GetRenderAPI() const
{
	return ovrpRenderAPI_OpenGL;
}


bool FOpenGLCustomPresent::IsUsingCorrectDisplayAdapter()
{
#if PLATFORM_WINDOWS
	// UNDONE
#endif
	return true;
}


void FOpenGLCustomPresent::UpdateMirrorTexture_RenderThread()
{
	SCOPE_CYCLE_COUNTER(STAT_BeginRendering);

	CheckInRenderThread();

	static const auto CVarMirrorMode = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.MirrorMode"));
	EMirrorWindowMode MirrorWindowMode = (EMirrorWindowMode)(CVarMirrorMode ? FMath::Clamp(CVarMirrorMode->GetValueOnRenderThread(), 0, (int32)EMirrorWindowMode::Last) : 1);
	const FVector2D MirrorWindowSize = OculusHMD->GetFrame_RenderThread()->WindowSize;

	if (ovrp_GetInitialized())
	{
		// Need to destroy mirror texture?
		if (MirrorTextureRHI.IsValid() && MirrorWindowSize != FVector2D(MirrorTextureRHI->GetSizeX(), MirrorTextureRHI->GetSizeY()))
		{
			ExecuteOnRHIThread([]()
			{
				ovrp_DestroyMirrorTexture2();
			});

			MirrorTextureRHI = nullptr;
		}

		// Need to create mirror texture?
		if (!MirrorTextureRHI.IsValid() &&
			MirrorWindowMode == EMirrorWindowMode::Distorted &&
			MirrorWindowSize.X != 0 && MirrorWindowSize.Y != 0)
		{
			int Width = (int)MirrorWindowSize.X;
			int Height = (int)MirrorWindowSize.Y;
			ovrpTextureHandle TextureHandle;

			ExecuteOnRHIThread([&]()
			{
				ovrp_SetupMirrorTexture2(GetOvrpDevice(), Height, Width, ovrpTextureFormat_R8G8B8A8_sRGB, &TextureHandle);
			});

			UE_LOG(LogHMD, Log, TEXT("Allocated a new mirror texture (size %d x %d)"), Width, Height);

			MirrorTextureRHI = OpenGLCreateTexture2DAlias(
				static_cast<FOpenGLDynamicRHI*>(GDynamicRHI),
				(GLuint) TextureHandle,
				Width,
				Height,
				0,
				1,
				1,
				1,
				(EPixelFormat)PF_R8G8B8A8,
				TexCreate_RenderTargetable);
		}
	}
}


void* FOpenGLCustomPresent::GetOvrpDevice() const
{
	return nullptr;
}


EPixelFormat FOpenGLCustomPresent::GetDefaultPixelFormat() const
{
	return PF_R8G8B8A8;
}


FTextureSetProxyPtr FOpenGLCustomPresent::CreateTextureSet_RenderThread(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, uint32 InNumMips, uint32 InNumSamples, uint32 InArraySize, const TArray<ovrpTextureHandle>& InTextures)
{
	CheckInRenderThread();

	int systemRecommendedMSAALevel = 1;
	ovrp_GetSystemRecommendedMSAALevel2(&systemRecommendedMSAALevel);

	return CreateTextureSetProxy_OpenGL(InSizeX, InSizeY, InFormat, InNumMips, InNumSamples, systemRecommendedMSAALevel, InArraySize, InTextures);
}


//-------------------------------------------------------------------------------------------------
// APIs
//-------------------------------------------------------------------------------------------------

FCustomPresent* CreateCustomPresent_OpenGL(FOculusHMD* InOculusHMD)
{
	return new FOpenGLCustomPresent(InOculusHMD);
}


} // namespace OculusHMD

#if PLATFORM_WINDOWS
#undef WINDOWS_PLATFORM_TYPES_GUARD
#endif

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS_OPENGL
