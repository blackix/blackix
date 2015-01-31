// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLViewport.cpp: OpenGL viewport RHI implementation.
=============================================================================*/

#include "OpenGLDrvPrivate.h"

/**
 * RHI console variables used by viewports.
 */
namespace RHIOpenGLConsoleVariables
{
	int32 SyncInterval = 1;
	static FAutoConsoleVariableRef CVarSyncInterval(
#if PLATFORM_MAC
		TEXT("RHI.SyncInterval"),
#else
		TEXT("RHI.SyncIntervalOgl"),
#endif
		SyncInterval,
		TEXT("When synchronizing with OpenGL, specifies the interval at which to refresh.")
		);
};

void FOpenGLDynamicRHI::RHIGetSupportedResolution(uint32 &Width, uint32 &Height)
{
	PlatformGetSupportedResolution(Width, Height);
}

bool FOpenGLDynamicRHI::RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
{
	const bool Result = PlatformGetAvailableResolutions(Resolutions, bIgnoreRefreshRate);
	if (Result)
	{
		Resolutions.Sort([](const FScreenResolutionRHI& L, const FScreenResolutionRHI& R)
							{
								if (L.Width != R.Width)
								{
									return L.Width < R.Width;
								}
								else if (L.Height != R.Height)
								{
									return L.Height < R.Height;
								}
								else
								{
									return L.RefreshRate < R.RefreshRate;
								}
							});
	}
	return Result;
}

/*=============================================================================
 *	The following RHI functions must be called from the main thread.
 *=============================================================================*/
FViewportRHIRef FOpenGLDynamicRHI::RHICreateViewport(void* WindowHandle,uint32 SizeX,uint32 SizeY,bool bIsFullscreen)
{
	check(IsInGameThread());

//#if !PLATFORM_MAC
//	SCOPED_SUSPEND_RENDERING_THREAD(true);
//#endif

	return new FOpenGLViewport(this,WindowHandle,SizeX,SizeY,bIsFullscreen);
}

void FOpenGLDynamicRHI::RHIResizeViewport(FViewportRHIParamRef ViewportRHI,uint32 SizeX,uint32 SizeY,bool bIsFullscreen)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Viewport,Viewport);
	check( IsInGameThread() );

//#if !PLATFORM_MAC
//	SCOPED_SUSPEND_RENDERING_THREAD(true);
//#endif

	Viewport->Resize(SizeX,SizeY,bIsFullscreen);
}

void FOpenGLDynamicRHI::RHITick( float DeltaTime )
{
}

/*=============================================================================
 *	Viewport functions.
 *=============================================================================*/

// Ignore functions from RHIMethods.h when parsing documentation; Doxygen's preprocessor can't parse the declaration, so spews warnings for the definitions.
#if !UE_BUILD_DOCS

void FOpenGLDynamicRHI::RHIBeginDrawingViewport(FViewportRHIParamRef ViewportRHI, FTextureRHIParamRef RenderTarget)
{
	VERIFY_GL_SCOPE();

	DYNAMIC_CAST_OPENGLRESOURCE(Viewport,Viewport);

	SCOPE_CYCLE_COUNTER(STAT_OpenGLPresentTime);

	check(!DrawingViewport);
	DrawingViewport = Viewport;

	bRevertToSharedContextAfterDrawingViewport = false;
	EOpenGLCurrentContext CurrentContext = PlatformOpenGLCurrentContext( PlatformDevice );
	if( CurrentContext != CONTEXT_Rendering )
	{
		check(CurrentContext == CONTEXT_Shared);
		check(!bIsRenderingContextAcquired || !GUseThreadedRendering);
		bRevertToSharedContextAfterDrawingViewport = true;
		PlatformRenderingContextSetup(PlatformDevice);
	}

	if(!GPUProfilingData.FrameTiming.IsInitialized())
	{
		GPUProfilingData.FrameTiming.InitResource();
	}
	
	// Set the render target and viewport.
	if( RenderTarget )
	{
		FRHIRenderTargetView RTV(RenderTarget);
		RHISetRenderTargets(1, &RTV, FTextureRHIRef(), 0, NULL);
	}
	else
	{
		FRHIRenderTargetView RTV(DrawingViewport->GetBackBuffer());
		RHISetRenderTargets(1, &RTV, FTextureRHIRef(), 0, NULL);
	}
}

void FOpenGLDynamicRHI::RHIEndDrawingViewport(FViewportRHIParamRef ViewportRHI,bool bPresent,bool bLockToVsync)
{
	VERIFY_GL_SCOPE();

	DYNAMIC_CAST_OPENGLRESOURCE(Viewport,Viewport);

	SCOPE_CYCLE_COUNTER(STAT_OpenGLPresentTime);

	check(DrawingViewport.GetReference() == Viewport);

	FOpenGLTexture2D* BackBuffer = Viewport->GetBackBuffer();

	bool bNeedFinishFrame = PlatformBlitToViewport(PlatformDevice,
		*Viewport, 
		BackBuffer->GetSizeX(),
		BackBuffer->GetSizeY(),
		bPresent,
		bLockToVsync,
		RHIOpenGLConsoleVariables::SyncInterval
	);

	// Always consider the Framebuffer in the rendering context dirty after the blit
	RenderingContextState.Framebuffer = -1;

	DrawingViewport = NULL;

	// Don't wait on the GPU when using SLI, let the driver determine how many frames behind the GPU should be allowed to get
	if (GNumActiveGPUsForRendering == 1)
	{
		if (bNeedFinishFrame)
		{
			static const auto CFinishFrameVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.FinishCurrentFrame"));
			if (!CFinishFrameVar->GetValueOnRenderThread())
			{
				// Wait for the GPU to finish rendering the previous frame before finishing this frame.
				Viewport->WaitForFrameEventCompletion();
				Viewport->IssueFrameEvent();
			}
			else
			{
				// Finish current frame immediately to reduce latency
				Viewport->IssueFrameEvent();
				Viewport->WaitForFrameEventCompletion();
			}
		}
		
		// If the input latency timer has been triggered, block until the GPU is completely
		// finished displaying this frame and calculate the delta time.
		if ( GInputLatencyTimer.RenderThreadTrigger )
		{
			Viewport->WaitForFrameEventCompletion();
			uint32 EndTime = FPlatformTime::Cycles();
			GInputLatencyTimer.DeltaTime = EndTime - GInputLatencyTimer.StartTime;
			GInputLatencyTimer.RenderThreadTrigger = false;
		}
	}

	if (bRevertToSharedContextAfterDrawingViewport)
	{
		PlatformSharedContextSetup(PlatformDevice);
		bRevertToSharedContextAfterDrawingViewport = false;
	}
}

/**
 * Determine if currently drawing the viewport
 *
 * @return true if currently within a BeginDrawingViewport/EndDrawingViewport block
 */
bool FOpenGLDynamicRHI::RHIIsDrawingViewport()
{
	return DrawingViewport != NULL;
}

#endif

FTexture2DRHIRef FOpenGLDynamicRHI::RHIGetViewportBackBuffer(FViewportRHIParamRef ViewportRHI)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Viewport,Viewport);
	return Viewport->GetBackBuffer();
}

FOpenGLViewport::FOpenGLViewport(FOpenGLDynamicRHI* InOpenGLRHI,void* InWindowHandle,uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen)
	: OpenGLRHI(InOpenGLRHI)
	, OpenGLContext(NULL)
	, SizeX(0)
	, SizeY(0)
	, bIsFullscreen(false)
	, bIsValid(true)
	, FrameSyncEvent(InOpenGLRHI)
{
	check(OpenGLRHI);
    //@to-do spurious check for HTML5, will need to go away. 
#if !PLATFORM_HTML5
	check(InWindowHandle);
#endif 
	check(IsInGameThread());
	PlatformGlGetError();	// flush out old errors.
	OpenGLRHI->Viewports.Add(this);
	check(PlatformOpenGLCurrentContext(OpenGLRHI->PlatformDevice) == CONTEXT_Shared);
	OpenGLContext = PlatformCreateOpenGLContext(OpenGLRHI->PlatformDevice, InWindowHandle);
	Resize(InSizeX, InSizeY, bInIsFullscreen);
	check(PlatformOpenGLCurrentContext(OpenGLRHI->PlatformDevice) == CONTEXT_Shared);

	BeginInitResource(&FrameSyncEvent);
}

FOpenGLViewport::~FOpenGLViewport()
{
	check(IsInRenderingThread());

	if (bIsFullscreen)
	{
		PlatformRestoreDesktopDisplayMode();
	}

	FrameSyncEvent.ReleaseResource();

	// Release back buffer, before OpenGL context becomes invalid, making it impossible
	BackBuffer.SafeRelease();
	check(!IsValidRef(BackBuffer));

	PlatformDestroyOpenGLContext(OpenGLRHI->PlatformDevice,OpenGLContext);
	OpenGLContext = NULL;
	OpenGLRHI->Viewports.Remove(this);
}

void FOpenGLViewport::Resize(uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen)
{
	if ((InSizeX == SizeX) && (InSizeY == SizeY) && (bInIsFullscreen == bIsFullscreen))
	{
		return;
	}

	VERIFY_GL_SCOPE();

	if (IsValidRef(CustomPresent))
	{
		CustomPresent->OnBackBufferResize();
	}

	BackBuffer.SafeRelease();	// when the rest of the engine releases it, its framebuffers will be released too (those the engine knows about)

	BackBuffer = (FOpenGLTexture2D*)PlatformCreateBuiltinBackBuffer(OpenGLRHI, InSizeX, InSizeY);
	if (!BackBuffer)
	{
		BackBuffer = (FOpenGLTexture2D*)OpenGLRHI->CreateOpenGLTexture(InSizeX, InSizeY, false, false, PF_B8G8R8A8, 1, 1, 1, TexCreate_RenderTargetable);
	}

	PlatformResizeGLContext(OpenGLRHI->PlatformDevice, OpenGLContext, InSizeX, InSizeY, bInIsFullscreen, bIsFullscreen, BackBuffer->Target, BackBuffer->Resource);

	SizeX = InSizeX;
	SizeY = InSizeY;
	bIsFullscreen = bInIsFullscreen;
}

void* FOpenGLViewport::GetNativeWindow(void** AddParam) const
{
	return PlatformGetWindow(OpenGLContext, AddParam);
}

