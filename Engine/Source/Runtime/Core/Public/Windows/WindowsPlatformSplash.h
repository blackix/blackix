// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
/*=============================================================================================
	WindowsPlatformSplash.h: Windows platform splash screen...
==============================================================================================*/

#pragma once

/**
* Windows splash implementation
**/
struct CORE_API FWindowsPlatformSplash : public FGenericPlatformSplash
{
	/**
	* Show the splash screen
	*/
	static void Show();
	/**
	* Hide the splash screen
	*/
	static void Hide();

	/**
	 * Sets the text displayed on the splash screen (for startup/loading progress)
	 *
	 * @param	InType		Type of text to change
	 * @param	InText		Text to display
	 */
	static void SetSplashText( const SplashTextType::Type InType, const TCHAR* InText );
};

typedef FWindowsPlatformSplash FPlatformSplash;
