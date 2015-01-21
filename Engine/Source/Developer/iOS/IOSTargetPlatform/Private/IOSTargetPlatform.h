// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IOSTargetPlatform.h: Declares the FIOSTargetPlatform class.
=============================================================================*/

#pragma once

#include "Ticker.h"

#if WITH_ENGINE
#include "StaticMeshResources.h"
#endif // WITH_ENGINE

/**
 * FIOSTargetPlatform, abstraction for cooking iOS platforms
 */
class FIOSTargetPlatform
	: public TTargetPlatformBase<FIOSPlatformProperties>
{
public:

	/**
	 * Default constructor.
	 */
	FIOSTargetPlatform();

	/**
	 * Destructor.
	 */
	~FIOSTargetPlatform();

public:

	// Begin TTargetPlatformBase interface

	virtual bool IsServerOnly( ) const override
	{
		return false;
	}

	// End TTargetPlatformBase interface

public:

	// Begin ITargetPlatform interface

	virtual void EnableDeviceCheck(bool OnOff) override;

	virtual void GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const override;

	virtual ECompressionFlags GetBaseCompressionMethod() const override
	{
		return COMPRESS_ZLIB;
	}

	virtual bool GenerateStreamingInstallManifest(const TMultiMap<FString, int32>& ChunkMap, const TSet<int32>& ChunkIDsInUse) const override
	{
		return true;
	}

	virtual ITargetDevicePtr GetDefaultDevice( ) const override;

	virtual ITargetDevicePtr GetDevice( const FTargetDeviceId& DeviceId ) override;

	virtual bool IsRunningPlatform( ) const override
	{
		#if PLATFORM_IOS && WITH_EDITOR
			return true;
		#else
			return false;
		#endif
	}

	virtual bool SupportsFeature( ETargetPlatformFeatures Feature ) const override
	{
		if (Feature == ETargetPlatformFeatures::Packaging)
		{
			// not implemented yet
			return true;
		}

		return TTargetPlatformBase<FIOSPlatformProperties>::SupportsFeature(Feature);
	}

	virtual bool IsSdkInstalled(bool bProjectHasCode, FString& OutTutorialPath) const override;
	virtual int32 CheckRequirements(const FString& ProjectPath, bool bProjectHasCode, FString& OutTutorialPath) const override;


#if WITH_ENGINE
	virtual void GetReflectionCaptureFormats( TArray<FName>& OutFormats ) const override
	{
		OutFormats.Add(FName(TEXT("EncodedHDR")));
	}

	virtual void GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const override;

	virtual void GetAllTargetedShaderFormats( TArray<FName>& OutFormats ) const override;

	virtual const class FStaticMeshLODSettings& GetStaticMeshLODSettings( ) const override
	{
		return StaticMeshLODSettings;
	}

	virtual void GetTextureFormats( const UTexture* Texture, TArray<FName>& OutFormats ) const override;

	virtual const struct FTextureLODSettings& GetTextureLODSettings( ) const override;

	virtual FName GetWaveFormat( const class USoundWave* Wave ) const override;
#endif // WITH_ENGINE


	DECLARE_DERIVED_EVENT(FIOSTargetPlatform, ITargetPlatform::FOnTargetDeviceDiscovered, FOnTargetDeviceDiscovered);
	virtual FOnTargetDeviceDiscovered& OnDeviceDiscovered( ) override
	{
		return DeviceDiscoveredEvent;
	}

	DECLARE_DERIVED_EVENT(FIOSTargetPlatform, ITargetPlatform::FOnTargetDeviceLost, FOnTargetDeviceLost);
	virtual FOnTargetDeviceLost& OnDeviceLost( ) override
	{
		return DeviceLostEvent;
	}

	// Begin ITargetPlatform interface

protected:

	/**
	 * Sends a ping message over the network to find devices running the launch daemon.
	 */
	void PingNetworkDevices( );

private:

	// Handles when the ticker fires.
	bool HandleTicker( float DeltaTime );

	// Handles received pong messages from the LauncherDaemon.
	void HandlePongMessage( const FIOSLaunchDaemonPong& Message, const IMessageContextRef& Context );

    void HandleDeviceConnected( const FIOSLaunchDaemonPong& Message );
    void HandleDeviceDisconnected( const FIOSLaunchDaemonPong& Message );

private:

	// Contains all discovered IOSTargetDevices over the network.
	TMap<FTargetDeviceId, FIOSTargetDevicePtr> Devices;

	// Holds a delegate to be invoked when the widget ticks.
	FTickerDelegate TickDelegate;

	// Handle to the registered TickDelegate.
	FDelegateHandle TickDelegateHandle;

	// Holds the message endpoint used for communicating with the LaunchDaemon.
	FMessageEndpointPtr MessageEndpoint;

#if WITH_ENGINE
	// Holds the Engine INI settings, for quick use.
	FConfigFile EngineSettings;

	// Holds the cache of the target LOD settings.
	FTextureLODSettings TextureLODSettings;

	// Holds the static mesh LOD settings.
	FStaticMeshLODSettings StaticMeshLODSettings;
#endif // WITH_ENGINE

    // holds usb device helper
    FIOSDeviceHelper DeviceHelper;

private:

	// Holds an event delegate that is executed when a new target device has been discovered.
	FOnTargetDeviceDiscovered DeviceDiscoveredEvent;

	// Holds an event delegate that is executed when a target device has been lost, i.e. disconnected or timed out.
	FOnTargetDeviceLost DeviceLostEvent;
};
