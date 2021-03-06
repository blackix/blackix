// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once

class SGraphPinColor : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinColor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

	/** Return the current color value stored in the pin */
	FLinearColor GetColor () const;

protected:
	// Begin SGraphPin interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() OVERRIDE;
	// End SGraphPin interface

	/** Called when clicking on the color button */
	FReply OnColorBoxClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Called when the color picker commits a color value */
	void OnColorCommitted(FLinearColor color);


private:
	/** Current selected color */
	FLinearColor SelectedColor;
};
