// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "SecureHash.h"
#include "DebuggingDefines.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "EngineVersion.h"

#define LOCTEXT_NAMESPACE "LinkerLoad"

DECLARE_STATS_GROUP_VERBOSE(TEXT("Linker Load"), STATGROUP_LinkerLoad, STATCAT_Advanced);

DECLARE_CYCLE_STAT(TEXT("Linker Preload"),STAT_LinkerPreload,STATGROUP_LinkerLoad);
DECLARE_CYCLE_STAT(TEXT("Linker Precache"),STAT_LinkerPrecache,STATGROUP_LinkerLoad);
DECLARE_CYCLE_STAT(TEXT("Linker Serialize"),STAT_LinkerSerialize,STATGROUP_LinkerLoad);


/** Points to the main PackageLinker currently being serialized (Defined in Linker.cpp) */
extern ULinkerLoad* GSerializedPackageLinker;
/** The main Import Index currently being used for serialization by CreateImports() (Defined in Linker.cpp) */
extern int32 GSerializedImportIndex;
/** Points to the main Linker currently being used for serialization by CreateImports() (Defined in Linker.cpp) */
extern ULinkerLoad* GSerializedImportLinker;
/** Points to the main UObject currently being serialized (Defined in Linker.cpp) */
extern UObject* GSerializedObject;

/** The most recently used export Index for serialization by CreateExport() */
static int32 GSerializedExportIndex = INDEX_NONE;
/** Points to the most recently used Linker for serialization by CreateExport() */
static ULinkerLoad*	GSerializedExportLinker = NULL;

/** Map that keeps track of any precached full package reads															*/
TMap<FString, ULinkerLoad::FPackagePrecacheInfo> ULinkerLoad::PackagePrecacheMap;

UClass* ULinkerLoad::UTexture2DStaticClass = NULL;

FName ULinkerLoad::NAME_LoadErrors("LoadErrors");

/**
 * Here is the format for the ClassRedirection:
 * 
 *  ; Basic redirects
 *  ;ActiveClassRedirects=(OldClassName="MyClass",NewClassName="NewNativePackage.MyClass")
 *	ActiveClassRedirects=(OldClassName="CylinderComponent",NewClassName="CapsuleComponent")
 *  Note: For class name redirects, the OldClassName must be the plain OldClassName, it cannot be OldPackage.OldClassName
 *
 *	; Keep both classes around, but convert any existing instances of that object to a particular class (insert into the inheritance hierarchy
 *	;ActiveClassRedirects=(OldClassName="MyClass",NewClassName="MyClassParent",InstanceOnly="true")
 *
 */


TMap<FName, FName> ULinkerLoad::ObjectNameRedirects;			    // OldClassName to NewClassName for ImportMap
TMap<FName, FName> ULinkerLoad::ObjectNameRedirectsInstanceOnly;	// OldClassName to NewClassName for ExportMap
TMap<FName, FName> ULinkerLoad::ObjectNameRedirectsObjectOnly;		// Object name to NewClassName for export map
TMap<FName, FName> ULinkerLoad::GameNameRedirects;					// Game package name to new game package name
TMap<FName, FName> ULinkerLoad::StructNameRedirects;				// Old struct name to new struct name mapping
TMap<FString, FString> ULinkerLoad::PluginNameRedirects;			// Old plugin name to new plugin name mapping
TMap<FName, ULinkerLoad::FSubobjectRedirect> ULinkerLoad::SubobjectNameRedirects;	

/*----------------------------------------------------------------------------
	Helpers
----------------------------------------------------------------------------*/

/**
 * Add redirects to ULinkerLoad static map
 */
void ULinkerLoad::CreateActiveRedirectsMap(const FString& GEngineIniName)
{		
	static bool bAlreadyInitialized_CreateActiveRedirectsMap = false;
	if (bAlreadyInitialized_CreateActiveRedirectsMap)
	{
		return;
	}
	else
	{
		bAlreadyInitialized_CreateActiveRedirectsMap = true;
	}

	if (GConfig)
	{
		FConfigSection* PackageRedirects = GConfig->GetSectionPrivate( TEXT("/Script/Engine.Engine"), false, true, GEngineIniName );
		for( FConfigSection::TIterator It(*PackageRedirects); It; ++It )
		{
			if( It.Key() == TEXT("ActiveClassRedirects") )
			{
				FName OldClassName = NAME_None;
				FName NewClassName = NAME_None;
				FName ObjectName = NAME_None;
				FName OldSubobjName = NAME_None;
				FName NewSubobjName = NAME_None;

				bool bInstanceOnly = false;

				FParse::Bool( *It.Value(), TEXT("InstanceOnly="), bInstanceOnly );
				FParse::Value( *It.Value(), TEXT("ObjectName="), ObjectName );

				FParse::Value( *It.Value(), TEXT("OldClassName="), OldClassName );
				FParse::Value( *It.Value(), TEXT("NewClassName="), NewClassName );

				FParse::Value( *It.Value(), TEXT("OldSubobjName="), OldSubobjName );
				FParse::Value( *It.Value(), TEXT("NewSubobjName="), NewSubobjName );

				if (NewSubobjName != NAME_None || OldSubobjName != NAME_None)
				{
					check(OldSubobjName != NAME_None && OldClassName != NAME_None );
					SubobjectNameRedirects.Add(OldSubobjName, FSubobjectRedirect(OldClassName, NewSubobjName));
				}
				//instances only
				else if( bInstanceOnly )
				{
					ObjectNameRedirectsInstanceOnly.Add(OldClassName,NewClassName);
				}
				//objects only on a per-object basis
				else if( ObjectName != NAME_None )
				{
					ObjectNameRedirectsObjectOnly.Add(ObjectName, NewClassName);
				}
				//full redirect
				else
				{
					if (NewClassName.ToString().Find(TEXT(".")) != NewClassName.ToString().Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd))
					{
						UE_LOG(LogLinker, Error, TEXT("Currently we cannot rename nested objects for '%s'; if you want to leave the outer alone, just specify the name with no path"), *NewClassName.ToString());
					}
					else
					{
						ObjectNameRedirects.Add(OldClassName,NewClassName);
					}
				}
			}	
			else if( It.Key() == TEXT("ActiveGameNameRedirects") )
			{
				FName OldGameName = NAME_None;
				FName NewGameName = NAME_None;

				FParse::Value( *It.Value(), TEXT("OldGameName="), OldGameName );
				FParse::Value( *It.Value(), TEXT("NewGameName="), NewGameName );

				GameNameRedirects.Add(OldGameName, NewGameName);
			}
			else if ( It.Key() == TEXT("ActiveStructRedirects") )
			{
				FName OldStructName = NAME_None;
				FName NewStructName = NAME_None;

				FParse::Value( *It.Value(), TEXT("OldStructName="), OldStructName );
				FParse::Value( *It.Value(), TEXT("NewStructName="), NewStructName );

				StructNameRedirects.Add(OldStructName, NewStructName);
			}
			else if ( It.Key() == TEXT("ActivePluginRedirects") )
			{
				FString OldPluginName;
				FString NewPluginName;

				FParse::Value( *It.Value(), TEXT("OldPluginName="), OldPluginName );
				FParse::Value( *It.Value(), TEXT("NewPluginName="), NewPluginName );

				OldPluginName = FString(TEXT("/")) + OldPluginName + FString(TEXT("/"));
				NewPluginName = FString(TEXT("/")) + NewPluginName + FString(TEXT("/"));

				PluginNameRedirects.Add(OldPluginName, NewPluginName);
			}
		}
	}
	else
	{
		UE_LOG(LogLinker, Warning, TEXT(" **** ACTIVE CLASS REDIRECTS UNABLE TO INITIALIZE! (mActiveClassRedirects) **** "));
	}
}

/** Helper struct to keep track of the first time CreateImport() is called in the current callstack. */
struct FScopedCreateImportCounter
{
	/**
	 *	Constructor. Called upon CreateImport() entry.
	 *	@param Linker	- Current Linker
	 *	@param Index	- Index of the current Import
	 */
	FScopedCreateImportCounter( ULinkerLoad* Linker, int32 Index )
	{
		// First time CreateImport() is called for this callstack?
		if ( Counter++ == 0 )
		{
			// Remember the current linker and index.
			GSerializedImportLinker = Linker;
			GSerializedImportIndex = Index;
		}
	}

	/** Destructor. Called upon CreateImport() exit. */
	~FScopedCreateImportCounter()
	{
		// Last time CreateImport() exits for this callstack?
		if ( --Counter == 0 )
		{
			GSerializedImportLinker = NULL;
			GSerializedImportIndex = INDEX_NONE;
		}
	}

	/** Number of times CreateImport() has been called in the current callstack. */
	static int32	Counter;
};

/** Number of times CreateImport() has been called in the current callstack. */
int32 FScopedCreateImportCounter::Counter = 0;

/** Helper struct to keep track of the CreateExport() entry/exit. */
struct FScopedCreateExportCounter
{
	/**
	 *	Constructor. Called upon CreateImport() entry.
	 *	@param Linker	- Current Linker
	 *	@param Index	- Index of the current Import
	 */
	FScopedCreateExportCounter( ULinkerLoad* Linker, int32 Index )
	{
		GSerializedExportLinker = Linker;
		GSerializedExportIndex = Index;
	}

	/** Destructor. Called upon CreateImport() exit. */
	~FScopedCreateExportCounter()
	{
		GSerializedExportLinker = NULL;
		GSerializedExportIndex = INDEX_NONE;
	}
};

/**
 * Exception save guard to ensure GSerializedPackageLinker is reset after this
 * class goes out of scope.
 */
class FSerializedPackageLinkerGuard
{
	/** Pointer to restore to after going out of scope. */
	ULinkerLoad* PrevSerializedPackageLinker;
public:
	FSerializedPackageLinkerGuard() 
	:	PrevSerializedPackageLinker(GSerializedPackageLinker) 
	{}
	~FSerializedPackageLinkerGuard() 
	{ 
		GSerializedPackageLinker = PrevSerializedPackageLinker; 
	}
};

namespace ULinkerDefs
{
	/** Number of progress steps for reporting status to a GUI while loading packages */
	const int32 TotalProgressSteps = 5;
}

/**
 * Creates a platform-specific ResourceMem. If an AsyncCounter is provided, it will allocate asynchronously.
 *
 * @param SizeX				Width of the stored largest mip-level
 * @param SizeY				Height of the stored largest mip-level
 * @param NumMips			Number of stored mips
 * @param TexCreateFlags	ETextureCreateFlags bit flags
 * @param AsyncCounter		If specified, starts an async allocation. If NULL, allocates memory immediately.
 * @return					Platform-specific ResourceMem.
 */
static FTexture2DResourceMem* CreateResourceMem(int32 SizeX, int32 SizeY, int32 NumMips, uint32 Format, uint32 TexCreateFlags, FThreadSafeCounter* AsyncCounter)
{
	FTexture2DResourceMem* ResourceMem = NULL;
	return ResourceMem;
}

/** 
 * Returns whether we should ignore the fact that this class has been removed instead of deprecated. 
 * Normally the script compiler would spit out an error but it makes sense to silently ingore it in 
 * certain cases in which case the below code should be extended to include the class' name.
 *
 * @param	ClassName	Name of class to find out whether we should ignore complaining about it not being present
 * @return	true if we should ignore the fact that it doesn't exist, false otherwise
 */
static bool IgnoreMissingReferencedClass( FName ClassName )
{
	static TArray<FName>	MissingClassesToIgnore;
	static bool			bAlreadyInitialized = false;
	if( !bAlreadyInitialized )
	{
		//@deprecated with VER_RENDERING_REFACTOR
		MissingClassesToIgnore.Add( FName(TEXT("SphericalHarmonicMap")) );
		MissingClassesToIgnore.Add( FName(TEXT("LightMap1D")) );
		MissingClassesToIgnore.Add( FName(TEXT("LightMap2D")) );
		bAlreadyInitialized = true;
	}
	return MissingClassesToIgnore.Find( ClassName ) != INDEX_NONE;
}

static inline int32 HashNames( FName A, FName B, FName C )
{
	return A.GetIndex() + 7 * B.GetIndex() + 31 * FPackageName::GetShortFName(C).GetIndex();
}

static FORCEINLINE bool IsCoreUObjectPackage(const FName& PackageName)
{
	return PackageName == NAME_CoreUObject || PackageName == GLongCoreUObjectPackageName || PackageName == NAME_Core || PackageName == GLongCorePackageName;
}

/*----------------------------------------------------------------------------
	ULinkerLoad.
----------------------------------------------------------------------------*/

/**
 * Fills in the passed in TArray with the packages that are in its PrecacheMap
 *
 * @param TArray<FString> to be populated
 */
void ULinkerLoad::GetListOfPackagesInPackagePrecacheMap( TArray<FString>& ListOfPackages )
{
	for ( TMap<FString, ULinkerLoad::FPackagePrecacheInfo>::TIterator It(PackagePrecacheMap); It; ++It )
	{
		ListOfPackages.Add( It.Key() );
	}
}

void ULinkerLoad::StaticInit(UClass* InUTexture2DStaticClass)
{
	UTexture2DStaticClass = InUTexture2DStaticClass;
}

/**
 * Creates and returns a ULinkerLoad object.
 *
 * @param	Parent		Parent object to load into, can be NULL (most likely case)
 * @param	Filename	Name of file on disk to load
 * @param	LoadFlags	Load flags determining behavior
 *
 * @return	new ULinkerLoad object for Parent/ Filename
 */
ULinkerLoad* ULinkerLoad::CreateLinker( UPackage* Parent, const TCHAR* Filename, uint32 LoadFlags )
{
	// This should not happen during async load, otherwise we're blocking async streaming to load a package
	// in the main thread.
	if (FPlatformProperties::RequiresCookedData() && GIsAsyncLoading)
	{
		UE_LOG(LogLinker, Warning, TEXT("ULinkerLoad::CreateLinker(%s) blocking async loading!"), Filename);
	}
	ULinkerLoad* Linker = CreateLinkerAsync( Parent, Filename, LoadFlags );
	{
		FSerializedPackageLinkerGuard Guard;	
		GSerializedPackageLinker = Linker;
		if (Linker->Tick( 0.f, false, false ) == LINKER_Failed)
		{
			return NULL;
		}
	}
	FCoreDelegates::PackageCreatedForLoad.Broadcast(Parent);
	return Linker;
}

/**
 * Looks for an existing linker for the given package, without trying to make one if it doesn't exist
 */
ULinkerLoad* ULinkerLoad::FindExistingLinkerForPackage(UPackage* Package)
{
	return GObjLoaders.FindRef(Package);
}

/**
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * CAUTION:  This function is potentially DANGEROUS.  Should only be used when you're really, really sure you know what you're doing.
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 * Replaces OldObject's entry in its linker with NewObject, so that all subsequent loads of OldObject will return NewObject.
 * This is used to update instanced components that were serialized out, but regenerated during compile-on-load
 *
 * OldObject will be consigned to oblivion, and NewObject will take its place.
 *
 * WARNING!!!	This function is potentially very dangerous!  It should only be used at very specific times, and in very specific cases.
 *				If you're unsure, DON'T TRY TO USE IT!!!
 */
void ULinkerLoad::PRIVATE_PatchNewObjectIntoExport(UObject* OldObject, UObject* NewObject)
{
	// Cache off the old object's linker and export index.  We'll slide the new object in here.
	ULinkerLoad* OldObjectLinker = OldObject->GetLinker();
	// if this thing doesn't have a linker, then it wasn't loaded off disk and all of this is moot
	if (OldObjectLinker)
	{
		const int32 CachedLinkerIndex = OldObject->GetLinkerIndex();
		FObjectExport& ObjExport = OldObjectLinker->ExportMap[CachedLinkerIndex];

		// Detach the old object to make room for the new
		OldObject->ClearFlags(RF_NeedLoad|RF_NeedPostLoad);
		OldObject->SetLinker(NULL, INDEX_NONE, true);

		// Move the new object into the old object's slot, so any references to this object will now reference the new
		NewObject->SetLinker(OldObjectLinker, CachedLinkerIndex);
		ObjExport.Object = NewObject;

		// If the object was in the GObjLoaded queue (exported, but not yet serialized), swap out for our new object
		int32 ObjLoadedIdx = GObjLoaded.Find(OldObject);
		if( ObjLoadedIdx != INDEX_NONE )
		{
			GObjLoaded[ObjLoadedIdx] = NewObject;
		}
	}
}

void ULinkerLoad::InvalidateExport(UObject* OldObject)
{
	// Cache off the old object's linker and export index.  We'll slide the new object in here.
	ULinkerLoad* OldObjectLinker = OldObject->GetLinker();
	const int32 CachedLinkerIndex = OldObject->GetLinkerIndex();

	if (OldObjectLinker && OldObjectLinker->ExportMap.IsValidIndex(CachedLinkerIndex))
	{
		FObjectExport& ObjExport = OldObjectLinker->ExportMap[CachedLinkerIndex];
		ObjExport.bExportLoadFailed = true;
	}
}

FName ULinkerLoad::FindSubobjectRedirectName(const FName& Name)
{
	FSubobjectRedirect *Redirect = SubobjectNameRedirects.Find(Name);
	if (Redirect)
	{
		return Redirect->NewName;
	}

	return FName();
}

/**
 * Creates a ULinkerLoad object for async creation. Tick has to be called manually till it returns
 * true in which case the returned linker object has finished the async creation process.
 *
 * @param	Parent		Parent object to load into, can be NULL (most likely case)
 * @param	Filename	Name of file on disk to load
 * @param	LoadFlags	Load flags determining behavior
 *
 * @return	new ULinkerLoad object for Parent/ Filename
 */
ULinkerLoad* ULinkerLoad::CreateLinkerAsync( UPackage* Parent, const TCHAR* Filename, uint32 LoadFlags )
{
	// See whether there already is a linker for this parent/ linker root.
	ULinkerLoad* Linker = FindExistingLinkerForPackage(Parent);
	if (Linker)
	{
		UE_LOG(LogStreaming, Log, TEXT("ULinkerLoad::CreateLinkerAsync: Found existing linker for '%s'"), *Parent->GetName());
	}

	// Create a new linker if there isn't an existing one.
	if( Linker == NULL )
	{
		if( GUseSeekFreeLoading )
		{
			LoadFlags |= LOAD_SeekFree;
		}
		Linker = new ULinkerLoad( FPostConstructInitializeProperties(), Parent, Filename, LoadFlags );
	}
	return Linker;
}

/**
 * Ticks an in-flight linker and spends InTimeLimit seconds on creation. This is a soft time limit used
 * if bInUseTimeLimit is true.
 *
 * @param	InTimeLimit		Soft time limit to use if bInUseTimeLimit is true
 * @param	bInUseTimeLimit	Whether to use a (soft) timelimit
 * @param	bInUseFullTimeLimit	Whether to use the entire time limit, even if blocked on I/O
 * 
 * @return	true if linker has finished creation, false if it is still in flight
 */
ULinkerLoad::ELinkerStatus ULinkerLoad::Tick( float InTimeLimit, bool bInUseTimeLimit, bool bInUseFullTimeLimit )
{
	ELinkerStatus Status = LINKER_Loaded;

	if( bHasFinishedInitialization == false )
	{
		// Store variables used by functions below.
		TickStartTime		= FPlatformTime::Seconds();
		bTimeLimitExceeded	= false;
		bUseTimeLimit		= bInUseTimeLimit;
		bUseFullTimeLimit	= bInUseFullTimeLimit;
		TimeLimit			= InTimeLimit;

		do
		{
			// Create loader, aka FArchive used for serialization and also precache the package file summary.
			// false is returned until any precaching is complete.
			if( true )
			{
				Status = CreateLoader();
			}

			// Serialize the package file summary and presize the various arrays (name, import & export map)
			if( Status == LINKER_Loaded )
			{
				Status = SerializePackageFileSummary();
			}

			// Serialize the name map and register the names.
			if( Status == LINKER_Loaded )
			{
				Status = SerializeNameMap();
			}

			// Serialize the import map.
			if( Status == LINKER_Loaded )
			{
				Status = SerializeImportMap();
			}

			// Serialize the export map.
			if( Status == LINKER_Loaded )
			{
				Status = SerializeExportMap();
			}

			// Start pre-allocation of texture memory.
			if( Status == LINKER_Loaded )
			{
#if WITH_ENGINE
				Status = StartTextureAllocation();
#endif		// WITH_ENGINE
			}

			// Fix up import map for backward compatible serialization.
			if( Status == LINKER_Loaded )
			{	
				Status = FixupImportMap();
			}

			if ( Status == LINKER_Loaded )
			{
				Status = RemapImports();
			}

			// Fix up export map for object class conversion 
			if( Status == LINKER_Loaded )
			{	
				Status = FixupExportMap();
			}

			// Serialize the dependency map.
			if( Status == LINKER_Loaded )
			{
				Status = SerializeDependsMap();
			}

			// Hash exports.
			if( Status == LINKER_Loaded )
			{
				Status = CreateExportHash();
			}

			// Find existing objects matching exports and associate them with this linker.
			if( Status == LINKER_Loaded )
			{
				Status = FindExistingExports();
			}

			// Finalize creation process.
			if( Status == LINKER_Loaded )
			{
				Status = FinalizeCreation();
			}
		}
		// Loop till we are done if no time limit is specified, or loop until the real time limit is up if we want to use full time
		while (Status == LINKER_TimedOut && 
			(!bUseTimeLimit || (bUseFullTimeLimit && !IsTimeLimitExceeded(TEXT("Checking Full Timer"))))
			);
	}

	// Return whether we completed or not.
	return Status;
}

/**
 * Private constructor, passing arguments through from CreateLinker.
 *
 * @param	Parent		Parent object to load into, can be NULL (most likely case)
 * @param	Filename	Name of file on disk to load
 * @param	LoadFlags	Load flags determining behavior
 */
ULinkerLoad::ULinkerLoad( const class FPostConstructInitializeProperties& PCIP, UPackage* InParent, const TCHAR* InFilename, uint32 InLoadFlags )
:	ULinker( PCIP, InParent, InFilename )
,	LoadFlags( InLoadFlags )
,	bHaveImportsBeenVerified( false )
{
	check(!HasAnyFlags(RF_ClassDefaultObject));
}

/**
 * Returns whether the time limit allotted has been exceeded, if enabled.
 *
 * @param CurrentTask	description of current task performed for logging spilling over time limit
 * @param Granularity	Granularity on which to check timing, useful in cases where FPlatformTime::Seconds is slow (e.g. PC)
 *
 * @return true if time limit has been exceeded (and is enabled), false otherwise (including if time limit is disabled)
 */
bool ULinkerLoad::IsTimeLimitExceeded( const TCHAR* CurrentTask, int32 Granularity )
{
	IsTimeLimitExceededCallCount++;
	if( !bTimeLimitExceeded 
	&&	bUseTimeLimit 
	&&  (IsTimeLimitExceededCallCount % Granularity) == 0 )
	{
		double CurrentTime = FPlatformTime::Seconds();
		bTimeLimitExceeded = CurrentTime - TickStartTime > TimeLimit;
		if (!FPlatformProperties::HasEditorOnlyData())
		{
			// Log single operations that take longer than timelimit.
			if( (CurrentTime - TickStartTime) > (2.5 * TimeLimit) )
			{
 				UE_LOG(LogStreaming, Log, TEXT("ULinkerLoad: %s took (less than) %5.2f ms"), 
 					CurrentTask, 
 					(CurrentTime - TickStartTime) * 1000);
			}
		}
	}
	return bTimeLimitExceeded;
}

void UpdateObjectLoadingStatusMessage()
{
#if WITH_EDITOR
	// Used to control animation of the load progress status updates.
	static int32 ProgressIterator = 3;

	// Time that progress was last updated
	static  double LastProgressUpdateTime = 0.0;

	const double UpdateDelta = 0.25;
	const double SlowLoadDelta = 2.0;
	FText StatusUpdate;

	// This can be a long operation so we will output some progress feedback to the 
	//  user in the form of 3 dots that animate between "." ".." "..."
	const double CurTime = FPlatformTime::Seconds();
	if ( CurTime - LastProgressUpdateTime > UpdateDelta )
	{
		if ( ProgressIterator == 1 )
		{
			StatusUpdate = NSLOCTEXT("Core", "LoadingRefObjectsMessageState1", "Loading.");
		}
		else if ( ProgressIterator == 2 )
		{
			StatusUpdate = NSLOCTEXT("Core", "LoadingRefObjectsMessageState2", "Loading..");
		}
		else if ( ProgressIterator == 3 )
		{
			StatusUpdate = NSLOCTEXT("Core", "LoadingRefObjectsMessageState3", "Loading...");
		}
		else
		{
			StatusUpdate = NSLOCTEXT("Core", "LoadingRefObjectsMessageState0", "Loading");
		}

		LastProgressUpdateTime = CurTime;
		ProgressIterator = (ProgressIterator + 1) % 4;
	}		

	GWarn->StatusUpdate( -1, -1, StatusUpdate );
#endif
}

/**
 * Creates loader used to serialize content.
 */
ULinkerLoad::ELinkerStatus ULinkerLoad::CreateLoader()
{
	CreateActiveRedirectsMap( GEngineIni );

	if( !Loader )
	{
		bool bIsSeekFree = LoadFlags & LOAD_SeekFree;

#if WITH_EDITOR
		if ((LoadFlags & ( LOAD_Quiet | LOAD_SeekFree ) ) == 0)
		{
			FString CleanFilename = FPaths::GetCleanFilename( *Filename );

			// We currently only allow status updates during the editor load splash screen.
			const bool bAllowStatusUpdate = GIsEditor && !IsRunningCommandlet() && !GIsSlowTask;
			if( bAllowStatusUpdate )
			{
				UpdateObjectLoadingStatusMessage();
			}
			else
			{
				if ( GIsSlowTask )
				{
					FFormatNamedArguments Args;
					Args.Add( TEXT("CleanFilename"), FText::FromString( CleanFilename ) );
					GWarn->StatusUpdate( 0, ULinkerDefs::TotalProgressSteps, FText::Format( NSLOCTEXT("Core", "Loading", "Loading file: {CleanFilename}..."), Args ) );
				}
			}
			GWarn->PushStatus();
		}
#endif

		// NOTE: Precached memory read gets highest priority, then memory reader, then seek free, then normal

		// check to see if there is was an async preload request for this file
		FPackagePrecacheInfo* PrecacheInfo = PackagePrecacheMap.Find(*Filename);
		// if so, serialize from memory (note this will have uncompressed a fully compressed package)
		if (PrecacheInfo)
		{
			// block until the async read is complete
			if( PrecacheInfo->SynchronizationObject->GetValue() != 0 )
			{
				double StartTime = FPlatformTime::Seconds();
				while (PrecacheInfo->SynchronizationObject->GetValue() != 0)
				{
					SHUTDOWN_IF_EXIT_REQUESTED;
					FPlatformProcess::Sleep(0);
				}
				float WaitTime = FPlatformTime::Seconds() - StartTime;
				UE_LOG(LogInit, Log, TEXT("Waited %.3f sec for async package '%s' to complete caching."), WaitTime, *Filename);
			}

			// create a buffer reader using the read in data
			// assume that all precached startup packages have SHA entries
			Loader = new FBufferReaderWithSHA(PrecacheInfo->PackageData, PrecacheInfo->PackageDataSize, true, *Filename, true);

			// remove the precache info from the map
			PackagePrecacheMap.Remove(*Filename);
		}
		else if ((LoadFlags & LOAD_MemoryReader) || !bIsSeekFree)
		{
			// Create file reader used for serialization.
			FArchive* FileReader = IFileManager::Get().CreateFileReader( *Filename, 0 );
			if( !FileReader )
			{
				UE_LOG(LogLinker, Warning, TEXT("Error opening file '%s'."), *Filename );
				return LINKER_Failed;
			}

			bool bHasHashEntry = FSHA1::GetFileSHAHash(*Filename, NULL);
			// force preload into memory if file has an SHA entry
			if( LoadFlags & LOAD_MemoryReader || 
				bHasHashEntry )
			{
					// Serialize data from memory instead of from disk.
					check(FileReader);
					uint32	BufferSize	= FileReader->TotalSize();
					void*	Buffer		= FMemory::Malloc( BufferSize );
					FileReader->Serialize( Buffer, BufferSize );
					if( bHasHashEntry )
					{
						// create buffer reader and spawn SHA verify when it gets closed
						Loader = new FBufferReaderWithSHA( Buffer, BufferSize, true, *Filename, true );
					}
					else
					{
						// create a buffer reader
						Loader = new FBufferReader( Buffer, BufferSize, true, true );
					}
					delete FileReader;
			}
			else
			{
				// read directly from file
				Loader = FileReader;
			}
		}
		else if (bIsSeekFree)
		{
			// Use the async archive as it supports proper Precache and package compression.
			Loader = new FArchiveAsync( *Filename );

			// An error signifies that the package couldn't be opened.
			if( Loader->IsError() )
			{
				delete Loader;
				UE_LOG(LogLinker, Warning, TEXT("Error opening file '%s'."), *Filename );
				return LINKER_Failed;
			}
		}
		check( Loader );
		check( !Loader->IsError() );

		if( ULinkerLoad::FindExistingLinkerForPackage(LinkerRoot) )
		{
			UE_LOG(LogLinker, Warning, TEXT("Linker for '%s' already exists"), *LinkerRoot->GetName() );
			return LINKER_Failed;
		}

		// Set status info.
		ArUE3Ver		= VER_LAST_ENGINE_UE3;
		ArUE4Ver		= GPackageFileUE4Version;
		ArLicenseeUE4Ver	= GPackageFileLicenseeUE4Version;
		ArIsLoading		= true;
		ArIsPersistent	= true;

		// Reset all custom versions
		ResetCustomVersions();

		if ((LoadFlags & ( LOAD_Quiet | LOAD_SeekFree ) ) == 0)
		{
			GWarn->UpdateProgress( 1, ULinkerDefs::TotalProgressSteps );
		}
	}

	bool bExecuteNextStep = true;
	if( bHasSerializedPackageFileSummary == false )
	{
		// Precache up to one ECC block before serializing package file summary.
		// If the package is partially compressed, we'll know that quickly and
		// end up discarding some of the precached data so we can re-fetch
		// and decompress it.
		static int64 MinimumReadSize = 32 * 1024;
		checkSlow(MinimumReadSize >= 2048 && MinimumReadSize <= 1024 * 1024); // not a hard limit, but we should be loading at least a reasonable amount of data
		int32 PrecacheSize = FMath::Min( MinimumReadSize, Loader->TotalSize() );
		check( PrecacheSize > 0 );
		// Wait till we're finished precaching before executing the next step.
		bExecuteNextStep = Loader->Precache( 0, PrecacheSize);
	}

	return (bExecuteNextStep && !IsTimeLimitExceeded( TEXT("creating loader") )) ? LINKER_Loaded : LINKER_TimedOut;
}

/**
 * Serializes the package file summary.
 */
ULinkerLoad::ELinkerStatus ULinkerLoad::SerializePackageFileSummary()
{
	if( bHasSerializedPackageFileSummary == false )
	{
		// Read summary from file.
		*this << Summary;

		// Check tag.
		if( Summary.Tag != PACKAGE_FILE_TAG )
		{
			UE_LOG(LogLinker, Warning, TEXT("The file '%s' contains unrecognizable data, check that it is of the expected type."), *Filename );
			return LINKER_Failed;
		}

		// Validate the summary.
		if( Summary.GetFileVersionUE3() < VER_MIN_ENGINE_UE3 || Summary.GetFileVersionUE4() < VER_UE4_OLDEST_LOADABLE_PACKAGE)
		{
			UE_LOG(LogLinker, Warning, TEXT("The file %s was saved by a previous version which is not backwards compatible with this one. Min Required Version: %i  Package Version: %i"), *Filename, (int32)VER_UE4_OLDEST_LOADABLE_PACKAGE, Summary.GetFileVersionUE4() );
			return LINKER_Failed;
		}

		// Don't load packages that were saved engine version newer than the current one.
		if( !GEngineVersion.IsCompatibleWith(Summary.EngineVersion) )
		{
			UE_LOG(LogLinker, Warning, TEXT("Asset '%s' has been saved with engine version newer than current and therefore can't be loaded. CurrEngineVersion: %s AssetEngineVersion: %s"), *Filename, *GEngineVersion.ToString(), *Summary.EngineVersion.ToString() );
			return LINKER_Failed;
		}
		else if( !FPlatformProperties::RequiresCookedData() && !Summary.EngineVersion.IsPromotedBuild() && GEngineVersion.IsPromotedBuild() )
		{
			// This warning can be disabled in ini with [Core.System] ZeroEngineVersionWarning=False
			static struct FInitZeroEngineVersionWarning
			{
				bool bDoWarn;
				FInitZeroEngineVersionWarning()
				{
					if (!GConfig->GetBool(TEXT("Core.System"), TEXT("ZeroEngineVersionWarning"), bDoWarn, GEngineIni))
					{
						bDoWarn = true;
					}
				}
				FORCEINLINE operator bool() const { return bDoWarn; }
			} ZeroEngineVersionWarningEnabled;			
			UE_CLOG(ZeroEngineVersionWarningEnabled, LogLinker, Warning, TEXT("Asset '%s' has been saved with empty engine version. The asset will be loaded but may be incompatible."), *Filename );
		}

		// Don't load packages that were saved with package version newer than the current one.
		if( (Summary.GetFileVersionUE3() > VER_LAST_ENGINE_UE3) || (Summary.GetFileVersionUE4() > GPackageFileUE4Version) || (Summary.GetFileVersionLicenseeUE4() > GPackageFileLicenseeUE4Version) )
		{
			UE_LOG(LogLinker, Warning, TEXT("Unable to load package (%s) PackageVersion %i, MaxExpected %i : UE4PackageVersion %i, MaxExpected %i : LicenseePackageVersion %i, MaxExpected %i."), *Filename, Summary.GetFileVersionUE3(), (int32)VER_LAST_ENGINE_UE3, Summary.GetFileVersionUE4(), GPackageFileUE4Version, Summary.GetFileVersionLicenseeUE4(), GPackageFileLicenseeUE4Version );
			return LINKER_Failed;
		}

		// don't load packages that contain editor only data in builds that don't support that and vise versa
		if ( (!FPlatformProperties::HasEditorOnlyData() && !(Summary.PackageFlags & PKG_FilterEditorOnly)) ||
			 (FPlatformProperties::HasEditorOnlyData() && !!(Summary.PackageFlags & PKG_FilterEditorOnly)) )
		{
			UE_LOG(LogLinker, Warning, TEXT("Unable to load package (%s). Package contains EditorOnly data which is not supported by the current build or vice versa."), *Filename );
			return LINKER_Failed;
		}

#if PLATFORM_WINDOWS
		// check if this package version stored the 4-byte magic post tag
		if (Summary.GetFileVersionUE4() >= VER_UE4_PACKAGE_MAGIC_POSTTAG)
		{
			// get the offset of the post tag
			int64 MagicOffset = TotalSize() - sizeof(uint32);
			// store the current file offset
			int64 OriginalOffset = Tell();
			
			uint32 Tag = 0;
			
			// seek to the post tag and serialize it
			Seek(MagicOffset);
			*this << Tag;

			if (Tag != PACKAGE_FILE_TAG)
			{
				UE_LOG(LogLinker, Warning, TEXT("Unable to load package (%s). Post Tag is not valid. File might be corrupted."), *Filename );
				return LINKER_Failed;
			}

			// seek back to the position after the package summary
			Seek(OriginalOffset);
		}
#endif // PLATFORM_WINDOWS

		// Check custom versions.
		const FCustomVersionContainer& LatestCustomVersions  = FCustomVersionContainer::GetRegistered();
		const TArray<FCustomVersion>&  PackageCustomVersions = Summary.GetCustomVersionContainer().GetAllVersions();
		for (auto It = PackageCustomVersions.CreateConstIterator(); It; ++It)
		{
			const FCustomVersion& SerializedCustomVersion = *It;

			auto* LatestVersion = LatestCustomVersions.GetVersion(SerializedCustomVersion.Key);
			if (!LatestVersion)
			{
				// Loading a package with custom integration that we don't know about!
				// Temporarily just warn and continue. @todo: this needs to be fixed properly
				UE_LOG(LogLinker, Warning, TEXT("Package %s was saved with a custom integration that is not present. Tag %s  Version %d"), *Filename, *SerializedCustomVersion.Key.ToString(), SerializedCustomVersion.Version);
			}
			else if (SerializedCustomVersion.Version > LatestVersion->Version)
			{
				// Loading a package with a newer custom version than the current one.
				UE_LOG(LogLinker, Error, TEXT("Package %s was saved with a newer custom version than the current. Tag %s  PackageVersion %d  MaxExpected %d"), *Filename, *SerializedCustomVersion.Key.ToString(), SerializedCustomVersion.Version, LatestVersion->Version);
				return LINKER_Failed;
			}
		}

		// Loader needs to be the same version.
		Loader->SetUE3Ver(Summary.GetFileVersionUE3());
		Loader->SetUE4Ver(Summary.GetFileVersionUE4());
		Loader->SetLicenseeUE4Ver(Summary.GetFileVersionLicenseeUE4());

		ArUE3Ver = Summary.GetFileVersionUE3();
		ArUE4Ver = Summary.GetFileVersionUE4();
		ArLicenseeUE4Ver = Summary.GetFileVersionLicenseeUE4();

		const FCustomVersionContainer& SummaryVersions = Summary.GetCustomVersionContainer();
		Loader->SetCustomVersions(SummaryVersions);
		SetCustomVersions(SummaryVersions);

		// Package has been stored compressed.
#if DEBUG_DISTRIBUTED_COOKING
		if( false )
#else
		if( Summary.PackageFlags & PKG_StoreCompressed )
#endif
		{
			// Set compression mapping. Failure means Loader doesn't support package compression.
			check( Summary.CompressedChunks.Num() );
			if( !Loader->SetCompressionMap( &Summary.CompressedChunks, (ECompressionFlags) Summary.CompressionFlags ) )
			{
				// Current loader doesn't support it, so we need to switch to one known to support it.
				
				// We need keep track of current position as we already serialized the package file summary.
				int32		CurrentPos				= Loader->Tell();
				// Serializing the package file summary determines whether we are forcefully swapping bytes
				// so we need to propage this information from the old loader to the new one.
				bool	bHasForcedByteSwapping	= Loader->ForceByteSwapping();

				// Delete existing loader...
				delete Loader;
				// ... and create new one using FArchiveAsync as it supports package compression.
				Loader = new FArchiveAsync( *Filename );
				check( !Loader->IsError() );

				// Seek to current position as package file summary doesn't need to be serialized again.
				Loader->Seek( CurrentPos );
				// Propagate byte-swapping behavior.
				Loader->SetByteSwapping( bHasForcedByteSwapping );
				
				// Set the compression map and verify it won't fail this time.
				verify( Loader->SetCompressionMap( &Summary.CompressedChunks, (ECompressionFlags) Summary.CompressionFlags ) );
			}
		}

		UPackage* LinkerRootPackage = LinkerRoot;
		if( LinkerRootPackage )
		{
			// Preserve PIE package flag
			uint32 PIEFlag = (LinkerRootPackage->PackageFlags & PKG_PlayInEditor);
			
			// Propagate package flags
			LinkerRootPackage->PackageFlags = (Summary.PackageFlags | PIEFlag);

			// Propagate package folder name
			LinkerRootPackage->SetFolderName(*Summary.FolderName);

			// Propagate streaming install ChunkID
			LinkerRootPackage->SetChunkIDs(Summary.ChunkIDs);
			
			// Propagate package file size
			LinkerRootPackage->FileSize = TotalSize();
		}
		
		// Propagate fact that package cannot use lazy loading to archive (aka this).
		if( (Summary.PackageFlags & PKG_DisallowLazyLoading) )
		{
			ArAllowLazyLoading = false;
		}
		else
		{
			ArAllowLazyLoading = true;
		}

		// Slack everything according to summary.
		ImportMap   .Empty( Summary.ImportCount   );
		ExportMap   .Empty( Summary.ExportCount   );
		NameMap		.Empty( Summary.NameCount     );
		// Depends map gets pre-sized in SerializeDependsMap if used.

		// Avoid serializing it again.
		bHasSerializedPackageFileSummary = true;

		if ((LoadFlags & ( LOAD_Quiet | LOAD_SeekFree ) ) == 0)
		{
			GWarn->UpdateProgress( 2, ULinkerDefs::TotalProgressSteps );
		}
	}

	return !IsTimeLimitExceeded( TEXT("serializing package file summary") ) ? LINKER_Loaded : LINKER_TimedOut;
}

/**
 * Serializes the name table.
 */
ULinkerLoad::ELinkerStatus ULinkerLoad::SerializeNameMap()
{
	// The name map is the first item serialized. We wait till all the header information is read
	// before any serialization. @todo async, @todo seamless: this could be spread out across name,
	// import and export maps if the package file summary contained more detailed information on
	// serialized size of individual entries.
	bool bFinishedPrecaching = true;

	if( NameMapIndex == 0 && Summary.NameCount > 0 )
	{
		Seek( Summary.NameOffset );
		// Make sure there is something to precache first.
		if( Summary.TotalHeaderSize > 0 )
		{
			// Precache name, import and export map.
			bFinishedPrecaching = Loader->Precache( Summary.NameOffset, Summary.TotalHeaderSize - Summary.NameOffset );
		}
		// Backward compat code for VER_MOVED_EXPORTIMPORTMAPS_ADDED_TOTALHEADERSIZE.
		else
		{
			bFinishedPrecaching = true;
		}
	}

	while( bFinishedPrecaching && NameMapIndex < Summary.NameCount && !IsTimeLimitExceeded(TEXT("serializing name map"),100) )
	{
		// Read the name entry from the file.
		FNameEntry NameEntry(ENAME_LinkerConstructor);
		*this << NameEntry;

		// Add it to the name table. We disregard the context flags as we don't support flags on names for final release builds.

		// now, we make sure we DO NOT split the name here because it will have been written out
		// split, and we don't want to keep splitting A_3_4_9 every time

		NameMap.Add( 
			NameEntry.IsWide() ? 
				FName(ENAME_LinkerConstructor, NameEntry.GetWideName()) : 
				FName(ENAME_LinkerConstructor, NameEntry.GetAnsiName())
			);
		NameMapIndex++;
	}

	// Return whether we finished this step and it's safe to start with the next.
	return ((NameMapIndex == Summary.NameCount) && !IsTimeLimitExceeded( TEXT("serializing name map") )) ? LINKER_Loaded : LINKER_TimedOut;
}

/**
 * Serializes the import map.
 */
ULinkerLoad::ELinkerStatus ULinkerLoad::SerializeImportMap()
{
	if( ImportMapIndex == 0 && Summary.ImportCount > 0 )
	{
		Seek( Summary.ImportOffset );
	}

	while( ImportMapIndex < Summary.ImportCount && !IsTimeLimitExceeded(TEXT("serializing import map"),100) )
	{
		FObjectImport* Import = new(ImportMap)FObjectImport;
		*this << *Import;
		ImportMapIndex++;
	}
	
	// Return whether we finished this step and it's safe to start with the next.
	return ((ImportMapIndex == Summary.ImportCount) && !IsTimeLimitExceeded( TEXT("serializing import map") )) ? LINKER_Loaded : LINKER_TimedOut;
}

/**
 * Fixes up the import map, performing remapping for backward compatibility and such.
 */
ULinkerLoad::ELinkerStatus ULinkerLoad::FixupImportMap()
{
	if( bHasFixedUpImportMap == false )
	{
		// Fix up imports, not required if everything is cooked.
		if (!FPlatformProperties::RequiresCookedData())
		{
			bool bDone = false;
			while (!bDone)
			{
				bDone = true;
				for( int32 i=0; i<ImportMap.Num(); i++ )
				{
					FObjectImport& Import = ImportMap[i];
					{
						FSubobjectRedirect *Redirect = SubobjectNameRedirects.Find(Import.ObjectName);
						if (Redirect)
						{
							if (Import.ClassName == Redirect->MatchClass)
							{
								if (!Import.OuterIndex.IsNull())
								{
									FString Was = GetImportFullName(i);
									Import.ObjectName = Redirect->NewName;

									if (Import.ObjectName != NAME_None)
									{
										FString Now = GetImportFullName(i);
										UE_LOG(LogLinker, Verbose, TEXT("ULinkerLoad::FixupImportMap() - Renamed component from %s   to   %s"), *Was, *Now);
									}
									else
									{
										UE_LOG(LogLinker, Verbose, TEXT("ULinkerLoad::FixupImportMap() - Removed component %s"), *Was);
									}
									
									bDone = false;
									continue;
								}
							}
						}
					}

					static FName NAME_ScriptStruct(TEXT("ScriptStruct"));

					bool bIsClass = Import.ClassName == NAME_Class;
					bool bIsStruct = Import.ClassName == NAME_ScriptStruct;
					bool bIsEnum = Import.ClassName == NAME_Enum;
					bool bIsClassOrStructOrEnum = bIsClass || bIsStruct || bIsEnum;

					FString RedirectName, ResultPackage, ResultClass;
					FName* RedirectNameObj = ObjectNameRedirects.Find(Import.ObjectName);
					FName* RedirectNameClass = ObjectNameRedirects.Find(Import.ClassName);
					int32 OldOuterIndex = 0;
					if ( (RedirectNameObj && bIsClassOrStructOrEnum) || RedirectNameClass )
					{
						FString NewDefaultObjectName = Import.ObjectName.ToString();
						FObjectImport OldImport = Import;
						bool bUpdateOuterIndex = false;
						int32 ImportPackage = -1;

						// We are dealing with an object that needs to be redirected to a new classname (possibly a new package as well)

						FString stringObjectName(FString(Import.ObjectName.ToString()));
						if ( RedirectNameClass )
						{
							// This is an object instance
							RedirectName = RedirectNameClass->ToString();
						}
						else if ( RedirectNameObj && bIsClassOrStructOrEnum )
						{
							// This is a class object (needs to have its OuterIndex changed if the package is different)
							bUpdateOuterIndex = true;
							RedirectName = RedirectNameObj->ToString();
						}

						// Accepts either "PackageName.ClassName" or just "ClassName"
						int32 Offset = RedirectName.Find(TEXT("."));
						if ( Offset >= 0 )
						{
							// A package class name redirect
							ResultPackage = RedirectName.Left(Offset);
							ResultClass = RedirectName.Right(RedirectName.Len() - Offset - 1);
						}
						else
						{
							// Just a class name change within the same package
							ResultPackage = Import.ClassPackage.ToString();
							ResultClass = RedirectName;
							bUpdateOuterIndex = false;
						}

						// Find the OuterIndex of the current package for the Import
						for ( int32 ImportIndex = 0; ImportIndex < ImportMap.Num(); ImportIndex++ )
						{
							if ( ImportMap[ImportIndex].ClassName == NAME_Package && ImportMap[ImportIndex].ObjectName == Import.ClassPackage )
							{
								OldOuterIndex = ImportIndex;
								break;
							}
						}
						if ( !Import.OuterIndex.IsNull() && Import.OuterIndex == FPackageIndex::FromImport(OldOuterIndex) )
						{
							// This is a object instance that is owned by a specific package (default class instance or an archetype etc)
							// (needs its OuterIndex changed if the package is different)
							if(ResultPackage != Import.ClassPackage.ToString())
							{
								bUpdateOuterIndex = true;
							}					
						}

						if ( bUpdateOuterIndex && ResultPackage.Len() > 0 )
						{
							// Reset the Import.OuterIndex to the package it is intended to be in
							for ( int32 ImportIndex = 0; ImportIndex < ImportMap.Num(); ImportIndex++ )
							{
								if ( ImportMap[ImportIndex].ClassName == NAME_Package && ImportMap[ImportIndex].ObjectName == FName(*ResultPackage) )
								{
									ImportPackage = ImportIndex;
									break;
								}
							}
							if (ImportPackage == -1 && !IsCoreUObjectPackage(FName(*ResultPackage)))
							{
								// We are adding a new import to the map as we need the new package dependency added to the works
								ImportMap.AddUninitialized();
								ImportMap[ImportMap.Num()-1].ClassName = NAME_Package;
								ImportMap[ImportMap.Num()-1].ClassPackage = GLongCoreUObjectPackageName;
								ImportMap[ImportMap.Num()-1].ObjectName = FName(*ResultPackage);
								ImportMap[ImportMap.Num()-1].OuterIndex = FPackageIndex();
								ImportMap[ImportMap.Num()-1].XObject = 0;
								ImportMap[ImportMap.Num()-1].SourceLinker = 0;
								ImportMap[ImportMap.Num()-1].SourceIndex = -1;
								ImportPackage = ImportMap.Num() - 1;

								// Since this destroys the array, the current Import object is invalid and we must restart the whole process again
								bDone = false;
								break;
							}

							// Assign the new OuterIndex for a default object instance or a class itself
							if ( ImportPackage != -1 )
							{
								Import.OuterIndex = FPackageIndex::FromImport(ImportPackage);
							}
						}

						if ( RedirectNameClass )
						{
							// Changing the package and class name of an object instance
							Import.ClassPackage = *ResultPackage;
#if WITH_EDITOR
							Import.OldClassName = Import.ClassName;
#endif
							Import.ClassName = *ResultClass;
						}

						if ( RedirectNameObj && bIsClassOrStructOrEnum )
						{
							// Changing the object name of a class object
#if WITH_EDITOR
							Import.OldClassName = Import.ObjectName;
#endif
							Import.ObjectName = *ResultClass;							
						}

						// Default objects should be converted by name as well
						if ( NewDefaultObjectName.Left(9) == FString("Default__") )
						{
							NewDefaultObjectName = FString("Default__");
							NewDefaultObjectName += *ResultClass;
							Import.ObjectName = *NewDefaultObjectName;
						}

						// Log the object redirection to the console for review
						if ( OldImport.ObjectName != Import.ObjectName || OldImport.ClassName != Import.ClassName || OldImport.ClassPackage != Import.ClassPackage || OldImport.OuterIndex != Import.OuterIndex )
						{
							UE_LOG(LogLinker, Verbose, TEXT("ULinkerLoad::FixupImportMap() - Pkg<%s> [Obj<%s> Cls<%s> Pkg<%s> Out<%s>] -> [Obj<%s> Cls<%s> Pkg<%s> Out<%s>]"), *LinkerRoot->GetName(),
								*OldImport.ObjectName.ToString(), *OldImport.ClassName.ToString(), *OldImport.ClassPackage.ToString(), OldImport.OuterIndex.IsImport() ? *Imp(OldImport.OuterIndex).ObjectName.ToString() : TEXT("None"),
								*Import.ObjectName.ToString(), *Import.ClassName.ToString(), *Import.ClassPackage.ToString(),	Import.OuterIndex.IsImport() ? *Imp(Import.OuterIndex).ObjectName.ToString() : TEXT("None"));
						}
					}
				}
			}
		}
		// Avoid duplicate work in async case.
		bHasFixedUpImportMap = true;

		if ((LoadFlags & ( LOAD_Quiet | LOAD_SeekFree ) ) == 0)
		{
			GWarn->UpdateProgress( 3, ULinkerDefs::TotalProgressSteps );
		}
	}
	return IsTimeLimitExceeded( TEXT("fixing up import map") ) ? LINKER_TimedOut : LINKER_Loaded;
}

/**
 * Serializes the export map.
 */
ULinkerLoad::ELinkerStatus ULinkerLoad::SerializeExportMap()
{
	if( ExportMapIndex == 0 && Summary.ExportCount > 0 )
	{
		Seek( Summary.ExportOffset );
	}

	while( ExportMapIndex < Summary.ExportCount && !IsTimeLimitExceeded(TEXT("serializing export map"),100) )
	{
		FObjectExport* Export = new(ExportMap)FObjectExport;
		*this << *Export;
		ExportMapIndex++;
	}

	// Return whether we finished this step and it's safe to start with the next.
	return ((ExportMapIndex == Summary.ExportCount) && !IsTimeLimitExceeded( TEXT("serializing export map") )) ? LINKER_Loaded : LINKER_TimedOut;
}

ULinkerLoad::ELinkerStatus ULinkerLoad::RemapImports()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	for (int32 ImportIndex = 0; ImportIndex < ImportMap.Num(); ImportIndex++)
	{
		FObjectImport& Import = ImportMap[ImportIndex];

		FName* RedirectPackageName = GameNameRedirects.Find(Import.ClassPackage);
		if (RedirectPackageName != NULL)
		{
			Import.ClassPackage = *RedirectPackageName;
		}
		if (Import.ClassName == NAME_Package)
		{
			RedirectPackageName = GameNameRedirects.Find(Import.ObjectName);
			if (RedirectPackageName != NULL)
			{
				Import.ObjectName = *RedirectPackageName;
			}

			for ( TMap<FString, FString>::TConstIterator PluginRedirectIt = PluginNameRedirects.CreateConstIterator(); PluginRedirectIt; ++PluginRedirectIt )
			{
				if ( Import.ObjectName.ToString().StartsWith( PluginRedirectIt.Key() ) )
				{
					const FString NewPath = PluginRedirectIt.Value() + Import.ObjectName.ToString().RightChop(PluginRedirectIt.Key().Len());
					Import.ObjectName = FName(*NewPath);
				}
			}
		}
	}

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	return LINKER_Loaded;
}

#if WITH_ENGINE
/**
 * Kicks off async memory allocations for all textures that will be loaded from this package.
 */
ULinkerLoad::ELinkerStatus ULinkerLoad::StartTextureAllocation()
{
	double StartTime = FPlatformTime::Seconds();
	int32 NumAllocationsStarted = 0;
	int32 NumAllocationsConsidered = 0;

	// Only kick off async allocation if the loader is async.
	bool bIsDone = true;
	if ( bUseTimeLimit && !Summary.TextureAllocations.HaveAllAllocationsBeenConsidered() )
	{
		bool bContinue = true;
		for ( int32 TypeIndex=Summary.TextureAllocations.NumTextureTypesConsidered;
			  TypeIndex < Summary.TextureAllocations.TextureTypes.Num() && bContinue;
			  ++TypeIndex )
		{
			FTextureAllocations::FTextureType& TextureType = Summary.TextureAllocations.TextureTypes[ TypeIndex ];
			for ( int32 ResourceIndex=TextureType.NumExportIndicesProcessed; ResourceIndex < TextureType.ExportIndices.Num() && bContinue; ++ResourceIndex )
			{
				int32 ExportIndex = TextureType.ExportIndices[ ResourceIndex ];
				if ( WillTextureBeLoaded( UTexture2DStaticClass, ExportIndex ) )
				{
					FTexture2DResourceMem* ResourceMem = CreateResourceMem(
						TextureType.SizeX,
						TextureType.SizeY,
						TextureType.NumMips,
						TextureType.Format,
						TextureType.TexCreateFlags,
						&Summary.TextureAllocations.PendingAllocationCount );

					if ( ResourceMem )
					{
						TextureType.Allocations.Add( ResourceMem );
						Summary.TextureAllocations.PendingAllocationSize += ResourceMem->GetResourceBulkDataSize();
						Summary.TextureAllocations.PendingAllocationCount.Increment();
						NumAllocationsStarted++;
					}
				}

				TextureType.NumExportIndicesProcessed++;
				NumAllocationsConsidered++;

				bContinue = !IsTimeLimitExceeded( TEXT("allocating texture memory") );
			}

			// Have we processed all potential allocations for this texture type yet?
			if ( TextureType.HaveAllAllocationsBeenConsidered() )
			{
				Summary.TextureAllocations.NumTextureTypesConsidered++;
			}
		}
		bIsDone = Summary.TextureAllocations.HaveAllAllocationsBeenConsidered();
	}

	double Duration = FPlatformTime::Seconds() - StartTime;

	// For profiling:
// 	if ( NumAllocationsStarted )
// 	{
// 		UE_LOG(LogLinker, Log,  TEXT("StartTextureAllocation duration: %.3f ms (%d textures allocated, %d textures considered)"), Duration*1000.0, NumAllocationsStarted, NumAllocationsConsidered );
// 	}

	return (bIsDone && !IsTimeLimitExceeded( TEXT("kicking off texture allocations") )) ? LINKER_Loaded : LINKER_TimedOut;
}
#endif // WITH_ENGINE

/**
 * Serializes the depends map.
 */
ULinkerLoad::ELinkerStatus ULinkerLoad::SerializeDependsMap()
{
	// Skip serializing depends map if we are using seekfree loading
	if( FPlatformProperties::RequiresCookedData() 
	// or we are neither Editor nor commandlet
	|| !(GIsEditor || IsRunningCommandlet()) )
	{
		return LINKER_Loaded;
	}

	// depends map size is same as export map size
	if (DependsMapIndex == 0 && Summary.ExportCount > 0)
	{
		Seek(Summary.DependsOffset);

		// Pre-size array to avoid re-allocation of array of arrays!
		DependsMap.AddZeroed(Summary.ExportCount);
	}

	while (DependsMapIndex < Summary.ExportCount && !IsTimeLimitExceeded(TEXT("serializing depends map"), 100))
	{
		TArray<FPackageIndex>& Depends = DependsMap[DependsMapIndex];
		*this << Depends;
		DependsMapIndex++;
	}
	
	// Return whether we finished this step and it's safe to start with the next.
	return ((DependsMapIndex == Summary.ExportCount) && !IsTimeLimitExceeded( TEXT("serializing depends map") )) ? LINKER_Loaded : LINKER_TimedOut;
}

/**
 * Serializes thumbnails
 */
ULinkerLoad::ELinkerStatus ULinkerLoad::SerializeThumbnails( bool bForceEnableInGame/*=false*/ )
{
#if WITH_EDITORONLY_DATA
	// Skip serializing thumbnails if we are using seekfree loading
	if( !bForceEnableInGame && !GIsEditor )
	{
		return LINKER_Loaded;
	}

	if( Summary.ThumbnailTableOffset > 0 )
	{
		// Seek to the thumbnail table of contents
		Seek( Summary.ThumbnailTableOffset );


		// Load number of thumbnails
		int32 ThumbnailCount = 0;
		*this << ThumbnailCount;


		// Allocate a new thumbnail map if we need one
		if( !LinkerRoot->ThumbnailMap.IsValid() )
		{
			LinkerRoot->ThumbnailMap.Reset( new FThumbnailMap() );
		}


		// Load thumbnail names and file offsets
		TArray< FObjectFullNameAndThumbnail > ThumbnailInfoArray;
		for( int32 CurObjectIndex = 0; CurObjectIndex < ThumbnailCount; ++CurObjectIndex )
		{
			FObjectFullNameAndThumbnail ThumbnailInfo;

			FString ObjectClassName;
				// Newer packages always store the class name for each asset
				*this << ObjectClassName;

			// Object path
			FString ObjectPathWithoutPackageName;
			*this << ObjectPathWithoutPackageName;
			const FString ObjectPath( LinkerRoot->GetName() + TEXT( "." ) + ObjectPathWithoutPackageName );


			// Create a full name string with the object's class and fully qualified path
			const FString ObjectFullName( ObjectClassName + TEXT( " " ) + ObjectPath );
			ThumbnailInfo.ObjectFullName = FName( *ObjectFullName );

			// File offset for the thumbnail (already saved out.)
			*this << ThumbnailInfo.FileOffset;

			// Only bother loading thumbnails that don't already exist in memory yet.  This is because when we
			// go to load thumbnails that aren't in memory yet when saving packages we don't want to clobber
			// thumbnails that were freshly-generated during that editor session
			if( !LinkerRoot->ThumbnailMap->Contains( ThumbnailInfo.ObjectFullName ) )
			{
				// Add to list of thumbnails to load
				ThumbnailInfoArray.Add( ThumbnailInfo );
			}
		}



		// Now go and load and cache all of the thumbnails
		for( int32 CurObjectIndex = 0; CurObjectIndex < ThumbnailInfoArray.Num(); ++CurObjectIndex )
		{
			const FObjectFullNameAndThumbnail& CurThumbnailInfo = ThumbnailInfoArray[ CurObjectIndex ];


			// Seek to the location in the file with the image data
			Seek( CurThumbnailInfo.FileOffset );

			// Load the image data
			FObjectThumbnail LoadedThumbnail;
			LoadedThumbnail.Serialize( *this );

			// Store the data!
			LinkerRoot->ThumbnailMap->Add( CurThumbnailInfo.ObjectFullName, LoadedThumbnail );
		}
	}
#endif // WITH_EDITORONLY_DATA

	// Finished!
	return LINKER_Loaded;
}



/** 
 * Creates the export hash. This relies on the import and export maps having already been serialized.
 */
ULinkerLoad::ELinkerStatus ULinkerLoad::CreateExportHash()
{
	// Zero initialize hash on first iteration.
	if( ExportHashIndex == 0 )
	{
		for( int32 i=0; i<ARRAY_COUNT(ExportHash); i++ )
		{
			ExportHash[i] = INDEX_NONE;
		}
	}

	// Set up export hash, potentially spread across several frames.
	while( ExportHashIndex < ExportMap.Num() && !IsTimeLimitExceeded(TEXT("creating export hash"),100) )
	{
		FObjectExport& Export = ExportMap[ExportHashIndex];

		const int32 iHash = HashNames( Export.ObjectName, GetExportClassName(ExportHashIndex), GetExportClassPackage(ExportHashIndex) ) & (ARRAY_COUNT(ExportHash)-1);
		Export.HashNext = ExportHash[iHash];
		ExportHash[iHash] = ExportHashIndex;

		ExportHashIndex++;
	}

	// Return whether we finished this step and it's safe to start with the next.
	return ((ExportHashIndex == ExportMap.Num()) && !IsTimeLimitExceeded( TEXT("creating export hash") )) ? LINKER_Loaded : LINKER_TimedOut;
}

/**
 * Finds existing exports in memory and matches them up with this linker. This is required for PIE to work correctly
 * and also for script compilation as saving a package will reset its linker and loading will reload/ replace existing
 * objects without a linker.
 */
ULinkerLoad::ELinkerStatus ULinkerLoad::FindExistingExports()
{
	if( bHasFoundExistingExports == false )
	{
		// only look for existing exports in the editor after it has started up
#if WITH_EDITOR
		if( GIsEditor && GIsRunning )
		{
			// Hunt down any existing objects and hook them up to this linker unless the user is either currently opening this
			// package manually via the generic browser or the package is a map package. We want to overwrite (aka load on top)
			// the objects in those cases, so don't try to find existing exports.
			//
			bool bContainsMap			= LinkerRoot ? LinkerRoot->ContainsMap() : false;
			bool bRequestFindExisting	= FCoreDelegates::ShouldLoadOnTop.IsBound() ? !FCoreDelegates::ShouldLoadOnTop.Execute(Filename) : true;
			if( (!IsRunningCommandlet() && bRequestFindExisting && !bContainsMap) )
			{
				for (int32 ExportIndex = 0; ExportIndex < ExportMap.Num(); ExportIndex++)
				{
					FindExistingExport(ExportIndex);
				}
			}
		}
#endif // WITH_EDITOR

		// Avoid duplicate work in the case of async linker creation.
		bHasFoundExistingExports = true;

		if ((LoadFlags & ( LOAD_Quiet | LOAD_SeekFree ) ) == 0)
		{
			GWarn->UpdateProgress( 4, ULinkerDefs::TotalProgressSteps );
		}
	}
	return IsTimeLimitExceeded( TEXT("finding existing exports") ) ? LINKER_TimedOut : LINKER_Loaded;
}

/**
 * Finalizes linker creation, adding linker to loaders array and potentially verifying imports.
 */
ULinkerLoad::ELinkerStatus ULinkerLoad::FinalizeCreation()
{
	if( bHasFinishedInitialization == false )
	{
		// Add this linker to the object manager's linker array.
		GObjLoaders.Add( LinkerRoot, this );

		// check if the package source matches the package filename's CRC (if it doens't match, a user saved this package)
		if (Summary.PackageSource != FCrc::StrCrc_DEPRECATED(*FPaths::GetBaseFilename(Filename).ToUpper()))
		{
//			UE_LOG(LogLinker, Log, TEXT("Found a user created pacakge (%s)"), *(FPaths::GetBaseFilename(Filename)));
		}

		if( !(LoadFlags & LOAD_NoVerify))
		{
			Verify();
		}

		// This means that _Linker references are not NULL'd when using FArchiveReplaceObjectRef
		SetFlags(RF_Public);

		// Avoid duplicate work in the case of async linker creation.
		bHasFinishedInitialization = true;

		if ((LoadFlags & ( LOAD_Quiet | LOAD_SeekFree ) ) == 0)
		{
			GWarn->UpdateProgress( 5, ULinkerDefs::TotalProgressSteps );
			GWarn->PopStatus();
		}
	}
	return IsTimeLimitExceeded( TEXT("finalizing creation") ) ? LINKER_TimedOut : LINKER_Loaded;
}

/**
 * Before loading anything objects off disk, this function can be used to discover
 * the object in memory. This could happen in the editor when you save a package (which
 * destroys the linker) and then play PIE, which would cause the Linker to be
 * recreated. However, the objects are still in memory, so there is no need to reload
 * them.
 *
 * @param ExportIndex	The index of the export to hunt down
 * @return The object that was found, or NULL if it wasn't found
 */
UObject* ULinkerLoad::FindExistingExport(int32 ExportIndex)
{
	check(ExportMap.IsValidIndex(ExportIndex));
	FObjectExport& Export = ExportMap[ExportIndex];

	// if we were already found, leave early
	if (Export.Object)
	{
		return Export.Object;
	}

	// find the outer package for this object, if it's already loaded
	UObject* OuterObject = NULL;
	if (Export.OuterIndex.IsNull())
	{
		// this export's outer is the UPackage root of this loader
		OuterObject = LinkerRoot;
	}
	else
	{
		// if we have a PackageIndex, then we are in a group or other object, and we should look for it
		OuterObject = FindExistingExport(Export.OuterIndex.ToExport());
	}

	// if we found one, keep going. if we didn't find one, then this package has never been loaded before
	// things inside a class however should not be touched, as they are in .u files and shouldn't have SetLinker called on them
	if (OuterObject && !GetOuter()->IsInA(UClass::StaticClass()))
	{
		// find the class of this object
		UClass* TheClass;
		if (Export.ClassIndex.IsNull())
		{
			TheClass = UClass::StaticClass();
		}
		else
		{
			// Check if this object export is a non-native class, non-native classes are always exports.
			// If so, then use the outer object as a package.
			UObject* ClassPackage = Export.ClassIndex.IsExport() ? LinkerRoot : ANY_PACKAGE;

			TheClass = (UClass*)StaticFindObject(UClass::StaticClass(), ClassPackage, *ImpExp(Export.ClassIndex).ObjectName.ToString(), false);
		}

		// if the class exists, try to find the object
		if (TheClass)
		{
			TheClass->GetDefaultObject(); // build the CDO if it isn't already built
			Export.Object = StaticFindObject(TheClass, OuterObject, *Export.ObjectName.ToString(), 1);

			// if we found an object, set it's linker to us
			if (Export.Object)
			{
				Export.Object->SetLinker(this, ExportIndex);
			}
		}
	}

	return Export.Object;
}

void ULinkerLoad::Verify()
{
	if(!FApp::IsGame() || GIsEditor || IsRunningCommandlet())
	{
		if( !bHaveImportsBeenVerified )
		{
			// Validate all imports and map them to their remote linkers.
			for( int32 i=0; i<Summary.ImportCount; i++ )
			{
				VerifyImport( i );
			}
		}
	}
	bHaveImportsBeenVerified = true;
}

FName ULinkerLoad::GetExportClassPackage( int32 i )
{
	FObjectExport& Export = ExportMap[ i ];
	if( Export.ClassIndex.IsImport() )
	{
		FObjectImport& Import = Imp(Export.ClassIndex);
		return ImpExp(Import.OuterIndex).ObjectName;
	}
	else if ( !Export.ClassIndex.IsNull() )
	{
		// the export's class is contained within the same package
		return LinkerRoot->GetFName();
	}
	else
	{
		return GLongCoreUObjectPackageName;
	}
}

FString ULinkerLoad::GetArchiveName() const
{
	return *Filename;
}


/**
 * Recursively gathers the dependencies of a given export (the recursive chain of imports
 * and their imports, and so on)

 * @param ExportIndex Index into the linker's ExportMap that we are checking dependencies
 * @param Dependencies Array of all dependencies needed
 * @param bSkipLoadedObjects Whether to skip already loaded objects when gathering dependencies
 */
void ULinkerLoad::GatherExportDependencies(int32 ExportIndex, TSet<FDependencyRef>& Dependencies, bool bSkipLoadedObjects)
{
	// make sure we have dependencies
	// @todo: remove this check after all packages have been saved up to VER_ADDED_LINKER_DEPENDENCIES
	if (DependsMap.Num() == 0)
	{
		return;
	}

	// validate data
	check(DependsMap.Num() == ExportMap.Num());

	// get the list of imports the export needs
	TArray<FPackageIndex>& ExportDependencies = DependsMap[ExportIndex];

//UE_LOG(LogLinker, Warning, TEXT("Gathering dependencies for %s"), *GetExportFullName(ExportIndex));

	for (int32 DependIndex = 0; DependIndex < ExportDependencies.Num(); DependIndex++)
	{
		FPackageIndex ObjectIndex = ExportDependencies[DependIndex];

		// if it's an import, use the import version to recurse (which will add the export the import points to to the array)
		if (ObjectIndex.IsImport())
		{
			GatherImportDependencies(ObjectIndex.ToImport(), Dependencies, bSkipLoadedObjects);
		}
		else
		{
			int32 RefExportIndex = ObjectIndex.ToExport();
			FObjectExport& Export = ExportMap[RefExportIndex];

			if( (Export.Object) && ( bSkipLoadedObjects == true ) )
			{
				continue;
			}

			// fill out the ref
			FDependencyRef NewRef;
			NewRef.Linker = this;
			NewRef.ExportIndex = RefExportIndex;

			// Add to set and recurse if not already present.
			bool bIsAlreadyInSet = false;
			Dependencies.Add( NewRef, &bIsAlreadyInSet );
			if (!bIsAlreadyInSet && NewRef.Linker)
			{
				NewRef.Linker->GatherExportDependencies(RefExportIndex, Dependencies, bSkipLoadedObjects);
			}
		}
	}
}

/**
 * Recursively gathers the dependencies of a given import (the recursive chain of imports
 * and their imports, and so on). Will add itself to the list of dependencies

 * @param ImportIndex Index into the linker's ImportMap that we are checking dependencies
 * @param Dependencies Set of all dependencies needed
 * @param bSkipLoadedObjects Whether to skip already loaded objects when gathering dependencies
 */
void ULinkerLoad::GatherImportDependencies(int32 ImportIndex, TSet<FDependencyRef>& Dependencies, bool bSkipLoadedObjects)
{
	// get the import
	FObjectImport& Import = ImportMap[ImportIndex];

	// we don't need the top level package imports to be checked, since there is no real object associated with them
	if (Import.OuterIndex.IsNull())
	{
		return;
	}
	//	UE_LOG(LogLinker, Warning, TEXT("  Dependency import %s [%x, %d]"), *GetImportFullName(ImportIndex), Import.SourceLinker, Import.SourceIndex);

	// if the object already exists, we don't need this import
	if (Import.XObject)
	{
		return;
	}

	BeginLoad();

	// load the linker and find export in sourcelinker
	if (Import.SourceLinker == NULL || Import.SourceIndex == INDEX_NONE)
	{
#if DO_CHECK
		int32 NumObjectsBefore = GUObjectArray.GetObjectArrayNum();
#endif

		// temp storage we can ignore
		FString Unused;

		// remember that we are gathering imports so that VerifyImportInner will no verify all imports
		bIsGatheringDependencies = true;

		// if we failed to find the object, ignore this import
		// @todo: Tag the import to not be searched again
		VerifyImportInner(ImportIndex, Unused);

		// turn off the flag
		bIsGatheringDependencies = false;

		bool bIsValidImport =
			(Import.XObject != NULL && !Import.XObject->HasAnyFlags(RF_Native) && (!Import.XObject->HasAnyFlags(RF_ClassDefaultObject) || !Import.XObject->GetClass()->HasAllFlags(EObjectFlags(RF_Public|RF_Native|RF_Transient)))) ||
			(Import.SourceLinker != NULL && Import.SourceIndex != INDEX_NONE);

		// make sure it succeeded
		if (!bIsValidImport)
		{
			// don't print out for intrinsic native classes
			if (!Import.XObject || !(Import.XObject->GetClass()->HasAnyClassFlags(CLASS_Intrinsic)))
			{
				UE_LOG(LogLinker, Warning, TEXT("VerifyImportInner failed [(%x, %d), (%x, %d)] for %s with linker: %s %s"), 
					Import.XObject, Import.XObject ? (Import.XObject->HasAnyFlags(RF_Native) ? 1 : 0) : 0, 
					Import.SourceLinker, Import.SourceIndex, 
					*GetImportFullName(ImportIndex), *this->GetFullName(), *this->Filename );
			}
			EndLoad();
			return;
		}

#if DO_CHECK && !NO_LOGGING
		// only object we should create are one ULinkerLoad for source linker
		if (GUObjectArray.GetObjectArrayNum() - NumObjectsBefore > 2)
		{
			UE_LOG(LogLinker, Warning, TEXT("Created %d objects checking %s"), GUObjectArray.GetObjectArrayNum() - NumObjectsBefore, *GetImportFullName(ImportIndex));
		}
#endif
	}

	// save off information BEFORE calling EndLoad so that the Linkers are still associated
	FDependencyRef NewRef;
	if (Import.XObject)
	{
		UE_LOG(LogLinker, Warning, TEXT("Using non-native XObject %s!!!"), *Import.XObject->GetFullName());
		NewRef.Linker = Import.XObject->GetLinker();
		NewRef.ExportIndex = Import.XObject->GetLinkerIndex();
	}
	else
	{
		NewRef.Linker = Import.SourceLinker;
		NewRef.ExportIndex = Import.SourceIndex;
	}

	EndLoad();

	// Add to set and recurse if not already present.
	bool bIsAlreadyInSet = false;
	Dependencies.Add( NewRef, &bIsAlreadyInSet );
	if (!bIsAlreadyInSet && NewRef.Linker)
	{
		NewRef.Linker->GatherExportDependencies(NewRef.ExportIndex, Dependencies, bSkipLoadedObjects);
	}
}




/**
 * A wrapper around VerifyImportInner. If the VerifyImportInner (previously VerifyImport) fails, this function
 * will look for a UObjectRedirector that will point to the real location of the object. You will see this if
 * an object was renamed to a different package or group, but something that was referencing the object was not
 * not currently open. (Rename fixes up references of all loaded objects, but naturally not for ones that aren't
 * loaded).
 *
 * @param	i	The index into this packages ImportMap to verify
 */
void ULinkerLoad::VerifyImport( int32 i )
{
	FObjectImport& Import = ImportMap[i];

	// keep a string of modifiers to add to the Editor Warning dialog
	FString WarningAppend;

	// try to load the object, but don't print any warnings on error (so we can try the redirector first)
	// note that a true return value here does not mean it failed or succeeded, just tells it how to respond to a further failure
	bool bCrashOnFail = VerifyImportInner(i,WarningAppend);
	if (FPlatformProperties::HasEditorOnlyData() == false)
	{
		bCrashOnFail = false;
	}

	// by default, we haven't failed yet
	bool bFailed = false;
	bool bRedir = false;

	// these checks find out if the VerifyImportInner was successful or not 
	if (Import.SourceLinker && Import.SourceIndex == INDEX_NONE && Import.XObject == NULL && !Import.OuterIndex.IsNull() && Import.ObjectName != NAME_ObjectRedirector)
	{
		// if we found the package, but not the object, look for a redirector
		FObjectImport OriginalImport = Import;
		Import.ClassName = NAME_ObjectRedirector;
		Import.ClassPackage = GLongCoreUObjectPackageName;

		// try again for the redirector
		VerifyImportInner(i,WarningAppend);

		// if the redirector wasn't found, then it truly doesn't exist
		if (Import.SourceIndex == INDEX_NONE)
		{
			bFailed = true;
		}
		// otherwise, we found that the redirector exists
		else
		{
			// this notes that for any load errors we get that a ObjectRedirector was involved (which may help alleviate confusion
			// when people don't understand why it was trying to load an object that was redirected from or to)
			WarningAppend += LOCTEXT("LoadWarningSuffix_redirection", " [redirection]").ToString();

			// Create the redirector (no serialization yet)
			UObjectRedirector* Redir = Cast<UObjectRedirector>(Import.SourceLinker->CreateExport(Import.SourceIndex));
			// this should probably never fail, but just in case
			if (!Redir)
			{
				bFailed = true;
			}
			else
			{
				// serialize in the properties of the redirector (to get the object the redirector point to)
				// Always load redirectors in case there was a circular dependency. This will allow inner redirector
				// references to always serialize fully here before accessing the DestinationObject
				Redir->SetFlags(RF_NeedLoad);
				Preload(Redir);

				UObject* DestObject = Redir->DestinationObject;

				// check to make sure the destination obj was loaded,
				if ( DestObject == NULL )
				{
					bFailed = true;
				}
				// check that in fact it was the type we thought it should be
				else if ( DestObject->GetClass()->GetFName() != OriginalImport.ClassName

					// if the destination object is a CDO, allow class changes
					&&	!DestObject->HasAnyFlags(RF_ClassDefaultObject) )
				{
					bFailed = true;
					// if the destination is a ObjectRedirector you've most likely made a nasty circular loop
					if( Redir->DestinationObject->GetClass() == UObjectRedirector::StaticClass() )
					{
						WarningAppend += LOCTEXT("LoadWarningSuffix_circularredirection", " [circular redirection]").ToString();
					}
				}
				else
				{
					// send a callback saying we followed a redirector successfully
					FCoreDelegates::RedirectorFollowed.Broadcast(Filename, Redir);

					// now, fake our Import to be what the redirector pointed to
					Import.XObject = Redir->DestinationObject;
					GImportCount++;
					GObjLoadersWithNewImports.Add(this);
				}
			}
		}

		// fix up the import. We put the original data back for the ClassName and ClassPackage (which are read off disk, and
		// are expected not to change)
		Import.ClassName = OriginalImport.ClassName;
		Import.ClassPackage = OriginalImport.ClassPackage;

		// if nothing above failed, then we are good to go
		if (!bFailed)
		{
			// we update the runtime information (SourceIndex, SourceLinker) to point to the object the redirector pointed to
			Import.SourceIndex = Import.XObject->GetLinkerIndex();
			Import.SourceLinker = Import.XObject->GetLinker();
		}
		else
		{
			// put us back the way we were and peace out
			Import = OriginalImport;
			// if the original VerifyImportInner told us that we need to throw an exception if we weren't redirected,
			// then do the throw here
			if (bCrashOnFail)
			{
				UE_LOG(LogLinker, Fatal,  TEXT("Failed import: %s %s (file %s)"), *Import.ClassName.ToString(), *GetImportFullName(i), *Import.SourceLinker->Filename );
				return;
			}
			// otherwise just printout warnings, and if in the editor, popup the EdLoadWarnings box
			else
			{
				// try to get a pointer to the class of the original object so that we can display the class name of the missing resource
				UObject* ClassPackage = FindObject<UPackage>( NULL, *Import.ClassPackage.ToString() );
				UClass* FindClass = ClassPackage ? FindObject<UClass>( ClassPackage, *OriginalImport.ClassName.ToString() ) : NULL;
				if( GIsEditor && !IsRunningCommandlet() )
				{
					FFormatNamedArguments Arguments[2];
					Arguments[0].Add(TEXT("ImportClass"), FText::FromName(GetImportClassName(i)));
					Arguments[1].Add(TEXT("Warning"), FText::FromString(WarningAppend));

					// put something into the load warnings dialog, with any extra information from above (in WarningAppend)
					FMessageLog(NAME_LoadErrors).Error(FText::Format(LOCTEXT("ImportFailure", "Failed import: {ImportClass}"), Arguments[0]))
						->AddToken(FAssetNameToken::Create(GetImportPathName(i)))
						->AddToken(FTextToken::Create(FText::Format(LOCTEXT("ImportFailure_WarningIn", "{Warning} in"), Arguments[1])))
						->AddToken(FAssetNameToken::Create(LinkerRoot->GetName()));
				}

#if UE_BUILD_DEBUG
				if( !IgnoreMissingReferencedClass( Import.ObjectName ) )
				{
					// failure to load a class, most likely deleted instead of deprecated
					if ( (!GIsEditor || IsRunningCommandlet()) && (FindClass->IsChildOf(UClass::StaticClass())) )
					{
						UE_LOG(LogLinker, Warning, TEXT("Missing Class '%s' referenced by package '%s' ('%s').  Classes should not be removed if referenced by content; mark the class 'deprecated' instead."),
							*GetImportFullName(i),
							*LinkerRoot->GetName(),
							GSerializedExportLinker ? *GSerializedExportLinker->GetExportPathName(GSerializedExportIndex) : TEXT("Unknown") );
					}
					// ignore warnings for missing imports if the object's class has been deprecated.
					else if ( FindClass == NULL || !FindClass->HasAnyClassFlags(CLASS_Deprecated) )
					{
						UE_LOG(LogLinker, Warning, TEXT("Missing Class '%s' referenced by package '%s' ('%s')."),
							*GetImportFullName(i),
							*LinkerRoot->GetName(),
							GSerializedExportLinker ? *GSerializedExportLinker->GetExportPathName(GSerializedExportIndex) : TEXT("Unknown") );
					}
				}
#endif
			}
		}
	}
}

/**
 * Safely verify that an import in the ImportMap points to a good object. This decides whether or not
 * a failure to load the object redirector in the wrapper is a fatal error or not (return value)
 *
 * @param	i	The index into this packages ImportMap to verify
 *
 * @return true if the wrapper should crash if it can't find a good object redirector to load
 */
bool ULinkerLoad::VerifyImportInner(const int32 ImportIndex, FString& WarningSuffix)
{
	check(IsLoading());

	FObjectImport& Import = ImportMap[ImportIndex];

	if
	(	(Import.SourceLinker && Import.SourceIndex != INDEX_NONE)
	||	Import.ClassPackage	== NAME_None
	||	Import.ClassName	== NAME_None
	||	Import.ObjectName	== NAME_None )
	{
		// Already verified, or not relevent in this context.
		return false;
	}


	bool SafeReplace = false;
	UObject* Pkg=NULL;
	UPackage* TmpPkg=NULL;

	// Find or load the linker load that contains the FObjectExport for this import
	if (Import.OuterIndex.IsNull() && Import.ClassName!=NAME_Package )
	{
		UE_LOG(LogLinker, Warning, TEXT("%s has an inappropriate outermost, it was probably saved with a deprecated outer."), *Import.ObjectName.ToString());
		Import.SourceLinker = NULL;
		return false;
	}
	else if( Import.OuterIndex.IsNull() )
	{
		// our Outer is a UPackage
		check(Import.ClassName==NAME_Package);
		uint32 InternalLoadFlags = LoadFlags & (LOAD_NoVerify|LOAD_NoWarn|LOAD_Quiet);

		// Check if the package has already been fully loaded, then we can skip the linker.
		bool bWasFullyLoaded = false;
		if (FPlatformProperties::RequiresCookedData())
		{
			TmpPkg = FindObjectFast<UPackage>(NULL, Import.ObjectName);
			bWasFullyLoaded = TmpPkg && TmpPkg->IsFullyLoaded();
		}
		if (!bWasFullyLoaded)
		{
			// we now fully load the package that we need a single export from - however, we still use CreatePackage below as it handles all cases when the package
			// didn't exist (native only), etc		
			TmpPkg = LoadPackage(NULL, *Import.ObjectName.ToString(), InternalLoadFlags);
		}

		// following is the original VerifyImport code
		// @todo linkers: This could quite possibly be cleaned up
		if (TmpPkg == NULL)
		{
			TmpPkg = CreatePackage( NULL, *Import.ObjectName.ToString() );
		}

		// if we couldn't create the package or it is 
		// to be linked to any other package's ImportMaps
		if ( !TmpPkg || (TmpPkg->PackageFlags&PKG_Compiling) != 0 )
		{
			return false;
		}

		// while gathering dependencies, there is no need to verify all of the imports for the entire package
		if (bIsGatheringDependencies)
		{
			InternalLoadFlags |= LOAD_NoVerify;
		}

		// Get the linker if the package hasn't been fully loaded already.
		if (!bWasFullyLoaded)
		{
			Import.SourceLinker = GetPackageLinker( TmpPkg, NULL, InternalLoadFlags, NULL, NULL );
		}
	}
	else
	{
		// this resource's Outer is not a UPackage
		checkf(Import.OuterIndex.IsImport(),TEXT("Outer for Import %s (%i) is not an import - OuterIndex:%i"), *GetImportFullName(ImportIndex), ImportIndex, Import.OuterIndex.ForDebugging());

		VerifyImport( Import.OuterIndex.ToImport() );

		FObjectImport& OuterImport = Imp(Import.OuterIndex);

		if (!OuterImport.SourceLinker && OuterImport.XObject)
		{
			FObjectImport* Top;
			for (Top = &OuterImport;	Top->OuterIndex.IsImport(); Top = &Imp(Top->OuterIndex))
			{
				// for loop does what we need
			}
			 if (Cast<UPackage>(Top->XObject) && (Cast<UPackage>(Top->XObject)->PackageFlags & PKG_CompiledIn))
			 {
				 // this is an import to a compiled in thing, just search for it in the package
				 TmpPkg = Cast<UPackage>(Top->XObject);
			 }
		}

		// Copy the SourceLinker from the FObjectImport for our Outer
		Import.SourceLinker = OuterImport.SourceLinker;

		//check(Import.SourceLinker);
		//@todo what does it mean if we don't have a SourceLinker here?
		if( Import.SourceLinker )
		{
			FObjectImport* Top;
			for (Top = &Import;	Top->OuterIndex.IsImport(); Top = &Imp(Top->OuterIndex))
			{
				// for loop does what we need
			}

			// Top is now pointing to the top-level UPackage for this resource
			Pkg = CreatePackage(NULL, *Top->ObjectName.ToString() );

			// Find this import within its existing linker.
			int32 iHash = HashNames( Import.ObjectName, Import.ClassName, Import.ClassPackage) & (ARRAY_COUNT(ExportHash)-1);

			//@Package name transition, if we can match without shortening the names, then we must not take a shortened match
			bool bMatchesWithoutShortening = false;
			FName TestName = Import.ClassPackage;
			
			for( int32 j=Import.SourceLinker->ExportHash[iHash]; j!=INDEX_NONE; j=Import.SourceLinker->ExportMap[j].HashNext )
			{
				FObjectExport& SourceExport = Import.SourceLinker->ExportMap[ j ];
				if
					(
					SourceExport.ObjectName == Import.ObjectName
					&&	Import.SourceLinker->GetExportClassName(j) == Import.ClassName
					&&  Import.SourceLinker->GetExportClassPackage(j) == Import.ClassPackage 
					)
				{
					bMatchesWithoutShortening = true;
					break;
				}
			}
			if (!bMatchesWithoutShortening)
			{
				TestName = FPackageName::GetShortFName(TestName);
			}

			for( int32 j=Import.SourceLinker->ExportHash[iHash]; j!=INDEX_NONE; j=Import.SourceLinker->ExportMap[j].HashNext )
			{
				FObjectExport& SourceExport = Import.SourceLinker->ExportMap[ j ];
				if
				(	
					SourceExport.ObjectName==Import.ObjectName               
					&&	Import.SourceLinker->GetExportClassName(j)==Import.ClassName
					&&  (bMatchesWithoutShortening ? Import.SourceLinker->GetExportClassPackage(j) : FPackageName::GetShortFName(Import.SourceLinker->GetExportClassPackage(j))) == TestName 
				)
				{
					// at this point, SourceExport is an FObjectExport in another linker that looks like it
					// matches the FObjectImport we're trying to load - double check that we have the correct one
					if( Import.OuterIndex.IsImport() )
					{
						// OuterImport is the FObjectImport for this resource's Outer
						if( OuterImport.SourceLinker )
						{
							// if the import for our Outer doesn't have a SourceIndex, it means that
							// we haven't found a matching export for our Outer yet.  This should only
							// be the case if our Outer is a top-level UPackage
							if( OuterImport.SourceIndex==INDEX_NONE )
							{
								// At this point, we know our Outer is a top-level UPackage, so
								// if the FObjectExport that we found has an Outer that is
								// not a linker root, this isn't the correct resource
								if( !SourceExport.OuterIndex.IsNull() )
								{
									continue;
								}
							}

							// The import for our Outer has a matching export - make sure that the import for
							// our Outer is pointing to the same export as the SourceExport's Outer
							else if( FPackageIndex::FromExport(OuterImport.SourceIndex) != SourceExport.OuterIndex )
							{
								continue;
							}
						}
					}
					if( !(SourceExport.ObjectFlags & RF_Public) )
					{
						SafeReplace = SafeReplace || (GIsEditor && !IsRunningCommandlet());

						// determine if this find the thing that caused this import to be saved into the map
						FPackageIndex FoundIndex = FPackageIndex::FromImport(ImportIndex);
						for ( int32 i = 0; i < Summary.ExportCount; i++ )
						{
							FObjectExport& Export = ExportMap[i];
							if ( Export.SuperIndex == FoundIndex )
							{
								UE_LOG(LogLinker, Log, TEXT("Private import was referenced by export '%s' (parent)"), *Export.ObjectName.ToString());
								SafeReplace = false;
							}
							else if ( Export.ClassIndex == FoundIndex )
							{
								UE_LOG(LogLinker, Log, TEXT("Private import was referenced by export '%s' (class)"), *Export.ObjectName.ToString());
								SafeReplace = false;
							}
							else if ( Export.OuterIndex == FoundIndex )
							{
								UE_LOG(LogLinker, Log, TEXT("Private import was referenced by export '%s' (outer)"), *Export.ObjectName.ToString());
								SafeReplace = false;
							}
						}
						for ( int32 i = 0; i < Summary.ImportCount; i++ )
						{
							if ( i != ImportIndex )
							{
								FObjectImport& TestImport = ImportMap[i];
								if ( TestImport.OuterIndex == FoundIndex )
								{
									UE_LOG(LogLinker, Log, TEXT("Private import was referenced by import '%s' (outer)"), *Import.ObjectName.ToString());
									SafeReplace = false;
								}
							}
						}

						if ( !SafeReplace )
						{
							UE_LOG(LogLinker, Warning, TEXT("%s"), *FString::Printf( TEXT("Can't import private object %s %s"), *Import.ClassName.ToString(), *GetImportFullName(ImportIndex) ) );
							return false;
						}
						else
						{
							FString Suffix = LOCTEXT("LoadWarningSuffix_privateobject", " [private]").ToString();
							if ( !WarningSuffix.Contains(Suffix) )
							{
								WarningSuffix += Suffix;
							}
							break;
						}
					}

					// Found the FObjectExport for this import
					Import.SourceIndex = j;
					break;
				}
			}
		}
	}

	bool bCameFromCompiledInPackage = false;
	if (!Pkg && TmpPkg && (TmpPkg->PackageFlags&PKG_CompiledIn))
	{
		Pkg = TmpPkg; // this is a compiled in package, so that is the package to search regardless of FindIfFail
		bCameFromCompiledInPackage = true;

		if (IsCoreUObjectPackage(Import.ClassPackage) && Import.ClassName == NAME_Package && !TmpPkg->GetOuter())
		{
			if (Import.ObjectName == TmpPkg->GetFName())
			{
				// except if we are looking for _the_ package...in which case we are looking for TmpPkg, so we are done
				Import.XObject = TmpPkg;
				GImportCount++;
				GObjLoadersWithNewImports.Add(this);
				return false;
			}
		}
	}

	if( (Pkg == NULL) && ((LoadFlags & LOAD_FindIfFail) != 0) )
	{
		Pkg = ANY_PACKAGE;
	}

	// If not found in file, see if it's a public native transient class or field.
	if( Import.SourceIndex==INDEX_NONE && Pkg!=NULL )
	{
		UObject* ClassPackage = FindObject<UPackage>( NULL, *Import.ClassPackage.ToString() );
		if( ClassPackage )
		{
			UClass* FindClass = FindObject<UClass>( ClassPackage, *Import.ClassName.ToString() );
			if( FindClass )
			{
				UObject* FindOuter			= Pkg;

				if ( Import.OuterIndex.IsImport() )
				{
					// if this import corresponds to an intrinsic class, OuterImport's XObject will be NULL if this import
					// belongs to the same package that the import's class is in; in this case, the package is the correct Outer to use
					// for finding this object
					// otherwise, this import represents a field of an intrinsic class, and OuterImport's XObject should be non-NULL (the object
					// that contains the field)
					FObjectImport& OuterImport	= Imp(Import.OuterIndex);
					if ( OuterImport.XObject != NULL )
					{
						FindOuter = OuterImport.XObject;
					}
				}

				UObject* FindObject = StaticFindObject(FindClass, FindOuter, *Import.ObjectName.ToString());
				// reference to native transient class or CDO of such a class
				bool IsNativeTransient	= bCameFromCompiledInPackage || (FindObject != NULL && (FindObject->HasAllFlags(EObjectFlags(RF_Public|RF_Native|RF_Transient)) || (FindObject->HasAnyFlags(RF_ClassDefaultObject) && FindObject->GetClass()->HasAllFlags(EObjectFlags(RF_Public|RF_Native|RF_Transient)))));
				// Check for structs which have been moved to another header (within the same class package).
				if (!FindObject && IsNativeTransient && FindClass == UScriptStruct::StaticClass())
				{
					FindObject = StaticFindObject( FindClass, ANY_PACKAGE, *Import.ObjectName.ToString(), true );
					if (FindObject && FindOuter->GetOutermost() != FindObject->GetOutermost())
					{
						// Limit the results to the same package.
						FindObject = NULL;
					}
				}
				if (FindObject != NULL && ((LoadFlags & LOAD_FindIfFail) || IsNativeTransient))
				{
					Import.XObject = FindObject;
					GImportCount++;
					GObjLoadersWithNewImports.Add(this);
				}
				else
				{
					SafeReplace = true;
				}
			}
			else
			{
				SafeReplace = true;
			}
		}

		if (!Import.XObject && !SafeReplace)
		{
			return true;
		}
	}
	return false;
}

UObject* ULinkerLoad::CreateExportAndPreload(int32 ExportIndex, bool bForcePreload /* = false */)
{
	UObject *Object = CreateExport(ExportIndex);
	if (Object && (bForcePreload || (Cast<UClass>(Object) != NULL) || Object->IsTemplate() || (Cast<UObjectRedirector>(Object) != NULL)))
	{
		Preload(Object);
	}

	return Object;
}

int32 ULinkerLoad::LoadMetaDataFromExportMap(bool bForcePreload/* = false */)
{
	int32 MetaDataIndex = INDEX_NONE;

	// Try to find MetaData and load it first as other objects can depend on it.
	for (int32 ExportIndex = 0; ExportIndex < ExportMap.Num(); ++ExportIndex)
	{
		if (ExportMap[ExportIndex].ObjectName == NAME_PackageMetaData)
		{
			CreateExportAndPreload(ExportIndex, bForcePreload);
			MetaDataIndex = ExportIndex;
			break;
		}
	}

	// If not found then try to use old name and rename.
	if (MetaDataIndex == INDEX_NONE)
	{
		for (int32 ExportIndex = 0; ExportIndex < ExportMap.Num(); ++ExportIndex)
		{
			if (ExportMap[ExportIndex].ObjectName == *UMetaData::StaticClass()->GetName())
			{
				UObject* Object = CreateExportAndPreload(ExportIndex, bForcePreload);
				Object->Rename(*FName(NAME_PackageMetaData).ToString(), NULL, REN_ForceNoResetLoaders);

				MetaDataIndex = ExportIndex;
				break;
			}
		}
	}

	return MetaDataIndex;
}

/**
 * Loads all objects in package.
 *
 * @param bForcePreload	Whether to explicitly call Preload (serialize) right away instead of being
 *						called from EndLoad()
 */
void ULinkerLoad::LoadAllObjects( bool bForcePreload )
{
	if ( (LoadFlags&LOAD_SeekFree) != 0 )
	{
		bForcePreload = true;
	}

	bool bAllowedToShowStatusUpdate = (LoadFlags & ( LOAD_Quiet | LOAD_SeekFree ) ) == 0;
	double StartTime = FPlatformTime::Seconds();

	// MetaData object index in this package.
	int32 MetaDataIndex = INDEX_NONE;

	if(!FPlatformProperties::RequiresCookedData())
	{
		MetaDataIndex = LoadMetaDataFromExportMap(bForcePreload);
	}

	for(int32 ExportIndex = 0; ExportIndex < ExportMap.Num(); ++ExportIndex)
	{
		if(ExportIndex == MetaDataIndex)
		{
			continue;
		}

		CreateExportAndPreload(ExportIndex, bForcePreload);
	}

	// Mark package as having been fully loaded.
	if( LinkerRoot )
	{
		LinkerRoot->MarkAsFullyLoaded();
	}
}

/**
 * Returns the ObjectName associated with the resource indicated.
 * 
 * @param	ResourceIndex	location of the object resource
 *
 * @return	ObjectName for the FObjectResource at ResourceIndex, or NAME_None if not found
 */
FName ULinkerLoad::ResolveResourceName( FPackageIndex ResourceIndex )
{
	if (ResourceIndex.IsNull())
	{
		return NAME_None;
	}
	return ImpExp(ResourceIndex).ObjectName;
}

// Find the index of a specified object without regard to specific package.
int32 ULinkerLoad::FindExportIndex( FName ClassName, FName ClassPackage, FName ObjectName, FPackageIndex ExportOuterIndex )
{
	int32 iHash = HashNames( ObjectName, ClassName, ClassPackage ) & (ARRAY_COUNT(ExportHash)-1);

	for( int32 i=ExportHash[iHash]; i!=INDEX_NONE; i=ExportMap[i].HashNext )
	{
		if
		(  (ExportMap[i].ObjectName  ==ObjectName                              )
			&& (GetExportClassPackage(i) ==ClassPackage                            )
			&& (GetExportClassName   (i) ==ClassName                               ) 
			&& (ExportMap[i].OuterIndex  ==ExportOuterIndex 
			|| ExportOuterIndex.IsImport()) // this is very not legit to be passing INDEX_NONE into this function to mean "ignore"
		)
		{
			return i;
		}
	}
	
	// If an object with the exact class wasn't found, look for objects with a subclass of the requested class.
	for(int32 ExportIndex = 0;ExportIndex < ExportMap.Num();ExportIndex++)
	{
		FObjectExport&	Export = ExportMap[ExportIndex];

		if(Export.ObjectName == ObjectName && (ExportOuterIndex.IsImport() || Export.OuterIndex == ExportOuterIndex)) // this is very not legit to be passing INDEX_NONE into this function to mean "ignore"
		{
			UClass*	ExportClass = Cast<UClass>(IndexToObject(Export.ClassIndex));

			// See if this export's class inherits from the requested class.
			for(UClass* ParentClass = ExportClass;ParentClass;ParentClass = ParentClass->GetSuperClass())
			{
				if(ParentClass->GetFName() == ClassName)
				{
					return ExportIndex;
				}
			}
		}
	}

	return INDEX_NONE;
}

/**
 * Function to create the instance of, or verify the presence of, an object as found in this Linker.
 *
 * @param ObjectClass	The class of the object
 * @param ObjectName	The name of the object
 * @param Outer			Find the object inside this outer (and only directly inside this outer, as we require fully qualified names)
 * @param LoadFlags		Flags used to determine if the object is being verified or should be created
 * @param Checked		Whether or not a failure will throw an error
 * @return The created object, or (UObject*)-1 if this is just verifying
 */
UObject* ULinkerLoad::Create( UClass* ObjectClass, FName ObjectName, UObject* Outer, uint32 InLoadFlags, bool Checked )
{
	// We no longer handle a NULL outer, which used to mean look in any outer, but we need fully qualified names now
	// The other case where this was NULL is if you are calling StaticLoadObject on the top-level package, but
	// you should be using LoadPackage. If for some weird reason you need to load the top-level package with this,
	// then I believe you'd want to set OuterIndex to 0 when Outer is NULL, but then that could get confused with
	// loading A.A (they both have OuterIndex of 0, according to Ron)
	check(Outer);

	int32 OuterIndex = INDEX_NONE;

	// if the outer is the outermost of the package, then we want OuterIndex to be 0, as objects under the top level
	// will have an OuterIndex to 0
	if (Outer == Outer->GetOutermost())
	{
		OuterIndex = 0;
	}
	// otherwise get the linker index of the outer to be the outer index that we look in
	else
	{
		OuterIndex = Outer->GetLinkerIndex();
		// we _need_ the linker index of the outer to look in, which means that the outer must have been actually 
		// loaded off disk, and not just CreatePackage'd
		check(OuterIndex != INDEX_NONE);
	}

	FPackageIndex OuterPackageIndex;

	if (OuterIndex)
	{
		OuterPackageIndex = FPackageIndex::FromExport(OuterIndex);
	}

	int32 Index = FindExportIndex(ObjectClass->GetFName(), ObjectClass->GetOuter()->GetFName(), ObjectName, OuterPackageIndex);
	if (Index != INDEX_NONE)
	{
		return (InLoadFlags & LOAD_Verify) ? INVALID_OBJECT : CreateExport(Index);
	}

	// since we didn't find it, see if we can find an object redirector with the same name
	// Are we allowed to follow redirects?
	if( !( InLoadFlags & LOAD_NoRedirects ) )
	{
		Index = FindExportIndex(UObjectRedirector::StaticClass()->GetFName(), NAME_CoreUObject, ObjectName, OuterPackageIndex);
		if (Index == INDEX_NONE)
		{
			Index = FindExportIndex(UObjectRedirector::StaticClass()->GetFName(), GLongCoreUObjectPackageName, ObjectName, OuterPackageIndex);			
		}

		// if we found a redirector, create it, and move on down the line
		if (Index != INDEX_NONE)
		{
			// create the redirector,
			UObjectRedirector* Redir = (UObjectRedirector*)CreateExport(Index);
			Preload(Redir);
			// if we found what it was point to, then return it
			if (Redir->DestinationObject && Redir->DestinationObject->IsA(ObjectClass))
			{
				// send a callback saying we followed a redirector successfully
				FCoreDelegates::RedirectorFollowed.Broadcast(Filename, Redir);
				// and return the object we are being redirected to
				return Redir->DestinationObject;
			}
		}
	}


// Set this to 1 to find nonqualified names anyway
#define FIND_OBJECT_NONQUALIFIED 0
// Set this to 1 if you want to see what it would have found previously. This is useful for fixing up hundreds
// of now-illegal references in script code.
#define DEBUG_PRINT_NONQUALIFIED_RESULT 1

#if DEBUG_PRINT_NONQUALIFIED_RESULT || FIND_OBJECT_NONQUALIFIED
	Index = FindExportIndex(ObjectClass->GetFName(), ObjectClass->GetOuter()->GetFName(), ObjectName, FPackageIndex::FromImport(0));// this is very not legit to be passing INDEX_NONE into this function to mean "ignore"
	if (Index != INDEX_NONE)
	{
#if DEBUG_PRINT_NONQUALIFIED_RESULT
		UE_LOG(LogLinker, Warning, TEXT("Using a non-qualified name (would have) found: %s"), *GetExportFullName(Index));
#endif
#if FIND_OBJECT_NONQUALIFIED
		return (InLoadFlags & LOAD_Verify) ? INVALID_OBJECT : CreateExport(Index);
#endif
	}
#endif


	// if we are checking for failure cases, and we failed, throw an error
	if( Checked )
	{
		UE_LOG(LogLinker, Warning, TEXT("%s"), *FString::Printf( TEXT("%s %s not found for creation"), *ObjectClass->GetName(), *ObjectName.ToString() ));
	}
	return NULL;
}

/**
 * Serialize the object data for the specified object from the unreal package file.  Loads any
 * additional resources required for the object to be in a valid state to receive the loaded
 * data, such as the object's Outer, Class, or ObjectArchetype.
 *
 * When this function exits, Object is guaranteed to contain the data stored that was stored on disk.
 *
 * @param	Object	The object to load data for.  If the data for this object isn't stored in this
 *					ULinkerLoad, routes the call to the appropriate linker.  Data serialization is 
 *					skipped if the object has already been loaded (as indicated by the RF_NeedLoad flag
 *					not set for the object), so safe to call on objects that have already been loaded.
 *					Note that this function assumes that Object has already been initialized against
 *					its template object.
 *					If Object is a UClass and the class default object has already been created, calls
 *					Preload for the class default object as well.
 */
void ULinkerLoad::Preload( UObject* Object )
{
	check(IsValidLowLevel());
	check(Object);
	// Preload the object if necessary.
	if( Object->HasAnyFlags(RF_NeedLoad) )
	{
		if( Object->GetLinker()==this )
		{
			SCOPE_CYCLE_COUNTER(STAT_LinkerPreload);
			FScopeCycleCounterUObject PreloadScope(Object, GET_STATID(STAT_LinkerPreload));
			UClass* Cls = NULL;

			// If this is a struct, make sure that its parent struct is completely loaded
			if(	Object->IsA(UStruct::StaticClass()) )
			{
				Cls = Cast<UClass>(Object);
				if( ((UStruct*)Object)->GetSuperStruct() )
				{
					Preload( ((UStruct*)Object)->GetSuperStruct() );
				}
			}

			// make sure this object didn't get loaded in the above Preload call
			if (Object->HasAnyFlags(RF_NeedLoad))
			{
				// grab the resource for this Object
				FObjectExport& Export = ExportMap[ Object->GetLinkerIndex() ];
				check(Export.Object==Object);

				const int64 SavedPos = Loader->Tell();

				// move to the position in the file where this object's data
				// is stored
				Loader->Seek( Export.SerialOffset );

				{
					SCOPE_CYCLE_COUNTER(STAT_LinkerPrecache);
					// tell the file reader to read the raw data from disk
					Loader->Precache( Export.SerialOffset, Export.SerialSize );
				}

				// mark the object to indicate that it has been loaded
				Object->ClearFlags ( RF_NeedLoad );

				{
					SCOPE_CYCLE_COUNTER(STAT_LinkerSerialize);
					if ( Object->HasAnyFlags(RF_ClassDefaultObject) )
					{
						Object->GetClass()->SerializeDefaultObject(Object, *this);
					}
					else
					{
						// Maintain the current GSerializedObjects.
						UObject* PrevSerializedObject = GSerializedObject;
						GSerializedObject = Object;
						Object->Serialize( *this );
						Object->SetFlags(RF_LoadCompleted);
						GSerializedObject = PrevSerializedObject;
					}
				}

				// Make sure we serialized the right amount of stuff.
				if( Tell()-Export.SerialOffset != Export.SerialSize )
				{
					if (Object->GetClass()->HasAnyClassFlags(CLASS_Deprecated))
					{
						UE_LOG(LogLinker, Warning, TEXT("%s"), *FString::Printf( TEXT("%s: Serial size mismatch: Got %d, Expected %d"), *Object->GetFullName(), (int32)(Tell()-Export.SerialOffset), Export.SerialSize ) );
					}
					else
					{
						UE_LOG(LogLinker, Fatal, TEXT("%s"), *FString::Printf( TEXT("%s: Serial size mismatch: Got %d, Expected %d"), *Object->GetFullName(), (int32)(Tell()-Export.SerialOffset), Export.SerialSize ) );
					}
				}

				Loader->Seek( SavedPos );

				// if this is a UClass object and it already has a class default object
				if ( Cls != NULL && Cls->GetDefaultsCount() )
				{
					// make sure that the class default object is completely loaded as well
					Preload(Cls->GetDefaultObject());
				}

#if WITH_EDITOR
				// Check if this object's class has been changed by ActiveClassRedirects.
				FName OldClassName;
				if (Export.OldClassName != NAME_None && Object->GetClass()->GetFName() != Export.OldClassName)
				{
					// This happens when the class has changed only for object instance.
					OldClassName = Export.OldClassName;
				}
				else if (Export.ClassIndex.IsImport())
				{
					// Check if the class has been renamed / replaced in the import map.
					const FObjectImport& ClassImport = Imp(Export.ClassIndex);
					if (ClassImport.OldClassName != NAME_None && ClassImport.OldClassName != Object->GetClass()->GetFName())
					{
						OldClassName = ClassImport.OldClassName;
					}
				}
				else if (Export.ClassIndex.IsExport())
				{
					// Handle blueprints. This is slightly different from the other cases as we're looking for the first 
					// native super of the blueprint class (first import).
					const FObjectExport* ClassExport = NULL;
					for (ClassExport = &Exp(Export.ClassIndex); ClassExport->SuperIndex.IsExport(); ClassExport = &Exp(Export.SuperIndex));
					if (ClassExport->SuperIndex.IsImport())
					{
						const FObjectImport& ClassImport = Imp(ClassExport->SuperIndex);
						if (ClassImport.OldClassName != NAME_None)
						{
							OldClassName = ClassImport.OldClassName;
						}
					}
				}
				if (OldClassName != NAME_None)
				{
					// Notify if the object's class has changed as a result of active class redirects.
					Object->LoadedFromAnotherClass(OldClassName);
				}
#endif

				// It's ok now to call PostLoad on blueprint CDOs
				if (Object->HasAnyFlags(RF_ClassDefaultObject) && Object->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
				{
					Object->SetFlags(RF_NeedPostLoad|RF_WasLoaded);
					GObjLoaded.Add( Object );
				}
			}
		}
		else if( Object->GetLinker() )
		{
			// Send to the object's linker.
			Object->GetLinker()->Preload( Object );
		}
	}
}

/**
 * Builds a string containing the full path for a resource in the export table.
 *
 * @param OutPathName		[out] Will contain the full path for the resource
 * @param ResourceIndex		Index of a resource in the export table
 */
void ULinkerLoad::BuildPathName( FString& OutPathName, FPackageIndex ResourceIndex ) const
{
	if ( ResourceIndex.IsNull() )
	{
		return;
	}
	const FObjectResource& Resource = ImpExp(ResourceIndex);
	BuildPathName( OutPathName, Resource.OuterIndex );
	if ( OutPathName.Len() > 0 )
	{
		OutPathName += TEXT('.');
	}
	OutPathName += Resource.ObjectName.ToString();
}

/**
 * Checks if the specified export should be loaded or not.
 * Performs similar checks as CreateExport().
 *
 * @param ExportIndex	Index of the export to check
 * @return				true of the export should be loaded
 */
bool ULinkerLoad::WillTextureBeLoaded( UClass* Class, int32 ExportIndex )
{
	const FObjectExport& Export = ExportMap[ ExportIndex ];

	// Already loaded?
	if ( Export.Object || FilterExport(Export))  // it was "not for" in all acceptable positions
	{
		return false;
	}

	// Build path name
	FString PathName;
	PathName.Reserve(256);
	BuildPathName( PathName, FPackageIndex::FromExport(ExportIndex) );

	UObject* ExistingTexture = StaticFindObjectFastExplicit( Class, Export.ObjectName, PathName, false, RF_NoFlags );
	if ( ExistingTexture )
	{
		return false;
	}
	else
	{
		return true;
	}
}

UObject* ULinkerLoad::CreateExport( int32 Index )
{
	FScopedCreateExportCounter ScopedCounter( this, Index );
	FMessageLog LoadErrors(NAME_LoadErrors);

	// Map the object into our table.
	FObjectExport& Export = ExportMap[ Index ];

	// Check whether we already loaded the object and if not whether the context flags allow loading it.
	if( !Export.Object && !FilterExport(Export) ) // for some acceptable position, it was not "not for" 
	{
		check(Export.ObjectName!=NAME_None || !(Export.ObjectFlags&RF_Public));
		check(IsLoading());

		// Get the object's class.
		if(Export.ClassIndex.IsImport() ) 
		{
			VerifyImport(Export.ClassIndex.ToImport()); 
		}
		UClass* LoadClass = (UClass*)IndexToObject( Export.ClassIndex );
		if( !LoadClass && !Export.ClassIndex.IsNull() ) // Hack to load packages with classes which do not exist.
		{
			return NULL;
		}
#if WITH_EDITOR
		// NULL (None) active class redirect.
		if( !LoadClass && Export.ObjectName.IsNone() && Export.ClassIndex.IsNull() && !Export.OldClassName.IsNone() )
		{
			return NULL;
		}
#endif
		if( !LoadClass )
		{
			LoadClass = UClass::StaticClass();
		}
		UObjectRedirector* LoadClassRedirector = Cast<UObjectRedirector>(LoadClass);
		if( LoadClassRedirector)
		{
			// mark this export as unloadable (so that other exports that
			// reference this one won't continue to execute the above logic), then return NULL
			Export.bExportLoadFailed = true;

			// otherwise, return NULL and let the calling code determine what to do
			FString OuterName = Export.OuterIndex.IsNull() ? LinkerRoot->GetFullName() : GetFullImpExpName(Export.OuterIndex);
			UE_LOG(LogLinker, Warning, TEXT("CreateExport: Failed to load Outer for resource because its class is a redirector '%s': %s"), *Export.ObjectName.ToString(), *OuterName);
			return NULL;
		}

		check(LoadClass);
		check(Cast<UClass>(LoadClass) != NULL);

		// Check for a valid superstruct while there is still time to safely bail, if this export has one
		if( !Export.SuperIndex.IsNull() )
		{
			UStruct* SuperStruct = (UStruct*)IndexToObject( Export.SuperIndex );
			if( !SuperStruct )
			{
				if( LoadClass->IsChildOf(UFunction::StaticClass()) )
				{
					// If this is a function whose super has been removed, give it a NULL super, as we would have in the script compiler
					UE_LOG(LogLinker, Warning, TEXT("CreateExport: Failed to load Super for %s; removing super information, but keeping function"), *GetExportFullName(Index));
					Export.SuperIndex = FPackageIndex();
				}
				else
				{
					UE_LOG(LogLinker, Warning, TEXT("CreateExport: Failed to load Super for %s"), *GetExportFullName(Index));
					return NULL;
				}
			}
		}

		// Only UClass objects and UProperty objects of intrinsic classes can have RF_Native set. Those property objects are never
		// serialized so we only have to worry about classes. If we encounter an object that is not a class and has RF_Native set
		// we warn about it and remove the flag.
		if( (Export.ObjectFlags & RF_Native) != 0 && !LoadClass->IsChildOf(UField::StaticClass()) )
		{
			UE_LOG(LogLinker, Warning,TEXT("%s %s has RF_Native set but is not a UField derived class"),*LoadClass->GetName(),*Export.ObjectName.ToString());
			// Remove RF_Native;
			Export.ObjectFlags = EObjectFlags(Export.ObjectFlags & ~RF_Native);
		}

		if ( !LoadClass->HasAnyClassFlags(CLASS_Intrinsic) )
		{
			Preload( LoadClass );

			// Check if the Preload() above caused the class to be regenerated (LoadClass will be out of date), and refresh the LoadClass pointer if that is the case
			if( LoadClass->HasAnyClassFlags(CLASS_NewerVersionExists) )
			{
				if( Export.ClassIndex.IsImport() )
				{
					FObjectImport& ClassImport = Imp(Export.ClassIndex);
					ClassImport.XObject = NULL;
				}

				LoadClass = (UClass*)IndexToObject( Export.ClassIndex );
			}

			if ( LoadClass->HasAnyClassFlags(CLASS_Deprecated) && GIsEditor && !IsRunningCommandlet() && !FApp::IsGame() )
			{
				if ( (Export.ObjectFlags&RF_ClassDefaultObject) == 0 )
				{
					FFormatNamedArguments Arguments;
					Arguments.Add(TEXT("ObjectName"), FText::FromString(GetExportFullName(Index)));
					Arguments.Add(TEXT("ClassName"), FText::FromString(LoadClass->GetPathName()));
					LoadErrors.Warning(FText::Format(LOCTEXT("LoadedDeprecatedClassInstance", "{ObjectName}: class {ClassName} has been deprecated."), Arguments));
				}
			}
		}

		// detect cases where a class has been made transient when there are existing instances of this class in content packages,
		// and this isn't the class default object; when this happens, it can cause issues which are difficult to debug since they'll
		// only appear much later after this package has been loaded
		if ( LoadClass->HasAnyClassFlags(CLASS_Transient) && (Export.ObjectFlags&RF_ClassDefaultObject) == 0 && (Export.ObjectFlags&RF_ArchetypeObject) == 0 )
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("PackageName"), FText::FromString(Filename));
			Arguments.Add(TEXT("ObjectName"), FText::FromName(Export.ObjectName));
			Arguments.Add(TEXT("ClassName"), FText::FromString(LoadClass->GetPathName()));
			//@todo - should this actually be an assertion?
			LoadErrors.Warning(FText::Format(LOCTEXT("LoadingTransientInstance", "Attempting to load an instance of a transient class from disk - Package:'{PackageName}'  Object:'{ObjectName}'  Class:'{ClassName}'"), Arguments));
		}

		
		// Find or create the object's Outer.
		UObject* ThisParent = NULL;
		if( !Export.OuterIndex.IsNull() )
		{
			ThisParent = IndexToObject(Export.OuterIndex);
		}
		else if( Export.bForcedExport )
		{
			// Create the forced export in the TopLevel instead of LinkerRoot. Please note that CreatePackage
			// will find and return an existing object if one exists and only create a new one if there doesn't.
			Export.Object = CreatePackage( NULL, *Export.ObjectName.ToString() );
			check(Export.Object);
			GForcedExportCount++;
		}
		else
		{
			ThisParent = LinkerRoot;
		}

		// If loading the object's Outer caused the object to be loaded or if it was a forced export package created
		// above, return it.
		if( Export.Object != NULL )
		{
			return Export.Object;
		}

		UObjectRedirector* ParentRedirector = Cast<UObjectRedirector>(ThisParent);
		if( ThisParent == NULL || ParentRedirector)
		{
			// mark this export as unloadable (so that other exports that
			// reference this one won't continue to execute the above logic), then return NULL
			Export.bExportLoadFailed = true;

			// otherwise, return NULL and let the calling code determine what to do
			FString OuterName = Export.OuterIndex.IsNull() ? LinkerRoot->GetFullName() : GetFullImpExpName(Export.OuterIndex);
			if (ParentRedirector)
			{
				UE_LOG(LogLinker, Warning, TEXT("CreateExport: Failed to load Outer for resource because it is a redirector '%s': %s"), *Export.ObjectName.ToString(), *OuterName);
			}
			else
			{
				UE_LOG(LogLinker, Warning, TEXT("CreateExport: Failed to load Outer for resource '%s': %s"), *Export.ObjectName.ToString(), *OuterName);
			}
			return NULL;
		}

		// Find the Archetype object for the one we are loading.
		UObject* Template = GetArchetypeFromRequiredInfo(LoadClass, ThisParent, Export.ObjectName, !!(Export.ObjectFlags&RF_ClassDefaultObject));

		check(Template);
		checkfSlow(((Export.ObjectFlags&RF_ClassDefaultObject)!=0 || Template->IsA(LoadClass)), TEXT("Mismatch between template %s and load class %s.  If this is a legacy blueprint or map, it may need to be resaved with bRecompileOnLoad turned off."), *Template->GetPathName(), *LoadClass->GetPathName());
		
		// we also need to ensure that the template has set up any instances
		Template->ConditionalPostLoadSubobjects();

		// Try to find existing object first in case we're a forced export to be able to reconcile. Also do it for the
		// case of async loading as we cannot in-place replace objects.

		UObject* ActualObjectWithTheName = StaticFindObjectFastInternal(NULL, ThisParent, Export.ObjectName, true);

		if(	(FApp::IsGame() && !GIsEditor && !IsRunningCommandlet()) 
		||	GIsAsyncLoading 
		||	Export.bForcedExport 
		||	LinkerRoot->ShouldFindExportsInMemoryFirst()
			)
		{
			// Find object after making sure it isn't already set. This would be bad as the code below NULLs it in a certain
			// case, which if it had been set would cause a linker detach mismatch.
			check( Export.Object == NULL );
			if (ActualObjectWithTheName && (ActualObjectWithTheName->GetClass() == LoadClass))
			{
				Export.Object = ActualObjectWithTheName;
			}

			// Object is found in memory.
			if( Export.Object )
			{
				// Mark that we need to dissociate forced exports later on if we are a forced export.
				if( Export.bForcedExport )
				{
					GForcedExportCount++;
				}
				// Associate linker with object to avoid detachment mismatches.
				else
				{
					Export.Object->SetLinker( this, Index );

					// If this object was allocated but never loaded (components created by a constructor) make sure it gets loaded
					// Do this for all subobjects created in the native constructor.
					GObjLoaded.AddUnique( Export.Object );
					if( Export.Object->HasAnyFlags(RF_DefaultSubObject) 
						|| (ThisParent && ThisParent->HasAnyFlags(RF_ClassDefaultObject)))
					{
						Export.Object->SetFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_WasLoaded);
					}
				}
				return Export.Object;
			}
		}

		// In cases when an object has been consolidated but its package hasn't been saved, look for UObjectRedirector before
		// constructing the object and loading it again from disk (the redirector hasn't been saved yet so it's not part of the package)
#if WITH_EDITOR
		if ( GIsEditor && GIsRunning && !Export.Object )
		{
			UObjectRedirector* Redirector = (UObjectRedirector*)StaticFindObject(UObjectRedirector::StaticClass(), ThisParent, *Export.ObjectName.ToString(), 1);
			if (Redirector && Redirector->DestinationObject && Redirector->DestinationObject->IsA(LoadClass))
			{
				// A redirector has been found, replace this export with it.
				LoadClass = UObjectRedirector::StaticClass();
				// Create new import for UObjectRedirector class
				FObjectImport* RedirectorImport = new( ImportMap )FObjectImport( UObjectRedirector::StaticClass() );
				GObjLoadersWithNewImports.Add(this);
				GImportCount++;
				Export.ClassIndex = FPackageIndex::FromImport(ImportMap.Num() - 1);
				Export.Object = Redirector;
				Export.Object->SetLinker( this, Index );
				// Return the redirector. It will be handled properly by the calling code
				return Export.Object;
			}
		}
#endif // WITH_EDITOR

		// Create the export object, marking it with the appropriate flags to
		// indicate that the object's data still needs to be loaded.
		if (ActualObjectWithTheName && !ActualObjectWithTheName->GetClass()->IsChildOf(LoadClass))
		{
			UE_LOG(LogLinker, Error, TEXT("Failed import: class '%s' name '%s' outer '%s'. There is another object (of '%s' class) at the path."),
				*LoadClass->GetName(), *Export.ObjectName.ToString(), *ThisParent->GetName(), *ActualObjectWithTheName->GetClass()->GetName());
			return NULL;
		}

		EObjectFlags ObjectLoadFlags = Export.ObjectFlags;
		// if we are loading objects just to verify an object reference during script compilation,
		if (!GVerifyObjectReferencesOnly
		||	(ObjectLoadFlags&RF_ClassDefaultObject) != 0					// only load this object if it's a class default object
		||	(LinkerRoot->PackageFlags&PKG_ContainsScript) != 0		// or we're loading an existing package and it's a script package
		||	ThisParent->IsTemplate(RF_ClassDefaultObject)			// or if its a subobject template in a CDO
		||	LoadClass->IsChildOf(UField::StaticClass())				// or if it is a UField
		||	LoadClass->IsChildOf(UObjectRedirector::StaticClass()))	// or if its a redirector to another object
		{
			ObjectLoadFlags = EObjectFlags(ObjectLoadFlags |RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_WasLoaded);
		}

		FName NewName = Export.ObjectName;

		LoadClass->GetDefaultObject();


		Export.Object = StaticConstructObject
		(
			LoadClass,
			ThisParent,
			NewName,
			EObjectFlags(ObjectLoadFlags | (GIsInitialLoad ? RF_RootSet : 0)),
			Template
		);
		LoadClass = Export.Object->GetClass(); // this may have changed if we are overwriting a CDO component

		if (NewName != Export.ObjectName)
		{
			// create a UObjectRedirector with the same name as the old object we are redirecting
			UObjectRedirector* Redir = (UObjectRedirector*)StaticConstructObject(UObjectRedirector::StaticClass(), Export.Object->GetOuter(), Export.ObjectName, RF_Standalone | RF_Public);
			// point the redirector object to this object
			Redir->DestinationObject = Export.Object;
		}
		
		if( Export.Object )
		{
			// Check to see if LoadClass is a blueprint, which potentially needs to be refreshed and regenerated.  If so, regenerate and patch it back into the export table
			if( !LoadClass->bCooked
				&& LoadClass->ClassGeneratedBy 
				&& (LoadClass->GetOutermost() != GetTransientPackage()) 
				&& ((Export.ObjectFlags&RF_ClassDefaultObject) != 0) )
			{
				{
					// For classes that are about to be regenerated, make sure we register them with the linker, so future references to this linker index will be valid
					const EObjectFlags OldFlags = Export.Object->GetFlags();
					Export.Object->ClearFlags(RF_NeedLoad|RF_NeedPostLoad);
					Export.Object->SetLinker( this, Index, false );
					Export.Object->SetFlags(OldFlags);
				}

				if ( RegenerateBlueprintClass(LoadClass, Export.Object) )
				{
					return Export.Object;
				}
			}
			else
			{
				// we created the object, but the data stored on disk for this object has not yet been loaded,
				// so add the object to the list of objects that need to be loaded, which will be processed
				// in EndLoad()
				Export.Object->SetLinker( this, Index );
				GObjLoaded.Add( Export.Object );
			}
		}
		else
		{
			UE_LOG(LogLinker, Warning, TEXT("ULinker::CreatedExport failed to construct object %s %s"), *LoadClass->GetName(), *Export.ObjectName.ToString() );
		}

		if ( Export.Object != NULL )
		{
			// If it's a struct or class, set its parent.
			if( Export.Object->IsA(UStruct::StaticClass()) )
			{
				if ( !Export.SuperIndex.IsNull() )
				{
					((UStruct*)Export.Object)->SetSuperStruct( (UStruct*)IndexToObject( Export.SuperIndex ) );
				}

				// If it's a class, bind it to C++.
				if( Export.Object->IsA( UClass::StaticClass() ) )
				{
					UClass* ClassObject = static_cast<UClass*>(Export.Object);

#if WITH_EDITOR
					// Before we serialize the class, begin a scoped class dependency gather to create a list of other classes that may need to be recompiled
					FScopedClassDependencyGather DependencyHelper(ClassObject);
#endif //WITH_EDITOR

					ClassObject->Bind();

 					// Preload classes on first access.  Note that this may update the Export.Object, so ClassObject is not guaranteed to be valid after this point
					// If we're async loading on a cooked build we can skip this as there's no chance we will need to recompile the class. 
					// Preload will be called during async package tick when the data has been precached
					if( !FPlatformProperties::RequiresCookedData() )
					{
 						Preload( Export.Object );
					}
				}
			}
	
			// Mark that we need to dissociate forced exports later on.
			if( Export.bForcedExport )
			{
				GForcedExportCount++;
			}
		}
	}
	return Export.Object;
}

// Return the loaded object corresponding to an import index; any errors are fatal.
UObject* ULinkerLoad::CreateImport( int32 Index )
{
	FScopedCreateImportCounter ScopedCounter( this, Index );
	FObjectImport& Import = ImportMap[ Index ];

	if( Import.XObject == NULL )
	{
		// Look in memory first.
		if (!GIsEditor && !IsRunningCommandlet())
		{
			// Try to find existing version in memory first.
			if( UPackage* ClassPackage = FindObjectFast<UPackage>( NULL, Import.ClassPackage, false, false ) )
			{
				if( UClass*	FindClass = FindObjectFast<UClass>( ClassPackage, Import.ClassName, false, false ) )
				{
					// Make sure the class has been loaded and linked before creating a CDO.
					// This is an edge case, but can happen if a blueprint package has not finished creating exports for a class
					// during async loading, and another package creates the class via CreateImport while in cooked builds because
					// we don't call preload immediately after creating a class in CreateExport like in non-cooked builds.
					Preload( FindClass );

					FindClass->GetDefaultObject(); // build the CDO if it isn't already built
					UObject*	FindObject		= NULL;
	
					// Import is a toplevel package.
					if( Import.OuterIndex.IsNull() )
					{
						FindObject = CreatePackage( NULL, *Import.ObjectName.ToString() );
					}
					// Import is regular import/ export.
					else
					{
						// Find the imports' outer.
						UObject* FindOuter = NULL;
						// Import.
						if( Import.OuterIndex.IsImport() )
						{
							FObjectImport& OuterImport = Imp(Import.OuterIndex);
							// Outer already in memory.
							if( OuterImport.XObject )
							{
								FindOuter = OuterImport.XObject;
							}
							// Outer is toplevel package, create/ find it.
							else if( OuterImport.OuterIndex.IsNull() )
							{
								FindOuter = CreatePackage( NULL, *OuterImport.ObjectName.ToString() );
							}
							// Outer is regular import/ export, use IndexToObject to potentially recursively load/ find it.
							else
							{
								FindOuter = IndexToObject( Import.OuterIndex );
							}
						}
						// Export.
						else 
						{
							// Create/ find the object's outer.
							FindOuter = IndexToObject( Import.OuterIndex );
						}
						if (!FindOuter)
						{
							FString OuterName = Import.OuterIndex.IsNull() ? LinkerRoot->GetFullName() : GetFullImpExpName(Import.OuterIndex);
							UE_LOG(LogLinker, Warning, TEXT("CreateImport: Failed to load Outer for resource '%s': %s"), *Import.ObjectName.ToString(), *OuterName);
							return NULL;
						}
	
						// Find object now that we know it's class, outer and name.
						FindObject = StaticFindObjectFast( FindClass, FindOuter, Import.ObjectName, false, false );
					}

					if( FindObject )
					{		
						// Associate import and indicate that we associated an import for later cleanup.
						Import.XObject = FindObject;
						GImportCount++;
						GObjLoadersWithNewImports.Add(this);
					}
				}
			}
		}

		if( Import.XObject == NULL )
		{
			if( Import.SourceLinker == NULL )
			{
				VerifyImport(Index);
			}
			if(Import.SourceIndex != INDEX_NONE)
			{
				check(Import.SourceLinker);
				Import.XObject = Import.SourceLinker->CreateExport( Import.SourceIndex );
				// If an object has been replaced (consolidated) in the editor and its package hasn't been saved yet
				// it's possible to get UbjectRedirector here as the original export is dynamically replaced
				// with the redirector (the original object has been deleted but the data on disk hasn't been updated)
#if WITH_EDITOR
				if( GIsEditor )
				{
					UObjectRedirector* Redirector = Cast<UObjectRedirector>(Import.XObject);
					if( Redirector )
					{
						Import.XObject = Redirector->DestinationObject;
					}
				}
#endif
				GImportCount++;
				GObjLoadersWithNewImports.Add(this);
			}
		}
	}
	return Import.XObject;
}

// Map an import/export index to an object; all errors here are fatal.
UObject* ULinkerLoad::IndexToObject( FPackageIndex Index )
{
	if( Index.IsExport() )
	{
		check(ExportMap.IsValidIndex( Index.ToExport() ) );
		return CreateExport( Index.ToExport() );
	}
	else if( Index.IsImport() )
	{
		check(ImportMap.IsValidIndex( Index.ToImport() ) );
		return CreateImport( Index.ToImport() );
	}
	else 
	{
		return NULL;
	}
}



// Detach an export from this linker.
void ULinkerLoad::DetachExport( int32 i )
{
	FObjectExport& E = ExportMap[ i ];
	check(E.Object);
	if( !E.Object->IsValidLowLevel() )
	{
		UE_LOG(LogLinker, Fatal, TEXT("Linker object %s %s.%s is invalid"), *GetExportClassName(i).ToString(), *LinkerRoot->GetName(), *E.ObjectName.ToString() );
	}
	if( E.Object->GetLinker()!=this )
	{
		UObject* Object = E.Object;
		UE_LOG(LogLinker, Log, TEXT("Object            : %s"), *Object->GetFullName() );
		UE_LOG(LogLinker, Log, TEXT("Object Linker     : %s"), *Object->GetLinker()->GetFullName() );
		UE_LOG(LogLinker, Log, TEXT("Linker LinkerRoot : %s"), Object->GetLinker() ? *Object->GetLinker()->LinkerRoot->GetFullName() : TEXT("None") );
		UE_LOG(LogLinker, Log, TEXT("Detach Linker     : %s"), *GetFullName() );
		UE_LOG(LogLinker, Log, TEXT("Detach LinkerRoot : %s"), *LinkerRoot->GetFullName() );
		UE_LOG(LogLinker, Fatal, TEXT("Linker object %s %s.%s mislinked!"), *GetExportClassName(i).ToString(), *LinkerRoot->GetName(), *E.ObjectName.ToString() );
	}
	check(E.Object->GetLinkerIndex() == i);
	ExportMap[i].Object->SetLinker( NULL, INDEX_NONE );
}

//@todo. Remove extern of global during next linker cleanup
extern TArray<ULinkerLoad*> GDelayedLinkerClosePackages;

/**
 * Detaches linker from exports and removes itself from array of loaders.
 */
void ULinkerLoad::Detach( bool bEnsureAllBulkDataIsLoaded )
{
#if WITH_EDITOR
	// Detach all lazy loaders.
	DetachAllBulkData( bEnsureAllBulkDataIsLoaded );
#endif
	// Detach all objects linked with this linker.
	for( int32 i=0; i<ExportMap.Num(); i++ )
	{	
		if( ExportMap[i].Object )
		{
			DetachExport( i );
		}
	}

	// Remove from object manager, if it has been added.
	GObjLoaders.Remove( this->LinkerRoot );
	GObjLoadersWithNewImports.Remove(this);
	if (!FPlatformProperties::HasEditorOnlyData())
	{
		GDelayedLinkerClosePackages.Remove(this);
	}
	if( Loader )
	{
		delete Loader;
	}
	Loader = NULL;

	// Empty out no longer used arrays.
	NameMap.Empty();
	ImportMap.Empty();
	ExportMap.Empty();

	// Make sure we're never associated with LinkerRoot again.
	LinkerRoot = NULL;
}

void ULinkerLoad::BeginDestroy()
{
	// Detaches linker.
	Detach( false );
	Super::BeginDestroy();
}

#if WITH_EDITOR

/**
 * Attaches/ associates the passed in bulk data object with the linker.
 *
 * @param	Owner		UObject owning the bulk data
 * @param	BulkData	Bulk data object to associate
 */
void ULinkerLoad::AttachBulkData( UObject* Owner, FUntypedBulkData* BulkData )
{
	check( BulkDataLoaders.Find(BulkData)==INDEX_NONE );
	BulkDataLoaders.Add( BulkData );
}

/**
 * Detaches the passed in bulk data object from the linker.
 *
 * @param	BulkData	Bulk data object to detach
 * @param	bEnsureBulkDataIsLoaded	Whether to ensure that the bulk data is loaded before detaching
 */
void ULinkerLoad::DetachBulkData( FUntypedBulkData* BulkData, bool bEnsureBulkDataIsLoaded )
{
	int32 RemovedCount = BulkDataLoaders.Remove( BulkData );
	if( RemovedCount!=1 )
	{	
		UE_LOG(LogLinker, Fatal, TEXT("Detachment inconsistency: %i (%s)"), RemovedCount, *Filename );
	}
	BulkData->DetachFromArchive( this, bEnsureBulkDataIsLoaded );
}

/**
 * Detaches all attached bulk  data objects.
 *
 * @param	bEnsureBulkDataIsLoaded	Whether to ensure that the bulk data is loaded before detaching
 */
void ULinkerLoad::DetachAllBulkData( bool bEnsureAllBulkDataIsLoaded )
{
	for( int32 BulkDataIndex=0; BulkDataIndex<BulkDataLoaders.Num(); BulkDataIndex++ )
	{
		FUntypedBulkData* BulkData = BulkDataLoaders[BulkDataIndex];
		check( BulkData );
		BulkData->DetachFromArchive( this, bEnsureAllBulkDataIsLoaded );
	}
	BulkDataLoaders.Empty();
}

#endif // WITH_EDITOR

/**
 * Hint the archive that the region starting at passed in offset and spanning the passed in size
 * is going to be read soon and should be precached.
 *
 * The function returns whether the precache operation has completed or not which is an important
 * hint for code knowing that it deals with potential async I/O. The archive is free to either not 
 * implement this function or only partially precache so it is required that given sufficient time
 * the function will return true. Archives not based on async I/O should always return true.
 *
 * This function will not change the current archive position.
 *
 * @param	PrecacheOffset	Offset at which to begin precaching.
 * @param	PrecacheSize	Number of bytes to precache
 * @return	false if precache operation is still pending, true otherwise
 */
bool ULinkerLoad::Precache( int64 PrecacheOffset, int64 PrecacheSize )
{
	return Loader->Precache( PrecacheOffset, PrecacheSize );
}

void ULinkerLoad::Seek( int64 InPos )
{
	Loader->Seek( InPos );
}

int64 ULinkerLoad::Tell()
{
	return Loader->Tell();
}

int64 ULinkerLoad::TotalSize()
{
	return Loader->TotalSize();
}

FArchive& ULinkerLoad::operator<<( UObject*& Object )
{
	FPackageIndex Index;
	FArchive& Ar = *this;
	Ar << Index;

	UObject* Temporary = NULL;
	Temporary = IndexToObject( Index );

	FMemory::Memcpy(&Object, &Temporary, sizeof(UObject*));
	return *this;
}

FArchive& ULinkerLoad::operator<<( FLazyObjectPtr& LazyObjectPtr)
{
	FArchive& Ar = *this;
	FUniqueObjectGuid ID;
	Ar << ID;
	LazyObjectPtr = ID;
	return Ar;
}

FArchive& ULinkerLoad::operator<<( FAssetPtr& AssetPtr)
{
	FArchive& Ar = *this;
	FStringAssetReference ID;
	Ar << ID;
	AssetPtr = ID;
	return Ar;
}

FArchive& ULinkerLoad::operator<<( FName& Name )
{
	NAME_INDEX NameIndex;
	FArchive& Ar = *this;
	Ar << NameIndex;

	if( !NameMap.IsValidIndex(NameIndex) )
	{
		UE_LOG(LogLinker, Fatal, TEXT("Bad name index %i/%i"), NameIndex, NameMap.Num() );
	}

	// if the name wasn't loaded (because it wasn't valid in this context)
	if (NameMap[NameIndex] == NAME_None)
	{
		int32 TempNumber;
		Ar << TempNumber;
		Name = NAME_None;
	}
	else
	{
		int32 Number;
		Ar << Number;
		// simply create the name from the NameMap's name index and the serialized instance number
#ifndef __clang__
		Name = FName((EName)NameMap[NameIndex].GetIndex(), Number);
#else
		// @todo-mobile: IOS crashes on the assignment; need to do a memcpy manually...
		FName TempName = FName((EName)NameMap[NameIndex].GetIndex(), Number);
		FMemory::Memcpy(&Name, &TempName, sizeof(FName));
#endif
	}

	return *this;
}

void ULinkerLoad::Serialize( void* V, int64 Length )
{
	Loader->Serialize( V, Length );
}

/**
* Kick off an async load of a package file into memory
* 
* @param PackageName Name of package to read in. Must be the same name as passed into LoadPackage
*/
void ULinkerLoad::AsyncPreloadPackage(const TCHAR* PackageName)
{
	// get package filename
	FString PackageFilename;
	if (!FPackageName::DoesPackageExist(PackageName, NULL, &PackageFilename))
	{
		UE_LOG(LogLinker, Fatal,TEXT("Failed to find file for package %s for async preloading."), PackageName);
	}

	// make sure it wasn't already there
	check(PackagePrecacheMap.Find(*PackageFilename) == NULL);

	// add a new one to the map
	FPackagePrecacheInfo& PrecacheInfo = PackagePrecacheMap.Add(*PackageFilename, FPackagePrecacheInfo());

	// make a new sync object (on heap so the precache info can be copied in the map, etc)
	PrecacheInfo.SynchronizationObject = new FThreadSafeCounter;

	// increment the sync object, later we'll wait for it to be decremented
	PrecacheInfo.SynchronizationObject->Increment();
	
	// default to not compressed
	bool bWasCompressed = false;

	// get filesize (first checking if it was compressed)
	const int32 UncompressedSize = -1;
	const int32 FileSize = IFileManager::Get().FileSize(*PackageFilename);

	// if we were compressed, the size we care about on the other end is the uncompressed size
	CA_SUPPRESS(6326)
	PrecacheInfo.PackageDataSize = UncompressedSize == -1 ? FileSize : UncompressedSize;
	
	// allocate enough space
	PrecacheInfo.PackageData = FMemory::Malloc(PrecacheInfo.PackageDataSize);

	uint64 RequestId;
	// kick off the async read (uncompressing if needed) of the whole file and make sure it worked
	CA_SUPPRESS(6326)
	if (UncompressedSize != -1)
	{
		PrecacheInfo.PackageDataSize = UncompressedSize;
		RequestId = FIOSystem::Get().LoadCompressedData(
						PackageFilename, 
						0, 
						FileSize, 
						UncompressedSize, 
						PrecacheInfo.PackageData, 
						COMPRESS_Default, 
						PrecacheInfo.SynchronizationObject,
						AIOP_Normal);
	}
	else
	{
		PrecacheInfo.PackageDataSize = FileSize;
		RequestId = FIOSystem::Get().LoadData(
						PackageFilename, 
						0, 
						PrecacheInfo.PackageDataSize, 
						PrecacheInfo.PackageData, 
						PrecacheInfo.SynchronizationObject, 
						AIOP_Normal);
	}

	// give a hint to the IO system that we are done with this file for now
	FIOSystem::Get().HintDoneWithFile(PackageFilename);

	check(RequestId);
}

/**
 * Called when an object begins serializing property data using script serialization.
 */
void ULinkerLoad::MarkScriptSerializationStart( const UObject* Obj )
{
	if ( Obj != NULL && Obj->GetLinker() == this && ExportMap.IsValidIndex(Obj->GetLinkerIndex()) )
	{
		FObjectExport& Export = ExportMap[Obj->GetLinkerIndex()];
		Export.ScriptSerializationStartOffset = Tell();
	}
}

/**
 * Called when an object stops serializing property data using script serialization.
 */
void ULinkerLoad::MarkScriptSerializationEnd( const UObject* Obj )
{
	if ( Obj != NULL && Obj->GetLinker() == this && ExportMap.IsValidIndex(Obj->GetLinkerIndex()) )
	{
		FObjectExport& Export = ExportMap[Obj->GetLinkerIndex()];
		Export.ScriptSerializationEndOffset = Tell();
	}
}

/**
 * Locates the class adjusted index and its package adjusted index for a given class name in the import map
 */
bool ULinkerLoad::FindImportClassAndPackage( FName ClassName, FPackageIndex &ClassIdx, FPackageIndex &PackageIdx )
{
	for ( int32 ImportMapIdx = 0; ImportMapIdx < ImportMap.Num(); ImportMapIdx++ )
	{
		if ( ImportMap[ImportMapIdx].ObjectName == ClassName && ImportMap[ImportMapIdx].ClassName == NAME_Class )
		{
			ClassIdx = FPackageIndex::FromImport(ImportMapIdx);
			PackageIdx = ImportMap[ImportMapIdx].OuterIndex;
			return true;
		}
	}

	return false;
}

/**
* Attempts to find the index for the given class object in the import list and adds it + its package if it does not exist
*/
bool ULinkerLoad::CreateImportClassAndPackage( FName ClassName, FName PackageName, FPackageIndex &ClassIdx, FPackageIndex &PackageIdx )
{
	//look for an existing import first
	//might as well look for the package at the same time ...
	bool bPackageFound = false;		
	for ( int32 ImportMapIdx = 0; ImportMapIdx < ImportMap.Num(); ImportMapIdx++ )
	{
		//save one iteration by checking for the package in this loop
		if( PackageName != NAME_None && ImportMap[ImportMapIdx].ClassName == NAME_Package && ImportMap[ImportMapIdx].ObjectName == PackageName )
		{
			bPackageFound = true;
			PackageIdx = FPackageIndex::FromImport(ImportMapIdx);
		}
		if ( ImportMap[ImportMapIdx].ObjectName == ClassName && ImportMap[ImportMapIdx].ClassName == NAME_Class )
		{
			ClassIdx = FPackageIndex::FromImport(ImportMapIdx);
			PackageIdx = ImportMap[ImportMapIdx].OuterIndex;
			return true;
		}
	}

	//an existing import couldn't be found, so add it
	//first add the needed package if it didn't already exist in the import map
	if( !bPackageFound )
	{
		int32 Index = ImportMap.AddUninitialized();
		ImportMap[Index].ClassName = NAME_Package;
		ImportMap[Index].ClassPackage = GLongCoreUObjectPackageName;
		ImportMap[Index].ObjectName = PackageName;
		ImportMap[Index].OuterIndex = FPackageIndex();
		ImportMap[Index].XObject = 0;
		ImportMap[Index].SourceLinker = 0;
		ImportMap[Index].SourceIndex = -1;
		PackageIdx = FPackageIndex::FromImport(Index);
	}
	{
		//now add the class import
		int32 Index = ImportMap.AddUninitialized();
		ImportMap[Index].ClassName = NAME_Class;
		ImportMap[Index].ClassPackage = GLongCoreUObjectPackageName;
		ImportMap[Index].ObjectName = ClassName;
		ImportMap[Index].OuterIndex = PackageIdx;
		ImportMap[Index].XObject = 0;
		ImportMap[Index].SourceLinker = 0;
		ImportMap[Index].SourceIndex = -1;
		ClassIdx = FPackageIndex::FromImport(Index);
	}

	return true;
}

TArray<FName> ULinkerLoad::FindPreviousNamesForClass(FString CurrentClassPath, bool bIsInstance)
{
	TArray<FName> OldNames;
	for (auto It = ObjectNameRedirects.CreateConstIterator(); It; ++It)
	{
		if (It.Value().ToString() == CurrentClassPath)
		{
			OldNames.Add(It.Key());
		}
	}

	if (bIsInstance)
	{
		for (auto It = ObjectNameRedirectsInstanceOnly.CreateConstIterator(); It; ++It)
		{
			if (It.Value().ToString() == CurrentClassPath)
			{
				OldNames.Add(It.Key());
			}
		}
	}

	return OldNames;
}

FName ULinkerLoad::FindNewNameForClass(FName OldClassName, bool bIsInstance)
{
	FName *RedirectName = ObjectNameRedirects.Find(OldClassName);

	if (RedirectName)
	{
		return *RedirectName;
	}

	if (bIsInstance)
	{
		RedirectName = ObjectNameRedirectsInstanceOnly.Find(OldClassName);
	}

	if (RedirectName)
	{
		return *RedirectName;
	}

	return NAME_None;
}


/**
* Allows object instances to be converted to other classes upon loading a package
*/
ULinkerLoad::ELinkerStatus ULinkerLoad::FixupExportMap()
{
	// No need to fixup exports if everything is cooked.
	if (!FPlatformProperties::RequiresCookedData())
	{
		if (bFixupExportMapDone)
		{
			return LINKER_Loaded;
		}

		for ( int32 ExportMapIdx = 0; ExportMapIdx < ExportMap.Num(); ExportMapIdx++ )
		{
			FObjectExport &Export = ExportMap[ExportMapIdx];
			FName NameClass = GetExportClassName(ExportMapIdx);
			FName NamePackage = GetExportClassPackage(ExportMapIdx);

			{
				FSubobjectRedirect *Redirect = SubobjectNameRedirects.Find(Export.ObjectName);
				if (Redirect)
				{
					if (NameClass == Redirect->MatchClass)
					{
						if (!Export.OuterIndex.IsNull())
						{
							FString Was = GetExportFullName(ExportMapIdx);
							Export.ObjectName = Redirect->NewName;

							if (Export.ObjectName != NAME_None)
							{
								FString Now = GetExportFullName(ExportMapIdx);
								UE_LOG(LogLinker, Log, TEXT("ULinkerLoad::FixupExportMap() - Renamed component from %s   to   %s"), *Was, *Now);
							}
							else
							{
								Export.bExportLoadFailed = true;
								UE_LOG(LogLinker, Log, TEXT("ULinkerLoad::FixupExportMap() - Removed component %s"), *Was);
							}
							continue;
						}
					}
				}
			}
			FName *RedirectName = ObjectNameRedirectsInstanceOnly.Find(NameClass);
			if ( RedirectName )
			{
				FString StrRedirectName, ResultPackage, ResultClass;
				FString StrObjectName = Export.ObjectName.ToString();

				StrRedirectName = RedirectName->ToString();

				// Accepts either "PackageName.ClassName" or just "ClassName"
				int32 Offset = StrRedirectName.Find(TEXT("."));
				if ( Offset >= 0 )
				{
					// A package class name redirect
					ResultPackage = StrRedirectName.Left(Offset);
					ResultClass = StrRedirectName.Right(StrRedirectName.Len() - Offset - 1);
				}
				else
				{
					// Just a class name change within the same package
					ResultPackage = NamePackage.ToString();
					ResultClass = StrRedirectName;
				}

				// Never modify the default object instances
				if ( StrObjectName.Left(9) != TEXT("Default__") )
				{
					FPackageIndex NewClassIndex;
					FPackageIndex NewPackageIndex;
					if ( ResultClass == TEXT("None") )
					{
						UE_LOG(LogLinker, Log, TEXT("ULinkerLoad::FixupExportMap() - Pkg<%s> [Obj<%s> Cls<%s> ClsPkg<%s>] -> removed"), *LinkerRoot->GetName(),
							*Export.ObjectName.ToString(), *NameClass.ToString(), *NamePackage.ToString());

						Export.ClassIndex = NewClassIndex;
						Export.OuterIndex = NewClassIndex;
						Export.ObjectName = NAME_None;
#if WITH_EDITOR
						Export.OldClassName = NameClass;
#endif
					}
					else if ( CreateImportClassAndPackage(*ResultClass, *ResultPackage, NewClassIndex, NewPackageIndex) )
					{
						Export.ClassIndex = NewClassIndex;
#if WITH_EDITOR
						Export.OldClassName = NameClass;
#endif
						//Export.OuterIndex = newPackageIndex;

						UE_LOG(LogLinker, Log, TEXT("ULinkerLoad::FixupExportMap() - Pkg<%s> [Obj<%s> Cls<%s> ClsPkg<%s>] -> [Obj<%s> Cls<%s> ClsPkg<%s>]"), *LinkerRoot->GetName(),
							*Export.ObjectName.ToString(), *NameClass.ToString(), *NamePackage.ToString(),
							*Export.ObjectName.ToString(), *ResultClass, *ResultPackage);
					}
					else
					{
						UE_LOG(LogLinker, Log, TEXT("ULinkerLoad::FixupExportMap() - object redirection failed at %s"), *Export.ObjectName.ToString());
					}
				}
			}
			else
			{
				//UE_LOG(LogLinker, Log, TEXT("Export: <%s>"), *( LinkerRoot->Name.ToString() + TEXT(".") + Export.ObjectName.ToString() ));
				RedirectName = ObjectNameRedirectsObjectOnly.Find(*( LinkerRoot->GetName() + TEXT(".") + *Export.ObjectName.ToString() ) );
				if ( RedirectName )
				{
					FString StrRedirectName, ResultPackage, ResultClass;
					FString StrObjectName = Export.ObjectName.ToString();

					StrRedirectName = RedirectName->ToString();

					// Accepts either "PackageName.ClassName" or just "ClassName"
					int32 Offset = StrRedirectName.Find(TEXT("."));
					if ( Offset >= 0 )
					{
						// A package class name redirect
						ResultPackage = StrRedirectName.Left(Offset);
						ResultClass = StrRedirectName.Right(StrRedirectName.Len() - Offset - 1);
					}
					else
					{
						ResultClass = StrRedirectName;
					}

					// Never modify the default object instances
					if ( StrObjectName.Left(9) != TEXT("Default__") )
					{
						FPackageIndex NewClassIndex;
						FPackageIndex NewPackageIndex;
						if ( CreateImportClassAndPackage(*ResultClass, *ResultPackage, NewClassIndex, NewPackageIndex) )
						{
							Export.ClassIndex = NewClassIndex;
#if WITH_EDITOR
							Export.OldClassName = NameClass;
#endif
							UE_LOG(LogLinker, Log, TEXT("ULinkerLoad::FixupExportMap() - Pkg<%s> [Obj<%s> Cls<%s> ClsPkg<%s>] -> [Obj<%s> Cls<%s> ClsPkg<%s>]"), *LinkerRoot->GetName(),
								*Export.ObjectName.ToString(), *NameClass.ToString(), *NamePackage.ToString(),
								*Export.ObjectName.ToString(), *ResultClass, *ResultPackage);
						}
						else
						{
							UE_LOG(LogLinker, Log, TEXT("ULinkerLoad::FixupExportMap() - object redirection failed at %s"), *Export.ObjectName.ToString());
						}
					}
				}
			}	
		}

		bFixupExportMapDone = true;
		return !IsTimeLimitExceeded( TEXT("fixing up export map") ) ? LINKER_Loaded : LINKER_TimedOut;
	}
	else
	{
		return LINKER_Loaded;
	}
}

IMPLEMENT_CORE_INTRINSIC_CLASS(ULinkerLoad, ULinker,
	{
	}
);

#undef LOCTEXT_NAMESPACE
