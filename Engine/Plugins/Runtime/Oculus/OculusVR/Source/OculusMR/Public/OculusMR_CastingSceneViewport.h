#pragma once

#include "IOculusMRModule.h"
#include "CoreMinimal.h"
#include "Slate/SceneViewport.h"

class FOculusMR_CastingSceneViewport : public FSceneViewport
{
public:
	typedef FSceneViewport Super;

	FOculusMR_CastingSceneViewport(FViewportClient* InViewportClient, TSharedPtr<SViewport> InViewportWidget, int32 InMaximumBufferedFrames);

	double GetExpectedLantencyInSeconds() const { return ExpectedLantencyInSeconds; }
	void SetExpectedLantencyInSeconds(double InLatency);

	virtual bool IsStereoRenderingAllowed() const override;

	/** Called before BeginRenderFrame is enqueued */
	virtual void EnqueueBeginRenderFrame(const bool bShouldPresent) override;

	/** ISlateViewport interface */
	virtual FSlateShaderResource* GetViewportRenderTargetTexture() const override;
	virtual FSlateShaderResource* GetViewportRenderTargetTexture() override;

protected:
	// FRenderResource interface.
	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;

	int32 GetPresentBufferredTargetIndex() const;

	int32 MaximumBufferedFrames;
	TArray<double> BufferedFrameStartTimes;

	double ExpectedLantencyInSeconds;
};