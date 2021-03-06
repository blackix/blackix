// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "BlankModulePrivatePCH.h"


class FBlankModule : public IBlankModule
{
	/** IModuleInterface implementation */
	virtual void StartupModule() OVERRIDE;
	virtual void ShutdownModule() OVERRIDE;
};

IMPLEMENT_MODULE( FBlankModule, BlankModule )



void FBlankModule::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
}


void FBlankModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}



