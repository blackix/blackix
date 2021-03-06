// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SAutomationWindowCommandBar.h: Declares the SAutomationWindowCommandBar class.
=============================================================================*/

#pragma once


/**
 * Implements the automation console command bar widget.
 */
class SAutomationWindowCommandBar
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SAutomationWindowCommandBar) { }

 		/**
 		 * Called when the copy log button is clicked.
 		 */
 		SLATE_EVENT(FOnClicked, OnCopyLogClicked)

	SLATE_END_ARGS()

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs - The declaration data for this widget.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef< SNotificationList >& InNotificationList );

	/**
	 * Sets the number of messages selected in the log window.
	 *
	 * @param Count - Number of selected messages.
	 */
	void SetNumLogMessages( int Count );

private:

	// Handles clicking the copy log button.
	FReply HandleCopyButtonClicked( );

private:

 	// Holds the copy log button.
 	TSharedPtr<SButton> CopyButton;

private:

 	// Holds a delegate that is executed when the copy log button is clicked.
 	FOnClicked OnCopyLogClicked;
};
