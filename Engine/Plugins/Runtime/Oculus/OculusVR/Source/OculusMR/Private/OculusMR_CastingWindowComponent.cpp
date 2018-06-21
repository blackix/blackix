#include "OculusMR_CastingWindowComponent.h"
#include "OculusMRPrivate.h"
#include "Engine/Engine.h"
#include "Engine/CastingViewportClient.h"
#include "OculusMR_CastingCameraActor.h"
#include "Widgets/SWindow.h"
#include "Widgets/SViewport.h"
#include "Slate/SGameLayerManager.h"
#include "Slate/SceneViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "CanvasTypes.h"
#include "UnrealEngine.h"

UOculusMR_CastingWindowComponent::UOculusMR_CastingWindowComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CastingViewportClient(NULL)
	, CompositionViewportClient(NULL)
	, WorldContext(NULL)
{
#if OCULUS_MR_SUPPORTED_PLATFORMS
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
	bAutoActivate = true;
#endif
}

void UOculusMR_CastingWindowComponent::BeginPlay()
{
	Super::BeginPlay();
	//OpenCastingWindow();
}

void UOculusMR_CastingWindowComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasCastingWindowOpened())
	{
		CloseCastingWindow();
	}
	Super::EndPlay(EndPlayReason);
}

void UOculusMR_CastingWindowComponent::OpenCastingWindow(ECastingViewportCompositionMethod CompositionMethod, int WidthPerView, int HeightPerView)
{
	AOculusMR_CastingCameraActor* CastingCameraActor = dynamic_cast<AOculusMR_CastingCameraActor*>(GetOwner());
	if (!CastingCameraActor)
	{
		UE_LOG(LogMR, Log, TEXT("UOculusMR_OutputWindowComponent - Invalid Camera Actor"));
		return;
	}

	if (HasCastingWindowOpened())
	{
		UE_LOG(LogMR, Log, TEXT("Casting window exists"));
		return;
	}

	UGameInstance* GameInstance = GEngine->GameViewport->GetGameInstance();
	WorldContext = GameInstance->GetWorldContext();

	uint32 WindowWidth = WidthPerView;
	uint32 WindowHeight = HeightPerView;

	switch (CompositionMethod)
	{
	case ECastingViewportCompositionMethod::MultiView:
		WindowWidth = WidthPerView * 2;
		WindowHeight = HeightPerView;
		break;
	case ECastingViewportCompositionMethod::DirectComposition:
		WindowWidth = WidthPerView;
		WindowHeight = HeightPerView;
		break;
	default:
		checkNoEntry();
		break;
	}

	OutputWindow = SNew(SWindow)
		.Type(EWindowType::CastingWindow)
		.AutoCenter(EAutoCenter::None)
		.ScreenPosition(FVector2D(10, 40))
		.Title(FText::FromString(TEXT("Oculus MR Output")))
		.ClientSize(FVector2D(WindowWidth, WindowHeight))
		.CreateTitleBar(true)
		.SizingRule(ESizingRule::FixedSize)
		.UseOSWindowBorder(true)
		.SupportsMaximize(false)
		.SupportsMinimize(true)
		.HasCloseButton(true)
		.IsTopmostWindow(false)
		.IsInitiallyMinimized(CastingCameraActor->bProjectToMirrorWindow);

	FSlateApplication::Get().AddWindow(OutputWindow.ToSharedRef());

	TSharedRef<SOverlay> ViewportOverlayWidgetRef = SNew(SOverlay);
	ViewportOverlayWidgetRef->SetCursor(EMouseCursor::Default);

	CastingViewportClient = NewObject<UCastingViewportClient>(GEngine);
	ECastingViewportCompositionMethod CastingCompositionMethod = CompositionMethod;
	CastingViewportClient->Init(*WorldContext, GameInstance, CastingCameraActor, CastingCompositionMethod);

	WorldContext->CastingViewports.Add(CastingViewportClient);

	TSharedPtr<SViewport> CastingViewport = SNew(SViewport)
		.RenderDirectlyToWindow(false)
		.EnableGammaCorrection(false)
		.EnableStereoRendering(false)
		.Cursor(EMouseCursor::Default)
		[
			ViewportOverlayWidgetRef
		];

	CastingSceneViewport = MakeShared<FOculusMR_CastingSceneViewport>(CastingViewportClient, CastingViewport, 10);
	CastingViewport->SetViewportInterface(CastingSceneViewport.ToSharedRef());
	CastingViewportClient->SetViewportFrame(CastingSceneViewport.Get());
	CastingViewportClient->Viewport->SetInitialSize(FIntPoint(WindowWidth, WindowHeight));

	OutputWindow->SetContent(CastingViewport.ToSharedRef());
	OutputWindow->ShowWindow();

	if (CastingCameraActor->bProjectToMirrorWindow)
	{
		CastingViewportClient->bProjectToMirrorWindow = true;
	}

	OutputWindow->SetOnWindowClosed(FOnWindowClosed::CreateLambda([this](const TSharedRef<SWindow>& Window) {
		OutputWindow = nullptr;
		WorldContext->CastingViewports.Remove(CastingViewportClient);
		CastingViewportClient->OnEndDraw().Clear();
		CastingViewportClient = nullptr;
		CastingSceneViewport.Reset();
		if (CompositionViewportClient)
		{
			CompositionViewportClient->SetCastingSceneViewport(NULL);
			CompositionViewportClient->SetCameraColorTexture(NULL);
		}
		CompositionViewportClient = NULL;
		CompositionSceneViewport.Reset();
		WorldContext = nullptr;
		OnWindowClosedDelegate.ExecuteIfBound();
	}));
}

void UOculusMR_CastingWindowComponent::CloseCastingWindow()
{
	if (!HasCastingWindowOpened())
	{
		UE_LOG(LogMR, Log, TEXT("Casting window does not exist"));
		return;
	}

	if (OutputWindow.IsValid())
	{
		OutputWindow->RequestDestroyWindow();
		OutputWindow = nullptr;
	}
}

bool UOculusMR_CastingWindowComponent::HasCastingWindowOpened() const
{
	return OutputWindow.IsValid();
}

void UOculusMR_CastingWindowComponent::SetCameraColorTexture(UTexture2D* InCameraColorTexture)
{
	if (CompositionViewportClient)
	{
		CompositionViewportClient->SetCameraColorTexture(InCameraColorTexture);
	}
}

double UOculusMR_CastingWindowComponent::GetExpectedLantencyInSeconds() const
{
	return CastingSceneViewport->GetExpectedLantencyInSeconds();
}

void UOculusMR_CastingWindowComponent::SetExpectedLantencyInSeconds(double InLatency)
{
	CastingSceneViewport->SetExpectedLantencyInSeconds(InLatency);
}

void UOculusMR_CastingWindowComponent::DrawCompositionViewport()
{
	UWorld* ViewportWorld = CastingViewportClient->GetWorld();
	FCanvas Canvas(CompositionSceneViewport.Get(), NULL, ViewportWorld, ViewportWorld ? ViewportWorld->FeatureLevel : GMaxRHIFeatureLevel);
	FIntPoint SizeXY = CompositionSceneViewport->GetViewport()->GetSizeXY();
	if (SizeXY.X > 0 && SizeXY.Y > 0)
	{
		CompositionSceneViewport->EnqueueBeginRenderFrame(false);

		Canvas.SetRenderTargetRect(FIntRect(0, 0, SizeXY.X, SizeXY.Y));
		{
			// Make sure the Canvas is not rendered upside down
			Canvas.SetAllowSwitchVerticalAxis(false);
			CompositionViewportClient->Draw(CompositionSceneViewport.Get(), &Canvas);
		}
		Canvas.Flush_GameThread();

		//@todo UE4: If Slate controls this viewport, we should not present
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VSync"));
		bool bLockToVsync = CVar->GetValueOnGameThread() != 0;
		struct FCompositionEndDrawingCommandParams
		{
			FViewport* Viewport;
			uint32 bLockToVsync : 1;
			uint32 bShouldTriggerTimerEvent : 1;
			uint32 bShouldPresent : 1;
		};
		FCompositionEndDrawingCommandParams Params = { CompositionSceneViewport.Get(), (uint32)bLockToVsync, (uint32)GInputLatencyTimer.GameThreadTrigger, 1/*bShouldPresent*/ };
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			CompositionEndDrawingCommand,
			FCompositionEndDrawingCommandParams, Parameters, Params,
			{
				//ViewportEndDrawing(RHICmdList, Parameters);
				GInputLatencyTimer.RenderThreadTrigger = Parameters.bShouldTriggerTimerEvent;
				Parameters.Viewport->EndRenderFrame(RHICmdList, Parameters.bShouldPresent, Parameters.bLockToVsync);
			}
		);
	}
}