// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyEditing.h"
#include "PropertyCustomizationHelpers.h"

class FEnvTraceDataCustomization : public IPropertyTypeCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	// Begin IPropertyTypeCustomization interface
	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	// End IPropertyTypeCustomization interface

protected:

	void CacheTraceModes(TSharedRef<class IPropertyHandle> StructPropertyHandle);

	/** cached names of trace modes */
	struct FTextIntPair
	{
		FText Text;
		int32 Int;

		FTextIntPair() {}
		FTextIntPair(FText InText, int32 InInt) : Text(InText), Int(InInt) {}
	};
	TArray<FTextIntPair> TraceModes;
	bool bCanShowProjection;
	uint8 ActiveMode;

	TSharedPtr<IPropertyHandle> PropTraceMode;
	TSharedPtr<IPropertyHandle> PropTraceShape;

	void OnTraceModeChanged(int32 Index);
	TSharedRef<SWidget> OnGetTraceModeContent();
	FText GetCurrentTraceModeDesc() const;
	FText GetShortDescription() const;

	EVisibility GetGeometryVisibility() const;
	EVisibility GetNavigationVisibility() const;
	EVisibility GetProjectionVisibility() const;
	EVisibility GetExtentX() const;
	EVisibility GetExtentY() const;
	EVisibility GetExtentZ() const;
};
