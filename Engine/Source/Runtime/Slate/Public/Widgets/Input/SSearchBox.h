// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#define LOCTEXT_NAMESPACE "Slate"


/** A text box that is used for searching. Meant to be as easy to use as possible with as few options as possible. */
class SLATE_API SSearchBox
	: public SEditableTextBox
{

public:
	/** Which direction to go when searching */
	enum SearchDirection
	{
		Previous,
		Next,
	};

	DECLARE_DELEGATE_OneParam(FOnSearch, SSearchBox::SearchDirection);

	SLATE_BEGIN_ARGS(SSearchBox)
		: _Style()
		, _HintText( LOCTEXT("SearchHint", "Search") )
		, _InitialText()
		, _OnTextChanged()
		, _OnTextCommitted()
		, _OnSearch()
		, _SelectAllTextWhenFocused( true )
		, _DelayChangeNotificationsWhileTyping( false )
	{ }

		/** Style used to draw this search box */
		SLATE_ARGUMENT( TOptional<const FSearchBoxStyle*>, Style )

		/** The text displayed in the SearchBox when no text has been entered */
		SLATE_ATTRIBUTE( FText, HintText )

		/** The text displayed in the SearchBox when it's created */
		SLATE_ATTRIBUTE( FText, InitialText )

		/** Invoked whenever the text changes */
		SLATE_EVENT( FOnTextChanged, OnTextChanged )

		/** Invoked whenever the text is committed (e.g. user presses enter) */
		SLATE_EVENT( FOnTextCommitted, OnTextCommitted )

		/** This will add a next and previous button to your search box */
		SLATE_EVENT( FOnSearch, OnSearch )

		/** Whether to select all text when the user clicks to give focus on the widget */
		SLATE_ATTRIBUTE( bool, SelectAllTextWhenFocused )
		
		/** Minimum width that a text block should be */
		SLATE_ATTRIBUTE( float, MinDesiredWidth )

		/** Whether the SearchBox should delay notifying listeners of text changed events until the user is done typing */
		SLATE_ATTRIBUTE( bool, DelayChangeNotificationsWhileTyping )

	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

public:

	// SWidget overrides

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

private:

	/** Callback for changes in the editable text box. */
	void HandleTextChanged(const FText& NewText);

	/** Callback for committing changes in the editable text box. */
	void HandleTextCommitted(const FText& NewText, ETextCommit::Type CommitType);

private:

	/** @return should we show the X to clear search? */
	EVisibility GetXVisibility() const;

	/** @return should we show the search glass icon? */
	EVisibility GetSearchGlassVisibility() const;

	FReply OnClickedSearch(SSearchBox::SearchDirection Direction);

	/** Invoked when user clicks the X*/
	FReply OnClearSearch();

	/** Invoked to get the font to use for the editable text box */
	FSlateFontInfo GetWidgetFont() const;

	/** Delegate that is invoked when the user does next or previous */
	FOnSearch OnSearchDelegate;

	/** Delegate that is invoked when the text changes */
	FOnTextChanged OnTextChangedDelegate;

	/** Delegate that is invoked when the text is committed */
	FOnTextCommitted OnTextCommittedDelegate;

	/** Whether the SearchBox should delay notifying listeners of text changed events until the user is done typing */
	TAttribute< bool > DelayChangeNotificationsWhileTyping;

	/** Fonts that specify how to render search text when inactive, and active */
	FSlateFontInfo ActiveFont, InactiveFont;

	/** When true, the user is typing in the search box. This is used to delay the actual filter until the user is done typing. */
	double CurrentTime;
	double LastTypeTime;
	double FilterDelayAfterTyping;
	bool bTypingFilterText;
	FText LastPendingTextChangedValue;
};


#undef LOCTEXT_NAMESPACE
