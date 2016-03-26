// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TickableObjectRenderThread.h"

// Base class for asynchronous loading splash.
class FAsyncLoadingSplash : public TSharedFromThis<FAsyncLoadingSplash>
{
public:
	class FTicker : public FTickableObjectRenderThread, public TSharedFromThis<FTicker>
	{
	public:
		FTicker(FAsyncLoadingSplash* InSplash) : FTickableObjectRenderThread(false, true), pSplash(InSplash) {}

		virtual void Tick(float DeltaTime) override { pSplash->Tick(DeltaTime); }
		virtual TStatId GetStatId() const override  { RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncLoadingSplash, STATGROUP_Tickables); }
		virtual bool IsTickable() const override	{ return pSplash->IsTickable(); }
	protected:
		FAsyncLoadingSplash* pSplash;
	};

	FAsyncLoadingSplash();
	virtual ~FAsyncLoadingSplash();

	// FTickableObjectRenderThread implementation
	virtual void Tick(float DeltaTime) {}
	virtual bool IsTickable() const { return IsLoadingStarted() && !IsDone(); }

	virtual void Startup();
	virtual void Shutdown();

	virtual bool IsLoadingStarted() const	{ return LoadingStarted.GetValue() == 1; }
	virtual bool IsDone() const				{ return LoadingCompleted.GetValue() == 1; }

	virtual void OnLoadingBegins();
	virtual void OnLoadingEnds();

	virtual void SetParams(const FString& InTexturePath, const FVector& InDistanceInMeters, const FVector2D& InSizeInMeters, const FVector& InRotationAxis, float InRotationDeltaInDeg);
	virtual void GetParams(FString& OutTexturePath, FVector& OutDistanceInMeters, FVector2D& OutSizeInMeters, FVector& OutRotationAxis, float& OutRotationDeltaInDeg) const;

	// delegate method, called when loading begins
	void OnPreLoadMap() { OnLoadingBegins(); }

	// delegate method, called when loading ends
	void OnPostLoadMap() { OnLoadingEnds(); }

protected:
	void LoadTexture(const FString& TexturePath);
	void UnloadTexture();

	TSharedPtr<FTicker>	RenTicker;

	UPROPERTY()
	UTexture2D*			LoadingTexture;

	FThreadSafeCounter	LoadingCompleted;
	FThreadSafeCounter	LoadingStarted;

	FString				TexturePath;
	FVector				QuadCenterDistanceInMeters;
	FVector2D			QuadSizeInMeters;
	float				RotationDeltaInDeg;
	FVector				RotationAxis;

	bool				bInitialized;
};
