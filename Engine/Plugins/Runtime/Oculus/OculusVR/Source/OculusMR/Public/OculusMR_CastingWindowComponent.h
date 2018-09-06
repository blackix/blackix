#pragma once

#include "IOculusMRModule.h"
#include "ActorComponent.h"
#include "Widgets/SWindow.h"
#include "Engine/CastingViewportClient.h"
#include "OculusMR_CastingSceneViewport.h"
#include "OculusMR_CompositionViewportClient.h"
#include "OculusMR_CastingWindowComponent.generated.h"

struct FWorldContext;

DECLARE_DELEGATE(FOculusMR_OnCastingWindowClosed);

UCLASS(MinimalAPI, meta = (BlueprintSpawnableComponent), ClassGroup = OculusMR)
class UOculusMR_CastingWindowComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void OpenCastingWindow(ECastingViewportCompositionMethod CompositionMethod, int WidthPerView, int HeightPerView);
	void CloseCastingWindow();
	bool HasCastingWindowOpened() const;

	void SetCameraColorTexture(UTexture2D* InCameraColorTexture);

	double GetExpectedLantencyInSeconds() const;
	void SetExpectedLantencyInSeconds(double InLatency);

	void DrawCompositionViewport();

	TSharedPtr<SWindow> OutputWindow;

	TSharedPtr<FOculusMR_CastingSceneViewport> CastingSceneViewport;

	UPROPERTY()
	class UCastingViewportClient* CastingViewportClient;

	TSharedPtr<FSceneViewport> CompositionSceneViewport;

	UPROPERTY()
	class UOculusMR_CompositionViewportClient* CompositionViewportClient;

	FWorldContext* WorldContext;

	FOculusMR_OnCastingWindowClosed OnWindowClosedDelegate;
};
