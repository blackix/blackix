// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnrealEngine.cpp: Implements the UEngine class and helpers.
=============================================================================*/

#include "EnginePrivate.h"

#include "Net/UnrealNetwork.h"
#include "Engine/Console.h"
#include "VisualLog.h"
#include "FileManagerGeneric.h"
#include "Database.h"
#include "SkeletalMeshMerge.h"
#include "Slate.h"
#include "RenderCore.h"
#include "ShaderCompiler.h"
#include "ColorList.h"
#include "AVIWriter.h"
#include "Slate/SlateSoundDevice.h"
#include "DerivedDataCacheInterface.h"
#include "Networking.h"
#include "ProfilingHelpers.h"
#include "ImageWrapper.h"
#include "OnlineSubsystem.h"
#include "OnlineExternalUIInterface.h"
#include "EngineAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "CrashTracker.h"
#include "TickTaskManagerInterface.h"
#include "TargetPlatform.h"
#include "AudioEffect.h"
#include "Net/NetworkProfiler.h"
#include "MallocProfiler.h"
#include "../../Launch/Resources/Version.h"
#include "StereoRendering.h"
#include "IHeadMountedDisplayModule.h"
#include "IHeadMountedDisplay.h"
#include "Scalability.h"
#include "StatsData.h"
#include "ScreenRendering.h"
#include "RHIStaticStates.h"
#include "AudioDevice.h"
#include "ActiveSound.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "Animation/SkeletalMeshActor.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Character.h"
#include "Engine/LevelStreamingVolume.h"
#include "Vehicles/TireType.h"

#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/TypeData/ParticleModuleTypeDataMesh.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleSpriteEmitter.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"

#include "Sound/ReverbEffect.h"
#include "Sound/SoundWave.h"

// @todo this is here only due to circular dependency to AIModule. To be removed
#include "BehaviorTree/BehaviorTreeManager.h"
#include "EnvironmentQuery/EnvQueryManager.h"

#if !UE_BUILD_SHIPPING
#include "STaskGraph.h"
#endif
#if WITH_EDITORONLY_DATA
#include "ObjectEditorUtils.h"
#endif

#include "HardwareInfo.h"
#include "EngineModule.h"
#include "UnrealExporter.h"
#include "ComponentReregisterContext.h"
#include "ContentStreaming.h"

DEFINE_LOG_CATEGORY_STATIC(LogEngine, Log, All);

IMPLEMENT_MODULE( FEngineModule, Engine );

#define LOCTEXT_NAMESPACE "UnrealEngine"

void FEngineModule::StartupModule()
{
	// Setup delegate callback for ProfilingHelpers to access current map name
	extern const FString GetMapNameStatic();
	GGetMapNameDelegate.BindStatic(&GetMapNameStatic);
}


/* Global variables
 *****************************************************************************/

/**
 * Global engine pointer. Can be 0 so don't use without checking.
 */
ENGINE_API UEngine*	GEngine = NULL;

/**
 * Whether to visualize the light map selected by the Debug Camera.
 */
ENGINE_API bool GShowDebugSelectedLightmap = false;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/**
	 * true if we debug material names with SCOPED_DRAW_EVENT.
	 * Toggle with "ShowMaterialDrawEvents" console command.
	 */
	bool GShowMaterialDrawEvents = false;
#endif

ENGINE_API uint32 GGPUFrameTime = 0;

/** System resolution instance */
FSystemResolution GSystemResolution;

/** Threshold for a frame to be considered a hitch (in seconds. */
static TAutoConsoleVariable<float> GHitchThresholdCVar(
	TEXT("t.HitchThreshold"),
	GHitchThreshold,
	TEXT("Time in seconds that is considered a hitch by \"stat dumphitches\"")
	);

static TAutoConsoleVariable<int32> CVarAllowOneFrameThreadLag(
	TEXT("r.OneFrameThreadLag"),
	1,
	TEXT("Whether to allow the rendering thread to lag one frame behind the game thread (0: disabled, otherwise enabled)")
	);

static FAutoConsoleVariable CVarSystemResolution(
	TEXT("r.SetRes"),
	TEXT("1280x720w"),
	TEXT("Set the display resolution for the current game view. Has no effect in the editor.")
	TEXT("  Format e.g. 1280x720w")
	TEXT("  	   e.g. 1920x1080f")
	);

static TAutoConsoleVariable<float> CVarDepthOfFieldNearBlurSizeThreshold(
	TEXT("r.DepthOfFieldNearBlurSizeThreshold"),
	0.01f,
	TEXT("Sets the minimum near blur size before the effect is forcably disabled. Currently only affects Gaussian DOF.\n")
	TEXT(" (default = 0.01f)"),
	ECVF_RenderThreadSafe);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static TAutoConsoleVariable<float> CVarSetOverrideFPS(
	TEXT("t.OverrideFPS"),
	0.0f,
	TEXT("This allows to override the frame time measurement with a fixed fps number (game can run faster or slower).\n")
	TEXT("<=0:off, in frames per second, e.g. 60"),
	ECVF_Cheat);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

/** Enum entries represent index to global object referencer stored in UGameEngine */
enum EGametypeContentReferencerTypes
{
	GametypeCommon_ReferencerIndex,
	GametypeCommon_LocalizedReferencerIndex,
	GametypeContent_ReferencerIndex,
	GametypeContent_LocalizedReferencerIndex,
	MAX_ReferencerIndex
};

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** 
	 *	A global to allow turning off the 'NOT RUNNING IN HD' warning.
	 *	Is enabled by default - and is *not* stored in an ini file 
	 *	so it will always show up when you launch in non-HD mode.
	 *
	 *	Disable via the console command "TOGGLEHDWARNING"
	 */
	bool GbWarn_NotRunningInHD = true;
#endif

/** Whether texture memory has been corrupted because we ran out of memory in the pool. */
bool GIsTextureMemoryCorrupted = false;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** Whether PrepareMapChange is attempting to load a map that doesn't exist */
	bool GIsPrepareMapChangeBroken = false;
#endif

// We expose these variables to everyone as we need to access them in other files via an extern
ENGINE_API float GAverageFPS = 0.0f;
ENGINE_API float GAverageMS = 0.0f;
ENGINE_API double GLastMemoryWarningTime = 0.f;

static FCachedSystemScalabilityCVars GCachedScalabilityCVars;

const FCachedSystemScalabilityCVars& GetCachedScalabilityCVars()
{
	return GCachedScalabilityCVars;
}

FCachedSystemScalabilityCVars::FCachedSystemScalabilityCVars()
	: DetailMode(-1)
	, MaterialQualityLevel(EMaterialQualityLevel::Num)
	, MaxAnisotropy(-1)
	, MaxShadowResolution(-1)
	, ViewDistanceScale(-1)
	, ViewDistanceScaleSquared(-1)
	, GaussianDOFNearThreshold(-1)
{

}

void ScalabilityCVarsSinkCallback()
{
	IConsoleManager& ConsoleMan = IConsoleManager::Get();

	static const auto DetailMode = ConsoleMan.FindTConsoleVariableDataInt(TEXT("r.DetailMode"));
	if( GCachedScalabilityCVars.DetailMode != DetailMode->GetValueOnGameThread() )
	{
		TArray<UClass*> ExcludeComponents;
		ExcludeComponents.Add(UAudioComponent::StaticClass());

		FGlobalComponentReregisterContext PropagateDetailModeChanges(ExcludeComponents);
		GCachedScalabilityCVars.DetailMode = DetailMode->GetValueOnGameThread();
	}

	static const auto* MaxAnisotropy = ConsoleMan.FindTConsoleVariableDataInt(TEXT("r.MaxAnisotropy"));
	static const auto* MaxShadowResolution = ConsoleMan.FindTConsoleVariableDataInt(TEXT("r.Shadow.MaxResolution"));
	static const auto ViewDistanceScale = ConsoleMan.FindTConsoleVariableDataFloat(TEXT("r.ViewDistanceScale"));
	GCachedScalabilityCVars.MaxAnisotropy = MaxAnisotropy->GetValueOnGameThread();
	GCachedScalabilityCVars.MaxShadowResolution = MaxShadowResolution->GetValueOnGameThread();
	GCachedScalabilityCVars.ViewDistanceScale = FMath::Clamp(ViewDistanceScale->GetValueOnGameThread(), 0.0f, 1.0f);
	GCachedScalabilityCVars.ViewDistanceScaleSquared = FMath::Square(GCachedScalabilityCVars.ViewDistanceScale);
	GCachedScalabilityCVars.GaussianDOFNearThreshold = CVarDepthOfFieldNearBlurSizeThreshold.GetValueOnGameThread();

	// action needed if we change r.MaterialQualityLevel at runtime
	{
		static const auto MaterialQualityLevelVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MaterialQualityLevel"));

		EMaterialQualityLevel::Type NewMaterialQualityLevel = (EMaterialQualityLevel::Type)FMath::Clamp(MaterialQualityLevelVar->GetValueOnGameThread(), 0, 1);

		// has the state changed ?
		if(GCachedScalabilityCVars.MaterialQualityLevel != NewMaterialQualityLevel)
		{
			// we had a state before?
			if(GCachedScalabilityCVars.MaterialQualityLevel != EMaterialQualityLevel::Type::Num)
			{
				// state has changed, some action is needed

				// Deregister all components
				FGlobalComponentReregisterContext RecreateComponents;

				// after FGlobalComponentReregisterContext to have the renderthread flushed before so it can use the variable on either thread
				GCachedScalabilityCVars.MaterialQualityLevel = NewMaterialQualityLevel;

				// For all materials, UMaterial::CacheResourceShadersForRendering
				UMaterial::AllMaterialsCacheResourceShadersForRendering();
				UMaterialInstance::AllMaterialsCacheResourceShadersForRendering();

				// destructor of RecreateComponents will register the components again
			}
			else
			{
				GCachedScalabilityCVars.MaterialQualityLevel = NewMaterialQualityLevel;
			}
		}
	}

	// action needed if we change r.SimpleDynamicLighting at runtime
	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SimpleDynamicLighting"));

		// 0:off, 1:on, -1:unknown
		static int32 CurrentSDL = -1;

		int32 NewSDL = FMath::Clamp(CVar->GetInt(), 0, 1);

		// has the state changed ?
		if(CurrentSDL != NewSDL)
		{
			// we had a state before?
			if(CurrentSDL != -1)
			{
				CurrentSDL = NewSDL;

				// state has changed, some action is needed

				// Deregister all components
				FGlobalComponentReregisterContext RecreateComponents;

				// destructor of RecreateComponents will register the components again
			}
			else
			{
				CurrentSDL = NewSDL;
			}
		}
	}
}

void SystemResolutionSinkCallback()
{
	auto ResString = CVarSystemResolution->GetString();
	
	uint32 ResX, ResY;
	int32 WindowModeInt = GSystemResolution.WindowMode;
	
	if (FParse::Resolution(*ResString, ResX, ResY, WindowModeInt))
	{
		EWindowMode::Type WindowMode = EWindowMode::ConvertIntToWindowMode(WindowModeInt);

		// TODO: This isn't correct, as we also need to compare the required fullscreen mode to the existing one.
		if( GSystemResolution.ResX != ResX ||
			GSystemResolution.ResY != ResY ||
			GSystemResolution.WindowMode != WindowMode)
		{
			GSystemResolution.ResX = ResX;
			GSystemResolution.ResY = ResY;
			GSystemResolution.WindowMode = WindowMode;

			if(GEngine && GEngine->GameViewport && GEngine->GameViewport->ViewportFrame)
			{
				GEngine->GameViewport->ViewportFrame->ResizeFrame(ResX, ResY, WindowMode);
			}
		}
	}
}

/*
 * if we need to update the sample states
*/
void RefreshSamplerStatesCallback()
{
	if (FApp::CanEverRender() == false)
	{
		// Avoid unnecessary work when running in dedicated server mode.
		return;
	}

	bool bRefreshSamplerStates = false;

	{
		float MipMapBiasOffset = UTexture2D::GetGlobalMipMapLODBias();
		static float LastMipMapLODBias = 0;

		if(LastMipMapLODBias != MipMapBiasOffset)
		{
			LastMipMapLODBias = MipMapBiasOffset;
			bRefreshSamplerStates = true;
		}
	}

	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MaxAnisotropy"));
		int32 MaxAnisotropy = CVar->GetValueOnGameThread();
		// compare against the default so with that number we avoid RefreshSamplerStates() calls on startup
		static int32 LastMaxAnisotropy = 4;

		if(LastMaxAnisotropy != MaxAnisotropy)
		{
			LastMaxAnisotropy = MaxAnisotropy;
			bRefreshSamplerStates = true;
		}
	}

	if(bRefreshSamplerStates)
	{
		for (TObjectIterator<UTexture2D>It; It; ++It)
		{
			UTexture2D* Texture = *It;
			Texture->RefreshSamplerStates();
		}
		UMaterialInterface::RecacheAllMaterialUniformExpressions();
	}
}

ENGINE_API void InitializeRenderingCVarsCaching()
{
	extern void FreeSkeletalMeshBuffersSinkCallback();
	IConsoleManager::Get().RegisterConsoleVariableSink(FConsoleCommandDelegate::CreateStatic(&RefreshSamplerStatesCallback));
	IConsoleManager::Get().RegisterConsoleVariableSink(FConsoleCommandDelegate::CreateStatic(&ScalabilityCVarsSinkCallback));
	IConsoleManager::Get().RegisterConsoleVariableSink(FConsoleCommandDelegate::CreateStatic(&FreeSkeletalMeshBuffersSinkCallback));
	IConsoleManager::Get().RegisterConsoleVariableSink(FConsoleCommandDelegate::CreateStatic(&SystemResolutionSinkCallback));

	// Initialise this to invalid
	GCachedScalabilityCVars.MaterialQualityLevel = EMaterialQualityLevel::Num;

	// Initial cache
	SystemResolutionSinkCallback();
	ScalabilityCVarsSinkCallback();
}

void ShutdownRenderingCVarsCaching()
{
	extern void FreeSkeletalMeshBuffersSinkCallback();
	IConsoleManager::Get().UnregisterConsoleVariableSink(FConsoleCommandDelegate::CreateStatic(&RefreshSamplerStatesCallback));
	IConsoleManager::Get().UnregisterConsoleVariableSink(FConsoleCommandDelegate::CreateStatic(&ScalabilityCVarsSinkCallback));
	IConsoleManager::Get().UnregisterConsoleVariableSink(FConsoleCommandDelegate::CreateStatic(&FreeSkeletalMeshBuffersSinkCallback));
	IConsoleManager::Get().UnregisterConsoleVariableSink(FConsoleCommandDelegate::CreateStatic(&SystemResolutionSinkCallback));
}

namespace
{
	/**
	 * Attempts to set process limits as configured in Engine.ini or elsewhere.
	 * Assumed to be called during initialization.
	 */
	void SetConfiguredProcessLimits()
	{
		int32 VirtualMemoryLimitInKB = 0;
		if (GConfig)
		{
			GConfig->GetInt(TEXT("ProcessLimits"), TEXT("VirtualMemoryLimitInKB"), VirtualMemoryLimitInKB, GEngineIni);
		}
		
		// command line parameters take precendence
		FParse::Value(FCommandLine::Get(), TEXT("virtmemkb="), VirtualMemoryLimitInKB);

		if (VirtualMemoryLimitInKB > 0)
		{
			UE_LOG(LogInit, Display, TEXT("Limiting process virtual memory size to %d KB"), VirtualMemoryLimitInKB);
			if (!FPlatformProcess::SetProcessLimits(EProcessResource::VirtualMemory, static_cast< uint64 >(VirtualMemoryLimitInKB) * 1024))
			{
				UE_LOG(LogInit, Fatal, TEXT("Could not limit process virtual memory usage to %d KB"), VirtualMemoryLimitInKB);
			}
		}
	}
}
/*-----------------------------------------------------------------------------
	Object class implementation.
-----------------------------------------------------------------------------*/

/**
 * Compresses and decompresses thumbnails using the PNG format.  This is used by the package loading and
 * saving process.
 */
class FPNGThumbnailCompressor
	: public FThumbnailCompressionInterface
{

public:

	/**
	 * Compresses an image
	 *
	 * @param	InUncompressedData	The uncompressed image data
	 * @param	InWidth				Width of the image
	 * @param	InHeight			Height of the image
	 * @param	OutCompressedData	[Out] Compressed image data
	 *
	 * @return	true if the image was compressed successfully, otherwise false if an error occurred
	 */
	virtual bool CompressImage( const TArray< uint8 >& InUncompressedData, const int32 InWidth, const int32 InHeight, TArray< uint8 >& OutCompressedData )
	{
		bool bSucceeded = false;
		OutCompressedData.Reset();
		if( InUncompressedData.Num() > 0 )
		{
			IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );
			IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper( EImageFormat::PNG );
			if ( ImageWrapper.IsValid() && ImageWrapper->SetRaw( &InUncompressedData[ 0 ], InUncompressedData.Num(), InWidth, InHeight, ERGBFormat::RGBA, 8 ) )
			{
				OutCompressedData = ImageWrapper->GetCompressed();
				bSucceeded = true;
			}
		}

		return bSucceeded;
	}


	/**
	 * Decompresses an image
	 *
	 * @param	InCompressedData	The compressed image data
	 * @param	InWidth				Width of the image
	 * @param	InHeight			Height of the image
	 * @param	OutUncompressedData	[Out] Uncompressed image data
	 *
	 * @return	true if the image was decompressed successfully, otherwise false if an error occurred
	 */
	virtual bool DecompressImage( const TArray< uint8 >& InCompressedData, const int32 InWidth, const int32 InHeight, TArray< uint8 >& OutUncompressedData )
	{
		bool bSucceeded = false;
		OutUncompressedData.Reset();
		if( InCompressedData.Num() > 0 )
		{
			IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );
			IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper( EImageFormat::PNG );
			if ( ImageWrapper.IsValid() && ImageWrapper->SetCompressed( &InCompressedData[ 0 ], InCompressedData.Num() ) )
			{
				check( ImageWrapper->GetWidth() == InWidth );
				check( ImageWrapper->GetHeight() == InHeight );
				const TArray<uint8>* RawData = NULL;
				if ( ImageWrapper->GetRaw( ERGBFormat::RGBA, 8, RawData ) )	// @todo CB: Eliminate image copy here? (decompress straight to buffer)
				{
					OutUncompressedData = *RawData;
					bSucceeded = true;
				}
			}
		}

		return bSucceeded;
	}


};


/**
 * Helper class inhibiting screen saver by e.g. moving the mouse by 0 pixels every 50 seconds.
 */
class FScreenSaverInhibitor : public FRunnable
{
	// FRunnable interface. Not required to be implemented.
	bool Init() { return true; }
	void Stop() {}
	void Exit() {}

	/**
	 * Prevents screensaver from kicking in by calling FPlatformMisc::PreventScreenSaver every 50 seconds.
	 * 
	 * @return	never returns
	 */
	uint32 Run()
	{
		while( true )
		{
			FPlatformProcess::Sleep( 50 );
			FPlatformMisc::PreventScreenSaver();
		}
		return 0;
	}
};

/*-----------------------------------------------------------------------------
	World/ Level/ Actor GC verification.
-----------------------------------------------------------------------------*/

#if STATS

/** Used by a delegate for access to player's viewpoint from StatsNotifyProviders */
void GetFirstPlayerViewPoint(FVector& out_Location, FRotator& out_Rotation)
{
	ULocalPlayer* Player = GEngine->GetDebugLocalPlayer();
	if( Player != NULL && Player->PlayerController != NULL )
	{
		// Calculate the player's view information.
		Player->PlayerController->GetPlayerViewPoint( out_Location, out_Rotation );		
	}
}

#endif


namespace EngineDefs
{
	// Time between successive runs of the hardware survey
	static const FTimespan HardwareSurveyInterval(30, 0, 0, 0);	// 30 days
}

/*-----------------------------------------------------------------------------
	Engine init and exit.
-----------------------------------------------------------------------------*/

/** Callback from OS when we get a low memory warning.
  * Note: might not be called from the game thread
  */
void EngineMemoryWarningHandler(const FGenericMemoryWarningContext& GenericContext)
{
	FPlatformMemoryStats Stats = FPlatformMemory::GetStats();

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("EngineMemoryWarningHandler: Mem Used %.2f MB, Texture Memory %.2f MB, Render Target memory %.2f MB, OS Free %.2f MB\n"), 
		Stats.UsedPhysical / 1048576.0f, 
		GCurrentTextureMemorySize / 1048576.0f, 
		GCurrentRendertargetMemorySize / 1048576.0f, 
		Stats.AvailablePhysical / 1048576.0f);

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	const auto OOMMemReportVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("Debug.OOMMemReport")); 
	const int32 OOMMemReport = OOMMemReportVar ? OOMMemReportVar->GetValueOnAnyThread() : false;
	if( OOMMemReport )
	{
		GEngine->Exec(NULL, TEXT("OBJ LIST"));
		GEngine->Exec(NULL, TEXT("MEM FROMREPORT"));
	}
#endif

	GLastMemoryWarningTime = FPlatformTime::Seconds();
}

UEngine::FOnNewStatRegistered UEngine::NewStatDelegate;

//
// Initialize the engine.
//
void UEngine::Init(IEngineLoop* InEngineLoop)
{
	UE_LOG(LogEngine, Log, TEXT("Initializing Engine..."));
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Engine Initialized"), STAT_EngineStartup, STATGROUP_LoadTime);

	// Set the memory warning handler
	FPlatformMisc::SetMemoryWarningHandler(EngineMemoryWarningHandler);

	EngineLoop = InEngineLoop;

	// Subsystems.
	FURL::StaticInit();
	ULinkerLoad::StaticInit(UTexture2D::StaticClass());

#if !UE_BUILD_SHIPPING
	// Check for overrides to the default map on the command line
	TCHAR MapName[512];
	if ( FParse::Value(FCommandLine::Get(), TEXT("DEFAULTMAP="), MapName, ARRAY_COUNT(MapName)) )
	{
		UE_LOG(LogEngine, Log, TEXT("Overriding default map to %s"), MapName);

		FString MapString = FString(MapName);
		UGameMapsSettings::SetGameDefaultMap(MapString);
	}
#endif // !UE_BUILD_SHIPPING

	// Add to root.
	AddToRoot();

	// Initialize the HMD, if any
	InitializeHMDDevice();

	// Disable the screensaver when running the game.
	if( GIsClient && !GIsEditor )
	{
		EnableScreenSaver( false );
	}

	if (!IsRunningDedicatedServer() && !IsRunningCommandlet())
	{
		// If Slate is being used, initialize the renderer after RHIInit 
		FSlateApplication& CurrentSlateApp = FSlateApplication::Get();
		CurrentSlateApp.InitializeSound( TSharedRef<FSlateSoundDevice>( new FSlateSoundDevice() ) );


		// Create test windows (if we were asked to do that)
		if( FParse::Param( FCommandLine::Get(), TEXT("SlateDebug") ) )
		{
			RestoreSlateTestSuite();
		}
	}

	// Assign thumbnail compressor/decompressor
	FObjectThumbnail::SetThumbnailCompressor( new FPNGThumbnailCompressor() );

	LoadObject<UClass>(UEngine::StaticClass()->GetOuter(), *UEngine::StaticClass()->GetName(), NULL, LOAD_Quiet|LOAD_NoWarn, NULL );
	// This reads the Engine.ini file to get the proper DefaultMaterial, etc.
	LoadConfig();

	SetConfiguredProcessLimits();

	bIsOverridingSelectedColor = false;

	// Set colors for selection materials
	SelectedMaterialColor = DefaultSelectedMaterialColor;
	SelectionOutlineColor = DefaultSelectedMaterialColor;

	InitializeObjectReferences();

	if (GConfig)
	{
		bool bTemp = true;
		GConfig->GetBool(TEXT("/Script/Engine.Engine"), TEXT("bEnableOnScreenDebugMessages"), bTemp, GEngineIni);
		bEnableOnScreenDebugMessages = bTemp ? true : false;
		bEnableOnScreenDebugMessagesDisplay = bEnableOnScreenDebugMessages;

		GConfig->GetBool(TEXT("DevOptions.Debug"), TEXT("ShowSelectedLightmap"), GShowDebugSelectedLightmap, GEngineIni);
	}

	GNearClippingPlane = NearClipPlane;

	// Initialize the audio device
	InitializeAudioDevice();

	if (GIsEditor)
	{
		// Create a WorldContext for the editor to use and create an initially empty world.
		FWorldContext &InitialWorldContext = CreateNewWorldContext(EWorldType::Editor);
		InitialWorldContext.SetCurrentWorld( UWorld::CreateWorld( EWorldType::Editor, true ) );
		GWorld = InitialWorldContext.World();
	}

	if ( IsConsoleBuild() )
	{
		bUseConsoleInput = true;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Optionally Exec an exec file
	FString Temp;
	if( FParse::Value(FCommandLine::Get(), TEXT("EXEC="), Temp) )
	{
		new(GEngine->DeferredCommands) FString(FString(TEXT("exec ")) + Temp);
	}

	// Optionally exec commands passed in the command line.
	FString ExecCmds;
	if( FParse::Value(FCommandLine::Get(), TEXT("ExecCmds="), ExecCmds, false) )
	{
		TArray<FString> CommandArray;
		ExecCmds.ParseIntoArray( &CommandArray, TEXT(","), true );

		for( int32 Cx = 0; Cx < CommandArray.Num(); ++Cx )
		{
			const FString& Command = CommandArray[Cx];
			// Skip leading whitespaces in the command.
			int32 Index = 0;
			while( FChar::IsWhitespace( Command[Index] ) )
			{
				Index++;
			}

			if( Index < Command.Len()-1 )
			{
				new(GEngine->DeferredCommands) FString(*Command+Index);
			}
		}
	}

	// optionally set the vsync console variable
	if( FParse::Param(FCommandLine::Get(), TEXT("vsync")) )
	{
		new(GEngine->DeferredCommands) FString(TEXT("r.vsync 1"));
	}

	// optionally set the vsync console variable
	if( FParse::Param(FCommandLine::Get(), TEXT("novsync")) )
	{
		new(GEngine->DeferredCommands) FString(TEXT("r.vsync 0"));
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	if (GetDerivedDataCache())
	{
		GetDerivedDataCacheRef().NotifyBootComplete();
	}

	// Manually delete any potential leftover crash videos in case we can't access the module
	// because the crash reporter will upload any leftover crash video from last session
	FString CrashVideoPath = FPaths::GameLogDir() + TEXT("CrashVideo.avi");
	IFileManager::Get().Delete(*CrashVideoPath);
	
	// register the engine with the travel and network failure broadcasts
	// games can override these to provide proper behavior in each error case
	OnTravelFailure().AddUObject(this, &UEngine::HandleTravelFailure);
	OnNetworkFailure().AddUObject(this, &UEngine::HandleNetworkFailure);

	UE_LOG(LogInit, Log, TEXT("Texture streaming: %s"), IStreamingManager::Get().IsTextureStreamingEnabled() ? TEXT("Enabled") : TEXT("Disabled") );

	IOnlineSubsystem* SubSystem = IOnlineSubsystem::Get();
	if(SubSystem)
	{
		IOnlineExternalUIPtr ExternalUI = SubSystem->GetExternalUIInterface();
		if(ExternalUI.IsValid())
		{
			FOnExternalUIChangeDelegate OnExternalUIChangeDelegate;
			OnExternalUIChangeDelegate.BindUObject(this, &UEngine::OnExternalUIChange);

			ExternalUI->AddOnExternalUIChangeDelegate(OnExternalUIChangeDelegate);
		}
	}

	// Initialise buffer visualization system data
	GetBufferVisualizationData().Initialize();

	// Connect the engine analytics provider
	FEngineAnalytics::Initialize();

#if WITH_EDITOR
	// register screenshot capture if we are dumping a movie
	if(GIsDumpingMovie)
	{
		UGameViewportClient::OnScreenshotCaptured().AddUObject(this, &UEngine::HandleScreenshotCaptured);
	}
#endif

	//Load the streaming pause rendering module.
	FModuleManager::LoadModulePtr<IModuleInterface>(TEXT("StreamingPauseRendering"));

	// Add the stats to the list, note this is also the order that they get rendered in if active.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_Version"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatVersion, NULL, true));
#endif
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_NamedEvents"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatNamedEvents, &UEngine::ToggleStatNamedEvents, true));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_FPS"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatFPS, &UEngine::ToggleStatFPS, true));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_Summary"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatSummary, NULL, true));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_Unit"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatUnit, &UEngine::ToggleStatUnit, true));
	/* @todo Slate Rendering
#if STATS
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_SlateBatches"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatSlateBatches, NULL, true));
#endif
	*/
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_Hitches"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatHitches, &UEngine::ToggleStatHitches, true));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_AI"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatAI, NULL, true));

	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_ColorList"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatColorList, NULL));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_Levels"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatLevels, NULL));
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_SoundMixes"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatSoundMixes, NULL));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_Reverb"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatReverb, NULL));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_SoundWaves"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatSoundWaves, NULL));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_SoundCues"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatSoundCues, NULL));
#endif
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_Sounds"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatSounds, &UEngine::ToggleStatSounds));
/* @todo UE4 physx fix this once we have convexelem drawing again
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_LevelMap"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatLevelMap, NULL));
*/
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_Detailed"), TEXT("STATCAT_Engine"), FText::GetEmpty(), NULL, &UEngine::ToggleStatDetailed));
#if !UE_BUILD_SHIPPING
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_UnitMax"), TEXT("STATCAT_Engine"), FText::GetEmpty(), NULL, &UEngine::ToggleStatUnitMax));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_UnitGraph"), TEXT("STATCAT_Engine"), FText::GetEmpty(), NULL, &UEngine::ToggleStatUnitGraph));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_UnitTime"), TEXT("STATCAT_Engine"), FText::GetEmpty(), NULL, &UEngine::ToggleStatUnitTime));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_Raw"), TEXT("STATCAT_Engine"), FText::GetEmpty(), NULL, &UEngine::ToggleStatRaw));
#endif

	// Let any listeners know about the new stats
	for (int32 StatIdx = 0; StatIdx < EngineStats.Num(); StatIdx++)
	{
		const FEngineStatFuncs& EngineStat = EngineStats[StatIdx];
		NewStatDelegate.Broadcast(EngineStat.CommandName, EngineStat.CategoryName, EngineStat.DescriptionString);
	}

	// Record the analytics for any attached HMD devices
	RecordHMDAnalytics();
}

void UEngine::RegisterBeginStreamingPauseRenderingDelegate( FBeginStreamingPauseDelegate* InDelegate )
{
	BeginStreamingPauseDelegate = InDelegate;
}

void UEngine::RegisterEndStreamingPauseRenderingDelegate( FEndStreamingPauseDelegate* InDelegate )
{
	EndStreamingPauseDelegate = InDelegate;
}

void UEngine::OnExternalUIChange(bool bInIsOpening)
{
	FSlateApplication::Get().ExternalUIChange(bInIsOpening);
}

void UEngine::ShutdownAudioDevice()
{
	if (AudioDevice)
	{
		AudioDevice->Teardown();
		AudioDevice = NULL;
	}
}

void UEngine::PreExit()
{
	ShutdownRenderingCVarsCaching();
	FEngineAnalytics::Shutdown();

#if WITH_EDITOR
	UGameViewportClient::OnScreenshotCaptured().RemoveUObject(this, &UEngine::HandleScreenshotCaptured);
#endif

	if (ScreenSaverInhibitor)
	{
		ScreenSaverInhibitor->Kill();
		delete ScreenSaverInhibitor;
	}

	delete ScreenSaverInhibitorRunnable;
}

void UEngine::TickDeferredCommands()
{
	// Execute all currently queued deferred commands (allows commands to be queued up for next frame).
	const int32 DeferredCommandsCount = DeferredCommands.Num();
	for( int32 DeferredCommandsIndex=0; DeferredCommandsIndex<DeferredCommandsCount; DeferredCommandsIndex++ )
	{
		// Use LocalPlayer if available...
		ULocalPlayer* LocalPlayer = GetDebugLocalPlayer();
		if( LocalPlayer )
		{
			LocalPlayer->Exec( LocalPlayer->GetWorld(), *DeferredCommands[DeferredCommandsIndex], *GLog );
		}
		// and fall back to UEngine otherwise.
		else
		{
			Exec( GWorld, *DeferredCommands[DeferredCommandsIndex], *GLog );
		}
	}
	DeferredCommands.RemoveAt(0, DeferredCommandsCount);
}

void UEngine::UpdateTimeAndHandleMaxTickRate()
{
	// start at now minus a bit so we don't get a zero delta.
	static double LastTime = FPlatformTime::Seconds() - 0.0001;
	static bool bTimeWasManipulated = false;

	// Figure out whether we want to use real or fixed time step.
	const bool bUseFixedTimeStep = FApp::IsBenchmarking() || FApp::UseFixedTimeStep();

	FApp::UpdateLastTime();

	// Calculate delta time and update time.
	if( bUseFixedTimeStep )
	{
		bTimeWasManipulated = true;

		FApp::SetDeltaTime(FApp::GetFixedDeltaTime());
		LastTime = FApp::GetCurrentTime();
		FApp::SetCurrentTime(FApp::GetCurrentTime() + FApp::GetDeltaTime());
	}
	else
	{
		FApp::SetCurrentTime(FPlatformTime::Seconds());
		// Did we just switch from a fixed time step to real-time?  If so, then we'll update our
		// cached 'last time' so our current interval isn't huge (or negative!)
		if( bTimeWasManipulated )
		{
			LastTime = FApp::GetCurrentTime() - FApp::GetDeltaTime();
			bTimeWasManipulated = false;
		}

		// Calculate delta time.
		float DeltaTime = FApp::GetCurrentTime() - LastTime;

		// Negative delta time means something is wrong with the system. Error out so user can address issue.
		if( DeltaTime < 0 )
		{
			// AMD dual-core systems are a known issue that require AMD CPU drivers to be installed. Installer will take care of this for shipping.
			UE_LOG(LogEngine, Fatal,TEXT("Detected negative delta time - on AMD systems please install http://files.aoaforums.com/I3199-setup.zip.html"));
			DeltaTime = 0.01;
		}

		// Get max tick rate based on network settings and current delta time.
		const float MaxTickRate	= GetMaxTickRate( DeltaTime );
		float WaitTime		= 0;
		// Convert from max FPS to wait time.
		if( MaxTickRate > 0 )
		{
			WaitTime = FMath::Max( 1.f / MaxTickRate - DeltaTime, 0.f );
		}

		// Enforce maximum framerate and smooth framerate by waiting.
		STAT( double ActualWaitTime = 0.f ); 
		if( WaitTime > 0 )
		{
			double WaitEndTime = FApp::GetCurrentTime() + WaitTime;
			SCOPE_SECONDS_COUNTER(ActualWaitTime);
			SCOPE_CYCLE_COUNTER(STAT_GameTickWaitTime);
			SCOPE_CYCLE_COUNTER(STAT_GameIdleTime);

			if (IsRunningDedicatedServer()) // We aren't so concerned about wall time with a server, lots of CPU is wasted spinning. I suspect there is more to do with sleeping and time on dedicated servers.
			{
				FPlatformProcess::Sleep(WaitTime);
			}
			else
			{
				// Sleep if we're waiting more than 5 ms. We set the scheduler granularity to 1 ms
				// at startup on PC. We reserve 2 ms of slack time which we will wait for by giving
				// up our timeslice.
				if( WaitTime > 5 / 1000.f )
				{
					FPlatformProcess::Sleep( WaitTime - 0.002f );
				}

				// Give up timeslice for remainder of wait time.
				while( FPlatformTime::Seconds() < WaitEndTime )
				{
					FPlatformProcess::Sleep( 0 );
				}
			}
			FApp::SetCurrentTime(FPlatformTime::Seconds());
		}


		SET_FLOAT_STAT(STAT_GameTickWantedWaitTime,WaitTime * 1000.f);
		SET_FLOAT_STAT(STAT_GameTickAdditionalWaitTime,FMath::Max<float>((ActualWaitTime-WaitTime)*1000.f,0.f));

		FApp::SetDeltaTime(FApp::GetCurrentTime() - LastTime);

		// Negative delta time means something is wrong with the system. Error out so user can address issue.
		if( FApp::GetDeltaTime() < 0 )
		{
			// AMD dual-core systems are a known issue that require AMD CPU drivers to be installed. Installer will take care of this for shipping.
			UE_LOG(LogEngine, Fatal,TEXT("Detected negative delta time - on AMD systems please install http://files.aoaforums.com/I3199-setup.zip.html"));
			FApp::SetDeltaTime(0.01);
		}
		LastTime			= FApp::GetCurrentTime();

		// Enforce a maximum delta time if wanted.
		UGameEngine* GameEngine = Cast<UGameEngine>(this);
		const float MaxDeltaTime = GameEngine ? GameEngine->MaxDeltaTime : 0.f;
		if( MaxDeltaTime > 0.f )
		{
			UWorld* World = NULL;

			int32 NumGamePlayers = 0;
			for (int32 WorldIndex = 0; WorldIndex < WorldList.Num(); ++WorldIndex)
			{
				if (WorldList[WorldIndex].WorldType == EWorldType::Game)
				{
					World = WorldList[WorldIndex].World();
					NumGamePlayers = WorldList[WorldIndex].GamePlayers.Num();
					break;
				}
			}

			// We don't want to modify delta time if we are dealing with network clients as either host or client.
			if( World != NULL
				// Not having a game info implies being a client.
				&& ( ( World->GetAuthGameMode() != NULL
				// NumPlayers and GamePlayer only match in standalone game types and handles the case of splitscreen.
				&&	World->GetAuthGameMode()->NumPlayers == NumGamePlayers ) ) )
			{
				// Happy clamping!
				FApp::SetDeltaTime(FMath::Min<double>(FApp::GetDeltaTime(), MaxDeltaTime));
			}
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	{
		float OverrideFPS = CVarSetOverrideFPS.GetValueOnGameThread();
		if(OverrideFPS >= 0.001f)
		{
			// in seconds
			FApp::SetDeltaTime(1.0f / OverrideFPS);
			LastTime = FApp::GetCurrentTime();
			FApp::SetCurrentTime(FApp::GetCurrentTime() + FApp::GetDeltaTime());
			bTimeWasManipulated = true;
		}
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}


void UEngine::ParseCommandline()
{
	// If dedicated server, the -nosound, or -benchmark parameters are used, disable sound.
	if(FParse::Param(FCommandLine::Get(),TEXT("nosound")) || FApp::IsBenchmarking() || IsRunningDedicatedServer() || IsRunningCommandlet())
	{
		bUseSound = false;
	}

	if( FParse::Param( FCommandLine::Get(), TEXT("noailogging")) )
	{
		bDisableAILogging = true;
	}

	if( FParse::Param( FCommandLine::Get(), TEXT("enableailogging")) )
	{
		bDisableAILogging = false;
	}

	bStartWithMatineeCapture = false;
	bCompressMatineeCapture = false;
#if WITH_EDITOR
	if (!GIsEditor && FParse::Value(FCommandLine::Get(), TEXT("-MATINEEAVICAPTURE="), MatineeCaptureName))
	{
		MatineeCaptureType = EMatineeCaptureType::AVI;
		bStartWithMatineeCapture = true;
	}
	else if (!GIsEditor && FParse::Value(FCommandLine::Get(), TEXT("-MATINEESSCAPTURE="), MatineeCaptureName))
	{
		MatineeCaptureType = EMatineeCaptureType::BMP;

		FString MatineeCaptureFormat;
		if(FParse::Value(FCommandLine::Get(), TEXT("-MATINEESSFORMAT="), MatineeCaptureFormat))
		{
			if(MatineeCaptureFormat == TEXT("BMP"))
			{
				MatineeCaptureType = EMatineeCaptureType::BMP;
			}
			else if(MatineeCaptureFormat == TEXT("PNG"))
			{
				MatineeCaptureType = EMatineeCaptureType::PNG;
			}
			else if(MatineeCaptureFormat == TEXT("JPEG"))
			{
				MatineeCaptureType = EMatineeCaptureType::JPEG;
			}
		}

		bStartWithMatineeCapture = true;
	}

	// If we are capturing a matinee movie and we want to dump the buffer visualization shots too, for on all required functionality
	if (!GIsEditor && FParse::Param(FCommandLine::Get(), TEXT("MATINEEBUFFERVISUALIZATIONDUMP")))
	{
		static IConsoleVariable* CVarDumpFrames = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BufferVisualizationDumpFrames"));
	
		if (CVarDumpFrames)
		{
			CVarDumpFrames->Set(1);
		}
	}

	if (bStartWithMatineeCapture)
	{
		FParse::Value(FCommandLine::Get(), TEXT("-MATINEEPACKAGE="), MatineePackageCaptureName);
	}

	if ( !GIsEditor && FParse::Param(FCommandLine::Get(), TEXT("COMPRESSCAPTURE")) )
	{
		bCompressMatineeCapture = true;
	}
#endif
	MatineeCaptureFPS = 30;
}


/**
 * Loads a special material and verifies that it is marked as a special material (some shaders
 * will only be compiled for materials marked as "special engine material")
 *
 * @param MaterialName Fully qualified name of a material to load/find
 * @param Material Reference to a material object pointer that will be filled out
 * @param bCheckUsage Check if the material has been marked to be used as a special engine material
 */
void LoadSpecialMaterial(const FString& MaterialName, UMaterial*& Material, bool bCheckUsage)
{
	// only bother with materials that aren't already loaded
	if (Material == NULL)
	{
		// find or load the object
		Material = LoadObject<UMaterial>(NULL, *MaterialName, NULL, LOAD_None, NULL);	

		if (!Material)
		{
#if !WITH_EDITORONLY_DATA
			UE_LOG(LogEngine, Log, TEXT("ERROR: Failed to load special material '%s'. This will probably have bad consequences (depending on its use)"), *MaterialName);
#else
			UE_LOG(LogEngine, Fatal,TEXT("Failed to load special material '%s'"), *MaterialName);
#endif
		}
		// if the material wasn't marked as being a special engine material, then not all of the shaders 
		// will have been compiled on it by this point, so we need to compile them and alert the use
		// to set the bit
		else if (!Material->bUsedAsSpecialEngineMaterial && bCheckUsage) 
		{
#if !WITH_EDITORONLY_DATA
			// consoles must have the flag set properly in the editor
			UE_LOG(LogEngine, Fatal,TEXT("The special material (%s) was not marked with bUsedAsSpecialEngineMaterial. Make sure this flag is set in the editor, save the package, and compile shaders for this platform"), *MaterialName);
#else
			Material->bUsedAsSpecialEngineMaterial = true;
			Material->MarkPackageDirty();

			// make sure all necessary shaders for the default are compiled, now that the flag is set
			Material->PostEditChange();

			FMessageDialog::Open( EAppMsgType::Ok, FText::Format( NSLOCTEXT("Engine", "SpecialMaterialConfiguredIncorrectly", "The special material ({0}) has not been marked with bUsedAsSpecialEngineMaterial.\nThis will prevent shader precompiling properly, so the flag has been set automatically.\nMake sure to save the package and distribute to everyone using this material."), FText::FromString( MaterialName ) ) );
#endif
		}
	}
}


/**
 * Loads all Engine object references from their corresponding config entries.
 */
void UEngine::InitializeObjectReferences()
{
	// initialize the special engine/editor materials
	if (AllowDebugViewmodes())
	{
		// Materials that are needed in-game if debug viewmodes are allowed
		LoadSpecialMaterial(WireframeMaterialName.AssetLongPathname, WireframeMaterial, true);
		LoadSpecialMaterial(LevelColorationLitMaterialName.AssetLongPathname, LevelColorationLitMaterial, true);
		LoadSpecialMaterial(LevelColorationUnlitMaterialName.AssetLongPathname, LevelColorationUnlitMaterial, true);
		LoadSpecialMaterial(LightingTexelDensityName.AssetLongPathname, LightingTexelDensityMaterial, false);
		LoadSpecialMaterial(ShadedLevelColorationLitMaterialName.AssetLongPathname, ShadedLevelColorationLitMaterial, true);
		LoadSpecialMaterial(ShadedLevelColorationUnlitMaterialName.AssetLongPathname, ShadedLevelColorationUnlitMaterial, true);
		LoadSpecialMaterial(VertexColorMaterialName.AssetLongPathname, VertexColorMaterial, false);
		LoadSpecialMaterial(VertexColorViewModeMaterialName_ColorOnly.AssetLongPathname, VertexColorViewModeMaterial_ColorOnly, false);
		LoadSpecialMaterial(VertexColorViewModeMaterialName_AlphaAsColor.AssetLongPathname, VertexColorViewModeMaterial_AlphaAsColor, false);
		LoadSpecialMaterial(VertexColorViewModeMaterialName_RedOnly.AssetLongPathname, VertexColorViewModeMaterial_RedOnly, false);
		LoadSpecialMaterial(VertexColorViewModeMaterialName_GreenOnly.AssetLongPathname, VertexColorViewModeMaterial_GreenOnly, false);
		LoadSpecialMaterial(VertexColorViewModeMaterialName_BlueOnly.AssetLongPathname, VertexColorViewModeMaterial_BlueOnly, false);
	}

	// Materials that may or may not be needed when debug viewmodes are disabled but haven't been fixed up yet
	LoadSpecialMaterial(RemoveSurfaceMaterialName.AssetLongPathname, RemoveSurfaceMaterial, false);	

	// these one's are needed both editor and standalone 
	LoadSpecialMaterial(DebugMeshMaterialName.AssetLongPathname, DebugMeshMaterial, false);
	LoadSpecialMaterial(InvalidLightmapSettingsMaterialName.AssetLongPathname, InvalidLightmapSettingsMaterial, false);
	LoadSpecialMaterial(ArrowMaterialName.AssetLongPathname, ArrowMaterial, false);


	if (GIsEditor && !IsRunningCommandlet())
	{
		// Materials that are only needed in the interactive editor
#if WITH_EDITORONLY_DATA
		LoadSpecialMaterial(GeomMaterialName.AssetLongPathname, GeomMaterial, false);
		LoadSpecialMaterial(EditorBrushMaterialName.AssetLongPathname, EditorBrushMaterial, false);
		LoadSpecialMaterial(BoneWeightMaterialName.AssetLongPathname, BoneWeightMaterial, false);
#endif

		LoadSpecialMaterial(PreviewShadowsIndicatorMaterialName.AssetLongPathname, PreviewShadowsIndicatorMaterial, false);
		LoadSpecialMaterial(ConstraintLimitMaterialName.AssetLongPathname, ConstraintLimitMaterial, false);

		//@TODO: This should move into the editor (used in editor modes exclusively)
		if (DefaultBSPVertexTexture == NULL)
		{
			DefaultBSPVertexTexture = LoadObject<UTexture2D>(NULL, *DefaultBSPVertexTextureName.AssetLongPathname, NULL, LOAD_None, NULL);
		}
	}

	if( DefaultTexture == NULL )
	{
		DefaultTexture = LoadObject<UTexture2D>(NULL, *DefaultTextureName.AssetLongPathname, NULL, LOAD_None, NULL);	
	}

	if( DefaultDiffuseTexture == NULL )
	{
		DefaultDiffuseTexture = LoadObject<UTexture2D>(NULL, *DefaultDiffuseTextureName.AssetLongPathname, NULL, LOAD_None, NULL);	
	}

	if( HighFrequencyNoiseTexture == NULL )
	{
		HighFrequencyNoiseTexture = LoadObject<UTexture2D>(NULL, *HighFrequencyNoiseTextureName.AssetLongPathname, NULL, LOAD_None, NULL);	
	}

	if( DefaultBokehTexture == NULL )
	{
		DefaultBokehTexture = LoadObject<UTexture2D>(NULL, *DefaultBokehTextureName.AssetLongPathname, NULL, LOAD_None, NULL);	
	}

	if( PreIntegratedSkinBRDFTexture == NULL )
	{
		PreIntegratedSkinBRDFTexture = LoadObject<UTexture2D>(NULL, *PreIntegratedSkinBRDFTextureName.AssetLongPathname, NULL, LOAD_None, NULL);	
	}

	if( MiniFontTexture == NULL )
	{
		MiniFontTexture = LoadObject<UTexture2D>(NULL, *MiniFontTextureName.AssetLongPathname, NULL, LOAD_None, NULL);	
	}

	if( WeightMapPlaceholderTexture == NULL )
	{
		WeightMapPlaceholderTexture = LoadObject<UTexture2D>(NULL, *WeightMapPlaceholderTextureName.AssetLongPathname, NULL, LOAD_None, NULL);	
	}

	if (LightMapDensityTexture == NULL)
	{
		LightMapDensityTexture = LoadObject<UTexture2D>(NULL, *LightMapDensityTextureName.AssetLongPathname, NULL, LOAD_None, NULL);
	}

	if ( DefaultPhysMaterial == NULL )
	{
		DefaultPhysMaterial = LoadObject<UPhysicalMaterial>(NULL, *DefaultPhysMaterialName.AssetLongPathname, NULL, LOAD_None, NULL);	

		checkf(DefaultPhysMaterial != NULL, TEXT("The default material (%s) is not found. Please make sure you have default material set up correctly."), *DefaultPhysMaterialName.AssetLongPathname);
	}

	if ( ConsoleClass == NULL )
	{
		ConsoleClass = LoadClass<UConsole>(NULL, *ConsoleClassName.ClassName, NULL, LOAD_None, NULL);
	}

	if ( GameViewportClientClass == NULL )
	{
		GameViewportClientClass = LoadClass<UGameViewportClient>(NULL, *GameViewportClientClassName.ClassName, NULL, LOAD_None, NULL);

		checkf(GameViewportClientClass != NULL, TEXT("Engine config value GameViewportClientClassName is not a valid class name."));
	}

	if ( LocalPlayerClass == NULL )
	{
		LocalPlayerClass = LoadClass<ULocalPlayer>(NULL, *LocalPlayerClassName.ClassName, NULL, LOAD_None, NULL);
	}

	if ( WorldSettingsClass == NULL )
	{
		WorldSettingsClass = LoadClass<AWorldSettings>(NULL, *WorldSettingsClassName.ClassName, NULL, LOAD_None, NULL);
	}

	if ( NavigationSystemClass == NULL )
	{
		NavigationSystemClass = LoadClass<UNavigationSystem>(NULL, *NavigationSystemClassName.ClassName, NULL, LOAD_None, NULL);
	}

	if ( AvoidanceManagerClass == NULL )
	{
		AvoidanceManagerClass = LoadClass<UAvoidanceManager>(NULL, *AvoidanceManagerClassName.ClassName, NULL, LOAD_None, NULL);
	}

	if ( PhysicsCollisionHandlerClass == NULL )
	{
		PhysicsCollisionHandlerClass = LoadClass<UPhysicsCollisionHandler>(NULL, *PhysicsCollisionHandlerClassName.ClassName, NULL, LOAD_None, NULL);
	}

	if ( GameUserSettingsClass == NULL )
	{
		GameUserSettingsClass = LoadClass<UGameUserSettings>(NULL, *GameUserSettingsClassName.ClassName, NULL, LOAD_None, NULL);
	}

	if ( LevelScriptActorClass == NULL )
	{
		LevelScriptActorClass = LoadClass<ALevelScriptActor>(NULL, *LevelScriptActorClassName.ClassName, NULL, LOAD_None, NULL);
	}

	// set the font object pointers
	if( TinyFont == NULL && TinyFontName.AssetLongPathname.Len() )
	{
		TinyFont = LoadObject<UFont>(NULL,*TinyFontName.AssetLongPathname,NULL,LOAD_None,NULL);
	}
	if( SmallFont == NULL && SmallFontName.AssetLongPathname.Len() )
	{
		SmallFont = LoadObject<UFont>(NULL,*SmallFontName.AssetLongPathname,NULL,LOAD_None,NULL);
	}
	if( MediumFont == NULL && MediumFontName.AssetLongPathname.Len() )
	{
		MediumFont = LoadObject<UFont>(NULL,*MediumFontName.AssetLongPathname,NULL,LOAD_None,NULL);
	}
	if( LargeFont == NULL && LargeFontName.AssetLongPathname.Len() )
	{
		LargeFont = LoadObject<UFont>(NULL,*LargeFontName.AssetLongPathname,NULL,LOAD_None,NULL);
	}
	if( SubtitleFont == NULL && SubtitleFontName.AssetLongPathname.Len() )
	{
		SubtitleFont = LoadObject<UFont>(NULL,*SubtitleFontName.AssetLongPathname,NULL,LOAD_None,NULL);
	}

	// Additional fonts.
	AdditionalFonts.Empty( AdditionalFontNames.Num() );
	for ( int32 FontIndex = 0 ; FontIndex < AdditionalFontNames.Num() ; ++FontIndex )
	{
		const FString& FontName = AdditionalFontNames[FontIndex];
		UFont* NewFont = NULL;
		if( FontName.Len() )
		{
			NewFont = LoadObject<UFont>(NULL,*FontName,NULL,LOAD_None,NULL);
		}
		AdditionalFonts.Add( NewFont );
	}

	if ( GameSingleton == NULL && GameSingletonClassName.ClassName.Len() > 0)
	{
		UClass *SingletonClass = LoadClass<UObject>(NULL, *GameSingletonClassName.ClassName, NULL, LOAD_None, NULL);

		checkf(SingletonClass != NULL, TEXT("Engine config value GameSingletonClassName is not a valid class name."));

		GameSingleton = ConstructObject<UObject>(SingletonClass, this);
	}

	if ( DefaultTireType == NULL && DefaultTireTypeName.AssetLongPathname.Len() )
	{
		DefaultTireType = LoadObject<UTireType>(NULL,*DefaultTireTypeName.AssetLongPathname,NULL,LOAD_None,NULL);
	}

	if( DefaultPreviewPawnClass == NULL && DefaultPreviewPawnClassName.ClassName.Len() )
	{
		DefaultPreviewPawnClass = LoadClass<APawn>(NULL, *DefaultPreviewPawnClassName.ClassName, NULL, LOAD_None, NULL);

		checkf(DefaultPreviewPawnClass != NULL, TEXT("Engine config value DefaultPreviewPawnClass is not a valid class name."));
	}
}

//
// Exit the engine.
//
void UEngine::FinishDestroy()
{
	// Remove from root.
	RemoveFromRoot();

	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		// shut down all subsystems.
		GEngine = NULL;
		if (AudioDevice)
		{
			AudioDevice->Teardown();
		}

		FURL::StaticExit();
	}

	Super::FinishDestroy();
}

void UEngine::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// count memory
	if (Ar.IsCountingMemory())
	{
		if (AudioDevice)
		{
			AudioDevice->CountBytes(Ar);
		}
	}
}

void UEngine::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UEngine* This = CastChecked<UEngine>(InThis);

	// track objects in the audio device
	if (This->AudioDevice)
	{
		This->AudioDevice->AddReferencedObjects(Collector);
	}
	Super::AddReferencedObjects(This, Collector);
}

void UEngine::CleanupGameViewport()
{
	for (auto WorldIt = WorldList.CreateIterator(); WorldIt; ++WorldIt)
	{
		FWorldContext &Context = *WorldIt;
		// Clean up the viewports that have been closed.
		for(int32 idx = Context.GamePlayers.Num()-1; idx >= 0; --idx)
		{
			ULocalPlayer *Player = Context.GamePlayers[idx];

			if(Player && Player->ViewportClient && !Player->ViewportClient->Viewport)
			{
				if (Player->PlayerController)
				{
					Player->PlayerController->CleanupGameViewport();
				}
				Player->ViewportClient = NULL;
				Player->PlayerRemoved();
				Context.GamePlayers.RemoveAt(idx);
			}
		}

		if ( Context.GameViewport != NULL && Context.GameViewport->Viewport == NULL )
		{
			if (Context.GameViewport == GameViewport)
			{
				GameViewport = NULL;
			}

			Context.GameViewport->DetachViewportClient();
			Context.GameViewport = NULL;
		}
	}
}

bool UEngine::IsEditor()
{
	return GIsEditor;
}


UFont* UEngine::GetTinyFont()
{
	return GEngine->TinyFont;
}


UFont* UEngine::GetSmallFont()
{
	return GEngine->SmallFont;
}


UFont* UEngine::GetMediumFont()
{
	return GEngine->MediumFont;
}

/**
 * Returns the engine's default large font
 */
UFont* UEngine::GetLargeFont()
{
	return GEngine->LargeFont;
}

/**
 * Returns the engine's default subtitle font
 */
UFont* UEngine::GetSubtitleFont()
{
	return GEngine->SubtitleFont;
}

/**
 * Returns the specified additional font.
 *
 * @param	AdditionalFontIndex		Index into the AddtionalFonts array.
 */
UFont* UEngine::GetAdditionalFont(int32 AdditionalFontIndex)
{
	return GEngine->AdditionalFonts.IsValidIndex(AdditionalFontIndex) ? GEngine->AdditionalFonts[AdditionalFontIndex] : NULL;
}

/**
 *	Initialize the audio device
 *
 *	@return	bool		true if successful, false if not
 */
bool UEngine::InitializeAudioDevice()
{
	if (AudioDevice == NULL)
	{
		// Initialize the audio device.
		if (bUseSound == true)
		{
			// get the module name from the ini file
			FString AudioDeviceModuleName;
			GConfig->GetString(TEXT("Audio"), TEXT("AudioDeviceModuleName"), AudioDeviceModuleName, GEngineIni);

			if (AudioDeviceModuleName.Len() > 0)
			{
				// load the module by name from the .ini
				IAudioDeviceModule* AudioDeviceModule = FModuleManager::LoadModulePtr<IAudioDeviceModule>(*AudioDeviceModuleName);

				// did the module exist?
				if (AudioDeviceModule)
				{
					// use the module object to create the audio device
					AudioDevice = AudioDeviceModule->CreateAudioDevice();
					if (AudioDevice)
					{
						// Attempt to initialize the device
						if ( !AudioDevice->Init() )
						{
							// Failed to initialize the device. Delete it.
							delete AudioDevice;
							AudioDevice = NULL;
						}
					}
				}
			}
		}
	}

	return (AudioDevice != NULL);
}

bool UEngine::UseSound() const
{
	return (bUseSound && AudioDevice);
}
/**
 * A fake stereo rendering device used to test stereo rendering without an attached device.
 */
class FFakeStereoRenderingDevice : public IStereoRendering
{
public:
	virtual ~FFakeStereoRenderingDevice() {}

	virtual bool IsStereoEnabled() const override { return true; }

	virtual bool EnableStereo(bool stereo = true) override { return true; }

	virtual void AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override
	{
		SizeX = SizeX / 2;
		if( StereoPass == eSSP_RIGHT_EYE )
		{
			X += SizeX;
		}
	}

	virtual void CalculateStereoViewOffset(const enum EStereoscopicPass StereoPassType, const FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation) override
	{
		if( StereoPassType != eSSP_FULL)
		{
			float EyeOffset = 3.20000005f;
			const float PassOffset = (StereoPassType == eSSP_LEFT_EYE) ? EyeOffset : -EyeOffset;
			ViewLocation += ViewRotation.Quaternion().RotateVector(FVector(0,PassOffset,0));
		}
	}

	virtual FMatrix GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType, const float FOV) const override
	{
		const float ProjectionCenterOffset = 0.151976421f;
		const float PassProjectionOffset = (StereoPassType == eSSP_LEFT_EYE) ? ProjectionCenterOffset : -ProjectionCenterOffset;

		const float HalfFov = 2.19686294f / 2.f;
		const float InWidth = 640.f;
		const float InHeight = 480.f;
		const float XS = 1.0f / tan(HalfFov);
		const float YS = InWidth / tan(HalfFov) / InHeight;

		const float InNearZ = GNearClippingPlane;
		return FMatrix(
			FPlane(XS,                      0.0f,								    0.0f,							0.0f),
			FPlane(0.0f,					YS,	                                    0.0f,							0.0f),
			FPlane(0.0f,	                0.0f,								    0.0f,							1.0f),
			FPlane(0.0f,					0.0f,								    InNearZ,						0.0f))
 
			* FTranslationMatrix(FVector(PassProjectionOffset,0,0));

	}

	virtual void InitCanvasFromView(FSceneView* InView, UCanvas* Canvas) {}

	virtual void PushViewportCanvas(EStereoscopicPass StereoPass, FCanvas *InCanvas, UCanvas *InCanvasObject, FViewport *InViewport) const override 
	{
		FMatrix m;
		m.SetIdentity();
		InCanvas->PushAbsoluteTransform(m);
	}

	virtual void PushViewCanvas(EStereoscopicPass StereoPass, FCanvas *InCanvas, UCanvas *InCanvasObject, FSceneView *InView) const override 
	{
		FMatrix m;
		m.SetIdentity();
		InCanvas->PushAbsoluteTransform(m);
	}

	virtual void GetEyeRenderParams_RenderThread(EStereoscopicPass StereoPass, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const override
	{
		EyeToSrcUVOffsetValue = FVector2D::ZeroVector;
		EyeToSrcUVScaleValue = FVector2D(1.0f, 1.0f);
	}

	virtual bool ShouldUseSeparateRenderTarget() const override 
	{ 
		// should return true to test rendering into a separate texture; however, there is a bug
		// in DrawNormalizedScreenQuad (FScreenVS shader), TTP #338597, so false for now.
		return false; //true; 
	}

	virtual void RenderTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef BackBuffer, FTexture2DRHIParamRef SrcTexture) const override
	{
		check(IsInRenderingThread());

		//RHISetRenderTarget( BackBuffer, FTextureRHIRef() );
		SetRenderTarget(RHICmdList, BackBuffer, FTextureRHIRef());
		const uint32 ViewportWidth = BackBuffer->GetSizeX();
		const uint32 ViewportHeight = BackBuffer->GetSizeY();
		RHICmdList.SetViewport( 0,0,0,ViewportWidth, ViewportHeight, 1.0f );

		
		RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
		RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
		RHICmdList.Clear(true, FLinearColor::Black, false, 0, false, 0, FIntRect());
	}
};

bool UEngine::InitializeHMDDevice()
{
	if( !GIsEditor )
	{
		if (FParse::Param(FCommandLine::Get(),TEXT("emulatestereo")))
		{
			TSharedPtr<FFakeStereoRenderingDevice> FakeStereoDevice(new FFakeStereoRenderingDevice());
			StereoRenderingDevice = FakeStereoDevice;
		}
		// No reason to connect an HMD on a dedicated server.  Also fixes dedicated servers stealing the oculus connection.
		else if(!HMDDevice.IsValid() && !FParse::Param(FCommandLine::Get(),TEXT("nohmd")) && !IsRunningDedicatedServer())
		{
			// Get a list of plugins that implement this feature
			TArray<IHeadMountedDisplayModule*> HMDImplementations = IModularFeatures::Get().GetModularFeatureImplementations<IHeadMountedDisplayModule>( IHeadMountedDisplayModule::GetModularFeatureName() );
			for( auto HMDModuleIt = HMDImplementations.CreateIterator(); HMDModuleIt && !HMDDevice.IsValid(); ++HMDModuleIt )
			{
				HMDDevice = (*HMDModuleIt)->CreateHeadMountedDisplay();
				if( HMDDevice.IsValid() )
				{
					StereoRenderingDevice = HMDDevice;
				}
			}
		}
	}
 
	return StereoRenderingDevice.IsValid();
}

void UEngine::RecordHMDAnalytics()
{
	if( !GIsEditor )
	{
		if(HMDDevice.IsValid() && !FParse::Param(FCommandLine::Get(),TEXT("nohmd")))
		{
			HMDDevice->RecordAnalytics();
		}
	}
}

/** @return whether we're currently running in split screen (more than one local player) */
bool UEngine::IsSplitScreen(UWorld *InWorld)
{
	if (InWorld == NULL)
{
		// If no specified world, return true if any world context has multiple local players
		for (auto It = WorldList.CreateIterator(); It; ++It)
		{
			if (It->GamePlayers.Num() > 1)
			{
				return true;
			}
		}

		return false;
	}

	return (GetNumGamePlayers(InWorld) > 1);
}

/** @return whether we're currently running with stereoscopic 3D enabled */
bool UEngine::IsStereoscopic3D()
{
	return !GIsEditor && StereoRenderingDevice.IsValid() && StereoRenderingDevice->IsStereoEnabled();
}

ULocalPlayer* GetLocalPlayerFromControllerId_local(const TArray<class ULocalPlayer*>& GamePlayers, int32 ControllerId)
{
	for ( int32 PlayerIndex = 0; PlayerIndex < GamePlayers.Num(); PlayerIndex++ )
	{
		ULocalPlayer* const Player = GamePlayers[PlayerIndex];
		if ( Player && Player->ControllerId == ControllerId )
		{
			return Player;
		}
	}

	return NULL;
}

ULocalPlayer* UEngine::GetLocalPlayerFromControllerId( const UGameViewportClient * InViewport, int32 ControllerId )
{
	if (GetWorldContextFromGameViewport(InViewport) != NULL)
	{
		const TArray<class ULocalPlayer*>& GamePlayers = GetGamePlayers(InViewport);
		return GetLocalPlayerFromControllerId_local(GamePlayers, ControllerId);
	}
	return NULL;
}

ULocalPlayer* UEngine::GetLocalPlayerFromControllerId( UWorld * InWorld, int32 ControllerId )
{
	const TArray<class ULocalPlayer*>& GamePlayers = GetGamePlayers(InWorld);
	return GetLocalPlayerFromControllerId_local(GamePlayers, ControllerId);
}

void UEngine::SwapControllerId(ULocalPlayer *NewPlayer, int32 CurrentControllerId, int32 NewControllerID)
{
	for (auto It = WorldList.CreateIterator(); It; ++It)
{
		if (It->GamePlayers.Contains(NewPlayer))
		{
			// This is the world context that NewPlayer belongs to, see if anyone is using his CurrentControllerId
			for (int32 i=0; i < It->GamePlayers.Num(); ++i)
			{
				if(It->GamePlayers[i] && It->GamePlayers[i]->ControllerId == NewControllerID)
				{
					It->GamePlayers[i]->ControllerId = CurrentControllerId;
					return;
				}
			}
		}
	}
}

APlayerController* UEngine::GetFirstLocalPlayerController(UWorld *InWorld)
{
	const TArray<class ULocalPlayer*>& GamePlayers = GetGamePlayers(InWorld);

	for( int32 iPlayers=0; iPlayers < GamePlayers.Num(); iPlayers++ )
	{
		if( GamePlayers[iPlayers] && GamePlayers[iPlayers]->PlayerController )
		{
			return GamePlayers[iPlayers]->PlayerController;
		}
	}

	return NULL;
}

void UEngine::GetAllLocalPlayerControllers(TArray<APlayerController*> & PlayerList)
{
	for (auto It = WorldList.CreateIterator(); It; ++It)
	{
		for (auto PlayerIt = It->GamePlayers.CreateIterator(); PlayerIt; ++PlayerIt)
		{
			ULocalPlayer *Player = *PlayerIt;
			PlayerList.Add( Player->PlayerController );
		}
	}
}


/*-----------------------------------------------------------------------------
	Input.
-----------------------------------------------------------------------------*/

#if !UE_BUILD_SHIPPING

/**
 * Helper structure for sorting textures by relative cost.
 */
struct FSortedTexture 
{
	int32		OrigSizeX;
	int32		OrigSizeY;
	int32		CookedSizeX;
	int32		CookedSizeY;
	int32		CurSizeX;
	int32		CurSizeY;
	int32		LODBias;
	int32		MaxSize;
	int32		CurrentSize;
	FString Name;
	int32		LODGroup;
	bool	bIsStreaming;
	int32		UsageCount;

	/** Constructor, initializing every member variable with passed in values. */
	FSortedTexture(	int32 InOrigSizeX, int32 InOrigSizeY, int32 InCookedSizeX, int32 InCookedSizeY, int32 InCurSizeX, int32 InCurSizeY, int32 InLODBias, int32 InMaxSize, int32 InCurrentSize, const FString& InName, int32 InLODGroup, bool bInIsStreaming, int32 InUsageCount )
	:	OrigSizeX( InOrigSizeX )
	,	OrigSizeY( InOrigSizeY )
	,	CookedSizeX( InCookedSizeX )
	,	CookedSizeY( InCookedSizeY )
	,	CurSizeX( InCurSizeX )
	,	CurSizeY( InCurSizeY )
	,	LODBias( InLODBias )
	,	MaxSize( InMaxSize )
	,	CurrentSize( InCurrentSize )
	,	Name( InName )
	,	LODGroup( InLODGroup )
	,	bIsStreaming( bInIsStreaming )
	,	UsageCount( InUsageCount )
	{}
};
struct FCompareFSortedTexture
{
	bool bAlphaSort;
	FCompareFSortedTexture( bool InAlphaSort )
		: bAlphaSort( InAlphaSort )
	{}
	FORCEINLINE bool operator()( const FSortedTexture& A, const FSortedTexture& B ) const
	{
		return bAlphaSort ? ( A.Name < B.Name ) : ( B.MaxSize < A.MaxSize );
	}
};

/** Helper struct for sorting anim sets by size */
struct FSortedSet
{
	FString Name;
	int32		Size;

	FSortedSet( const FString& InName, int32 InSize )
	:	Name(InName)
	,	Size(InSize)
	{}
};
struct FCompareFSortedSet
{
	bool bAlphaSort;
	FCompareFSortedSet( bool InAlphaSort )
		: bAlphaSort( InAlphaSort )
	{}
	FORCEINLINE bool operator()( const FSortedSet& A, const FSortedSet& B ) const
	{
		return bAlphaSort ? ( A.Name < B.Name ) : ( B.Size < A.Size );
	}
};

#if !UE_BUILD_SHIPPING
struct FSortedParticleSet
{
	FString Name;
	int32		Size;
	int32		PSysSize;
	int32		ModuleSize;
	int32		ComponentSize;
	int32		ComponentCount;
	int32		ComponentResourceSize;
	int32		ComponentTrueResourceSize;

	FSortedParticleSet( const FString& InName, int32 InSize, int32 InPSysSize, int32 InModuleSize, 
		int32 InComponentSize, int32 InComponentCount, int32 InComponentResourceSize, int32 InComponentTrueResourceSize) :
		  Name(InName)
		, Size(InSize)
		, PSysSize(InPSysSize)
		, ModuleSize(InModuleSize)
		, ComponentSize(InComponentSize)
		, ComponentCount(InComponentCount)
		, ComponentResourceSize(InComponentResourceSize)
		, ComponentTrueResourceSize(InComponentTrueResourceSize)
	{}
};

struct FCompareFSortedParticleSet
{
	bool bAlphaSort;
	FCompareFSortedParticleSet( bool InAlphaSort )
		: bAlphaSort( InAlphaSort )
	{}
	FORCEINLINE bool operator()( const FSortedParticleSet& A, const FSortedParticleSet& B ) const
	{
		return bAlphaSort ? ( A.Name < B.Name ) : ( B.Size < A.Size );
	}
};

#endif

static void ShowSubobjectGraph( FOutputDevice& Ar, UObject* CurrentObject, const FString& IndentString )
{
	if ( CurrentObject == NULL )
	{
		Ar.Logf(TEXT("%sX NULL"), *IndentString);
	}
	else
	{
		TArray<UObject*> ReferencedObjs;
		FReferenceFinder RefCollector( ReferencedObjs, CurrentObject, true, false, false, false);
		RefCollector.FindReferences( CurrentObject );

		if ( ReferencedObjs.Num() == 0 )
		{
			Ar.Logf(TEXT("%s. %s"), *IndentString, IndentString.Len() == 0 ? *CurrentObject->GetPathName() : *CurrentObject->GetName());
		}
		else
		{
			Ar.Logf(TEXT("%s+ %s"), *IndentString, IndentString.Len() == 0 ? *CurrentObject->GetPathName() : *CurrentObject->GetName());
			for ( int32 ObjIndex = 0; ObjIndex < ReferencedObjs.Num(); ObjIndex++ )
			{
				ShowSubobjectGraph(Ar, ReferencedObjs[ObjIndex], IndentString + TEXT("|\t"));
			}
		}
	}
}

/** Holds information about memory usage. */
struct FMemItem
{
	int32		Count;
	SIZE_T	Num, Max, Res;
	UObject* Object;

	FMemItem()
		: Count(0)
		, Num(0)
		, Max(0)
		, Res(0)
		, Object( NULL )
	{}

	FMemItem( UObject* InObject, SIZE_T	InRes )
		: Count(0)
		, Num(0)
		, Max(0)
		, Res(InRes)
		, Object( InObject )
	{}

	void Add( FArchiveCountMem& Ar, SIZE_T InRes )
	{
		Count ++; 
		Num += Ar.GetNum(); 
		Max += Ar.GetMax(); 
		Res += InRes;
	}

	void AddRes( SIZE_T InRes )
	{
		Count ++; 
		Res += InRes;
	}
};

struct FItem
{
	UClass*	Class;
	int32		Count;
	SIZE_T	Num, Max, Res, TrueRes;
	FItem( UClass* InClass=NULL )
	: Class(InClass), Count(0), Num(0), Max(0), Res(0), TrueRes(0)
	{}
	void Add( FArchiveCountMem& Ar, SIZE_T InRes, SIZE_T InTrueRes )
	{
		Count++;
		Num += Ar.GetNum();
		Max += Ar.GetMax();
		Res += InRes;
		TrueRes += InTrueRes;
	}
};
struct FSubItem
{
	UObject* Object;
	SIZE_T Num, Max, Res, TrueRes;
	FSubItem( UObject* InObject, SIZE_T InNum, SIZE_T InMax, SIZE_T InRes, SIZE_T InTrueRes )
	: Object( InObject ), Num( InNum ), Max( InMax ), Res( InRes ), TrueRes( InTrueRes )
	{}
};

#endif // !UE_BUILD_SHIPPING

MSVC_PRAGMA(warning(push))
MSVC_PRAGMA(warning(disable : 4717))
static void InfiniteRecursionFunction(bool B)
{
	if(B)
		InfiniteRecursionFunction(B);
}
MSVC_PRAGMA(warning(pop))

/** DEBUG used for exe "DEBUG BUFFEROVERFLOW" */
static void BufferOverflowFunction(SIZE_T BufferSize, const ANSICHAR* Buffer) 
{
	ANSICHAR LocalBuffer[32];
	LocalBuffer[0] = LocalBuffer[31] = 0; //if BufferSize is 0 then there's nothing to print out!

	BufferSize = FMath::Min<SIZE_T>(BufferSize, ARRAY_COUNT(LocalBuffer)-1);

	for( uint32 i = 0; i < BufferSize; i++ ) 
	{
		LocalBuffer[i] = Buffer[i];
	}
	UE_LOG(LogEngine, Log, TEXT("BufferOverflowFunction BufferSize=%d LocalBuffer=%s"),(int32)BufferSize, ANSI_TO_TCHAR(LocalBuffer));
}

bool UEngine::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	// If we don't have a viewport specified to catch the stat commands, use to the game viewport
	if (GStatProcessingViewportClient == NULL)
	{
		GStatProcessingViewportClient = GameViewport;
	}


	// See if any other subsystems claim the command.
	if (StaticExec(InWorld, Cmd,Ar) == true)
	{
		return true;
	}
	

	if (GDebugToolExec && (GDebugToolExec->Exec( InWorld, Cmd,Ar) == true))
	{
		return true;
	}

	if (GMalloc && (GMalloc->Exec( InWorld, Cmd,Ar) == true))
	{
		return true;
	}

	if (GSystemSettings.Exec( InWorld, Cmd,Ar) == true)
	{
		return true;
	}

	if (GetAudioDevice() && (GetAudioDevice()->Exec( InWorld, Cmd,Ar) == true))
	{
		return true;
	}
	
	if (FPlatformMisc::Exec( InWorld, Cmd,Ar) == true)
	{
		return true;
	}

	if (HMDDevice.IsValid() && HMDDevice->Exec( InWorld, Cmd, Ar ))
	{
		return true;
	}

	// Handle engine command line.
	if ( FParse::Command(&Cmd,TEXT("FLUSHLOG")) )
	{
		return HandleFlushLogCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("EXIT")) || FParse::Command(&Cmd,TEXT("QUIT")))
	{
		return HandleExitCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd, TEXT("GAMEVER")) ||  FParse::Command(&Cmd, TEXT("GAMEVERSION")))
	{
		return HandleGameVerCommand( Cmd, Ar );
	}
#if 0
	else if (FParse::Command(&Cmd, TEXT("HOTFIXTEST")))
	{
		FTestHotFixPayload Test;
		Test.ValueToReturn = true;
		Test.Result = false;
		Test.Message = FString(TEXT("Hi there"));
		FCoreDelegates::GetHotfixDelegate(EHotfixDelegates::Test).ExecuteIfBound(&Test, sizeof(FTestHotFixPayload));
		check(Test.ValueToReturn == Test.Result);
	}
#endif
	else if( FParse::Command(&Cmd,TEXT("STAT")) )
	{
		return HandleStatCommand(InWorld, GStatProcessingViewportClient, Cmd, Ar);
	}
	else if( FParse::Command(&Cmd,TEXT("STARTMOVIECAPTURE")) && (GEngine->bStartWithMatineeCapture == true || GIsEditor) )
	{
		return HandleStartMovieCaptureCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("STOPMOVIECAPTURE")) && (GEngine->bStartWithMatineeCapture == true || GIsEditor) )
	{
		return HandleStopMovieCaptureCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("CRACKURL")) )
	{
		return HandleCrackURLCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("DEFER")) )
	{
		return HandleDeferCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT("OPEN") ) )
	{
		return HandleOpenCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command( &Cmd, TEXT("STREAMMAP")) )
	{
		return HandleStreamMapCommand( Cmd, Ar, InWorld );
	}
#if WITH_SERVER_CODE
	else if (FParse::Command(&Cmd, TEXT("SERVERTRAVEL")) )
	{
		return HandleServerTravelCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command( &Cmd, TEXT("SAY") ) )
	{
		return HandleSayCommand( Cmd, Ar, InWorld );
	}
#endif // WITH_SERVER_CODE
	else if( FParse::Command( &Cmd, TEXT("DISCONNECT")) )
	{
		return HandleDisconnectCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command( &Cmd, TEXT("RECONNECT")) )
	{
		return HandleReconnectCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command( &Cmd, TEXT("TRAVEL") ) )
	{
		return HandleTravelCommand( Cmd, Ar, InWorld );
	}
	else if (FParse::Command(&Cmd, TEXT("CE")))
	{
		return HandleCeCommand( InWorld, Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("GAMMA") ) )
	{
		return HandleGammaCommand( Cmd, Ar );
	}
#if STATS
	else if( FParse::Command( &Cmd, TEXT("DUMPPARTICLEMEM") ) )
	{
		return HandleDumpParticleMemCommand( Cmd, Ar );
	}
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	else if( FParse::Command(&Cmd,TEXT("HotReload")) )
	{
		return HandleHotReloadCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("DumpConsoleCommands")))
	{
		return HandleDumpConsoleCommandsCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command(&Cmd,TEXT("SHOWMATERIALDRAWEVENTS")) )
	{
		return HandleShowMaterialDrawEventsCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("DUMPAVAILABLERESOLUTIONS")))
	{
		return HandleDumpAvailableResolutionsCommand( Cmd, Ar );
	}
	else if(FParse::Command(&Cmd,TEXT("ANIMSEQSTATS")))
	{
		return HandleAnimSeqStatsCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("CountDisabledParticleItems")))
	{
		return HandleCountDisabledParticleItemsCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT("VIEWNAMES") ) )
	{
		return HandleViewnamesCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("FREEZESTREAMING")) )
	{
		return HandleFreezeStreamingCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command(&Cmd,TEXT("FREEZEALL")) )
	{
		return HandleFreezeAllCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command(&Cmd, TEXT("FLUSHIOMANAGER")) )
	{
		return HandleFlushIOManagerCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("ToggleRenderingThread")) )
	{
		return HandleToggleRenderingThreadCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("RecompileShaders")) )				    
	{
		return HandleRecompileShadersCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("RecompileGlobalShaders")) )
	{
		return HandleRecompileGlobalShadersCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("DUMPSHADERSTATS")) )
	{
		return HandleDumpShaderStatsCommand( Cmd, Ar );		
	}
	else if( FParse::Command(&Cmd,TEXT("DUMPMATERIALSTATS")) )
	{
		return HandleDumpMaterialStatsCommand( Cmd, Ar );	
	}
	else if( FParse::Command(&Cmd,TEXT("PROFILEGPU")) )
	{
		return HandleProfileGPUCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("visrt")))
	{
		extern bool HandleVisualizeRT();
		return HandleVisualizeRT();
	}
	else if( FParse::Command(&Cmd,TEXT("PROFILE")) )
	{
		return HandleProfileCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("PROFILEGPUHITCHES")) )				    
	{
		return HandleProfileGPUHitchesCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("SHADERCOMPLEXITY")) )
	{
		return HandleShaderComplexityCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("FREEZERENDERING")) )
	{
		return HandleFreezeRenderingCommand( Cmd, Ar, InWorld );
	}
	else if (FParse::Command(&Cmd, TEXT("ShowSelectedLightmap")))
	{
		return HandleShowSelectedLightmapCommand( Cmd, Ar );
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if !UE_BUILD_SHIPPING
	else if( FParse::Command(&Cmd,TEXT("SHOWLOG")) )
	{
		return HandleShowLogCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("STARTFPSCHART")) )
	{
		return HandleStartFPSChartCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("STOPFPSCHART")) )
	{
		return HandleStopFPSChartCommand( Cmd, Ar, InWorld );
	}
	else if (FParse::Command(&Cmd, TEXT("DumpLevelScriptActors")))
	{
		return HandleDumpLevelScriptActorsCommand( InWorld, Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("KE")) || FParse::Command(&Cmd, TEXT("KISMETEVENT")))
	{
		return HandleKismetEventCommand( Cmd, Ar );
	}
	else if(FParse::Command(&Cmd,TEXT("LISTTEXTURES")))
	{
		return HandleListTexturesCommand( Cmd, Ar );
	}
	else if(FParse::Command(&Cmd,TEXT("REMOTETEXTURESTATS")))
	{
		return HandleRemoteTextureStatsCommand( Cmd, Ar );
	}
	else if(FParse::Command(&Cmd,TEXT("LISTPARTICLESYSTEMS")))
	{
		return HandleListParticleSystemsCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("LISTSPAWNEDACTORS")) )
	{
		return HandleListSpawnedActorsCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command( &Cmd, TEXT("MemReport") ) )
	{
		return HandleMemReportCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command( &Cmd, TEXT("MemReportDeferred") ) )
	{
		return HandleMemReportDeferredCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command( &Cmd, TEXT("PARTICLEMESHUSAGE") ) )
	{
		return HandleParticleMeshUsageCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT("DUMPPARTICLECOUNTS") ) )
	{
		return HandleDumpParticleCountsCommand( Cmd, Ar );
	}
	// This will list out the packages which are in the precache list and have not been "loaded" out.  (e.g. could be just there taking up memory!)
	else if( FParse::Command( &Cmd, TEXT("ListPrecacheMapPackages") ) )
	{		
		return HandleListPreCacheMapPackagesCommand( Cmd, Ar );
	}
	// we can't always do an obj linkers, as cooked games have their linkers tossed out.  So we need to look at the actual packages which are loaded
	else if( FParse::Command( &Cmd, TEXT("ListLoadedPackages") ) )
	{
		return HandleListLoadedPackagesCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd,TEXT("MEM")) )
	{
		return HandleMemCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("LOGOUTSTATLEVELS")) )
	{
		return HandleLogoutStatLevelsCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command( &Cmd, TEXT("DEBUG") ) )
	{
		return HandleDebugCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("MERGEMESH")))
	{
		return HandleMergeMeshCommand( Cmd, Ar, InWorld );
	}
	else if (FParse::Command(&Cmd, TEXT("CONTENTCOMPARISON")))
	{
		return HandleContentComparisonCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("TOGGLEGTPSYSLOD")))
	{
		return HandleTogglegtPsysLODCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("OBJ")) )
	{
		return HandleObjCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT("TESTSLATEGAMEUI")) && InWorld->IsGameWorld() )
	{
		return HandleTestslateGameUICommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("DIR")) )		// DIR [path\pattern]
	{
		return HandleDirCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("TRACKPARTICLERENDERINGSTATS")) )
	{
		return HandleTrackParticleRenderingStatsCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("DUMPPARTICLERENDERINGSTATS")) )
	{
		return HandleDumpParticleRenderingStatsCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("DUMPPARTICLEFRAMERENDERINGSTATS")) )
	{
		return HandleDumpParticleFrameRenderingStatsCommand( Cmd, Ar );
	}
	else if ( FParse::Command(&Cmd,TEXT("DUMPALLOCS")) )
	{
		return HandleDumpAllocatorStats( Cmd, Ar );
	}
	else if ( FParse::Command(&Cmd,TEXT("HEAPCHECK")) )
	{
		return HandleHeapCheckCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("TOGGLEONSCREENDEBUGMESSAGEDISPLAY")))
	{
		return HandleToggleOnscreenDebugMessageDisplayCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("TOGGLEONSCREENDEBUGMESSAGESYSTEM")))
	{
		return HandleToggleOnscreenDebugMessageSystemCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("DISABLEALLSCREENMESSAGES")) || FParse::Command(&Cmd,TEXT("DISABLESCREENMESSAGES")))
	{
		return HandleDisableAllScreenMessagesCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("ENABLEALLSCREENMESSAGES")) || FParse::Command(&Cmd,TEXT("ENABLESCREENMESSAGES")))
	{
		return HandleEnableAllScreenMessagesCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("TOGGLEALLSCREENMESSAGES")) || FParse::Command(&Cmd,TEXT("TOGGLESCREENMESSAGES")) || FParse::Command(&Cmd,TEXT("CAPTUREMODE")))
	{
		return HandleToggleAllScreenMessagesCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("CONFIGHASH")) )
	{
		return HandleConfigHashCommand( Cmd, Ar );
	}
	else if ( FParse::Command(&Cmd,TEXT("CONFIGMEM")) )
	{
		return HandleConfigMemCommand( Cmd, Ar );
	}
#endif // !UE_BUILD_SHIPPING
	
	else if ( FParse::Command(&Cmd,TEXT("SCALABILITY")) )
	{
		Scalability::ProcessCommand(Cmd, Ar);
		return true;
	}
	else if(IConsoleManager::Get().ProcessUserConsoleInput(Cmd, Ar, InWorld))
	{
		// console variable interaction (get value, set value or get help)
		return true;
	}
	else if (!IStreamingManager::HasShutdown() && IStreamingManager::Get().Exec( InWorld, Cmd,Ar ))
	{
		// The streaming manager has handled the exec command.
	}
	else if( FParse::Command(&Cmd, TEXT("DUMPTICKS")) )
	{
		return HandleDumpTicksCommand( InWorld, Cmd, Ar );
	}
#if USE_NETWORK_PROFILER
	else if( FParse::Command(&Cmd,TEXT("NETPROFILE")) )
	{
		GNetworkProfiler.Exec( InWorld, Cmd, Ar );
	}
#endif
	else 
	{
		return false;
	}

	return true;
}

bool UEngine::HandleStartMovieCaptureCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FAVIWriter* AVIWriter = FAVIWriter::GetInstance();
	if (AVIWriter && !AVIWriter->IsCapturing())
	{
		AVIWriter->StartCapture();
		return true;
	}
	return false;
}

bool UEngine::HandleStopMovieCaptureCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FAVIWriter* AVIWriter = FAVIWriter::GetInstance();
	if (AVIWriter && AVIWriter->IsCapturing() && !AVIWriter->IsCapturingSlateRenderer())
	{
		AVIWriter->StopCapture();
		return true;
	}
	return false;
}

bool UEngine::HandleGameVerCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString VersionString = FString::Printf(TEXT("GameVersion Branch: %s, Configuration: %s, Version: %s, CommandLine: %s"), 
		TEXT(BRANCH_NAME), EBuildConfigurations::ToString(FApp::GetBuildConfiguration()), *GEngineVersion.ToString(), FCommandLine::Get());

	Ar.Logf( *VersionString );
	FPlatformMisc::ClipboardCopy( *VersionString );

	return 1;
}

bool UEngine::HandleCrackURLCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FURL URL(NULL,Cmd,TRAVEL_Absolute);
	if( URL.Valid )
	{
		Ar.Logf( TEXT("     Protocol: %s"), *URL.Protocol );
		Ar.Logf( TEXT("         Host: %s"), *URL.Host );
		Ar.Logf( TEXT("         Port: %i"), URL.Port );
		Ar.Logf( TEXT("          Map: %s"), *URL.Map );
		Ar.Logf( TEXT("   NumOptions: %i"), URL.Op.Num() );
		for( int32 i=0; i<URL.Op.Num(); i++ )
			Ar.Logf( TEXT("     Option %i: %s"), i, *URL.Op[i] );
		Ar.Logf( TEXT("       Portal: %s"), *URL.Portal );
		Ar.Logf( TEXT("       String: '%s'"), *URL.ToString() );
	}
	else Ar.Logf( TEXT("BAD URL") );
	return 1;
}

bool UEngine::HandleDeferCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	new(DeferredCommands)FString(Cmd);
	return 1;
}

#if !UE_BUILD_SHIPPING
bool UEngine::HandleMergeMeshCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	FString CmdCopy = Cmd;
	TArray<FString> Tokens;
	while (CmdCopy.Len() > 0)
	{
		const TCHAR* LocalCmd = *CmdCopy;
		FString Token = FParse::Token(LocalCmd, true);
		Tokens.Add(Token);
		CmdCopy = CmdCopy.Right(CmdCopy.Len() - Token.Len() - 1);
	}

	// array of source meshes that will be merged
	TArray<USkeletalMesh*> SourceMeshList;

	if ( Tokens.Num() >= 2 )
	{
		for (int32 I=0; I<Tokens.Num(); ++I)
		{
			USkeletalMesh * SrcMesh = LoadObject<USkeletalMesh>(NULL, *Tokens[I], NULL, LOAD_None, NULL);
			if (SrcMesh)
			{
				SourceMeshList.Add(SrcMesh);
			}
		}
	}

	// find player controller skeletalmesh
	APawn * PlayerPawn = NULL;
	USkeletalMesh * PlayerMesh = NULL;
	for( FConstPlayerControllerIterator Iterator = InWorld->GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		APlayerController* PlayerController = *Iterator;
		if (PlayerController->GetCharacter() != NULL && PlayerController->GetCharacter()->Mesh.IsValid())
		{
			PlayerPawn = PlayerController->GetCharacter();
			PlayerMesh = PlayerController->GetCharacter()->Mesh->SkeletalMesh;
			break;
		}
	}

	if (PlayerMesh)
	{
		if (SourceMeshList.Num() ==  0)
		{
			SourceMeshList.Add(PlayerMesh);
			SourceMeshList.Add(PlayerMesh);
		}
	}
	else
	{
		// we don't have a pawn (because we couldn't find a mesh), use any pawn as a spawn point
		for( FConstPlayerControllerIterator Iterator = InWorld->GetPlayerControllerIterator(); Iterator; ++Iterator )
		{
			APlayerController* PlayerController = *Iterator;
			if (PlayerController->GetPawn() != NULL)
			{
				PlayerPawn = PlayerController->GetPawn();
				break;
			}
		}		
	}

	if (PlayerPawn && SourceMeshList.Num() >= 2)
	{
		// create the composite mesh
		USkeletalMesh* CompositeMesh = CastChecked<USkeletalMesh>(StaticConstructObject(USkeletalMesh::StaticClass(), GetTransientPackage(), NAME_None, RF_Transient));

		TArray<FSkelMeshMergeSectionMapping> InForceSectionMapping;
		// create an instance of the FSkeletalMeshMerge utility
		FSkeletalMeshMerge MeshMergeUtil( CompositeMesh, SourceMeshList, InForceSectionMapping,  0);

		// merge the source meshes into the composite mesh
		if( !MeshMergeUtil.DoMerge() )
		{
			// handle errors
			// ...
			UE_LOG(LogEngine, Log, TEXT("DoMerge Error: Merge Mesh Test Failed"));
			return true;
		}

		FVector SpawnLocation = PlayerPawn->GetActorLocation() + PlayerPawn->GetActorRotation().Vector()*50.f;

		// set the new composite mesh in the existing skeletal mesh component
		ASkeletalMeshActor* const SMA = PlayerPawn->GetWorld()->SpawnActor<ASkeletalMeshActor>( SpawnLocation, PlayerPawn->GetActorRotation()*-1);
		if (SMA)
		{
			SMA->SkeletalMeshComponent->SetSkeletalMesh(CompositeMesh);
		}
	}

	return true;
}
#endif // !UE_BUILD_SHIPPING


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
bool UEngine::HandleHotReloadCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString Module(FParse::Token(Cmd, 0));
	FString PackagePath( FString( TEXT( "/Script/" ) ) + Module );
	UPackage *Package = FindPackage(NULL,*PackagePath );
	if (!Package)
	{
		Ar.Logf( TEXT("Could not HotReload '%s', package not found in memory"),*Module);
	}
	else
	{
		Ar.Logf( TEXT("HotReloading %s..."),*Module);
		TArray< UPackage*> PackagesToRebind;
		PackagesToRebind.Add( Package );
		const bool bWaitForCompletion = true;	// Always wait when hotreload is initiated from the console
		RebindPackages(PackagesToRebind, TArray<FName>(), bWaitForCompletion, Ar);
	}
	return 1;
}

bool UEngine::HandleDumpConsoleCommandsCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	Ar.Logf(TEXT("DumpConsoleCommands: %s*"), Cmd);
	Ar.Logf(TEXT(""));
	ConsoleCommandLibrary_DumpLibrary( InWorld, *GEngine, FString(Cmd) + TEXT("*"), Ar);
	return true;
}

bool UEngine::HandleShowMaterialDrawEventsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GShowMaterialDrawEvents = !GShowMaterialDrawEvents;
	UE_LOG(LogEngine, Warning, TEXT("Show material names in SCOPED_DRAW_EVENT: %s"), GShowMaterialDrawEvents ? TEXT("true") : TEXT("false") );
	return true;
}

bool UEngine::HandleDumpAvailableResolutionsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	UE_LOG(LogEngine, Log, TEXT("DumpAvailableResolutions"));

	FScreenResolutionArray ResArray;
	if (RHIGetAvailableResolutions(ResArray, false))
	{
		for (int32 ModeIndex = 0; ModeIndex < ResArray.Num(); ModeIndex++)
		{
			FScreenResolutionRHI& ScreenRes = ResArray[ModeIndex];
			UE_LOG(LogEngine, Log, TEXT("DefaultAdapter - %4d x %4d @ %d"), 
				ScreenRes.Width, ScreenRes.Height, ScreenRes.RefreshRate);
		}
	}
	else
	{
		UE_LOG(LogEngine, Log, TEXT("Failed to get available resolutions!"));
	}
	return true;
}
bool UEngine::HandleAnimSeqStatsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	extern void GatherAnimSequenceStats(FOutputDevice& Ar);
	GatherAnimSequenceStats( Ar );
	return true;
}

bool UEngine::HandleCountDisabledParticleItemsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	int32 ParticleSystemCount = 0;
	int32 EmitterCount = 0;
	int32 DisabledEmitterCount = 0;
	int32 CookedOutEmitterCount = 0;
	int32 LODLevelCount = 0;
	int32 DisabledLODLevelCount = 0;
	int32 ModuleCount = 0;
	int32 DisabledModuleCount = 0;
	TMap<FString, int32> ModuleMap;
	for (TObjectIterator<UParticleSystem> It; It; ++It)
	{
		ParticleSystemCount++;

		TArray<UParticleModule*> ProcessedModules;
		TArray<UParticleModule*> DisabledModules;

		UParticleSystem* PSys = *It;
		for (int32 EmitterIdx = 0; EmitterIdx < PSys->Emitters.Num(); EmitterIdx++)
		{
			UParticleEmitter* Emitter = PSys->Emitters[EmitterIdx];
			if (Emitter != NULL)
			{
				bool bDisabledEmitter = true;
				EmitterCount++;
				if (Emitter->bCookedOut == true)
				{
					CookedOutEmitterCount++;
				}
				for (int32 LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
				{
					UParticleLODLevel* LODLevel = Emitter->LODLevels[LODIdx];
					if (LODLevel != NULL)
					{
						LODLevelCount++;
						if (LODLevel->bEnabled == false)
						{
							DisabledLODLevelCount++;
						}
						else
						{
							bDisabledEmitter = false;
						}
						for (int32 ModuleIdx = -3; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
						{
							UParticleModule* Module = NULL;
							switch (ModuleIdx)
							{
							case -3:	Module = LODLevel->RequiredModule;		break;
							case -2:	Module = LODLevel->SpawnModule;			break;
							case -1:	Module = LODLevel->TypeDataModule;		break;
							default:	Module = LODLevel->Modules[ModuleIdx];	break;
							}

							int32 DummyIdx;
							if ((Module != NULL) && (ProcessedModules.Find(Module, DummyIdx) == false))
							{
								ModuleCount++;
								ProcessedModules.AddUnique(Module);
								if (Module->bEnabled == false)
								{
									check(DisabledModules.Find(Module, DummyIdx) == false);
									DisabledModules.AddUnique(Module);
									DisabledModuleCount++;
								}

								FString ModuleName = Module->GetClass()->GetName();
								int32* ModuleCounter = ModuleMap.Find(ModuleName);
								if (ModuleCounter == NULL)
								{
									int32 TempInt = 0;
									ModuleMap.Add(ModuleName, TempInt);
									ModuleCounter = ModuleMap.Find(ModuleName);
								}
								check(ModuleCounter != NULL);
								*ModuleCounter = (*ModuleCounter + 1);
							}
						}
					}
				}

				if (bDisabledEmitter)
				{
					DisabledEmitterCount++;
				}
			}
		}
	}

	UE_LOG(LogEngine, Log, TEXT("%5d particle systems w/ %7d emitters (%5d disabled or %5.3f%% - %4d cookedout)"),
		ParticleSystemCount, EmitterCount, DisabledEmitterCount, (float)DisabledEmitterCount / (float)EmitterCount, CookedOutEmitterCount);
	UE_LOG(LogEngine, Log, TEXT("\t%8d lodlevels (%5d disabled or %5.3f%%)"),
		LODLevelCount, DisabledLODLevelCount, (float)DisabledLODLevelCount / (float)LODLevelCount);
	UE_LOG(LogEngine, Log, TEXT("\t\t%10d modules (%5d disabled or %5.3f%%)"),
		ModuleCount, DisabledModuleCount, (float)DisabledModuleCount / (float)ModuleCount);
	for (TMap<FString,int32>::TIterator It(ModuleMap); It; ++It)
	{
		FString ModuleName = It.Key();
		int32 ModuleCounter = It.Value();

		UE_LOG(LogEngine, Log, TEXT("\t\t\t%4d....%s"), ModuleCounter, *ModuleName);
	}

	return true;
}

// View the last N number of names added to the name table. Useful for tracking down name table bloat
bool UEngine::HandleViewnamesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	int32 NumNames = 0;
	if (FParse::Value(Cmd,TEXT("NUM="),NumNames))
	{
		for (int32 NameIndex = FMath::Max<int32>(FName::GetMaxNames() - NumNames, 0); NameIndex < FName::GetMaxNames(); NameIndex++)
		{
			Ar.Logf(TEXT("%d->%s"), NameIndex, *FName::SafeString(EName(NameIndex)));
		}
	}
	return true;
}

bool UEngine::HandleFreezeStreamingCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	ProcessToggleFreezeStreamingCommand(InWorld);
	return true;
}

bool UEngine::HandleFreezeAllCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld  )
{
	ProcessToggleFreezeCommand( InWorld );
	ProcessToggleFreezeStreamingCommand(InWorld);
	return true;
}

bool UEngine::HandleFlushIOManagerCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FIOSystem::Get().BlockTillAllRequestsFinishedAndFlushHandles();
	return true;
}

bool UEngine::HandleFreezeRenderingCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld  )
{
	ProcessToggleFreezeCommand( InWorld );
	return true;
}

bool UEngine::HandleShowSelectedLightmapCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GShowDebugSelectedLightmap = !GShowDebugSelectedLightmap;
	GConfig->SetBool(TEXT("DevOptions.Debug"), TEXT("ShowSelectedLightmap"), GShowDebugSelectedLightmap, GEngineIni);
	Ar.Logf( TEXT( "Showing the selected lightmap: %s" ), GShowDebugSelectedLightmap ? TEXT("true") : TEXT("false") );
	return true;
}

bool UEngine::HandleShaderComplexityCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString FlagStr(FParse::Token(Cmd, 0));
	if( FlagStr.Len() > 0 )
	{
		if( FCString::Stricmp(*FlagStr,TEXT("MAX"))==0)
		{
			float NewMax = FCString::Atof(Cmd);
			if (NewMax > 0.0f)
			{
				GEngine->MaxPixelShaderAdditiveComplexityCount = NewMax;
			}
		}
		else
		{
			Ar.Logf( TEXT("Format is 'shadercomplexity [toggleadditive] [togglepixel] [max $int]"));
			return true;
		}

		float CurrentMax = GEngine->MaxPixelShaderAdditiveComplexityCount;

		Ar.Logf( TEXT("New ShaderComplexity Settings: Max = %f"), CurrentMax);
	} 
	else
	{
		Ar.Logf( TEXT("Format is 'shadercomplexity [max $int]"));
	}
	return true; 
}

bool UEngine::HandleProfileGPUHitchesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GTriggerGPUHitchProfile = !GTriggerGPUHitchProfile;
	if (GTriggerGPUHitchProfile)
	{
		Ar.Logf(TEXT("Profiling GPU hitches."));
	}
	else
	{
		Ar.Logf(TEXT("Stopped profiling GPU hitches."));
	}
	return true;
}

bool UEngine::HandleToggleRenderingThreadCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if(GIsThreadedRendering)
	{
		StopRenderingThread();
		GUseThreadedRendering = false;
	}
	else
	{
		GUseThreadedRendering = true;
		StartRenderingThread();
	}
	Ar.Logf( TEXT("RenderThread is now in %s threaded mode."), GUseThreadedRendering ? TEXT("multi") : TEXT("single"));
	return true;
}

bool UEngine::HandleRecompileShadersCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	return RecompileShaders(Cmd, Ar);
}

bool UEngine::HandleRecompileGlobalShadersCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	extern void RecompileGlobalShaders();
	RecompileGlobalShaders();
	return 1;
}

bool UEngine::HandleDumpShaderStatsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString FlagStr(FParse::Token(Cmd, 0));
	EShaderPlatform Platform = GRHIShaderPlatform;
	if (FlagStr.Len() > 0)
	{
		Platform = ShaderFormatToLegacyShaderPlatform(FName(*FlagStr));
	}
	Ar.Logf(TEXT("Dumping shader stats for platform %s"), *LegacyShaderPlatformToShaderFormat(Platform).ToString());
	// Dump info on all loaded shaders regardless of platform and frequency.
	DumpShaderStats( Platform, SF_NumFrequencies );
	return true;
}

bool UEngine::HandleDumpMaterialStatsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString FlagStr(FParse::Token(Cmd, 0));
	EShaderPlatform Platform = GRHIShaderPlatform;
	if (FlagStr.Len() > 0)
	{
		Platform = ShaderFormatToLegacyShaderPlatform(FName(*FlagStr));
	}
	Ar.Logf(TEXT("Dumping material stats for platform %s"), *LegacyShaderPlatformToShaderFormat(Platform).ToString());
	// Dump info on all loaded shaders regardless of platform and frequency.
	extern ENGINE_API void DumpMaterialStats(EShaderPlatform);
	DumpMaterialStats( Platform );
	return true;
}

bool UEngine::HandleProfileGPUCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if (!GTriggerGPUHitchProfile)
	{
		GTriggerGPUProfile = true;
		Ar.Logf(TEXT("Profiling the next GPU frame"));
	}
	else
	{
		Ar.Logf(TEXT("Can't do a gpu profile during a hitch profile!"));
	}
	return true;
}

bool UEngine::HandleProfileCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if ( FParse::Command(&Cmd,TEXT("GPU")) )
	{
		if (!GTriggerGPUHitchProfile)
		{
			GTriggerGPUProfile = true;
			Ar.Logf(TEXT("Profiling the next GPU frame"));
		}
		else
		{
			Ar.Logf(TEXT("Can't do a gpu profile during a hitch profile!"));
		}
		return true;
	}
	return false;
}

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if !UE_BUILD_SHIPPING
bool UEngine::HandleShowLogCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Toggle display of console log window.
	if( GLogConsole )
	{
		GLogConsole->Show( !GLogConsole->IsShown() );
	}
	return 1;
}

bool UEngine::HandleStartFPSChartCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// start the chart data capture
	StartFPSChart();
	return true;
}

bool UEngine::HandleStopFPSChartCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	// stop the chart data capture
	StopFPSChart();

	// save out to disk
	FString MapName = InWorld ? InWorld->GetMapName() : TEXT("None");
	DumpFPSChart( MapName, true );
	return true;
}

bool UEngine::HandleDumpLevelScriptActorsCommand( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Dumps the object properties for all level script actors
	for( TArray<ULevel*>::TConstIterator  it = InWorld->GetLevels().CreateConstIterator(); it; ++it )
	{
		ULevel* CurrentLevel = *it;
		if( CurrentLevel && CurrentLevel->GetLevelScriptActor() )
		{
			AActor* LSActor = CurrentLevel->GetLevelScriptActor();
			if( LSActor )
			{
				UE_LOG(LogEngine, Log, TEXT("--- %s (%s) ---"), *LSActor->GetName(), *LSActor->GetOutermost()->GetName())
					for( TFieldIterator<UProperty> PropertyIt(LSActor->GetClass(), EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt )
					{
						UObjectPropertyBase* MyProperty = Cast<UObjectPropertyBase>(*PropertyIt);
						if( MyProperty )
						{
							UObject* PointedObject = NULL;
							PointedObject = MyProperty->GetObjectPropertyValue_InContainer(LSActor);

							if( PointedObject != NULL )
							{
								UObject* PointedOutermost = PointedObject->GetOutermost();
								UE_LOG(LogEngine, Log, TEXT("%s: %s (%s)"), *MyProperty->GetName(), *PointedObject->GetName(), *PointedOutermost->GetName());
							}

						}
					}
			}
		}
	}
	return true;
}

bool UEngine::HandleKismetEventCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString ObjectName = FParse::Token(Cmd, 0);
	if (ObjectName == TEXT("*"))
	{
		// Send the command to everything in the universe...
		for (TObjectIterator<UObject> It; It; ++It)
		{
			UObject* Obj = *It;
			Obj->CallFunctionByNameWithArguments(Cmd, Ar, NULL);
		}
	}
	else
	{
		UObject* ObjectToMatch = FindObject<UObject>(ANY_PACKAGE, *ObjectName);

		if (ObjectToMatch == NULL)
		{
			Ar.Logf(TEXT("Failed to find object named '%s'.  Specify a valid name or *"), *ObjectName);
		}
		else
		{
			ObjectToMatch->CallFunctionByNameWithArguments(Cmd, Ar, NULL);
		}
	}

	return true;
}

bool UEngine::HandleListTexturesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	const bool bShouldOnlyListStreaming = FParse::Command(&Cmd, TEXT("STREAMING"));
	const bool bShouldOnlyListNonStreaming = FParse::Command(&Cmd, TEXT("NONSTREAMING"));
	const bool bAlphaSort = FParse::Param( Cmd, TEXT("ALPHASORT") );

	Ar.Logf( TEXT("Listing %s textures."), bShouldOnlyListNonStreaming ? TEXT("non streaming") : bShouldOnlyListStreaming ? TEXT("streaming") : TEXT("all")  );

	// Find out how many times a texture is referenced by primitive components.
	TMap<UTexture2D*,int32> TextureToUsageMap;
	for( TObjectIterator<UPrimitiveComponent> It; It; ++It )
	{
		UPrimitiveComponent* PrimitiveComponent = *It;

		// Use the existing texture streaming functionality to gather referenced textures. Worth noting
		// that GetStreamingTextureInfo doesn't check whether a texture is actually streamable or not
		// and is also implemented for skeletal meshes and such.
		TArray<FStreamingTexturePrimitiveInfo> StreamingTextures;
		PrimitiveComponent->GetStreamingTextureInfo( StreamingTextures );

		// Increase usage count for all referenced textures
		for( int32 TextureIndex=0; TextureIndex<StreamingTextures.Num(); TextureIndex++ )
		{
			UTexture2D* Texture = Cast<UTexture2D>(StreamingTextures[TextureIndex].Texture);
			if( Texture )
			{
				// Initializes UsageCount to 0 if texture is not found.
				int32 UsageCount = TextureToUsageMap.FindRef( Texture );
				TextureToUsageMap.Add(Texture, UsageCount+1);
			}
		}
	}

	// Collect textures.
	TArray<FSortedTexture> SortedTextures;
	for( TObjectIterator<UTexture2D> It; It; ++It )
	{
		UTexture2D*		Texture				= *It;
		int32				LODGroup			= Texture->LODGroup;
		int32				LODBias				= Texture->GetCachedLODBias();
		int32				NumMips				= Texture->GetNumMips();	
		int32				MaxMips				= FMath::Max( 1, FMath::Min( NumMips - Texture->GetCachedLODBias(), GMaxTextureMipCount ) );
		int32				OrigSizeX			= Texture->GetSizeX();
		int32				OrigSizeY			= Texture->GetSizeY();
		int32				CookedSizeX			= Texture->GetSizeX() >> LODBias;
		int32				CookedSizeY			= Texture->GetSizeY() >> LODBias;
		int32				DroppedMips			= Texture->GetNumMips() - Texture->ResidentMips;
		int32				CurSizeX			= Texture->GetSizeX() >> DroppedMips;
		int32				CurSizeY			= Texture->GetSizeY() >> DroppedMips;
		bool			bIsStreamingTexture		= IStreamingManager::Get().IsTextureStreamingEnabled() ? IStreamingManager::Get().GetTextureStreamingManager().IsManagedStreamingTexture( Texture ) : false;
		int32				MaxSize				= Texture->CalcTextureMemorySizeEnum( TMC_AllMips );
		int32				CurrentSize			= Texture->CalcTextureMemorySizeEnum( TMC_ResidentMips );
		int32				UsageCount			= TextureToUsageMap.FindRef( Texture );

		if( (bShouldOnlyListStreaming && bIsStreamingTexture) 
			||	(bShouldOnlyListNonStreaming && !bIsStreamingTexture) 
			||	(!bShouldOnlyListStreaming && !bShouldOnlyListNonStreaming) )
		{
			new(SortedTextures) FSortedTexture( 
				OrigSizeX, 
				OrigSizeY, 
				CookedSizeX,
				CookedSizeY,
				CurSizeX,
				CurSizeY,
				LODBias, 
				MaxSize / 1024, 
				CurrentSize / 1024, 
				Texture->GetPathName(), 
				LODGroup, 
				bIsStreamingTexture,
				UsageCount);
		}
	}

	// Sort textures by cost.
	SortedTextures.Sort( FCompareFSortedTexture( bAlphaSort ) );

	// Retrieve mapping from LOD group enum value to text representation.
	TArray<FString> TextureGroupNames = FTextureLODSettings::GetTextureGroupNames();

	// Display.
	int32 TotalMaxSize		= 0;
	int32 TotalCurrentSize	= 0;
	Ar.Logf( TEXT(",Authored Width,Authored Height,Cooked Width,Cooked Height,Current Width,Current Height,Max Size,Current Size,LODBias,LODGroup,Name,Streaming,Usage Count") );
	for( int32 TextureIndex=0; TextureIndex<SortedTextures.Num(); TextureIndex++ )
	{
		const FSortedTexture& SortedTexture = SortedTextures[TextureIndex];
		Ar.Logf( TEXT(",%i,%i,%i,%i,%i,%i,%i,%i,%i,%s,%s,%s,%i"),
			SortedTexture.OrigSizeX,
			SortedTexture.OrigSizeY,
			SortedTexture.CookedSizeX,
			SortedTexture.CookedSizeY,
			SortedTexture.CurSizeX,
			SortedTexture.CurSizeY,
			SortedTexture.MaxSize,
			SortedTexture.CurrentSize,
			SortedTexture.LODBias,
			TextureGroupNames.IsValidIndex(SortedTexture.LODGroup) ? *TextureGroupNames[SortedTexture.LODGroup] : TEXT("INVALID"),
			*SortedTexture.Name,
			SortedTexture.bIsStreaming ? TEXT("YES") : TEXT("NO"),
			SortedTexture.UsageCount );

		TotalMaxSize		+= SortedTexture.MaxSize;
		TotalCurrentSize	+= SortedTexture.CurrentSize;
	}

	Ar.Logf(TEXT("Total size: Current= %d  Max= %d  Count=%d"), TotalCurrentSize, TotalMaxSize, SortedTextures.Num() );
	return true;
}

bool UEngine::HandleRemoteTextureStatsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Address which sent the command.  We will send stats back to this address
	FString Addr(FParse::Token(Cmd, 0));
	// Port to send to
	FString Port(FParse::Token(Cmd, 0));

	// Make an IP address. 	// @TODO ONLINE - Revisit "send over network"
	//FIpAddr SrcAddr;
	//SrcAddr.Port = FCString::Atoi( *Port );
	//SrcAddr.Addr = FCString::Atoi( *Addr );

	// Gather stats.
	double LastTime = FApp::GetLastTime();

	UE_LOG(LogEngine, Log,  TEXT("Remote AssetsStats request received.") );

	TMap<UTexture2D*,int32> TextureToUsageMap;

	TArray<UMaterialInterface*> UsedMaterials;
	TArray<UTexture*> UsedTextures;

	// Find out how many times a texture is referenced by primitive components.
	for( TObjectIterator<UPrimitiveComponent> It; It; ++It )
	{
		UPrimitiveComponent* PrimitiveComponent = *It;

		UsedMaterials.Reset();
		// Get the used materials off the primitive component so we can find the textures
		PrimitiveComponent->GetUsedMaterials( UsedMaterials );
		for( int32 MatIndex = 0; MatIndex < UsedMaterials.Num(); ++MatIndex )
		{
			// Ensure we dont have any NULL elements.
			if( UsedMaterials[ MatIndex ] )
			{
				UsedTextures.Reset();
				UsedMaterials[ MatIndex ]->GetUsedTextures( UsedTextures, EMaterialQualityLevel::Num, false );

				// Increase usage count for all referenced textures
				for( int32 TextureIndex=0; TextureIndex<UsedTextures.Num(); TextureIndex++ )
				{
					UTexture2D* Texture = Cast<UTexture2D>( UsedTextures[TextureIndex] );
					if( Texture )
					{
						// Initializes UsageCount to 0 if texture is not found.
						int32 UsageCount = TextureToUsageMap.FindRef( Texture );
						TextureToUsageMap.Add(Texture, UsageCount+1);
					}
				}
			}
		}
	}

	for(TObjectIterator<UTexture> It; It; ++It)
	{
		UTexture* Texture = *It;
		FString FullyQualifiedPath = Texture->GetPathName();
		FString MaxDim = FString::Printf( TEXT( "%dx%d" ), (int32)Texture->GetSurfaceWidth(), (int32)Texture->GetSurfaceHeight() );

		uint32 GroupId = Texture->LODGroup;
		uint32 FullyLoadedInBytes = Texture->CalcTextureMemorySizeEnum( TMC_AllMips );
		uint32 CurrentInBytes = Texture->CalcTextureMemorySizeEnum( TMC_ResidentMips );
		FString TexType;	// e.g. "2D", "Cube", ""
		uint32 FormatId = 0;
		float LastTimeRendered = FLT_MAX;
		uint32 NumUses = 0;
		int32 LODBias = Texture->GetCachedLODBias();
		FTexture* Resource = Texture->Resource; 

		if(Resource)
		{
			LastTimeRendered = LastTime - Resource->LastRenderTime;
		}

		FString CurrentDim = TEXT("?");
		UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
		if(Texture2D)
		{
			FormatId = Texture2D->GetPixelFormat();
			TexType = TEXT("2D");
			NumUses = TextureToUsageMap.FindRef( Texture2D );

			// Calculate in game current dimensions 
			const int32 DroppedMips = Texture2D->GetNumMips() - Texture2D->ResidentMips;
			CurrentDim = FString::Printf(TEXT("%dx%d"), Texture2D->GetSizeX() >> DroppedMips, Texture2D->GetSizeY() >> DroppedMips);
		}
		else
		{
			UTextureCube* TextureCube = Cast<UTextureCube>(Texture);
			if(TextureCube)
			{
				FormatId = TextureCube->GetPixelFormat();
				TexType = TEXT("Cube");
				// Calculate in game current dimensions
				CurrentDim = FString::Printf(TEXT("%dx%d"), TextureCube->GetSizeX(), TextureCube->GetSizeY());
			}
		}

		float CurrentKB = CurrentInBytes / 1024.0f;
		float FullyLoadedKB = FullyLoadedInBytes / 1024.0f;

		// @TODO ONLINE - Revisit "send over network"
		// 10KB should be enough
		//FNboSerializeToBuffer PayloadWriter(10 * 1024);
		//PayloadWriter << FullyQualifiedPath << MaxDim << CurrentDim << TexType << FormatId << GroupId << FullyLoadedKB << CurrentKB << LastTimeRendered << NumUses << LODBias;	
	}
	return true;
}

bool UEngine::HandleListParticleSystemsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TArray<FString> Switches;
	TArray<FString> Tokens;
	FCommandLine::Parse(Cmd, Tokens, Switches);

	const bool bAlphaSort = (Tokens.Find(TEXT("ALPHASORT")) != INDEX_NONE) || (Switches.Find(TEXT("ALPHASORT")) != INDEX_NONE);
	bool bDumpMesh = (Tokens.Find(TEXT("DUMPMESH")) != INDEX_NONE) || (Switches.Find(TEXT("DUMPMESH")) != INDEX_NONE);

	TArray<FSortedParticleSet> SortedSets;
	TMap<UObject *,int32> SortMap;

	FString Description;
	for( TObjectIterator<UParticleSystem> SystemIt; SystemIt; ++SystemIt )
	{			
		UParticleSystem* Tree = *SystemIt;
		Description = FString::Printf(TEXT("%s"), *Tree->GetPathName());
		FArchiveCountMem Count( Tree );
		int32 RootSize = Count.GetMax();

		FSortedParticleSet *pSet = new(SortedSets) FSortedParticleSet(Description, RootSize, RootSize, 0, 0, 0, 0, 0);

		SortMap.Add(Tree,SortedSets.Num() - 1);
	}

	for( TObjectIterator<UParticleModule> It; It; ++It )
	{
		UParticleModule* Module = *It;
		int32 *pIndex = SortMap.Find(Module->GetOuter());

		if (pIndex && SortedSets.IsValidIndex(*pIndex))
		{
			FSortedParticleSet &Set = SortedSets[*pIndex];
			FArchiveCountMem ModuleCount( Module );
			Set.ModuleSize += ModuleCount.GetMax();
			Set.Size += ModuleCount.GetMax();
		}
	}

	for( TObjectIterator<UParticleSystemComponent> It; It; ++It )
	{
		UParticleSystemComponent* Comp = *It;
		int32 *pIndex = SortMap.Find(Comp->Template);

		if (pIndex && SortedSets.IsValidIndex(*pIndex))
		{				
			FSortedParticleSet &Set = SortedSets[*pIndex];				
			FArchiveCountMem ComponentCount( Comp );
			Set.ComponentSize += ComponentCount.GetMax();

			// Save this for adding to the total
			SIZE_T CompResSize = Comp->GetResourceSize(EResourceSizeMode::Inclusive);
			Set.ComponentResourceSize += CompResSize;
			Set.ComponentTrueResourceSize += Comp->GetResourceSize(EResourceSizeMode::Exclusive);

			Set.Size += ComponentCount.GetMax();
			Set.Size += CompResSize;
			Set.ComponentCount++;

			UParticleSystem* Tree = Comp->Template;
			if (bDumpMesh && Tree != NULL)
			{
				for (int32 EmitterIdx = 0; (EmitterIdx < Tree->Emitters.Num()); EmitterIdx++)
				{
					UParticleEmitter* Emitter = Tree->Emitters[EmitterIdx];
					if (Emitter != NULL)
					{
						// Have to check each LOD level...
						if (Emitter->LODLevels.Num() > 0)
						{
							UParticleLODLevel* LODLevel = Emitter->LODLevels[0];
							if (LODLevel != NULL)
							{
								if (LODLevel->RequiredModule->bUseLocalSpace == true)
								{
									UParticleModuleTypeDataMesh* MeshTD = Cast<UParticleModuleTypeDataMesh>(LODLevel->TypeDataModule);
									if (MeshTD != NULL)
									{
										int32 InstCount = 0;
										// MESH EMITTER
										if (EmitterIdx < Comp->EmitterInstances.Num())
										{
											FParticleEmitterInstance* Inst = Comp->EmitterInstances[EmitterIdx];
											if (Inst != NULL)
											{
												InstCount = Inst->ActiveParticles;
											}

											UE_LOG(LogEngine, Warning, TEXT("---> PSys w/ mesh emitters: %2d %4d %s %s "), EmitterIdx, InstCount, 
												Comp->SceneProxy ? TEXT("Y") : TEXT("N"),
												*(Tree->GetPathName()));
										}
									}
								}
							}
						}
					}
				}
			}

		}
	}

	// Sort anim sets by cost
	SortedSets.Sort( FCompareFSortedParticleSet( bAlphaSort ) );

	// Now print them out.
	Ar.Logf(TEXT("ParticleSystems:"));
	Ar.Logf(TEXT("Size,Name,PSysSize,ModuleSize,ComponentSize,ComponentCount,CompResSize,CompTrueResSize"));
	int32 TotalSize = 0;
	for(int32 i=0; i<SortedSets.Num(); i++)
	{
		FSortedParticleSet& SetInfo = SortedSets[i];
		TotalSize += SetInfo.Size;
		Ar.Logf(TEXT("%10d,%s,%d,%d,%d,%d,%d,%d"), 
			SetInfo.Size, *SetInfo.Name, SetInfo.PSysSize, SetInfo.ModuleSize, SetInfo.ComponentSize, 
			SetInfo.ComponentCount, SetInfo.ComponentResourceSize, SetInfo.ComponentTrueResourceSize);
	}
	Ar.Logf(TEXT("Total Size:%d(%0.2f KB)"), TotalSize, TotalSize/1024.f);
	return true;
}

bool UEngine::HandleListSpawnedActorsCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	if( InWorld )
	{
		const float	TimeSeconds		    = InWorld->GetTimeSeconds();

		// Create alphanumerically sorted list of actors in persistent level.
		TArray<AActor*> SortedActorList = InWorld->PersistentLevel->Actors;
		SortedActorList.Remove(NULL);
		SortedActorList.Sort();

		Ar.Logf(TEXT("Listing spawned actors in persistent level:"));
		Ar.Logf(TEXT("Total: %d" ), SortedActorList.Num());

		if ( GetNumGamePlayers(InWorld) )
		{
			// If have local player, give info on distance to player
			const FVector PlayerLocation = GetGamePlayers(InWorld)[0]->LastViewLocation;

			// Iterate over all non-static actors and log detailed information.
			Ar.Logf(TEXT("TimeUnseen,TimeAlive,Distance,Class,Name,Owner"));
			for( int32 ActorIndex=0; ActorIndex<SortedActorList.Num(); ActorIndex++ )
			{
				AActor* Actor = SortedActorList[ActorIndex];
				if( !Actor->IsNetStartupActor() )
				{
					// Calculate time actor has been alive for. Certain actors can be spawned before TimeSeconds is valid
					// so we manually reset them to the same time as TimeSeconds.
					float TimeAlive	= TimeSeconds - Actor->CreationTime;
					if( TimeAlive < 0 )
					{
						TimeAlive = TimeSeconds;
					}
					const float TimeUnseen = TimeSeconds - Actor->GetLastRenderTime();
					const float DistanceToPlayer = FVector::Dist( Actor->GetActorLocation(), PlayerLocation );
					Ar.Logf(TEXT("%6.2f,%6.2f,%8.0f,%s,%s,%s"),TimeUnseen,TimeAlive,DistanceToPlayer,*Actor->GetClass()->GetName(),*Actor->GetName(),*GetNameSafe(Actor->GetOwner()));
				}
			}
		}
		else
		{
			// Iterate over all non-static actors and log detailed information.
			Ar.Logf(TEXT("TimeAlive,Class,Name,Owner"));
			for( int32 ActorIndex=0; ActorIndex<SortedActorList.Num(); ActorIndex++ )
			{
				AActor* Actor = SortedActorList[ActorIndex];
				if( !Actor->IsNetStartupActor() )
				{
					// Calculate time actor has been alive for. Certain actors can be spawned before TimeSeconds is valid
					// so we manually reset them to the same time as TimeSeconds.
					float TimeAlive	= TimeSeconds - Actor->CreationTime;
					if( TimeAlive < 0 )
					{
						TimeAlive = TimeSeconds;
					}
					Ar.Logf(TEXT("%6.2f,%s,%s,%s"),TimeAlive,*Actor->GetClass()->GetName(),*Actor->GetName(),*GetNameSafe(Actor->GetOwner()));
				}
			}
		}
	}
	else
	{
		Ar.Logf(TEXT("LISTSPAWNEDACTORS failed."));
	}
	return true;
}

bool UEngine::HandleMemReportCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	// This will defer the report to the end of the frame so we can force a GC and get a real report with no gcable objects
	GEngine->DeferredCommands.Add(FString::Printf(TEXT("MemReportDeferred %s"), Cmd ));

	return true;
}

bool UEngine::HandleMemReportDeferredCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	const bool bPerformSlowCommands = FParse::Param( Cmd, TEXT("FULL") );
	const bool bLogOutputToFile = !FParse::Param( Cmd, TEXT("LOG") );

	// Turn off as it makes diffing hard
	TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);

	// Flush rendering and do a GC
	FlushAsyncLoading();
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS,true);
	FlushRenderingCommands();

	FOutputDevice* ReportAr = &Ar;
	FOutputDeviceFile* FileAr = NULL;
	FString FilenameFull;
	
	if (bLogOutputToFile)
	{	
		const FString PathName = *(FPaths::ProfilingDir() + TEXT("MemReports/"));
		IFileManager::Get().MakeDirectory( *PathName );

		const FString Filename = CreateProfileFilename( TEXT(".memreport"), true );
		FilenameFull = PathName + Filename;
		FileAr = new FOutputDeviceFile(*FilenameFull);
		ReportAr = FileAr;

		UE_LOG(LogEngine, Log, TEXT("MemReportDeferred: saving to %s"), *FilenameFull);		
	}

	ReportAr->Logf( *FString::Printf( TEXT( "CommandLine Options: %s" ) LINE_TERMINATOR, FCommandLine::Get() ) );

	// Run commands from the ini
	FConfigSection* CommandsToRun = GConfig->GetSectionPrivate(TEXT("MemReportCommands"), 0, 1, GEngineIni);

	if (CommandsToRun)
	{
		for (FConfigSectionMap::TIterator It(*CommandsToRun); It; ++It)
		{
			Exec( InWorld, *It.Value(), *ReportAr );
			ReportAr->Logf( LINE_TERMINATOR );
		}
	}

	if (bPerformSlowCommands)
	{
		CommandsToRun = GConfig->GetSectionPrivate(TEXT("MemReportFullCommands"), 0, 1, GEngineIni);

		if (CommandsToRun)
		{
			for (FConfigSectionMap::TIterator It(*CommandsToRun); It; ++It)
			{
				Exec( InWorld, *It.Value(), *ReportAr );
				ReportAr->Logf( LINE_TERMINATOR );
			}
		}
	}

	if (FileAr)
	{
		FileAr->TearDown();
		// This no longer seems to work in UE4
		// SendDataToPCViaUnrealConsole( TEXT("UE_PROFILER!MEMREPORT:"), *(FilenameFull) );

		delete FileAr;
	}

	return true;
}


bool UEngine::HandleParticleMeshUsageCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Mapping from static mesh to particle systems using it.
	TMultiMap<UStaticMesh*,UParticleSystem*> StaticMeshToParticleSystemMap;
	// Unique array of referenced static meshes, used for sorting and index into map.
	TArray<UStaticMesh*> UniqueReferencedMeshes;

	// Iterate over all mesh modules to find and keep track of mesh to system mappings.
	for( TObjectIterator<UParticleModuleTypeDataMesh> It; It; ++It )
	{
		UStaticMesh* StaticMesh = It->Mesh;
		if( StaticMesh )
		{
			// Find particle system in outer chain.
			UParticleSystem* ParticleSystem = NULL;
			UObject* Outer = It->GetOuter();
			while( Outer && !ParticleSystem )
			{
				ParticleSystem = Cast<UParticleSystem>(Outer);
				Outer = Outer->GetOuter();
			}

			// Add unique mapping from static mesh to particle system.
			if( ParticleSystem )
			{
				StaticMeshToParticleSystemMap.AddUnique( StaticMesh, ParticleSystem );
				UniqueReferencedMeshes.AddUnique( StaticMesh );
			}
		}
	}

	struct FCompareUStaticMeshByResourceSize
	{
		FORCEINLINE bool operator()( UStaticMesh& A, UStaticMesh& B ) const
		{
			return B.GetResourceSize(EResourceSizeMode::Inclusive) < A.GetResourceSize(EResourceSizeMode::Inclusive);
		}
	};

	// Sort by resource size.
	UniqueReferencedMeshes.Sort( FCompareUStaticMeshByResourceSize() );

	// Calculate total size for summary.
	int32 TotalSize = 0;
	for( int32 StaticMeshIndex=0; StaticMeshIndex<UniqueReferencedMeshes.Num(); StaticMeshIndex++ )
	{
		UStaticMesh* StaticMesh	= UniqueReferencedMeshes[StaticMeshIndex];
		TotalSize += StaticMesh->GetResourceSize(EResourceSizeMode::Inclusive);
	}

	// Log sorted summary.
	Ar.Logf(TEXT("%5i KByte of static meshes referenced by particle systems:"),TotalSize / 1024);
	for( int32 StaticMeshIndex=0; StaticMeshIndex<UniqueReferencedMeshes.Num(); StaticMeshIndex++ )
	{
		UStaticMesh* StaticMesh	= UniqueReferencedMeshes[StaticMeshIndex];

		// Find all particle systems using this static mesh.
		TArray<UParticleSystem*> ParticleSystems;
		StaticMeshToParticleSystemMap.MultiFind( StaticMesh, ParticleSystems );

		// Log meshes including resource size and referencing particle systems.
		Ar.Logf(TEXT("%5i KByte  %s"), StaticMesh->GetResourceSize(EResourceSizeMode::Inclusive) / 1024, *StaticMesh->GetFullName());
		for( int32 ParticleSystemIndex=0; ParticleSystemIndex<ParticleSystems.Num(); ParticleSystemIndex++ )
		{
			UParticleSystem* ParticleSystem = ParticleSystems[ParticleSystemIndex];
			Ar.Logf(TEXT("             %s"),*ParticleSystem->GetFullName());
		}
	}

	return true;
}

class ParticleSystemUsage
{
public:
	UParticleSystem* Template;
	int32	Count;
	int32	ActiveTotal;
	int32	MaxActiveTotal;
	// Reported whether the emitters are instanced or not...
	int32	StoredMaxActiveTotal;

	TArray<int32>		EmitterActiveTotal;
	TArray<int32>		EmitterMaxActiveTotal;
	// Reported whether the emitters are instanced or not...
	TArray<int32>		EmitterStoredMaxActiveTotal;

	ParticleSystemUsage() :
		Template(NULL),
		Count(0),
		ActiveTotal(0),
		MaxActiveTotal(0),
		StoredMaxActiveTotal(0)
	{
	}

	~ParticleSystemUsage()
	{
	}
};

bool UEngine::HandleDumpParticleCountsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TMap<UParticleSystem*, ParticleSystemUsage> UsageMap;

	bool bTrackUsage = FParse::Command(&Cmd, TEXT("USAGE"));
	bool bTrackUsageOnly = FParse::Command(&Cmd, TEXT("USAGEONLY"));
	for( TObjectIterator<UObject> It; It; ++It )
	{
		UParticleSystemComponent* PSysComp = Cast<UParticleSystemComponent>(*It);
		if (PSysComp)
		{
			ParticleSystemUsage* Usage = NULL;

			if (bTrackUsageOnly == false)
			{
				Ar.Logf( TEXT("ParticleSystemComponent %s"), *(PSysComp->GetName()));
			}

			UParticleSystem* PSysTemplate = PSysComp->Template;
			if (PSysTemplate != NULL)
			{
				if (bTrackUsage || bTrackUsageOnly)
				{
					ParticleSystemUsage* pUsage = UsageMap.Find(PSysTemplate);
					if (pUsage == NULL)
					{
						ParticleSystemUsage TempUsage;
						TempUsage.Template = PSysTemplate;
						TempUsage.Count = 1;

						UsageMap.Add(PSysTemplate, TempUsage);
						Usage = UsageMap.Find(PSysTemplate);
						check(Usage);
					}					
					else
					{
						Usage = pUsage;
						Usage->Count++;
					}
				}
				if (bTrackUsageOnly == false)
				{
					Ar.Logf( TEXT("\tTemplate         : %s"), *(PSysTemplate->GetPathName()));
				}
			}
			else
			{
				if (bTrackUsageOnly == false)
				{
					Ar.Logf( TEXT("\tTemplate         : %s"), TEXT("NULL"));
				}
			}

			// Dump each emitter
			int32 TotalActiveCount = 0;
			if (bTrackUsageOnly == false)
			{
				Ar.Logf( TEXT("\tEmitterCount     : %d"), PSysComp->EmitterInstances.Num());
			}

			if (PSysComp->EmitterInstances.Num() > 0)
			{
				for (int32 EmitterIndex = 0; EmitterIndex < PSysComp->EmitterInstances.Num(); EmitterIndex++)
				{
					FParticleEmitterInstance* EmitInst = PSysComp->EmitterInstances[EmitterIndex];
					if (EmitInst)
					{
						UParticleLODLevel* LODLevel = EmitInst->SpriteTemplate ? EmitInst->SpriteTemplate->LODLevels[0] : NULL;
						if (bTrackUsageOnly == false)
						{
							Ar.Logf( TEXT("\t\tEmitter %2d:\tActive = %4d\tMaxActive = %4d"), 
								EmitterIndex, EmitInst->ActiveParticles, EmitInst->MaxActiveParticles);
						}
						TotalActiveCount += EmitInst->MaxActiveParticles;
						if (bTrackUsage || bTrackUsageOnly)
						{
							check(Usage);
							Usage->ActiveTotal += EmitInst->ActiveParticles;
							Usage->MaxActiveTotal += EmitInst->MaxActiveParticles;
							Usage->StoredMaxActiveTotal += EmitInst->MaxActiveParticles;
							if (Usage->EmitterActiveTotal.Num() <= EmitterIndex)
							{
								int32 CheckIndex;
								CheckIndex = Usage->EmitterActiveTotal.AddZeroed(1);
								check(CheckIndex == EmitterIndex);
								CheckIndex = Usage->EmitterMaxActiveTotal.AddZeroed(1);
								check(CheckIndex == EmitterIndex);
								CheckIndex = Usage->EmitterStoredMaxActiveTotal.AddZeroed(1);
								check(CheckIndex == EmitterIndex);
							}
							Usage->EmitterActiveTotal[EmitterIndex] = Usage->EmitterActiveTotal[EmitterIndex] + EmitInst->ActiveParticles;
							Usage->EmitterMaxActiveTotal[EmitterIndex] = Usage->EmitterMaxActiveTotal[EmitterIndex] + EmitInst->MaxActiveParticles;
							Usage->EmitterStoredMaxActiveTotal[EmitterIndex] = Usage->EmitterStoredMaxActiveTotal[EmitterIndex] + EmitInst->MaxActiveParticles;
						}
					}
					else
					{
						if (bTrackUsageOnly == false)
						{
							Ar.Logf( TEXT("\t\tEmitter %2d:\tActive = %4d\tMaxActive = %4d"), EmitterIndex, 0, 0);
						}
					}
				}
			}
			else
				if (PSysTemplate != NULL)
				{
					for (int32 EmitterIndex = 0; EmitterIndex < PSysTemplate->Emitters.Num(); EmitterIndex++)
					{
						UParticleEmitter* Emitter = PSysTemplate->Emitters[EmitterIndex];
						if (Emitter)
						{
							int32 MaxActive = 0;

							for (int32 LODIndex = 0; LODIndex < Emitter->LODLevels.Num(); LODIndex++)
							{
								UParticleLODLevel* LODLevel = Emitter->LODLevels[LODIndex];
								if (LODLevel)
								{
									if (LODLevel->PeakActiveParticles > MaxActive)
									{
										MaxActive = LODLevel->PeakActiveParticles;
									}
								}
							}

							if (bTrackUsage || bTrackUsageOnly)
							{
								check(Usage);
								Usage->StoredMaxActiveTotal += MaxActive;
								if (Usage->EmitterStoredMaxActiveTotal.Num() <= EmitterIndex)
								{
									int32 CheckIndex;
									CheckIndex = Usage->EmitterActiveTotal.AddZeroed(1);
									check(CheckIndex == EmitterIndex);
									CheckIndex = Usage->EmitterMaxActiveTotal.AddZeroed(1);
									check(CheckIndex == EmitterIndex);
									CheckIndex = Usage->EmitterStoredMaxActiveTotal.AddZeroed(1);
									check(CheckIndex == EmitterIndex);
								}
								// Don't update the non-stored entries...
								Usage->EmitterStoredMaxActiveTotal[EmitterIndex] = Usage->EmitterStoredMaxActiveTotal[EmitterIndex] + MaxActive;
							}
						}
					}
				}
				if (bTrackUsageOnly == false)
				{
					Ar.Logf( TEXT("\tTotalActiveCount : %d"), TotalActiveCount);
				}
		}
	}

	if (bTrackUsage || bTrackUsageOnly)
	{
		Ar.Logf( TEXT("PARTICLE USAGE DUMP:"));
		for (TMap<UParticleSystem*, ParticleSystemUsage>::TIterator It(UsageMap); It; ++It)
		{
			ParticleSystemUsage& Usage = It.Value();
			UParticleSystem* Template = Usage.Template;
			check(Template);

			Ar.Logf( TEXT("\tParticleSystem..%s"), *(Usage.Template->GetPathName()));
			Ar.Logf( TEXT("\t\tCount.....................%d"), Usage.Count);
			Ar.Logf( TEXT("\t\tActiveTotal...............%5d"), Usage.ActiveTotal);
			Ar.Logf( TEXT("\t\tMaxActiveTotal............%5d (%4d per instance)"), Usage.MaxActiveTotal, (Usage.MaxActiveTotal / Usage.Count));
			Ar.Logf( TEXT("\t\tPotentialMaxActiveTotal...%5d (%4d per instance)"), Usage.StoredMaxActiveTotal, (Usage.StoredMaxActiveTotal / Usage.Count));
			Ar.Logf( TEXT("\t\tEmitters..................%d"), Usage.EmitterActiveTotal.Num());
			check(Usage.EmitterActiveTotal.Num() == Usage.EmitterMaxActiveTotal.Num());
			for (int32 EmitterIndex = 0; EmitterIndex < Usage.EmitterActiveTotal.Num(); EmitterIndex++)
			{
				int32 EActiveTotal = Usage.EmitterActiveTotal[EmitterIndex];
				int32 EMaxActiveTotal = Usage.EmitterMaxActiveTotal[EmitterIndex];
				int32 EStoredMaxActiveTotal = Usage.EmitterStoredMaxActiveTotal[EmitterIndex];
				Ar.Logf( TEXT("\t\t\tEmitter %2d - AT = %5d, MT = %5d (%4d per emitter), Potential MT = %5d (%4d per emitter)"),
					EmitterIndex, EActiveTotal,
					EMaxActiveTotal, (EMaxActiveTotal / Usage.Count),
					EStoredMaxActiveTotal, (EStoredMaxActiveTotal / Usage.Count)
					);
			}
		}
	}
	return true;
}

bool UEngine::HandleListPreCacheMapPackagesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TArray<FString> Packages;
	ULinkerLoad::GetListOfPackagesInPackagePrecacheMap( Packages );

	Packages.Sort();

	Ar.Logf( TEXT( "Total Number Of Packages In PrecacheMap: %i " ), Packages.Num() );

	for( int32 i = 0; i < Packages.Num(); ++i )
	{
		Ar.Logf( TEXT( "%i %s" ), i, *Packages[i] );
	}
	Ar.Logf( TEXT( "Total Number Of Packages In PrecacheMap: %i " ), Packages.Num() );

	return true;
}

bool UEngine::HandleListLoadedPackagesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TArray<FString> Packages;

	for( TObjectIterator<UPackage> It; It; ++It )
	{
		UPackage* Package = *It;

		const bool bIsARootPackage = Package->GetOuter() == NULL;

		if( bIsARootPackage == true )
		{
			Packages.Add( Package->GetFullName() );
			//UE_LOG(LogParticle, Warning, TEXT("Package %s"), *Package->GetFullName() );
		}
	}

	Packages.Sort();

	Ar.Logf( TEXT( "Total Number Of Packages Loaded: %i " ), Packages.Num() );

	for( int32 i = 0; i < Packages.Num(); ++i )
	{
		Ar.Logf( TEXT( "%4i %s" ), i, *Packages[i] );
	}
	Ar.Logf( TEXT( "Total Number Of Packages Loaded: %i " ), Packages.Num() );

	return true;
}

bool UEngine::HandleMemCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	const FString Token = FParse::Token( Cmd, 0 );
	const bool bDetailed = ( Token == TEXT("DETAILED") || Token == TEXT("STAT"));
	const bool bReport = ( Token == TEXT("FROMREPORT") );

	if (!bReport)
	{
		// Mem report is called 
		FlushAsyncLoading();
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS,true);
		FlushRenderingCommands();
	}

#if !NO_LOGGING
	const FName CategoryName(LogMemory.GetCategoryName());
#else
	const FName CategoryName(TEXT("LogMemory"));
#endif
	FPlatformMemory::DumpStats( Ar );
	Ar.CategorizedLogf( CategoryName, ELogVerbosity::Log, TEXT("") );
	GMalloc->DumpAllocatorStats( Ar );

	if( bDetailed || bReport)
	{
		Ar.CategorizedLogf( CategoryName, ELogVerbosity::Log, TEXT("Memory Stats:") );
		Ar.CategorizedLogf( CategoryName, ELogVerbosity::Log, TEXT("FMemStack (gamethread) allocation size [used/ unused] = [%.2f / %.2f] MB"), FMemStack::Get().GetByteCount() / (1024.0f * 1024.0f), FMemStack::Get().GetUnusedByteCount() / (1024.0f * 1024.0f)  );
		Ar.CategorizedLogf( CategoryName, ELogVerbosity::Log, TEXT("Nametable memory usage = %.2f MB"), FName::GetNameTableMemorySize() / (1024.0f * 1024.0f) );

#if STATS
		TArray<FStatMessage> Stats;
		GetPermanentStats(Stats);

		FName NAME_STATGROUP_SceneMemory("STATGROUP_SceneMemory");
		FName NAME_STATGROUP_Memory("STATGROUP_Memory");
		FName NAME_STATGROUP_TextureGroup("STATGROUP_TextureGroup");
		FName NAME_STATGROUP_RHI("STATGROUP_RHI");

		for (int32 Index = 0; Index < Stats.Num(); Index++)
		{
			FStatMessage const& Meta = Stats[Index];
			FName LastGroup = Meta.NameAndInfo.GetGroupName();
			if ((LastGroup == NAME_STATGROUP_SceneMemory || LastGroup == NAME_STATGROUP_Memory || LastGroup == NAME_STATGROUP_TextureGroup || LastGroup == NAME_STATGROUP_RHI)  && Meta.NameAndInfo.GetFlag(EStatMetaFlags::IsMemory))
			{
				Ar.CategorizedLogf( CategoryName, ELogVerbosity::Log, TEXT("%s"), *FStatsUtils::DebugPrint(Meta));
			}
		}
#endif
	}

	return true;
}

// debug flag to allocate memory every frame, to trigger an OOM condition
static bool GDebugAllocMemEveryFrame = false;

bool UEngine::HandleDebugCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if( FParse::Command(&Cmd,TEXT("RENDERCRASH")) )
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND( CauseRenderThreadCrash, { UE_LOG(LogEngine, Warning, TEXT("Printed warning to log.") ); UE_LOG(LogEngine, Fatal, TEXT("Crashing the renderthread at your request") ); } );
		return true;
	}
	if( FParse::Command(&Cmd,TEXT("RENDERCHECK")) )
	{
		struct FFatal
		{
			static void Crash()
			{
				UE_LOG(LogEngine, Warning, TEXT("Printed warning to log.") );
				check(!"Crashing the renderthread via check(0) at your request");
			}
		};			
		ENQUEUE_UNIQUE_RENDER_COMMAND( CauseRenderThreadCrash, { FFatal::Crash();});
		return true;
	}
	if( FParse::Command(&Cmd,TEXT("RENDERGPF")) )
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND( CauseRenderThreadCrash, {UE_LOG(LogEngine, Warning, TEXT("Printed warning to log.") ); *(int32 *)3 = 123;} );
		return true;
	}
	if( FParse::Command(&Cmd,TEXT("THREADCRASH")) )
	{
		struct FFatal
		{
			static void Crash(ENamedThreads::Type, const  FGraphEventRef&)
			{
				UE_LOG(LogEngine, Warning, TEXT("Printed warning to log.") );
				UE_LOG(LogEngine, Fatal, TEXT("Crashing the worker thread at your request") );
			}
		};
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(FDelegateGraphTask::CreateAndDispatchWhenReady(FDelegateGraphTask::FDelegate::CreateStatic(FFatal::Crash), TEXT("Crash")), ENamedThreads::GameThread);
		return true;
	}
	if( FParse::Command(&Cmd,TEXT("THREADCHECK")) )
	{
		struct FFatal
		{
			static void Crash(ENamedThreads::Type, const  FGraphEventRef&)
			{
				UE_LOG(LogEngine, Warning, TEXT("Printed warning to log.") );
				check(!"Crashing a worker thread via check(0) at your request");
			}
		};
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(FDelegateGraphTask::CreateAndDispatchWhenReady(FDelegateGraphTask::FDelegate::CreateStatic(FFatal::Crash), TEXT("Crash")), ENamedThreads::GameThread);
		return true;
	}
	if( FParse::Command(&Cmd,TEXT("THREADGPF")) )
	{
		struct FFatal
		{
			static void Crash(ENamedThreads::Type, const  FGraphEventRef&)
			{
				UE_LOG(LogEngine, Warning, TEXT("Printed warning to log.") );
				*(int32 *)3 = 123;
			}
		};
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(FDelegateGraphTask::CreateAndDispatchWhenReady(FDelegateGraphTask::FDelegate::CreateStatic(FFatal::Crash), TEXT("Crash")), ENamedThreads::GameThread);
		return true;
	}
	else if( FParse::Command(&Cmd,TEXT("CRASH")) )
	{
		UE_LOG(LogEngine, Warning, TEXT("Printed warning to log.") );
		UE_LOG(LogEngine, Fatal, TEXT("%s"), TEXT("Crashing the gamethread at your request") );
		return true;
	}
	else if( FParse::Command(&Cmd,TEXT("CHECK")) )
	{
		UE_LOG(LogEngine, Warning, TEXT("Printed warning to log.") );
		check(!"Crashing the game thread via check(0) at your request");
		return true;
	}
	else if( FParse::Command( &Cmd, TEXT("GPF") ) )
	{
		UE_LOG(LogEngine, Warning, TEXT("Printed warning to log.") );
		Ar.Log( TEXT("Crashing with voluntary GPF") );
		// changed to 3 from NULL because clang noticed writing to NULL and warned about it
		*(int32 *)3 = 123;
		return true;
	}
	else if( FParse::Command( &Cmd, TEXT("ASSERT") ) )
	{
		UE_LOG(LogEngine, Warning, TEXT("Printed warning to log.") );
		check(0);
		return true;
	}
	else if( FParse::Command( &Cmd, TEXT("ENSURE") ) )
	{
		UE_LOG(LogEngine, Warning, TEXT("Printed warning to log.") );
		if( !ensure( 0 ) )
		{
			return true;
		}
	}
	else if( FParse::Command( &Cmd, TEXT("RESETLOADERS") ) )
	{
		ResetLoaders( NULL );
		return true;
	}
	else if( FParse::Command( &Cmd, TEXT("BUFFEROVERRUN") ) )
	{
		// stack overflow test - this case should be caught by /GS (Buffer Overflow Check) compile option
		ANSICHAR SrcBuffer[] = "12345678901234567890123456789012345678901234567890";
		BufferOverflowFunction(ARRAY_COUNT(SrcBuffer),SrcBuffer);
		return true;
	}
	else if( FParse::Command(&Cmd, TEXT("CRTINVALID")) )
	{
		FString::Printf(NULL);
		return true;
	}
	else if( FParse::Command(&Cmd, TEXT("HITCH")) )
	{
		SCOPE_CYCLE_COUNTER(STAT_IntentionalHitch);
		FPlatformProcess::Sleep(1.0f);
		return true;
	}
	else if( FParse::Command(&Cmd, TEXT("RENDERHITCH")) )
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND( CauseRenderThreadHitch, { SCOPE_CYCLE_COUNTER(STAT_IntentionalHitch); FPlatformProcess::Sleep(1.0f); } );
		return true;
	}
	else if ( FParse::Command(&Cmd,TEXT("LONGLOG")) )
	{
		UE_LOG(LogEngine, Log, TEXT("This is going to be a really long log message to test the code to resize the buffer used to log with. %02048s"), TEXT("HAHA, this isn't really a long string, but it sure has a lot of zeros!"));
	}
#if 0
	else if( FParse::Command( &Cmd, TEXT("RECURSE") ) )
	{
		Ar.Logf( TEXT("Recursing") );
		InfiniteRecursionFunction(1);
		return true;
	}
#endif
	else if( FParse::Command( &Cmd, TEXT("EATMEM") ) )
	{
		Ar.Log( TEXT("Eating up all available memory") );
		while( 1 )
		{
			void* Eat = FMemory::Malloc(65536);
			FMemory::Memset( Eat, 0, 65536 );
		}
		return true;
	}
	else if( FParse::Command( &Cmd, TEXT("OOM") ) )
	{
		Ar.Log( TEXT("Will continuously allocate 1MB per frame until we hit OOM") );
		GDebugAllocMemEveryFrame = true;
		return true;
	}

	return false;
}

bool UEngine::HandleContentComparisonCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TArray<FString> Tokens, Switches;
	FCommandLine::Parse(Cmd, Tokens, Switches);
	if (Tokens.Num() > 0)
	{
		// The first token MUST be the base class name of interest
		FString BaseClassName = Tokens[0];
		TArray<FString> BaseClassesToIgnore;
		int32 Depth = 1;
		for (int32 TokenIdx = 1; TokenIdx < Tokens.Num(); TokenIdx++)
		{
			FString Token = Tokens[TokenIdx];
			FString TempString;
			if (FParse::Value(*Token, TEXT("DEPTH="), TempString))
			{
				Depth = FCString::Atoi(*TempString);
			}
			else
			{
				BaseClassesToIgnore.Add(Token);
				UE_LOG(LogEngine, Log, TEXT("Added ignored base class: %s"), *Token);
			}
		}

		UE_LOG(LogEngine, Log, TEXT("Calling CompareClasses w/ Depth of %d on %s"), Depth, *BaseClassName);
		UE_LOG(LogEngine, Log, TEXT("Ignoring base classes:"));
		for (int32 DumpIdx = 0; DumpIdx < BaseClassesToIgnore.Num(); DumpIdx++)
		{
			UE_LOG(LogEngine, Log, TEXT("\t%s"), *(BaseClassesToIgnore[DumpIdx]));
		}
		FContentComparisonHelper ContentComparisonHelper;
		ContentComparisonHelper.CompareClasses(BaseClassName, BaseClassesToIgnore, Depth);
	}
	return true;
}

bool UEngine::HandleTogglegtPsysLODCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	extern bool GbEnableGameThreadLODCalculation;
		GbEnableGameThreadLODCalculation = !GbEnableGameThreadLODCalculation;
		UE_LOG(LogEngine, Warning, TEXT("Particle LOD determination is now on the %s thread!"),
			GbEnableGameThreadLODCalculation ? TEXT("GAME") : TEXT("RENDER"));
	return true;
}

struct FHierarchyNode
{
	UObject* This;
	UObject* Parent;
	TSet<UObject*> Children;
	TSet<UObject*> Items;
	int64 Inc;
	int64 Exc;
	int32 IncCnt;
	int32 ExcCnt;
	FHierarchyNode()
		: This(NULL)
		, Parent(NULL)
		, Inc(-1)
		, Exc(-1)
		, IncCnt(-1)
		, ExcCnt(-1)
	{
	}
	bool operator< (FHierarchyNode const& Other) const
	{
		return Inc > Other.Inc;
	}

	bool IsLeaf()
	{
		return Children.Num() + Items.Num() == 0;
	}
};

struct FHierarchy
{
	int64 Limit;
	TMap<UObject*, FHierarchyNode> Nodes;

	FHierarchy(int32 InLimit)
		: Limit(InLimit)
	{
	}
	FHierarchyNode& AddFlat(UObject* This)
	{
		FHierarchyNode& Node = Nodes.FindOrAdd(This);
		if (!Node.This && This)
		{
			Node.This = This;
			Node.Parent = NULL;
			AddFlat(NULL).Children.Add(This);
		}
		return Node;
	}
	FHierarchyNode& AddOuter(UObject* This)
	{
		FHierarchyNode& Node = Nodes.FindOrAdd(This);
		if (!Node.This && This)
		{
			Node.This = This;
			Node.Parent = This->GetOuter();
			AddOuter(Node.Parent).Children.Add(This);
		}
		return Node;
	}
	FHierarchyNode& AddClass(UClass* This)
	{
		FHierarchyNode& Node = Nodes.FindOrAdd(This);
		if (!Node.This && This)
		{
			Node.This = This;
			Node.Parent = This->GetSuperClass();
			AddClass(This->GetSuperClass()).Children.Add(This);
		}
		return Node;
	}
	void AddClassInstance(UObject* This)
	{
		if (!This->IsA(UClass::StaticClass()))
		{
			AddClass(This->GetClass()).Items.Add(This);
			FHierarchyNode& Node = Nodes.FindOrAdd(This);
			if (!Node.This)
			{
				Node.This = This;
				Node.Parent = This->GetClass();
			}
		}
		else
		{
			AddClass((UClass*)This);
		}
	}
	FHierarchyNode& Compute(UObject* This, TMap<UObject*, FSubItem> const& Objects, bool bCntItems)
	{
		FHierarchyNode& Node = Nodes.FindChecked(This);
		if (Node.Inc < 0)
		{
			Node.Exc = 0;
			Node.ExcCnt = 1;
			if (This)
			{
				FSubItem const& Item = Objects.FindChecked(This);
				Node.Exc += Item.Max;
				Node.Exc += Item.TrueRes;
				if (bCntItems)
				{
					Node.ExcCnt += Node.Items.Num();
				}
				else
				{
					Node.ExcCnt += Node.Children.Num();
				}
			}
			Node.Inc = Node.Exc;
			Node.IncCnt = Node.ExcCnt;
			for (TSet<UObject*>::TConstIterator It(Node.Children); It; ++It)
			{
				FHierarchyNode& Child = Compute(*It, Objects, bCntItems);
				Node.Inc += Child.Inc;
				if (!bCntItems)
				{
					Node.IncCnt += Child.IncCnt;
				}
			}
			for (TSet<UObject*>::TConstIterator It(Node.Items); It; ++It)
			{
				FHierarchyNode& Child = Compute(*It, Objects, bCntItems);
				Node.Inc += Child.Inc;
				if (bCntItems)
				{
					Node.IncCnt += Child.IncCnt;
				}
			}
		}
		return Node;
	}
	void SortSet(TSet<UObject*> const& In, TArray<FHierarchyNode>& Out)
	{
		Out.Empty(In.Num());
		for (TSet<UObject*>::TConstIterator It(In); It; ++It)
		{
			Out.Add(Nodes.FindChecked(*It));
		}
		Out.Sort();
	}
	FString Size(uint64 Mem)
	{
		if (Mem / 1024 < 10000)
		{
			return FString::Printf(TEXT("%4lldK"), Mem / 1024);
		}
		if (Mem / (1024*1024) < 10000)
		{
			return FString::Printf(TEXT("%4lldM"), Mem / (1024*1024));
		}
		return FString::Printf(TEXT("%4lldG"), Mem / (1024*1024*1024));
	}
	void LogSet(TSet<UObject*> const& In, bool bCntItems, int Indent)
	{
		TArray<FHierarchyNode> Children;
		SortSet(In, Children);
		int32 Index = 0;
		for (; Index < Children.Num(); Index++)
		{
			if (!Log(Children[Index].This, bCntItems, Indent + 1, Index + 1 < Children.Num()))
			{
				break;
			}
		}
		if (Index < Children.Num())
		{
			int32 NumExtra = 0;
			FHierarchyNode Extra;
			Extra.Exc = 0;
			Extra.Inc = 0;
			Extra.ExcCnt = 0;
			Extra.IncCnt = 0;
			for (; Index < Children.Num(); Index++)
			{
				Extra.Exc += Children[Index].Exc;
				Extra.Inc += Children[Index].Inc;
				Extra.ExcCnt += Children[Index].ExcCnt;
				Extra.IncCnt += Children[Index].IncCnt;
				NumExtra++;
			}
			FString Line = FString::Printf(TEXT("%s        %5d %s (%d)"), *Size(Extra.Inc), Extra.IncCnt, TEXT("More"), NumExtra);
			UE_LOG(LogEngine, Log, TEXT("%s%s") , FCString::Spc(2 * (Indent + 1)), *Line);
		}
	}
	bool Log(UObject* This, bool bCntItems, int Indent = 0, bool bAllowCull = true)
	{
		FHierarchyNode& Node = Nodes.FindChecked(This);
		if (bAllowCull && Node.Inc < Limit && Node.Exc < Limit)
		{
			return false;
		}
		FString Line;
		if (Node.IsLeaf())
		{
			Line = FString::Printf(TEXT("%s        %5d %s"), *Size(Node.Inc), Node.IncCnt, Node.This ? *Node.This->GetFullName() : TEXT("Root"));
			UE_LOG(LogEngine, Log, TEXT("%s%s") , FCString::Spc(2 * Indent), *Line);
		}
		else
		{
			Line = FString::Printf(TEXT("%s %sx %5d %s"), *Size(Node.Inc), *Size(Node.Exc), Node.IncCnt, Node.This ? *Node.This->GetFullName() : TEXT("Root"));
			UE_LOG(LogEngine, Log, TEXT("%s%s") , FCString::Spc(2 * Indent), *Line);
			if (bCntItems && Node.Children.Num())
			{
				UE_LOG(LogEngine, Log, TEXT("%s%s") , FCString::Spc(2 * (Indent + 1)), TEXT("Child Classes"));
			}
			LogSet(Node.Children, bCntItems, Indent + 2);

			if (bCntItems && Node.Items.Num())
			{
				UE_LOG(LogEngine, Log, TEXT("%s%s") , FCString::Spc(2 * (Indent + 1)), TEXT("Instances"));
			}
			LogSet(Node.Items, bCntItems, Indent);
		}

		return true;
	}
};

bool UEngine::HandleObjCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if (FParse::Command(&Cmd,TEXT("LIST2")))
	{			
		UClass* ClassToCheck = NULL;
		ParseObject<UClass>(Cmd, TEXT("CLASS="  ), ClassToCheck, ANY_PACKAGE );

		if (ClassToCheck == NULL)
		{
			ClassToCheck = UObject::StaticClass();
		}

		FObjectMemoryAnalyzer MemAnalyze(ClassToCheck);
		MemAnalyze.PrintResults(Ar, FObjectMemoryAnalyzer::EPrintFlags::PrintReferences);
		return true;
	}
	if (FParse::Command(&Cmd,TEXT("Mem")))
	{			

		int32 Limit = 50;
		FParse::Value(Cmd, TEXT("CULL="), Limit);
		Limit *= 1024;

		FHierarchy Classes(Limit);
		FHierarchy Outers(Limit);
		FHierarchy Flat(Limit);

		TMap<UObject*, FSubItem> Objects;
		for( FObjectIterator It; It; ++It )
		{
			FArchiveCountMem Count( *It );
			// Get the 'old-style' resource size and the truer resource size
			const SIZE_T ResourceSize = It->GetResourceSize(EResourceSizeMode::Inclusive);
			const SIZE_T TrueResourceSize = It->GetResourceSize(EResourceSizeMode::Exclusive);
			Objects.Add(*It, FSubItem(*It, Count.GetNum(), Count.GetMax(), ResourceSize, TrueResourceSize));
			Classes.AddClassInstance(*It);
			Outers.AddOuter(*It);
			Flat.AddFlat(*It);
		}
		UE_LOG(LogEngine, Log, TEXT("********************************************** By Outer Hierarchy") );
		Outers.Compute(NULL, Objects, false);
		Outers.Log(NULL, false);

		UE_LOG(LogEngine, Log, TEXT("********************************************** By Class Hierarchy") );
		Classes.Compute(NULL, Objects, true);
		Classes.Log(NULL, true);
		UE_LOG(LogEngine, Log, TEXT("********************************************** Flat") );
		Flat.Compute(NULL, Objects, false);
		Flat.Log(NULL, false);
		UE_LOG(LogEngine, Log, TEXT("**********************************************") );

		return true;
	}
	else if( FParse::Command(&Cmd,TEXT("LIST")) )
	{
		FString CmdLineOut = FString::Printf(TEXT("Obj List: %s"), Cmd);
		Ar.Log( *CmdLineOut );
		Ar.Log( TEXT("Objects:") );
		Ar.Log( TEXT("") );

		UClass*   CheckType     = NULL;
		UClass*   MetaClass		= NULL;
		const bool bExportToFile = FParse::Param(Cmd,TEXT("FILE"));

		// allow checking for any Outer, not just a UPackage
		UObject* CheckOuter = NULL;
		UPackage* InsidePackage = NULL;
		UObject* InsideObject = NULL;
		ParseObject<UClass>(Cmd, TEXT("CLASS="  ), CheckType, ANY_PACKAGE );
		ParseObject<UObject>(Cmd, TEXT("OUTER="), CheckOuter, ANY_PACKAGE);

		ParseObject<UPackage>(Cmd, TEXT("PACKAGE="), InsidePackage, NULL);
		if ( InsidePackage == NULL )
		{
			ParseObject<UObject>( Cmd, TEXT("INSIDE=" ), InsideObject, NULL );
		}
		int32 Depth = -1;
		FParse::Value(Cmd, TEXT("DEPTH="), Depth);

		FString ObjectName;
		FParse::Value(Cmd, TEXT("NAME="), ObjectName);

		TArray<FItem> List;
		TArray<FSubItem> Objects;
		FItem Total;

		// support specifying metaclasses when listing class objects
		if ( CheckType && CheckType->IsChildOf(UClass::StaticClass()) )
		{
			ParseObject<UClass>  ( Cmd, TEXT("TYPE="   ), MetaClass,     ANY_PACKAGE );
		}

		const bool bAll				= FParse::Param( Cmd, TEXT("ALL") );

		// if we specified a parameter in the command, but no objects of that parameter were found,
		// and they didn't specify "all", then don't list all objects
		if ( bAll ||
			((CheckType		||	!FCString::Strfind(Cmd,TEXT("CLASS=")))
			&&	(MetaClass		||	!FCString::Strfind(Cmd,TEXT("TYPE=")))
			&&	(CheckOuter		||	!FCString::Strfind(Cmd,TEXT("OUTER=")))
			&&	(InsidePackage	||	!FCString::Strfind(Cmd,TEXT("PACKAGE="))) 
			&&	(InsideObject	||	!FCString::Strfind(Cmd,TEXT("INSIDE=")))))
		{
			const bool bTrackDetailedObjectInfo		= bAll || (CheckType != NULL && CheckType != UObject::StaticClass()) || CheckOuter != NULL || InsideObject != NULL || InsidePackage != NULL || !ObjectName.IsEmpty();
			const bool bOnlyListGCObjects				= FParse::Param( Cmd, TEXT("GCONLY") );
			const bool bOnlyListRootObjects				= FParse::Param( Cmd, TEXT("ROOTONLY") );
			const bool bShouldIncludeDefaultObjects	= FParse::Param( Cmd, TEXT("INCLUDEDEFAULTS") );
			const bool bOnlyListDefaultObjects			= FParse::Param( Cmd, TEXT("DEFAULTSONLY") );
			const bool bShowDetailedObjectInfo			= FParse::Param( Cmd, TEXT("NODETAILEDINFO") ) == false && bTrackDetailedObjectInfo;

			for( FObjectIterator It; It; ++It )
			{
				if (It->IsTemplate(RF_ClassDefaultObject))
				{
					if( !bShouldIncludeDefaultObjects )
					{
						continue;
					}
				}
				else if( bOnlyListDefaultObjects )
				{
					continue;
				}

				if ( bOnlyListGCObjects && GUObjectArray.IsDisregardForGC(*It) )
				{
					continue;
				}

				if ( bOnlyListRootObjects && !It->IsRooted() )
				{
					continue;
				}

				if ( CheckType && !It->IsA(CheckType) )
				{
					continue;
				}

				if ( CheckOuter && It->GetOuter() != CheckOuter )
				{
					continue;
				}

				if ( InsidePackage && !It->IsIn(InsidePackage) )
				{
					continue;
				}

				if ( InsideObject && !It->IsIn(InsideObject) )
				{
					continue;
				}

				if (!ObjectName.IsEmpty() && It->GetName() != ObjectName)
				{
					continue;
				}

				if ( MetaClass )
				{
					UClass* ClassObj = Cast<UClass>(*It);
					if ( ClassObj && !ClassObj->IsChildOf(MetaClass) )
					{
						continue;
					}
				}

				FArchiveCountMem Count( *It );

				// Get the 'old-style' resource size and the truer resource size
				const SIZE_T ResourceSize = It->GetResourceSize(EResourceSizeMode::Inclusive);
				const SIZE_T TrueResourceSize = It->GetResourceSize(EResourceSizeMode::Exclusive);

				int32 i;

				// which class are we going to file this object under? by default, it's class
				UClass* ClassToUse = It->GetClass();
				// if we specified a depth to use, then put this object into the class Depth away from Object
				if (Depth != -1)
				{
					UClass* Travel = ClassToUse;
					// go up the class hierarchy chain, using a trail pointer Depth away
					for (int32 Up = 0; Up < Depth && Travel != UObject::StaticClass(); Up++)
					{
						Travel = Travel->GetSuperClass();
					}
					// when travel is a UObject, ClassToUse will be pointing to a class Depth away
					while (Travel != UObject::StaticClass())
					{
						Travel = Travel->GetSuperClass();
						ClassToUse = ClassToUse->GetSuperClass();
					}
				}

				for( i=0; i<List.Num(); i++ )
				{
					if( List[i].Class == ClassToUse )
					{
						break;
					}
				}
				if( i==List.Num() )
				{
					i = List.Add(FItem( ClassToUse ));
				}

				if( bShowDetailedObjectInfo )
				{
					new(Objects)FSubItem( *It, Count.GetNum(), Count.GetMax(), ResourceSize, TrueResourceSize );
				}
				List[i].Add( Count, ResourceSize, TrueResourceSize );
				Total.Add( Count, ResourceSize, TrueResourceSize );
			}
		}

		const bool bAlphaSort = FParse::Param( Cmd, TEXT("ALPHASORT") );
		const bool bCountSort = FParse::Param( Cmd, TEXT("COUNTSORT") );

		if( Objects.Num() )
		{
			struct FCompareFSubItem
			{
				bool bAlphaSort;
				FCompareFSubItem( bool InAlphaSort )
					: bAlphaSort( InAlphaSort )
				{}

				FORCEINLINE bool operator()( const FSubItem& A, const FSubItem& B ) const
				{
					return bAlphaSort ? (A.Object->GetPathName() < B.Object->GetPathName()) : (B.Max < A.Max);
				}
			};
			Objects.Sort( FCompareFSubItem( bAlphaSort ) );

			Ar.Logf( TEXT("%140s % 10s % 10s % 10s % 10s"), TEXT("Object"), TEXT("NumKBytes"), TEXT("MaxKBytes"), TEXT("ResKBytes"), TEXT("ExclusiveResKBytes") );

			for( int32 ObjIndex=0; ObjIndex<Objects.Num(); ObjIndex++ )
			{
				const FSubItem& ObjItem = Objects[ObjIndex];

				///MSSTART DAN PRICE MICROSOFT Mar 12th, 2007 export object data to a file
				if(bExportToFile)
				{
					FString Path = TEXT("./ObjExport");
					FString MungedPath = ObjItem.Object->GetOutermost()->GetName();
					MungedPath.ReplaceInline(TEXT("/"), TEXT("_"));
					const FString Filename = Path /  + TEXT(".") + MungedPath / ObjItem.Object->GetName() + TEXT(".t3d");
					Ar.Logf( TEXT("%s"),*Filename);
					UExporter::ExportToFile(ObjItem.Object, NULL, *Filename, 1, 0);
				}					
				//MSEND

				Ar.Logf( TEXT("%140s % 10iK % 10iK % 10iK % 10iK"), *ObjItem.Object->GetFullName(), (int32)ObjItem.Num / 1024, (int32)ObjItem.Max / 1024, (int32)ObjItem.Res / 1024, (int32)ObjItem.TrueRes / 1024 );
			}
			Ar.Log( TEXT("") );
		}
		if( List.Num() )
		{
			struct FCompareFItem
			{
				bool bAlphaSort, bCountSort;
				FCompareFItem( bool InAlphaSort, bool InCountSort )
					: bAlphaSort( InAlphaSort )
					, bCountSort( InCountSort )
				{}
				FORCEINLINE bool operator()( const FItem& A, const FItem& B ) const
				{
					return bAlphaSort ? (A.Class->GetName() < B.Class->GetName()) : bCountSort ? (B.Count < A.Count) : (B.Max < A.Max); 
				}
			};
			List.Sort( FCompareFItem( bAlphaSort, bCountSort ) );
			Ar.Logf(TEXT(" %100s % 6s % 10s % 10s % 10s % 10s"), TEXT("Class"), TEXT("Count"), TEXT("NumKBytes"), TEXT("MaxKBytes"), TEXT("ResKBytes"), TEXT("ExclusiveResKBytes") );

			for( int32 i=0; i<List.Num(); i++ )
			{
				Ar.Logf(TEXT(" %100s % 6i % 10iK % 10iK % 10iK % 10iK"), *List[i].Class->GetName(), (int32)List[i].Count, (int32)(List[i].Num/1024), (int32)(List[i].Max/1024), (int32)(List[i].Res/1024), (int32)(List[i].TrueRes/1024) );
			}
			Ar.Log( TEXT("") );
		}
		Ar.Logf( TEXT("%i Objects (%.3fM / %.3fM / %.3fM / %.3fM)"), Total.Count, (float)Total.Num/1024.0/1024.0, (float)Total.Max/1024.0/1024.0, (float)Total.Res/1024.0/1024.0, (float)Total.TrueRes/1024.0/1024.0 );
		return true;

	}
	else if ( FParse::Command(&Cmd,TEXT("COMPONENTS")) )
	{
		UObject* Obj=NULL;
		FString ObjectName;

		if ( FParse::Token(Cmd,ObjectName,true) )
		{
			Obj = FindObject<UObject>(ANY_PACKAGE,*ObjectName);

			if ( Obj != NULL )
			{
				Ar.Log(TEXT(""));
				DumpComponents(Obj);
				Ar.Log(TEXT(""));
			}
			else
			{
				Ar.Logf(TEXT("No objects found named '%s'"), *ObjectName);
			}
		}
		else
		{
			Ar.Logf(TEXT("Syntax: OBJ COMPONENTS <Name Of Object>"));
		}
		return true;
	}
	else if ( FParse::Command(&Cmd,TEXT("DUMP")) )
	{
		// Dump all variable values for the specified object
		// supports specifying categories to hide or show
		// OBJ DUMP playercontroller0 hide="actor,object,lighting,movement"     OR
		// OBJ DUMP playercontroller0 show="playercontroller,controller"        OR
		// OBJ DUMP class=playercontroller name=playercontroller0 show=object OR
		// OBJ DUMP playercontroller0 recurse=true
		TCHAR ObjectName[1024];
		UObject* Obj = NULL;
		UClass* Cls = NULL;

		TArray<FString> HiddenCategories, ShowingCategories;

		if ( !ParseObject<UClass>( Cmd, TEXT("CLASS="), Cls, ANY_PACKAGE ) || !ParseObject(Cmd,TEXT("NAME="), Cls, Obj, ANY_PACKAGE) )
		{
			if ( FParse::Token(Cmd,ObjectName,ARRAY_COUNT(ObjectName), 1) )
			{
				Obj = FindObject<UObject>(ANY_PACKAGE,ObjectName);
			}
		}

		if ( Obj )
		{
			if ( Cast<UClass>(Obj) != NULL )
			{
				Obj = Cast<UClass>(Obj)->GetDefaultObject();
			}

			FString Value;

			Ar.Logf(TEXT(""));

			const bool bRecurse = FParse::Value(Cmd, TEXT("RECURSE=true"), Value);
			Ar.Logf(TEXT("*** Property dump for object %s'%s' ***"), bRecurse ? TEXT("(Recursive) ") : TEXT(""), *Obj->GetFullName() );

			if ( bRecurse )
			{
				const FExportObjectInnerContext Context;
				ExportProperties( &Context, Ar, Obj->GetClass(), (uint8*)Obj, 0, Obj->GetArchetype()->GetClass(), (uint8*)Obj->GetArchetype(), Obj, PPF_IncludeTransient );
			}
			else
			{
#if WITH_EDITORONLY_DATA
				//@todo: add support to FParse::Value() for specifying characters that should be ignored
				if ( FParse::Value(Cmd, TEXT("HIDE="), Value/*, TEXT(",")*/) )
				{
					Value.ParseIntoArray(&HiddenCategories,TEXT(","),1);
				}
				else if ( FParse::Value(Cmd, TEXT("SHOW="), Value/*, TEXT(",")*/) )
				{
					Value.ParseIntoArray(&ShowingCategories,TEXT(","),1);
				}
#endif
				UClass* LastOwnerClass = NULL;
				for ( TFieldIterator<UProperty> It(Obj->GetClass()); It; ++It )
				{
					Value.Empty();
#if WITH_EDITORONLY_DATA
					if ( HiddenCategories.Num() )
					{
						const FString Category = FObjectEditorUtils::GetCategory(*It);
						int32 i;
						for ( i = 0; i < HiddenCategories.Num(); i++ )
						{
							if ( !Category.IsEmpty() && HiddenCategories[i] == Category )
							{
								break;
							}

							if ( HiddenCategories[i] == *It->GetOwnerClass()->GetName() )
							{
								break;
							}
						}

						if ( i < HiddenCategories.Num() )
						{
							continue;
						}
					}
					else if ( ShowingCategories.Num() )
					{
						const FString Category = FObjectEditorUtils::GetCategory(*It);
						int32 i;
						for ( i = 0; i < ShowingCategories.Num(); i++ )
						{
							if ( !Category.IsEmpty() && ShowingCategories[i] == Category )
							{
								break;
							}

							if ( ShowingCategories[i] == *It->GetOwnerClass()->GetName() )
							{
								break;
							}
						}

						if ( i == ShowingCategories.Num() )
						{
							continue;
						}
					}
#endif // #if WITH_EDITORONLY_DATA
					if ( LastOwnerClass != It->GetOwnerClass() )
					{
						LastOwnerClass = It->GetOwnerClass();
						Ar.Logf(TEXT("=== %s properties ==="), *LastOwnerClass->GetName());
					}

					if ( It->ArrayDim > 1 )
					{
						for ( int32 i = 0; i < It->ArrayDim; i++ )
						{
							Value.Empty();
							It->ExportText_InContainer(i, Value, Obj, Obj, Obj, PPF_IncludeTransient);
							Ar.Logf(TEXT("  %s[%i]=%s"), *It->GetName(), i, *Value);
						}
					}
					else
					{
						UArrayProperty* ArrayProp = Cast<UArrayProperty>(*It);
						if ( ArrayProp != NULL )
						{
							FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, Obj);
							for( int32 i=0; i<FMath::Min(ArrayHelper.Num(),100); i++ )
							{
								Value.Empty();
								ArrayProp->Inner->ExportTextItem( Value, ArrayHelper.GetRawPtr(i), ArrayHelper.GetRawPtr(i), Obj, PPF_IncludeTransient );
								Ar.Logf(TEXT("  %s(%i)=%s"), *ArrayProp->GetName(), i, *Value);
							}

							if ( ArrayHelper.Num() >= 100 )
							{
								Ar.Logf(TEXT("  ... %i more elements"), ArrayHelper.Num() - 99);
							}
						}
						else
						{
							It->ExportText_InContainer(0, Value, Obj, Obj, Obj, PPF_IncludeTransient);
							Ar.Logf(TEXT("  %s=%s"), *It->GetName(), *Value);
						}
					}
				}
			}

			TMap<FString,FString> NativePropertyValues;
			if ( Obj->GetNativePropertyValues(NativePropertyValues) )
			{
				int32 LargestKey = 0;
				for ( TMap<FString,FString>::TIterator It(NativePropertyValues); It; ++It )
				{
					LargestKey = FMath::Max(LargestKey, It.Key().Len());
				}

				Ar.Log(TEXT("=== Native properties ==="));
				for ( TMap<FString,FString>::TIterator It(NativePropertyValues); It; ++It )
				{
					Ar.Logf(TEXT("  %s%s"), *It.Key().RightPad(LargestKey), *It.Value());
				}
			}
		}
		else
		{
			UE_SUPPRESS(LogExec, Warning, Ar.Logf(TEXT("No objects found using command '%s'"), Cmd));
		}

		return true;
	}
	else
	{
		// OBJ command but not supported here
		return false;
	}
	return false;
}

bool UEngine::HandleDirCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TArray<FString> Files;
	TArray<FString> Directories;

	IFileManager::Get().FindFiles( Files, Cmd, 1, 0 );
	IFileManager::Get().FindFiles( Directories, Cmd, 0, 1 );

	// Directories
	Directories.Sort();
	for( int32 x = 0 ; x < Directories.Num() ; x++ )
	{
		Ar.Logf( TEXT("[%s]"), *Directories[x] );
	}

	// Files
	Files.Sort();
	for( int32 x = 0 ; x < Files.Num() ; x++ )
	{
		Ar.Logf( TEXT("[%s]"), *Files[x] );
	}

	return true;
}

bool UEngine::HandleTrackParticleRenderingStatsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	extern float GTimeBetweenParticleRenderStatCaptures;
	extern float GMinParticleDrawTimeToTrack;
	FString FlagStr(FParse::Token(Cmd, 0));
	if (FlagStr.Len() > 0)
	{
		GTimeBetweenParticleRenderStatCaptures = FCString::Atof(*FlagStr);
	}

	FString FlagStr2(FParse::Token(Cmd, 0));
	if (FlagStr2.Len() > 0)
	{
		GMinParticleDrawTimeToTrack = FCString::Atof(*FlagStr2);
	}

	extern bool GTrackParticleRenderingStats;
	GTrackParticleRenderingStats = !GTrackParticleRenderingStats;
	if (GTrackParticleRenderingStats)
	{
		if (GetCachedScalabilityCVars().DetailMode == DM_High)
		{
			Ar.Logf(TEXT("Currently in high detail mode, note that particle stats will only be captured in medium or low detail modes (eg splitscreen)."));
		}
		Ar.Logf(TEXT("Enabled particle render stat tracking with %.1fs between captures, min tracked time of %.4fs, use DUMPPARTICLERENDERINGSTATS to save results."),
			GTimeBetweenParticleRenderStatCaptures, GMinParticleDrawTimeToTrack);
	}
	else
	{
		Ar.Logf(TEXT("Disabled particle render stat tracking."));
	}
	return 1;
}

bool UEngine::HandleDumpParticleRenderingStatsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	extern void DumpParticleRenderingStats(FOutputDevice& Ar);
	DumpParticleRenderingStats(Ar);
	return 1;
}

bool UEngine::HandleDumpParticleFrameRenderingStatsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	extern bool GWantsParticleStatsNextFrame;
	GWantsParticleStatsNextFrame = true;
	UE_LOG(LogEngine, Warning, TEXT("DUMPPARTICLEFRAMERENDERINGSTATS triggered"));
	return 1;
}

bool UEngine::HandleDumpAllocatorStats( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GMalloc->DumpAllocatorStats(Ar);
	return true;
}

bool UEngine::HandleHeapCheckCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GMalloc->ValidateHeap();
	return true;
}

bool UEngine::HandleToggleOnscreenDebugMessageDisplayCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GEngine->bEnableOnScreenDebugMessagesDisplay = !GEngine->bEnableOnScreenDebugMessagesDisplay;
	UE_LOG(LogEngine, Log, TEXT("OnScreenDebug Message Display is now %s"), 
		GEngine->bEnableOnScreenDebugMessagesDisplay ? TEXT("ENABLED") : TEXT("DISABLED"));
	if ((GEngine->bEnableOnScreenDebugMessagesDisplay == true) && (GEngine->bEnableOnScreenDebugMessages == false))
	{
		UE_LOG(LogEngine, Log, TEXT("OnScreenDebug Message system is DISABLED!"));
	}
	return true;
}

bool UEngine::HandleToggleOnscreenDebugMessageSystemCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GEngine->bEnableOnScreenDebugMessages = !GEngine->bEnableOnScreenDebugMessages;
	UE_LOG(LogEngine, Log, TEXT("OnScreenDebug Message System is now %s"), 
		GEngine->bEnableOnScreenDebugMessages ? TEXT("ENABLED") : TEXT("DISABLED"));
	return true;
}

bool UEngine::HandleDisableAllScreenMessagesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GAreScreenMessagesEnabled = false;
	UE_LOG(LogEngine, Log, TEXT("Onscreen warnings/messages are now DISABLED"));
	return true;
}


bool UEngine::HandleEnableAllScreenMessagesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GAreScreenMessagesEnabled = true;
	UE_LOG(LogEngine, Log, TEXT("Onscreen warngins/messages are now ENABLED"));
	return true;
}


bool UEngine::HandleToggleAllScreenMessagesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GAreScreenMessagesEnabled = !GAreScreenMessagesEnabled;
	UE_LOG(LogEngine, Log, TEXT("Onscreen warngins/messages are now %s"),
		GAreScreenMessagesEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));
	return true;
}

#endif // !UE_BUILD_SHIPPING

bool UEngine::HandleCeCommand( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	const TCHAR* ErrorMessage = TEXT("No level found for CE processing");
	bool bResult = false;

	// Try to execute the command on all level script actors
	for( TArray<ULevel*>::TConstIterator it = InWorld->GetLevels().CreateConstIterator(); it; ++it )
	{
		ULevel* CurrentLevel = *it;
		if( CurrentLevel )
		{
			ErrorMessage = TEXT("No LevelScriptActor found for CE processing");

			if( CurrentLevel->GetLevelScriptActor() )
			{
				ErrorMessage = 0;

				// return true if at least one level handles the command
				bResult |= CurrentLevel->GetLevelScriptActor()->CallFunctionByNameWithArguments(Cmd, Ar, NULL);
			}
		}
	}

	if(!bResult)
	{
		ErrorMessage = TEXT("CE command wasn't processed");
	}

	if(ErrorMessage)
	{
		UE_LOG(LogEngine, Error, TEXT("%s"), ErrorMessage);
	}

	// the command was processed (resulted in executing the command or an error message) - no other spot handles "CE"
	return true;
}

#if STATS
bool UEngine::HandleDumpParticleMemCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FParticleMemoryStatManager::DumpParticleMemoryStats(Ar);
	return true;
}
#endif

bool UEngine::HandleStatCommand( UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Cmd, FOutputDevice& Ar )
{
	const TCHAR* Temp = Cmd;
	for (int32 StatIdx = 0; StatIdx < EngineStats.Num(); StatIdx++)
	{
		const FEngineStatFuncs& EngineStat = EngineStats[StatIdx];
		FString CommandName = EngineStat.CommandName.ToString();
		if (CommandName.RemoveFromStart(TEXT("STAT_")) && (FParse::Command(&Temp, *CommandName)))
		{
			if (EngineStat.ToggleFunc)
			{
				return (this->*(EngineStat.ToggleFunc))(World, ViewportClient, Temp);
			}
			return true;
		}
	}
	return false;
}

#if !UE_BUILD_SHIPPING
bool UEngine::HandleTestslateGameUICommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TSharedRef<SWidget> GameUI = 
		SNew( SHorizontalBox )
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding( 5.0f )
		.HAlign( HAlign_Left )
		.VAlign( VAlign_Top )
		[
			SNew( SButton )
			.Text( NSLOCTEXT("UnrealEd", "TestSlateGameUIButtonText", "Test Button!") )
		]
	+ SHorizontalBox::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding( 5.0f )
		.FillWidth(0.66f)
		[
			SNew(SThrobber)
		];

	GEngine->GameViewport->AddViewportWidgetContent( GameUI );
	return true;
}

bool UEngine::HandleConfigHashCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString ConfigFilename;
	if ( FParse::Token(Cmd, ConfigFilename, true) )
	{
		if ( ConfigFilename == TEXT("NAMESONLY") )
		{
			Ar.Log( TEXT("Files map:") );
			for ( FConfigCacheIni::TIterator It(*GConfig); It; ++It )
			{
				Ar.Logf(TEXT("FileName: %s"), *It.Key());
			}
		}
		else
		{
			Ar.Logf(TEXT("Attempting to dump data for config file: %s"), *ConfigFilename);
			GConfig->Dump(Ar, *ConfigFilename);
		}
	}
	else
	{
		GConfig->Dump( Ar );
	}
	return true;
}

bool UEngine::HandleConfigMemCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GConfig->ShowMemoryUsage(Ar);
	return true;
}
#endif // !UE_BUILD_SHIPPING

bool UEngine::HandleFlushLogCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GLog->FlushThreadedLogs();
	GLog->Flush();
	return true;
}

bool UEngine::HandleExitCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Ignore these commands when running the editor
	if( !GIsEditor )
	{
		Ar.Log( TEXT("Closing by request") );
		FPlatformMisc::RequestExit( 0 );
	}
	return true;
}

bool UEngine::HandleDumpTicksCommand( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Handle optional parameters, will dump all tick functions by default.
	bool bShowEnabled = true;
	bool bShowDisabled = true;
	if (FParse::Command(&Cmd, TEXT("ENABLED")))
	{
		bShowDisabled = false;
	}
	else if (FParse::Command(&Cmd, TEXT("DISABLED")))
	{
		bShowEnabled = false;
	}
	FTickTaskManagerInterface::Get().DumpAllTickFunctions(Ar, InWorld, bShowEnabled, bShowDisabled);
	return true;
}

bool UEngine::HandleGammaCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	DisplayGamma = (*Cmd != 0) ? FMath::Clamp<float>(FCString::Atof(*FParse::Token(Cmd, false)), 0.5f, 5.0f) : 2.2f;
	return true;
}

/**
 * Computes a color to use for property coloration for the given object.
 *
 * @param	Object		The object for which to compute a property color.
 * @param	OutColor	[out] The returned color.
 * @return				true if a color was successfully set on OutColor, false otherwise.
 */
bool UEngine::GetPropertyColorationColor(UObject* Object, FColor& OutColor)
{
	return false;
}

/** Uses StatColorMappings to find a color for this stat's value. */
bool UEngine::GetStatValueColoration(const FString& StatName, float Value, FColor& OutColor)
{
	for(const FStatColorMapping& Mapping : StatColorMappings)
	{
		if(StatName == Mapping.StatName)
		{
			const int32 NumPoints = Mapping.ColorMap.Num();

			// If no point in curve, return the Default value we passed in.
			if( NumPoints == 0 )
			{
				return false;
			}

			// If only one point, or before the first point in the curve, return the first points value.
			if( NumPoints < 2 || (Value <= Mapping.ColorMap[0].In) )
			{
				OutColor = Mapping.ColorMap[0].Out;
				return true;
			}

			// If beyond the last point in the curve, return its value.
			if( Value >= Mapping.ColorMap[NumPoints-1].In )
			{
				OutColor = Mapping.ColorMap[NumPoints-1].Out;
				return true;
			}

			// Somewhere with curve range - linear search to find value.
			for( int32 PointIndex=1; PointIndex<NumPoints; PointIndex++ )
			{	
				if( Value < Mapping.ColorMap[PointIndex].In )
				{
					if (Mapping.DisableBlend)
					{
						OutColor = Mapping.ColorMap[PointIndex].Out;
					}
					else
					{
						const float Diff = Mapping.ColorMap[PointIndex].In - Mapping.ColorMap[PointIndex-1].In;
						const float Alpha = (Value - Mapping.ColorMap[PointIndex-1].In) / Diff;

						FLinearColor A(Mapping.ColorMap[PointIndex-1].Out);
						FVector AV(A.R, A.G, A.B);

						FLinearColor B(Mapping.ColorMap[PointIndex].Out);
						FVector BV(B.R, B.G, B.B);

						FVector OutColorV = FMath::Lerp( AV, BV, Alpha );
						OutColor = FLinearColor(OutColorV.X, OutColorV.Y, OutColorV.Z);
					}

					return true;
				}
			}

			OutColor = Mapping.ColorMap[NumPoints-1].Out;
			return true;
		}
	}

	// No entry for this stat name
	return false;
}

void UEngine::OnLostFocusPause(bool EnablePause)
{
	if( bPauseOnLossOfFocus )
	{
		for (auto It = WorldList.CreateIterator(); It; ++It)
		{
			FWorldContext &Context = *It;

		// Iterate over all players and pause / unpause them
		// Note: pausing / unpausing the player is done via their HUD pausing / unpausing
			for (int32 PlayerIndex = 0; PlayerIndex < Context.GamePlayers.Num(); ++PlayerIndex)
		{
				APlayerController* PlayerController = Context.GamePlayers[PlayerIndex]->PlayerController;
			if(PlayerController && PlayerController->MyHUD)
			{
				PlayerController->MyHUD->OnLostFocusPause(EnablePause);
			}
		}
	}
}
}

void UEngine::InitHardwareSurvey()
{
	if (GConfig)
	{
		bool bEnabled = false;

		// The hardware survey costs time and we don't want to slow down debug builds.
		// This is mostly because of the CPU benchmark running in the survey and the results in debug are not being valid.
#if UE_BUILD_DEBUG == 0
		GConfig->GetBool(TEXT("Engine.HardwareSurvey"), TEXT("bEnableHardwareSurvey"), bEnabled, GEngineIni);
#endif

		if (bEnabled)
		{
			if (IsHardwareSurveyRequired())
			{
				bPendingHardwareSurveyResults = true;
			}
		}	
	}
}

void UEngine::TickHardwareSurvey()
{
#if !UE_BUILD_SHIPPING
	// Debug routine to eat 1MB of memory every frame
	if (GDebugAllocMemEveryFrame)
	{
		for( int32 i=0;i<16;i++ )
		{
			void* Eat = FMemory::Malloc(65536);
			FMemory::Memset( Eat, 0, 65536 );
		}
	}
#endif

	if (bPendingHardwareSurveyResults)
	{
		FHardwareSurveyResults HardwareSurveyResults;
		if (FPlatformSurvey::GetSurveyResults(HardwareSurveyResults))
		{
			OnHardwareSurveyComplete(HardwareSurveyResults);
			bPendingHardwareSurveyResults = false;
		}
	}
}

bool UEngine::IsHardwareSurveyRequired()
{
#if PLATFORM_DESKTOP
	// Analytics must have been initialized FIRST.
	if (!FEngineAnalytics::IsAvailable())
	{
		return false;
	}

	bool bSurveyDone = false;
	GConfig->GetBool(TEXT("Engine.HardwareSurvey"), TEXT("bHardwareSurveyDone"), bSurveyDone, GEditorGameAgnosticIni);

	bool bSurveyExpired = false;
	if (bSurveyDone)
	{
		bSurveyExpired = true;
		FString SurveyDateTimeString;
		if (GConfig->GetString(TEXT("Engine.HardwareSurvey"), TEXT("HardwareSurveyDateTime"), SurveyDateTimeString, GEditorGameAgnosticIni))
		{
			FDateTime SurveyDateTime;
			if (FDateTime::Parse(SurveyDateTimeString, SurveyDateTime))
			{
				FDateTime Now = FDateTime::UtcNow();
				int MonthsDelta = 12 * (Now.GetYear() - SurveyDateTime.GetYear()) + Now.GetMonth() - SurveyDateTime.GetMonth();

				bSurveyExpired = MonthsDelta > 1 || (MonthsDelta == 1 && Now.GetDay() >= SurveyDateTime.GetDay());
			}
		}
	}

	return !bSurveyDone || bSurveyExpired;
#else
	return false;
#endif		// PLATFORM_DESKTOP
}

FString UEngine::HardwareSurveyBucketRAM(uint32 MemoryMB)
{
	const float GBToMB = 1024.0f;
	FString BucketedRAM;

	if (MemoryMB < 2.0f * GBToMB) BucketedRAM = TEXT("<2GB");
	else if (MemoryMB < 4.0f * GBToMB) BucketedRAM = TEXT("2GB-4GB");
	else if (MemoryMB < 6.0f * GBToMB) BucketedRAM = TEXT("4GB-6GB");
	else if (MemoryMB < 8.0f * GBToMB) BucketedRAM = TEXT("6GB-8GB");
	else if (MemoryMB < 12.0f * GBToMB) BucketedRAM = TEXT("8GB-12GB");
	else if (MemoryMB < 16.0f * GBToMB) BucketedRAM = TEXT("12GB-16GB");
	else if (MemoryMB < 20.0f * GBToMB) BucketedRAM = TEXT("16GB-20GB");
	else if (MemoryMB < 24.0f * GBToMB) BucketedRAM = TEXT("20GB-24GB");
	else if (MemoryMB < 28.0f * GBToMB) BucketedRAM = TEXT("24GB-28GB");
	else if (MemoryMB < 32.0f * GBToMB) BucketedRAM = TEXT("28GB-32GB");
	else if (MemoryMB < 36.0f * GBToMB) BucketedRAM = TEXT("32GB-36GB");
	else BucketedRAM = TEXT(">36GB");

	return BucketedRAM;
}

FString UEngine::HardwareSurveyBucketVRAM(uint32 VidMemoryMB)
{
	const float GBToMB = 1024.0f;
	FString BucketedVRAM;

	if (VidMemoryMB < 0.25f * GBToMB) BucketedVRAM = TEXT("<256MB");
	else if (VidMemoryMB < 0.5f * GBToMB) BucketedVRAM = TEXT("256MB-512MB");
	else if (VidMemoryMB < 1.0f * GBToMB) BucketedVRAM = TEXT("512MB-1GB");
	else if (VidMemoryMB < 1.5f * GBToMB) BucketedVRAM = TEXT("1GB-1.5GB");
	else if (VidMemoryMB < 2.0f * GBToMB) BucketedVRAM = TEXT("1.5GB-2GB");
	else if (VidMemoryMB < 2.5f * GBToMB) BucketedVRAM = TEXT("2GB-2.5GB");
	else if (VidMemoryMB < 3.0f * GBToMB) BucketedVRAM = TEXT("2.5GB-3GB");
	else if (VidMemoryMB < 4.0f * GBToMB) BucketedVRAM = TEXT("3GB-4GB");
	else if (VidMemoryMB < 6.0f * GBToMB) BucketedVRAM = TEXT("4GB-6GB");
	else if (VidMemoryMB < 8.0f * GBToMB) BucketedVRAM = TEXT("6GB-8GB");
	else BucketedVRAM = TEXT(">8GB");

	return BucketedVRAM;
}

FString UEngine::HardwareSurveyBucketResolution(uint32 DisplayWidth, uint32 DisplayHeight)
{
	FString BucketedRes;
	float AspectRatio = (float)DisplayWidth / DisplayHeight;

	if (AspectRatio < 1.5f)
	{
		// approx 4:3
		if (DisplayWidth < 1150)
		{
			BucketedRes = TEXT("1024x768");
		}
		else if (DisplayHeight < 912)
		{
			BucketedRes = TEXT("1280x800");
		}
		else
		{
			BucketedRes = TEXT("1280x1024");
		}
	}
	else
	{
		// widescreen
		if (DisplayWidth < 1400)
		{
			BucketedRes = TEXT("1366x768");
		}
		else if (DisplayWidth < 1520)
		{
			BucketedRes = TEXT("1440x900");
		}
		else if (DisplayWidth < 1640)
		{
			BucketedRes = TEXT("1600x900");
		}
		else if (DisplayWidth < 1800)
		{
			BucketedRes = TEXT("1680x1050");
		}
		else if (DisplayHeight < 1140)
		{
			BucketedRes = TEXT("1920x1080");
		}
		else
		{
			BucketedRes = TEXT("1920x1200");
		}
	}

	return BucketedRes;
}

FString UEngine::HardwareSurveyGetResolutionClass(uint32 LargestDisplayHeight)
{
	FString ResolutionClass = TEXT( "720" );

	if( LargestDisplayHeight < 700 )
	{
		ResolutionClass = TEXT( "<720" );
	}
	else if( LargestDisplayHeight > 1024 )
	{
		ResolutionClass = TEXT( "1080+" );
	}

	return ResolutionClass;
}

void UEngine::OnHardwareSurveyComplete(const FHardwareSurveyResults& SurveyResults)
{
#if PLATFORM_DESKTOP
	if (GConfig)
	{
		GConfig->SetBool(TEXT("Engine.HardwareSurvey"), TEXT("bHardwareSurveyDone"), true, GEditorGameAgnosticIni);
		GConfig->SetString(TEXT("Engine.HardwareSurvey"), TEXT("HardwareSurveyDateTime"), *FDateTime::UtcNow().ToString(), GEditorGameAgnosticIni);
	}

	if (FEngineAnalytics::IsAvailable())
	{
		IAnalyticsProvider& Analytics = FEngineAnalytics::GetProvider();

		TArray<FAnalyticsEventAttribute> HardwareWEIAttribs;
		HardwareWEIAttribs.Add(FAnalyticsEventAttribute(TEXT( "CPU.WEI" ), FString::Printf( TEXT( "%.1f" ), SurveyResults.CPUPerformanceIndex )));
		HardwareWEIAttribs.Add(FAnalyticsEventAttribute(TEXT( "GPU.WEI" ), FString::Printf( TEXT( "%.1f" ), SurveyResults.GPUPerformanceIndex )));
		HardwareWEIAttribs.Add(FAnalyticsEventAttribute(TEXT( "Memory.WEI" ), FString::Printf( TEXT( "%.1f" ), SurveyResults.RAMPerformanceIndex )));

		Analytics.RecordEvent(TEXT( "Hardware.WEI.1" ), HardwareWEIAttribs);
		Analytics.RecordUserAttribute(HardwareWEIAttribs);

		FString MainGPUName(TEXT("Unknown"));
		float MainGPUVRAMMB = 0.0f;
		FString MainGPUDriverVer(TEXT("UnknownVersion"));
		if (SurveyResults.DisplayCount > 0)
		{
			MainGPUName = &SurveyResults.Displays[0].GPUCardName[0];
			MainGPUVRAMMB = SurveyResults.Displays[0].GPUDedicatedMemoryMB;
			MainGPUDriverVer = &SurveyResults.Displays[0].GPUDriverVersion[0];
		}

		uint32 LargestDisplayHeight = 0;
		FString DisplaySize[3];
		if (SurveyResults.DisplayCount > 0)
		{
			DisplaySize[0] = HardwareSurveyBucketResolution(SurveyResults.Displays[0].CurrentModeWidth, SurveyResults.Displays[0].CurrentModeHeight);
			LargestDisplayHeight = FMath::Max(LargestDisplayHeight, SurveyResults.Displays[0].CurrentModeHeight);
		}
		if (SurveyResults.DisplayCount > 1)
		{
			DisplaySize[1] = HardwareSurveyBucketResolution(SurveyResults.Displays[1].CurrentModeWidth, SurveyResults.Displays[1].CurrentModeHeight);
			LargestDisplayHeight = FMath::Max(LargestDisplayHeight, SurveyResults.Displays[1].CurrentModeHeight);
		}
		if (SurveyResults.DisplayCount > 2)
		{
			DisplaySize[2] = HardwareSurveyBucketResolution(SurveyResults.Displays[2].CurrentModeWidth, SurveyResults.Displays[2].CurrentModeHeight);
			LargestDisplayHeight = FMath::Max(LargestDisplayHeight, SurveyResults.Displays[2].CurrentModeHeight);
		}

		// Resolution Class
		FString ResolutionClass;
		if (LargestDisplayHeight < 700)
		{
			ResolutionClass = TEXT("<720");
		}
		else if (LargestDisplayHeight < 1024)
		{
			ResolutionClass = TEXT("720");
		}
		else
		{
			ResolutionClass = TEXT("1080+");
		}

		// Bucket RAM
		FString BucketedRAM = HardwareSurveyBucketRAM(SurveyResults.MemoryMB);

		// Bucket VRAM
		FString BucketedVRAM = HardwareSurveyBucketVRAM(MainGPUVRAMMB);

		TArray<FAnalyticsEventAttribute> HardwareStatsAttribs;
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "Platform" ), SurveyResults.Platform ));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "CPU.WEI" ), FString::Printf( TEXT( "%.1f" ), SurveyResults.CPUPerformanceIndex )));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "CPU.Brand" ), SurveyResults.CPUBrand));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "CPU.Speed" ), FString::Printf( TEXT( "%.1fGHz" ), SurveyResults.CPUClockGHz )));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "CPU.Count" ), FString::Printf( TEXT( "%d" ), SurveyResults.CPUCount )));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "CPU.Name" ), SurveyResults.CPUNameString));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "CPU.Info" ), FString::Printf( TEXT( "0x%08x" ), SurveyResults.CPUInfo )));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "GPU.WEI" ), FString::Printf( TEXT( "%.1f" ), SurveyResults.GPUPerformanceIndex )));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "GPU.Name" ), MainGPUName));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "GPU.VRAM" ), BucketedVRAM));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "GPU.DriverVersion" ), MainGPUDriverVer));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "RAM" ), BucketedRAM));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "RAM.WEI" ), FString::Printf( TEXT( "%.1f" ), SurveyResults.RAMPerformanceIndex )));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "NumberOfMonitors" ), FString::Printf( TEXT( "%d" ), SurveyResults.DisplayCount )));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "MonitorResolution.0" ), DisplaySize[0]));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "MonitorResolution.1" ), DisplaySize[1]));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "MonitorResolution.2" ), DisplaySize[2]));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "ResolutionClass" ), ResolutionClass));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "OS.Version" ), SurveyResults.OSVersion));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "OS.SubVersion" ), SurveyResults.OSSubVersion));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "OS.Bits" ), FString::Printf( TEXT( "%d-bit" ), SurveyResults.OSBits)));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "OS.Language" ), SurveyResults.OSLanguage));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "IsLaptop" ), SurveyResults.bIsLaptopComputer ? TEXT("true") : TEXT("false")));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "IsRemoteSession" ), SurveyResults.bIsRemoteSession ? TEXT("true") : TEXT("false")));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "SynthIdx.CPU0" ), FString::Printf( TEXT( "%.1f" ), SurveyResults.SynthBenchmark.CPUStats[0].ComputePerfIndex() )));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "SynthIdx.CPU1" ), FString::Printf( TEXT( "%.1f" ), SurveyResults.SynthBenchmark.CPUStats[1].ComputePerfIndex() )));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "SynthIdx.GPU0" ), FString::Printf( TEXT( "%.1f" ), SurveyResults.SynthBenchmark.GPUStats[0].ComputePerfIndex() )));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "SynthIdx.GPU1" ), FString::Printf( TEXT( "%.1f" ), SurveyResults.SynthBenchmark.GPUStats[1].ComputePerfIndex() )));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "SynthIdx.GPU2" ), FString::Printf( TEXT( "%.1f" ), SurveyResults.SynthBenchmark.GPUStats[2].ComputePerfIndex() )));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "SynthIdx.GPU3" ), FString::Printf( TEXT( "%.1f" ), SurveyResults.SynthBenchmark.GPUStats[3].ComputePerfIndex() )));
		HardwareStatsAttribs.Add(FAnalyticsEventAttribute(TEXT( "SynthIdx.GPU4" ), FString::Printf( TEXT( "%.1f" ), SurveyResults.SynthBenchmark.GPUStats[4].ComputePerfIndex() )));

		Analytics.RecordEvent(TEXT( "HardwareStats.1" ), HardwareStatsAttribs);
		Analytics.RecordUserAttribute(TEXT( "ResolutionClass" ), ResolutionClass);

		TArray<FAnalyticsEventAttribute> HardwareStatErrorsAttribs;
		HardwareStatErrorsAttribs.Add(FAnalyticsEventAttribute(TEXT( "ErrorCount" ), FString::Printf( TEXT( "%d" ), SurveyResults.ErrorCount )));
		HardwareStatErrorsAttribs.Add(FAnalyticsEventAttribute(TEXT( "LastError" ), SurveyResults.LastSurveyError));
		HardwareStatErrorsAttribs.Add(FAnalyticsEventAttribute(TEXT( "LastError.Detail" ), SurveyResults.LastSurveyErrorDetail));			
		HardwareStatErrorsAttribs.Add(FAnalyticsEventAttribute(TEXT( "LastError.WEI" ), SurveyResults.LastPerformanceIndexError));
		HardwareStatErrorsAttribs.Add(FAnalyticsEventAttribute(TEXT( "LastError.WEI.Detail" ), SurveyResults.LastPerformanceIndexErrorDetail));

		Analytics.RecordEvent(TEXT( "HardwareStatErrors.1" ), HardwareStatErrorsAttribs);
	}
#endif		// PLATFORM_DESKTOP
}

static TAutoConsoleVariable<float> CVarMaxFPS(
	TEXT("t.MaxFPS"),0.f,
	TEXT("Caps FPS to the given value.  Set to <= 0 to be uncapped."));
// CauseHitches cvar
static TAutoConsoleVariable<int32> CVarCauseHitches(
	TEXT("CauseHitches"),0,
	TEXT("Causes a 200ms hitch every second."));

static TAutoConsoleVariable<int32> CVarUnsteadyFPS(
	TEXT("t.UnsteadyFPS"),0,
	TEXT("Causes FPS to bounce around randomly in the 8-32 range."));

/** Get tick rate limitor. */
float UEngine::GetMaxTickRate( float DeltaTime, bool bAllowFrameRateSmoothing )
{
	float MaxTickRate = 0;

	if (FPlatformProperties::AllowsFramerateSmoothing())
	{
		// Smooth the framerate if wanted. The code uses a simplistic running average. Other approaches, like reserving
		// a percentage of time, ended up creating negative feedback loops in conjunction with GPU load and were abandonend.
		if( bSmoothFrameRate && bAllowFrameRateSmoothing && !IsRunningDedicatedServer() )
		{
			if( DeltaTime < 0.0f )
			{
#if (UE_BUILD_SHIPPING && WITH_EDITOR)
				// End users don't have access to the secure parts of UDN. The localized string points to the release notes,
				// which should include a link to the AMD CPU drivers download site.
				UE_LOG(LogEngine, Fatal, TEXT("%s"), TEXT("CPU time drift detected! Please consult release notes on how to address this."));
#else
				// Send developers to the support list thread.
				UE_LOG(LogEngine, Fatal, TEXT("Negative delta time! Please see https://udn.epicgames.com/lists/showpost.php?list=ue3bugs&id=4364"));
#endif
			}

			// Running average delta time, initial value at 100 FPS so fast machines don't have to creep up
			// to a good frame rate due to code limiting upward "mobility".
			static float RunningAverageDeltaTime = 1 / 100.f;

			// Keep track of running average over 300 frames, clamping at min of 5 FPS for individual delta times.
			RunningAverageDeltaTime = FMath::Lerp<float>( RunningAverageDeltaTime, FMath::Min<float>( DeltaTime, 0.2f ), 1 / 300.f );

			// Work in FPS domain as that is what the function will return.
			MaxTickRate = 1.f / RunningAverageDeltaTime;

			// Clamp FPS into ini defined min/ max range.
			if (SmoothedFrameRateRange.HasLowerBound())
			{
				MaxTickRate = FMath::Max( MaxTickRate, SmoothedFrameRateRange.GetLowerBoundValue() );
			}
			if (SmoothedFrameRateRange.HasUpperBound())
			{
				MaxTickRate = FMath::Min( MaxTickRate, SmoothedFrameRateRange.GetUpperBoundValue() );
			}
		}
	}

	if (CVarCauseHitches.GetValueOnGameThread())
	{
		static float RunningHitchTimer = 0.f;
		RunningHitchTimer += DeltaTime;
		if (RunningHitchTimer > 1.f)
		{
			// hitch!
			FPlatformProcess::Sleep(0.2f);
			RunningHitchTimer = 0.f;
		}
	}

	if (CVarUnsteadyFPS.GetValueOnGameThread())
	{
		static float LastMaxTickRate = 20.f;
		float RandDelta = FMath::FRandRange(-5.f, 5.f);
		MaxTickRate = FMath::Clamp(LastMaxTickRate + RandDelta, 8.f, 32.f);
		LastMaxTickRate = MaxTickRate;
	}
	else if (CVarMaxFPS.GetValueOnGameThread() > 0)
	{
		MaxTickRate = CVarMaxFPS.GetValueOnGameThread();
	}

	return MaxTickRate;
}

/**
 * Enables or disables the ScreenSaver (desktop only)
 *
 * @param bEnable	If true the enable the screen saver, if false disable it.
 */
void UEngine::EnableScreenSaver( bool bEnable )
{
#if PLATFORM_DESKTOP
	TCHAR EnvVariable[32];
	FPlatformMisc::GetEnvironmentVariable(TEXT("UE-DisallowScreenSaverInhibitor"), EnvVariable, ARRAY_COUNT(EnvVariable));
	const bool bDisallowScreenSaverInhibitor = FString(EnvVariable).ToBool();
	
	// By default we allow to use screen saver inhibitor, but in some cases user can override this setting.
	if( !bDisallowScreenSaverInhibitor )
	{
		// try a simpler API first
		if ( !FPlatformMisc::ControlScreensaver( bEnable ? FPlatformMisc::EScreenSaverAction::Enable : FPlatformMisc::EScreenSaverAction::Disable ) )
		{
			// Screen saver inhibitor disabled if no multithreading is available.
			if (FPlatformProcess::SupportsMultithreading() )
			{
				if( !ScreenSaverInhibitor )
				{
					// Create thread inhibiting screen saver while it is running.
					ScreenSaverInhibitorRunnable = new FScreenSaverInhibitor();
					ScreenSaverInhibitor = FRunnableThread::Create(ScreenSaverInhibitorRunnable, TEXT("ScreenSaverInhibitor"), 16 * 1024, TPri_Normal, FPlatformAffinity::GetPoolThreadMask());
					// Only actually run when needed to not bypass group policies for screensaver, etc.
					ScreenSaverInhibitor->Suspend( true );
					ScreenSaverInhibitorSemaphore = 0;
				}

				if( bEnable && ScreenSaverInhibitorSemaphore > 0)
				{
					if( --ScreenSaverInhibitorSemaphore == 0 )
					{	
						// If the semaphore is zero and we are enabling the screensaver
						// the thread preventing the screen saver should be suspended
						ScreenSaverInhibitor->Suspend( true );
					}
				}
				else if( !bEnable )
				{
					if( ++ScreenSaverInhibitorSemaphore == 1 )
					{
						// If the semaphore is just becoming one, the thread 
						// is was not running so enable it.
						ScreenSaverInhibitor->Suspend( false );
					}
				}
			}
		}
	}
#endif
}

/**
 * Queue up view "slave" locations to the streaming system. These locations will be added properly at the next call to AddViewInformation,
 * re-using the screensize and FOV settings.
 *
 * @param SlaveLocation			World-space view origin
 * @param BoostFactor			A factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa.
 * @param bOverrideLocation		Whether this is an override location, which forces the streaming system to ignore all other locations
 * @param OverrideDuration		How long the streaming system should keep checking this location if bOverrideLocation is true, in seconds. 0 means just for the next Tick.
 */
void UEngine::AddTextureStreamingSlaveLoc(FVector InLoc, float BoostFactor, bool bOverrideLocation, float OverrideDuration)
{
	IStreamingManager::Get().AddViewSlaveLocation(InLoc, BoostFactor, bOverrideLocation, OverrideDuration);
}

/** Looks up the GUID of a package on disk. The package must NOT be in the autodownload cache.
 * This may require loading the header of the package in question and is therefore slow.
 */
FGuid UEngine::GetPackageGuid(FName PackageName)
{
	FGuid Result(0,0,0,0);

	BeginLoad();
	ULinkerLoad* Linker = GetPackageLinker(NULL, *PackageName.ToString(), LOAD_NoWarn | LOAD_NoVerify, NULL, NULL);
	if (Linker != NULL && Linker->LinkerRoot != NULL)
	{
		Result = Linker->LinkerRoot->GetGuid();
	}
	EndLoad();

	return Result;
}

/** 
 * Returns whether we are running on a console platform or on the PC.
 *
 * @return true if we're on a console, false if we're running on a PC
 */
bool UEngine::IsConsoleBuild(EConsoleType ConsoleType) const
{
	switch (ConsoleType)
	{
		case CONSOLE_Any:
#if !PLATFORM_DESKTOP
			return true;
#else
			return false;
#endif
		case CONSOLE_Mobile:
			return false;
		default:
			UE_LOG(LogEngine, Warning, TEXT("Unknown ConsoleType passed to IsConsoleBuild()"));
			return false;
	}
}

/**
 *	This function will add a debug message to the onscreen message list.
 *	It will be displayed for FrameCount frames.
 *
 *	@param	Key				A unique key to prevent the same message from being added multiple times.
 *	@param	TimeToDisplay	How long to display the message, in seconds.
 *	@param	DisplayColor	The color to display the text in.
 *	@param	DebugMessage	The message to display.
 */
void UEngine::AddOnScreenDebugMessage(uint64 Key,float TimeToDisplay,FColor DisplayColor,const FString& DebugMessage)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bEnableOnScreenDebugMessages == true)
	{
		if (Key == (uint64)-1)
		{
			FScreenMessageString* NewMessage = new(PriorityScreenMessages)FScreenMessageString();
			check(NewMessage);
			NewMessage->Key = Key;
			NewMessage->ScreenMessage = DebugMessage;
			NewMessage->DisplayColor = DisplayColor;
			NewMessage->TimeToDisplay = TimeToDisplay;
			NewMessage->CurrentTimeDisplayed = 0.0f;
		}
		else
		{
			FScreenMessageString* Message = ScreenMessages.Find(Key);
			if (Message == NULL)
			{
				FScreenMessageString NewMessage;
				NewMessage.CurrentTimeDisplayed = 0.0f;
				NewMessage.Key = Key;
				NewMessage.DisplayColor = DisplayColor;
				NewMessage.TimeToDisplay = TimeToDisplay;
				NewMessage.ScreenMessage = DebugMessage;
				ScreenMessages.Add((int32)Key, NewMessage);
			}
			else
			{
				// Set the message, and update the time to display and reset the current time.
				Message->ScreenMessage = DebugMessage;
				Message->DisplayColor = DisplayColor;
				Message->TimeToDisplay = TimeToDisplay;
				Message->CurrentTimeDisplayed = 0.0f;
			}
		}
	}
#endif
}

/** Wrapper from int32 to uint64 */
void UEngine::AddOnScreenDebugMessage(int32 Key, float TimeToDisplay, FColor DisplayColor, const FString& DebugMessage)
{
	if (bEnableOnScreenDebugMessages == true)
	{
		AddOnScreenDebugMessage( (uint64)Key, TimeToDisplay, DisplayColor, DebugMessage);
	}
}

bool UEngine::OnScreenDebugMessageExists(uint64 Key)
{
	if (bEnableOnScreenDebugMessages == true)
	{
		if (Key == (uint64)-1)
		{
			// Priority messages assumed to always exist...
			// May want to check for there being none.
			return true;
		}

		if (ScreenMessages.Find(Key) != NULL)
		{
			return true;
		}
	}

	return false;
}

void UEngine::ClearOnScreenDebugMessages()
{
	ScreenMessages.Empty();
	PriorityScreenMessages.Empty();
}

void UEngine::PerformanceCapture(const FString& CaptureName)
{
	//mapname
	FString PathName = CaptureName + TEXT("/") + FPlatformProperties::PlatformName();

	// Create the folder name based on the hardware specs we have been provided
	FString HardwareDetails = FHardwareInfo::GetHardwareDetailsString();

	FString RHIString;
	FString RHILookup = NAME_RHI.ToString() + TEXT( "=" );
	if( FParse::Value( *HardwareDetails, *RHILookup, RHIString ) )
	{
		PathName = ( PathName + TEXT( "_" ) ) + RHIString;
	}

	FString TextureFormatString;
	FString TextureFormatLookup = NAME_TextureFormat.ToString() + TEXT( "=" );
	if( FParse::Value( *HardwareDetails, *TextureFormatLookup, TextureFormatString ) )
	{
		PathName = ( PathName + TEXT( "_" ) ) + TextureFormatString;
	}

	FString DeviceTypeString;
	FString DeviceTypeLookup = NAME_DeviceType.ToString() + TEXT( "=" );
	if( FParse::Value( *HardwareDetails, *DeviceTypeLookup, DeviceTypeString ) )
	{
		PathName = ( PathName + TEXT( "_" ) ) + DeviceTypeString;
	}

	PathName += TEXT("/");

	//mapname/CaptureName/platform/version.png

	//Make path relative to the root.
	PathName = FPaths::AutomationDir() + PathName;
	FPaths::MakePathRelativeTo(PathName,*FPaths::RootDir());
	
	FString ScreenshotName = FString::Printf(TEXT("%s%d.png"), *PathName, GEngineVersion.GetChangelist());
	
	FScreenshotRequest::RequestScreenshot( ScreenshotName, false );

}

/** Transforms a location in 3D space into 'map space', in 2D */
static FVector2D TransformLocationToMap(FVector2D TopLeftPos, FVector2D BottomRightPos, FVector2D MapOrigin, const FVector2D& MapSize, FVector Loc)
{
	FVector2D MapPos;

	MapPos = MapOrigin;

	MapPos.X += MapSize.X * ((Loc.Y - TopLeftPos.Y)/(BottomRightPos.Y - TopLeftPos.Y));
	MapPos.Y += MapSize.Y * (1.0 - ((Loc.X - BottomRightPos.X)/(TopLeftPos.X - BottomRightPos.X)));	

	return MapPos;
}

/** Utility for drawing a volume geometry (as seen from above) onto the canvas */
static void DrawVolumeOnCanvas(const AVolume* Volume, FCanvas* Canvas, const FVector2D& TopLeftPos, const FVector2D& BottomRightPos, const FVector2D& MapOrigin, const FVector2D& MapSize, const FColor& VolColor)
{
	if(Volume && Volume->BrushComponent && Volume->BrushComponent->BrushBodySetup)
	{
		FTransform BrushTM = Volume->BrushComponent->ComponentToWorld;

		// Iterate over each piece
		for(int32 ConIdx=0; ConIdx<Volume->BrushComponent->BrushBodySetup->AggGeom.ConvexElems.Num(); ConIdx++)
		{
			FKConvexElem& ConvElem = Volume->BrushComponent->BrushBodySetup->AggGeom.ConvexElems[ConIdx];

#if 0 // @todo UE4 physx fix this once we have convexelem drawing again
			// Draw each triangle that makes up the convex hull
			const int32 NumTris = ConvElem.FaceTriData.Num()/3;
			for(int32 i=0; i<NumTris; i++)
			{
				// Get the verts that make up this triangle.
				const int32 I0 = ConvElem.FaceTriData((i*3)+0);
				const int32 I1 = ConvElem.FaceTriData((i*3)+1);
				const int32 I2 = ConvElem.FaceTriData((i*3)+2);

				const FVector V0 = BrushTM.TransformPosition(ConvElem.VertexData(I0));
				const FVector V1 = BrushTM.TransformPosition(ConvElem.VertexData(I1));
				const FVector V2 = BrushTM.TransformPosition(ConvElem.VertexData(I2));

				// We only want to draw faces pointing up
				const FVector Edge0 = V1 - V0;
				const FVector Edge1 = V2 - V1;
				const FVector Normal = (Edge1 ^ Edge0).SafeNormal();
				if(Normal.Z > 0.01)
				{
					// Transform as 2d points in 'map space'
					const FVector2D M0 = TransformLocationToMap( TopLeftPos, BottomRightPos, MapOrigin, MapSize, V0 );
					const FVector2D M1 = TransformLocationToMap( TopLeftPos, BottomRightPos, MapOrigin, MapSize, V1 );
					const FVector2D M2 = TransformLocationToMap( TopLeftPos, BottomRightPos, MapOrigin, MapSize, V2 );

					// dummy UVs
					FVector2D UVCoords(0,0);
					Canvas->DrawTriangle2D( M0, UVCoords, M1, UVCoords, M2, UVCoords, FLinearColor(VolColor));					

					// Draw edges of face
					if( ConvElem.DirIsFaceEdge(ConvElem.VertexData(I0) - ConvElem.VertexData(I1)) )
					{
						DrawLine( Canvas, FVector(M0.X,M0.Y,0) , FVector(M1.X,M1.Y,0), VolColor );
					}

					if( ConvElem.DirIsFaceEdge(ConvElem.VertexData(I1) - ConvElem.VertexData(I2)) )
					{
						DrawLine( Canvas, FVector(M1.X,M1.Y,0) , FVector(M2.X,M2.Y,0), VolColor );
					}

					if( ConvElem.DirIsFaceEdge(ConvElem.VertexData(I2) - ConvElem.VertexData(I0)) )
					{
						DrawLine( Canvas, FVector(M2.X,M2.Y,0) , FVector(M0.X,M0.Y,0), VolColor );
					}
				}
			}
#endif
		}
	}
}

/** Util that takes a 2D vector and rotates it by RotAngle (given in radians) */
static FVector2D RotateVec2D(const FVector2D InVec, float RotAngle)
{
	FVector2D OutVec;
	OutVec.X = (InVec.X * FMath::Cos(RotAngle)) - (InVec.Y * FMath::Sin(RotAngle));
	OutVec.Y = (InVec.X * FMath::Sin(RotAngle)) + (InVec.Y * FMath::Cos(RotAngle));
	return OutVec;
}

#if !UE_BUILD_SHIPPING
bool UEngine::HandleLogoutStatLevelsCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	const TArray<FSubLevelStatus> SubLevelsStatusList = GetSubLevelsStatus(InWorld);

	Ar.Logf( TEXT( "Levels:" ) );

	// now draw the "map" name
	if (SubLevelsStatusList.Num())
	{
		// First entry - always persistent level
		FString MapName	= SubLevelsStatusList[0].PackageName.ToString();
		if (SubLevelsStatusList[0].bPlayerInside)
		{
			MapName = *FString::Printf( TEXT("->  %s"), *MapName );
		}
		else
		{
			MapName = *FString::Printf( TEXT("    %s"), *MapName );
		}

		Ar.Logf( TEXT( "%s" ), *MapName );
	}

	// now log the levels
	for (int32 LevelIdx = 1; LevelIdx < SubLevelsStatusList.Num(); ++LevelIdx)
	{
		const FSubLevelStatus& LevelStatus = SubLevelsStatusList[LevelIdx];
		FString DisplayName = LevelStatus.PackageName.ToString();
		FString StatusName;

		switch( LevelStatus.StreamingStatus )
		{
		case LEVEL_Visible:
			StatusName = TEXT( "red loaded and visible" );
			break;
		case LEVEL_MakingVisible:
			StatusName = TEXT( "orange, in process of being made visible" );
			break;
		case LEVEL_Loaded:
			StatusName = TEXT( "yellow loaded but not visible" );
			break;
		case LEVEL_UnloadedButStillAround:
			StatusName = TEXT( "blue  (GC needs to occur to remove this)" );
			break;
		case LEVEL_Unloaded:
			StatusName = TEXT( "green Unloaded" );
			break;
		case LEVEL_Preloading:
			StatusName = TEXT( "purple (preloading)" );
			break;
		default:
			break;
		};

		if (LevelStatus.LODIndex != INDEX_NONE)
		{
			DisplayName += FString::Printf(TEXT(" [LOD%d]"), LevelStatus.LODIndex+1);
		}

		UPackage* LevelPackage = FindObjectFast<UPackage>( NULL, LevelStatus.PackageName );

		if( LevelPackage 
			&& (LevelPackage->GetLoadTime() > 0) 
			&& (LevelStatus.StreamingStatus != LEVEL_Unloaded) )
		{
			DisplayName += FString::Printf(TEXT(" - %4.1f sec"), LevelPackage->GetLoadTime());
		}
		else if( GetAsyncLoadPercentage( *LevelStatus.PackageName.ToString() ) >= 0 )
		{
			const int32 Percentage = FMath::TruncToInt( GetAsyncLoadPercentage( *LevelStatus.PackageName.ToString() ) );
			DisplayName += FString::Printf(TEXT(" - %3i %%"), Percentage ); 
		}

		if ( LevelStatus.bPlayerInside )
		{
			DisplayName = *FString::Printf( TEXT("->  %s"), *DisplayName );
		}
		else
		{
			DisplayName = *FString::Printf( TEXT("    %s"), *DisplayName );
		}

		DisplayName = FString::Printf( TEXT("%s \t\t%s"), *DisplayName, *StatusName );

		Ar.Logf( TEXT( "%s" ), *DisplayName );

	}

	return true;
}
#endif // !UE_BUILD_SHIPPING

/** Helper structure for sorting sounds by predefined criteria. */
struct FSoundInfo
{
	/** Path name to this sound. */
	FString PathName;

	/** Distance betweend a listener and this sound. */
	float Distance;

	/** Sound group this sound belongs to. */
	FName ClassName;

	/** Wave instances currently used by this sound. */
	TArray<FWaveInstance*> WaveInstances;

	FSoundInfo( FString InPathName, float InDistance, FName InClassName )
		: PathName( InPathName )
		, Distance( InDistance )
		, ClassName( InClassName )
	{}

	bool ComparePathNames( const FSoundInfo& Other ) const
	{
		return PathName < Other.PathName;
	}

	bool CompareDistance( const FSoundInfo& Other ) const
	{
		return Distance < Other.Distance;
	}

	bool CompareClass( const FSoundInfo& Other ) const
	{
		return ClassName < Other.ClassName;
	}

	bool CompareWaveInstancesNum( const FSoundInfo& Other ) const
	{
		return Other.WaveInstances.Num() < WaveInstances.Num();
	}
};

struct FCompareFSoundInfoByName
{
	FORCEINLINE bool operator()( const FSoundInfo& A, const FSoundInfo& B ) const { return A.ComparePathNames( B ); }
};

struct FCompareFSoundInfoByDistance
{
	FORCEINLINE bool operator()( const FSoundInfo& A, const FSoundInfo& B ) const { return A.CompareDistance( B ); }
};

struct FCompareFSoundInfoByClass
{
	FORCEINLINE bool operator()( const FSoundInfo& A, const FSoundInfo& B ) const { return A.CompareClass( B ); }
};

struct FCompareFSoundInfoByWaveInstNum
{
	FORCEINLINE bool operator()( const FSoundInfo& A, const FSoundInfo& B ) const { return A.CompareWaveInstancesNum( B ); }
};

/** draws a property of the given object on the screen similarly to stats */
static void DrawProperty(UCanvas* CanvasObject, UObject* Obj, const FDebugDisplayProperty& PropData, UProperty* Prop, int32 X, int32& Y)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	checkSlow(PropData.bSpecialProperty || Prop != NULL);
	checkSlow(Prop == NULL || Obj->GetClass()->IsChildOf(Prop->GetOwnerClass()));

	FCanvas* Canvas = CanvasObject->Canvas;
	FString PropText, ValueText;
	if (!PropData.bSpecialProperty)
	{
		if (PropData.WithinClass != NULL)
		{
			PropText = FString::Printf(TEXT("%s.%s.%s.%s = "), *Obj->GetOutermost()->GetName(), *Obj->GetOuter()->GetName(), *Obj->GetName(), *Prop->GetName());
		}
		else
		{
			PropText = FString::Printf(TEXT("%s.%s.%s = "), *Obj->GetOutermost()->GetName(), *Obj->GetName(), *Prop->GetName());
		}
		if (Prop->ArrayDim == 1)
		{
			Prop->ExportText_InContainer(0, ValueText, Obj, Obj, Obj, PPF_IncludeTransient);
		}
		else
		{
			ValueText += TEXT("(");
			for (int32 i = 0; i < Prop->ArrayDim; i++)
			{
				Prop->ExportText_InContainer(i, ValueText, Obj, Obj, Obj, PPF_IncludeTransient);
				if (i + 1 < Prop->ArrayDim)
				{
					ValueText += TEXT(",");
				}
			}
			ValueText += TEXT(")");
		}
	}
	else
	{
		if (PropData.PropertyName == NAME_None)
		{
			if (PropData.WithinClass != NULL)
			{
				PropText = FString::Printf(TEXT("%s.%s.%s"), *Obj->GetOutermost()->GetName(), *Obj->GetOuter()->GetName(), *Obj->GetName());
			}
			else
			{
				PropText = FString::Printf(TEXT("%s.%s"), *Obj->GetOutermost()->GetName(), *Obj->GetName());
			}
			ValueText = TEXT("");
		}
		else
		{
			if (PropData.WithinClass != NULL)
			{
				PropText = FString::Printf(TEXT("%s.%s.%s.(%s) = "), *Obj->GetOutermost()->GetName(), *Obj->GetOuter()->GetName(), *Obj->GetName(), *PropData.PropertyName.ToString());
			}
			else
			{
				PropText = FString::Printf(TEXT("%s.%s.(%s) = "), *Obj->GetOutermost()->GetName(), *Obj->GetName(), *PropData.PropertyName.ToString());
			}

			if (PropData.PropertyName == NAME_Location)
			{
				AActor *Actor = Cast<AActor>(Obj);
				ValueText = FString::Printf(TEXT("%s"), Actor != NULL ? *Actor->GetActorLocation().ToString() : TEXT("None"));
			}
			else if (PropData.PropertyName == NAME_Rotation)
			{
				AActor *Actor = Cast<AActor>(Obj);
				ValueText = FString::Printf(TEXT("%s"), Actor != NULL ? *Actor->GetActorRotation().ToString() : TEXT("None"));
			}
		}
	}

	
	int32 CommaIdx = -1;
	bool bDrawPropName = true;
	do
	{
		FString Str = ValueText;
		CommaIdx = ValueText.Find( TEXT(",") );
		if( CommaIdx >= 0 )
		{
			Str = ValueText.Left(CommaIdx);
			ValueText = ValueText.Mid( CommaIdx+1 );
		}

		int32 XL, YL;
		CanvasObject->ClippedStrLen(GEngine->GetSmallFont(), 1.0f, 1.0f, XL, YL, *PropText);
		FTextSizingParameters DrawParams(X, Y, CanvasObject->SizeX - X, 0, GEngine->GetSmallFont());
		TArray<FWrappedStringElement> TextLines;
		UCanvas::WrapString(DrawParams, X + XL, *Str, TextLines);
		int32 XL2 = XL;
		if (TextLines.Num() > 0)
		{
			XL2 += FMath::TruncToInt(TextLines[0].LineExtent.X);
			for (int32 i = 1; i < TextLines.Num(); i++)
			{
				XL2 = FMath::Max<int32>(XL2, FMath::TruncToInt(TextLines[i].LineExtent.X));
			}
		}
		Canvas->DrawTile(  X, Y, XL2 + 1, YL * FMath::Max<int>(TextLines.Num(), 1), 0, 0, CanvasObject->DefaultTexture->GetSizeX(), CanvasObject->DefaultTexture->GetSizeY(),
			FLinearColor(0.5f, 0.5f, 0.5f, 0.5f), CanvasObject->DefaultTexture->Resource );
		if( bDrawPropName )
		{
			bDrawPropName = false;
			Canvas->DrawShadowedString( X, Y, *PropText, GEngine->GetSmallFont(), FLinearColor(0.0f, 1.0f, 0.0f));
			if( TextLines.Num() > 1 )
			{
				Y += YL;
			}
		}
		if (TextLines.Num() > 0)
		{
			Canvas->DrawShadowedString( X + XL, Y, *TextLines[0].Value, GEngine->GetSmallFont(), FLinearColor(1.0f, 0.0f, 0.0f));
			for (int32 i = 1; i < TextLines.Num(); i++)
			{
				Canvas->DrawShadowedString( X, Y + YL * i, *TextLines[i].Value, GEngine->GetSmallFont(), FLinearColor(1.0f, 0.0f, 0.0f));
			}
			Y += YL * TextLines.Num();
		}
		else
		{
			Y += YL;
		}
	} while( CommaIdx >= 0 );
#endif
}

/** Basic timing collation - cannot use stats as these are not enabled in Win32 shipping */
static uint64 StatUnitLastFrameCounter = 0;
static uint32 StatUnitTotalFrameCount = 0;
static float StatUnitTotalFrameTime = 0.0f;
static float StatUnitTotalGameThreadTime = 0.0f;
static float StatUnitTotalRenderThreadTime = 0.0f;
static float StatUnitTotalGPUTime = 0.0f;

void UEngine::GetAverageUnitTimes( TArray<float>& AverageTimes )
{
	uint32 FrameCount = 0;
	AverageTimes.AddZeroed( 4 );

	if( StatUnitTotalFrameCount > 0 )
	{
		AverageTimes[0] = StatUnitTotalFrameTime / StatUnitTotalFrameCount;
		AverageTimes[1] = StatUnitTotalGameThreadTime / StatUnitTotalFrameCount;
		AverageTimes[2] = StatUnitTotalGPUTime / StatUnitTotalFrameCount;
		AverageTimes[3] = StatUnitTotalRenderThreadTime / StatUnitTotalFrameCount;
	}

	/** Reset the counters for the next call */
	StatUnitTotalFrameCount = 0;
	StatUnitTotalFrameTime = 0.0f;
	StatUnitTotalGameThreadTime = 0.0f;
	StatUnitTotalRenderThreadTime = 0.0f;
	StatUnitTotalGPUTime = 0.0f;
}

void UEngine::SetAverageUnitTimes(float FrameTime, float RenderThreadTime, float GameThreadTime, float GPUFrameTime)
{
	/** Only record the information once for the current frame */
	if (StatUnitLastFrameCounter != GFrameCounter)
	{
		StatUnitLastFrameCounter = GFrameCounter;

		/** Total times over a play session for averaging purposes */
		StatUnitTotalFrameCount++;
		StatUnitTotalFrameTime += FrameTime;
		StatUnitTotalRenderThreadTime += RenderThreadTime;
		StatUnitTotalGameThreadTime += GameThreadTime;
		StatUnitTotalGPUTime += GPUFrameTime;
	}
}

bool UEngine::ShouldThrottleCPUUsage() const
{
	return false;
}

/**
 *	Renders stats
 *
 *  @param World			The World to render stats about
 *	@param Viewport			The viewport to render to
 *	@param Canvas			Canvas object to use for rendering
 *	@param CanvasObject		Optional canvas object for visualizing properties
 *	@param DebugProperties	List of properties to visualize (in/out)
 *	@param ViewLocation		Location of camera
 *	@param ViewRotation		Rotation of camera
 */
void DrawStatsHUD( UWorld* World, FViewport* Viewport, FCanvas* Canvas, UCanvas* CanvasObject, TArray<FDebugDisplayProperty>& DebugProperties, const FVector& ViewLocation, const FRotator& ViewRotation )
{
	// We cannot draw without a canvas
	if (Canvas == NULL)
	{
		return;
	}
#if STATS
	uint32 DrawStatsBeginTime = FPlatformTime::Cycles();
#endif

	//@todo joeg: Move this stuff to a function, make safe to use on consoles by
	// respecting the various safe zones, and make it compile out.
	const int32 FPSXOffset	= (GEngine->IsStereoscopic3D()) ? Viewport->GetSizeXY().X * 0.5f * 0.334f : (FPlatformProperties::SupportsWindowedMode() ? 110 : 250);
	const int32 StatsXOffset	= FPlatformProperties::SupportsWindowedMode() ?  4 : 100;

	int32 MessageY = 35;

	if( !GIsEditor )
	{
		// Account for safe frame
		MessageY = 100;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if( !GIsHighResScreenshot && !GIsDumpingMovie && GAreScreenMessagesEnabled )
	{
		const int32 MessageX = 40;

		if (!GEngine->bSuppressMapWarnings)
		{
			FCanvasTextItem SmallTextItem( FVector2D( 0, 0 ), FText::GetEmpty(), GEngine->GetSmallFont(), FLinearColor::White );
			SmallTextItem.EnableShadow( FLinearColor::Black );

			if( GIsTextureMemoryCorrupted )
			{
				FCanvasTextItem TextItem( FVector2D( 100, 200 ), LOCTEXT("OutOfTextureMemory", "RAN OUT OF TEXTURE MEMORY, EXPECT CORRUPTION AND GPU HANGS!"), GEngine->GetMediumFont(), FLinearColor::Red );
				TextItem.EnableShadow( FLinearColor::Black );	
				Canvas->DrawItem( TextItem );
			}

			// Put the messages over fairly far to stay in the safe zone on consoles
			if( World->NumLightingUnbuiltObjects > 0 )
			{
				SmallTextItem.SetColor( FLinearColor::White );
				// Color unbuilt lighting red if encountered within the last second
				if( FApp::GetCurrentTime() - World->LastTimeUnbuiltLightingWasEncountered < 1 )
				{
					SmallTextItem.SetColor( FLinearColor::Red );
				}
				SmallTextItem.Text =  FText::FromString( FString::Printf(TEXT("LIGHTING NEEDS TO BE REBUILT (%u unbuilt object(s))"), World->NumLightingUnbuiltObjects) );				
				Canvas->DrawItem( SmallTextItem, FVector2D( MessageX, MessageY ) );
				MessageY += 20;
			}

			// check navmesh
#if WITH_EDITOR
			const bool bIsNavigationAutoUpdateEnabled = UNavigationSystem::GetIsNavigationAutoUpdateEnabled();
#else
			const bool bIsNavigationAutoUpdateEnabled = true;
#endif
			if (World->GetNavigationSystem() != NULL && World->GetNavigationSystem()->IsNavigationDirty() && 
				(!World->GetNavigationSystem()->bBuildNavigationAtRuntime || !bIsNavigationAutoUpdateEnabled))
			{
				SmallTextItem.SetColor( FLinearColor::White );
				SmallTextItem.Text =  LOCTEXT("NAVMESHERROR", "NAVMESH NEEDS TO BE REBUILT");				
				Canvas->DrawItem( SmallTextItem, FVector2D( MessageX, MessageY ) );
				MessageY += 20;
			}

			if( World->bKismetScriptError )
			{
				SmallTextItem.Text = LOCTEXT("BlueprintInLevelHadCompileErrorMessage", "BLUEPRINT COMPILE ERROR" );
				SmallTextItem.SetColor( FLinearColor::Red );
				Canvas->DrawItem( SmallTextItem, FVector2D( MessageX, MessageY ) );
				MessageY += 20;
			}

			SmallTextItem.SetColor( FLinearColor::White );

			if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
			{
				SmallTextItem.Text = FText::FromString(FString::Printf(TEXT("Shaders Compiling (%u)"), GShaderCompilingManager->GetNumRemainingJobs()));
				Canvas->DrawItem(SmallTextItem, FVector2D(MessageX, MessageY));
				MessageY += 20;
			}

#if ENABLE_VISUAL_LOG
			FVisualLog* VisLog = FVisualLog::Get();
			if (VisLog && (VisLog->IsRecording() || VisLog->IsRecordingOnServer()))
			{
				int32 XSize;
				int32 YSize;
				FString String = FString::Printf(TEXT("VisLog recording active"));
				StringSize(GEngine->GetSmallFont(), XSize, YSize, *String);

				SmallTextItem.Position = FVector2D((int32)Viewport->GetSizeXY().X - XSize - 16, 36);
				SmallTextItem.Text = FText::FromString(String);
				SmallTextItem.SetColor(FLinearColor::Red);
				SmallTextItem.EnableShadow(FLinearColor::Black);
				Canvas->DrawItem(SmallTextItem);
				SmallTextItem.SetColor(FLinearColor::White);
			}
#endif

			/* @todo ue4 temporarily disabled
			AWorldSettings* WorldSettings = World->GetWorldSettings();
			if( !WorldSettings->IsNavigationRebuilt() )
			{
				DrawShadowedString(Canvas,
					MessageX,
					MessageY,
					TEXT("PATHS NEED TO BE REBUILT"),
					GEngine->GetSmallFont(),
					FColor(128,128,128)
					);
				MessageY += 20;
			}
			*/

			if (World->bIsLevelStreamingFrozen)
			{
				SmallTextItem.Text =  LOCTEXT("Levelstreamingfrozen", "Level streaming frozen..." );
				Canvas->DrawItem( SmallTextItem, FVector2D( MessageX, MessageY ) );
				MessageY += 20;
			}

			if (GIsPrepareMapChangeBroken)
			{
				SmallTextItem.Text =  LOCTEXT("PrepareMapChangeError", "PrepareMapChange had a bad level name! Check the log (tagged with PREPAREMAPCHANGE) for info" );
				Canvas->DrawItem( SmallTextItem, FVector2D( MessageX, MessageY ) );
				MessageY += 20;
			}

#if STATS
			if (FThreadStats::IsCollectingData())
			{
				SmallTextItem.SetColor( FLinearColor::Red );
				if (!GEngine->bDisableAILogging)
				{				
					SmallTextItem.Text =  LOCTEXT("AIPROFILINGWARNING", "PROFILING WITH AI LOGGING ON!" );
					Canvas->DrawItem( SmallTextItem, FVector2D( MessageX, MessageY ) );
					MessageY += 20;
				}
				if (GShouldVerifyGCAssumptions)
				{
					SmallTextItem.Text =  LOCTEXT("GCPROFILINGWARNING", "PROFILING WITH GC VERIFY ON!" );
					Canvas->DrawItem( SmallTextItem, FVector2D( MessageX, MessageY ) );					
					MessageY += 20;
				}
			}
#endif
		}

		int32 YPos = MessageY;

		if (GEngine->bEnableOnScreenDebugMessagesDisplay && GEngine->bEnableOnScreenDebugMessages)
		{
			if (GEngine->PriorityScreenMessages.Num() > 0)
			{
				FCanvasTextItem MessageTextItem( FVector2D( 0, 0 ), FText::GetEmpty(), GEngine->GetSmallFont(), FLinearColor::White );
				MessageTextItem.EnableShadow( FLinearColor::Black );
				for (int32 PrioIndex = GEngine->PriorityScreenMessages.Num() - 1; PrioIndex >= 0; PrioIndex--)
				{
					FScreenMessageString& Message = GEngine->PriorityScreenMessages[PrioIndex];
					if (YPos < 700)
					{
						MessageTextItem.Text =  FText::FromString( Message.ScreenMessage );
						MessageTextItem.SetColor( Message.DisplayColor );
						Canvas->DrawItem( MessageTextItem, FVector2D( MessageX, YPos ) );
						YPos += 20;
					}
					Message.CurrentTimeDisplayed += World->GetDeltaSeconds();
					if (Message.CurrentTimeDisplayed >= Message.TimeToDisplay)
					{
						GEngine->PriorityScreenMessages.RemoveAt(PrioIndex);
					}
				}
			}

			if (GEngine->ScreenMessages.Num() > 0)
			{
				FCanvasTextItem MessageTextItem( FVector2D( 0, 0 ), FText::GetEmpty(), GEngine->GetSmallFont(), FLinearColor::White );
				MessageTextItem.EnableShadow( FLinearColor::Black );
				for (TMap<int32, FScreenMessageString>::TIterator MsgIt(GEngine->ScreenMessages); MsgIt; ++MsgIt)
				{
					FScreenMessageString& Message = MsgIt.Value();
					if (YPos < 700)
					{
						MessageTextItem.Text =  FText::FromString( Message.ScreenMessage );
						MessageTextItem.SetColor( Message.DisplayColor );
						Canvas->DrawItem( MessageTextItem, FVector2D( MessageX, YPos ) );												
						YPos += 20;
					}
					Message.CurrentTimeDisplayed += World->GetDeltaSeconds();
					if (Message.CurrentTimeDisplayed >= Message.TimeToDisplay)
					{
						MsgIt.RemoveCurrent();
					}
				}
			}
		}
	}
#endif

	{
		int32 X = (CanvasObject) ? CanvasObject->SizeX - FPSXOffset : Viewport->GetSizeXY().X - FPSXOffset; //??
		int32 Y = (GEngine->IsStereoscopic3D()) ? FMath::TruncToInt(Viewport->GetSizeXY().Y * 0.40f) : FMath::TruncToInt(Viewport->GetSizeXY().Y * 0.20f);

		//give the viewport first shot at drawing stats
		Y = Viewport->DrawStatsHUD(Canvas, X, Y);

#if DEBUGGING_VIEWPORT_SIZES
		// Useful for debugging viewport sizing/resizing, especially on external displays

		FCanvasTextItem ViewportTextItem( FVector2D( 0,0 ), FText::GetEmpty(), GEngine->GetSmallFont(), FLinearColor::Blue );
		ViewportTextItem.EnableShadow( FLinearColor::Black );
		FString CurrentRes = FString::Printf(TEXT("W = %d, H = %d"), Viewport->GetSizeXY().X,Viewport->GetSizeXY().Y);
		ViewportTextItem.Text = FText::FromString( CurrentRes );
		Canvas->DrawItem( ViewportTextItem, 5, Y );
		ViewportTextItem.Text =  LOCTEXT("00", "00" );
		Canvas->DrawItem( ViewportTextItem, 5, 5 );
		ViewportTextItem.Text =  LOCTEXT("0M", "0M" );
		Canvas->DrawItem( ViewportTextItem, 5,  Viewport->GetSizeXY().Y - 2 );
		ViewportTextItem.Text =  LOCTEXT("M0", "M0" );
		Canvas->DrawItem( ViewportTextItem, Viewport->GetSizeXY().X - 25, 5 );
		ViewportTextItem.Text =  LOCTEXT("MM", "MM" );
		Canvas->DrawItem( ViewportTextItem, Viewport->GetSizeXY().X - 25, Viewport->GetSizeXY().Y - 25 );
#endif

		// Render all the simple stats
		GEngine->RenderEngineStats(World, Viewport, Canvas, StatsXOffset, MessageY, X, Y, &ViewLocation, &ViewRotation);

#if STATS
		extern void RenderStats(FViewport* Viewport, class FCanvas* Canvas, int32 X, int32 Y);
		RenderStats(Viewport, Canvas, StatsXOffset, Y);
#endif
	}

	// draw debug properties
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#if (UE_BUILD_SHIPPING && WITH_EDITOR)
	if (GEngine != NULL && World->GetNetMode() == NM_Standalone && CanvasObject != NULL)
#endif
	{
		// construct a list of objects relevant to "getall" type elements, so that we only have to do the object iterator once
		// we do the iterator each frame so that new objects will show up immediately
		struct FDebugClass
		{
			UClass* Class;
			UClass* WithinClass;
			FDebugClass(UClass* InClass, UClass* InWithinClass)
				: Class(InClass), WithinClass(InWithinClass)
			{}
		};
		TArray<FDebugClass> DebugClasses;
		DebugClasses.Reserve(DebugProperties.Num());
		for (int32 i = 0; i < DebugProperties.Num(); i++)
		{
			if (DebugProperties[i].Obj != NULL && !DebugProperties[i].Obj->IsPendingKill())
			{
				UClass* Cls = Cast<UClass>(DebugProperties[i].Obj);
				if (Cls != NULL)
				{
					new(DebugClasses) FDebugClass(Cls, DebugProperties[i].WithinClass);
				}
			}
			else
			{
				// invalid, object was destroyed, etc. so remove the entry
				DebugProperties.RemoveAt(i--, 1);
			}
		}
		TArray<UObject*> RelevantObjects;
		if (DebugClasses.Num() > 0)
		{
			for (TObjectIterator<UObject> It(true); It; ++It)
			{
				if (It->GetWorld() && It->GetWorld() != World)
				{
					continue;
				}

				for (int32 i = 0; i < DebugClasses.Num(); i++)
				{
					if ( It->IsA(DebugClasses[i].Class) && !It->IsTemplate() &&
						(DebugClasses[i].WithinClass == NULL || (It->GetOuter() != NULL && It->GetOuter()->GetClass()->IsChildOf(DebugClasses[i].WithinClass))) )
					{
						RelevantObjects.Add(*It);
						break;
					}
				}
			}
		}
		// draw starting in the top left
		int32 X = StatsXOffset;
		int32 Y = FPlatformProperties::SupportsWindowedMode() ? 20 : 40;
		int32 MaxY = int32(Canvas->GetRenderTarget()->GetSizeXY().Y);
		for (int32 i = 0; i < DebugProperties.Num() && Y < MaxY; i++)
		{
			// we removed entries with invalid Obj above so no need to check for that here
			UClass* Cls = Cast<UClass>(DebugProperties[i].Obj);
			if (Cls != NULL)
			{
				UProperty* Prop = FindField<UProperty>(Cls, DebugProperties[i].PropertyName);
				if (Prop != NULL || DebugProperties[i].bSpecialProperty)
				{
					// getall
					for (int32 j = 0; j < RelevantObjects.Num(); j++)
					{
						if ( RelevantObjects[j]->IsA(Cls) && !RelevantObjects[j]->IsPendingKill() &&
							(DebugProperties[i].WithinClass == NULL || (RelevantObjects[j]->GetOuter() != NULL && RelevantObjects[j]->GetOuter()->GetClass()->IsChildOf(DebugProperties[i].WithinClass))) )
						{
							DrawProperty(CanvasObject, RelevantObjects[j], DebugProperties[i], Prop, X, Y);
						}
					}
				}
				else
				{
					// invalid entry
					DebugProperties.RemoveAt(i--, 1);
				}
			}
			else
			{
				UProperty* Prop = FindField<UProperty>(DebugProperties[i].Obj->GetClass(), DebugProperties[i].PropertyName);
				if (Prop != NULL || DebugProperties[i].bSpecialProperty)
				{
					DrawProperty(CanvasObject, DebugProperties[i].Obj, DebugProperties[i], Prop, X, Y);
				}
				else
				{
					DebugProperties.RemoveAt(i--, 1);
				}
			}
		}
	}
#endif

#if STATS
	uint32 DrawStatsEndTime = FPlatformTime::Cycles();
	SET_CYCLE_COUNTER(STAT_DrawStats, DrawStatsEndTime - DrawStatsBeginTime);
#endif
}



/**
 * Stats objects for Engine
 */
DEFINE_STAT(STAT_GameEngineTick);
DEFINE_STAT(STAT_GameViewportTick);
DEFINE_STAT(STAT_RedrawViewports);
DEFINE_STAT(STAT_UpdateLevelStreaming);
DEFINE_STAT(STAT_RHITickTime);
DEFINE_STAT(STAT_IntentionalHitch);
DEFINE_STAT(STAT_PlatformMessageTime);
DEFINE_STAT(STAT_FrameSyncTime);
DEFINE_STAT(STAT_DeferredTickTime);

/** Landscape stats */

DEFINE_STAT(STAT_LandscapeDynamicDrawTime);
DEFINE_STAT(STAT_LandscapeStaticDrawLODTime);
DEFINE_STAT(STAT_LandscapeVFDrawTime);
DEFINE_STAT(STAT_LandscapeComponents);
DEFINE_STAT(STAT_LandscapeDrawCalls);
DEFINE_STAT(STAT_LandscapeTriangles);
DEFINE_STAT(STAT_LandscapeVertexMem);
DEFINE_STAT(STAT_LandscapeComponentMem);

/** Input stat */
DEFINE_STAT(STAT_InputTime);
DEFINE_STAT(STAT_InputLatencyTime);

/** HUD stat */
DEFINE_STAT(STAT_HudTime);

/** Static mesh tris rendered */
DEFINE_STAT(STAT_StaticMeshTriangles);

/** Skeletal stats */
DEFINE_STAT(STAT_SkinningTime);
DEFINE_STAT(STAT_UpdateClothVertsTime);
DEFINE_STAT(STAT_UpdateSoftBodyVertsTime);
DEFINE_STAT(STAT_SkelMeshTriangles);
DEFINE_STAT(STAT_SkelMeshDrawCalls);
DEFINE_STAT(STAT_CPUSkinVertices);
DEFINE_STAT(STAT_GPUSkinVertices);

/** Frame chart stats */

DEFINE_STAT(STAT_FPSChart_0_5);
DEFINE_STAT(STAT_FPSChart_5_10);
DEFINE_STAT(STAT_FPSChart_10_15);
DEFINE_STAT(STAT_FPSChart_15_20);
DEFINE_STAT(STAT_FPSChart_20_25);
DEFINE_STAT(STAT_FPSChart_25_30);
DEFINE_STAT(STAT_FPSChart_30_35);
DEFINE_STAT(STAT_FPSChart_35_40);
DEFINE_STAT(STAT_FPSChart_40_45);
DEFINE_STAT(STAT_FPSChart_45_50);
DEFINE_STAT(STAT_FPSChart_50_55);
DEFINE_STAT(STAT_FPSChart_55_60);
DEFINE_STAT(STAT_FPSChart_60_INF);
DEFINE_STAT(STAT_FPSChart_30Plus);
DEFINE_STAT(STAT_FPSChart_UnaccountedTime);
DEFINE_STAT(STAT_FPSChart_FrameCount);
DEFINE_STAT(STAT_FPSChart_Hitch_5000_Plus);
DEFINE_STAT(STAT_FPSChart_Hitch_2500_5000);
DEFINE_STAT(STAT_FPSChart_Hitch_2000_2500);
DEFINE_STAT(STAT_FPSChart_Hitch_1500_2000);
DEFINE_STAT(STAT_FPSChart_Hitch_1000_1500);
DEFINE_STAT(STAT_FPSChart_Hitch_750_1000);
DEFINE_STAT(STAT_FPSChart_Hitch_500_750);
DEFINE_STAT(STAT_FPSChart_Hitch_300_500);
DEFINE_STAT(STAT_FPSChart_Hitch_200_300);
DEFINE_STAT(STAT_FPSChart_Hitch_150_200);
DEFINE_STAT(STAT_FPSChart_Hitch_100_150);
DEFINE_STAT(STAT_FPSChart_Hitch_60_100);
DEFINE_STAT(STAT_FPSChart_TotalHitchCount);

DEFINE_STAT(STAT_FPSChart_UnitFrame);
DEFINE_STAT(STAT_FPSChart_UnitGame);
DEFINE_STAT(STAT_FPSChart_UnitRender);
DEFINE_STAT(STAT_FPSChart_UnitGPU);

/*-----------------------------------------------------------------------------
	Lightmass object/actor implementations.
-----------------------------------------------------------------------------*/






/*-----------------------------------------------------------------------------
	ULightmappedSurfaceCollection
-----------------------------------------------------------------------------*/

UFont* GetStatsFont()
{
	return GEngine->GetSmallFont();
}


/**
 * Syncs the game thread with the render thread. Depending on passed in bool this will be a total
 * sync or a one frame lag.
 */
void FFrameEndSync::Sync( bool bAllowOneFrameThreadLag )
{
	check(IsInGameThread());			

	Fence[EventIndex].BeginFence();

	bool bEmptyGameThreadTasks = !FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::GameThread);

	if (bEmptyGameThreadTasks)
	{
		// need to process gamethread tasks at least once a frame no matter what
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	}

	// Use two events if we allow a one frame lag.
	if( bAllowOneFrameThreadLag )
	{
		EventIndex = (EventIndex + 1) % 2;
	}

	Fence[EventIndex].Wait(bEmptyGameThreadTasks);  // here we also opportunistically execute game thread tasks while we wait

}

FString appGetStartupMap(const TCHAR* CommandLine)
{
	FURL DefaultURL;
	DefaultURL.LoadURLConfig( TEXT("DefaultPlayer"), GGameIni );

	// convert commandline to a URL
	FString Error;
	TCHAR Parm[4096]=TEXT("");

#if UE_BUILD_SHIPPING
	// In shipping don't allow an override
	CommandLine = NULL;
#endif // UE_BUILD_SHIPPING

	const TCHAR* Tmp = CommandLine ? CommandLine : TEXT("");
	if (!FParse::Token(Tmp, Parm, ARRAY_COUNT(Parm), 0) || Parm[0] == '-')
	{
		const UGameMapsSettings* GameMapsSettings = GetDefault<UGameMapsSettings>();
		FCString::Strcpy(Parm, *(GameMapsSettings->GetGameDefaultMap() + GameMapsSettings->LocalMapOptions));
	}
	FURL URL(&DefaultURL, Parm, TRAVEL_Partial);

	// strip off extension of the map if there is one
	return FPaths::GetBaseFilename(URL.Map);
}

void appGetAllPotentialStartupPackageNames(TArray<FString>& PackageNames, const FString& EngineConfigFilename, bool bIsCreatingHashes)
{
	// startup packages from .ini
	FStartupPackages::GetStartupPackageNames(PackageNames, EngineConfigFilename, bIsCreatingHashes);

	// add the startup map
	PackageNames.Add(*appGetStartupMap(NULL));

	//@todo-packageloc Handle localized packages.
}

#if WITH_EDITOR
FOnSwitchWorldForPIE FScopedConditionalWorldSwitcher::SwitchWorldForPIEDelegate;

FScopedConditionalWorldSwitcher::FScopedConditionalWorldSwitcher( FViewportClient* InViewportClient )
	: ViewportClient( InViewportClient )
	, OldWorld( NULL )
{
	if( GIsEditor )
	{
		if( ViewportClient && ViewportClient == GEngine->GameViewport && !GIsPlayInEditorWorld )
		{
			OldWorld = GWorld; 
			const bool bSwitchToPIEWorld = true;
			// Delegate must be valid
			SwitchWorldForPIEDelegate.ExecuteIfBound( bSwitchToPIEWorld );
		} 
		else if( ViewportClient )
		{
			// Tell the viewport client to set the correct world and store what the world used to be
			OldWorld = ViewportClient->ConditionalSetWorld();
		}
	}
}

FScopedConditionalWorldSwitcher::~FScopedConditionalWorldSwitcher()
{
	// Only switch in the editor and if we made a swtich (OldWorld not null)
	if( GIsEditor && OldWorld )
	{
		if( ViewportClient && ViewportClient == GEngine->GameViewport && GIsPlayInEditorWorld )
		{
			const bool bSwitchToPIEWorld = false;
			// Delegate must be valid
			SwitchWorldForPIEDelegate.ExecuteIfBound( bSwitchToPIEWorld );
		} 
		else if( ViewportClient )
		{
			// Tell the viewport client to restore the old world
			ViewportClient->ConditionalRestoreWorld( OldWorld );
		}
	}
}

#endif

void UEngine::OverrideSelectedMaterialColor( const FLinearColor& OverrideColor )
{
	bIsOverridingSelectedColor = true;
	SelectedMaterialColorOverride = OverrideColor;
}

void UEngine::RestoreSelectedMaterialColor()
{
	bIsOverridingSelectedColor = false;
}

void UEngine::WorldAdded( UWorld* InWorld )
{
	WorldAddedEvent.Broadcast( InWorld );
}

void UEngine::WorldDestroyed( UWorld* InWorld )
{
	WorldDestroyedEvent.Broadcast( InWorld );
}

UWorld* UEngine::GetWorldFromContextObject(const UObject* Object, const bool bChecked) const
{
	if (!bChecked && Object == NULL)
	{
		return NULL;
	}

	check(Object);

	bool bSupported = true;
	UWorld* World = (bChecked ? Object->GetWorldChecked(bSupported) : Object->GetWorld());
	return (bSupported ? World : GWorld);
}

TArray<class ULocalPlayer*>::TConstIterator	UEngine::GetLocalPlayerIterator(UWorld *World)
{
	return GetGamePlayers(World).CreateConstIterator();
}

TArray<class ULocalPlayer*>::TConstIterator UEngine::GetLocalPlayerIterator(const UGameViewportClient *Viewport)
{
	return GetGamePlayers(Viewport).CreateConstIterator();
}

const TArray<class ULocalPlayer*>& UEngine::GetGamePlayers(UWorld *World)
{
	const FWorldContext &Context = GetWorldContextFromWorldChecked(World);
	return Context.GamePlayers;
}
	
const TArray<class ULocalPlayer*>& UEngine::GetGamePlayers(const UGameViewportClient *Viewport)
{
	return GetWorldContextFromGameViewportChecked(Viewport).GamePlayers;
}

ULocalPlayer* UEngine::LocalPlayerFromVoiceIndex(int32 VoiceId) const
{
	// Search for the first Game or PIE instance. This is imperfect and means we cannot support voice chat properly for
	// multiple UWorlds (but thats ok for the time being).
	for (auto It=WorldList.CreateConstIterator(); It; ++It)
	{
		const FWorldContext &Context = *It;
		if (Context.World() && (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE))
		{
			// Use this world context, look for the ULocalPlayer with this ControllerId
			for (int32 i=0; i < Context.GamePlayers.Num(); ++i)
			{
				if (Context.GamePlayers[i] && Context.GamePlayers[i]->ControllerId == VoiceId)
				{
					return Context.GamePlayers[i];
				}
			}
		}
	}

	return NULL;
}

int32 UEngine::GetNumGamePlayers(UWorld *InWorld)
{
	return GetGamePlayers(InWorld).Num();
}

int32 UEngine::GetNumGamePlayers(const UGameViewportClient *InViewport)
{
	return GetGamePlayers(InViewport).Num();
}

ULocalPlayer* UEngine::GetGamePlayer( UWorld * InWorld, int32 InPlayer )
{
	const TArray<class ULocalPlayer*>& PlayerList = GetGamePlayers(InWorld);
	check( InPlayer < PlayerList.Num() );
	return PlayerList[ InPlayer ];
}

ULocalPlayer* UEngine::GetGamePlayer( const UGameViewportClient * InViewport, int32 InPlayer )
{
	const TArray<class ULocalPlayer*>& PlayerList = GetGamePlayers(InViewport);
	check( InPlayer < PlayerList.Num() );
	return PlayerList[ InPlayer ];
}
	
ULocalPlayer* UEngine::GetFirstGamePlayer(UWorld *InWorld)
{
	const TArray<class ULocalPlayer*>& PlayerList = GetGamePlayers(InWorld);
	return PlayerList.Num() != 0 ? PlayerList[0] : NULL;
}

ULocalPlayer* UEngine::GetFirstGamePlayer( UPendingNetGame *PendingNetGame )
{
	for (auto It = WorldList.CreateConstIterator(); It; ++It)
	{
		if (It->PendingNetGame == PendingNetGame)
		{
			return It->GamePlayers.Num() > 0 ? It->GamePlayers[0] : NULL;
		}
	}
	return NULL;
}

ULocalPlayer* UEngine::GetFirstGamePlayer(const UGameViewportClient *InViewport )
{
	for (auto It = WorldList.CreateConstIterator(); It; ++It)
	{
		if (It->GameViewport == InViewport)
		{
			return It->GamePlayers.Num() > 0 ? It->GamePlayers[0] : NULL;
		}
	}
	return NULL;
}

ULocalPlayer* UEngine::GetDebugLocalPlayer()
{
	for (auto It = WorldList.CreateConstIterator(); It; ++It)
	{
		if (It->World() && It->GamePlayers.Num() > 0 )
		{
			return It->GamePlayers[0];
		}
	}
	return NULL;
}

void UEngine::AddGamePlayer( UWorld *InWorld, ULocalPlayer* InPlayer )
{
	GetWorldContextFromWorldChecked(InWorld).GamePlayers.AddUnique(InPlayer);
}

void UEngine::AddGamePlayer( const UGameViewportClient *InViewport, ULocalPlayer* InPlayer )
{
	GetWorldContextFromGameViewportChecked(InViewport).GamePlayers.AddUnique(InPlayer);
}

bool RemoveGamePlayer_Local(TArray<class ULocalPlayer*>& PlayerList, int32 InPlayerIndex)
{
	bool bResult = true;
	if( PlayerList.IsValidIndex( InPlayerIndex ) )
	{
		PlayerList.RemoveAt(InPlayerIndex);		
	}
	else
	{
		bResult = false;
	}
	return bResult;
}

bool UEngine::RemoveGamePlayer( UWorld *InWorld, int32 InPlayerIndex )
{
	TArray<class ULocalPlayer*>& PlayerList = const_cast<TArray<class ULocalPlayer*> & >(GetGamePlayers(InWorld));
	return RemoveGamePlayer_Local(PlayerList, InPlayerIndex);
}

bool UEngine::RemoveGamePlayer( const UGameViewportClient *InViewport, int32 InPlayerIndex )
{
	TArray<class ULocalPlayer*>& PlayerList = const_cast<TArray<class ULocalPlayer*> & >(GetGamePlayers(InViewport));
	return RemoveGamePlayer_Local(PlayerList, InPlayerIndex);
}

#if !UE_BUILD_SHIPPING
// Moved this class from Class.cpp because FExportObjectInnerContext is defined in Engine
static class FCDODump : private FSelfRegisteringExec
{
	static FString ObjectString(UObject* Object)
	{
		UnMarkAllObjects(EObjectMark(OBJECTMARK_TagExp | OBJECTMARK_TagImp));

		FStringOutputDevice Archive;
		const FExportObjectInnerContext Context;
		UExporter::ExportToOutputDevice(&Context, Object, NULL, Archive, TEXT("copy"), 0, PPF_Copy | PPF_DebugDump, false);
		Archive.Log(TEXT("\r\n\r\n"));

		FString ExportedText;
		ExportedText = Archive;
		return ExportedText;
	}

	/** Console commands, see embeded usage statement **/
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override
	{
		if(FParse::Command(&Cmd,TEXT("CDODump")))
		{
			FString All;
			TArray<UClass*> Classes;
			for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
			{
				UClass* Cls = *ClassIt;
				if (!Cls->IsChildOf(UClass::StaticClass()) && (Cls != UObject::StaticClass()) && (Cls->GetName() != TEXT("World")) && (Cls->GetName() != TEXT("Level")))
				{
					Classes.Add(Cls);
				}
			}
			Classes.Sort();

			for (int32 Index = 0; Index < Classes.Num(); Index++)
			{
				All += ObjectString(Classes[Index]->GetDefaultObject());
			}
			FString Filename = FPaths::GameSavedDir() / TEXT("CDO.txt");
			verify(FFileHelper::SaveStringToFile(All, *Filename));
			return true;
		}
		return false;
	}
} CDODump;
#endif // !UE_BUILD_SHIPPING

void UEngine::ShutdownWorldNetDriver( UWorld * World )
{
	if (World)
	{
		/**
		 * Shut down the world's net driver, completely disconnecting any clients/servers connected 
		 * at the time.   Destroys the net driver.
		 */
		UNetDriver* NetDriver = World->GetNetDriver();
		if (NetDriver)
		{
			UE_LOG(LogNet, Log, TEXT("World NetDriver shutdown %s [%s]"), *NetDriver->GetName(), *NetDriver->NetDriverName.ToString());
			DestroyNamedNetDriver(World, NetDriver->NetDriverName);
		}
	}
}

void UEngine::ShutdownAllNetDrivers()
{
	for (auto It = WorldList.CreateIterator(); It; ++It)
	{
		TArray<FNamedNetDriver> & ActiveNetDrivers = It->ActiveNetDrivers;

	for (int32 Index = 0; Index < ActiveNetDrivers.Num(); Index++)
	{
		FNamedNetDriver& NamedNetDriver = ActiveNetDrivers[Index];
		UNetDriver* NetDriver = NamedNetDriver.NetDriver;
		if (NetDriver)
		{
			UE_LOG(LogNet, Log, TEXT("World NetDriver shutdown %s [%s]"), *NetDriver->GetName(), *NetDriver->NetDriverName.ToString());
				UWorld * World = NetDriver->GetWorld();
				if (World)
				{
					World->SetNetDriver(NULL);
				}
				NetDriver->SetWorld(NULL);
				DestroyNamedNetDriver(It->World(), NetDriver->NetDriverName);
		}
	}

	ActiveNetDrivers.Empty();
}
}

UNetDriver* FindNamedNetDriver_Local(const TArray<FNamedNetDriver>& ActiveNetDrivers, FName NetDriverName)
{
	for (int32 Index = 0; Index < ActiveNetDrivers.Num(); Index++)
	{
		const FNamedNetDriver& NamedNetDriver = ActiveNetDrivers[Index];
		UNetDriver* NetDriver = NamedNetDriver.NetDriver;
		if (NetDriver && NetDriver->NetDriverName == NetDriverName)
		{
			return NetDriver;
		}
	}
	return NULL;
}

UNetDriver* UEngine::FindNamedNetDriver(UWorld * InWorld, FName NetDriverName)
{
	return FindNamedNetDriver_Local(GetWorldContextFromWorldChecked(InWorld).ActiveNetDrivers, NetDriverName);
}

UNetDriver* UEngine::FindNamedNetDriver(const UPendingNetGame * InPendingNetGame, FName NetDriverName)
{
	return FindNamedNetDriver_Local(GetWorldContextFromPendingNetGameChecked(InPendingNetGame).ActiveNetDrivers, NetDriverName);
}

bool CreateNamedNetDriver_Local(UEngine *Engine, FWorldContext &Context, FName NetDriverName, FName NetDriverDefinition)
{
	UNetDriver* NetDriver = FindNamedNetDriver_Local(Context.ActiveNetDrivers, NetDriverName);
	if (NetDriver == NULL)
	{
		for (int32 Index = 0; Index < Engine->NetDriverDefinitions.Num(); Index++)
		{
			FNetDriverDefinition& NetDriverDef = Engine->NetDriverDefinitions[Index];
			if (NetDriverDef.DefName == NetDriverDefinition)
			{
				// find the class to load
				UClass* NetDriverClass = StaticLoadClass(UNetDriver::StaticClass(), NULL, *NetDriverDef.DriverClassName.ToString(), NULL, LOAD_Quiet, NULL);

				// if it fails, then fall back to standard fallback
				if (NetDriverClass == NULL || !NetDriverClass->GetDefaultObject<UNetDriver>()->IsAvailable())
				{
					NetDriverClass = StaticLoadClass(UNetDriver::StaticClass(), NULL, *NetDriverDef.DriverClassNameFallback.ToString(), NULL, LOAD_None, NULL);
				}

				// Bail out if the net driver isn't available. The name may be incorrect or the class might not be built as part of the game configuration.
				if(NetDriverClass == NULL)
				{
					break;
				}

				// Try to create network driver.
				NetDriver = ConstructObject<UNetDriver>(NetDriverClass);
				check(NetDriver);
				NetDriver->NetDriverName = NetDriverName;
				
				new (Context.ActiveNetDrivers) FNamedNetDriver(NetDriver, &NetDriverDef);
				return true;
			}
		}
	}

	if (NetDriver)
	{
		UE_LOG(LogNet, Log, TEXT("CreateNamedNetDriver %s already exists as %s"), *NetDriverName.ToString(), *NetDriver->GetName());
	}
	else
	{
		UE_LOG(LogNet, Log, TEXT("CreateNamedNetDriver failed to create driver %s from definition %s"), *NetDriverName.ToString(), *NetDriverDefinition.ToString());
	}
	
	return false;
}

bool UEngine::CreateNamedNetDriver(UWorld *InWorld, FName NetDriverName, FName NetDriverDefinition)
{
	return CreateNamedNetDriver_Local( this, GetWorldContextFromWorldChecked(InWorld), NetDriverName, NetDriverDefinition );
}

bool UEngine::CreateNamedNetDriver(UPendingNetGame *PendingNetGame, FName NetDriverName, FName NetDriverDefinition)
{
	return CreateNamedNetDriver_Local( this, GetWorldContextFromPendingNetGameChecked(PendingNetGame), NetDriverName, NetDriverDefinition);
}

void DestroyNamedNetDriver_Local(FWorldContext &Context, FName NetDriverName)
{
	for (int32 Index = 0; Index < Context.ActiveNetDrivers.Num(); Index++)
	{
		FNamedNetDriver& NamedNetDriver = Context.ActiveNetDrivers[Index];
		UNetDriver* NetDriver = NamedNetDriver.NetDriver;
		if (NetDriver && NetDriver->NetDriverName == NetDriverName)
		{
			UE_LOG(LogNet, Log, TEXT("DestroyNamedNetDriver %s [%s]"), *NetDriver->GetName(), *NetDriverName.ToString());
			NetDriver->SetWorld(NULL);
			NetDriver->Shutdown();
			NetDriver->LowLevelDestroy();
			Context.ActiveNetDrivers.RemoveAtSwap(Index);
			break;
		}
	}
}

void UEngine::DestroyNamedNetDriver(UWorld *InWorld, FName NetDriverName)
{
	DestroyNamedNetDriver_Local( GetWorldContextFromWorldChecked(InWorld), NetDriverName);
}

void UEngine::DestroyNamedNetDriver(UPendingNetGame *PendingNetGame, FName NetDriverName)
{
	DestroyNamedNetDriver_Local( GetWorldContextFromPendingNetGameChecked(PendingNetGame), NetDriverName );
}

ENetMode UEngine::GetNetMode(const UWorld *World) const
{ 
	if (World)
	{
		return World->GetNetMode();
	}

	return NM_Standalone;
}

static inline void CallHandleDisconnectForFailure(UWorld* InWorld, UNetDriver* NetDriver)
{
	// No world will be created yet if you fail to initialize network driver while trying to connect via cmd line arg.
	if( InWorld )
	{
		AGameMode* const GameMode = InWorld->GetAuthGameMode();
		if (GameMode)
		{
			// Mark the server as having a problem
			GameMode->AbortMatch();
		}
	}
	
	// A valid world or NetDriver is required to look up a ULocalPlayer.
	if (InWorld)
	{
		ULocalPlayer* const LP = GEngine->GetFirstGamePlayer(InWorld);
		check(LP);
		LP->HandleDisconnect(InWorld, NetDriver);
	}
	else if(NetDriver && NetDriver->NetDriverName == NAME_PendingNetDriver)
	{
		// The only disconnect case without a valid InWorld, should be in a travel case where there is a pending game net driver.
		FWorldContext &Context = GEngine->GetWorldContextFromPendingNetGameNetDriverChecked(NetDriver);
		check(Context.GamePlayers.Num() > 0);

		ULocalPlayer* const LP = Context.GamePlayers[0];
		check(LP);
		LP->HandleDisconnect(InWorld, NetDriver);
	}
	else
	{
		// Handle disconnect should always have a valid world or net driver to give the call context
		UE_LOG(LogNet, Error, TEXT("CallHandleDisconnectForFailure called without valid world or netdriver. (NetDriver: %s"), NetDriver ? *NetDriver->GetName() : TEXT("NULL") );
	}
}

void UEngine::HandleTravelFailure(UWorld* InWorld, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (InWorld == NULL)
	{
		UE_LOG(LogNet, Error, TEXT("TravelFailure: %s, Reason for Failure: '%s' with a NULL UWorld"), ETravelFailure::ToString(FailureType), *ErrorString);
		return;
	}

	UE_LOG(LogNet, Log, TEXT("TravelFailure: %s, Reason for Failure: '%s'"), ETravelFailure::ToString(FailureType), *ErrorString);

	ENetMode NetMode = GetNetMode(InWorld);

	switch (FailureType)
	{
	case ETravelFailure::PackageMissing:
	case ETravelFailure::PackageVersion:
	case ETravelFailure::NoDownload:
	case ETravelFailure::NoLevel:
	case ETravelFailure::InvalidURL:
	case ETravelFailure::TravelFailure:
	case ETravelFailure::CheatCommands:
	case ETravelFailure::PendingNetGameCreateFailure:
	default:
		break;
	}

	// Cancel pending net game if there was one
	CancelPending(InWorld);

	// Any of these errors should attempt to load back to some stable map
	CallHandleDisconnectForFailure(InWorld, InWorld->GetNetDriver());
}

void UEngine::HandleNetworkFailure(UWorld *World, UNetDriver *NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogNet, Log, TEXT("NetworkFailure: %s, Error: '%s'"), ENetworkFailure::ToString(FailureType), *ErrorString);

	// Only handle failure at this level for game or pending net drivers.
	FName NetDriverName = NetDriver ? NetDriver->NetDriverName : NAME_None;
	if (NetDriverName == NAME_GameNetDriver || NetDriverName == NAME_PendingNetDriver)
	{
		// If this net driver has already been unregistered with this world, then don't handle it.
		if (World)
		{
			if (!FindNamedNetDriver(World, NetDriverName))
			{
				// This netdriver has already been destroyed (probably waiting for GC)
				return;
			}
		}

		ENetMode FailureNetMode = NetDriver->GetNetMode();	// NetMode of the driver that failed
		bool bShouldTravel = true;

		switch (FailureType)
		{
		case ENetworkFailure::FailureReceived:
			break;
		case ENetworkFailure::PendingConnectionFailure:
			// TODO stop the connecting movie
			break;
		case ENetworkFailure::ConnectionLost:
			// Hosts don't travel when clients disconnect
			bShouldTravel = (FailureNetMode == NM_Client);
			break;
		case ENetworkFailure::ConnectionTimeout:
			// Hosts don't travel when clients disconnect
			bShouldTravel = (FailureNetMode == NM_Client);
			break;
		case ENetworkFailure::NetDriverAlreadyExists:
		case ENetworkFailure::NetDriverCreateFailure:
		case ENetworkFailure::OutdatedClient:
		case ENetworkFailure::OutdatedServer:
		default:
			break;
		}

		if (bShouldTravel)
		{
			CallHandleDisconnectForFailure(World, NetDriver);
		}
	}
}

void UEngine::SpawnServerActors(UWorld *World)
{
	for( int32 i=0; i < ServerActors.Num(); i++ )
	{
		TCHAR Str[240];
		const TCHAR* Ptr = * ServerActors[i];
		if( FParse::Token( Ptr, Str, ARRAY_COUNT(Str), 1 ) )
		{
			UE_LOG(LogNet, Log, TEXT("Spawning: %s"), Str );
			UClass* HelperClass = StaticLoadClass( AActor::StaticClass(), NULL, Str, NULL, LOAD_None, NULL );
			AActor* Actor = World->SpawnActor( HelperClass );
			while( Actor && FParse::Token(Ptr,Str,ARRAY_COUNT(Str),1) )
			{
				TCHAR* Value = FCString::Strchr(Str,'=');
				if( Value )
				{
					*Value++ = 0;
					for( TFieldIterator<UProperty> It(Actor->GetClass()); It; ++It )
					{
						if(	FCString::Stricmp(*It->GetName(),Str)==0
							&&	(It->PropertyFlags & CPF_Config) )
						{
							It->ImportText( Value, It->ContainerPtrToValuePtr<uint8>(Actor), 0, Actor );
						}
					}
				}
			}
		}
	}
}

bool UEngine::HandleOpenCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld *InWorld  )
{
	FWorldContext &WorldContext = GetWorldContextFromWorldChecked(InWorld);
	FURL TestURL(&WorldContext.LastURL, Cmd, TRAVEL_Absolute);
	if (TestURL.IsLocalInternal())
	{
		// make sure the file exists if we are opening a local file
		if (!MakeSureMapNameIsValid(TestURL.Map))
		{
			Ar.Logf(TEXT("ERROR: The map '%s' does not exist."), *TestURL.Map);
			return true;
		}
	}

	SetClientTravel( InWorld, Cmd, TRAVEL_Absolute );
	return true;
}

bool UEngine::HandleTravelCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	FWorldContext &WorldContext = GetWorldContextFromWorldChecked(InWorld);
	FURL TestURL(&WorldContext.LastURL, Cmd, TRAVEL_Partial);
	if (TestURL.IsLocalInternal())
	{
		// make sure the file exists if we are opening a local file
		bool bMapFound = MakeSureMapNameIsValid(TestURL.Map);
		if (!bMapFound)
		{
			Ar.Logf(TEXT("ERROR: The map '%s' does not exist."), *TestURL.Map);
			return true;
		}
	}

	SetClientTravel( InWorld, Cmd, TRAVEL_Partial );
	return true;
}

bool UEngine::HandleStreamMapCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld *InWorld )
{
	FWorldContext &WorldContext = GetWorldContextFromWorldChecked(InWorld);
	FURL TestURL(&WorldContext.LastURL, Cmd, TRAVEL_Partial);
	if (TestURL.IsLocalInternal())
	{
		// make sure the file exists if we are opening a local file
		if (MakeSureMapNameIsValid(WorldContext.LastURL.Map))
		{
			TArray<FName> LevelNames;
			LevelNames.Add(*TestURL.Map);

			FWorldContext &Context = GetWorldContextFromWorldChecked(InWorld);

			PrepareMapChange(Context, LevelNames);
			Context.bShouldCommitPendingMapChange = true;
			ConditionalCommitMapChange(Context);
		}
		else
		{
			Ar.Logf(TEXT("ERROR: The map '%s' does not exist."), *TestURL.Map);
		}
	}
	else
	{
		Ar.Logf(TEXT("ERROR: Can only perform streaming load for local URLs."));
	}
	return true;
}

#if WITH_SERVER_CODE
bool UEngine::HandleServerTravelCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	if( InWorld->IsServer() )
	{
		FString MapName = Cmd;
		if (MakeSureMapNameIsValid(MapName))
		{
			InWorld->ServerTravel(MapName);
		}
		else
		{
			Ar.Logf(TEXT("ERROR: The map '%s' is either short package name or does not exist."), *MapName);
		}
		return true;
	}
	return false;
}

bool UEngine::HandleSayCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	if(GIsServer && !GIsClient)
	{
		AGameMode* const GameMode = InWorld->GetAuthGameMode();
		GameMode->Broadcast(NULL, Cmd, NAME_None);
		return true;
	}
	return false;
}
#endif

bool UEngine::HandleDisconnectCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld *InWorld )
{
	// This should only be called from typing 'disconnect' at the console. InWorld must have a valid WorldContext.
	check(InWorld);
	check(GetWorldContextFromWorld(InWorld));

	HandleDisconnect(InWorld, InWorld->GetNetDriver());
	return true;
}

void UEngine::HandleDisconnect( UWorld *InWorld, UNetDriver *NetDriver )
{
	// There must be some context for this disconnect
	check(InWorld || NetDriver);

	// If the NetDriver that failed was a pending netgame driver, cancel the PendingNetGame
	CancelPending(NetDriver);

	// InWorld might be null. It might also not map to any valid world context (for example, a pending net game disconnect)
	// If there is a context for this world, setup client travel.
	if (FWorldContext* WorldContext = GetWorldContextFromWorld(InWorld))
	{
		if (InWorld)
		{
			// If we have a world, then the failing NetDriver must be the world' net driver
			check(InWorld->GetNetDriver() == NetDriver);
		}

		// Remove ?Listen parameter, if it exists
		WorldContext->LastURL.RemoveOption( TEXT("Listen") );
		WorldContext->LastURL.RemoveOption( TEXT("LAN") );

		SetClientTravel( InWorld, TEXT("?closed"), TRAVEL_Absolute );
	}

	// Shut down any existing game connections
	if (NetDriver)
	{
		if (InWorld)
		{
			// Call this to remove the NetDriver from the world context's ActiveNetDriver list
			DestroyNamedNetDriver(InWorld, NetDriver->NetDriverName);
		}
		else
		{
			NetDriver->Shutdown();
			NetDriver->LowLevelDestroy();
		}
	}
}

bool UEngine::HandleReconnectCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld *InWorld )
{
	FWorldContext &WorldContext = GetWorldContextFromWorldChecked(InWorld);
	if (WorldContext.LastRemoteURL.Valid && WorldContext.LastRemoteURL.Host != TEXT(""))
	{
		SetClientTravel(InWorld, *WorldContext.LastRemoteURL.ToString(), TRAVEL_Absolute);
	}
	return true;
}

bool UEngine::MakeSureMapNameIsValid(FString& InOutMapName)
{
	// Check if the map name is long package name and if it actually exists.
	// Short package names are only supported in non-shipping builds.
	bool bIsValid = !FPackageName::IsShortPackageName(InOutMapName);
	if (bIsValid)
	{
		bIsValid = FPackageName::DoesPackageExist(InOutMapName);
	}
	else
	{
		// Look up on disk. Slow!
		FString LongPackageName;
		bIsValid = FPackageName::SearchForPackageOnDisk(InOutMapName, &LongPackageName);
		if (bIsValid)
		{
			InOutMapName = LongPackageName;
		}
	}
	return bIsValid;
}

void UEngine::SetClientTravel( UWorld *InWorld, const TCHAR* NextURL, ETravelType InTravelType )
{
	FWorldContext &Context = GetWorldContextFromWorldChecked(InWorld);

	// set TravelURL.  Will be processed safely on the next tick in UGameEngine::Tick().
	Context.TravelURL    = NextURL;
	Context.TravelType   = InTravelType;

	// Prevent crashing the game by attempting to connect to own listen server
	if ( Context.LastURL.HasOption(TEXT("Listen")) )
	{
		Context.LastURL.RemoveOption(TEXT("Listen"));
	}
}

void UEngine::SetClientTravel( UPendingNetGame *PendingNetGame, const TCHAR* NextURL, ETravelType InTravelType )
{
	FWorldContext &Context = GetWorldContextFromPendingNetGameChecked(PendingNetGame);

	// set TravelURL.  Will be processed safely on the next tick in UGameEngine::Tick().
	Context.TravelURL    = NextURL;
	Context.TravelType   = InTravelType;

	// Prevent crashing the game by attempting to connect to own listen server
	if ( Context.LastURL.HasOption(TEXT("Listen")) )
	{
		Context.LastURL.RemoveOption(TEXT("Listen"));
	}
}

void UEngine::SetClientTravelFromPendingGameNetDriver( UNetDriver *PendingGameNetDriverGame, const TCHAR* NextURL, ETravelType InTravelType )
{
	// Find WorldContext whose pendingNetGame's NetDriver is the passed in net driver
	for( int32 idx=0; idx < WorldList.Num(); ++idx)
	{
		FWorldContext &Context = WorldList[idx];
		if (Context.PendingNetGame && Context.PendingNetGame->NetDriver == PendingGameNetDriverGame)
		{
			SetClientTravel( Context.PendingNetGame, NextURL, InTravelType );
			return;
		}
	}
	check(false);
}

EBrowseReturnVal::Type UEngine::Browse( FWorldContext& WorldContext, FURL URL, FString& Error )
{
	Error = TEXT("");
	WorldContext.TravelURL = TEXT("");

	// Convert .unreal link files.
	const TCHAR* LinkStr = TEXT(".unreal");//!!
	if( FCString::Strstr(*URL.Map,LinkStr)-*URL.Map==FCString::Strlen(*URL.Map)-FCString::Strlen(LinkStr) )
	{
		UE_LOG(LogNet, Log,  TEXT("Link: %s"), *URL.Map );
		FString NewUrlString;
		if( GConfig->GetString( TEXT("Link")/*!!*/, TEXT("Server"), NewUrlString, *URL.Map ) )
		{
			// Go to link.
			URL = FURL( NULL, *NewUrlString, TRAVEL_Absolute );//!!
		}
		else
		{
			// Invalid link.
			Error = FText::Format( NSLOCTEXT("Engine", "InvalidLink", "Invalid Link: {0}"), FText::FromString( URL.Map ) ).ToString();
			return EBrowseReturnVal::Failure;
		}
	}

	// Crack the URL.
	UE_LOG(LogNet, Log, TEXT("Browse: %s"), *URL.ToString() );

	// Handle it.
	if( !URL.Valid )
	{
		// Unknown URL.
		Error = FText::Format( NSLOCTEXT("Engine", "InvalidUrl", "Invalid URL: {0}"), FText::FromString( URL.ToString() ) ).ToString();
		BroadcastTravelFailure(WorldContext.World(), ETravelFailure::InvalidURL, Error);
		return EBrowseReturnVal::Failure;
	}
	else if (URL.HasOption(TEXT("failed")) || URL.HasOption(TEXT("closed")))
	{
		// Browsing after a failure, load default map

		if (WorldContext.PendingNetGame)
		{
			CancelPending(WorldContext);
		}
		// Handle failure URL.
		UE_LOG(LogNet, Log, TEXT("%s"), TEXT("Failed; returning to Entry") );
		if (WorldContext.World() != NULL)
		{
			ResetLoaders( WorldContext.World()->GetOuter() );
		}
		
		const UGameMapsSettings* GameMapsSettings = GetDefault<UGameMapsSettings>();
		bool LoadSucces = LoadMap(WorldContext, FURL(&URL, *(GameMapsSettings->GetGameDefaultMap() + GameMapsSettings->LocalMapOptions), TRAVEL_Partial), NULL, Error);
		check(LoadSucces);

		CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

		// now remove "failed" and "closed" options from LastURL so it doesn't get copied on to future URLs
		WorldContext.LastURL.RemoveOption(TEXT("failed"));
		WorldContext.LastURL.RemoveOption(TEXT("closed"));
		return EBrowseReturnVal::Success;
	}
	else if( URL.HasOption(TEXT("restart")) )
	{
		// Handle restarting.
		URL = WorldContext.LastURL;
	}

	// Handle normal URL's.
	if (GDisallowNetworkTravel && URL.HasOption(TEXT("listen")))
	{
		Error = NSLOCTEXT("Engine", "UsedCheatCommands", "Console commands were used which are disallowed in netplay.  You must restart the game to create a match.").ToString();
		BroadcastTravelFailure(WorldContext.World(), ETravelFailure::CheatCommands, Error);
		return EBrowseReturnVal::Failure;
	}
	if( URL.IsLocalInternal() )
	{
		// Local map file.
		return LoadMap( WorldContext, URL, NULL, Error ) ? EBrowseReturnVal::Success : EBrowseReturnVal::Failure;
	}
	else if( URL.IsInternal() && GIsClient )
	{
		// Network URL.
		if( WorldContext.PendingNetGame )
		{
			CancelPending(WorldContext);
		}

		// Clean up the netdriver/socket so that the pending level succeeds
		if (WorldContext.World())
		{
			ShutdownWorldNetDriver(WorldContext.World());
		}

		WorldContext.PendingNetGame = new UPendingNetGame(FPostConstructInitializeProperties(), URL);
		WorldContext.PendingNetGame->InitNetDriver();
		if( !WorldContext.PendingNetGame->NetDriver )
		{
			// UPendingNetGame will set the appropriate error code and connection lost type, so
			// we just have to propagate that message to the game.
			BroadcastTravelFailure(WorldContext.World(), ETravelFailure::PendingNetGameCreateFailure, WorldContext.PendingNetGame->ConnectionError);
			WorldContext.PendingNetGame = NULL;
			return EBrowseReturnVal::Failure;
		}
		return EBrowseReturnVal::Pending;
	}
	else if( URL.IsInternal() )
	{
		// Invalid.
		Error = NSLOCTEXT("Engine", "ServerOpen", "Servers can't open network URLs").ToString();
		return EBrowseReturnVal::Failure;
	}
	{
		// External URL - disabled by default.
		// Client->Viewports(0)->Exec(TEXT("ENDFULLSCREEN"));
		// FPlatformProcess::LaunchURL( *URL.ToString(), TEXT(""), &Error );
		return EBrowseReturnVal::Failure;
	}
}

void UEngine::CancelPending(UNetDriver* PendingNetGameDriver)
{
	if (PendingNetGameDriver==NULL)
	{
		return;
	}

	// Find WorldContext whose pendingNetGame's NetDriver is the passed in net driver
	for( int32 idx=0; idx < WorldList.Num(); ++idx)
	{
		FWorldContext &Context = WorldList[idx];
		if (Context.PendingNetGame && Context.PendingNetGame->NetDriver == PendingNetGameDriver)
		{
			// Kill this PendingNetGame
			CancelPending(Context);
			check(Context.PendingNetGame == NULL);	// Verify PendingNetGame was cleared in CancelPending
		}
	}
}

void UEngine::CancelPending(FWorldContext &Context)
{
	if (Context.PendingNetGame && Context.PendingNetGame->NetDriver && Context.PendingNetGame->NetDriver->ServerConnection)
	{
		Context.PendingNetGame->NetDriver->ServerConnection->Close();
		DestroyNamedNetDriver_Local(Context, Context.PendingNetGame->NetDriver->NetDriverName);
		Context.PendingNetGame->NetDriver = NULL;
	}

	Context.PendingNetGame = NULL;
}

bool UEngine::WorldIsPIEInNewViewport(UWorld *InWorld)
{
	// UEditorEngine will override to check slate state
	return false;
}

void UEngine::CancelPending(UWorld *InWorld, UPendingNetGame *NewPendingNetGame)
{
	FWorldContext &Context = GetWorldContextFromWorldChecked(InWorld);
	CancelPending(Context);
	Context.PendingNetGame = NewPendingNetGame;
}

void UEngine::CancelAllPending()
{
	for( int32 idx=0; idx < WorldList.Num(); ++idx)
	{
		FWorldContext &Context = WorldList[idx];
		CancelPending(Context);
	}
}

void UEngine::BrowseToDefaultMap( FWorldContext& Context )
{
	FString Error;
	FURL DefaultURL;
	DefaultURL.LoadURLConfig( TEXT("DefaultPlayer"), GGameIni );
	const UGameMapsSettings* GameMapsSettings = GetDefault<UGameMapsSettings>();

	if (Browse( Context, FURL(&DefaultURL, *(GameMapsSettings->GetGameDefaultMap() + GameMapsSettings->LocalMapOptions), TRAVEL_Partial), Error ) != EBrowseReturnVal::Success)
	{
		UE_LOG(LogLoad, Fatal, TEXT("%s"), *Error);
	}
}

bool UEngine::TickWorldTravel(FWorldContext& Context, float DeltaSeconds)
{
	// Handle seamless traveling
	if (Context.SeamlessTravelHandler.IsInTransition())
	{
		// Note: SeamlessTravelHandler.Tick may automatically update Context.World and GWorld internally
		Context.SeamlessTravelHandler.Tick();
	}

	// Handle server traveling.
	if( !Context.World()->NextURL.IsEmpty() )
	{
		Context.World()->NextSwitchCountdown -= DeltaSeconds;
		if( Context.World()->NextSwitchCountdown <= 0.f )
		{
			UE_LOG(LogEngine, Log,  TEXT("Server switch level: %s"), *Context.World()->NextURL );
			if (Context.World()->GetAuthGameMode() != NULL)
			{
				Context.World()->GetAuthGameMode()->StartToLeaveMap();
			}
			FString Error;
			FString NextURL = Context.World()->NextURL;
			EBrowseReturnVal::Type Ret = Browse( Context, FURL(&Context.LastURL,*NextURL,(ETravelType)Context.World()->NextTravelType), Error );
			if (Ret != EBrowseReturnVal::Success )
			{
				UE_LOG(LogLoad, Warning, TEXT("UEngine::TickWorldTravel failed to Handle server travel to URL: %s. Error: %s"), *NextURL, *Error);
				check(Ret != EBrowseReturnVal::Pending); // server travel should never create a pending net game

				// Failed to load a new map
				if (Context.World() != NULL)
				{
					// If we didn't change worlds, clear out NextURL so we don't do this again next frame.
					Context.World()->NextURL = TEXT("");
				}
				else
				{
					// Our old world got stomped out. Load the default map
					BrowseToDefaultMap(Context);
				}

				// Let people know that we failed to server travel
				BroadcastTravelFailure(Context.World(), ETravelFailure::ServerTravelFailure, Error);
			}
			return false;
		}
	}

	// Handle client traveling.
	if( !Context.TravelURL.IsEmpty() )
	{	
		AGameMode* const GameMode = Context.World()->GetAuthGameMode();
		if (GameMode)
		{
			GameMode->StartToLeaveMap();
		}

		FString Error, TravelURLCopy = Context.TravelURL;
		if (Browse( Context, FURL(&Context.LastURL,*TravelURLCopy,(ETravelType)Context.TravelType), Error ) == EBrowseReturnVal::Failure)
		{
			// If the failure resulted in no world being loaded (we unloaded our last world, then failed to load the new one)
			// then load the default map to avoid getting in a situation where we have no valid UWorld.
			if (Context.World() == NULL)
			{
				BrowseToDefaultMap(Context);
			}

			// Let people know that we failed to client travel
			BroadcastTravelFailure(Context.World(), ETravelFailure::ClientTravelFailure, Error);
		}
		check(Context.World() != NULL);
		return false;
	}

	// Update the pending level.
	if( Context.PendingNetGame )
	{
		Context.PendingNetGame->Tick( DeltaSeconds );
		if ( Context.PendingNetGame && Context.PendingNetGame->ConnectionError.Len() > 0 )
		{
			BroadcastNetworkFailure(NULL, Context.PendingNetGame->NetDriver, ENetworkFailure::PendingConnectionFailure, Context.PendingNetGame->ConnectionError);
			CancelPending(Context);
		}
		else if( Context.PendingNetGame && Context.PendingNetGame->bSuccessfullyConnected && !Context.PendingNetGame->bSentJoinRequest )
		{
			// Attempt to load the map.
			FString Error;
			
			const bool bLoadedMapSuccessfully = LoadMap( Context, Context.PendingNetGame->URL, Context.PendingNetGame, Error );
				
			if( !bLoadedMapSuccessfully || Error != TEXT("") )
			{
				// we can't guarantee the current World is in a valid state, so travel to the default map
				BrowseToDefaultMap(Context);
				BroadcastTravelFailure(Context.World(), ETravelFailure::LoadMapFailure, Error);
				check(Context.World() != NULL);
			}
			else
			{
				// Show connecting message, cause precaching to occur.
				TransitionType = TT_Connecting;
					
				RedrawViewports();

				// Send join.
				Context.PendingNetGame->SendJoin();
				Context.PendingNetGame->NetDriver = NULL;
			}

			// Kill the pending level.
			Context.PendingNetGame = NULL;
		}
	}
	else if (TransitionType == TT_WaitingToConnect)
	{
		TransitionType = TT_None;
	}

	return true;
}

/**
* Finds object referencer in the content package and sets it in the global referencer list
*
* @param MPContentPackage - content package which has an obj referencer
* @param GameEngine - current game engine instance
* @param ContentType - EMPContentReferencerTypes entry for global content package type
*/
static void SetGametypeContentObjectReferencers(UObject* GametypeContentPackage, FName ContextHandle, EGametypeContentReferencerTypes ContentType)
{
	FWorldContext &WorldContext = GEngine->GetWorldContextFromHandleChecked(ContextHandle);

	// Make sure to allocate enough referencer entries
	if ( WorldContext.ObjectReferencers.Num() < MAX_ReferencerIndex )
	{
		WorldContext.ObjectReferencers.AddZeroed( MAX_ReferencerIndex );
	}
	// Release any previous object referencer
	WorldContext.ObjectReferencers[ContentType] = NULL;

	if( GametypeContentPackage )
	{	
		// Find the object referencer in the content package. There should only be one
		UObjectReferencer* ObjectReferencer = NULL;		
		for( TObjectIterator<UObjectReferencer> It; It; ++It )
		{
			if( It->IsIn(GametypeContentPackage) )
			{
				ObjectReferencer = *It;
				break;
			}
		}
		// Keep a reference to it in the game engine
		if( ObjectReferencer )
		{
			WorldContext.ObjectReferencers[ContentType] = ObjectReferencer;
		}
		else
		{
			UE_LOG(LogEngine, Warning, TEXT("MPContentObjectReferencers: Couldn't find object referencer in %s"), 
				*GametypeContentPackage->GetPathName() );
		}
	}
	else
	{
		UE_LOG(LogEngine, Warning, TEXT("MPContentObjectReferencers: package load failed") );
	}
}

/**
 * Callback function for when the localized MP game package is loaded.
 *
 * @param	PackageName			The package name we were trying to load
 * @param	ContentPackage		The package that was loaded.
 * @param	GameEngine			The GameEngine.
 */
static void AsyncLoadLocalizedMapGameTypeContentCallback(const FString& PackageName, UPackage* ContentPackage, FName InContextHandle)
{
	SetGametypeContentObjectReferencers(ContentPackage, InContextHandle, GametypeContent_LocalizedReferencerIndex);
}

/**
 * Callback function for when the MP game package is loaded.
 *
 * @param	PackageName			The package name we were trying to load
 * @param	ContentPackage		The package that was loaded.
 * @param	GameEngine			The GameEngine.
 */
static void AsyncLoadMapGameTypeContentCallback(const FString& PackageName, UPackage* ContentPackage, FName InContextHandle)
{
	SetGametypeContentObjectReferencers(ContentPackage, InContextHandle, GametypeContent_ReferencerIndex);
}

/**
 * Remove object referencer entries for the game type common packages
 */
void FreeGametypeCommonContent(FWorldContext &Context)
{
	UE_LOG(LogEngine, Log,  TEXT("Freeing Gametype Common Content") );
	if (Context.ObjectReferencers.Num() > 0)
	{
		Context.ObjectReferencers[GametypeCommon_ReferencerIndex] = NULL;
		Context.ObjectReferencers[GametypeCommon_LocalizedReferencerIndex] = NULL;
	}
}

/**
 * Parse game type from URL and return standalone seek-free package name for it
 *
 * @param URL 		current URL containing map and game type we are browsing to
 */
FString GetGameModeContentPackageStr(const FURL& URL )
{
	static const FString GAME_CONTENT_PKG_PREFIX(TEXT(""));

	// get game from URL
	FString GameModeClassName( URL.GetOption(TEXT("Game="), TEXT("")) );
	if (GameModeClassName == TEXT(""))
	{	
		// ask the default GameMode what we should use
		UClass* const DefaultGameClass = StaticLoadClass(AGameMode::StaticClass(), NULL, *UGameMapsSettings::GetGlobalDefaultGameMode(), NULL, LOAD_None, NULL);
		if (DefaultGameClass != NULL)
		{	
			FString Options(TEXT(""));
			for (int32 i = 0; i < URL.Op.Num(); i++)
			{
					Options += TEXT("?");
					Options += URL.Op[i];
			}
			GameModeClassName = DefaultGameClass->GetDefaultObject<AGameMode>()->GetDefaultGameClassPath(URL.Map, Options, *URL.Portal);
		}
	}

	// allow for remapping
	GameModeClassName = AGameMode::StaticGetFullGameClassName(GameModeClassName);

	// parse game class from full path
	int32 FoundIdx = GameModeClassName.Find(TEXT("."), ESearchCase::IgnoreCase);
	FString GameClassStr = GameModeClassName.Right(GameModeClassName.Len()-1 - FoundIdx);
	
	return GAME_CONTENT_PKG_PREFIX + GameClassStr + STANDALONE_SEEKFREE_SUFFIX;
}

/**
 * Remove object referencer entries for the game content packages
 */
void FreeGametypeContent(FWorldContext &Context)
{
//	FreeGametypeCommonContent(Context);
	UE_LOG(LogEngine, Log,  TEXT("Freeing Gametype Content") );
	if ( Context.ObjectReferencers.Num() > 0 )
	{
		Context.ObjectReferencers[GametypeContent_ReferencerIndex] = NULL;
		Context.ObjectReferencers[GametypeContent_LocalizedReferencerIndex] = NULL;
	}
}

void LoadGametypeContent_Helper(const FString& ContentStr, 
								FLoadPackageAsyncDelegate CompletionCallback, 
								FLoadPackageAsyncDelegate LocalizedCompletionCallback)
{
	//const TCHAR* Language = *(FInternationalization::GetCurrentCulture()->Name);
	//const FString LocalizedPreloadName(ContentStr + LOCALIZED_SEEKFREE_SUFFIX + TEXT("_") + Language);
	//FString LocalizedPreloadFilename;
	//if (FPackageName::DoesPackageExist(*LocalizedPreloadName, NULL, &LocalizedPreloadFilename))
	//{
	//	UE_LOG(LogEngine, Log, TEXT("Issuing preload for %s"), *LocalizedPreloadFilename);
	//	LoadPackageAsync(LocalizedPreloadFilename, LocalizedCompletionCallback);
	//}

	//@todo-packageloc Load localized packages based on culture.

	FString PreloadFilename;
	if (FPackageName::DoesPackageExist(ContentStr, NULL, &PreloadFilename))
	{
		UE_LOG(LogEngine, Log, TEXT("Issuing preload for %s"), *PreloadFilename);
		LoadPackageAsync(PreloadFilename, CompletionCallback);
	}
}

/**
* Async load the game content standalone seekfree packages for the current game 
*
* @param GameEngine - current game engine instance
* @param URL - current URL containing map and game type we are browsing to
*/
void LoadGametypeContent(FWorldContext &Context, const FURL& URL)
{
	FreeGametypeContent(Context);

	FString GameModeStr = GetGameModeContentPackageStr(URL);
	LoadGametypeContent_Helper(GameModeStr,
		FLoadPackageAsyncDelegate::CreateStatic(&AsyncLoadMapGameTypeContentCallback, Context.ContextHandle),
		FLoadPackageAsyncDelegate::CreateStatic(&AsyncLoadLocalizedMapGameTypeContentCallback, Context.ContextHandle)
		);
}

bool UEngine::LoadMap( FWorldContext& WorldContext, FURL URL, class UPendingNetGame* Pending, FString& Error )
{
	NETWORK_PROFILER(GNetworkProfiler.TrackSessionChange(true,URL));
	MALLOC_PROFILER( FMallocProfiler::SnapshotMemoryLoadMapStart( URL.Map ) );
	Error = TEXT("");
	
	// make sure level streaming isn't frozen
	if (WorldContext.World())
	{
		WorldContext.World()->bIsLevelStreamingFrozen = false;
	}

	// send a callback message
	FCoreDelegates::PreLoadMap.Broadcast();

	// Cancel any pending texture streaming requests.  This avoids a significant delay on consoles 
	// when loading a map and there are a lot of outstanding texture streaming requests from the previous map.
	UTexture2D::CancelPendingTextureStreaming();

	// play a load map movie if specified in ini
	bStartedLoadMapMovie = false;

	// clean up any per-map loaded packages for the map we are leaving
	if (WorldContext.World() && WorldContext.World()->PersistentLevel)
	{
		CleanupPackagesToFullyLoad(WorldContext, FULLYLOAD_Map, WorldContext.World()->PersistentLevel->GetOutermost()->GetName());
	}

	// cleanup the existing per-game pacakges
	// @todo: It should be possible to not unload/load packages if we are going from/to the same GameMode.
	//        would have to save the game pathname here and pass it in to SetGameMode below
	CleanupPackagesToFullyLoad(WorldContext, FULLYLOAD_Game_PreLoadClass, TEXT(""));
	CleanupPackagesToFullyLoad(WorldContext, FULLYLOAD_Game_PostLoadClass, TEXT(""));
	CleanupPackagesToFullyLoad(WorldContext, FULLYLOAD_Mutator, TEXT(""));


	// Cancel any pending async map changes after flushing async loading. We flush async loading before canceling the map change
	// to avoid completion after cancellation to not leave references to the "to be changed to" level around. Async loading is
	// implicitly flushed again later on during garbage collection.
	FlushAsyncLoading();
	CancelPendingMapChange(WorldContext);
	WorldContext.SeamlessTravelHandler.CancelTravel();

	double	StartTime = FPlatformTime::Seconds();
	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Loading URL"), STAT_LoadMap, STATGROUP_LoadTime);

	UE_LOG(LogLoad, Log,  TEXT("LoadMap: %s"), *URL.ToString() );
	GInitRunaway();

	// Get network package map.
	UPackageMap* PackageMap = NULL;
	if( Pending )
	{
		PackageMap = Pending->GetNetDriver()->ServerConnection->PackageMap;
	}

	// Unload the current world
	if( WorldContext.World() )
	{
		// Display loading screen.
		if( !URL.HasOption(TEXT("quiet")) )
		{
			TransitionType = TT_Loading;
			TransitionDescription = URL.Map;
			if (URL.HasOption(TEXT("Game=")))
			{
				TransitionGameMode = URL.GetOption(TEXT("Game="), TEXT(""));
			}
			else
			{
				TransitionGameMode = TEXT("");
			}			
			LoadMapRedrawViewports();			
			TransitionType = TT_None;
		}

		// Clean up networking
		ShutdownWorldNetDriver(WorldContext.World());

		// Clean up game state.
		WorldContext.World()->FlushLevelStreaming( NULL, true );
		
		// send a message that all levels are going away (NULL means every sublevel is being removed
		// without a call to RemoveFromWorld for each)
		FWorldDelegates::LevelRemovedFromWorld.Broadcast(NULL, WorldContext.World());

		// Disassociate the players from their PlayerControllers.
		for(auto It = WorldContext.GamePlayers.CreateIterator(); It; ++It)
		{
			ULocalPlayer *Player = *It;
			if(Player->PlayerController)
			{
				if(Player->PlayerController->GetPawn())
				{
					WorldContext.World()->DestroyActor(Player->PlayerController->GetPawn(), true);
				}
				WorldContext.World()->DestroyActor(Player->PlayerController, true);
				Player->PlayerController = NULL;
			}
			// reset split join info so we'll send one after loading the new map if necessary
			Player->bSentSplitJoin = false;
		}

		for (FActorIterator ActorIt(WorldContext.World()); ActorIt; ++ActorIt)
		{
			if (ActorIt->bActorInitialized)
			{
				ActorIt->EndPlay(EEndPlayReason::LevelTransition);
			}
		}

		// Do this after destroying pawns/playercontrollers, in case that spawns new things (e.g. dropped weapons)
		WorldContext.World()->CleanupWorld();

		if( GEngine )
		{
			// clear any "DISPLAY" properties referencing level objects
			if (GEngine->GameViewport != NULL)
			{
				ClearDebugDisplayProperties();
			}

			GEngine->WorldDestroyed(WorldContext.World());
		}
		WorldContext.World()->RemoveFromRoot();

		WorldContext.SetCurrentWorld(NULL);
	}

	// Stop all audio to remove references to current level.
	if( GEngine && GEngine->GetAudioDevice() )
	{
		GEngine->GetAudioDevice()->Flush( NULL );
		// reset transient volume
		GEngine->GetAudioDevice()->TransientMasterVolume = 1.0;
	}

	if (bCookSeparateSharedMPGameContent)
	{
		UE_LOG(LogLoad, Log,  TEXT("LoadMap: %s: freeing any shared GameMode resources"), *URL.ToString());
		FreeGametypeContent(WorldContext);
	}

	// Clean up the previous level out of memory.
	CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS, true );
	
	// For platforms which manage GPU memory directly we must Enqueue a flush, and wait for it to be processed
	// so that any pending frees that depend on the GPU will be processed.  Otherwise a whole map's worth of GPU memory
	// may be unavailable to load the next one.
	ENQUEUE_UNIQUE_RENDER_COMMAND(FlushCommand, 
		{
			RHIFlushResources();
		}
	);
	FlushRenderingCommands();	  

	// Cancels the Forced StreamType for textures using a timer.
	if (!IStreamingManager::HasShutdown())
	{
		IStreamingManager::Get().CancelForcedResources();
	}

	if (FPlatformProperties::RequiresCookedData())
	{
		appDefragmentTexturePool();
		appDumpTextureMemoryStats(TEXT(""));
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Dump info

	Exec( NULL, TEXT("MEM"));

	VerifyLoadMapWorldCleanup();
	
#endif

	MALLOC_PROFILER( FMallocProfiler::SnapshotMemoryLoadMapMid( URL.Map ); )

	if( GUseSeekFreeLoading )
	{
		// Load GameMode specific data
		if (bCookSeparateSharedMPGameContent)
		{
			UE_LOG(LogLoad, Log,  TEXT("LoadMap: %s: issuing load request for shared GameMode resources"), *URL.ToString());
			LoadGametypeContent(WorldContext, URL);
		}

		// Load localized part of level first in case it exists.
		FString LocalizedMapPackageName	= URL.Map + LOCALIZED_SEEKFREE_SUFFIX;
		FString LocalizedMapFilename;
		if( FPackageName::DoesPackageExist( *LocalizedMapPackageName, NULL, &LocalizedMapFilename ) )
		{
			LoadPackage( NULL, *LocalizedMapPackageName, LOAD_NoWarn );
		}
	}

	UPackage* MapOuter = NULL;

	// in the seekfree case (which hasn't already loaded anything), get linkers for any downloaded packages here,
	// so that any dependent packages will correctly find them as they will not search the cache by default
	if (Pending && Pending->NetDriver && Pending->NetDriver->ServerConnection)
	{
		// make the package, and use this for the new linker (and to load the map from)
		MapOuter = CreatePackage(NULL, *Pending->URL.Map);
#if WITH_EDITOR
		if (WorldContext.WorldType == EWorldType::PIE)
		{
			MapOuter->PackageFlags |= PKG_PlayInEditor;
		}
		MapOuter->PIEInstanceID = WorldContext.PIEInstance;
#endif
		// create the linker with the map name, and use the Guid so we find the downloaded version
		BeginLoad();
		GetPackageLinker(MapOuter, NULL, LOAD_NoWarn | LOAD_NoVerify | LOAD_Quiet, NULL, NULL);
		EndLoad();
	}

	UPackage* WorldPackage = NULL;
	UWorld*	NewWorld = NULL;

	
	// Is this a PIE networking thing?
	if (!WorldContext.PIERemapPrefix.IsEmpty())
	{
		if ( URL.Map.Contains(WorldContext.PIERemapPrefix) )
		{
			FString SourceWorldPackage = UWorld::RemovePIEPrefix(URL.Map);

			// We are loading a new world for this context, so clear out PIE fixups that might be lingering.
			// (note we dont want to do this in DuplicateWorldForPIE, since that is also called on streaming worlds.
			GPlayInEditorID = WorldContext.PIEInstance;
			FLazyObjectPtr::ResetPIEFixups();

			NewWorld = UWorld::DuplicateWorldForPIE(SourceWorldPackage, NULL);
			if (NewWorld == nullptr) 
			{
				// Load map from the disk in case editor does not have it
				const FString PIEPackageName = *UWorld::ConvertToPIEPackageName(SourceWorldPackage, WorldContext.PIEInstance);

				// Set the world type in the static map, so that UWorld::PostLoad can set the world type
				UWorld::WorldTypePreLoadMap.FindOrAdd( FName(*PIEPackageName) ) = WorldContext.WorldType;

				WorldPackage = LoadPackage(CreatePackage(NULL, *PIEPackageName), *SourceWorldPackage, LOAD_None);
				if (WorldPackage == nullptr)
				{
					Error = FString::Printf(TEXT("Failed to load package '%s' while in PIE"), *SourceWorldPackage);
					return false;
				}

				NewWorld = UWorld::FindWorldInPackage(WorldPackage);

				// If the world was not found, follow a redirector if there is one.
				if ( !NewWorld )
				{
					NewWorld = UWorld::FollowWorldRedirectorInPackage(WorldPackage);
					if ( NewWorld )
					{
						WorldPackage = NewWorld->GetOutermost();
					}
				}

				check(NewWorld);
#if WITH_EDITOR
				WorldPackage->PIEInstanceID = WorldContext.PIEInstance;
#endif			
				// Rename streaming levels to PIE
				for (auto StreamingLevel : NewWorld->StreamingLevels)
				{
					StreamingLevel->RenameForPIE(WorldContext.PIEInstance);
				}
			}
			else
			{
				WorldPackage = CastChecked<UPackage>(NewWorld->GetOuter());
			}

			NewWorld->StreamingLevelsPrefix = UWorld::BuildPIEPackagePrefix(WorldContext.PIEInstance);
			GIsPlayInEditorWorld = true;
		}
	}

	const FString URLTrueMapName = URL.Map;

	// Normal map loading
	if (NewWorld == NULL)
	{
		// Set the world type in the static map, so that UWorld::PostLoad can set the world type
		UWorld::WorldTypePreLoadMap.FindOrAdd( FName(*URL.Map) ) = WorldContext.WorldType;

		// See if the level is already in memory
		WorldPackage = FindPackage(MapOuter, *URL.Map);

		// If the level isn't already in memory, load level from disk
		if (WorldPackage == NULL)
		{
			WorldPackage = LoadPackage(MapOuter, *URL.Map, (WorldContext.WorldType == EWorldType::PIE ? LOAD_PackageForPIE : LOAD_None));
		}

		if( WorldPackage == NULL )
		{
			// it is now the responsibility of the caller to deal with a NULL return value and alert the user if necessary
			Error = FString::Printf(TEXT("Failed to load package '%s'"), *URL.Map);
			return false;
		}

		// Find the newly loaded world.
		NewWorld = UWorld::FindWorldInPackage(WorldPackage);

		// If the world was not found, it could be a redirector to a world. If so, follow it to the destination world.
		if ( !NewWorld )
		{
			NewWorld = UWorld::FollowWorldRedirectorInPackage(WorldPackage);
			if ( NewWorld )
			{
				WorldPackage = NewWorld->GetOutermost();
			}
		}
		check(NewWorld);

		FScopeCycleCounterUObject MapScope(WorldPackage);

		if (FPlatformProperties::RequiresCookedData() && GUseSeekFreeLoading && !(WorldPackage->PackageFlags & PKG_DisallowLazyLoading))
		{
			UE_LOG(LogLoad, Fatal, TEXT("Map '%s' has not been cooked correctly! Most likely stale version on the XDK."), *WorldPackage->GetName());
		}

		if (WorldContext.WorldType == EWorldType::PIE)
		{
			// If we are a PIE world and the world we just found is already initialized, then we're probably reloading the editor world and we
			// need to create a PIE world by duplication instead
			if (NewWorld->bIsWorldInitialized)
			{
				NewWorld = CreatePIEWorldByDuplication(WorldContext, NewWorld, URL.Map);
				// CreatePIEWorldByDuplication clears GIsPlayInEditorWorld so set it again
				GIsPlayInEditorWorld = true;
			}
			// Otherwise we are probably loading new map while in PIE, so we need to rename world package and all streaming levels
			else if (Pending == NULL)
			{
#if WITH_EDITOR
				WorldPackage->PIEInstanceID = WorldContext.PIEInstance;
#endif				
				const FString PIEPackageName = *UWorld::ConvertToPIEPackageName(WorldPackage->GetName(), WorldContext.PIEInstance);
			
				WorldPackage->Rename(*PIEPackageName);
				for (int32 StreamingLevelIdx = 0; StreamingLevelIdx < NewWorld->StreamingLevels.Num(); StreamingLevelIdx++)
				{
					NewWorld->StreamingLevels[StreamingLevelIdx]->RenameForPIE(WorldContext.PIEInstance);
				}
				
				NewWorld->StreamingLevelsPrefix = UWorld::BuildPIEPackagePrefix(WorldContext.PIEInstance);
			}
		}
	}

	GWorld = NewWorld;

	WorldContext.SetCurrentWorld(NewWorld);
	WorldContext.World()->WorldType = WorldContext.WorldType;
	
	// Fixme: hacky but we need to set PackageFlags here if we are in a PIE Context.
	// Also, dont add to root when in PIE, since PIE doesn't remove world from root
	if (WorldContext.WorldType == EWorldType::PIE)
	{
		check((CastChecked<UPackage>(WorldContext.World()->GetOutermost())->PackageFlags & PKG_PlayInEditor) == PKG_PlayInEditor);
		WorldContext.World()->ClearFlags(RF_Standalone);
	}
	else
	{
		WorldContext.World()->AddToRoot();
	}

	// In the PIE case the world will already have been initialized as part of CreatePIEWorldByDuplication
	if (!WorldContext.World()->bIsWorldInitialized)
	{
		WorldContext.World()->InitWorld();
	}

	// Handle pending level.
	if( Pending )
	{
		check(Pending == WorldContext.PendingNetGame);
		MovePendingLevel(WorldContext);
	}
	else
	{
		check(!WorldContext.World()->GetNetDriver());
	}

	WorldContext.World()->SetGameMode(URL);

	if( GetAudioDevice() )
	{
		GetAudioDevice()->SetDefaultBaseSoundMix( WorldContext.World()->GetWorldSettings()->DefaultBaseSoundMix );
	}

	// Listen for clients.
	if (Pending == NULL && (!GIsClient || URL.HasOption(TEXT("Listen"))))
	{
		if (!WorldContext.World()->Listen(URL))
		{
			UE_LOG(LogNet, Error, TEXT("LoadMap: failed to Listen(%s)"), *URL.ToString());
		}
	}

	const TCHAR* MutatorString = URL.GetOption(TEXT("Mutator="), TEXT(""));
	if (MutatorString)
	{
		TArray<FString> Mutators;
		FString(MutatorString).ParseIntoArray(&Mutators, TEXT(","), true);

		for (int32 MutatorIndex = 0; MutatorIndex < Mutators.Num(); MutatorIndex++)
		{
			LoadPackagesFully(WorldContext.World(), FULLYLOAD_Mutator, Mutators[MutatorIndex]);
		}
	}

	// load any per-map packages
	check(WorldContext.World()->PersistentLevel);
	LoadPackagesFully(WorldContext.World(), FULLYLOAD_Map, WorldContext.World()->PersistentLevel->GetOutermost()->GetName());

	// Set initial world origin and stream in levels
	if (WorldContext.World()->WorldComposition)
	{
		WorldContext.World()->NavigateTo(FIntPoint::ZeroValue);
	}
	
	UNavigationSystem::InitializeForWorld(WorldContext.World(), FNavigationSystem::GameMode);
	
	// Note that AI system will be created only if ai-system-creation conditions are met
	WorldContext.World()->CreateAISystem();

	// Initialize gameplay for the level.
	WorldContext.World()->InitializeActorsForPlay(URL);

	// Remember the URL. Put this before spawning player controllers so that
	// a player controller can get the map name during initialization and
	// have it be correct
	WorldContext.LastURL = URL;
	WorldContext.LastURL.Map = URLTrueMapName;

	if (WorldContext.World()->GetNetMode() == NM_Client)
	{
		WorldContext.LastRemoteURL = URL;
	}

	// Client init.
	for(auto It = WorldContext.GamePlayers.CreateIterator(); It; ++It)
	{
		FString Error2;
		if(!(*It)->SpawnPlayActor(URL.ToString(1),Error2,WorldContext.World()))
		{
			UE_LOG(LogEngine, Fatal, TEXT("Couldn't spawn player: %s"), *Error2);
		}
	}

	// Process global shader results before we try to render anything
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->ProcessAsyncResults(false, true);
	}

	// Prime texture streaming.
	IStreamingManager::Get().NotifyLevelChange();

	WorldContext.World()->BeginPlay();
	}

	// send a callback message
	FCoreDelegates::PostLoadMap.Broadcast();
	
	WorldContext.World()->bWorldWasLoadedThisTick = true;

	// We want to update streaming immediately so that there's no tick prior to processing any levels that should be initially visible
	// that requires calculating the scene, so redraw everything now to take care of it all though don't present the frame.
	RedrawViewports(false);

	// RedrawViewports() may have added a dummy playerstart location. Remove all views to start from fresh the next Tick().
	IStreamingManager::Get().RemoveStreamingViews( RemoveStreamingViews_All );
	
	MALLOC_PROFILER( FMallocProfiler::SnapshotMemoryLoadMapEnd( URL.Map ); )

	// Successfully started local level.
	return true;
}

void UEngine::CleanupPackagesToFullyLoad(FWorldContext &Context, EFullyLoadPackageType FullyLoadType, const FString& Tag)
{
	//UE_LOG(LogEngine, Log, TEXT("------------------ CleanupPackagesToFullyLoad: %i, %s"),(int32)FullyLoadType, *Tag);
	for (int32 MapIndex = 0; MapIndex < Context.PackagesToFullyLoad.Num(); MapIndex++)
	{
		FFullyLoadedPackagesInfo& PackagesInfo = Context.PackagesToFullyLoad[MapIndex];
		// is this entry for the map/game?
		if (PackagesInfo.FullyLoadType == FullyLoadType && (PackagesInfo.Tag == Tag || Tag == TEXT("")))
		{
			// mark all objects from this map as unneeded
			for (int32 ObjectIndex = 0; ObjectIndex < PackagesInfo.LoadedObjects.Num(); ObjectIndex++)
			{
//				UE_LOG(LogEngine, Log, TEXT("Removing %s from root"), *PackagesInfo.LoadedObjects(ObjectIndex)->GetFullName());
				PackagesInfo.LoadedObjects[ObjectIndex]->RemoveFromRoot();	
			}
			// empty the array of pointers to the objects
			PackagesInfo.LoadedObjects.Empty();
		}
	}
}

void UEngine::CancelPendingMapChange(FWorldContext &Context)
{
	// Empty intermediate arrays.
	Context.LevelsToLoadForPendingMapChange.Empty();
	Context.LoadedLevelsForPendingMapChange.Empty();

	// Reset state and make sure conditional map change doesn't fire.
	Context.PendingMapChangeFailureDescription	= TEXT("");
	Context.bShouldCommitPendingMapChange		= false;
	
	// Reset array of levels to prepare for client.
	if( Context.World() )
	{
		Context.World()->PreparingLevelNames.Empty();
	}
}

/** Clear out the debug properties array that is storing values to show on the screen */
void UEngine::ClearDebugDisplayProperties()
{
	for (int32 i = 0; i < GameViewport->DebugProperties.Num(); i++)
	{
		if (GameViewport->DebugProperties[i].Obj == NULL)
		{
			GameViewport->DebugProperties.RemoveAt(i, 1);
			i--;
		}
		else
		{
			for (UObject* TestObj = GameViewport->DebugProperties[i].Obj; TestObj != NULL; TestObj = TestObj->GetOuter())
			{
				if (TestObj->IsA(ULevel::StaticClass()) || TestObj->IsA(UWorld::StaticClass()) || TestObj->IsA(AActor::StaticClass()))
				{
					GameViewport->DebugProperties.RemoveAt(i, 1);
					i--;
					break;
				}
			}
		}
	}
}

void UEngine::MovePendingLevel(FWorldContext &Context)
{
	check(Context.World());
	check(Context.PendingNetGame);

	Context.World()->SetNetDriver(Context.PendingNetGame->NetDriver);
	
	UNetDriver* NetDriver = Context.PendingNetGame->NetDriver;
	if( NetDriver )
	{
		// The pending net driver is renamed to the current "game net driver"
		NetDriver->NetDriverName = NAME_GameNetDriver;
		NetDriver->SetWorld(Context.World());
	}

	// Reset the Navigation System
	Context.World()->SetNavigationSystem(NULL);
}

void UEngine::LoadPackagesFully(UWorld * InWorld, EFullyLoadPackageType FullyLoadType, const FString& Tag)
{
	FWorldContext &Context = GetWorldContextFromWorldChecked(InWorld);
	//UE_LOG(LogEngine, Log, TEXT("------------------ LoadPackagesFully: %i, %s"),(int32)FullyLoadType, *Tag);

	// look for all entries for the given map
	for (int32 MapIndex = ((Tag == TEXT("___TAILONLY___")) ? Context.PackagesToFullyLoad.Num() - 1 : 0); MapIndex < Context.PackagesToFullyLoad.Num(); MapIndex++)
	{
		FFullyLoadedPackagesInfo& PackagesInfo = Context.PackagesToFullyLoad[MapIndex];
		/*
		for (int32 PackageIndex = 0; PackageIndex < PackagesInfo.PackagesToLoad.Num(); PackageIndex++)
		{
			UE_LOG(LogEngine, Log, TEXT("--------------------- Considering: %i, %s, %s"),(int32)PackagesInfo.FullyLoadType, *PackagesInfo.Tag, *PackagesInfo.PackagesToLoad(PackageIndex).ToString());
		}
		*/

		// is this entry for the map/game?
		if (PackagesInfo.FullyLoadType == FullyLoadType && (PackagesInfo.Tag == Tag || Tag == TEXT("") || Tag == TEXT("___TAILONLY___")))
		{
			// go over all packages that need loading
			for (int32 PackageIndex = 0; PackageIndex < PackagesInfo.PackagesToLoad.Num(); PackageIndex++)
			{
				//UE_LOG(LogEngine, Log, TEXT("------------------------ looking for %s"),*PackagesInfo.PackagesToLoad(PackageIndex).ToString());

				// look for the package in the package cache
				FString SFPackageName = PackagesInfo.PackagesToLoad[PackageIndex].ToString() + STANDALONE_SEEKFREE_SUFFIX;
				bool bFoundFile = false;
				FString PackagePath;
				if (FPackageName::DoesPackageExist(SFPackageName, NULL, &PackagePath))
				{
					bFoundFile = true;
				}
				else if ( (FPackageName::DoesPackageExist(PackagesInfo.PackagesToLoad[PackageIndex].ToString(), NULL, &PackagePath)) )
				{
					bFoundFile = true;
				}
				if (bFoundFile)
				{
					// load the package
					// @todo: This would be nice to be async probably, but how would we add it to the root? (LOAD_AddPackageToRoot?)
					//UE_LOG(LogEngine, Log, TEXT("------------------ Fully loading %s"), *PackagePath);
					UPackage* Package = LoadPackage(NULL, *PackagePath, 0);

					// add package to root so we can find it
					Package->AddToRoot();

					// remember the object for unloading later
					PackagesInfo.LoadedObjects.Add(Package);

					// add the objects to the root set so that it will not be GC'd
					for (TObjectIterator<UObject> It; It; ++It)
					{
						if (It->IsIn(Package))
						{
//							UE_LOG(LogEngine, Log, TEXT("Adding %s to root"), *It->GetFullName());
							It->AddToRoot();

							// remember the object for unloading later
							PackagesInfo.LoadedObjects.Add(*It);
						}
					}
				}
				else
				{
					UE_LOG(LogEngine, Log, TEXT("Failed to find Package %s to FullyLoad [FullyLoadType = %d, Tag = %s]"), *PackagesInfo.PackagesToLoad[PackageIndex].ToString(), (int32)FullyLoadType, *Tag);
				}
			}
		}
		/*
		else
		{
			UE_LOG(LogEngine, Log, TEXT("DIDN't MATCH!!!"));
			for (int32 PackageIndex = 0; PackageIndex < PackagesInfo.PackagesToLoad.Num(); PackageIndex++)
			{
				UE_LOG(LogEngine, Log, TEXT("DIDN't MATCH!!! %i, \"%s\"(\"%s\"), %s"),(int32)PackagesInfo.FullyLoadType, *PackagesInfo.Tag, *Tag, *PackagesInfo.PackagesToLoad(PackageIndex).ToString());
			}
		}
		*/
	}
}


void UEngine::UpdateTransitionType(UWorld *CurrentWorld)
{
	// Update the transition screen.
	if(TransitionType == TT_Connecting)
	{
		// Check to see if all players have finished connecting.
		TransitionType = TT_None;

		FWorldContext &Context = GetWorldContextFromWorldChecked(CurrentWorld);
		for (auto It = Context.GamePlayers.CreateIterator(); It; ++It)
		{
			if(!(*It)->PlayerController)
			{
				// This player has not received a PlayerController from the server yet, so leave the connecting screen up.
				TransitionType = TT_Connecting;
				break;
			}
		}
	}
	else if(TransitionType == TT_None || TransitionType == TT_Paused)
	{
		// Display a paused screen if the game is paused.
		TransitionType = (CurrentWorld->GetWorldSettings()->Pauser != NULL) ? TT_Paused : TT_None;
	}
}

FWorldContext& UEngine::CreateNewWorldContext(EWorldType::Type WorldType)
{
	FWorldContext *NewWorldContext = (new (WorldList) FWorldContext);
	NewWorldContext->WorldType = WorldType;
	NewWorldContext->ContextHandle = FName(*FString::Printf(TEXT("Context_%d"), NextWorldContextHandle++));
	
	return *NewWorldContext;
}

FWorldContext& HandleInvalidWorldContext()
{
	if (!IsRunningCommandlet())
	{
		UE_LOG(LogLoad, Error, TEXT("WorldContext requested with invalid context object.") );
		check(false);
	}
	
	return GEngine->CreateNewWorldContext(EWorldType::None);
}

FWorldContext* UEngine::GetWorldContextFromHandle(FName WorldContextHandle)
{
	for (FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.ContextHandle == WorldContextHandle)
		{
			return &WorldContext;
		}
	}
	return NULL;
}

FWorldContext& UEngine::GetWorldContextFromHandleChecked(FName WorldContextHandle)
{
	if (FWorldContext* WorldContext = GetWorldContextFromHandle(WorldContextHandle))
	{
		return *WorldContext;
	}

	UE_LOG(LogLoad, Warning, TEXT("WorldContext requested with invalid context handle %s"), *WorldContextHandle.ToString());
	return HandleInvalidWorldContext();
}

FWorldContext* UEngine::GetWorldContextFromWorld(const UWorld* InWorld)
{
	for (FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.World() == InWorld)
		{
			return &WorldContext;
		}
	}
	return NULL;
}

FWorldContext& UEngine::GetWorldContextFromWorldChecked(UWorld *InWorld)
{
	if (FWorldContext* WorldContext = GetWorldContextFromWorld(InWorld))
	{
		return *WorldContext;
	}
	return HandleInvalidWorldContext();
}

UGameViewportClient* UEngine::GameViewportForWorld(UWorld *InWorld)
{
	FWorldContext* Context = GetWorldContextFromWorld(InWorld);
	return (Context ? Context->GameViewport : NULL);
}

FWorldContext* UEngine::GetWorldContextFromGameViewport(const UGameViewportClient *InViewport)
{
	for (FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.GameViewport == InViewport)
		{
			return &WorldContext;
		}
	}
	return NULL;
}

FWorldContext& UEngine::GetWorldContextFromGameViewportChecked(const UGameViewportClient *InViewport)
{
	if (FWorldContext* WorldContext = GetWorldContextFromGameViewport(InViewport))
	{
		return *WorldContext;
	}
	return HandleInvalidWorldContext();
}

FWorldContext* UEngine::GetWorldContextFromPendingNetGame(const UPendingNetGame *InPendingNetGame)
{
	for (FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.PendingNetGame == InPendingNetGame)
		{
			return &WorldContext;
		}
	}
	return NULL;
}

FWorldContext& UEngine::GetWorldContextFromPendingNetGameChecked(const UPendingNetGame *InPendingNetGame)
{
	if (FWorldContext* WorldContext = GetWorldContextFromPendingNetGame(InPendingNetGame))
	{
		return *WorldContext;
	}
	return HandleInvalidWorldContext();
}

FWorldContext* UEngine::GetWorldContextFromPendingNetGameNetDriver(const UNetDriver *InPendingNetDriver)
{
	for (FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.PendingNetGame && WorldContext.PendingNetGame->NetDriver == InPendingNetDriver)
		{
			return &WorldContext;
		}
	}
	return NULL;
}
FWorldContext& UEngine::GetWorldContextFromPendingNetGameNetDriverChecked(const UNetDriver *InPendingNetDriver)
{
	if (FWorldContext* WorldContext = GetWorldContextFromPendingNetGameNetDriver(InPendingNetDriver))
	{
		return *WorldContext;
	}
	return HandleInvalidWorldContext();
}

UPendingNetGame* UEngine::PendingNetGameFromWorld( UWorld* InWorld )
{
	return GetWorldContextFromWorldChecked(InWorld).PendingNetGame;
}

void UEngine::DestroyWorldContext(UWorld * InWorld)
{
	for (int32 idx=0; idx < WorldList.Num(); ++idx)
	{
		if (WorldList[idx].World() == InWorld)
		{
			// Set the current world to NULL so that any external referencers are cleaned up before we remove
			WorldList[idx].SetCurrentWorld(NULL);
			WorldList.RemoveAt(idx);
			break;
		}
	}
}

bool UEngine::WorldHasValidContext(UWorld *InWorld)
{
	return (GetWorldContextFromWorld(InWorld) != NULL);
}

void UEngine::VerifyLoadMapWorldCleanup()
{
	// All worlds at this point should be the CurrentWorld of some context or preview worlds.
	for( TObjectIterator<UWorld> It; It; ++It )
	{
		UWorld* World = *It;
		const bool bIsPersistantWorldType = (World->WorldType == EWorldType::Inactive) || (World->WorldType == EWorldType::Preview);
		if (!bIsPersistantWorldType && !WorldHasValidContext(World))
		{
			// Print some debug information...
			UE_LOG(LogLoad, Log, TEXT("%s not cleaned up by garbage collection! "), *World->GetFullName());
			StaticExec(World, *FString::Printf(TEXT("OBJ REFS CLASS=WORLD NAME=%s"), *World->GetPathName()));
			TMap<UObject*,UProperty*>	Route		= FArchiveTraceRoute::FindShortestRootPath( World, true, GARBAGE_COLLECTION_KEEPFLAGS );
			FString						ErrorString	= FArchiveTraceRoute::PrintRootPath( Route, World );
			UE_LOG(LogLoad, Log, TEXT("%s"),*ErrorString);
			// before asserting.
			UE_LOG(LogLoad, Fatal, TEXT("%s not cleaned up by garbage collection!") LINE_TERMINATOR TEXT("%s") , *World->GetFullName(), *ErrorString );
		}
	}
}


/*-----------------------------------------------------------------------------
	Async persistent level map change.
-----------------------------------------------------------------------------*/

/**
 * Callback function used in UGameEngine::PrepareMapChange to pass to LoadPackageAsync.
 *
 * @param	LevelPackage	level package that finished async loading
 * @param	InGameEngine	pointer to game engine object to associated loaded level with so it won't be GC'ed
 */
static void AsyncMapChangeLevelLoadCompletionCallback(const FString& PackageName, UPackage* LevelPackage, FName InWorldHandle )
{
	FWorldContext &Context = GEngine->GetWorldContextFromHandleChecked( InWorldHandle );

	if( LevelPackage )
	{	
		// Try to find a UWorld object in the level package.
		UWorld* World = UWorld::FindWorldInPackage(LevelPackage);

		// If the world was not found, try to follow a redirector if it exists
		if ( !World )
		{
			World = UWorld::FollowWorldRedirectorInPackage(LevelPackage);
			if ( World )
			{
				LevelPackage = World->GetOutermost();
			}
		}

		ULevel* Level = World ? World->PersistentLevel : NULL;	
		
		// Print out a warning and set the error if we couldn't find a level in this package.
		if( !Level )
		{
			// NULL levels can happen if existing package but not level is specified as a level name.
			Context.PendingMapChangeFailureDescription = FString::Printf(TEXT("Couldn't find level in package %s"), *LevelPackage->GetName());
			UE_LOG(LogEngine, Error, TEXT( "ERROR ERROR %s was not found in the PackageCache It must exist or the Level Loading Action will FAIL!!!! " ), *LevelPackage->GetName() );
			UE_LOG(LogEngine, Warning, TEXT("%s"), *Context.PendingMapChangeFailureDescription );
			UE_LOG(LogEngine, Error, TEXT( "ERROR ERROR %s was not found in the PackageCache It must exist or the Level Loading Action will FAIL!!!! " ), *LevelPackage->GetName() );
		}

		// Add loaded level to array to prevent it from being garbage collected.
		Context.LoadedLevelsForPendingMapChange.Add( Level );
	}
	else
	{
		// Add NULL entry so we don't end up waiting forever on a level that is never going to be loaded.
		Context.LoadedLevelsForPendingMapChange.Add( NULL );
		UE_LOG(LogEngine, Warning, TEXT("NULL LevelPackage as argument to AsyncMapChangeLevelCompletionCallback") );
	}
}


bool UEngine::PrepareMapChange(FWorldContext &Context, const TArray<FName>& LevelNames)
{
	// make sure level streaming isn't frozen
	Context.World()->bIsLevelStreamingFrozen = false;

	// Make sure we don't interrupt a pending map change in progress.
	if( !IsPreparingMapChange(Context) )
	{
		Context.LevelsToLoadForPendingMapChange.Empty();
		Context.LevelsToLoadForPendingMapChange += LevelNames;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Verify that all levels specified are in the package file cache.
		for( int32 LevelIndex=0; LevelIndex < Context.LevelsToLoadForPendingMapChange.Num(); LevelIndex++ )
		{
			const FName LevelName = Context.LevelsToLoadForPendingMapChange[LevelIndex];
			if( !FPackageName::DoesPackageExist( LevelName.ToString() ) )
			{
				Context.LevelsToLoadForPendingMapChange.Empty();
				Context.PendingMapChangeFailureDescription = FString::Printf(TEXT("Couldn't find package for level '%s'"), *LevelName.ToString());
				// write it out immediately so make sure it's in the log even without a CommitMapChange happening
				UE_LOG(LogEngine, Warning, TEXT("PREPAREMAPCHANGE: %s"), *Context.PendingMapChangeFailureDescription);

				// tell user on screen!
				extern bool GIsPrepareMapChangeBroken;
				GIsPrepareMapChangeBroken = true;

				return false;
			}
			//@todo streaming: make sure none of the maps are already loaded/ being loaded?
		}
#endif

		// copy LevelNames into the WorldInfo's array to keep track of the map change that we're preparing (primarily for servers so clients that join in progress can be notified)
		if (Context.World() != NULL)
		{
			Context.World()->PreparingLevelNames = LevelNames;
		}

		// Kick off async loading of packages.
		for( int32 LevelIndex=0; LevelIndex < Context.LevelsToLoadForPendingMapChange.Num(); LevelIndex++ )
		{
			const FName LevelName = Context.LevelsToLoadForPendingMapChange[LevelIndex];
			if( GUseSeekFreeLoading )
			{
				// Only load localized package if it exists as async package loading doesn't handle errors gracefully.
				FString LocalizedPackageName = LevelName.ToString() + LOCALIZED_SEEKFREE_SUFFIX;
				FString LocalizedFileName;
				if( FPackageName::DoesPackageExist( LocalizedPackageName, NULL, &LocalizedFileName ) )
				{
					// Load localized part of level first in case it exists. We don't need to worry about GC or completion 
					// callback as we always kick off another async IO for the level below.
					LoadPackageAsync( *LocalizedPackageName );
				}
			}
			
			LoadPackageAsync( *LevelName.ToString(), 
				FLoadPackageAsyncDelegate::CreateStatic(&AsyncMapChangeLevelLoadCompletionCallback, Context.ContextHandle)
				);
		}

		return true;
	}
	else
	{
		Context.PendingMapChangeFailureDescription = TEXT("Current map change still in progress");
		return false;
	}
}


FString UEngine::GetMapChangeFailureDescription(FWorldContext &Context)
{
	return Context.PendingMapChangeFailureDescription;
}
	

bool UEngine::IsPreparingMapChange(FWorldContext &Context)
{
	return Context.LevelsToLoadForPendingMapChange.Num() > 0;
}
	

bool UEngine::IsReadyForMapChange(FWorldContext &Context)
{
	return IsPreparingMapChange(Context) && (Context.LevelsToLoadForPendingMapChange.Num() == Context.LoadedLevelsForPendingMapChange.Num());
}


void UEngine::ConditionalCommitMapChange(FWorldContext &Context)
{
	// Check whether there actually is a pending map change and whether we want it to be committed yet.
	if( Context.bShouldCommitPendingMapChange && IsPreparingMapChange(Context) )
	{
		// Block on remaining async data.
		if( !IsReadyForMapChange(Context) )
		{
			FlushAsyncLoading( NAME_None );
			check( IsReadyForMapChange(Context) );
		}
		
		// Perform map change.
		if (!CommitMapChange(Context.World()))
		{
			UE_LOG(LogEngine, Warning, TEXT("Committing map change via %s was not successful: %s"), *GetFullName(), *GetMapChangeFailureDescription(Context));
		}
		// No pending map change - called commit without prepare.
		else
		{
			UE_LOG(LogEngine, Log, TEXT("Committed map change via %s"), *GetFullName());
		}

		// We just commited, so reset the flag.
		Context.bShouldCommitPendingMapChange = false;
	}
}

/** struct to temporarily hold on to already loaded but unbound levels we're going to make visible at the end of CommitMapChange() while we first trigger GC */
struct FPendingStreamingLevelHolder : public FGCObject
{
public:
	TArray<ULevel*> Levels;

	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		for( int32 LevelIndex = 0; LevelIndex < Levels.Num(); LevelIndex++ )
		{
			Collector.AddReferencedObject( Levels[ LevelIndex ] ); 
		}
	}
};


bool UEngine::CommitMapChange( FWorldContext &Context )
{
	if (!IsPreparingMapChange(Context))
	{
		Context.PendingMapChangeFailureDescription = TEXT("No map change is being prepared");
		return false;
	}
	else if (!IsReadyForMapChange(Context))
	{
		Context.PendingMapChangeFailureDescription = TEXT("Map change is not ready yet");
		return false;
	}
	else
	{
		check(Context.World());

		// tell the game we are about to switch levels
		if (Context.World()->GetAuthGameMode())
		{
			// get the actual persistent level's name
			FString PreviousMapName = Context.World()->PersistentLevel->GetOutermost()->GetName();
			FString NextMapName = Context.LevelsToLoadForPendingMapChange[0].ToString();

			// look for a persistent streamed in sublevel
			for (int32 LevelIndex = 0; LevelIndex < Context.World()->StreamingLevels.Num(); LevelIndex++)
			{
				ULevelStreamingPersistent* PersistentLevel = Cast<ULevelStreamingPersistent>(Context.World()->StreamingLevels[LevelIndex]);
				if (PersistentLevel)
				{
					PreviousMapName = PersistentLevel->PackageName.ToString();
					// only one persistent level
					break;
				}
			}
			Context.World()->GetAuthGameMode()->PreCommitMapChange(PreviousMapName, NextMapName); 
		}

		// on the client, check if we already loaded pending levels to be made visible due to e.g. the PackageMap
		FPendingStreamingLevelHolder LevelHolder;
		if (Context.PendingLevelStreamingStatusUpdates.Num() > 0)
		{
			// Iterating over GCed ULevels. A TObjectIterator<ULevel> can not do this.
			for (TObjectIterator<UObject> It(true); It; ++It)
			{
				ULevel* Level = Cast<ULevel>(*It);
				if ( Level )
				{
					for (int32 i = 0; i < Context.PendingLevelStreamingStatusUpdates.Num(); i++)
					{
						if ( Level->GetOutermost()->GetFName() == Context.PendingLevelStreamingStatusUpdates[i].PackageName && 
							(Context.PendingLevelStreamingStatusUpdates[i].bShouldBeLoaded || Context.PendingLevelStreamingStatusUpdates[i].bShouldBeVisible) )
						{
							LevelHolder.Levels.Add(Level);
							break;
						}
					}
				}
			}
		}

		// we are no longer preparing this change
		Context.World()->PreparingLevelNames.Empty();

		// Iterate over level collection, marking them to be forcefully unloaded.
		for( int32 LevelIndex=0; LevelIndex < Context.World()->StreamingLevels.Num(); LevelIndex++ )
		{
			ULevelStreaming* StreamingLevel	= Context.World()->StreamingLevels[LevelIndex];
			if( StreamingLevel )
			{
				StreamingLevel->bIsRequestingUnloadAndRemoval = true;
			}
		}

		// Collect garbage. @todo streaming: make sure that this doesn't stall due to async loading in the background
		CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS, true );

		// The new fake persistent level is first in the LevelsToLoadForPendingMapChange array.
		FName	FakePersistentLevelName = Context.LevelsToLoadForPendingMapChange[0];
		ULevel*	FakePersistentLevel		= NULL;
		// copy to WorldInfo to keep track of the last map change we performed (primarily for servers so clients that join in progress can be notified)
		// we don't need to remember secondary levels as the join code iterates over all streaming levels and updates them
		Context.World()->CommittedPersistentLevelName = FakePersistentLevelName;

		// Find level package in loaded levels array.
		for( int32 LevelIndex=0; LevelIndex < Context.LoadedLevelsForPendingMapChange.Num(); LevelIndex++ )
		{
			ULevel* Level = Context.LoadedLevelsForPendingMapChange[LevelIndex];

			// NULL levels can happen if existing package but not level is specified as a level name.
			if( Level && (FakePersistentLevelName == Level->GetOutermost()->GetFName()) )
			{
				FakePersistentLevel = Level;
				break;
			}
		}
		check( FakePersistentLevel );

		// Construct a new ULevelStreamingPersistent for the new persistent level.
		ULevelStreamingPersistent* LevelStreamingPersistent = ConstructObject<ULevelStreamingPersistent>(
			ULevelStreamingPersistent::StaticClass(),
			GetTransientPackage(),
			*FString::Printf(TEXT("LevelStreamingPersistent_%s"), *FakePersistentLevel->GetOutermost()->GetName()) );

		// Propagate level and name to streaming object.
		LevelStreamingPersistent->SetLoadedLevel(FakePersistentLevel);
		LevelStreamingPersistent->PackageName	= FakePersistentLevelName;
		// And add it to the world info's list of levels.
		Context.World()->StreamingLevels.Add( LevelStreamingPersistent );

		UWorld* FakeWorld = CastChecked<UWorld>(FakePersistentLevel->GetOuter());
		// Add secondary levels to the world info levels array.
		Context.World()->StreamingLevels += FakeWorld->StreamingLevels;

		// fixup up any kismet streaming objects to force them to be loaded if they were preloaded, this
		// will keep streaming volumes from immediately unloading the levels that were just loaded
		for( int32 LevelIndex=0; LevelIndex < Context.World()->StreamingLevels.Num(); LevelIndex++ )
		{
			ULevelStreaming* StreamingLevel	= Context.World()->StreamingLevels[LevelIndex];
			// mark any kismet streamers to force be loaded
			if (StreamingLevel)
			{
				bool bWasFound = false;
				// was this one of the packages we wanted to load?
				for (int32 LoadLevelIndex = 0; LoadLevelIndex < Context.LevelsToLoadForPendingMapChange.Num(); LoadLevelIndex++)
				{
					if (Context.LevelsToLoadForPendingMapChange[LoadLevelIndex] == StreamingLevel->PackageName)
					{
						bWasFound = true;
						break;
					}
				}

				// if this level was preloaded, mark it as to be loaded and visible
				if (bWasFound)
				{
					StreamingLevel->bShouldBeLoaded		= true;
					StreamingLevel->bShouldBeVisible	= true;

#if WITH_SERVER_CODE
					if (Context.World()->IsServer())
					{
						// notify players of the change
						for( FConstPlayerControllerIterator Iterator = Context.World()->GetPlayerControllerIterator(); Iterator; ++Iterator )
						{
							(*Iterator)->LevelStreamingStatusChanged( 
									StreamingLevel, 
									StreamingLevel->bShouldBeLoaded, 
									StreamingLevel->bShouldBeVisible,
									StreamingLevel->bShouldBlockOnLoad,
									StreamingLevel->LevelLODIndex);							
						}
					}
#endif // WITH_SERVER_CODE
				}
			}
		}

		// Update level streaming, forcing existing levels to be unloaded and their streaming objects 
		// removed from the world info.	We can't kick off async loading in this update as we want to 
		// collect garbage right below.
		Context.World()->FlushLevelStreaming( NULL, true );
		
		// make sure any looping sounds, etc are stopped
		if (GetAudioDevice() != NULL)
		{
			GetAudioDevice()->StopAllSounds();
		}

		// Remove all unloaded levels from memory and perform full purge.
		CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS, true );
		
		// if there are pending streaming changes replicated from the server, apply them immediately
		if (Context.PendingLevelStreamingStatusUpdates.Num() > 0)
		{
			for (int32 i = 0; i < Context.PendingLevelStreamingStatusUpdates.Num(); i++)
			{
				ULevelStreaming* LevelStreamingObject = NULL;
				for (int32 j = 0; j < Context.World()->StreamingLevels.Num(); j++)
				{
					if (Context.World()->StreamingLevels[j] != NULL && Context.World()->StreamingLevels[j]->PackageName == Context.PendingLevelStreamingStatusUpdates[i].PackageName)
					{
						LevelStreamingObject = Context.World()->StreamingLevels[j];
						if (LevelStreamingObject != NULL)
						{
							LevelStreamingObject->bShouldBeLoaded	= Context.PendingLevelStreamingStatusUpdates[i].bShouldBeLoaded;
							LevelStreamingObject->bShouldBeVisible	= Context.PendingLevelStreamingStatusUpdates[i].bShouldBeVisible;
							LevelStreamingObject->LevelLODIndex		= Context.PendingLevelStreamingStatusUpdates[i].LODIndex;
						}
						else
						{
							check(LevelStreamingObject);
							UE_LOG(LogStreaming, Log, TEXT("Unable to handle streaming object %s"),*LevelStreamingObject->GetName());
						}

						// break out of object iterator if we found a match
						break;
					}
				}

				if (LevelStreamingObject == NULL)
				{
					UE_LOG(LogStreaming, Log, TEXT("Unable to find streaming object %s"), *Context.PendingLevelStreamingStatusUpdates[i].PackageName.ToString());
				}
			}

			Context.PendingLevelStreamingStatusUpdates.Empty();

			Context.World()->FlushLevelStreaming(NULL, false);
		}
		else
		{
			// This will cause the newly added persistent level to be made visible and kick off async loading for others.
			Context.World()->FlushLevelStreaming( NULL, true );
		}

		// delay the use of streaming volumes for a few frames
		Context.World()->DelayStreamingVolumeUpdates(3);

		// Empty intermediate arrays.
		Context.LevelsToLoadForPendingMapChange.Empty();
		Context.LoadedLevelsForPendingMapChange.Empty();
		Context.PendingMapChangeFailureDescription = TEXT("");

		// Prime texture streaming.
		IStreamingManager::Get().NotifyLevelChange();

		// tell the game we are done switching levels
		AGameMode* const GameMode = Context.World()->GetAuthGameMode();
		if (GameMode)
		{
			GameMode->PostCommitMapChange(); 
		}

		return true;
	}
}
void UEngine::AddNewPendingStreamingLevel(UWorld *InWorld, FName PackageName, bool bNewShouldBeLoaded, bool bNewShouldBeVisible, int32 LODIndex)
{
	FWorldContext &Context = GetWorldContextFromWorldChecked(InWorld);
	new(Context.PendingLevelStreamingStatusUpdates) FLevelStreamingStatus(PackageName, bNewShouldBeLoaded, bNewShouldBeVisible, LODIndex);
}

bool UEngine::ShouldCommitPendingMapChange(UWorld *InWorld)
{
	FWorldContext* WorldContext = GetWorldContextFromWorld(InWorld);
	return (WorldContext ? WorldContext->bShouldCommitPendingMapChange : false);
}

void UEngine::SetShouldCommitPendingMapChange(UWorld *InWorld, bool NewShouldCommitPendingMapChange)
{
	FWorldContext &Context = GetWorldContextFromWorldChecked(InWorld);
	Context.bShouldCommitPendingMapChange = NewShouldCommitPendingMapChange;
}

FSeamlessTravelHandler&	UEngine::SeamlessTravelHandlerForWorld(UWorld *World)
{
	return GetWorldContextFromWorldChecked(World).SeamlessTravelHandler;
}

FURL& UEngine::LastURLFromWorld(UWorld *World)
{
	return GetWorldContextFromWorldChecked(World).LastURL;
}

void UEngine::CreateGameUserSettings()
{
	UGameUserSettings::LoadConfigIni();
	GameUserSettings = ConstructObject<UGameUserSettings>(GEngine->GameUserSettingsClass);
	GameUserSettings->LoadSettings();
}

const UGameUserSettings* UEngine::GetGameUserSettings() const
{
	if (GameUserSettings == NULL)
	{
		UEngine* ConstThis = const_cast< UEngine* >( this );	// Hack because Header Generator doesn't yet support mutable keyword
		ConstThis->CreateGameUserSettings();
	}
	return GameUserSettings;
}

UGameUserSettings* UEngine::GetGameUserSettings()
{
	if (GameUserSettings == NULL)
	{
		CreateGameUserSettings();
	}
	return GameUserSettings;
}

// Stores information (such as modified properties) for an instanced object (component or subobject)
// in the old CDO, to allow them to be reapplied to the new instance under the new CDO
struct FInstancedObjectRecord
{
	TArray<uint8> SavedProperties;
	UObject* OldInstance;
};

static TAutoConsoleVariable<int32> CVarDumpCopyPropertiesForUnrelatedObjects(
	TEXT("DumpCopyPropertiesForUnrelatedObjects"),
	0,
	TEXT("Dump the objects that are cross class copied")
	);

void UEngine::CopyPropertiesForUnrelatedObjects(UObject* OldObject, UObject* NewObject, FCopyPropertiesForUnrelatedObjectsParams Params)
{
	check(OldObject && NewObject);

	// Bad idea to write data to an actor while its components are registered
	AActor* NewActor = Cast<AActor>(NewObject);
	if(NewActor != NULL)
	{
		TArray<UActorComponent*> Components;
		NewActor->GetComponents(Components);

		for(int32 i=0; i<Components.Num(); i++)
		{
			ensure(!Components[i]->IsRegistered());
		}
	}

	// If the new object is an Actor, save the root component reference, to be restored later
	USceneComponent* SavedRootComponent = NULL;
	if(NewActor != NULL)
	{
		SavedRootComponent = NewActor->GetRootComponent();
	}

	// Serialize out the modified properties on the old default object
	TArray<uint8> SavedProperties;
	TIndirectArray<FInstancedObjectRecord> SavedInstances;
	TMap<FName, int32> OldInstanceMap;

	// Save the modified properties of the old CDO
	{
		FObjectWriter Writer(OldObject, SavedProperties, true, true, Params.bDoDelta);
	}

	{
		// Find all instanced objects of the old CDO, and save off their modified properties to be later applied to the newly instanced objects of the new CDO
		TArray<UObject*> Components;
		OldObject->CollectDefaultSubobjects(Components,true);

		for (int32 Index = 0; Index < Components.Num(); Index++)
		{
			FInstancedObjectRecord* pRecord = new(SavedInstances) FInstancedObjectRecord();
			UObject* OldInstance = Components[Index];
			pRecord->OldInstance = OldInstance;
			OldInstanceMap.Add(OldInstance->GetFName(), SavedInstances.Num() - 1);
			FObjectWriter Writer(OldInstance, pRecord->SavedProperties, true, true);
		}
	}

	// Gather references to old instances or objects that need to be replaced after we serialize in saved data
	TMap<UObject*, UObject*> ReferenceReplacementMap;
	ReferenceReplacementMap.Add(OldObject, NewObject);
	ReferenceReplacementMap.Add(OldObject->GetArchetype(), NewObject->GetArchetype());
	if (Params.bReplaceObjectClassReferences)
	{
		ReferenceReplacementMap.Add(OldObject->GetClass(), NewObject->GetClass());
	}
	ReferenceReplacementMap.Add(OldObject->GetClass()->GetDefaultObject(), NewObject->GetClass()->GetDefaultObject());

	TArray<UObject*> ComponentsOnNewObject;
	{
		// Find all instanced objects of the old CDO, and save off their modified properties to be later applied to the newly instanced objects of the new CDO
		NewObject->CollectDefaultSubobjects(ComponentsOnNewObject,true);

		// Serialize in the modified properties from the old CDO to the new CDO
		if (SavedProperties.Num() > 0)
		{
			FObjectReader Reader(NewObject, SavedProperties, true, true);
		}

		for (int32 Index = 0; Index < ComponentsOnNewObject.Num(); Index++)
		{
			UObject* NewInstance = ComponentsOnNewObject[Index];
			if (int32* pOldInstanceIndex = OldInstanceMap.Find(NewInstance->GetFName()))
			{
				// Restore modified properties into the new instance
				FInstancedObjectRecord& Record = SavedInstances[*pOldInstanceIndex];
				ReferenceReplacementMap.Add(Record.OldInstance, NewInstance);
				if (Params.bAggressiveDefaultSubobjectReplacement)
				{
					UClass* Class = OldObject->GetClass()->GetSuperClass();
					//UClass* Class = OldObject->GetClass();
					//while (Class)
					if (Class)
					{
						UObject *CDOInst = Class->GetDefaultSubobjectByName(NewInstance->GetFName());
						if (CDOInst)
						{
							ReferenceReplacementMap.Add(CDOInst, NewInstance);
#if WITH_EDITOR
							if (Class->ClassGeneratedBy && Cast<UBlueprint>(Class->ClassGeneratedBy)->SkeletonGeneratedClass)
							{
								UObject *CDOInstS = Cast<UBlueprint>(Class->ClassGeneratedBy)->SkeletonGeneratedClass->GetDefaultSubobjectByName(NewInstance->GetFName());
								if (CDOInstS)
								{
									ReferenceReplacementMap.Add(CDOInstS, NewInstance);
								}

							}
#endif // WITH_EDITOR
						}
						else
						{
							//break;
						}
						//Class = Class->GetSuperClass();
					}
				}
				FObjectReader Reader(NewInstance, Record.SavedProperties, true, true);
			}
			else
			{
				bool bContainedInsideNewInstance = false;
				for (UObject* Parent = NewInstance->GetOuter(); Parent != NULL; Parent = Parent->GetOuter())
				{
					if (Parent == NewObject)
					{
						bContainedInsideNewInstance = true;
						break;
					}
				}

				if (!bContainedInsideNewInstance)
				{
					// A bad thing has happened and cannot be reasonably fixed at this point
					UE_LOG(LogEngine, Log, TEXT("Warning: The CDO '%s' references a component that does not have the CDO in its outer chain!"), *NewObject->GetFullName(), *NewInstance->GetFullName()); 	
				}
			}
		}
	}

	// Replace anything with an outer of the old object with NULL, unless it already has a replacement
	TArray<UObject*> ObjectsInOuter;
	GetObjectsWithOuter(OldObject, ObjectsInOuter, true);
	for (int32 ObjectIndex = 0; ObjectIndex < ObjectsInOuter.Num(); ObjectIndex++)
	{
		if (!ReferenceReplacementMap.Contains(ObjectsInOuter[ObjectIndex]))
		{
			ReferenceReplacementMap.Add(ObjectsInOuter[ObjectIndex], NULL);
		}
	}

	// Replace references to old classes and instances on this object with the corresponding new ones
	FArchiveReplaceObjectRef<UObject> ReplaceInCDOAr(NewObject, ReferenceReplacementMap, /*bNullPrivateRefs=*/ false, /*bIgnoreOuterRef=*/ false, /*bIgnoreArchetypeRef=*/ false);

	// Replace references inside each individual component (overkill, but required in the case that all pointers to the new components were clobbered by deserializing the old stuff)
	for (int32 ComponentIndex = 0; ComponentIndex < ComponentsOnNewObject.Num(); ++ComponentIndex)
	{
		UObject* NewComponent = ComponentsOnNewObject[ComponentIndex];
		FArchiveReplaceObjectRef<UObject> ReplaceInComponentAr(NewComponent, ReferenceReplacementMap, /*bNullPrivateRefs=*/ false, /*bIgnoreOuterRef=*/ false, /*bIgnoreArchetypeRef=*/ false);
	}

	// Restore the root component reference
	if(NewActor != NULL)
	{
		NewActor->SetRootComponent(SavedRootComponent);
		NewActor->ResetOwnedComponents();
	}

	bool bDumpProperties = CVarDumpCopyPropertiesForUnrelatedObjects.GetValueOnGameThread() != 0;
	// Uncomment the next line to debug CPFUO for a specific object:
	// bDumpProperties |= (NewObject->GetName().InStr(TEXT("Charm_Vim")) != INDEX_NONE);
	if (bDumpProperties)
	{
		DumpObject(TEXT("CopyPropertiesForUnrelatedObjects: Old"), OldObject);
		DumpObject(TEXT("CopyPropertiesForUnrelatedObjects: New"), NewObject);
	}

	// Now notify any tools that aren't already updated via the FArchiveReplaceObjectRef path
	if( GEngine != NULL )
	{
		GEngine->NotifyToolsOfObjectReplacement(ReferenceReplacementMap);
	}
}

// This is a really bad hack for UBlueprintFunctionLibrary::GetFunctionCallspace. See additional comments there.
bool UEngine::ShouldAbsorbAuthorityOnlyEvent()
{
	for (auto It = WorldList.CreateIterator(); It; ++It)
	{
		FWorldContext &Context = *It;
		bool useIt = false;
		if (GPlayInEditorID != -1)
		{
			if (Context.WorldType == EWorldType::PIE && Context.PIEInstance == GPlayInEditorID)
			{
				useIt = true;
			}
		}
		else
		{
			if (Context.WorldType == EWorldType::Game)
			{
				useIt = true;
			}
		}

		if (useIt)
		{
			return (Context.World()->GetNetMode() ==  NM_Client);
		}
	}
	return false;
}


UDeviceProfileManager* UEngine::GetDeviceProfileManager()
{
	if(DeviceProfileManager == NULL)
	{
		DeviceProfileManager = ConstructObject< UDeviceProfileManager >( UDeviceProfileManager::StaticClass(), GetTransientPackage(), TEXT("GlobalDeviceProfileManager"), RF_Public|RF_Transient );
	}

	return DeviceProfileManager;
}


bool UEngine::ShouldAbsorbCosmeticOnlyEvent()
{
	for (auto It = WorldList.CreateIterator(); It; ++It)
	{
		FWorldContext &Context = *It;
		bool useIt = false;
		if (GPlayInEditorID != -1)
		{
			if (Context.WorldType == EWorldType::PIE && Context.PIEInstance == GPlayInEditorID)
			{
				useIt = true;
			}
		}
		else
		{
			if (Context.WorldType == EWorldType::Game)
			{
				useIt = true;
			}
		}

		if (useIt)
		{
			return (Context.World()->GetNetMode() == NM_DedicatedServer);
		}
	}
	return false;
}

static void SetNearClipPlane(const TArray<FString>& Args)
{
	const float MinClipPlane = 1.0f;
	float NewClipPlane = 20.0f;
	if (Args.Num())
	{
		NewClipPlane = FCString::Atof(*Args[0]);
	}
	FlushRenderingCommands();
	GNearClippingPlane = FMath::Max(NewClipPlane,MinClipPlane);
}
FAutoConsoleCommand GSetNearClipPlaneCmd(TEXT("r.SetNearClipPlane"),TEXT("Set the near clipping plane (in cm)"),FConsoleCommandWithArgsDelegate::CreateStatic(SetNearClipPlane));

bool AllowHighQualityLightmaps()
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HighQualityLightMaps"));
	return CVar->GetValueOnAnyThread() != 0;
}

// Helper function for changing system resolution via the r.setres console command
void FSystemResolution::RequestResolutionChange(int32 InResX, int32 InResY, EWindowMode::Type InWindowMode)
{
	FString WindowModeSuffix;
	switch (InWindowMode)
	{
		case EWindowMode::Windowed:
		{
			WindowModeSuffix = TEXT("w");
		} break;
		case EWindowMode::WindowedMirror:
		{
			WindowModeSuffix = TEXT("wm");
		} break;
		case EWindowMode::WindowedFullscreen:
		{
			WindowModeSuffix = TEXT("wf");
		} break;
		case EWindowMode::Fullscreen:
		{
			WindowModeSuffix = TEXT("f");
		} break;
	}

	FString NewValue = FString::Printf(TEXT("%dx%d%s"), InResX, InResY, *WindowModeSuffix);
	CVarSystemResolution->Set(*NewValue);
}


void UEngine::HandleScreenshotCaptured(int32 Width, int32 Height, const TArray<FColor>& Colors)
{
#if WITH_EDITOR
	if(GIsDumpingMovie && Colors.Num() > 0)
	{
		struct Local
		{
			

			static FString GenerateScreenshotFilename(const FString& Extension)
			{
				static const int32 MaxTestScreenShotIndex = 65536;
				static int32 ScreenShotIndex = 0;

				FString BaseFileName;
				FScreenshotRequest::CreateViewportScreenShotFilename(BaseFileName);

				for (int32 TestScreenShotIndex = ScreenShotIndex + 1; TestScreenShotIndex < MaxTestScreenShotIndex; ++TestScreenShotIndex)
				{
					const FString TestFileName = FString::Printf(TEXT("%s%05i.%s"), *BaseFileName, TestScreenShotIndex, *Extension);
					if (IFileManager::Get().FileSize(*TestFileName) < 0)
					{
						ScreenShotIndex = TestScreenShotIndex;
						return TestFileName;
					}
				}

				UE_LOG(LogEngine, Error, TEXT("Could not generate valid screenshot filename"));
				return FString();
			}
		};

		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );

		switch(MatineeCaptureType.GetValue())
		{
		default:
		case EMatineeCaptureType::BMP:
			{
				const FString Filename = Local::GenerateScreenshotFilename(TEXT("bmp"));
				if (Filename.Len() > 0)
				{
					FFileHelper::CreateBitmap(*Filename, Width, Height, Colors.GetTypedData());
				}
			}
			break;
		case EMatineeCaptureType::PNG:
			{
				const FString Filename = Local::GenerateScreenshotFilename(TEXT("png"));
				if (Filename.Len() > 0)
				{
					IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
					if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(&Colors[0], Colors.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
					{
						FFileHelper::SaveArrayToFile(ImageWrapper->GetCompressed(), *Filename);
					}
				}
			}
			break;
		case EMatineeCaptureType::JPEG:
			{
				const FString Filename = Local::GenerateScreenshotFilename(TEXT("jpeg"));
				if (Filename.Len() > 0)
				{
					IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
					if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(&Colors[0], Colors.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
					{
						FFileHelper::SaveArrayToFile(ImageWrapper->GetCompressed(), *Filename);
					}
				}
			}
			break;
		case EMatineeCaptureType::AVI:
			// Do nothing in this case
			break;
		}
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
// STATS

/** Utility that gets a color for a particular level status */
FColor GetColorForLevelStatus(int32 Status)
{
	FColor Color = FColor(255, 255, 255);
	switch (Status)
	{
	case LEVEL_Visible:
		Color = FColor(255, 0, 0);	// red  loaded and visible
		break;
	case LEVEL_MakingVisible:
		Color = FColor(255, 128, 0);	// orange, in process of being made visible
		break;
	case LEVEL_Loading:
		Color = FColor(255, 0, 255);	// purple, in process of being loaded
		break;
	case LEVEL_Loaded:
		Color = FColor(255, 255, 0);	// yellow loaded but not visible
		break;
	case LEVEL_UnloadedButStillAround:
		Color = FColor(0, 0, 255);	// blue  (GC needs to occur to remove this)
		break;
	case LEVEL_Unloaded:
		Color = FColor(0, 255, 0);	// green
		break;
	case LEVEL_Preloading:
		Color = FColor(255, 0, 255);	// purple (preloading)
		break;
	default:
		break;
	};

	return Color;
}

void UEngine::ExecEngineStat(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* InName)
{
	// Store a ptr to the viewport that needs to process this stat command
	GStatProcessingViewportClient = ViewportClient;

	FString StatCommand = TEXT("STAT ");
	StatCommand += InName;
	Exec(World, *StatCommand, *GLog);
}

bool UEngine::IsEngineStat(const FString& InName)
{
	for (int32 StatIdx = 0; StatIdx < EngineStats.Num(); StatIdx++)
	{
		const FEngineStatFuncs& EngineStat = EngineStats[StatIdx];
		FString CommandName = EngineStat.CommandName.ToString();
		if (CommandName.RemoveFromStart(TEXT("STAT_")) && CommandName == InName)
		{
			return true;
		}
	}
	return false;
}

void UEngine::SetEngineStat(UWorld* World, FCommonViewportClient* ViewportClient, const FString& InName, const bool bShow)
{
	check(ViewportClient);
	if (IsEngineStat(InName) && ViewportClient->IsStatEnabled(*InName) != bShow)
	{
		ExecEngineStat(World, ViewportClient, *InName);
	}
}

void UEngine::SetEngineStats(UWorld* World, FCommonViewportClient* ViewportClient, const TArray<FString>& InNames, const bool bShow)
{
	for (int32 StatIdx = 0; StatIdx < InNames.Num(); StatIdx++)
	{
		// If we need to disable, do it in the reverse order incase one stat affects another
		const int32 StatIndex = bShow ? StatIdx : (InNames.Num() - 1) - StatIdx;
		SetEngineStat(World, ViewportClient, *InNames[StatIndex], bShow);
	}
}

void UEngine::RenderEngineStats(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 LHSX, int32& InOutLHSY, int32 RHSX, int32& InOutRHSY, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	for (int32 StatIdx = 0; StatIdx < EngineStats.Num(); StatIdx++)
	{
		const FEngineStatFuncs& EngineStat = EngineStats[StatIdx];
		FString CommandName = EngineStat.CommandName.ToString();
		if (EngineStat.RenderFunc && CommandName.RemoveFromStart(TEXT("STAT_")) && (!Viewport->GetClient() || Viewport->GetClient()->IsStatEnabled(*CommandName)))
		{
			// Render the stat either on the left or right hand side of the screen, keeping track of the new Y position
			const int32 StatX = EngineStat.bIsRHS ? RHSX : LHSX;
			int32* StatY = EngineStat.bIsRHS ? &InOutRHSY : &InOutLHSY;
			*StatY = (this->*(EngineStat.RenderFunc))(World, Viewport, Canvas, StatX, *StatY, ViewLocation, ViewRotation);
		}
	}
}

// VERSION
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
int32 UEngine::RenderStatVersion(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	if (!GIsHighResScreenshot && !GIsDumpingMovie && GAreScreenMessagesEnabled)
	{
		if (!bSuppressMapWarnings)
		{
			FCanvasTextItem TextItem(FVector2D(X - 40, Y), FText::FromString(Viewport->AppVersionString), GetSmallFont(), FLinearColor::Yellow);
			TextItem.EnableShadow(FLinearColor::Black);
			Canvas->DrawItem(TextItem);
			Y += TextItem.DrawnSize.Y;
		}
	}
	return Y;
}
#endif

// DETAILED
bool UEngine::ToggleStatDetailed(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	check(ViewportClient);

	// Each of these stats should call "Detailed -Skip" when they themselves are disabled
	static bool bSetup = false;
	static TArray<FString> DetailedStats;
	if (!bSetup)
	{
		bSetup = true;
		DetailedStats.Add(TEXT("FPS"));
		DetailedStats.Add(TEXT("Unit"));
		DetailedStats.Add(TEXT("UnitMax"));
		DetailedStats.Add(TEXT("UnitGraph"));
		DetailedStats.Add(TEXT("Raw"));
	}

	// If any of the detailed stats are inactive, take this as enabling all, unless 'Skip' is specifically specified
	const bool bSkip = Stream ? FParse::Param(Stream, TEXT("Skip")) : false;
	if (!bSkip)
	{
		// Enable or disable all the other stats depending on the current state
		const bool bShowDetailed = ViewportClient->IsStatEnabled(TEXT("Detailed"));
		SetEngineStats(World, ViewportClient, DetailedStats, bShowDetailed);

		// Extra stat, needs to do the opposite of the others (order of exec unimportant)
		SetEngineStat(World, ViewportClient, TEXT("UnitTime"), !bShowDetailed);
	}

	return true;
}

// FPS
bool UEngine::ToggleStatFPS(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	const bool bShowFPS = ViewportClient->IsStatEnabled(TEXT("FPS"));
	const bool bShowDetailed = ViewportClient->IsStatEnabled(TEXT("Detailed"));
	if (!bShowFPS && bShowDetailed)
	{
		// Since we're turning this off, we also need to toggle off detailed too
		ExecEngineStat(World, ViewportClient, TEXT("Detailed -Skip"));
	}

	return true;
}

int32 UEngine::RenderStatFPS(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	// Pick a larger font on console.
	UFont* Font = FPlatformProperties::SupportsWindowedMode() ? GetSmallFont() : GetMediumFont();

	// Choose the counter color based on the average framerate.
	FColor FPSColor = GAverageFPS < 20.0f ? FColor(255, 0, 0) : (GAverageFPS < 29.5f ? FColor(255, 255, 0) : FColor(0, 255, 0));

	// Start drawing the various counters.
	const int32 RowHeight = FMath::TruncToInt(Font->GetMaxCharHeight() * 1.1f);
	// Draw the FPS counter.
	Canvas->DrawShadowedString(
		X,
		Y,
		*FString::Printf(TEXT("%5.2f FPS"), GAverageFPS),
		Font,
		FPSColor
		);
	Y += RowHeight;

	// Draw the frame time.
	Canvas->DrawShadowedString(
		X,
		Y,
		*FString::Printf(TEXT("%5.2f ms"), GAverageMS),
		Font,
		FPSColor
		);
	Y += RowHeight;
	return Y;
}

// HITCHES
bool UEngine::ToggleStatHitches(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	FPlatformProcess::Sleep(0.11f); // cause a hitch so it is evidently working
	return false;
}

int32 UEngine::RenderStatHitches(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	// Forward this draw request to the viewport client
	if (Viewport->GetClient())
	{
		checkf(Viewport->GetClient()->GetStatHitchesData(), TEXT("StatHitchesData must be allocated for this viewport if you wish to display stat."));
		Y = Viewport->GetClient()->GetStatHitchesData()->DrawStat(Viewport, Canvas, X, Y);
	}
	return Y;
}

// SUMMARY
int32 UEngine::RenderStatSummary(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	// Pick a larger font on console.
	UFont* Font = FPlatformProperties::SupportsWindowedMode() ? GetSmallFont() : GetMediumFont();

	// Retrieve allocation info.
	FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
	float MemoryInMByte = MemoryStats.UsedPhysical / 1024.f / 1024.f;

	// Draw the memory summary stats.
	Canvas->DrawShadowedString(
		X,
		Y,
		*FString::Printf(TEXT("%5.2f MByte"), MemoryInMByte),
		Font,
		FColor(30, 144, 255)
		);

	const int32 RowHeight = FMath::TruncToInt(Font->GetMaxCharHeight() * 1.1f);
	Y += RowHeight;
	return Y;
}

// NAMEDEVENTS
bool UEngine::ToggleStatNamedEvents(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	// Enable emission of named events and force enable cycle stats.
	check(ViewportClient);
	if (ViewportClient->IsStatEnabled(TEXT("NamedEvents")))
	{
		if (GCycleStatsShouldEmitNamedEvents == 0)
		{
			StatsMasterEnableAdd();
		}
		GCycleStatsShouldEmitNamedEvents++;
	}
	// Disable emission of named events and force-enabling cycle stats.
	else
	{
		if (GCycleStatsShouldEmitNamedEvents == 1)
		{
			StatsMasterEnableSubtract();
		}
		GCycleStatsShouldEmitNamedEvents = FMath::Max(0, GCycleStatsShouldEmitNamedEvents - 1);
	}
	return false;
}

int32 UEngine::RenderStatNamedEvents(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	FCanvasTextItem TextItem(FVector2D(X - 40, Y), LOCTEXT("NAMEDEVENTSENABLED", "NAMED EVENTS ENABLED"), GetSmallFont(), FLinearColor::Blue);
	TextItem.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(TextItem);
	Y += TextItem.DrawnSize.Y;
	return Y;
}

// COLORLIST
int32 UEngine::RenderStatColorList(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	UFont* Font = GetTinyFont();

	const int32 LineHeight = FMath::TruncToInt(Font->GetMaxCharHeight());
	const int32 ColorsNum = GColorList.GetColorsNum();
	const int32 MaxLinesInColumn = 35;
	const int32 ColumnsNum = FMath::CeilToInt((float)ColorsNum / (float)MaxLinesInColumn);

	Y += 16;
	const int32 SavedY = Y;
	int32 LowestY = Y + MaxLinesInColumn * LineHeight;

	// Draw columns with color list.
	for (int32 ColumnIndex = 0; ColumnIndex < ColumnsNum; ColumnIndex++)
	{
		int32 LineWidthMax = 0;

		for (int32 ColColorIndex = 0; ColColorIndex < MaxLinesInColumn; ColColorIndex++)
		{
			const int32 ColorIndex = ColumnIndex * MaxLinesInColumn + ColColorIndex;
			if (ColorIndex >= ColorsNum)
			{
				break;
			}

			const FColor& Color = GColorList.GetFColorByIndex(ColorIndex);
			const FString Line = *FString::Printf(TEXT("%3i %s %s"), ColorIndex, *GColorList.GetColorNameByIndex(ColorIndex), *Color.ToString());

			LineWidthMax = FMath::Max(LineWidthMax, Font->GetStringSize(*Line));

			Canvas->DrawShadowedString(X, Y, *Line, Font, FLinearColor(Color));
			Y += LineHeight;
		}

		X += LineWidthMax;
		LineWidthMax = 0;
		Y = SavedY;
	}
	return LowestY;
}

// LEVELS
int32 UEngine::RenderStatLevels(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	int32 MaxY = Y;
	const TArray<FSubLevelStatus> SubLevelsStatusList = GetSubLevelsStatus(World);

	// now do drawing to the screen

	// Render unloaded levels in red, loaded ones in yellow and visible ones in green. Blue signifies that a level is unloaded but
	// hasn't been garbage collected yet.
	Canvas->DrawShadowedString(X, Y, TEXT("Levels"), GetSmallFont(), FLinearColor::White);
	Y += 12;

	if (SubLevelsStatusList.Num())
	{
		// First entry - always persistent level
		FString MapName	= SubLevelsStatusList[0].PackageName.ToString();
		if (SubLevelsStatusList[0].bPlayerInside)
		{
			MapName = *FString::Printf( TEXT("->  %s"), *MapName );
		}
		else
		{
			MapName = *FString::Printf( TEXT("    %s"), *MapName );
		}

		Canvas->DrawShadowedString(X, Y, *MapName, GetSmallFont(), FColor(127, 127, 127));
		Y += 12;
	}

	int32 BaseY = Y;

	// now draw the levels
	for (int32 LevelIdx = 1; LevelIdx < SubLevelsStatusList.Num(); ++LevelIdx)
	{
		const FSubLevelStatus& LevelStatus = SubLevelsStatusList[LevelIdx];
		
		// Wrap around at the bottom.
		if (Y > Viewport->GetSizeXY().Y - 30)
		{
			MaxY = FMath::Max(MaxY, Y);
			Y = BaseY;
			X += 250;
		}

		FColor	Color = GetColorForLevelStatus(LevelStatus.StreamingStatus);
		FString DisplayName = LevelStatus.PackageName.ToString();

		if (LevelStatus.LODIndex != INDEX_NONE)
		{
			DisplayName += FString::Printf(TEXT(" [LOD%d]"), LevelStatus.LODIndex+1);
		}

		UPackage* LevelPackage = FindObjectFast<UPackage>(NULL, LevelStatus.PackageName);
				
		if (LevelPackage
			&& (LevelPackage->GetLoadTime() > 0)
			&& (LevelStatus.StreamingStatus != LEVEL_Unloaded))
		{
			DisplayName += FString::Printf(TEXT(" - %4.1f sec"), LevelPackage->GetLoadTime());
		}
		else if (GetAsyncLoadPercentage(*LevelStatus.PackageName.ToString()) >= 0)
		{
			const int32 Percentage = FMath::TruncToInt(GetAsyncLoadPercentage(*LevelStatus.PackageName.ToString()));
			DisplayName += FString::Printf(TEXT(" - %3i %%"), Percentage);
		}

		if (LevelStatus.bPlayerInside)
		{
			DisplayName = *FString::Printf(TEXT("->  %s"), *DisplayName);
		}
		else
		{
			DisplayName = *FString::Printf(TEXT("    %s"), *DisplayName);
		}
		
		Canvas->DrawShadowedString(X + 4, Y, *DisplayName, GetSmallFont(), Color);
		Y += 12;
	}
	return FMath::Max(MaxY, Y);
}

// LEVELMAP
int32 UEngine::RenderStatLevelMap(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	const FVector2D MapOrigin = FVector2D(512, 128);
	const FVector2D MapSize = FVector2D(512, 512);

	// Get status of each sublevel (by name)
	const TArray<FSubLevelStatus> SubLevelsStatusList = GetSubLevelsStatus(World);

	// First iterate to find bounds of all streaming volumes
	FBox AllVolBounds(0);
	for (const FSubLevelStatus& LevelStatus : SubLevelsStatusList)
	{
		ULevelStreaming* LevelStreaming = World->GetLevelStreamingForPackageName(LevelStatus.PackageName);
		if (LevelStreaming && LevelStreaming->bDrawOnLevelStatusMap)
		{
			AllVolBounds += LevelStreaming->GetStreamingVolumeBounds();
		}
	}

	// We need to ensure the XY aspect ratio of AllVolBounds is the same as the map

	// Work out scale factor between map and world
	FVector VolBoundsSize = (AllVolBounds.Max - AllVolBounds.Min);
	float ScaleX = MapSize.X / VolBoundsSize.X;
	float ScaleY = MapSize.Y / VolBoundsSize.Y;
	float UseScale = FMath::Min(ScaleX, ScaleY); // Pick the smallest scaling factor

	// Resize AllVolBounds
	FVector NewVolBoundsSize = VolBoundsSize;
	NewVolBoundsSize.X = MapSize.X / UseScale;
	NewVolBoundsSize.Y = MapSize.Y / UseScale;
	FVector DeltaBounds = (NewVolBoundsSize - VolBoundsSize);
	AllVolBounds.Min -= 0.5f * DeltaBounds;
	AllVolBounds.Max += 0.5f * DeltaBounds;

	// Find world-space location for top-left and bottom-right corners of map
	FVector2D TopLeftPos(AllVolBounds.Max.X, AllVolBounds.Min.Y); // max X, min Y
	FVector2D BottomRightPos(AllVolBounds.Min.X, AllVolBounds.Max.Y); // min X, max Y


	// Now we iterate and actually draw volumes
	for (const FSubLevelStatus& LevelStatus : SubLevelsStatusList)
	{
		// Find the color to draw this level in
		FColor StatusColor = GetColorForLevelStatus(LevelStatus.StreamingStatus);
		StatusColor.A = 64; // make it translucent

		ULevelStreaming* LevelStreaming = World->GetLevelStreamingForPackageName(LevelStatus.PackageName);
		if (LevelStreaming && LevelStreaming->bDrawOnLevelStatusMap)
		{
			for (int32 VolIdx = 0; VolIdx < LevelStreaming->EditorStreamingVolumes.Num(); VolIdx++)
			{
				ALevelStreamingVolume* StreamingVol = LevelStreaming->EditorStreamingVolumes[VolIdx];
				if (StreamingVol)
				{
					DrawVolumeOnCanvas(StreamingVol, Canvas, TopLeftPos, BottomRightPos, MapOrigin, MapSize, StatusColor);
				}
			}
		}
	}



	// Now we want to draw the player(s) location on the map
	{
		// Find map location for arrow
		check(ViewLocation);
		const FVector2D PlayerMapPos = TransformLocationToMap(TopLeftPos, BottomRightPos, MapOrigin, MapSize, *ViewLocation);

		// Make verts for little rotated arrow
		check(ViewRotation);
		float PlayerYaw = (ViewRotation->Yaw * PI / 180.f) - (0.5f * PI); // We have to add 90 degrees because +X in world space means -Y in map space
		const FVector2D M0 = PlayerMapPos + RotateVec2D(FVector2D(7, 0), PlayerYaw);
		const FVector2D M1 = PlayerMapPos + RotateVec2D(FVector2D(-7, 5), PlayerYaw);
		const FVector2D M2 = PlayerMapPos + RotateVec2D(FVector2D(-7, -5), PlayerYaw);

		FCanvasTriangleItem TriItem(M0, M1, M2, GWhiteTexture);
		Canvas->DrawItem(TriItem);
	}
	return Y;
}

// UNIT
bool UEngine::ToggleStatUnit(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	check(ViewportClient);
	const bool bShowUnitMaxTimes = ViewportClient->IsStatEnabled(TEXT("UnitMax"));
	if (bShowUnitMaxTimes != false)
	{
		// Toggle UnitMax back to Inactive
		ExecEngineStat(World, ViewportClient, TEXT("UnitMax"));

		// Force Unit back to Active if turning UnitMax off
		SetEngineStat(World, ViewportClient, TEXT("Unit"), true);
	}

	const bool bShowUnitTimes = ViewportClient->IsStatEnabled(TEXT("Unit"));
	const bool bShowDetailed = ViewportClient->IsStatEnabled(TEXT("Detailed"));
	if (!bShowUnitTimes && bShowDetailed)
	{
		// Since we're turning this off, we also need to toggle off detailed too
		ExecEngineStat(World, ViewportClient, TEXT("Detailed -Skip"));
	}

	return true;
}

int32 UEngine::RenderStatUnit(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	// Forward this draw request to the viewport client
	if (Viewport->GetClient())
	{
		checkf(Viewport->GetClient()->GetStatUnitData(), TEXT("StatUnitData must be allocated for this viewport if you wish to display stat."));
		Y = Viewport->GetClient()->GetStatUnitData()->DrawStat(Viewport, Canvas, X, Y);
	}
	return Y;
}

// UNITMAX
#if !UE_BUILD_SHIPPING
bool UEngine::ToggleStatUnitMax(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	check(ViewportClient);
	const bool bShowUnitMaxTimes = ViewportClient->IsStatEnabled(TEXT("UnitMax"));
	if (bShowUnitMaxTimes)
	{
		// Force Unit to Active
		SetEngineStat(World, ViewportClient, TEXT("Unit"), true);

		// Force UnitMax to true as Unit will have Toggled it back to false
		SetEngineStat(World, ViewportClient, TEXT("UnitMax"), true);
	}
	else
	{
		const bool bShowDetailed = ViewportClient->IsStatEnabled(TEXT("Detailed"));
		if (bShowDetailed)
		{
			// Since we're turning this off, we also need to toggle off detailed too
			ExecEngineStat(World, ViewportClient, TEXT("Detailed -Skip"));
		}
	}
	return true;
}

// UNITGRAPH
bool UEngine::ToggleStatUnitGraph(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	check(ViewportClient);
	const bool bShowUnitGraph = ViewportClient->IsStatEnabled(TEXT("UnitGraph"));
	if (bShowUnitGraph)
	{
		// Force Unit to Active
		SetEngineStat(World, ViewportClient, TEXT("Unit"), true);

		// Force UnitTime to Active
		SetEngineStat(World, ViewportClient, TEXT("UnitTime"), true);	
	}
	else
	{
		const bool bShowDetailed = ViewportClient->IsStatEnabled(TEXT("Detailed"));
		if (bShowDetailed)
		{
			// Since we're turning this off, we also need to toggle off detailed too
			ExecEngineStat(World, ViewportClient, TEXT("Detailed -Skip"));
		}
	}
	return true;
}

// UNITTIME
bool UEngine::ToggleStatUnitTime(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	check(ViewportClient);
	const bool bShowUnitTime = ViewportClient->IsStatEnabled(TEXT("UnitTime"));
	if (bShowUnitTime)
	{
		// Force UnitGraph to Active
		SetEngineStat(World, ViewportClient, TEXT("UnitGraph"), true);
	}
	return true;
}

// RAW
bool UEngine::ToggleStatRaw(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	const bool bShowRaw = ViewportClient->IsStatEnabled(TEXT("Raw"));
	const bool bShowDetailed = ViewportClient->IsStatEnabled(TEXT("Detailed"));
	if (bShowRaw)
	{
		// Force UnitGraph to Active
		SetEngineStat(World, ViewportClient, TEXT("UnitGraph"), true);
	}
	else if (bShowDetailed)
	{
		// Since we're turning this off, we also need to toggle off detailed too
		ExecEngineStat(World, ViewportClient, TEXT("Detailed -Skip"));
	}
	return true;
}
#endif

// REVERB
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
int32 UEngine::RenderStatReverb(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	FAudioDevice* AudioDevice = GetAudioDevice();
	if (AudioDevice)
	{
		UReverbEffect* ReverbEffect = (AudioDevice->Effects ? AudioDevice->Effects->GetCurrentReverbEffect() : NULL);
		FString TheString;
		if (ReverbEffect)
		{
			TheString = FString::Printf(TEXT("Active Reverb Effect: %s"), *ReverbEffect->GetName());
			Canvas->DrawShadowedString(X, Y, *TheString, GetSmallFont(), FLinearColor::White);
			Y += 12;

			ULocalPlayer* LocalPlayer = GetFirstGamePlayer(World);
			if (LocalPlayer)
			{
				const AReverbVolume* ReverbVolume = AudioDevice->CurrentReverbVolume;
				if (ReverbVolume && ReverbVolume->Settings.ReverbEffect)
				{
					TheString = FString::Printf(TEXT("  Reverb Volume Effect: %s (Priority: %g Volume Name: %s)"), *ReverbVolume->Settings.ReverbEffect->GetName(), ReverbVolume->Priority, *ReverbVolume->GetName());
				}
				else
				{
					TheString = TEXT("  Reverb Volume: None");
				}
				Canvas->DrawShadowedString(X, Y, *TheString, GetSmallFont(), FLinearColor::White);
				Y += 12;
				if (AudioDevice->ActivatedReverbs.Num() == 0)
				{
					TheString = TEXT("  Activated Reverb: None");
					Canvas->DrawShadowedString(X, Y, *TheString, GetSmallFont(), FLinearColor::White);
					Y += 12;
				}
				else if (AudioDevice->ActivatedReverbs.Num() == 1)
				{
					auto It = AudioDevice->ActivatedReverbs.CreateConstIterator();
					TheString = FString::Printf(TEXT("  Activated Reverb Effect: %s (Priority: %g Tag: '%s')"), *It.Value().ReverbSettings.ReverbEffect->GetName(), It.Value().Priority, *It.Key().ToString());
					Canvas->DrawShadowedString(X, Y, *TheString, GetSmallFont(), FLinearColor::White);
					Y += 12;
				}
				else
				{
					Canvas->DrawShadowedString(X, Y, TEXT("  Activated Reverb Effects:"), GetSmallFont(), FLinearColor::White);
					Y += 12;
					TMap<int32, FString> PrioritySortedActivatedReverbs;
					for (auto It = AudioDevice->ActivatedReverbs.CreateConstIterator(); It; ++It)
					{
						TheString = FString::Printf(TEXT("    %s (Priority: %g Tag: '%s')"), *It.Value().ReverbSettings.ReverbEffect->GetName(), It.Value().Priority, *It.Key().ToString());
						PrioritySortedActivatedReverbs.Add(It.Value().Priority, TheString);
					}
					for (auto It = PrioritySortedActivatedReverbs.CreateConstIterator(); It; ++It)
					{
						Canvas->DrawShadowedString(X, Y, *It.Value(), GetSmallFont(), FLinearColor::White);
						Y += 12;
					}
				}
			}
		}
		else
		{
			TheString = TEXT("Active Reverb Effect: None");
			Canvas->DrawShadowedString(X, Y, *TheString, GetSmallFont(), FLinearColor::White);
			Y += 12;
		}
	}
	return Y;
}

// SOUNDMIXES
int32 UEngine::RenderStatSoundMixes(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	FAudioDevice* AudioDevice = GetAudioDevice();
	if (AudioDevice)
	{
		Canvas->DrawShadowedString(X, Y, TEXT("Active Sound Mixes:"), GetSmallFont(), FColor(0, 255, 0));
		Y += 12;

		if (AudioDevice->SoundMixModifiers.Num() > 0)
		{
			USoundMix* CurrentEQMix = AudioDevice->Effects->GetCurrentEQMix();

			for (TMap< USoundMix*, FSoundMixState >::TIterator It(AudioDevice->SoundMixModifiers); It; ++It)
			{
				uint32 TotalRefCount = It.Value().ActiveRefCount + It.Value().PassiveRefCount;
				FString TheString = FString::Printf(TEXT("%s - Fade Proportion: %1.2f - Total Ref Count: %i"), *It.Key()->GetName(), It.Value().InterpValue, TotalRefCount);

				FColor TextColour = FColor(255, 255, 255);
				if (It.Key() == CurrentEQMix)
				{
					TextColour = FColor(255, 255, 0);
				}

				Canvas->DrawShadowedString(X + 12, Y, *TheString, GetSmallFont(), TextColour);
				Y += 12;
			}

		}
		else
		{
			Canvas->DrawShadowedString(X + 12, Y, TEXT("None"), GetSmallFont(), FColor(255, 255, 255));
			Y += 12;
		}
	}
	return Y;
}

// SOUNDWAVES
int32 UEngine::RenderStatSoundWaves(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	Canvas->DrawShadowedString(X, Y, TEXT("Active Sound Waves:"), GetSmallFont(), FLinearColor::White);
	Y += 12;

	TSet<FActiveSound*> ActiveSounds;

	FAudioDevice* AudioDevice = GetAudioDevice();
	if (AudioDevice)
	{
		TArray<FWaveInstance*> WaveInstances;
		int32 FirstActiveIndex = AudioDevice->GetSortedActiveWaveInstances(WaveInstances, ESortedActiveWaveGetType::QueryOnly);

		for (int32 InstanceIndex = FirstActiveIndex; InstanceIndex < WaveInstances.Num(); InstanceIndex++)
		{
			FWaveInstance* WaveInstance = WaveInstances[InstanceIndex];

			ActiveSounds.Add(WaveInstance->ActiveSound);

			AActor* SoundOwner = WaveInstance->ActiveSound->AudioComponent.IsValid() ? WaveInstance->ActiveSound->AudioComponent->GetOwner() : NULL;
			USoundClass* SoundClass = WaveInstance->SoundClass;

			FString TheString = *FString::Printf(TEXT("%4i.    %6.2f  %s   Owner: %s   SoundClass: %s"),
				InstanceIndex,
				WaveInstance->GetActualVolume(),
				*WaveInstance->WaveData->GetPathName(),
				SoundOwner ? *SoundOwner->GetName() : TEXT("None"),
				SoundClass ? *SoundClass->GetName() : TEXT("None"));

			Canvas->DrawShadowedString(X, Y, *TheString, GetSmallFont(), FColor(255, 255, 255));
			Y += 12;
		}

		int32 ActiveInstances = WaveInstances.Num() - FirstActiveIndex;
		int32 R, G, B;
		R = G = B = 0;
		int32 Max = AudioDevice->MaxChannels / 2;
		float f = FMath::Clamp<float>((float)(ActiveInstances - Max) / (float)Max, 0.f, 1.f);
		R = FMath::TruncToInt(f * 255);
		if (ActiveInstances > Max)
		{
			f = FMath::Clamp<float>((float)(Max - ActiveInstances) / (float)Max, 0.5f, 1.f);
		}
		else
		{
			f = 1.0f;
		}
		G = FMath::TruncToInt(f * 255);

		Canvas->DrawShadowedString(X, Y, *FString::Printf(TEXT(" Total: %i"), ActiveInstances), GetSmallFont(), FColor(R, G, B));
		Y += 12;
	}
	return Y;
}

// SOUNDCUES
int32 UEngine::RenderStatSoundCues(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	TSet<FActiveSound*> ActiveSounds;

	FAudioDevice* AudioDevice = GetAudioDevice();
	if (AudioDevice)
	{
		TArray<FWaveInstance*> WaveInstances;
		int32 FirstActiveIndex = AudioDevice->GetSortedActiveWaveInstances(WaveInstances, ESortedActiveWaveGetType::QueryOnly);

		for (int32 InstanceIndex = FirstActiveIndex; InstanceIndex < WaveInstances.Num(); InstanceIndex++)
		{
			FWaveInstance* WaveInstance = WaveInstances[InstanceIndex];

			ActiveSounds.Add(WaveInstance->ActiveSound);
		}
	}

	Canvas->DrawShadowedString(X, Y, TEXT("Active Sound Cues:"), GetSmallFont(), FColor(0, 255, 0));
	Y += 12;

	int32 ActiveSoundCount = 0;
	for (FActiveSound* ActiveSound : ActiveSounds)
	{
		USoundClass* SoundClass = ActiveSound->GetSoundClass();
		const FString TheString = FString::Printf(TEXT("%4i. %s %s"), ActiveSoundCount++, *ActiveSound->Sound->GetPathName(), (SoundClass ? *SoundClass->GetName() : TEXT("None")));
		Canvas->DrawShadowedString(X, Y, *TheString, GetSmallFont(), FColor(255, 255, 255));
		Y += 12;
	}

	Canvas->DrawShadowedString(X, Y, *FString::Printf(TEXT("Total: %i"), ActiveSounds.Num()), GetSmallFont(), FColor(0, 255, 0));
	Y += 12;
	return Y;
}
#endif

// SOUNDS
bool UEngine::ToggleStatSounds(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	check(ViewportClient);
	const bool bHelp = Stream ? FCString::Stristr(Stream, TEXT("?")) != NULL : false;
	if (bHelp)
	{
		GLog->Logf(TEXT("stat sounds description"));
		GLog->Logf(TEXT("  stat sounds off - Disables drawing stat sounds"));
		GLog->Logf(TEXT("  stat sounds sort=distance|class|name|waves|default"));
		GLog->Logf(TEXT("      distance - sort list by distance to player"));
		GLog->Logf(TEXT("      class - sort by sound class name"));
		GLog->Logf(TEXT("      name - sort by cue pathname"));
		GLog->Logf(TEXT("      waves - sort by waves' num"));
		GLog->Logf(TEXT("      default - sorting is no enabled"));
		GLog->Logf(TEXT("  stat sounds -debug - enables debugging mode like showing sound radius sphere and names, but only for cues with enabled property bDebug"));
		GLog->Logf(TEXT(""));
		GLog->Logf(TEXT("Ex. stat sounds sort=class -debug"));
		GLog->Logf(TEXT(" This will show only debug sounds sorted by sound class"));
	}


	uint32 ShowSounds = FViewportClient::ESoundShowFlags::Disabled;
	
	const bool bDebug = Stream ? FParse::Param(Stream, TEXT("debug")) : false;
	ShowSounds |= bDebug ? FViewportClient::ESoundShowFlags::Debug : 0;

	const bool bLongNames = Stream ? FParse::Param(Stream, TEXT("longnames")) : false;
	ShowSounds |= bLongNames ? FViewportClient::ESoundShowFlags::Long_Names : 0;

	FString SortStr;
	if (Stream)
	{
		FParse::Value(Stream, TEXT("sort="), SortStr);
	}
	if (SortStr == TEXT("distance"))
	{
		ShowSounds |= FViewportClient::ESoundShowFlags::Sort_Distance;
	}
	else if (SortStr == TEXT("class"))
	{
		ShowSounds |= FViewportClient::ESoundShowFlags::Sort_Class;
	}
	else if (SortStr == TEXT("name"))
	{
		ShowSounds |= FViewportClient::ESoundShowFlags::Sort_Name;
	}
	else if (SortStr == TEXT("waves"))
	{
		ShowSounds |= FViewportClient::ESoundShowFlags::Sort_WavesNum;
	}
	else
	{
		ShowSounds |= FViewportClient::ESoundShowFlags::Sort_Disabled;
	}

	const bool bHide = Stream ? FParse::Command(&Stream, TEXT("off")) : false;
	if (bHide)
	{
		ShowSounds = FViewportClient::ESoundShowFlags::Disabled;
	}

	ViewportClient->SetSoundShowFlags((FViewportClient::ESoundShowFlags::Type)ShowSounds);

	return true;
}

int32 UEngine::RenderStatSounds(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#if UE_BUILD_DEBUG

	typedef TMap< const FActiveSound*, FSoundInfo* > TMapSounds;

#else

	typedef TMemStackAllocator<> TMemStackGameAllocator;
	typedef TSetAllocator<TSparseArrayAllocator<TMemStackGameAllocator, TMemStackGameAllocator>, TMemStackGameAllocator> TMapStackAllocator;
	typedef TMap< const FActiveSound*, FSoundInfo*, TMapStackAllocator > TMapSounds;

#endif // UE_BUILD_DEBUG

	TMapSounds SoundInfos;
	FAudioDevice* AudioDevice = GetAudioDevice();
	const FViewportClient::ESoundShowFlags::Type ShowSounds = Viewport->GetClient() ? Viewport->GetClient()->GetSoundShowFlags() : FViewportClient::ESoundShowFlags::Disabled;
	const bool bDebug = ShowSounds & FViewportClient::ESoundShowFlags::Debug;

	if (AudioDevice)
	{
		// Refresh the wave instances inside audio components.
		static TArray<FWaveInstance*> WaveInstances;
		WaveInstances.Reset();

		int32 FirstActiveIndex = AudioDevice->GetSortedActiveWaveInstances(WaveInstances, ESortedActiveWaveGetType::QueryOnly);

		// Grab the list of all active sound cues.
		const FVector ListenerPosition = AudioDevice->Listeners[0].Transform.GetTranslation();

		const TArray<FActiveSound*>& ActiveSounds = AudioDevice->GetActiveSounds();

		for (int32 Nx = 0; Nx < ActiveSounds.Num(); ++Nx)
		{
			const FActiveSound* ActiveSound = ActiveSounds[Nx];

			if (ActiveSound->Sound)
			{
				if (!bDebug || ActiveSound->Sound->bDebug)
				{
					const FString PathName = ActiveSound->Sound->GetPathName();
					const float Distance = (ListenerPosition - ActiveSound->Transform.GetTranslation()).Size();
					const FName ClassName = (ActiveSound->GetSoundClass() ? ActiveSound->GetSoundClass()->GetFName() : NAME_None);

					SoundInfos.Add(ActiveSound, new FSoundInfo(PathName, Distance, ClassName));
				}
			}
		}

		// Iterate through all wave instances.
		for (int32 InstanceIndex = FirstActiveIndex; InstanceIndex < WaveInstances.Num(); ++InstanceIndex)
		{
			FWaveInstance* WaveInstance = WaveInstances[InstanceIndex];
			FSoundInfo* SoundInfo = SoundInfos.FindRef(WaveInstance->ActiveSound);
			if (SoundInfo)
			{
				SoundInfo->WaveInstances.Add(WaveInstance);
			}
		}

		FString SortingName = TEXT("disabled");

		// Sort the list.
		if (ShowSounds & FViewportClient::ESoundShowFlags::Sort_Name)
		{
			SoundInfos.ValueSort(FCompareFSoundInfoByName());
			SortingName = TEXT("pathname");
		}
		else if (ShowSounds & FViewportClient::ESoundShowFlags::Sort_Distance)
		{
			SoundInfos.ValueSort(FCompareFSoundInfoByDistance());
			SortingName = TEXT("distance");
		}
		else if (ShowSounds & FViewportClient::ESoundShowFlags::Sort_Class)
		{
			SoundInfos.ValueSort(FCompareFSoundInfoByClass());
			SortingName = TEXT("class");
		}
		else if (ShowSounds & FViewportClient::ESoundShowFlags::Sort_WavesNum)
		{
			SoundInfos.ValueSort(FCompareFSoundInfoByWaveInstNum());
			SortingName = TEXT("waves' num");
		}


		Canvas->DrawShadowedString(X, Y, TEXT("Active Sounds:"), GetSmallFont(), FColor(0, 255, 0));
		Y += 12;

		const FString InfoText = FString::Printf(TEXT(" Sorting: %s Debug: %s"), *SortingName, bDebug ? TEXT("enabled") : TEXT("disabled"));
		Canvas->DrawShadowedString(X, Y, *InfoText, GetSmallFont(), FColor(128, 255, 128));
		Y += 12;

		Canvas->DrawShadowedString(X, Y, TEXT("Index Path (Class) Distance"), GetSmallFont(), FColor(0, 255, 0));
		Y += 12;

		int32 TotalSoundWavesNum = 0;
		int32 SoundIndex = 0;
		for (TMapSounds::TConstIterator It(SoundInfos); It; ++It)
		{
			const FSoundInfo& SoundInfo = *It.Value();
			const int32 WaveInstancesNum = SoundInfo.WaveInstances.Num();

			if (WaveInstancesNum > 0)
			{
				{
					const FString TheString = FString::Printf(TEXT("%4i. %s (%s) %6.2f"), SoundIndex, *SoundInfo.PathName, *SoundInfo.ClassName.ToString(), SoundInfo.Distance);
					Canvas->DrawShadowedString(X, Y, *TheString, GetSmallFont(), FColor(255, 255, 255));
					Y += 12;
				}

				// Get the active sound waves.
				for (int32 WaveIndex = 0; WaveIndex < WaveInstancesNum; WaveIndex++)
				{
					FWaveInstance* WaveInstance = SoundInfo.WaveInstances[WaveIndex];
					FSoundSource* Source = AudioDevice->WaveInstanceSourceMap.FindRef(WaveInstance);

					FString SourceDesc = Source ? Source->Describe((ShowSounds & FViewportClient::ESoundShowFlags::Long_Names) != 0) : FString(TEXT("No source"));
					FString TheString = *FString::Printf(TEXT("    %4i. %s"), WaveIndex, *SourceDesc);

					Canvas->DrawShadowedString(X, Y, *TheString, GetSmallFont(), FColor(205, 205, 205));
					Y += 12;

					TotalSoundWavesNum++;
				}
				++SoundIndex;
			}
		}

		Canvas->DrawShadowedString(X, Y, *FString::Printf(TEXT("Total sounds: %i, sound waves: %i"), SoundIndex, TotalSoundWavesNum), GetSmallFont(), FColor(0, 255, 0));
		Y += 12;

		Canvas->DrawShadowedString(X, Y, *FString::Printf(TEXT("Listener position: %s"), *ListenerPosition.ToString()), GetSmallFont(), FColor(0, 255, 0));
		Y += 12;

		// Draw sound cue's sphere.
		if (bDebug)
		{
			for (TMapSounds::TConstIterator SoundInfoIt(SoundInfos); SoundInfoIt; ++SoundInfoIt)
			{
				const FActiveSound& ActiveSound = *SoundInfoIt.Key();
				const FSoundInfo& SoundInfo = *SoundInfoIt.Value();
				const int32 WaveInstancesNum = SoundInfo.WaveInstances.Num();

				if (ActiveSound.Sound->bDebug && SoundInfo.Distance > 100.0f && WaveInstancesNum > 0)
				{
					float SphereRadius = 0.f;
					float SphereInnerRadius = 0.f;

					TMap<EAttenuationShape::Type, FAttenuationSettings::AttenuationShapeDetails> ShapeDetailsMap;
					ActiveSound.CollectAttenuationShapesForVisualization(ShapeDetailsMap);

					if (ShapeDetailsMap.Num() > 0)
					{
						DrawDebugString(World, ActiveSound.Transform.GetTranslation(), SoundInfo.PathName, NULL, FColor::White, 0.01f);

						for (auto ShapeDetailsIt = ShapeDetailsMap.CreateConstIterator(); ShapeDetailsIt; ++ShapeDetailsIt)
						{
							const FAttenuationSettings::AttenuationShapeDetails& ShapeDetails = ShapeDetailsIt.Value();
							switch (ShapeDetailsIt.Key())
							{
							case EAttenuationShape::Sphere:
								if (ShapeDetails.Falloff > 0.f)
								{
									DrawDebugSphere(World, ActiveSound.Transform.GetTranslation(), ShapeDetails.Extents.X + ShapeDetails.Falloff, 10, FColor(155, 155, 255));
									DrawDebugSphere(World, ActiveSound.Transform.GetTranslation(), ShapeDetails.Extents.X, 10, FColor(55, 55, 255));
								}
								else
								{
									DrawDebugSphere(World, ActiveSound.Transform.GetTranslation(), ShapeDetails.Extents.X, 10, FColor(155, 155, 255));
								}
								break;

							case EAttenuationShape::Box:
								if (ShapeDetails.Falloff > 0.f)
								{
									DrawDebugBox(World, ActiveSound.Transform.GetTranslation(), ShapeDetails.Extents + FVector(ShapeDetails.Falloff), ActiveSound.Transform.GetRotation(), FColor(155, 155, 255));
									DrawDebugBox(World, ActiveSound.Transform.GetTranslation(), ShapeDetails.Extents, ActiveSound.Transform.GetRotation(), FColor(55, 55, 255));
								}
								else
								{
									DrawDebugBox(World, ActiveSound.Transform.GetTranslation(), ShapeDetails.Extents, ActiveSound.Transform.GetRotation(), FColor(155, 155, 255));
								}
								break;

							case EAttenuationShape::Capsule:

								if (ShapeDetails.Falloff > 0.f)
								{
									DrawDebugCapsule(World, ActiveSound.Transform.GetTranslation(), ShapeDetails.Extents.X + ShapeDetails.Falloff, ShapeDetails.Extents.Y + ShapeDetails.Falloff, ActiveSound.Transform.GetRotation(), FColor(155, 155, 255));
									DrawDebugCapsule(World, ActiveSound.Transform.GetTranslation(), ShapeDetails.Extents.X, ShapeDetails.Extents.Y, ActiveSound.Transform.GetRotation(), FColor(55, 55, 255));
								}
								else
								{
									DrawDebugCapsule(World, ActiveSound.Transform.GetTranslation(), ShapeDetails.Extents.X, ShapeDetails.Extents.Y, ActiveSound.Transform.GetRotation(), FColor(155, 155, 255));
								}
								break;

							case EAttenuationShape::Cone:
							{
								const FVector Origin = ActiveSound.Transform.GetTranslation() - (ActiveSound.Transform.GetUnitAxis(EAxis::X) * ShapeDetails.ConeOffset);

								if (ShapeDetails.Falloff > 0.f || ShapeDetails.Extents.Z > 0.f)
								{
									const float OuterAngle = FMath::DegreesToRadians(ShapeDetails.Extents.Y + ShapeDetails.Extents.Z);
									const float InnerAngle = FMath::DegreesToRadians(ShapeDetails.Extents.Y);
									DrawDebugCone(World, Origin, ActiveSound.Transform.GetUnitAxis(EAxis::X), ShapeDetails.Extents.X + ShapeDetails.Falloff + ShapeDetails.ConeOffset, OuterAngle, OuterAngle, 10, FColor(155, 155, 255));
									DrawDebugCone(World, Origin, ActiveSound.Transform.GetUnitAxis(EAxis::X), ShapeDetails.Extents.X + ShapeDetails.ConeOffset, InnerAngle, InnerAngle, 10, FColor(55, 55, 255));
								}
								else
								{
									const float Angle = FMath::DegreesToRadians(ShapeDetails.Extents.Y);
									DrawDebugCone(World, Origin, ActiveSound.Transform.GetUnitAxis(EAxis::X), ShapeDetails.Extents.X + ShapeDetails.ConeOffset, Angle, Angle, 10, FColor(155, 155, 255));
								}
								break;
							}

							default:
								check(false);
							}
						}
					}
				}
			}
		}

		for (TMapSounds::TConstIterator It(SoundInfos); It; ++It)
		{
			delete It.Value();
		}
	}
#endif
	return Y;
}

// AI
int32 UEngine::RenderStatAI(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	// Pick a larger font on console.
	UFont* Font = FPlatformProperties::SupportsWindowedMode() ? GetSmallFont() : GetMediumFont();

	// gather numbers
	int32 NumAI = 0;
	int32 NumAIRendered = 0;
	for (FConstControllerIterator Iterator = World->GetControllerIterator(); Iterator; ++Iterator)
	{
		AController* Controller = *Iterator;
		if (!Cast<APlayerController>(Controller))
		{
			++NumAI;
			if (Controller->GetPawn() != NULL && World->GetTimeSeconds() - Controller->GetPawn()->GetLastRenderTime() < 0.08f)
			{
				++NumAIRendered;
			}
		}
	}


#define MAXDUDES 20
#define BADAMTOFDUDES 12
	FColor TotalColor = FColor(0, 255, 0);
	if (NumAI > BADAMTOFDUDES)
	{
		float Scalar = 1.0f - FMath::Clamp<float>((float)NumAI / (float)MAXDUDES, 0.f, 1.f);

		TotalColor = FColor::MakeRedToGreenColorFromScalar(Scalar);
	}

	FColor RenderedColor = FColor(0, 255, 0);
	if (NumAIRendered > BADAMTOFDUDES)
	{
		float Scalar = 1.0f - FMath::Clamp<float>((float)NumAIRendered / (float)MAXDUDES, 0.f, 1.f);

		RenderedColor = FColor::MakeRedToGreenColorFromScalar(Scalar);

	}

	const int32 RowHeight = FMath::TruncToInt(Font->GetMaxCharHeight() * 1.1f);
	Canvas->DrawShadowedString(
		X,
		Y,
		*FString::Printf(TEXT("%i AI"), NumAI),
		Font,
		TotalColor
		);
	Y += RowHeight;

	Canvas->DrawShadowedString(
		X,
		Y,
		*FString::Printf(TEXT("%i AI Rendered"), NumAIRendered),
		Font,
		RenderedColor
		);
	Y += RowHeight;
	return Y;
}

// SLATEBATCHES
#if STATS
int32 UEngine::RenderStatSlateBatches(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	/* @todo Slate Rendering
	UFont* Font = SmallFont;
	
	const TArray<FBatchStats>& Stats = FSlateApplication::Get().GetRenderer()->GetBatchStats();
	
	// Start drawing the various counters.
	const int32 RowHeight = FMath::Trunc( Font->GetMaxCharHeight() * 1.1f );
	
	X = Viewport->GetSizeXY().X - 350;
	
	Canvas->DrawShadowedString(
		X,
		Y,
		TEXT("Slate Batches:"),
		Font, 
		FColor(0,255,0) );
	
	Y+=RowHeight;
	
	
	for( int32 I = 0; I < Stats.Num(); ++I )
	{
		const FBatchStats& Stat = Stats(I);
	
		// Draw a box representing the debug color of the batch
		DrawTriangle2D(Canvas, FVector2D(X,Y), FVector2D(0,0), FVector2D(X+10,Y), FVector2D(0,0), FVector2D(X+10,Y+7), FVector2D(0,0), Stat.BatchColor );
		DrawTriangle2D(Canvas, FVector2D(X,Y), FVector2D(0,0), FVector2D(X,Y+7), FVector2D(0,0), FVector2D(X+10,Y+7), FVector2D(0,0), Stat.BatchColor );
	
		Canvas->DrawShadowedString(
			X+15,
			Y,
			*FString::Printf(TEXT("Layer: %d, Elements: %d, Vertices: %d"), Stat.Layer, Stat.NumElementsInBatch, Stat.NumVertices ), 
			Font,
			FColor(0,255,0) );
		Y += RowHeight;
	}*/
	return Y;
}
#endif

#undef LOCTEXT_NAMESPACE
