// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class FFilePathStructCustomization : public IStructCustomization
{
public:
	static TSharedRef<IStructCustomization> MakeInstance();

	/** IStructCustomization interface */
	virtual void CustomizeStructHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IStructCustomizationUtils& StructCustomizationUtils ) OVERRIDE;

	virtual void CustomizeStructChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IStructCustomizationUtils& StructCustomizationUtils ) OVERRIDE;              

private:

	/** Delegate for displaying text value of file */
	FText GetDisplayedText(TSharedRef<IPropertyHandle> PropertyHandle) const;

	/** Delegate used to display a file picker */
	FReply OnPickFile(TSharedRef<IPropertyHandle> PropertyHandle) const;

	FString FileFilterExtension;
};