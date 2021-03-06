// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class SBspPalette : public SCompoundWidget, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS( SBspPalette ){}
	SLATE_END_ARGS();

	void Construct( const FArguments& InArgs );

private:

	/** Make a widget for the list view display */
	TSharedRef<ITableRow> MakeListViewWidget(TSharedPtr<struct FBspBuilderType> BspBuilder, const TSharedRef<STableViewBase>& OwnerTable);

	/** Delegate for when the list view selection changes */
	void OnSelectionChanged(TSharedPtr<FBspBuilderType> BspBuilder, ESelectInfo::Type SelectionType);

	/** Begin dragging a list widget */
	FReply OnDraggingListViewWidget(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Select additive mode */
	void OnAdditiveModeButtonClicked(ESlateCheckBoxState::Type CheckType);

	/** Select subtractive mode */
	void OnSubtractiveModeButtonClicked(ESlateCheckBoxState::Type CheckType);

	/** @return the check state of the additive mode checkbox */
	ESlateCheckBoxState::Type IsAdditiveModeChecked() const;

	/** @return the check state of the subtractive mode checkbox */
	ESlateCheckBoxState::Type IsSubtractiveModeChecked() const;

	/** Get the image for additive mode */
	const FSlateBrush* GetAdditiveModeImage() const;

	/** Get the image for subtractive mode */
	const FSlateBrush* GetSubtractiveModeImage() const;

private:
	/** Property view for brush options */
	TSharedPtr<class IDetailsView> BrushOptionView;

	/** Brush builder currently active */
	TWeakObjectPtr<UBrushBuilder> ActiveBrushBuilder;

	/** Additive or subtractive mode */
	bool bIsAdditive;
};