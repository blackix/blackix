// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"
#include "ModuleManager.h"		// For inline LoadModuleChecked()

/**
 * The public interface of the MovieSceneCoreTypes module
 */
class IMovieSceneCoreTypes : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to IMovieSceneCoreTypes
	 *
	 * @return Returns MovieSceneCoreTypes singleton instance, loading the module on demand if needed
	 */
	static inline IMovieSceneCoreTypes& Get()
	{
		return FModuleManager::LoadModuleChecked< IMovieSceneCoreTypes >( "MovieSceneCoreTypes" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "MovieSceneCoreTypes" );
	}


};

