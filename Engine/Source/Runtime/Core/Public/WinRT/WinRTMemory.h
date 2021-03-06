// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	WinRTPlatformMemory.h: WinRT platform memory functions
==============================================================================================*/

#pragma once

/**
 *	WinRT implementation of the FGenericPlatformMemoryStats.
 */
struct FPlatformMemoryStats : public FGenericPlatformMemoryStats
{};

/**
* WinRT implementation of the memory OS functions
**/
struct CORE_API FWinRTPlatformMemory : public FGenericPlatformMemory
{
	// Begin FGenericPlatformMemory interface
	static void Init();
	static const FPlatformMemoryConstants& GetConstants();
	// End FGenericPlatformMemory interface
};

typedef FWinRTPlatformMemory FPlatformMemory;



