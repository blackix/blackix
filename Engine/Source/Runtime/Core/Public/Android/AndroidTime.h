// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	AndroidTime.h: Android platform Time functions
==============================================================================================*/

#pragma once

//@todo android: this entire file

/**
 * Android implementation of the Time OS functions
 */
struct CORE_API FAndroidTime : public FGenericPlatformTime
{
	// android uses BSD time code from GenericPlatformTime
};

typedef FAndroidTime FPlatformTime;
