// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OculusMRPrivate.h"
#include "IOculusMRModule.h"

#define LOCTEXT_NAMESPACE "OculusMR"


//-------------------------------------------------------------------------------------------------
// FOculusInputModule
//-------------------------------------------------------------------------------------------------

class FOculusMRModule : public IOculusMRModule
{
public:
	FOculusMRModule();

	static inline FOculusMRModule& Get()
	{
		return FModuleManager::LoadModuleChecked< FOculusMRModule >("OculusMR");
	}

	// IOculusMRModule
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	bool IsInitialized() { return bInitialized; }

private:
	bool bInitialized;
};


#undef LOCTEXT_NAMESPACE
