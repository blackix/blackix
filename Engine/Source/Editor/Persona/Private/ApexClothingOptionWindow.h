// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate.h"

class SApexClothingOptionWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SApexClothingOptionWindow )
		: _WidgetWindow()
		, _NumLODs()
		, _ApexDetails()
	{}

		SLATE_ARGUMENT( TSharedPtr<SWindow>, WidgetWindow )
		SLATE_ARGUMENT( int32, NumLODs )
		SLATE_ARGUMENT(TSharedPtr<SUniformGridPanel>, ApexDetails )
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);

	FReply OnImport()
	{
		bCanImport = true;
		if ( WidgetWindow.IsValid() )
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	FReply OnCancel()
	{
		bCanImport = false;
		if ( WidgetWindow.IsValid() )
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	ESlateCheckBoxState::Type IsCheckedLOD() const
	{
		return bUseLOD ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
	}

	void OnUseLOD(ESlateCheckBoxState::Type CheckState)
	{
		bUseLOD = (CheckState == ESlateCheckBoxState::Checked);
	}

	bool CanImport() const
	{
		return bCanImport;
	}

	bool IsUsingLOD()
	{
		return bUseLOD;
	}

	SApexClothingOptionWindow() 
		: bCanImport(false)
		, bUseLOD(true)
	{}

private:
	bool			bCanImport;
	bool			bReimport;
	bool			bUseLOD;

	TWeakPtr< SWindow > WidgetWindow;
};
