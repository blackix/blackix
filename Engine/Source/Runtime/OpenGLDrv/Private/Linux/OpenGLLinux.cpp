// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLWindowsLoader.cpp: Manual loading of OpenGL functions from DLL.
=============================================================================*/

#include "OpenGLDrvPrivate.h"
#include "ds_extensions.h"

/*------------------------------------------------------------------------------
	OpenGL function pointers.
------------------------------------------------------------------------------*/
#define DEFINE_GL_ENTRYPOINTS(Type,Func) Type Func = NULL;
ENUM_GL_ENTRYPOINTS_ALL(DEFINE_GL_ENTRYPOINTS);


typedef SDL_Window*		SDL_HWindow;
typedef SDL_GLContext	SDL_HGLContext;

/*------------------------------------------------------------------------------
	OpenGL context management.
------------------------------------------------------------------------------*/
static void _ContextMakeCurrent( SDL_HWindow hWnd, SDL_HGLContext hGLDC )
{
	GLint Result = SDL_GL_MakeCurrent( hWnd, hGLDC );
	check( !Result );
}

static SDL_HGLContext _GetCurrentContext()
{
	return SDL_GL_GetCurrentContext();
}

/** Platform specific OpenGL context. */
struct FPlatformOpenGLContext
{
	SDL_HWindow		hWnd;
	SDL_HGLContext	hGLContext;		//	this is a (void*) pointer

	bool bReleaseWindowOnDestroy;
	int32 SyncInterval;
	GLuint	ViewportFramebuffer;
	GLuint	VertexArrayObject;	// one has to be generated and set for each context (OpenGL 3.2 Core requirements)
};


class FScopeContext
{
public:
	FScopeContext( FPlatformOpenGLContext* Context )
	{
		check(Context);
		hPreWnd = SDL_GL_GetCurrentWindow();
		hPreGLContext = SDL_GL_GetCurrentContext();

		bSameDCAndContext = ( hPreGLContext == Context->hGLContext );

		if	( !bSameDCAndContext )
		{
			if (hPreGLContext)
			{
				glFlush();
			}
			// no need to glFlush() on Windows, it does flush by itself before switching contexts

			_ContextMakeCurrent( Context->hWnd, Context->hGLContext );
		}
	}

   ~FScopeContext( void )
	{
		if (!bSameDCAndContext)
		{
			glFlush();	// not needed on Windows, it does flush by itself before switching contexts
			if	( hPreGLContext )
			{
				_ContextMakeCurrent( hPreWnd, hPreGLContext );
			}
			else
			{
				_ContextMakeCurrent( NULL, NULL );
			}
		}

	}

private:
	SDL_HWindow			hPreWnd;
	SDL_HGLContext		hPreGLContext;		//	this is a pointer, (void*)
	bool				bSameDCAndContext;
};



void _DeleteQueriesForCurrentContext( SDL_HGLContext hGLContext );

/**
 * Create a dummy window used to construct OpenGL contexts.
 */
static void _PlatformCreateDummyGLWindow( FPlatformOpenGLContext *OutContext )
{
	static bool bInitializedWindowClass = false;

	// Create a dummy window.
	SDL_HWindow h_wnd = SDL_CreateWindow(	NULL,
											0, 0, 1, 1,
											SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN );

	OutContext->hWnd					= h_wnd;
	OutContext->bReleaseWindowOnDestroy	= true;

	int CacheRet = DSEXT_CacheX11Info(h_wnd);
	if (EDSExtSuccess != CacheRet)
	{
		UE_LOG(LogRHI, Error, TEXT("Could not cache X11 info, DSExt error: %d"), CacheRet);
	}
}

/**
 * Determine OpenGL Context version based on command line arguments
 */

static bool _PlatformOpenGL4()
{
	return FParse::Param(FCommandLine::Get(),TEXT("opengl4"));
}

static void _PlatformOpenGLVersionFromCommandLine(int& OutMajorVersion, int& OutMinorVersion)
{
	if	( _PlatformOpenGL4() )
	{
		OutMajorVersion = 4;
		OutMinorVersion = 3;
	}
	else
	{
		OutMajorVersion = 3;
		OutMinorVersion = 2;
	}
}

/**
 * Enable/Disable debug context from the commandline
 */
static bool _PlatformOpenGLDebugCtx()
{
#if UE_BUILD_DEBUG
	return ! FParse::Param(FCommandLine::Get(),TEXT("openglNoDebug"));
#else
	return FParse::Param(FCommandLine::Get(),TEXT("openglDebug"));;
#endif
}

/**
 * Create a core profile OpenGL context.
 */
static void _PlatformCreateOpenGLContextCore( FPlatformOpenGLContext* OutContext)
{
	check( OutContext );

	SDL_HWindow prevWindow = SDL_GL_GetCurrentWindow();
	SDL_HGLContext prevContext = SDL_GL_GetCurrentContext();

	OutContext->SyncInterval = -1;	// invalid value to enforce setup on first buffer swap
	OutContext->ViewportFramebuffer = 0;

	OutContext->hGLContext = SDL_GL_CreateContext( OutContext->hWnd );

	SDL_GL_MakeCurrent( prevWindow, prevContext );
}

void PlatformReleaseOpenGLContext(FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context);

extern void OnQueryInvalidation( void );

/** Platform specific OpenGL device. */
struct FPlatformOpenGLDevice
{
    FPlatformOpenGLContext	SharedContext;
    FPlatformOpenGLContext	RenderingContext;
    int32					NumUsedContexts;

    /** Guards against operating on viewport contexts from more than one thread at the same time. */
    FCriticalSection*		ContextUsageGuard;

	FPlatformOpenGLDevice() : NumUsedContexts( 0 )
	{
		extern void InitDebugContext();

        ContextUsageGuard = new FCriticalSection;

		if	( SDL_GL_SetAttribute( SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0 ) )
		{
			verifyf( false, TEXT("OpenGLRHI: %s\n."), SDL_GetError() );
		}

		_PlatformCreateDummyGLWindow( &SharedContext );
		_PlatformCreateOpenGLContextCore( &SharedContext );

		check	( SharedContext.hGLContext )
		{
			FScopeContext ScopeContext( &SharedContext );
			InitDebugContext();
			glGenVertexArrays(1,&SharedContext.VertexArrayObject);
			glBindVertexArray(SharedContext.VertexArrayObject);
			InitDefaultGLContextState();
		}

		if	( SDL_GL_SetAttribute( SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1 ) )
		{
			verifyf( false, TEXT("OpenGLRHI: %s\n."), SDL_GetError() );
		}
		_ContextMakeCurrent( SharedContext.hWnd, SharedContext.hGLContext );

		_PlatformCreateDummyGLWindow( &RenderingContext );
		_PlatformCreateOpenGLContextCore( &RenderingContext );

		check	( RenderingContext.hGLContext )
		{
			FScopeContext ScopeContext( &RenderingContext );
			InitDebugContext();
			glGenVertexArrays(1,&RenderingContext.VertexArrayObject);
			glBindVertexArray(RenderingContext.VertexArrayObject);
			InitDefaultGLContextState();
		}
	}

   ~FPlatformOpenGLDevice()
	{
		check( NumUsedContexts==0 );

		_ContextMakeCurrent( NULL, NULL );

		OnQueryInvalidation();
		PlatformReleaseOpenGLContext( this,&RenderingContext );
        PlatformReleaseOpenGLContext( this,&SharedContext );

		delete ContextUsageGuard;
	}
};

FPlatformOpenGLDevice* PlatformCreateOpenGLDevice()
{
	return new FPlatformOpenGLDevice;
}

void PlatformDestroyOpenGLDevice( FPlatformOpenGLDevice* Device )
{
	delete Device;
}

/**
 * Create an OpenGL context.
 */
FPlatformOpenGLContext* PlatformCreateOpenGLContext(FPlatformOpenGLDevice* Device, void* InWindowHandle)
{
	check(InWindowHandle);
    FPlatformOpenGLContext* Context = new FPlatformOpenGLContext;

	Context->hWnd = (SDL_HWindow)InWindowHandle;
	Context->bReleaseWindowOnDestroy = false;

	check( Device->SharedContext.hGLContext )
	{
		FScopeContext Scope(&(Device->SharedContext));
		if	( SDL_GL_SetAttribute( SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1 ) )
		{
			verifyf( false, TEXT("OpenGLRHI: %s\n."), SDL_GetError() );
		}
		_PlatformCreateOpenGLContextCore( Context );
	}

	check( Context->hGLContext );
	{
		FScopeContext Scope(Context);
		InitDefaultGLContextState();
	}

    return Context;
}

/**
 * Release an OpenGL context.
 */
void PlatformReleaseOpenGLContext( FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context )
{
	check( Context && Context->hGLContext );

	{
		FScopeLock ScopeLock( Device->ContextUsageGuard );
		{
			FScopeContext ScopeContext( Context );

			_DeleteQueriesForCurrentContext( Context->hGLContext );
			glBindVertexArray(0);
			glDeleteVertexArrays(1, &Context->VertexArrayObject);

			if	( Context->ViewportFramebuffer )
			{
				glDeleteFramebuffers( 1,&Context->ViewportFramebuffer );	// this can be done from any context shared with ours, as long as it's not nil.
				Context->ViewportFramebuffer = 0;
			}
		}

		SDL_GL_DeleteContext( Context->hGLContext );
		Context->hGLContext = NULL;
	}

	check( Context->hWnd );

	if	( Context->bReleaseWindowOnDestroy )
	{
		SDL_DestroyWindow( Context->hWnd );
	}

	Context->hWnd = NULL;
}

/**
 * Destroy an OpenGL context.
 */
void PlatformDestroyOpenGLContext(FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context)
{
	PlatformReleaseOpenGLContext( Device, Context );
	delete Context;
}

void PlatformInitBridge(FOpenGLDynamicRHI* OpenGLRHI, FOpenGLBridge* OpenGLBridge)
{
}

void* PlatformGetWindow(FPlatformOpenGLContext* Context, void** AddParam)
{
	check(Context && Context->hWnd);

	if (AddParam)
	{
		*AddParam = &Context->hGLContext;
	}

	return (void*)&Context->hWnd;
}

/**
 * Main function for transferring data to on-screen buffers.
 * On Windows it temporarily switches OpenGL context, on Mac only context's output view.
 */
bool PlatformBlitToViewport(FPlatformOpenGLDevice* Device,
							const FOpenGLViewport& Viewport,
							uint32 BackbufferSizeX,
							uint32 BackbufferSizeY,
							bool bPresent,
							bool bLockToVsync,
							int32 SyncInterval )
{
	FPlatformOpenGLContext* const Context = Viewport.OpenGLContext;

	check( Context && Context->hWnd );

	int32 ret = 0;

	FScopeLock ScopeLock( Device->ContextUsageGuard );
	{
		FScopeContext ScopeContext( Context );

		glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );
		glDrawBuffer( GL_BACK );
		glBindFramebuffer( GL_READ_FRAMEBUFFER, Context->ViewportFramebuffer );
		glReadBuffer( GL_COLOR_ATTACHMENT0 );

		glBlitFramebuffer(	0, 0, BackbufferSizeX, BackbufferSizeY,
							0, BackbufferSizeY, BackbufferSizeX, 0,
							GL_COLOR_BUFFER_BIT,
							GL_NEAREST	);

		if ( bPresent )
		{
			int32 RealSyncInterval = bLockToVsync ? SyncInterval : 0;

			if ( Context->SyncInterval != RealSyncInterval)
			{
				//	0 for immediate updates
				//	1 for updates synchronized with the vertical retrace
				//	-1 for late swap tearing; 

				ret = SDL_GL_SetSwapInterval( RealSyncInterval );
				Context->SyncInterval = RealSyncInterval;
			}

			SDL_GL_SwapWindow( Context->hWnd );

			REPORT_GL_END_BUFFER_EVENT_FOR_FRAME_DUMP();
//			INITIATE_GL_FRAME_DUMP_EVERY_X_CALLS( 1000 );
		}
	}
	return true;
}

void PlatformFlushIfNeeded()
{
	glFinish();
}

void PlatformRebindResources(FPlatformOpenGLDevice* Device)
{
	// @todo: Figure out if we need to rebind frame & renderbuffers after switching contexts
}

void PlatformRenderingContextSetup(FPlatformOpenGLDevice* Device)
{
	check( Device && Device->RenderingContext.hWnd && Device->RenderingContext.hGLContext );

	if ( _GetCurrentContext() )
	{
		glFlush();
	}

	_ContextMakeCurrent( Device->RenderingContext.hWnd, Device->RenderingContext.hGLContext );
}

void PlatformSharedContextSetup(FPlatformOpenGLDevice* Device)
{
	check( Device && Device->SharedContext.hWnd && Device->SharedContext.hGLContext );

	// no need to glFlush() on Windows, it does flush by itself before switching contexts
	if ( _GetCurrentContext() )
	{
		glFlush();
	}

	_ContextMakeCurrent( Device->SharedContext.hWnd, Device->SharedContext.hGLContext );
}


void PlatformNULLContextSetup()
{
	if ( _GetCurrentContext() )
	{
		glFlush();
	}

	_ContextMakeCurrent( NULL, NULL );
}


#if 0
static void __GetBestFullscreenResolution( SDL_HWindow hWnd, int32 *pWidth, int32 *pHeight )
{
	uint32 InitializedMode = false;
	uint32 BestWidth = 0;
	uint32 BestHeight = 0;
	uint32 ModeIndex = 0;

	int32 dsp_idx = SDL_GetWindowDisplayIndex( hWnd );
	if ( dsp_idx < 0 )
	{	dsp_idx = 0;	}

	SDL_DisplayMode dsp_mode;
	FMemory::Memzero( &dsp_mode, sizeof(SDL_DisplayMode) );

	while ( !SDL_GetDisplayMode( dsp_idx, ModeIndex++, &dsp_mode ) )
	{
		bool IsEqualOrBetterWidth  = FMath::Abs((int32)dsp_mode.w - (int32)(*pWidth))  <= FMath::Abs((int32)BestWidth  - (int32)(*pWidth ));
		bool IsEqualOrBetterHeight = FMath::Abs((int32)dsp_mode.h - (int32)(*pHeight)) <= FMath::Abs((int32)BestHeight - (int32)(*pHeight));
		if	(!InitializedMode || (IsEqualOrBetterWidth && IsEqualOrBetterHeight))
		{
			BestWidth = dsp_mode.w;
			BestHeight = dsp_mode.h;
			InitializedMode = true;
		}
	}

	check(InitializedMode);

	*pWidth  = BestWidth;
	*pHeight = BestHeight;
}


static void __SetBestFullscreenDisplayMode( SDL_HWindow hWnd, int32 *pWidth, int32 *pHeight )
{
	SDL_DisplayMode dsp_mode;

	dsp_mode.w = *pWidth;
	dsp_mode.h = *pHeight;
	dsp_mode.format = SDL_PIXELFORMAT_ARGB8888;
	dsp_mode.refresh_rate = 60;
	dsp_mode.driverdata = NULL;

	__GetBestFullscreenResolution( hWnd, &dsp_mode.w, &dsp_mode.h );
	SDL_SetWindowDisplayMode( hWnd, &dsp_mode );

	*pWidth  = dsp_mode.w;
	*pHeight = dsp_mode.h;
}
#endif


/**
 * Resize the GL context.
 */
static int gResizeC = 0;

void PlatformResizeGLContext(	FPlatformOpenGLDevice* Device,
								FPlatformOpenGLContext* Context,
								uint32 SizeX, uint32 SizeY,
								bool bFullscreen, 
								bool bWasFullscreen,
								GLenum BackBufferTarget,
								GLuint BackBufferResource)
{
//	printf( "--------------------------------------------------------\n" );
//	printf("Ariel - PlatformResizeGLContext - Start - %03d\n", gResizeC );

	{
		FScopeLock ScopeLock(Device->ContextUsageGuard);

#if 0
		SDL_HWindow h_wnd = Context->hWnd;

		int32 closest_w = SizeX;
		int32 closest_h = SizeY;

		if	( bFullscreen )
		{

			SDL_SetWindowPosition( h_wnd, 0, 0 );
			__SetBestFullscreenDisplayMode( h_wnd, &closest_w, &closest_h );

			SDL_SetWindowBordered( h_wnd, SDL_FALSE );
			SDL_SetWindowFullscreen( h_wnd, SDL_WINDOW_FULLSCREEN );

		}
		else 
		if	( bWasFullscreen )
		{
			SDL_SetWindowFullscreen( h_wnd, 0 );
			SDL_SetWindowBordered( h_wnd, SDL_TRUE );
			SDL_SetWindowSize( h_wnd, SizeX, SizeY );
		}
#endif

		{
			FScopeContext ScopeContext(Context);

			if (Context->ViewportFramebuffer == 0)
			{
				glGenFramebuffers(1, &Context->ViewportFramebuffer);
			}
			glBindFramebuffer(GL_FRAMEBUFFER, Context->ViewportFramebuffer);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, BackBufferTarget, BackBufferResource, 0);
#if UE_BUILD_DEBUG
			glReadBuffer(GL_COLOR_ATTACHMENT0);
			glDrawBuffer(GL_COLOR_ATTACHMENT0);
#endif
			FOpenGL::CheckFrameBuffer();

			glViewport( 0, 0, SizeX, SizeY );			
		//	printf( "      - NewVPW = %d, NewVPH = %d\n", SizeX, SizeY );

			static GLfloat ZeroColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			glClearBufferfv(GL_COLOR, 0, ZeroColor );
		}


		//	bool b_need_reattach = bFullscreen || bWasFullscreen;
		if	( bFullscreen )
		{
			//	Deregister all components
			//	this line will fix the geometry missing and color distortion bug on Linux/Nvidia machine
			//	it detach all the components and re attach all the components
			FGlobalComponentReregisterContext RecreateComponents;
		}
		else
		if	( bWasFullscreen )
		{
			FGlobalComponentReregisterContext RecreateComponents;
		}
	}

//	printf("Ariel - PlatformResizeGLContext - End - %03d\n", gResizeC );
	gResizeC++;
}

//		int dip_num = SDL_GetNumVideoDisplays();
void PlatformGetSupportedResolution(uint32 &Width, uint32 &Height)
{
	uint32 InitializedMode = false;
	uint32 BestWidth = 0;
	uint32 BestHeight = 0;
	uint32 ModeIndex = 0;

	SDL_DisplayMode dsp_mode;
	FMemory::Memzero( &dsp_mode, sizeof(SDL_DisplayMode) );

	while ( !SDL_GetDisplayMode( 0, ModeIndex++, &dsp_mode ) )
	{
		bool IsEqualOrBetterWidth = FMath::Abs((int32)dsp_mode.w - (int32)Width) <= FMath::Abs((int32)BestWidth - (int32)Width);
		bool IsEqualOrBetterHeight = FMath::Abs((int32)dsp_mode.h - (int32)Height) <= FMath::Abs((int32)BestHeight - (int32)Height);
		if	(!InitializedMode || (IsEqualOrBetterWidth && IsEqualOrBetterHeight))
		{
			BestWidth = dsp_mode.w;
			BestHeight = dsp_mode.h;
			InitializedMode = true;
		}
	}
	check(InitializedMode);
	Width = BestWidth;
	Height = BestHeight;
}


bool PlatformGetAvailableResolutions( FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate )
{
	int32 MinAllowableResolutionX = 0;
	int32 MinAllowableResolutionY = 0;
	int32 MaxAllowableResolutionX = 10480;
	int32 MaxAllowableResolutionY = 10480;
	int32 MinAllowableRefreshRate = 0;
	int32 MaxAllowableRefreshRate = 10480;

	if (MaxAllowableResolutionX == 0)
	{
		MaxAllowableResolutionX = 10480;
	}
	if (MaxAllowableResolutionY == 0)
	{
		MaxAllowableResolutionY = 10480;
	}
	if (MaxAllowableRefreshRate == 0)
	{
		MaxAllowableRefreshRate = 10480;
	}

	uint32 ModeIndex = 0;
	SDL_DisplayMode DisplayMode;
	FMemory::Memzero(&DisplayMode, sizeof(SDL_DisplayMode));

	while ( !SDL_GetDisplayMode( 0, ModeIndex++, &DisplayMode ) )
	{
		if (((int32)DisplayMode.w >= MinAllowableResolutionX) &&
			((int32)DisplayMode.w <= MaxAllowableResolutionX) &&
			((int32)DisplayMode.h >= MinAllowableResolutionY) &&
			((int32)DisplayMode.h <= MaxAllowableResolutionY)
			)
		{
			bool bAddIt = true;
			if (bIgnoreRefreshRate == false)
			{
				if (((int32)DisplayMode.refresh_rate < MinAllowableRefreshRate) ||
					((int32)DisplayMode.refresh_rate > MaxAllowableRefreshRate)
					)
				{
					continue;
				}
			}
			else
			{
				// See if it is in the list already
				for (int32 CheckIndex = 0; CheckIndex < Resolutions.Num(); CheckIndex++)
				{
					FScreenResolutionRHI& CheckResolution = Resolutions[CheckIndex];
					if ((CheckResolution.Width == DisplayMode.w) &&
						(CheckResolution.Height == DisplayMode.h))
					{
						// Already in the list...
						bAddIt = false;
						break;
					}
				}
			}

			if (bAddIt)
			{
				// Add the mode to the list
				int32 Temp2Index = Resolutions.AddZeroed();
				FScreenResolutionRHI& ScreenResolution = Resolutions[Temp2Index];

				ScreenResolution.Width = DisplayMode.w;
				ScreenResolution.Height = DisplayMode.h;
				ScreenResolution.RefreshRate = DisplayMode.refresh_rate;
			}
		}
	}

	return true;
}

void PlatformRestoreDesktopDisplayMode()
{
//	printf( "ArielPlatform - RestoreDesktopDisplayMode\n" );
}


bool PlatformInitOpenGL()
{
	static bool bInitialized = false;
	static bool bOpenGLSupported = false;

	//	init the sdl here
	if	( SDL_WasInit( 0 ) == 0 )
	{
		SDL_Init( SDL_INIT_VIDEO );
	}
	else
	{
		Uint32 subsystem_init = SDL_WasInit( SDL_INIT_EVERYTHING );

		if	( !(subsystem_init & SDL_INIT_VIDEO) )
		{
			SDL_InitSubSystem( SDL_INIT_VIDEO );
		}
	}

	if (!bInitialized)
	{
		int MajorVersion = 0;
		int MinorVersion = 0;

		int DebugFlag = 0;

		if ( _PlatformOpenGLDebugCtx() )
		{
			DebugFlag = SDL_GL_CONTEXT_DEBUG_FLAG;
		}
	
		_PlatformOpenGLVersionFromCommandLine(MajorVersion, MinorVersion);
		if	( SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, MajorVersion ) )
		{
			verifyf( false, TEXT("OpenGLRHI: %s\n."), SDL_GetError() );
		}

		if	( SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, MinorVersion ) )
		{
			verifyf( false, TEXT("OpenGLRHI: %s\n."), SDL_GetError() );
		}

		if	( SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG | DebugFlag ) )
		{
			verifyf( false, TEXT("OpenGLRHI: %s\n."), SDL_GetError() );
		}

		if	( SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE ) )
		{
			verifyf( false, TEXT("OpenGLRHI: %s\n."), SDL_GetError() );
		}

		// Create a dummy context to verify opengl support.
		FPlatformOpenGLContext DummyContext;
		_PlatformCreateDummyGLWindow(&DummyContext);
		_PlatformCreateOpenGLContextCore(&DummyContext );	

		if (DummyContext.hGLContext)
		{
			bOpenGLSupported = true;
			_ContextMakeCurrent(DummyContext.hWnd, DummyContext.hGLContext);
		}
		else
		{
			UE_LOG(LogRHI,Error,TEXT("OpenGL %d.%d not supported by driver"),MajorVersion,MinorVersion);
		}

		if (bOpenGLSupported)
		{
			// Initialize all entry points required by Unreal.
			#define GET_GL_ENTRYPOINTS(Type,Func) Func = (Type)SDL_GL_GetProcAddress(#Func);
			ENUM_GL_ENTRYPOINTS(GET_GL_ENTRYPOINTS);
			ENUM_GL_ENTRYPOINTS_OPTIONAL(GET_GL_ENTRYPOINTS);
		
			// Check that all of the entry points have been initialized.
			bool bFoundAllEntryPoints = true;
			#define CHECK_GL_ENTRYPOINTS(Type,Func) if (Func == NULL) { bFoundAllEntryPoints = false; UE_LOG(LogRHI, Warning, TEXT("Failed to find entry point for %s"), TEXT(#Func)); }
			ENUM_GL_ENTRYPOINTS(CHECK_GL_ENTRYPOINTS);
			checkf(bFoundAllEntryPoints, TEXT("Failed to find all OpenGL entry points."));
		}

		// The dummy context can now be released.
		if (DummyContext.hGLContext)
		{
			_ContextMakeCurrent(NULL,NULL);
			SDL_GL_DeleteContext(DummyContext.hGLContext);
		}
		check(DummyContext.bReleaseWindowOnDestroy);
		SDL_DestroyWindow(DummyContext.hWnd);

		bInitialized = true;
	}

	return bOpenGLSupported;
}

bool PlatformOpenGLContextValid()
{
	return ( _GetCurrentContext() != NULL );
}

int32 PlatformGlGetError()
{
	return glGetError();
}

EOpenGLCurrentContext PlatformOpenGLCurrentContext( FPlatformOpenGLDevice* Device )
{
	SDL_HGLContext hGLContext = _GetCurrentContext();

	if	( hGLContext == Device->RenderingContext.hGLContext )	// most common case
	{
		return CONTEXT_Rendering;
	}
	else if	( hGLContext == Device->SharedContext.hGLContext )
	{
		return CONTEXT_Shared;
	}
	else if	( hGLContext )
	{
		return CONTEXT_Other;
	}
	return CONTEXT_Invalid;
}


void PlatformGetBackbufferDimensions( uint32& OutWidth, uint32& OutHeight )
{
	SDL_HWindow h_cwnd = SDL_GL_GetCurrentWindow();

	if	( h_cwnd )
	{
		SDL_Surface *p_surf = SDL_GetWindowSurface( h_cwnd );

		if	( p_surf )
		{
			OutWidth  = p_surf->w;
			OutHeight = p_surf->h;
		}
	}
}

// =============================================================

struct FOpenGLReleasedQuery
{
	SDL_HGLContext	hGLContext;
	GLuint			Query;
};

static TArray<FOpenGLReleasedQuery>	ReleasedQueries;
static FCriticalSection*			ReleasedQueriesGuard;

void PlatformGetNewRenderQuery( GLuint* OutQuery, uint64* OutQueryContext )
{
	if( !ReleasedQueriesGuard )
	{
		ReleasedQueriesGuard = new FCriticalSection;
	}

	{
		FScopeLock Lock(ReleasedQueriesGuard);

#ifdef UE_BUILD_DEBUG
		check( OutQuery && OutQueryContext );
#endif

		SDL_HGLContext hGLContext = _GetCurrentContext();
		check( hGLContext );

		GLuint NewQuery = 0;

		// Check for possible query reuse
		const int32 ArraySize = ReleasedQueries.Num();
		for( int32 Index = 0; Index < ArraySize; ++Index )
		{
			if( ReleasedQueries[Index].hGLContext == hGLContext )
			{
				NewQuery = ReleasedQueries[Index].Query;
				ReleasedQueries.RemoveAtSwap(Index);
				break;
			}
		}

		if( !NewQuery )
		{
			FOpenGL::GenQueries( 1, &NewQuery );
		}

		*OutQuery = NewQuery;
		*OutQueryContext = (uint64)hGLContext;

	}

}



void PlatformReleaseRenderQuery( GLuint Query, uint64 QueryContext )
{
	SDL_HGLContext hGLContext = _GetCurrentContext();
	if( (uint64)hGLContext == QueryContext )
	{
		FOpenGL::DeleteQueries(1, &Query );
	}
	else
	{
		FScopeLock Lock(ReleasedQueriesGuard);
#ifdef UE_BUILD_DEBUG
		check( Query && QueryContext && ReleasedQueriesGuard );
#endif
		FOpenGLReleasedQuery ReleasedQuery;
		ReleasedQuery.hGLContext = (SDL_HGLContext)QueryContext;
		ReleasedQuery.Query = Query;
		ReleasedQueries.Add(ReleasedQuery);
	}

}

bool PlatformContextIsCurrent( uint64 QueryContext )
{
	return (uint64)_GetCurrentContext() == QueryContext;
}

FRHITexture* PlatformCreateBuiltinBackBuffer( FOpenGLDynamicRHI* OpenGLRHI, uint32 SizeX, uint32 SizeY )
{
	return NULL;
}

void _DeleteQueriesForCurrentContext( SDL_HGLContext hGLContext )
{
	if( !ReleasedQueriesGuard )
	{
		ReleasedQueriesGuard = new FCriticalSection;
	}

	{
		FScopeLock Lock(ReleasedQueriesGuard);
		for( int32 Index = 0; Index < ReleasedQueries.Num(); ++Index )
		{
			if( ReleasedQueries[Index].hGLContext == hGLContext )
			{
				FOpenGL::DeleteQueries(1,&ReleasedQueries[Index].Query);
				ReleasedQueries.RemoveAtSwap(Index);
				--Index;
			}
		}
	}

}

void FLinuxOpenGL::ProcessExtensions( const FString& ExtensionsString )
{
	FOpenGL4::ProcessExtensions(ExtensionsString);

	FString VendorName( ANSI_TO_TCHAR((const ANSICHAR*)glGetString(GL_VENDOR) ) );

	if ( VendorName.Contains(TEXT("ATI ")) )
	{
		// Workaround for AMD driver not handling GL_SRGB8_ALPHA8 in glTexStorage2D() properly (gets treated as non-sRGB)
		glTexStorage1D = NULL;
		glTexStorage2D = NULL;
		glTexStorage3D = NULL;

		FOpenGLBase::bSupportsCopyImage  = false;
	}
}



