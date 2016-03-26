// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IHeadMountedDisplay.h"

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#include "HeadMountedDisplayCommon.h"
#include "AsyncLoadingSplash.h"
#include "OculusRiftLayers.h"

// Implementation of async splash for OculusRift
class FOculusRiftSplash : public FAsyncLoadingSplash
{
public:
	FOculusRiftSplash(class FOculusRiftHMD*);

	virtual void Tick(float DeltaTime);
	virtual bool IsTickable() const override;

	virtual void Startup() override;
	virtual void Shutdown() override;

	virtual void OnLoadingBegins() override;
	virtual void OnLoadingEnds() override;

	void Show();
	void Hide();

	void ReleaseResources();

protected:

	void PushFrame();
	void PushBlackFrame();

private:
	class FOculusRiftHMD*		pPlugin;
	TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> RenderFrame;
	OculusRift::FLayerManager	LayerMgr;
	uint32						SplashLID, SplashLID_RenderThread;
	FThreadSafeCounter			ShowingBlack;
	float						DisplayRefreshRate;
	float						CurrentAngle;
};

#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS
