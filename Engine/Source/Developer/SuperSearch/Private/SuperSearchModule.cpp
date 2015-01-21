// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SuperSearchPrivatePCH.h"
#include "SSuperSearch.h"

IMPLEMENT_MODULE( FSuperSearchModule,SuperSearch );

namespace SuperSearchModule
{
	static const FName FNameSuperSearchApp = FName(TEXT("SuperSearchApp"));
}

void FSuperSearchModule::StartupModule()
{
}

void FSuperSearchModule::ShutdownModule()
{
}

TSharedRef< SWidget > FSuperSearchModule::MakeSearchBox(TSharedPtr< SEditableTextBox >& OutExposedEditableTextBox, const TOptional<const FSearchBoxStyle*> InStyle) const
{
	TSharedRef< SSuperSearchBox > NewSearchBox = SNew(SSuperSearchBox).Style(InStyle);
	OutExposedEditableTextBox = NewSearchBox->GetEditableTextBox();
	return NewSearchBox;
}
