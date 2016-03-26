// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Engine.h"
#include "AsyncLoadingSplash.h"

DEFINE_LOG_CATEGORY_STATIC(LogLoadingSplash, Log, All);

FAsyncLoadingSplash::FAsyncLoadingSplash() : 
	LoadingTexture(nullptr)
	, QuadCenterDistanceInMeters(4.0f, 0.f, 0.f)
	, QuadSizeInMeters(3.f, 3.f)
	, RotationDeltaInDeg(0)
	, RotationAxis(1.f, 0, 0)
	, bInitialized(false)
{
}

FAsyncLoadingSplash::~FAsyncLoadingSplash()
{
	// Make sure RenTicker is freed in Shutdown
	check(!RenTicker.IsValid())
}

void FAsyncLoadingSplash::Startup()
{
	if (!bInitialized)
	{
		RenTicker = MakeShareable(new FTicker(this));
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(RegisterAsyncTick,
			FTickableObjectRenderThread*, RenTicker, RenTicker.Get(),
			{
				RenTicker->Register();
			});

		// Add a delegate to start playing movies when we start loading a map
		FCoreUObjectDelegates::PreLoadMap.AddSP(this, &FAsyncLoadingSplash::OnPreLoadMap);
		FCoreUObjectDelegates::PostLoadMap.AddSP(this, &FAsyncLoadingSplash::OnPostLoadMap);
		bInitialized = true;
	}
}

void FAsyncLoadingSplash::Shutdown()
{
	if (bInitialized)
	{
		UnloadTexture();

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(UnregisterAsyncTick, 
		TSharedPtr<FTicker>&, RenTicker, RenTicker,
		{
			RenTicker->Unregister();
			RenTicker = nullptr;
		});
		FlushRenderingCommands();

		FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
		FCoreUObjectDelegates::PostLoadMap.RemoveAll(this);

		bInitialized = false;
		LoadingCompleted.Set(0);
		LoadingStarted.Set(0);
	}
}

void FAsyncLoadingSplash::LoadTexture(const FString& TexturePath)
{
	UnloadTexture();

	UE_LOG(LogLoadingSplash, Log, TEXT("Loading texture for splash %s..."), *TexturePath);
	LoadingTexture = LoadObject<UTexture2D>(NULL, *TexturePath, NULL, LOAD_None, NULL);
	if (LoadingTexture != nullptr)
	{
		LoadingTexture->SetFlags(RF_RootSet);
		UE_LOG(LogLoadingSplash, Log, TEXT("...Success. "));
	}
}

void FAsyncLoadingSplash::UnloadTexture()
{
	if (LoadingTexture && LoadingTexture->IsValidLowLevel())
	{
		LoadingTexture->ClearFlags(RF_RootSet);
		LoadingTexture = nullptr;
	}
}

void FAsyncLoadingSplash::OnLoadingBegins()
{
	UE_LOG(LogLoadingSplash, Log, TEXT("Loading begins"));
	LoadingStarted.Set(1);
	LoadingCompleted.Set(0);
}

void FAsyncLoadingSplash::OnLoadingEnds()
{
	UE_LOG(LogLoadingSplash, Log, TEXT("Loading ends"));
	LoadingStarted.Set(0);
	LoadingCompleted.Set(1);
}

void FAsyncLoadingSplash::SetParams(const FString& InTexturePath, const FVector& InDistanceInMeters, const FVector2D& InSizeInMeters, const FVector& InRotationAxis, float InRotationDeltaInDeg)
{
	TexturePath						= InTexturePath;
	QuadCenterDistanceInMeters		= InDistanceInMeters;
	QuadSizeInMeters				= InSizeInMeters;
	RotationDeltaInDeg				= InRotationDeltaInDeg;
	RotationAxis					= InRotationAxis;
}

void FAsyncLoadingSplash::GetParams(FString& OutTexturePath, FVector& OutDistanceInMeters, FVector2D& OutSizeInMeters, FVector& OutRotationAxis, float& OutRotationDeltaInDeg) const
{
	OutTexturePath			= TexturePath;
	OutDistanceInMeters		= QuadCenterDistanceInMeters;
	OutSizeInMeters			= QuadSizeInMeters;
	OutRotationDeltaInDeg	= RotationDeltaInDeg;
	OutRotationAxis			= RotationAxis;
}

