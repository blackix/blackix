// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	HTML5Misc.h: HTML5 platform misc functions
==============================================================================================*/

#pragma once

#include "HTML5DebugLogging.h"

/**
 * HTML5 implementation of the misc OS functions
 */
struct CORE_API FHTML5Misc : public FGenericPlatformMisc
{
	static void PlatformInit();
	static class GenericApplication* CreateApplication();
	static uint32 GetKeyMap(uint16* KeyCodes, FString* KeyNames, uint32 MaxMappings);
	static uint32 GetCharKeyMap(uint16* KeyCodes, FString* KeyNames, uint32 MaxMappings);

	FORCEINLINE static int32 NumberOfCores()
	{
		return 1;
	}

	FORCEINLINE static void MemoryBarrier()
	{
		// Do nothing on x86; the spec requires load/store ordering even in the absence of a memory barrier.
		
		// @todo HTML5: Will this be applicable for final?
	}
	/** Return true if a debugger is present */
	FORCEINLINE static bool IsDebuggerPresent()
	{
		return true; 
	}

	/** Break into the debugger, if IsDebuggerPresent returns true, otherwise do nothing  */
	FORCEINLINE static void DebugBreak()
	{
		if (IsDebuggerPresent())
		{
#if PLATFORM_HTML5_WIN32
			exit(-1);
#else
			emscripten_log(255, "DebugBreak() called!");
#endif
		}
	}

	FORCEINLINE static void LocalPrint( const TCHAR* Str )
	{
		wprintf(TEXT("%ls"), Str);
	}

};

typedef FHTML5Misc FPlatformMisc;
