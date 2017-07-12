#include "OculusMR_CompositionViewportClient.h"
#include "OculusMRPrivate.h"
#include "CanvasTypes.h"
#include "Engine/Texture2D.h"

UOculusMR_CompositionViewportClient::UOculusMR_CompositionViewportClient(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ViewportFrame(NULL)
	, Viewport(NULL)
	, CameraColorTexture(NULL)
{
}

UOculusMR_CompositionViewportClient::UOculusMR_CompositionViewportClient(FVTableHelper& Helper)
	: Super(Helper)
	, ViewportFrame(NULL)
	, Viewport(NULL)
	, CameraColorTexture(NULL)
{
}

UOculusMR_CompositionViewportClient::~UOculusMR_CompositionViewportClient()
{
}

void UOculusMR_CompositionViewportClient::SetCastingSceneViewport(TSharedPtr<FOculusMR_CastingSceneViewport> InCastingSceneViewport)
{
	CastingSceneViewport = InCastingSceneViewport;
}

void UOculusMR_CompositionViewportClient::SetCameraColorTexture(UTexture2D* InCameraColorTexture)
{
	CameraColorTexture = InCameraColorTexture;
}

#pragma region UObject

void UOculusMR_CompositionViewportClient::PostInitProperties()
{
	Super::PostInitProperties();
}

void UOculusMR_CompositionViewportClient::BeginDestroy()
{
	Super::BeginDestroy();
}

#pragma endregion UObject

#pragma region FViewportClient

void UOculusMR_CompositionViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{
	// clear background
	SceneCanvas->DrawTile(0, 0, InViewport->GetSizeXY().X, InViewport->GetSizeXY().Y, 0.0f, 0.0f, 1.0f, 1.f, FLinearColor::Black, NULL, false);

	if (CameraColorTexture)
	{
		SceneCanvas->DrawTile(0, 0, InViewport->GetSizeXY().X, InViewport->GetSizeXY().Y, 0.0f, 0.0f, 1.0f, 1.f, FLinearColor::White, CameraColorTexture->Resource, false);
	}
}

void UOculusMR_CompositionViewportClient::ProcessScreenShots(FViewport* InViewport)
{
}

TOptional<bool> UOculusMR_CompositionViewportClient::QueryShowFocus(const EFocusCause InFocusCause) const
{
	return false;
}

void UOculusMR_CompositionViewportClient::LostFocus(FViewport* InViewport)
{
}

void UOculusMR_CompositionViewportClient::ReceivedFocus(FViewport* InViewport)
{
}

bool UOculusMR_CompositionViewportClient::IsFocused(FViewport* InViewport)
{
	return InViewport->HasFocus() || InViewport->HasMouseCapture();
}

void UOculusMR_CompositionViewportClient::Activated(FViewport* InViewport, const FWindowActivateEvent& InActivateEvent)
{
	ReceivedFocus(InViewport);
}

void UOculusMR_CompositionViewportClient::Deactivated(FViewport* InViewport, const FWindowActivateEvent& InActivateEvent)
{
	LostFocus(InViewport);
}

bool UOculusMR_CompositionViewportClient::WindowCloseRequested()
{
	return false;
}

void UOculusMR_CompositionViewportClient::CloseRequested(FViewport* InViewport)
{
	check(InViewport == Viewport);
	SetViewportFrame(NULL);
}

bool UOculusMR_CompositionViewportClient::IsOrtho() const
{
	return false;
}
#pragma endregion FViewportClient


void UOculusMR_CompositionViewportClient::SetViewportFrame(FViewportFrame* InViewportFrame)
{
	ViewportFrame = InViewportFrame;
	SetViewport(ViewportFrame ? ViewportFrame->GetViewport() : NULL);
}


void UOculusMR_CompositionViewportClient::SetViewport(FViewport* InViewport)
{
	FViewport* PreviousViewport = Viewport;
	Viewport = InViewport;
}

