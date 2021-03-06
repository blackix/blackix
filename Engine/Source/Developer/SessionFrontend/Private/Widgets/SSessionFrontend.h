// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SSessionFrontend.h: Declares the SSessionFrontend class.
=============================================================================*/

#pragma once

/**
 * Implements the launcher application
 */
class SSessionFrontend
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSessionFrontend) { }
	SLATE_END_ARGS()


public:

	/**
	 * Constructs the application.
	 *
	 * @param InArgs - The Slate argument list.
	 * @param ConstructUnderMajorTab - The major tab which will contain the session front-end.
	 * @param ConstructUnderWindow - The window in which this widget is being constructed.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow );


protected:

	/**
	 * Fills the Window menu with menu items.
	 *
	 * @param MenuBuilder - The multi-box builder that should be filled with content for this pull-down menu.
	 * @param RootMenuGroup - The root menu group.
	 * @param AppMenuGroup - The application menu group.
	 * @param TabManager - A Tab Manager from which to populate tab spawner menu items.
	 */
	static void FillWindowMenu( FMenuBuilder& MenuBuilder, TSharedRef<FWorkspaceItem> RootMenuGroup, TSharedRef<FWorkspaceItem> AppMenuGroup, const TSharedPtr<FTabManager> TabManager );

	/**
	 * Creates and initializes the controller classes.
	 */
	void InitializeControllers( );


private:

	// Callback for handling automation module shutdowns.
	void HandleAutomationModuleShutdown();

	// Callback for spawning tabs.
	TSharedRef<SDockTab> HandleTabManagerSpawnTab( const FSpawnTabArgs& Args, FName TabIdentifier ) const;


private:

	// Holds the target device proxy manager.
	ITargetDeviceProxyManagerPtr DeviceProxyManager;
	
	// Holds a flag indicating whether the launcher overlay is visible.
	bool LauncherOverlayVisible;

	// Holds the 'new session' button.
	TSharedPtr<SButton> NewSessionButton;

	// Holds a pointer to the session manager.
	ISessionManagerPtr SessionManager;

	// Holds a pointer to the session manager.
	IScreenShotManagerPtr ScreenShotManager;

	// Holds the tab manager that manages the front-end's tabs.
	TSharedPtr<FTabManager> TabManager;
};
