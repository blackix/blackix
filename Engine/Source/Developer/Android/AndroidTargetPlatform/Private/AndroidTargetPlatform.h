// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidTargetPlatform.h: Declares the FAndroidTargetPlatform class.
=============================================================================*/

#pragma once

#include "Ticker.h"

#if WITH_ENGINE
#include "StaticMeshResources.h"
#endif // WITH_ENGINE

/**
 * Defines supported texture format names.
 */
namespace AndroidTexFormat
{
	// Compressed Texture Formats
	static FName NamePVRTC2(TEXT("PVRTC2"));
	static FName NamePVRTC4(TEXT("PVRTC4"));
	static FName NameAutoPVRTC(TEXT("AutoPVRTC"));
	static FName NameDXT1(TEXT("DXT1"));
	static FName NameDXT5(TEXT("DXT5"));
	static FName NameAutoDXT(TEXT("AutoDXT"));
	static FName NameATC_RGB(TEXT("ATC_RGB"));
	static FName NameATC_RGBA_E(TEXT("ATC_RGBA_E"));		// explicit alpha
	static FName NameATC_RGBA_I(TEXT("ATC_RGBA_I"));		// interpolated alpha
	static FName NameAutoATC(TEXT("AutoATC"));
	static FName NameETC1(TEXT("ETC1"));
	static FName NameAutoETC1(TEXT("AutoETC1"));			// ETC1 or uncompressed RGBA, if alpha channel required
	static FName NameETC2_RGB(TEXT("ETC2_RGB"));
	static FName NameETC2_RGBA(TEXT("ETC2_RGBA"));
	static FName NameAutoETC2(TEXT("AutoETC2"));

	// Uncompressed Texture Formats
	static FName NameBGRA8(TEXT("BGRA8"));
	static FName NameG8(TEXT("G8"));
	static FName NameVU8(TEXT("VU8"));
	static FName NameRGBA16F(TEXT("RGBA16F"));
}


/**
 * FAndroidTargetPlatform, abstraction for cooking Android platforms
 */
template<class TPlatformProperties>
class FAndroidTargetPlatform
	: public TTargetPlatformBase< TPlatformProperties >
{
public:

	/**
	 * Default constructor.
	 */
	FAndroidTargetPlatform( );

	/**
	 * Destructor
	 */
	~FAndroidTargetPlatform();

public:

	/**
	 * Gets the name of the Android platform variant, i.e. ATC, DXT or PVRTC.
	 *
	 * @param Variant name.
	 */
	virtual FString GetAndroidVariantName( )
	{
		return FString();	
	}

public:

	// Begin ITargetPlatform interface

	virtual void EnableDeviceCheck(bool OnOff) override {}

	virtual bool AddDevice( const FString& DeviceName, bool bDefault ) override
	{
		return false;
	}

	virtual void GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const override;

	virtual ECompressionFlags GetBaseCompressionMethod( ) const override;

	virtual bool GenerateStreamingInstallManifest(const TMultiMap<FString, int32>& ChunkMap, const TSet<int32>& ChunkIDsInUse) const override
	{
		return true;
	}

	virtual ITargetDevicePtr GetDefaultDevice( ) const override;

	virtual ITargetDevicePtr GetDevice( const FTargetDeviceId& DeviceId ) override;

	virtual bool IsRunningPlatform( ) const override;

	virtual bool IsServerOnly( ) const override
	{
		return false;
	}

	virtual bool IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const override;

	virtual bool SupportsFeature( ETargetPlatformFeatures Feature ) const override;

	virtual bool SupportsTextureFormat( FName Format ) const 
	{
		// By default we support all texture formats.
		return true;
	}

#if WITH_ENGINE
	virtual void GetReflectionCaptureFormats( TArray<FName>& OutFormats ) const override
	{
		OutFormats.Add(FName(TEXT("EncodedHDR")));
	}

	virtual void GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const override;

	virtual void GetAllTargetedShaderFormats(TArray<FName>& OutFormats) const override;

	virtual const class FStaticMeshLODSettings& GetStaticMeshLODSettings() const override;

	virtual void GetTextureFormats( const UTexture* InTexture, TArray<FName>& OutFormats ) const override;

	virtual const struct FTextureLODSettings& GetTextureLODSettings( ) const override;

	virtual FName GetWaveFormat( const class USoundWave* Wave ) const override;
#endif //WITH_ENGINE

	virtual bool SupportsVariants() const override;

	virtual FText GetVariantTitle() const override;

	DECLARE_DERIVED_EVENT(FAndroidTargetPlatform, ITargetPlatform::FOnTargetDeviceDiscovered, FOnTargetDeviceDiscovered);
	virtual FOnTargetDeviceDiscovered& OnDeviceDiscovered( ) override
	{
		return DeviceDiscoveredEvent;
	}

	DECLARE_DERIVED_EVENT(FAndroidTargetPlatform, ITargetPlatform::FOnTargetDeviceLost, FOnTargetDeviceLost);
	virtual FOnTargetDeviceLost& OnDeviceLost( ) override
	{
		return DeviceLostEvent;
	}

	// End ITargetPlatform interface

protected:

	/**
	 * Adds the specified texture format to the OutFormats if this android target platforms supports it.
	 *
	 * @param Format - The format to add.
	 * @param OutFormats - The collection of formats to add to.
	 */
	void AddTextureFormatIfSupports( FName Format, TArray<FName>& OutFormats ) const;

	/**
	 * Return true if this device has a supported set of extensions for this platform.
	 *
	 * @param Extensions - The GL extensions string.
	 * @param GLESVersion - The GLES version reported by this device.
	 */
	virtual bool SupportedByExtensionsString( const FString& ExtensionsString, const int GLESVersion ) const
	{
		return true;
	}

private:

	// Handles when the ticker fires.
	bool HandleTicker( float DeltaTime );

private:

	// Holds a map of valid devices.
	TMap<FString, FAndroidTargetDevicePtr> Devices;

	// Holds a delegate to be invoked when the widget ticks.
	FTickerDelegate TickDelegate;

	// Handle to the registered TickDelegate.
	FDelegateHandle TickDelegateHandle;

	// Pointer to the device detection handler that grabs device ids in another thread
	IAndroidDeviceDetection* DeviceDetection;

#if WITH_ENGINE
	// Holds the Engine INI settings (for quick access).
	FConfigFile EngineSettings;

	// Holds a cache of the target LOD settings.
	FTextureLODSettings TextureLODSettings;

	// Holds the static mesh LOD settings.
	FStaticMeshLODSettings StaticMeshLODSettings;

	ITargetDevicePtr DefaultDevice;
#endif //WITH_ENGINE

private:

	// Holds an event delegate that is executed when a new target device has been discovered.
	FOnTargetDeviceDiscovered DeviceDiscoveredEvent;

	// Holds an event delegate that is executed when a target device has been lost, i.e. disconnected or timed out.
	FOnTargetDeviceLost DeviceLostEvent;
};


#include "AndroidTargetPlatform.inl"
