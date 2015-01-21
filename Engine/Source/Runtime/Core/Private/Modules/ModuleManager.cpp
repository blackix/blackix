// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "ModuleManager.h"
#include "ModuleVersion.h"
#include "EngineBuildSettings.h"
#include "UProjectInfo.h"


DEFINE_LOG_CATEGORY_STATIC(LogModuleManager, Log, All);

#if WITH_HOT_RELOAD
	/** If true, we are reloading a class for HotReload */
	CORE_API bool			GIsHotReload							= false;
#endif


int32 FModuleManager::FModuleInfo::CurrentLoadOrder = 1;


FModuleManager& FModuleManager::Get()
{
	// FModuleManager is not thread-safe
	ensure(IsInGameThread());

	// NOTE: The reason we initialize to NULL here is due to an issue with static initialization of variables with
	// constructors/destructors across DLL boundaries, where a function called from a statically constructed object
	// calls a function in another module (such as this function) that creates a static variable.  A crash can occur
	// because the static initialization of this DLL has not yet happened, and the CRT's list of static destructors
	// cannot be written to because it has not yet been initialized fully.	(@todo UE4 DLL)
	static FModuleManager* ModuleManager = NULL;

	if( ModuleManager == NULL )
	{
		ModuleManager = new FModuleManager();
	}

	return *ModuleManager;
}


FModuleManager::~FModuleManager()
{
	// NOTE: It may not be safe to unload modules by this point (static deinitialization), as other
	//       DLLs may have already been unloaded, which means we can't safely call clean up methods
}


void FModuleManager::FindModules(const TCHAR* WildcardWithoutExtension, TArray<FName>& OutModules)
{
	// @todo plugins: Try to convert existing use cases to use plugins, and get rid of this function
#if !IS_MONOLITHIC

	TMap<FName, FString> ModulePaths;
	FindModulePaths(WildcardWithoutExtension, ModulePaths);

	for(TMap<FName, FString>::TConstIterator Iter(ModulePaths); Iter; ++Iter)
	{
		if(CheckModuleCompatibility(*Iter.Value()))
		{
			OutModules.Add(Iter.Key());
		}
	}

#else
	FString Wildcard(WildcardWithoutExtension);
	for (FStaticallyLinkedModuleInitializerMap::TConstIterator It(StaticallyLinkedModuleInitializers); It; ++It)
	{
		if (It.Key().ToString().MatchesWildcard(Wildcard))
		{
			OutModules.Add(It.Key());
		}
	}
#endif
}


bool FModuleManager::IsModuleLoaded( const FName InModuleName ) const
{
	// Do we even know about this module?
	const TSharedRef< FModuleInfo >* ModuleInfoPtr = Modules.Find( InModuleName );
	if( ModuleInfoPtr != NULL )
	{
		const TSharedRef< FModuleInfo >& ModuleInfo( *ModuleInfoPtr );

		// Only if already loaded
		if( ModuleInfo->Module.IsValid()  )
		{
			// Module is loaded and ready
			return true;
		}
	}

	// Not loaded, or not fully initialized yet (StartupModule wasn't called)
	return false;
}


bool FModuleManager::IsModuleUpToDate( const FName InModuleName ) const
{
	TMap<FName, FString> ModulePathMap;
	FindModulePaths(*InModuleName.ToString(), ModulePathMap);

	if(ModulePathMap.Num() != 1)
	{
		return false;
	}

	return CheckModuleCompatibility(*TMap<FName, FString>::TConstIterator(ModulePathMap).Value());
}


void FModuleManager::AddModule( const FName InModuleName )
{
	// Do we already know about this module?  If not, we'll create information for this module now.
	if( ensureMsg( InModuleName != NAME_None, TEXT( "FModuleManager::AddModule() was called with an invalid module name (empty string or 'None'.)  This is not allowed." ) ) &&
		!Modules.Contains( InModuleName ) )
	{
		// Add this module to the set of modules that we know about
		TSharedRef< FModuleInfo > ModuleInfo( new FModuleInfo() );

#if !IS_MONOLITHIC
		FString ModuleNameString = InModuleName.ToString();

		TMap<FName, FString> ModulePathMap;
		FindModulePaths(*ModuleNameString, ModulePathMap);

		if(ModulePathMap.Num() == 1)
		{
			// Add this module to the set of modules that we know about
			ModuleInfo->OriginalFilename = TMap<FName, FString>::TConstIterator(ModulePathMap).Value();
			ModuleInfo->Filename = ModuleInfo->OriginalFilename;

			// When iterating on code during development, it's possible there are multiple rolling versions of this
			// module's DLL file.  This can happen if the programmer is recompiling DLLs while the game is loaded.  In
			// this case, we want to load the newest iteration of the DLL file, so that behavior is the same after
			// restarting the application.
			{
				// NOTE: We leave this enabled in UE_BUILD_SHIPPING editor builds so module authors can iterate on custom modules
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || (UE_BUILD_SHIPPING && WITH_EDITOR)
				// In some cases, sadly, modules may be loaded before appInit() is called.  We can't cleanly support rolling files for those modules.
				{
					// First, check to see if the module we added already exists on disk
					const FDateTime OriginalModuleFileTime = IFileManager::Get().GetTimeStamp( *ModuleInfo->OriginalFilename );
					if( OriginalModuleFileTime != FDateTime::MinValue() )
					{
						const FString ModuleName = *InModuleName.ToString();
						const int32 MatchPos = ModuleInfo->OriginalFilename.Find( ModuleName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
						if( ensureMsgf( MatchPos != INDEX_NONE, TEXT("Could not find module name '%s' in module filename '%s'"), *ModuleName, *ModuleInfo->OriginalFilename ) )
						{
							const int32 SuffixPos = MatchPos + ModuleName.Len();

							const FString Prefix = ModuleInfo->OriginalFilename.Left( SuffixPos );
							const FString Suffix = ModuleInfo->OriginalFilename.Right( ModuleInfo->OriginalFilename.Len() - SuffixPos );

							const FString ModuleFileSearchString = FString::Printf( TEXT( "%s-*%s" ), *Prefix, *Suffix);
							const FString ModuleFileSearchDirectory = FPaths::GetPath(ModuleFileSearchString);

							// Search for module files
							TArray<FString> FoundFiles;
							IFileManager::Get().FindFiles( FoundFiles, *ModuleFileSearchString, true, false );

							// Figure out what the newest module file is
							int32 NewestFoundFileIndex = INDEX_NONE;
							FDateTime NewestFoundFileTime = OriginalModuleFileTime;
							if( FoundFiles.Num() > 0 )
							{
								for( int32 CurFileIndex = 0; CurFileIndex < FoundFiles.Num(); ++CurFileIndex )
								{
									// FoundFiles contains file names with no directory information, but we need the full path up
									// to the file, so we'll prefix it back on if we have a path.
									const FString& FoundFile = FoundFiles[ CurFileIndex ];
									const FString FoundFilePath = ModuleFileSearchDirectory.IsEmpty() ? FoundFile : ( ModuleFileSearchDirectory / FoundFile );

									// need to reject some files here that are not numbered...release executables, do have a suffix, so we need to make sure we don't find the debug version
									check(FoundFilePath.Len() > Prefix.Len() + Suffix.Len());
									FString Center = FoundFilePath.Mid(Prefix.Len(), FoundFilePath.Len() - Prefix.Len() - Suffix.Len());
									check(Center.StartsWith(TEXT("-"))); // a minus sign is still considered numeric, so we can leave it.
									if (!Center.IsNumeric())
									{
										// this is a debug DLL or something, it is not a numbered hot DLL
										continue;
									}


									// Check the time stamp for this file
									const FDateTime FoundFileTime = IFileManager::Get().GetTimeStamp( *FoundFilePath );
									if( ensure( FoundFileTime != -1.0 ) )
									{
										// Was this file modified more recently than our others?
										if( FoundFileTime > NewestFoundFileTime )
										{
											NewestFoundFileIndex = CurFileIndex;
											NewestFoundFileTime = FoundFileTime;
										}
									}
									else
									{
										// File wasn't found, should never happen as we searched for these files just now
									}
								}


								// Did we find a variant of the module file that is newer than our original file?
								if( NewestFoundFileIndex != INDEX_NONE )
								{
									// Update the module working file name to the most recently-modified copy of that module
									const FString& NewestModuleFilename = FoundFiles[ NewestFoundFileIndex ];
									const FString NewestModuleFilePath = ModuleFileSearchDirectory.IsEmpty() ? NewestModuleFilename : ( ModuleFileSearchDirectory / NewestModuleFilename );
									ModuleInfo->Filename = NewestModuleFilePath;
								}
								else
								{
									// No variants were found that were newer than the original module file name, so
									// we'll continue to use that!
								}
							}
						}
					}
				}
#endif	// !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			}
		}
#endif	// !IS_MONOLITHIC

		// Update hash table
		Modules.Add( InModuleName, ModuleInfo );

		// List of known modules has changed.  Fire callbacks.
		ModulesChangedEvent.Broadcast( InModuleName, EModuleChangeReason::PluginDirectoryChanged );
	}
}


TSharedPtr<IModuleInterface> FModuleManager::LoadModule( const FName InModuleName, bool bWasReloaded )
{
	EModuleLoadResult FailureReason;
	return LoadModuleWithFailureReason(InModuleName, FailureReason, bWasReloaded );
}


TSharedPtr<IModuleInterface> FModuleManager::LoadModuleChecked( const FName InModuleName, const bool bWasReloaded )
{
	TSharedPtr<IModuleInterface> Module = LoadModule(InModuleName, bWasReloaded);
	check(Module.IsValid());

	return Module;
}


TSharedPtr<IModuleInterface> FModuleManager::LoadModuleWithFailureReason( const FName InModuleName, EModuleLoadResult& OutFailureReason, bool bWasReloaded /*=false*/ )
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Module Load"), STAT_ModuleLoad, STATGROUP_LoadTime);

	TSharedPtr<IModuleInterface> LoadedModule;
	OutFailureReason = EModuleLoadResult::Success;

	// Update our set of known modules, in case we don't already know about this module
	AddModule( InModuleName );

	// Grab the module info.  This has the file name of the module, as well as other info.
	TSharedRef< FModuleInfo > ModuleInfo = Modules.FindChecked( InModuleName );

	if( ModuleInfo->Module.IsValid() )
	{
		// Assign the already loaded module into the return value, otherwise the return value gives the impression the module failed load!
		LoadedModule = ModuleInfo->Module;
	}
	else
	{
		// Make sure this isn't a module that we had previously loaded, and then unloaded at shutdown time.
		//
		// If this assert goes off, your trying to load a module during the shutdown phase that was already
		// cleaned up.  The easiest way to fix this is to change your code to query for an already-loaded
		// module instead of trying to load it directly.
		checkf( ( !ModuleInfo->bWasUnloadedAtShutdown ), TEXT( "Attempted to load module '%s' that was already unloaded at shutdown.  FModuleManager::LoadModule() was called to load a module that was previously loaded, and was unloaded at shutdown time.  If this assert goes off, your trying to load a module during the shutdown phase that was already cleaned up.  The easiest way to fix this is to change your code to query for an already-loaded module instead of trying to load it directly." ), *InModuleName.ToString() );

		// Check if we're statically linked with the module.  Those modules register with the module manager using a static variable,
		// so hopefully we already know about the name of the module and how to initialize it.
		const FInitializeStaticallyLinkedModule* ModuleInitializerPtr = StaticallyLinkedModuleInitializers.Find( InModuleName );
		if( ModuleInitializerPtr != NULL )
		{
			const FInitializeStaticallyLinkedModule& ModuleInitializer( *ModuleInitializerPtr );

			// Initialize the module!
			ModuleInfo->Module = MakeShareable( ModuleInitializer.Execute() );

			if( ModuleInfo->Module.IsValid() )
			{
				// Startup the module
				ModuleInfo->Module->StartupModule();

				// Module was started successfully!  Fire callbacks.
				ModulesChangedEvent.Broadcast( InModuleName, EModuleChangeReason::ModuleLoaded );

				// Set the return parameter
				LoadedModule = ModuleInfo->Module;
			}
			else
			{
				UE_LOG(LogModuleManager, Warning, TEXT( "ModuleManager: Unable to load module '%s' because InitializeModule function failed (returned NULL pointer.)" ), *InModuleName.ToString() );
				OutFailureReason = EModuleLoadResult::FailedToInitialize;
			}
		}
#if IS_MONOLITHIC
		else
		{
			// Monolithic builds that do not have the initializer were *not found* during the build step, so return FileNotFound
			// (FileNotFound is an acceptable error in some case - ie loading a content only project)
			UE_LOG(LogModuleManager, Warning, TEXT( "ModuleManager: Module '%s' not found - its StaticallyLinkedModuleInitializers function is null." ), *InModuleName.ToString() );
			OutFailureReason = EModuleLoadResult::FileNotFound;
		}
#endif
#if !IS_MONOLITHIC
		else
		{
			// Make sure that any UObjects that need to be registered were already processed before we go and
			// load another module.  We just do this so that we can easily tell whether UObjects are present
			// in the module being loaded.
			if( bCanProcessNewlyLoadedObjects )
			{
				ProcessLoadedObjectsCallback.Broadcast();
			}

			// Try to dynamically load the DLL

			UE_LOG(LogModuleManager, Verbose, TEXT( "ModuleManager: Load Module '%s' DLL '%s'" ), *InModuleName.ToString(), *ModuleInfo->Filename);

			// Determine which file to load for this module.
			const FString ModuleFileToLoad = FPaths::ConvertRelativePathToFull(ModuleInfo->Filename);

			// Clear the handle and set it again below if the module is successfully loaded
			ModuleInfo->Handle = NULL;

			// Skip this check if file manager has not yet been initialized
			if ( FPaths::FileExists(ModuleFileToLoad) )
			{
				if ( CheckModuleCompatibility(*ModuleFileToLoad) )
				{
					ModuleInfo->Handle = FPlatformProcess::GetDllHandle(*ModuleFileToLoad);
					if( ModuleInfo->Handle != NULL )
					{
						// First things first.  If the loaded DLL has UObjects in it, then their generated code's
						// static initialization will have run during the DLL loading phase, and we'll need to
						// go in and make sure those new UObject classes are properly registered.
						{
							// Sometimes modules are loaded before even the UObject systems are ready.  We need to assume
							// these modules aren't using UObjects.
							if( bCanProcessNewlyLoadedObjects)
							{
								// OK, we've verified that loading the module caused new UObject classes to be
								// registered, so we'll treat this module as a module with UObjects in it.
								ProcessLoadedObjectsCallback.Broadcast();
							}
						}

						// Find our "InitializeModule" global function, which must exist for all module DLLs
						FInitializeModuleFunctionPtr InitializeModuleFunctionPtr =
							( FInitializeModuleFunctionPtr )FPlatformProcess::GetDllExport( ModuleInfo->Handle, TEXT( "InitializeModule" ) );
						if( InitializeModuleFunctionPtr != NULL )
						{
							// Initialize the module!
							ModuleInfo->Module = MakeShareable( InitializeModuleFunctionPtr() );

							if( ModuleInfo->Module.IsValid() )
							{
								// Startup the module
								ModuleInfo->Module->StartupModule();

								// Module was started successfully!  Fire callbacks.
								ModulesChangedEvent.Broadcast( InModuleName, EModuleChangeReason::ModuleLoaded );

								// Set the return parameter
								LoadedModule = ModuleInfo->Module;
							}
							else
							{
								UE_LOG(LogModuleManager, Warning, TEXT( "ModuleManager: Unable to load module '%s' because InitializeModule function failed (returned NULL pointer.)" ), *ModuleFileToLoad );

								FPlatformProcess::FreeDllHandle( ModuleInfo->Handle );
								ModuleInfo->Handle = NULL;
								OutFailureReason = EModuleLoadResult::FailedToInitialize;
							}
						}
						else
						{
							UE_LOG(LogModuleManager, Warning, TEXT( "ModuleManager: Unable to load module '%s' because InitializeModule function was not found." ), *ModuleFileToLoad );

							FPlatformProcess::FreeDllHandle( ModuleInfo->Handle );
							ModuleInfo->Handle = NULL;
							OutFailureReason = EModuleLoadResult::FailedToInitialize;
						}
					}
					else
					{
						UE_LOG(LogModuleManager, Warning, TEXT( "ModuleManager: Unable to load module '%s' because the file couldn't be loaded by the OS." ), *ModuleFileToLoad );
						OutFailureReason = EModuleLoadResult::CouldNotBeLoadedByOS;
					}
				}
				else
				{
					// The log warning about this failure reason is within CheckModuleCompatibility
					OutFailureReason = EModuleLoadResult::FileIncompatible;
				}
			}
			else
			{
				UE_LOG(LogModuleManager, Warning, TEXT( "ModuleManager: Unable to load module '%s' because the file '%s' was not found." ), *InModuleName.ToString(), *ModuleFileToLoad );
				OutFailureReason = EModuleLoadResult::FileNotFound;
			}
		}
#endif
	}

	return LoadedModule;
}


bool FModuleManager::UnloadModule( const FName InModuleName, bool bIsShutdown )
{
	// Do we even know about this module?
	TSharedRef< FModuleInfo >* ModuleInfoPtr = Modules.Find( InModuleName );
	if( ModuleInfoPtr != NULL )
	{
		TSharedRef< FModuleInfo >& ModuleInfo( *ModuleInfoPtr );

		// Only if already loaded
		if( ModuleInfo->Module.IsValid() )
		{
			// Shutdown the module
			ModuleInfo->Module->ShutdownModule();

			// Verify that we have the only outstanding reference to this module.  No one should still be 
			// referencing a module that is about to be destroyed!
			check( ModuleInfo->Module.IsUnique() );

			// Release reference to module interface.  This will actually destroy the module object.
			ModuleInfo->Module.Reset();

#if !IS_MONOLITHIC
			if( ModuleInfo->Handle != NULL )
			{
				// If we're shutting down then don't bother actually unloading the DLL.  We'll simply abandon it in memory
				// instead.  This makes it much less likely that code will be unloaded that could still be called by
				// another module, such as a destructor or other virtual function.  The module will still be unloaded by
				// the operating system when the process exits.
				if( !bIsShutdown )
				{
					// Unload the DLL
					FPlatformProcess::FreeDllHandle( ModuleInfo->Handle );
				}
				ModuleInfo->Handle = NULL;
			}
#endif

			// If we're shutting down, then we never want this module to be "resurrected" in this session.
			// It's gone for good!  So we'll mark it as such so that we can catch cases where a routine is
			// trying to load a module that we've unloaded/abandoned at shutdown.
			if( bIsShutdown )
			{
				ModuleInfo->bWasUnloadedAtShutdown = true;
			}

			// Don't bother firing off events while we're in the middle of shutting down.  These events
			// are designed for subsystems that respond to plugins dynamically being loaded and unloaded,
			// such as the ModuleUI -- but they shouldn't be doing work to refresh at shutdown.
			else
			{
				// A module was successfully unloaded.  Fire callbacks.
				ModulesChangedEvent.Broadcast( InModuleName, EModuleChangeReason::ModuleUnloaded );
			}

			return true;
		}
	}

	return false;
}


void FModuleManager::AbandonModule( const FName InModuleName )
{
	// Do we even know about this module?
	TSharedRef< FModuleInfo >* ModuleInfoPtr = Modules.Find( InModuleName );
	if( ModuleInfoPtr != NULL )
	{
		TSharedRef< FModuleInfo >& ModuleInfo( *ModuleInfoPtr );

		// Only if already loaded
		if( ModuleInfo->Module.IsValid() )
		{
			// Release reference to module interface.  This will actually destroy the module object.
			// @todo UE4 DLL: Could be dangerous in some cases to reset the module interface while abandoning.  Currently not
			// a problem because script modules don't implement any functionality here.  Possible, we should keep these references
			// alive off to the side somewhere (intentionally leak)
			ModuleInfo->Module.Reset();

			// A module was successfully unloaded.  Fire callbacks.
			ModulesChangedEvent.Broadcast( InModuleName, EModuleChangeReason::ModuleUnloaded );
		}
	}
}


void FModuleManager::UnloadModulesAtShutdown()
{
	struct FModulePair
	{
		FName ModuleName;
		int32 LoadOrder;
		FModulePair(FName InModuleName, int32 InLoadOrder)
			: ModuleName(InModuleName)
			, LoadOrder(InLoadOrder)
		{
			check(LoadOrder > 0); // else was never initialized
		}
		bool operator<(const FModulePair& Other) const
		{
			return LoadOrder > Other.LoadOrder; //intentionally backwards, we want the last loaded module first
		}
	};
	TArray<FModulePair> ModulesToUnload;
	for( FModuleMap::TConstIterator ModuleIt( Modules ); ModuleIt; ++ModuleIt )
	{
		TSharedRef< FModuleInfo > ModuleInfo( ModuleIt.Value() );

		// Only if already loaded
		if( ModuleInfo->Module.IsValid() )
		{
			// Only if the module supports shutting down in this phase
			if( ModuleInfo->Module->SupportsAutomaticShutdown() )
			{
				new (ModulesToUnload) FModulePair(ModuleIt.Key(), ModuleIt.Value()->LoadOrder);
			}
		}
	}

	ModulesToUnload.Sort();

	for (int32 Index = 0; Index < ModulesToUnload.Num(); Index++)
	{
		UE_LOG(LogModuleManager, Log, TEXT( "Shutting down and abandoning module %s (%d)" ), *ModulesToUnload[Index].ModuleName.ToString(), ModulesToUnload[Index].LoadOrder );
		const bool bIsShutdown = true;
		UnloadModule( ModulesToUnload[Index].ModuleName, bIsShutdown );
	}
}


TSharedPtr<IModuleInterface> FModuleManager::GetModule( const FName InModuleName )
{
	// Do we even know about this module?
	const TSharedRef<FModuleInfo>* ModuleInfo = Modules.Find(InModuleName);

	if (ModuleInfo == nullptr)
	{
		return nullptr;
	}

	return (*ModuleInfo)->Module;
}


bool FModuleManager::Exec( UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar )
{
#if !UE_BUILD_SHIPPING
	if ( FParse::Command( &Cmd, TEXT( "Module" ) ) )
	{
		// List
		if( FParse::Command( &Cmd, TEXT( "List" ) ) )
		{
			if( Modules.Num() > 0 )
			{
				Ar.Logf( TEXT( "Listing all %i known modules:\n" ), Modules.Num() );

				TArray< FString > StringsToDisplay;
				for( FModuleMap::TConstIterator ModuleIt( Modules ); ModuleIt; ++ModuleIt )
				{
					StringsToDisplay.Add(
						FString::Printf( TEXT( "    %s [File: %s] [Loaded: %s]" ),
							*ModuleIt.Key().ToString(),
							*ModuleIt.Value()->Filename,
							ModuleIt.Value()->Module.IsValid() != false? TEXT( "Yes" ) : TEXT( "No" ) ) );
				}

				// Sort the strings
				StringsToDisplay.Sort();

				// Display content
				for( TArray< FString >::TConstIterator StringIt( StringsToDisplay ); StringIt; ++StringIt )
				{
					Ar.Log( *StringIt );
				}
			}
			else
			{
				Ar.Logf( TEXT( "No modules are currently known." ), Modules.Num() );
			}

			return true;
		}

#if !IS_MONOLITHIC
		// Load <ModuleName>
		else if( FParse::Command( &Cmd, TEXT( "Load" ) ) )
		{
			const FString ModuleNameStr = FParse::Token( Cmd, 0 );
			if( !ModuleNameStr.IsEmpty() )
			{
				const FName ModuleName( *ModuleNameStr );
				if( !IsModuleLoaded( ModuleName ) )
				{
					Ar.Logf( TEXT( "Loading module" ) );
					LoadModuleWithCallback( ModuleName, Ar );
				}
				else
				{
					Ar.Logf( TEXT( "Module is already loaded." ) );
				}
			}
			else
			{
				Ar.Logf( TEXT( "Please specify a module name to load." ) );
			}

			return true;
		}


		// Unload <ModuleName>
		else if( FParse::Command( &Cmd, TEXT( "Unload" ) ) )
		{
			const FString ModuleNameStr = FParse::Token( Cmd, 0 );
			if( !ModuleNameStr.IsEmpty() )
			{
				const FName ModuleName( *ModuleNameStr );

				if( IsModuleLoaded( ModuleName ) )
				{
					Ar.Logf( TEXT( "Unloading module." ) );
					UnloadOrAbandonModuleWithCallback( ModuleName, Ar );
				}
				else
				{
					Ar.Logf( TEXT( "Module is not currently loaded." ) );
				}
			}
			else
			{
				Ar.Logf( TEXT( "Please specify a module name to unload." ) );
			}

			return true;
		}


		// Reload <ModuleName>
		else if( FParse::Command( &Cmd, TEXT( "Reload" ) ) )
		{
			const FString ModuleNameStr = FParse::Token( Cmd, 0 );
			if( !ModuleNameStr.IsEmpty() )
			{
				const FName ModuleName( *ModuleNameStr );

				if( IsModuleLoaded( ModuleName ) )
				{
					Ar.Logf( TEXT( "Reloading module.  (Module is currently loaded.)" ) );
					UnloadOrAbandonModuleWithCallback( ModuleName, Ar );
				}
				else
				{
					Ar.Logf( TEXT( "Reloading module.  (Module was not loaded.)" ) );
				}

				if( !IsModuleLoaded( ModuleName ) )
				{
					Ar.Logf( TEXT( "Reloading module" ) );
					LoadModuleWithCallback( ModuleName, Ar );
				}
			}

			return true;
		}
#endif // !IS_MONOLITHIC
	}
#endif // !UE_BUILD_SHIPPING

	return false;
}


bool FModuleManager::QueryModule( const FName InModuleName, FModuleStatus& OutModuleStatus )
{
	// Do we even know about this module?
	const TSharedRef< FModuleInfo >* ModuleInfoPtr = Modules.Find( InModuleName );
	if( ModuleInfoPtr != NULL )
	{
		const TSharedRef< FModuleInfo >& ModuleInfo( *ModuleInfoPtr );

		OutModuleStatus.Name = InModuleName.ToString();
		OutModuleStatus.FilePath = FPaths::ConvertRelativePathToFull(ModuleInfo->Filename);
		OutModuleStatus.bIsLoaded = ( ModuleInfo->Module.IsValid() != false );

		if( OutModuleStatus.bIsLoaded )
		{
			OutModuleStatus.bIsGameModule = ModuleInfo->Module->IsGameModule();
		}

		return true;
	}

	// Not known to us
	return false;
}


void FModuleManager::QueryModules( TArray< FModuleStatus >& OutModuleStatuses )
{
	OutModuleStatuses.Reset();

	for( FModuleMap::TConstIterator ModuleIt( Modules ); ModuleIt; ++ModuleIt )
	{
		const FName CurModuleFName = ModuleIt.Key();
		const TSharedRef< FModuleInfo > CurModule = ModuleIt.Value();

		FModuleStatus ModuleStatus;
		{
			ModuleStatus.Name = CurModuleFName.ToString();
			ModuleStatus.FilePath = FPaths::ConvertRelativePathToFull(CurModule->Filename);
			ModuleStatus.bIsLoaded = CurModule->Module.IsValid();

			if( ModuleStatus.bIsLoaded  )
			{
				ModuleStatus.bIsGameModule = CurModule->Module->IsGameModule();
			}
		}
		OutModuleStatuses.Add( ModuleStatus );
	}
}


FString FModuleManager::GetModuleFilename(FName ModuleName) const
{
	return Modules.FindChecked(ModuleName)->Filename;
}


void FModuleManager::SetModuleFilename(FName ModuleName, const FString& Filename)
{
	auto& Module = Modules.FindChecked(ModuleName);
	Module->Filename = Filename;
	// If it's a new module then also update its OriginalFilename
	if (Module->OriginalFilename.IsEmpty())
	{
		Module->OriginalFilename = Filename;
	}
}


FString FModuleManager::GetCleanModuleFilename(FName ModuleName, bool bGameModule)
{
	FString Prefix, Suffix;
	GetModuleFilenameFormat(bGameModule, Prefix, Suffix);
	return Prefix + ModuleName.ToString() + Suffix;
}


void FModuleManager::GetModuleFilenameFormat(bool bGameModule, FString& OutPrefix, FString& OutSuffix)
{
	// Get the module configuration for this directory type
	const TCHAR* ConfigSuffix = NULL;
	switch(FApp::GetBuildConfiguration())
	{
	case EBuildConfigurations::Debug:
		ConfigSuffix = TEXT("-Debug");
		break;
	case EBuildConfigurations::DebugGame:
		ConfigSuffix = bGameModule? TEXT("-DebugGame") : NULL;
		break;
	case EBuildConfigurations::Development:
		ConfigSuffix = NULL;
		break;
	case EBuildConfigurations::Test:
		ConfigSuffix = TEXT("-Test");
		break;
	case EBuildConfigurations::Shipping:
		ConfigSuffix = TEXT("-Shipping");
		break;
	default:
		check(false);
		break;
	}

	// Get the base name for modules of this application
	OutPrefix = FPlatformProcess::GetModulePrefix() + FPaths::GetBaseFilename(FPlatformProcess::ExecutableName());
	if (OutPrefix.Contains(TEXT("-")))
	{
		OutPrefix = OutPrefix.Left(OutPrefix.Find(TEXT("-")) + 1);
	}
	else
	{
		OutPrefix += TEXT("-");
	}

	// Get the suffix for each module
	OutSuffix.Empty();
	if (ConfigSuffix != NULL)
	{
		OutSuffix += TEXT("-");
		OutSuffix += FPlatformProcess::GetBinariesSubdirectory();
		OutSuffix += ConfigSuffix;
	}
	OutSuffix += TEXT(".");
	OutSuffix += FPlatformProcess::GetModuleExtension();
}


void FModuleManager::FindModulePaths(const TCHAR* NamePattern, TMap<FName, FString> &OutModulePaths) const
{
	// Search through the engine directory
	FindModulePathsInDirectory(FPlatformProcess::GetModulesDirectory(), false, NamePattern, OutModulePaths);

	// Search any engine directories
	for (int Idx = 0; Idx < EngineBinariesDirectories.Num(); Idx++)
	{
		FindModulePathsInDirectory(EngineBinariesDirectories[Idx], false, NamePattern, OutModulePaths);
	}

	// Search any game directories
	for (int Idx = 0; Idx < GameBinariesDirectories.Num(); Idx++)
	{
		FindModulePathsInDirectory(GameBinariesDirectories[Idx], true, NamePattern, OutModulePaths);
	}
}


void FModuleManager::FindModulePathsInDirectory(const FString& InDirectoryName, bool bIsGameDirectory, const TCHAR* NamePattern, TMap<FName, FString> &OutModulePaths) const
{
	// Get the prefix and suffix for module filenames
	FString ModulePrefix, ModuleSuffix;
	GetModuleFilenameFormat(bIsGameDirectory, ModulePrefix, ModuleSuffix);

	// Find all the files
	TArray<FString> FullFileNames;
	IFileManager::Get().FindFilesRecursive(FullFileNames, *InDirectoryName, *(ModulePrefix + NamePattern + ModuleSuffix), true, false);

	// Parse all the matching module names
	for (int32 Idx = 0; Idx < FullFileNames.Num(); Idx++)
	{
		const FString &FullFileName = FullFileNames[Idx];

		FString FileName = FPaths::GetCleanFilename(FullFileName);
		if (FileName.StartsWith(ModulePrefix) && FileName.EndsWith(ModuleSuffix))
		{
			FString ModuleName = FileName.Mid(ModulePrefix.Len(), FileName.Len() - ModulePrefix.Len() - ModuleSuffix.Len());
			if (!ModuleName.EndsWith("-Debug") && !ModuleName.EndsWith("-Shipping") && !ModuleName.EndsWith("-Test") && !ModuleName.EndsWith("-DebugGame"))
			{
				OutModulePaths.Add(FName(*ModuleName), FullFileName);
			}
		}
	}
}


void FModuleManager::UnloadOrAbandonModuleWithCallback( const FName InModuleName, FOutputDevice &Ar )
{
	TSharedRef< FModuleInfo > Module = Modules.FindChecked( InModuleName );
	
	Module->Module->PreUnloadCallback();

	const bool bIsHotReloadable = DoesLoadedModuleHaveUObjects( InModuleName );
	if( !bIsHotReloadable && Module->Module->SupportsDynamicReloading() )
	{
		if( !UnloadModule( InModuleName ))
		{
			Ar.Logf(TEXT("Module couldn't be unloaded, and so can't be recompiled while the engine is running."));
		}
	}
	else
	{
		Ar.Logf( TEXT( "Module being reloaded does not support dynamic unloading -- abandoning existing loaded module so that we can load the recompiled version!" ) );
		AbandonModule( InModuleName );
	}
}


bool FModuleManager::LoadModuleWithCallback( const FName InModuleName, FOutputDevice &Ar )
{
	TSharedPtr<IModuleInterface> LoadedModule = LoadModule( InModuleName, true );
	bool bWasSuccessful = IsModuleLoaded( InModuleName );

	if (bWasSuccessful && LoadedModule.IsValid())
	{
		LoadedModule->PostLoadCallback();
	}
	else
	{
		Ar.Logf(TEXT("Module couldn't be loaded."));
	}

	return bWasSuccessful;
}


void FModuleManager::MakeUniqueModuleFilename( const FName InModuleName, FString& UniqueSuffix, FString& UniqueModuleFileName )
{
	TSharedRef<FModuleInfo> Module = Modules.FindChecked(InModuleName);

	do
	{
		// Use a random number as the unique file suffix, but mod it to keep it of reasonable length
		UniqueSuffix = FString::FromInt( FMath::Rand() % 10000 );

		const FString ModuleName = *InModuleName.ToString();
		const int32 MatchPos = Module->OriginalFilename.Find(ModuleName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

		if (ensure(MatchPos != INDEX_NONE))
		{
			const int32 SuffixPos = MatchPos + ModuleName.Len();
			UniqueModuleFileName = FString::Printf( TEXT( "%s-%s%s" ),
				*Module->OriginalFilename.Left( SuffixPos ),
				*UniqueSuffix,
				*Module->OriginalFilename.Right( Module->OriginalFilename.Len() - SuffixPos ) );
		}
	}
	while (IFileManager::Get().GetFileAgeSeconds(*UniqueModuleFileName) != -1.0);
}


const TCHAR *FModuleManager::GetUBTConfiguration()
{
	return EBuildConfigurations::ToString(FApp::GetBuildConfiguration());
}


bool FModuleManager::CheckModuleCompatibility(const TCHAR* Filename)
{
	int32 ModuleApiVersion = FPlatformProcess::GetDllApiVersion(Filename);

	if (ModuleApiVersion != MODULE_API_VERSION)
	{
		UE_LOG(LogModuleManager, Warning, TEXT("Found module file %s (API version %d), but it was incompatible with the current engine API version (%d). This is likely a stale module that must be recompiled."), Filename, ModuleApiVersion, MODULE_API_VERSION);
		return false;
	}

	return true;
}


void FModuleManager::StartProcessingNewlyLoadedObjects()
{
	// Only supposed to be called once
	ensure( bCanProcessNewlyLoadedObjects == false );	
	bCanProcessNewlyLoadedObjects = true;
}


void FModuleManager::AddBinariesDirectory(const TCHAR *InDirectory, bool bIsGameDirectory)
{
	if (bIsGameDirectory)
	{
		GameBinariesDirectories.Add(InDirectory);
	}
	else
	{
		EngineBinariesDirectories.Add(InDirectory);
	}

	FPlatformProcess::AddDllDirectory(InDirectory);
}


void FModuleManager::SetGameBinariesDirectory(const TCHAR* InDirectory)
{
#if !IS_MONOLITHIC
	// Before loading game DLLs, make sure that the DLL files can be located by the OS by adding the
	// game binaries directory to the OS DLL search path.  This is so that game module DLLs which are
	// statically loaded as dependencies of other game modules can be located by the OS.
	FPlatformProcess::PushDllDirectory(InDirectory);

	// Add it to the list of game directories to search
	GameBinariesDirectories.Add(InDirectory);
#endif
}

FString FModuleManager::GetGameBinariesDirectory() const
{
	if (GameBinariesDirectories.Num())
	{
		return GameBinariesDirectories[0];
	}
	return FString();
}

bool FModuleManager::DoesLoadedModuleHaveUObjects( const FName ModuleName )
{
	if (IsModuleLoaded(ModuleName) && IsPackageLoaded.IsBound())
	{
		return IsPackageLoaded.Execute(*FString(FString(TEXT("/Script/")) + ModuleName.ToString()));
	}

	return false;
}
