// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/CastingViewportClient.h"

// The WITH_OCULUS_PRIVATE_CODE tag is kept for reference
//#if WITH_OCULUS_PRIVATE_CODE

#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CoreDelegates.h"
#include "Misc/App.h"
#include "GameMapsSettings.h"
#include "EngineStats.h"
#include "RenderingThread.h"
#include "SceneView.h"
//#include "AI/Navigation/NavigationSystem.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "GameFramework/Volume.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "SceneManagement.h"
#include "Particles/ParticleSystemComponent.h"
#include "Engine/NetDriver.h"
#include "Camera/CastingCameraActor.h"
#include "Camera/CameraComponent.h"
#include "ContentStreaming.h"
#include "UnrealEngine.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Engine/Console.h"
#include "GameFramework/HUD.h"
#include "FXSystem.h"
#include "SubtitleManager.h"
#include "ImageUtils.h"
#include "SceneViewExtension.h"
#include "IHeadMountedDisplay.h"
#include "EngineModule.h"
#include "Sound/SoundWave.h"
#include "HighResScreenshot.h"
#include "BufferVisualizationData.h"
#include "GameFramework/InputSettings.h"
#include "Components/LineBatchComponent.h"
#include "Debug/DebugDrawService.h"
#include "Components/BrushComponent.h"
#include "Engine/GameEngine.h"
#include "Logging/MessageLog.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/UserInterfaceSettings.h"
#include "Slate/SceneViewport.h"
#include "Slate/SGameLayerManager.h"
#include "ActorEditorUtils.h"
#include "ComponentRecreateRenderStateContext.h"
#include "LegacyScreenPercentageDriver.h"

#define LOCTEXT_NAMESPACE "CastingViewport"

/** This variable allows forcing full screen of the first player controller viewport, even if there are multiple controllers plugged in and no cinematic playing. */
extern ENGINE_API bool GForceFullscreen;

/** Whether to visualize the lightmap selected by the Debug Camera. */
extern ENGINE_API bool GShowDebugSelectedLightmap;
/** The currently selected component in the actor. */
extern ENGINE_API UPrimitiveComponent* GDebugSelectedComponent;
/** The lightmap used by the currently selected component, if it's a static mesh component. */
extern ENGINE_API class FLightMap2D* GDebugSelectedLightmap;

/** Delegate called at the end of the frame when a screenshot is captured */
FOnScreenshotCaptured UCastingViewportClient::ScreenshotCapturedDelegate;

/** Delegate called when the game viewport is created. */
FSimpleMulticastDelegate UCastingViewportClient::CreatedDelegate;

UCastingViewportClient::UCastingViewportClient(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , ViewportFrame(NULL)
    , Viewport(NULL)
    , FullLayerViewState()
    , BackgroundLayerViewState()
    , ForegroundLayerViewState()
    , EngineShowFlags(ESFIM_Game)
	, CurrentBufferVisualizationMode(NAME_None)
    , CompositionMethod(ECastingViewportCompositionMethod::MultiView)
    , bProjectToMirrorWindow(false)
{
    FullLayerViewState.Allocate();
    BackgroundLayerViewState.Allocate();
    ForegroundLayerViewState.Allocate();

	ViewModeIndex = VMI_Lit;
}

UCastingViewportClient::UCastingViewportClient(FVTableHelper& Helper)
	: Super(Helper)
    , ViewportFrame(NULL)
    , Viewport(NULL)
    , EngineShowFlags(ESFIM_Game)
	, CurrentBufferVisualizationMode(NAME_None)
    , CompositionMethod(ECastingViewportCompositionMethod::MultiView)
{
}

UCastingViewportClient::~UCastingViewportClient()
{
}


void UCastingViewportClient::PostInitProperties()
{
	Super::PostInitProperties();
	EngineShowFlags = FEngineShowFlags(ESFIM_Game);
	EngineShowFlags.PostProcessing = false;
}

void UCastingViewportClient::BeginDestroy()
{
	RemoveAllViewportWidgets();
	Super::BeginDestroy();
}


void UCastingViewportClient::DetachViewportClient()
{
	//ViewportConsole = NULL;
	RemoveAllViewportWidgets();
	RemoveFromRoot();
}

FSceneViewport* UCastingViewportClient::GetCastingViewport()
{
	return static_cast<FSceneViewport*>(Viewport);
}

TSharedPtr<class SViewport> UCastingViewportClient::GetCastingViewportWidget()
{
	FSceneViewport* SceneViewport = GetCastingViewport();
	if (SceneViewport != nullptr)
	{
		TWeakPtr<SViewport> WeakViewportWidget = SceneViewport->GetViewportWidget();
		TSharedPtr<SViewport> ViewportWidget = WeakViewportWidget.Pin();
		return ViewportWidget;
	}
	return nullptr;
}

void UCastingViewportClient::Tick( float DeltaTime )
{
	TickDelegate.Broadcast(DeltaTime);
}

void UCastingViewportClient::Init(struct FWorldContext& WorldContext, UGameInstance* OwningGameInstance, ACastingCameraActor* InCastingCameraActor, ECastingViewportCompositionMethod InCompositionMethod)
{
	// set reference to world context
	WorldContext.AddRef(World);

	// remember our game instance
	GameInstance = OwningGameInstance;

    // remember the casting camera actor
    CastingCameraActor = InCastingCameraActor;

    CompositionMethod = InCompositionMethod;
}

FSceneInterface* UCastingViewportClient::GetScene() const
{
    UWorld* MyWorld = GetWorld();
    if (MyWorld)
    {
        return MyWorld->Scene;
    }

    return NULL;
}

UWorld* UCastingViewportClient::GetWorld() const
{
	return World;
}

UGameInstance* UCastingViewportClient::GetGameInstance() const
{
	return GameInstance;
}

ACastingCameraActor* UCastingViewportClient::GetCastingCameraActor() const
{
    return CastingCameraActor;
}

bool UCastingViewportClient::GetMousePosition(FVector2D& MousePosition) const
{
	bool bGotMousePosition = false;

	if (Viewport && FSlateApplication::Get().IsMouseAttached())
	{
		FIntPoint MousePos;
		Viewport->GetMousePos(MousePos);
		if (MousePos.X >= 0 && MousePos.Y >= 0)
		{
			MousePosition = FVector2D(MousePos);
			bGotMousePosition = true;
		}
	}

	return bGotMousePosition;
}

FVector2D UCastingViewportClient::GetMousePosition() const
{
	FVector2D MousePosition;
	if (!GetMousePosition(MousePosition))
	{
		MousePosition = FVector2D::ZeroVector;
	}

	return MousePosition;
}


bool UCastingViewportClient::RequiresUncapturedAxisInput() const
{
	return false;
}


void UCastingViewportClient::SetViewportFrame( FViewportFrame* InViewportFrame )
{
	ViewportFrame = InViewportFrame;
	SetViewport( ViewportFrame ? ViewportFrame->GetViewport() : NULL );
}


void UCastingViewportClient::SetViewport( FViewport* InViewport )
{
	FViewport* PreviousViewport = Viewport;
	Viewport = InViewport;
}

void UCastingViewportClient::GetViewportSize( FVector2D& out_ViewportSize ) const
{
	if ( Viewport != NULL )
	{
		out_ViewportSize.X = Viewport->GetSizeXY().X;
		out_ViewportSize.Y = Viewport->GetSizeXY().Y;
	}
}

bool UCastingViewportClient::IsFullScreenViewport() const
{
	return Viewport->IsFullscreen();
}

bool UCastingViewportClient::ShouldForceFullscreenViewport() const
{
    return false;
}

/** Util to find named canvas in transient package, and create if not found */
static UCanvas* GetCanvasByName(FName CanvasName)
{
	// Cache to avoid FString/FName conversions/compares
	static TMap<FName, UCanvas*> CanvasMap;
	UCanvas** FoundCanvas = CanvasMap.Find(CanvasName);
	if (!FoundCanvas)
	{
		UCanvas* CanvasObject = FindObject<UCanvas>(GetTransientPackage(),*CanvasName.ToString());
		if( !CanvasObject )
		{
			CanvasObject = NewObject<UCanvas>(GetTransientPackage(), CanvasName);
			CanvasObject->AddToRoot();
		}

		CanvasMap.Add(CanvasName, CanvasObject);
		return CanvasObject;
	}

	return *FoundCanvas;
}

//////////////////////////////////////////////////////////////////////////
//
// Configures the specified FSceneView object with the view and projection matrices for this viewport.

void UCastingViewportClient::CalcAndAddSceneView(FSceneViewFamily* ViewFamily, ECastingLayer CastingLayer, uint8 RowIndex, uint8 ColumnIndex, uint8 TotalRows, uint8 TotalColumns, FName BufferVisualizationMode)
{
    FSceneViewInitOptions ViewInitOptions;

    FTransform CastingCameraActorTransform = CastingCameraActor->GetActorTransform();
    ViewInitOptions.ViewOrigin = CastingCameraActorTransform.GetLocation();
    FRotator ViewRotation = FRotator(CastingCameraActorTransform.GetRotation());

    const FIntPoint ViewportSizeXY = Viewport->GetSizeXY();

    int32 CellWidth = ViewportSizeXY.X / TotalColumns;
    int32 CellHeight = ViewportSizeXY.Y / TotalRows;

    FIntRect ViewRect = FIntRect(ColumnIndex * CellWidth, RowIndex * CellHeight, (ColumnIndex + 1) * CellWidth, (RowIndex + 1) * CellHeight);
    ViewInitOptions.SetViewRectangle(ViewRect);

    // no matter how we are drawn (forced or otherwise), reset our time here
    //TimeForForceRedraw = 0.0;

    const bool bConstrainAspectRatio = true;
    const EAspectRatioAxisConstraint AspectRatioAxisConstraint = EAspectRatioAxisConstraint::AspectRatio_MajorAxisFOV;// GetDefault<ULevelEditorViewportSettings>()->AspectRatioAxisConstraint;
    float AspectRatio = CastingCameraActor->GetCameraComponent()->AspectRatio;

    AWorldSettings* WorldSettings = nullptr;
    if (GetScene() != nullptr && GetScene()->GetWorld() != nullptr)
    {
        WorldSettings = GetScene()->GetWorld()->GetWorldSettings();
    }
    if (WorldSettings != nullptr)
    {
        ViewInitOptions.WorldToMetersScale = WorldSettings->WorldToMeters;
    }

    // Create the view matrix
    ViewInitOptions.ViewRotationMatrix = FInverseRotationMatrix(ViewRotation);

    // Rotate view 90 degrees
    ViewInitOptions.ViewRotationMatrix = ViewInitOptions.ViewRotationMatrix * FMatrix(
        FPlane(0, 0, 1, 0),
        FPlane(1, 0, 0, 0),
        FPlane(0, 1, 0, 0),
        FPlane(0, 0, 0, 1));

    float MinZ = GNearClippingPlane;
    float MaxZ = MinZ;

	// Adjusting the MinZ/MaxZ could cause the lighting artifact. Use the backdrop mesh to replace it
    //if (CompositionMethod == ECastingViewportCompositionMethod::MultiView)
    //{
    //    if (CastingLayer == ECastingLayer::Background)
    //    {
    //        MinZ = FMath::Max(GNearClippingPlane, CastingCameraActor->GetClippingPlaneDistance() - CastingCameraActor->GetClippingPlaneDistanceTolerance());
    //        MaxZ = MinZ;
    //    }
    //    else if (CastingLayer == ECastingLayer::Foreground)
    //    {
    //        MaxZ = CastingCameraActor->GetClippingPlaneDistance();
    //    }
    //}

    // Avoid zero ViewFOV's which cause divide by zero's in projection matrix
    const float ViewFOV = CastingCameraActor->GetCameraComponent()->FieldOfView;
    const float MatrixFOV = FMath::Max(0.001f, ViewFOV) * (float)PI / 360.0f;

    if ((bool)ERHIZBuffer::IsInverted)
    {
        ViewInitOptions.ProjectionMatrix = FReversedZPerspectiveMatrix(
            MatrixFOV,
            MatrixFOV,
            1.0f,
            AspectRatio,
            MinZ,
            MaxZ
        );
    }
    else
    {
        ViewInitOptions.ProjectionMatrix = FPerspectiveMatrix(
            MatrixFOV,
            MatrixFOV,
            1.0f,
            AspectRatio,
            MinZ,
            MaxZ
        );
    }
    if (bConstrainAspectRatio)
    {
        ViewInitOptions.SetConstrainedViewRectangle(Viewport->CalculateViewExtents(AspectRatio, ViewRect));
    }

    ViewInitOptions.ViewFamily = ViewFamily;
    if (CastingLayer == ECastingLayer::Full)
    {
        ViewInitOptions.SceneViewStateInterface = FullLayerViewState.GetReference();
    }
    else if (CastingLayer == ECastingLayer::Background)
    {
        ViewInitOptions.SceneViewStateInterface = BackgroundLayerViewState.GetReference();
    }
    else if (CastingLayer == ECastingLayer::Foreground)
    {
        ViewInitOptions.SceneViewStateInterface = ForegroundLayerViewState.GetReference();
    }
#if WITH_OCULUS_PRIVATE_CODE
    ViewInitOptions.CastingLayer = CastingLayer;
#endif

    ViewInitOptions.StereoPass = eSSP_FULL;

    ViewInitOptions.ViewElementDrawer = NULL;

    ViewInitOptions.BackgroundColor = CastingCameraActor->GetForegroundLayerBackgroundColor();

#if WITH_EDITOR
    // for ortho views to steal perspective view origin
    ViewInitOptions.OverrideLODViewOrigin = FVector::ZeroVector;
    ViewInitOptions.bUseFauxOrthoViewPos = true;
#endif

    FSceneView* View = new FSceneView(ViewInitOptions);

    View->ViewLocation = CastingCameraActorTransform.GetLocation();
    View->ViewRotation = ViewRotation;

#if WITH_EDITOR
    View->SubduedSelectionOutlineColor = GEngine->GetSubduedSelectionOutlineColor();
#endif

    ViewFamily->Views.Add(View);

    //View->StartFinalPostprocessSettings(View->ViewLocation);

    //View->EndFinalPostprocessSettings(ViewInitOptions);

    if (/*CastingViewportConfiguration != ECastingViewportConfiguration::Composition && */ View->FinalPostProcessSettings.AutoExposureMethod == EAutoExposureMethod::AEM_Histogram)
    {
        // use the Basic exposure when we render foreground and background in individual passes
        View->FinalPostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Basic;
    }

    for (int ViewExt = 0; ViewExt < ViewFamily->ViewExtensions.Num(); ViewExt++)
    {
        ViewFamily->ViewExtensions[ViewExt]->SetupView(*ViewFamily, *View);
    }

    if (ViewFamily->EngineShowFlags.Wireframe)
    {
        // Wireframe color is emissive-only, and mesh-modifying materials do not use material substitution, hence...
        View->DiffuseOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
        View->SpecularOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
    }
    else if (ViewFamily->EngineShowFlags.OverrideDiffuseAndSpecular)
    {
        View->DiffuseOverrideParameter = FVector4(GEngine->LightingOnlyBrightness.R, GEngine->LightingOnlyBrightness.G, GEngine->LightingOnlyBrightness.B, 0.0f);
        View->SpecularOverrideParameter = FVector4(.1f, .1f, .1f, 0.0f);
    }
    else if (ViewFamily->EngineShowFlags.ReflectionOverride)
    {
        View->DiffuseOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
        View->SpecularOverrideParameter = FVector4(1, 1, 1, 0.0f);
        View->NormalOverrideParameter = FVector4(0, 0, 1, 0.0f);
        View->RoughnessOverrideParameter = FVector2D(0.0f, 0.0f);
    }

    if (!ViewFamily->EngineShowFlags.Diffuse)
    {
        View->DiffuseOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
    }

    if (!ViewFamily->EngineShowFlags.Specular)
    {
        View->SpecularOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
    }

    View->CurrentBufferVisualizationMode = BufferVisualizationMode;

    View->CameraConstrainedViewRect = View->UnscaledViewRect;
}

void UCastingViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{
	//Valid SceneCanvas is required.  Make this explicit.
	check(SceneCanvas);

	BeginDrawDelegate.Broadcast();

    const bool bStereoRendering = false;// GEngine->IsStereoscopic3D(InViewport);

	// Create a temporary canvas if there isn't already one.
	static FName CanvasObjectName(TEXT("CanvasObject"));
	UCanvas* CanvasObject = GetCanvasByName(CanvasObjectName);
	CanvasObject->Canvas = SceneCanvas;

	if (SceneCanvas)
	{
		SceneCanvas->SetScaledToRenderTarget(bStereoRendering);
		SceneCanvas->SetStereoRendering(bStereoRendering);
	}

	bool bUIDisableWorldRendering = false;

	UWorld* MyWorld = GetWorld();

	// create the view family for rendering the world scene to the viewport's render target
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		InViewport,
		MyWorld->Scene,
		EngineShowFlags)
		.SetRealtimeUpdate(true)
#if WITH_OCULUS_PRIVATE_CODE
        .SetIsCasting(true)
#endif
	);

	ViewFamily.ViewExtensions = GEngine->ViewExtensions->GatherActiveExtensions(InViewport);

	for (auto ViewExt : ViewFamily.ViewExtensions)
	{
		ViewExt->SetupViewFamily(ViewFamily);
	}

	//if (bStereoRendering && GEngine->HMDDevice.IsValid())
	//{
	//	// Allow HMD to modify screen settings
	//	GEngine->HMDDevice->UpdateScreenSettings(Viewport);
	//}

	EngineShowFlagOverride(ESFIM_Game, (EViewModeIndex)ViewModeIndex, ViewFamily.EngineShowFlags, NAME_None);

	if (ViewFamily.EngineShowFlags.VisualizeBuffer && AllowDebugViewmodes())
	{
		// Process the buffer visualization console command
		FName NewBufferVisualizationMode = NAME_None;
		static IConsoleVariable* ICVar = IConsoleManager::Get().FindConsoleVariable(FBufferVisualizationData::GetVisualizationTargetConsoleCommandName());
		if (ICVar)
		{
			static const FName OverviewName = TEXT("Overview");
			FString ModeNameString = ICVar->GetString();
			FName ModeName = *ModeNameString;
			if (ModeNameString.IsEmpty() || ModeName == OverviewName || ModeName == NAME_None)
			{
				NewBufferVisualizationMode = NAME_None;
			}
			else
			{
				if (GetBufferVisualizationData().GetMaterial(ModeName) == NULL)
				{
					// Mode is out of range, so display a message to the user, and reset the mode back to the previous valid one
					UE_LOG(LogConsoleResponse, Warning, TEXT("Buffer visualization mode '%s' does not exist"), *ModeNameString);
					NewBufferVisualizationMode = CurrentBufferVisualizationMode;
					// todo: cvars are user settings, here the cvar state is used to avoid log spam and to auto correct for the user (likely not what the user wants)
					ICVar->Set(*NewBufferVisualizationMode.GetPlainNameString(), ECVF_SetByCode);
				}
				else
				{
					NewBufferVisualizationMode = ModeName;
				}
			}
		}

		if (NewBufferVisualizationMode != CurrentBufferVisualizationMode)
		{
			CurrentBufferVisualizationMode = NewBufferVisualizationMode;
		}
	}

    if (CompositionMethod == ECastingViewportCompositionMethod::MultiView)
    {
        CalcAndAddSceneView(&ViewFamily, ECastingLayer::Foreground, 0, 0, 1, 2, CurrentBufferVisualizationMode);
        CalcAndAddSceneView(&ViewFamily, ECastingLayer::Background, 0, 1, 1, 2, CurrentBufferVisualizationMode);
    }
    else if (CompositionMethod == ECastingViewportCompositionMethod::DirectComposition)
    {
        CalcAndAddSceneView(&ViewFamily, ECastingLayer::Full, 0, 0, 1, 1, CurrentBufferVisualizationMode);
    }
    else
    {
        checkNoEntry();
    }

	// Find largest rectangle bounded by all rendered views.
	uint32 MinX=InViewport->GetSizeXY().X, MinY=InViewport->GetSizeXY().Y, MaxX=0, MaxY=0;
	uint32 TotalArea = 0;
	{
		for( int32 ViewIndex = 0; ViewIndex < ViewFamily.Views.Num(); ++ViewIndex )
		{
			const FSceneView* View = ViewFamily.Views[ViewIndex];

			FIntRect UpscaledViewRect = View->UnscaledViewRect;

			MinX = FMath::Min<uint32>(UpscaledViewRect.Min.X, MinX);
			MinY = FMath::Min<uint32>(UpscaledViewRect.Min.Y, MinY);
			MaxX = FMath::Max<uint32>(UpscaledViewRect.Max.X, MaxX);
			MaxY = FMath::Max<uint32>(UpscaledViewRect.Max.Y, MaxY);
			TotalArea += FMath::TruncToInt(UpscaledViewRect.Width()) * FMath::TruncToInt(UpscaledViewRect.Height());
		}

		// To draw black borders around the rendered image (prevents artifacts from post processing passes that read outside of the image e.g. PostProcessAA)
		{
			int32 BlackBorders = 0; // not implemented. refer to CVarSetBlackBordersEnabled in GameViewportClient.cpp

			if(ViewFamily.Views.Num() == 1 && BlackBorders)
			{
				MinX += BlackBorders;
				MinY += BlackBorders;
				MaxX -= BlackBorders;
				MaxY -= BlackBorders;
				TotalArea = (MaxX - MinX) * (MaxY - MinY);
			}
		}
	}

	// If the views don't cover the entire bounding rectangle, clear the entire buffer.
	bool bBufferCleared = false;
	if ( ViewFamily.Views.Num() == 0 || TotalArea != (MaxX-MinX)*(MaxY-MinY) || bDisableWorldRendering )
	{
		SceneCanvas->DrawTile(0,0,InViewport->GetSizeXY().X,InViewport->GetSizeXY().Y,0.0f,0.0f,1.0f,1.f,FLinearColor::Black,NULL,false);
		bBufferCleared = true;
	}

	// If not doing VR rendering, apply DPI derived resolution fraction even if show flag is disabled
	if (!bStereoRendering)
	{
		ViewFamily.SecondaryViewFraction = GetDPIDerivedResolutionFraction();
	}

	// If a screen percentage interface was not set by one of the view extension, then set the legacy one.
	if (ViewFamily.GetScreenPercentageInterface() == nullptr)
	{
		float GlobalResolutionFraction = 1.0f;

		ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(
			ViewFamily, GlobalResolutionFraction, /* AllowPostProcessSettingsScreenPercentage = */ false));
	}

	// Draw the player views.
	if (!bDisableWorldRendering && !bUIDisableWorldRendering /*&& PlayerViewMap.Num() > 0*/) //-V560
	{
		GetRendererModule().BeginRenderingViewFamily(SceneCanvas,&ViewFamily);
	}
	else
	{
		// Make sure RHI resources get flushed if we're not using a renderer
		ENQUEUE_UNIQUE_RENDER_COMMAND( UCastingViewportClient_FlushRHIResources,
		{
			FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
		});
	}

	// Clear areas of the rendertarget (backbuffer) that aren't drawn over by the views.
	if (!bBufferCleared)
	{
		// clear left
		if( MinX > 0 )
		{
			SceneCanvas->DrawTile(0,0,MinX,InViewport->GetSizeXY().Y,0.0f,0.0f,1.0f,1.f,FLinearColor::Black,NULL,false);
		}
		// clear right
		if( MaxX < (uint32)InViewport->GetSizeXY().X )
		{
			SceneCanvas->DrawTile(MaxX,0,InViewport->GetSizeXY().X,InViewport->GetSizeXY().Y,0.0f,0.0f,1.0f,1.f,FLinearColor::Black,NULL,false);
		}
		// clear top
		if( MinY > 0 )
		{
			SceneCanvas->DrawTile(MinX,0,MaxX,MinY,0.0f,0.0f,1.0f,1.f,FLinearColor::Black,NULL,false);
		}
		// clear bottom
		if( MaxY < (uint32)InViewport->GetSizeXY().Y )
		{
			SceneCanvas->DrawTile(MinX,MaxY,MaxX,InViewport->GetSizeXY().Y,0.0f,0.0f,1.0f,1.f,FLinearColor::Black,NULL,false);
		}
	}

	EndDrawDelegate.Broadcast();
}

void UCastingViewportClient::ProcessScreenShots(FViewport* InViewport)
{
	if (GIsDumpingMovie || FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
	{
		TArray<FColor> Bitmap;

		bool bShowUI = false;
		TSharedPtr<SWindow> WindowPtr = GetWindow();
		if (!GIsDumpingMovie && (FScreenshotRequest::ShouldShowUI() && WindowPtr.IsValid()))
		{
			bShowUI = true;
		}

		bool bScreenshotSuccessful = false;
		FIntVector Size(InViewport->GetSizeXY().X, InViewport->GetSizeXY().Y, 0);
		if( bShowUI && FSlateApplication::IsInitialized() )
		{
			TSharedRef<SWidget> WindowRef = WindowPtr.ToSharedRef();
			bScreenshotSuccessful = FSlateApplication::Get().TakeScreenshot( WindowRef, Bitmap, Size);
			GScreenshotResolutionX = Size.X;
			GScreenshotResolutionY = Size.Y;
		}
		else
		{
			bScreenshotSuccessful = GetViewportScreenShot(InViewport, Bitmap);
		}

		if (bScreenshotSuccessful)
		{
			if (ScreenshotCapturedDelegate.IsBound())
			{
				// Ensure that all pixels' alpha is set to 255
				for (auto& Color : Bitmap)
				{
					Color.A = 255;
				}

				// If delegate subscribed, fire it instead of writing out a file to disk
				ScreenshotCapturedDelegate.Broadcast(Size.X, Size.Y, Bitmap);
			}
			else
			{
				FString ScreenShotName = FScreenshotRequest::GetFilename();
				if (GIsDumpingMovie && ScreenShotName.IsEmpty())
				{
					// Request a new screenshot with a formatted name
					bShowUI = false;
					const bool bAddFilenameSuffix = true;
					FScreenshotRequest::RequestScreenshot(FString(), bShowUI, bAddFilenameSuffix);
					ScreenShotName = FScreenshotRequest::GetFilename();
				}

				GetHighResScreenshotConfig().MergeMaskIntoAlpha(Bitmap);

				FIntRect SourceRect(0, 0, GScreenshotResolutionX, GScreenshotResolutionY);
				if (GIsHighResScreenshot)
				{
					SourceRect = GetHighResScreenshotConfig().CaptureRegion;
				}

				if (!FPaths::GetExtension(ScreenShotName).IsEmpty())
				{
					ScreenShotName = FPaths::GetBaseFilename(ScreenShotName, false);
					ScreenShotName += TEXT(".png");
				}

				// Save the contents of the array to a png file.
				TArray<uint8> CompressedBitmap;
				FImageUtils::CompressImageArray(Size.X, Size.Y, Bitmap, CompressedBitmap);
				FFileHelper::SaveArrayToFile(CompressedBitmap, *ScreenShotName);
			}
		}

		FScreenshotRequest::Reset();
		// Reeanble screen messages - if we are NOT capturing a movie
		GAreScreenMessagesEnabled = GScreenMessagesRestoreState;
	}
}

TOptional<bool> UCastingViewportClient::QueryShowFocus(const EFocusCause InFocusCause) const
{
	UUserInterfaceSettings* UISettings = GetMutableDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass());

	if ( UISettings->RenderFocusRule == ERenderFocusRule::Never ||
		(UISettings->RenderFocusRule == ERenderFocusRule::NonPointer && InFocusCause == EFocusCause::Mouse) ||
		(UISettings->RenderFocusRule == ERenderFocusRule::NavigationOnly && InFocusCause != EFocusCause::Navigation))
	{
		return false;
	}

	return true;
}

void UCastingViewportClient::LostFocus(FViewport* InViewport)
{
}

void UCastingViewportClient::ReceivedFocus(FViewport* InViewport)
{
}

bool UCastingViewportClient::IsFocused(FViewport* InViewport)
{
	return InViewport->HasFocus() || InViewport->HasMouseCapture();
}

void UCastingViewportClient::Activated(FViewport* InViewport, const FWindowActivateEvent& InActivateEvent)
{
	ReceivedFocus(InViewport);
}

void UCastingViewportClient::Deactivated(FViewport* InViewport, const FWindowActivateEvent& InActivateEvent)
{
	LostFocus(InViewport);
}

bool UCastingViewportClient::WindowCloseRequested()
{
	return !WindowCloseRequestedDelegate.IsBound() || WindowCloseRequestedDelegate.Execute();
}

void UCastingViewportClient::CloseRequested(FViewport* InViewport)
{
	check(InViewport == Viewport);

#if PLATFORM_DESKTOP
	FSlateApplication::Get().SetGameIsFakingTouchEvents(false);
#endif

	// broadcast close request to anyone that registered an interest
	CloseRequestedDelegate.Broadcast(InViewport);

	SetViewportFrame(NULL);
}

bool UCastingViewportClient::IsOrtho() const
{
	return false;
}

void UCastingViewportClient::PostRender(UCanvas* Canvas)
{
}

void UCastingViewportClient::AddViewportWidgetContent( TSharedRef<SWidget> ViewportContent, const int32 ZOrder )
{
	TSharedPtr< SOverlay > PinnedViewportOverlayWidget( ViewportOverlayWidget.Pin() );
	if( ensure( PinnedViewportOverlayWidget.IsValid() ) )
	{
		// NOTE: Returns FSimpleSlot but we're ignoring here.  Could be used for alignment though.
		PinnedViewportOverlayWidget->AddSlot( ZOrder )
			[
				ViewportContent
			];
	}
}

void UCastingViewportClient::RemoveViewportWidgetContent( TSharedRef<SWidget> ViewportContent )
{
	TSharedPtr< SOverlay > PinnedViewportOverlayWidget( ViewportOverlayWidget.Pin() );
	if( PinnedViewportOverlayWidget.IsValid() )
	{
		PinnedViewportOverlayWidget->RemoveSlot( ViewportContent );
	}
}

void UCastingViewportClient::RemoveAllViewportWidgets()
{
	TSharedPtr< SOverlay > PinnedViewportOverlayWidget( ViewportOverlayWidget.Pin() );
	if( PinnedViewportOverlayWidget.IsValid() )
	{
		PinnedViewportOverlayWidget->ClearChildren();
	}
}

FPopupMethodReply UCastingViewportClient::OnQueryPopupMethod() const
{
	return FPopupMethodReply::UseMethod(EPopupMethod::UseCurrentWindow)
		.SetShouldThrottle(EShouldThrottle::No);
}

bool UCastingViewportClient::SetDisplayConfiguration(const FIntPoint* Dimensions, EWindowMode::Type WindowMode)
{
	if (Viewport == NULL || ViewportFrame == NULL)
	{
		return true;
	}

	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);

	if (GameEngine)
	{
		UGameUserSettings* UserSettings = GameEngine->GetGameUserSettings();

		UserSettings->SetFullscreenMode(WindowMode);

		if (Dimensions)
		{
			UserSettings->SetScreenResolution(*Dimensions);
		}

		UserSettings->ApplySettings(false);
	}
	else
	{
		int32 NewX = GSystemResolution.ResX;
		int32 NewY = GSystemResolution.ResY;

		if (Dimensions)
		{
			NewX = Dimensions->X;
			NewY = Dimensions->Y;
		}

		FSystemResolution::RequestResolutionChange(NewX, NewY, WindowMode);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

//#endif // WITH_OCULUS_PRIVATE_CODE