// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IOSRuntimeSettings.generated.h"


UENUM()
enum class EPowerUsageFrameRateLock : uint8
{
    /** Frame rate is not limited */
    PUFRL_None = 0 UMETA(DisplayName="None"),
        
    /** Frame rate is limited to a maximum of 20 frames per second */
    PUFRL_20 = 20 UMETA(DisplayName="20 FPS"),
    
    /** Frame rate is limited to a maximum of 30 frames per second */
    PUFRL_30 = 30 UMETA(DisplayName="30 FPS"),
    
    /** Frame rate is limited to a maximum of 60 frames per second */
    PUFRL_60 = 60 UMETA(DisplayName="60 FPS"),
};

UENUM()
	enum class EIOSVersion : uint8
{
	/** iOS 6.1 */
	IOS_61 = 6 UMETA(DisplayName="6.1"),

	/** iOS 7 */
	IOS_7 = 7 UMETA(DisplayName="7.0"),

	/** iOS 8 */
	IOS_8 = 8 UMETA(DisplayName="8.0"),
};


/**
 *	IOS Build resource file struct, used to serialize filepaths to the configs for use in the build system,
 */
USTRUCT()
struct FIOSBuildResourceFilePath
{
	GENERATED_USTRUCT_BODY()
	
	/**
	 * Custom export item used to serialize FIOSBuildResourceFilePath types as only a filename, no garland.
	 */
	bool ExportTextItem(FString& ValueStr, FIOSBuildResourceFilePath const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
	{
		ValueStr += FilePath;
		return true;
	}

	/**
	 * Custom import item used to parse ini entries straight into the filename.
	 */
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
	{
		FilePath = Buffer;
		return true;
	}

	/**
	 * The path to the file.
	 */
	UPROPERTY(EditAnywhere, Category = FilePath)
	FString FilePath;
};

/**
 *	Setup our resource filepath to make it easier to parse in UBT
 */
template<>
struct TStructOpsTypeTraits<FIOSBuildResourceFilePath> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithExportTextItem = true,
		WithImportTextItem = true,
	};
};



/**
 * Implements the settings for the iOS target platform.
 */
UCLASS(config=Engine, defaultconfig)
class IOSRUNTIMESETTINGS_API UIOSRuntimeSettings : public UObject
{
public:
	GENERATED_UCLASS_BODY()

	// Should Game Center support (iOS Online Subsystem) be enabled?
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Online)
	uint32 bEnableGameCenterSupport : 1;
	
	// Whether or not to add support for Metal API (requires IOS8 and A7 processors)
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Rendering, meta = (DisplayName = "Supports Forward Rendering with Metal (A7 and up devices)"))
	bool bSupportsMetal;

	// Whether or not to add support for deferred rendering Metal API (requires IOS8 and A7 processors)
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Rendering, meta = (DisplayName = "Supports Deferred Rendering with Metal (A8 and up devices)"))
	bool bSupportsMetalMRT;
	
	// Whether or not to add support for OpenGL ES2 (if this is false, then your game should specify minimum IOS8 version and use "metal" instead of "opengles-2" in UIRequiredDeviceCapabilities)
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Rendering)
	bool bSupportsOpenGLES2;

	// Enable ArmV7 support? (this will be used if all type are unchecked) [CURRENTLY AVAILABLE ON MAC ONLY OR FULL SOURCE BUILD]
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build, meta = (DisplayName = "Support armv7 in Development"))
	bool bDevForArmV7;

	// Enable Arm64 support? [CURRENTLY AVAILABLE ON MAC ONLY OR FULL SOURCE BUILD]
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build, meta = (DisplayName = "Support arm64 in Development"))
	bool bDevForArm64;

	// Enable ArmV7s support? [CURRENTLY AVAILABLE ON MAC ONLY OR FULL SOURCE BUILD]
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build, meta = (DisplayName = "Support armv7s in Development"))
	bool bDevForArmV7S;

	// Enable ArmV7 support for shipping build? (this will be used if all type are unchecked) [CURRENTLY AVAILABLE ON MAC ONLY OR FULL SOURCE BUILD]
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build, meta = (DisplayName = "Support armv7 in Shipping"))
	bool bShipForArmV7;

	// Enable Arm64 support for shipping build? [CURRENTLY AVAILABLE ON MAC ONLY OR FULL SOURCE BUILD]
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build, meta = (DisplayName = "Support arm64 in Shipping"))
	bool bShipForArm64;

	// Enable ArmV7s support for shipping build? [CURRENTLY AVAILABLE ON MAC ONLY OR FULL SOURCE BUILD]
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build, meta = (DisplayName = "Support armv7s in Shipping"))
	bool bShipForArmV7S;
	
	// The name or ip address of the remote mac which will be used to build IOS
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Build")
	FString RemoteServerName;

	// Enable the use of RSync for remote builds on a mac
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Build", meta = (DisplayName = "Use RSync for IOS build"))
	bool bUseRSync;

	// The mac users name which matches the SSH Private Key, for remote builds using RSync.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Build", meta = (EditCondition = "bUseRSync", DisplayName = "User for RSync enabled builds."))
	FString RSyncUsername;

	// The path of the ssh permissions key to be used when connecting to the remote server.
	UPROPERTY(VisibleAnywhere, Category = "Build", meta = (DisplayName = "Existing SSH permissions file"))
	FString SSHPrivateKeyLocation;

	// The path of the ssh permissions key to be used when connecting to the remote server.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Build", meta = (EditCondition = "bUseRSync", DisplayName = "Override existing SSH permissions file"))
	FIOSBuildResourceFilePath SSHPrivateKeyOverridePath;

	// Does the application support portrait orientation?
	UPROPERTY(GlobalConfig, EditAnywhere, Category = DeviceOrientations)
	uint32 bSupportsPortraitOrientation : 1;

	// Does the application support upside down orientation?
	UPROPERTY(GlobalConfig, EditAnywhere, Category = DeviceOrientations)
	uint32 bSupportsUpsideDownOrientation : 1;

	// Does the application support landscape left orientation?
	UPROPERTY(GlobalConfig, EditAnywhere, Category = DeviceOrientations)
	uint32 bSupportsLandscapeLeftOrientation : 1;

	// Does the application support landscape right orientation?
	UPROPERTY(GlobalConfig, EditAnywhere, Category = DeviceOrientations)
	uint32 bSupportsLandscapeRightOrientation : 1;

	// Bundle Display Name
	UPROPERTY(GlobalConfig, EditAnywhere, Category = BundleInfo)
	FString BundleDisplayName;

	// Bundle Name
	UPROPERTY(GlobalConfig, EditAnywhere, Category = BundleInfo)
	FString BundleName;

	// Bundle identifier
	UPROPERTY(GlobalConfig, EditAnywhere, Category = BundleInfo)
	FString BundleIdentifier;

	// version info
	UPROPERTY(GlobalConfig, EditAnywhere, Category = BundleInfo)
	FString VersionInfo;
    
    /** Set the maximum frame rate to save on power consumption */
    UPROPERTY(GlobalConfig, EditAnywhere, Category = PowerUsage)
    TEnumAsByte<EPowerUsageFrameRateLock> FrameRateLock;

	/** Set the minimum iOS setting */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = OSInfo)
	TEnumAsByte<EIOSVersion> MinimumiOSVersion;

	/** Does the application support iPad */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = DeviceUsage)
	uint32 bSupportsIPad : 1;

	// Does the application support iPhone */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = DeviceUsage)
	uint32 bSupportsIPhone : 1;

	// extra data to be stored in the plist
	UPROPERTY(GlobalConfig, EditAnywhere, Category = ExtraData)
	FString AdditionalPlistData;

#if WITH_EDITOR
	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostInitProperties() override;
	// End of UObject interface
#endif
};
