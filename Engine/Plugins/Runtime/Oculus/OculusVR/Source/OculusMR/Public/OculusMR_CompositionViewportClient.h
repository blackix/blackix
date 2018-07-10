#pragma once

#include "IOculusMRModule.h"
#include "CoreMinimal.h"
#include "UnrealClient.h"
#include "OculusMR_CastingSceneViewport.h"
#include "OculusMR_CompositionViewportClient.generated.h"

class UTexture2D;

UCLASS(transient)
class UOculusMR_CompositionViewportClient : public UObject, public FCommonViewportClient
{
	GENERATED_UCLASS_BODY()

public:
	/** DO NOT USE. This constructor is for internal usage only for hot-reload purposes. */
	UOculusMR_CompositionViewportClient(FVTableHelper& Helper);
	virtual ~UOculusMR_CompositionViewportClient();

	void SetCastingSceneViewport(TSharedPtr<FOculusMR_CastingSceneViewport> InCastingSceneViewport);
	void SetCameraColorTexture(UTexture2D* InCameraColorTexture);

	//~ Begin UObject Interface
	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;
	//~ End UObject Interface

	//~ Begin FViewportClient Interface.
	virtual void RedrawRequested(FViewport* InViewport) override {}

	virtual void Draw(FViewport* Viewport, FCanvas* SceneCanvas) override;
	virtual void ProcessScreenShots(FViewport* Viewport) override;
	virtual TOptional<bool> QueryShowFocus(const EFocusCause InFocusCause) const override;
	virtual void LostFocus(FViewport* Viewport) override;
	virtual void ReceivedFocus(FViewport* Viewport) override;
	virtual bool IsFocused(FViewport* Viewport) override;
	virtual void Activated(FViewport* InViewport, const FWindowActivateEvent& InActivateEvent) override;
	virtual void Deactivated(FViewport* InViewport, const FWindowActivateEvent& InActivateEvent) override;
	virtual bool WindowCloseRequested() override;
	virtual void CloseRequested(FViewport* Viewport) override;
	virtual bool RequiresHitProxyStorage() override { return 0; }
	virtual bool IsOrtho() const override;
	//~ End FViewportClient Interface.

	/**
	* Set this GameViewportClient's viewport and viewport frame to the viewport specified
	* @param	InViewportFrame	The viewportframe to set
	*/
	virtual void SetViewportFrame(FViewportFrame* InViewportFrame);

	/**
	* Set this GameViewportClient's viewport to the viewport specified
	* @param	InViewportFrame	The viewport to set
	*/
	virtual void SetViewport(FViewport* InViewportFrame);

	/** The platform-specific viewport which this viewport client is attached to. */
	FViewport* Viewport;

	/** The platform-specific viewport frame which this viewport is contained by. */
	FViewportFrame* ViewportFrame;

	UPROPERTY()
	UTexture2D* CameraColorTexture;

	TSharedPtr<FOculusMR_CastingSceneViewport> CastingSceneViewport;
};
