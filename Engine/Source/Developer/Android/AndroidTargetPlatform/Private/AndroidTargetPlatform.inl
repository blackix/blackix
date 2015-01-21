// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidTargetPlatform.inl: Implements the FAndroidTargetPlatform class.
=============================================================================*/


/* FAndroidTargetPlatform structors
 *****************************************************************************/

#define LOCTEXT_NAMESPACE "FAndroidTargetPlatform"

template<class TPlatformProperties>
inline FAndroidTargetPlatform<TPlatformProperties>::FAndroidTargetPlatform( ) :
	DeviceDetection(nullptr)
{
	#if WITH_ENGINE
		FConfigCacheIni::LoadLocalIniFile(EngineSettings, TEXT("Engine"), true, *TTargetPlatformBase<TPlatformProperties>::PlatformName());
		TextureLODSettings.Initialize(EngineSettings, TEXT("SystemSettings"));
		StaticMeshLODSettings.Initialize(EngineSettings);
	#endif

	TickDelegate = FTickerDelegate::CreateRaw(this, &FAndroidTargetPlatform::HandleTicker);
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate, 4.0f);
}


template<class TPlatformProperties>
inline FAndroidTargetPlatform<TPlatformProperties>::~FAndroidTargetPlatform()
{ 
	 FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
}


/* ITargetPlatform overrides
 *****************************************************************************/

template<class TPlatformProperties>
inline void FAndroidTargetPlatform<TPlatformProperties>::GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const
{
	OutDevices.Reset();

	for (auto Iter = Devices.CreateConstIterator(); Iter; ++Iter)
	{
		OutDevices.Add(Iter.Value());
	}
}

template<class TPlatformProperties>
inline ECompressionFlags FAndroidTargetPlatform<TPlatformProperties>::GetBaseCompressionMethod( ) const
{
	return COMPRESS_ZLIB;
}

template<class TPlatformProperties>
inline ITargetDevicePtr FAndroidTargetPlatform<TPlatformProperties>::GetDefaultDevice( ) const
{
	// return the first device in the list
	if (Devices.Num() > 0)
	{
		auto Iter = Devices.CreateConstIterator();
		if (Iter)
		{
			return Iter.Value();
		}
	}

	return nullptr;
}

template<class TPlatformProperties>
inline ITargetDevicePtr FAndroidTargetPlatform<TPlatformProperties>::GetDevice( const FTargetDeviceId& DeviceId )
{
	if (DeviceId.GetPlatformName() == TTargetPlatformBase<TPlatformProperties>::PlatformName())
	{
		return Devices.FindRef(DeviceId.GetDeviceName());
	}

	return nullptr;
}

template<class TPlatformProperties>
inline bool FAndroidTargetPlatform<TPlatformProperties>::IsRunningPlatform( ) const
{
	return false; // This platform never runs the target platform framework
}


template<class TPlatformProperties>
inline bool FAndroidTargetPlatform<TPlatformProperties>::IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const
{
	OutDocumentationPath = FString("Shared/Tutorials/SettingUpAndroidTutorial");

	TCHAR ANDROID_HOME[MAX_PATH];
	TCHAR JAVA_HOME[MAX_PATH];
	TCHAR ANT_HOME[MAX_PATH];
	TCHAR NDKROOT[MAX_PATH];
	FPlatformMisc::GetEnvironmentVariable(TEXT("ANDROID_HOME"), ANDROID_HOME, MAX_PATH);
	FPlatformMisc::GetEnvironmentVariable(TEXT("JAVA_HOME"), JAVA_HOME, MAX_PATH);
	FPlatformMisc::GetEnvironmentVariable(TEXT("ANT_HOME"), ANT_HOME, MAX_PATH);
	FPlatformMisc::GetEnvironmentVariable(TEXT("NDKROOT"), NDKROOT, MAX_PATH);

	// make sure ANDROID_HOME points to the right thing
	if (ANDROID_HOME[0] == 0 ||
#if PLATFORM_WINDOWS
		IFileManager::Get().FileSize(*(FString(ANDROID_HOME) / TEXT("platform-tools/adb.exe"))) < 0)
#else
		IFileManager::Get().FileSize(*(FString(ANDROID_HOME) / TEXT("platform-tools/adb"))) < 0)
#endif
	{
		return false;
	}

#if PLATFORM_WINDOWS
	// make sure that JAVA_HOME points to the right thing
	if (JAVA_HOME[0] == 0 ||
		IFileManager::Get().FileSize(*(FString(JAVA_HOME) / TEXT("bin/javac.exe"))) < 0)
	{
		return false;
	}
#endif

	// now look for ANT_HOME, or the ADT workaround of looking for a plugin
	if (ANT_HOME[0] == 0)
	{
		// look for plugins in eclipse (this is enough to assume we have an ant plugin)
		if (!IFileManager::Get().DirectoryExists(*(FString(ANDROID_HOME) / TEXT("../eclipse/plugins"))))
		{
			return false;
		}
	}

	// we need NDKROOT if the game has code
	if (bProjectHasCode)
	{
		if (NDKROOT[0] == 0 ||
			IFileManager::Get().FileSize(*(FString(NDKROOT) / TEXT("ndk-build.cmd"))) < 0)
		{
			return false;
		}
	}

	return true;
}


template<class TPlatformProperties>
inline bool FAndroidTargetPlatform<TPlatformProperties>::SupportsFeature( ETargetPlatformFeatures Feature ) const
{
	if (Feature == ETargetPlatformFeatures::Packaging)
	{
		return true;
	}

	return TTargetPlatformBase<TPlatformProperties>::SupportsFeature(Feature);
}


#if WITH_ENGINE

template<class TPlatformProperties>
inline void FAndroidTargetPlatform<TPlatformProperties>::GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const
{
	static FName NAME_OPENGL_ES2(TEXT("GLSL_ES2"));
	OutFormats.AddUnique(NAME_OPENGL_ES2);
}

template<class TPlatformProperties>
inline void FAndroidTargetPlatform<TPlatformProperties>::GetAllTargetedShaderFormats( TArray<FName>& OutFormats ) const
{
	GetAllPossibleShaderFormats(OutFormats);
}


template<class TPlatformProperties>
inline const FStaticMeshLODSettings& FAndroidTargetPlatform<TPlatformProperties>::GetStaticMeshLODSettings( ) const
{
	return StaticMeshLODSettings;
}


template<class TPlatformProperties>
inline void FAndroidTargetPlatform<TPlatformProperties>::GetTextureFormats( const UTexture* InTexture, TArray<FName>& OutFormats ) const
{
	check(InTexture);

	// The order we add texture formats to OutFormats is important. When multiple formats are cooked
	// and supported by the device, the first supported format listed will be used. 
	// eg, ETC1/uncompressed should always be last

	bool bNoCompression = InTexture->CompressionNone								// Code wants the texture uncompressed.
		|| (InTexture->LODGroup == TEXTUREGROUP_ColorLookupTable)	// Textures in certain LOD groups should remain uncompressed.
		|| (InTexture->LODGroup == TEXTUREGROUP_Bokeh)
		|| (InTexture->CompressionSettings == TC_EditorIcon)
		|| (InTexture->Source.GetSizeX() < 4)						// Don't compress textures smaller than the DXT block size.
		|| (InTexture->Source.GetSizeY() < 4)
		|| (InTexture->Source.GetSizeX() % 4 != 0)
		|| (InTexture->Source.GetSizeY() % 4 != 0);

	// Determine the pixel format of the compressed texture.
	if (bNoCompression && InTexture->HasHDRSource())
	{
		OutFormats.Add(AndroidTexFormat::NameRGBA16F);
	}
	else if (bNoCompression)
	{
		OutFormats.Add(AndroidTexFormat::NameBGRA8);
	}
	else if (InTexture->CompressionSettings == TC_HDR)
	{
		OutFormats.Add(AndroidTexFormat::NameRGBA16F);
	}
	else if (InTexture->CompressionSettings == TC_Normalmap)
	{
		AddTextureFormatIfSupports(AndroidTexFormat::NamePVRTC4, OutFormats);
		AddTextureFormatIfSupports(AndroidTexFormat::NameDXT5, OutFormats);
		AddTextureFormatIfSupports(AndroidTexFormat::NameATC_RGBA_I, OutFormats);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC2, OutFormats);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC1, OutFormats);
	}
	else if (InTexture->CompressionSettings == TC_Displacementmap)
	{
		OutFormats.Add(AndroidTexFormat::NameRGBA16F);
	}
	else if (InTexture->CompressionSettings == TC_VectorDisplacementmap)
	{
		OutFormats.Add(AndroidTexFormat::NameBGRA8);
	}
	else if (InTexture->CompressionSettings == TC_Grayscale)
	{
		OutFormats.Add(AndroidTexFormat::NameG8);
	}
	else if (InTexture->CompressionSettings == TC_Alpha)
	{
		OutFormats.Add(AndroidTexFormat::NameG8);
	}
	else if (InTexture->CompressionSettings == TC_DistanceFieldFont)
	{
		OutFormats.Add(AndroidTexFormat::NameG8);
	}
	else if (InTexture->bForcePVRTC4)
	{
		AddTextureFormatIfSupports(AndroidTexFormat::NamePVRTC4, OutFormats);
		AddTextureFormatIfSupports(AndroidTexFormat::NameDXT5, OutFormats);
		AddTextureFormatIfSupports(AndroidTexFormat::NameATC_RGBA_I, OutFormats);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC2, OutFormats);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC1, OutFormats);
	}
	else if (InTexture->CompressionNoAlpha)
	{
		AddTextureFormatIfSupports(AndroidTexFormat::NamePVRTC2, OutFormats);
		AddTextureFormatIfSupports(AndroidTexFormat::NameDXT1, OutFormats);
		AddTextureFormatIfSupports(AndroidTexFormat::NameATC_RGB, OutFormats);
		AddTextureFormatIfSupports(AndroidTexFormat::NameETC2_RGB, OutFormats);
		AddTextureFormatIfSupports(AndroidTexFormat::NameETC1, OutFormats);
	}
	else if (InTexture->bDitherMipMapAlpha)
	{
		AddTextureFormatIfSupports(AndroidTexFormat::NamePVRTC4, OutFormats);
		AddTextureFormatIfSupports(AndroidTexFormat::NameDXT5, OutFormats);
		AddTextureFormatIfSupports(AndroidTexFormat::NameATC_RGBA_I, OutFormats);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC2, OutFormats);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC1, OutFormats);
	}
	else
	{
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoPVRTC, OutFormats);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoDXT, OutFormats);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoATC, OutFormats);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC2, OutFormats);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC1, OutFormats);
	}
}


template<class TPlatformProperties>
const FTextureLODSettings& FAndroidTargetPlatform<TPlatformProperties>::GetTextureLODSettings( ) const
{
	return TextureLODSettings;
}


template<class TPlatformProperties>
FName FAndroidTargetPlatform<TPlatformProperties>::GetWaveFormat( const class USoundWave* Wave ) const
{
	static bool formatRead = false;
	static FName NAME_FORMAT;

	if (!formatRead)
	{
		formatRead = true;

		FString audioSetting;
		if (!GConfig->GetString(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("AndroidAudio"), audioSetting, GEngineIni))
		{
			audioSetting = TEXT("DEFAULT");
		}

#if WITH_OGGVORBIS
		if (audioSetting == TEXT("OGG") || audioSetting == TEXT("Default"))
		{
			static FName NAME_OGG(TEXT("OGG"));
			NAME_FORMAT = NAME_OGG;
		}
#else
		if (audioSetting == TEXT("OGG"))
		{
			UE_LOG(LogAudio, Error, TEXT("Attemped to select Ogg Vorbis encoding when the cooker is built without Ogg Vorbis support."));
		}
#endif
		else
		{
	
			// Otherwise return ADPCM as it'll either be option '2' or 'default' depending on WITH_OGGVORBIS config
			static FName NAME_ADPCM(TEXT("ADPCM"));
			NAME_FORMAT = NAME_ADPCM;
		}
	}
	return NAME_FORMAT;
}

#endif //WITH_ENGINE

template<class TPlatformProperties>
bool FAndroidTargetPlatform<TPlatformProperties>::SupportsVariants() const
{
	return true;
}

template<class TPlatformProperties>
FText FAndroidTargetPlatform<TPlatformProperties>::GetVariantTitle() const
{
	return LOCTEXT("AndroidVariantTitle", "Texture Format");
}

/* FAndroidTargetPlatform implementation
 *****************************************************************************/

template<class TPlatformProperties>
inline void FAndroidTargetPlatform<TPlatformProperties>::AddTextureFormatIfSupports( FName Format, TArray<FName>& OutFormats ) const
{
	if (SupportsTextureFormat(Format))
	{
		OutFormats.Add(Format);
	}
}


/* FAndroidTargetPlatform callbacks
 *****************************************************************************/

template<class TPlatformProperties>
inline bool FAndroidTargetPlatform<TPlatformProperties>::HandleTicker( float DeltaTime )
{
	if (DeviceDetection == nullptr)
	{
		DeviceDetection = FModuleManager::LoadModuleChecked<IAndroidDeviceDetectionModule>("AndroidDeviceDetection").GetAndroidDeviceDetection();
	}

	TArray<FString> ConnectedDeviceIds;

	{
		FScopeLock ScopeLock(DeviceDetection->GetDeviceMapLock());

		auto DeviceIt = DeviceDetection->GetDeviceMap().CreateConstIterator();
		
		for (; DeviceIt; ++DeviceIt)
		{
			ConnectedDeviceIds.Add(DeviceIt.Key());

			// see if this device is already known
			if (Devices.Contains(DeviceIt.Key()))
			{
				continue;
			}

			const FAndroidDeviceInfo& DeviceInfo = DeviceIt.Value();

			// check if this platform is supported by the extensions and version
			if (!SupportedByExtensionsString(DeviceInfo.GLESExtensions, DeviceInfo.GLESVersion))
			{
				continue;
			}

			// create target device
			FAndroidTargetDevicePtr& Device = Devices.Add(DeviceInfo.SerialNumber);

			Device = MakeShareable(new FAndroidTargetDevice(*this, DeviceInfo.SerialNumber, GetAndroidVariantName()));

			Device->SetConnected(true);
			Device->SetModel(DeviceInfo.Model);
			Device->SetDeviceName(DeviceInfo.DeviceName);
			Device->SetAuthorized(!DeviceInfo.bUnauthorizedDevice);
			Device->SetVersions(DeviceInfo.SDKVersion, DeviceInfo.HumanAndroidVersion);

			DeviceDiscoveredEvent.Broadcast(Device.ToSharedRef());
		}
	}

	// remove disconnected devices
	for (auto Iter = Devices.CreateIterator(); Iter; ++Iter)
	{
		if (!ConnectedDeviceIds.Contains(Iter.Key()))
		{
			FAndroidTargetDevicePtr RemovedDevice = Iter.Value();
			RemovedDevice->SetConnected(false);

			Iter.RemoveCurrent();

			DeviceLostEvent.Broadcast(RemovedDevice.ToSharedRef());
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
