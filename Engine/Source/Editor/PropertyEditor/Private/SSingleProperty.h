// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

class SSingleProperty : public ISinglePropertyView
{
public:
	SLATE_BEGIN_ARGS( SSingleProperty )
		: _Object(NULL)
		, _NotifyHook( NULL )
		, _PropertyFont( FEditorStyle::GetFontStyle( PropertyEditorConstants::PropertyFontStyle ) ) 
		, _NamePlacement( EPropertyNamePlacement::Left )
		, _NameOverride()
	{}

		SLATE_ARGUMENT( UObject*, Object )
		SLATE_ARGUMENT( FName, PropertyName )
		SLATE_ARGUMENT( FNotifyHook*, NotifyHook )
		SLATE_ARGUMENT( FSlateFontInfo, PropertyFont )
		SLATE_ARGUMENT( EPropertyNamePlacement::Type, NamePlacement )
		SLATE_ARGUMENT( FString, NameOverride )
	SLATE_END_ARGS()	

	void Construct( const FArguments& InArgs );

	/** ISinglePropertyView interface */
	virtual bool HasValidProperty() const OVERRIDE { return RootPropertyNode.IsValid() && ValueNode.IsValid(); }
	virtual void SetObject( UObject* InObject ) OVERRIDE;
	virtual void SetOnPropertyValueChanged( FSimpleDelegate& InOnPropertyValueChanged ) OVERRIDE;

	/**
	 * Replaces objects being observed by the view with new objects
	 *
	 * @param OldToNewObjectMap	Mapping from objects to replace to their replacement
	 */
	void ReplaceObjects( const TMap<UObject*, UObject*>& OldToNewObjectMap );

	/**
	 * Removes objects from the view because they are about to be deleted
	 *
	 * @param DeletedObjects	The objects to delete
	 */
	void RemoveDeletedObjects( const TArray<UObject*>& DeletedObjects );

	/** Creates a color picker window for a property node */
	void CreateColorPickerWindow( const TSharedRef< class FPropertyEditor >& PropertyEditor, bool bUseAlpha );

	/** @return The notify hook used by the property */
	FNotifyHook* GetNotifyHook() const { return NotifyHook; }


private:
	/**
	 * Sets the color if this is a color property
	 *
	 * @param NewColor The color to set
	 */
	void SetColorPropertyFromColorPicker(FLinearColor NewColor);
private:
	/** The root property node for the value node (contains the root object */
	TSharedPtr<FObjectPropertyNode> RootPropertyNode;
	/** The node for the property being edited */
	TSharedPtr<FPropertyNode> ValueNode;
	/** Property utilities for handling common functionality of property editors */
	TSharedPtr<class FSinglePropertyUtilities> PropertyUtilities;
	/** Name override to display instead of the property name */
	FString NameOverride;
	/** Font to use */
	FSlateFontInfo PropertyFont;
	/** Notify hook to use when editing values */
	FNotifyHook* NotifyHook;
	/** Name of the property */
	FName PropertyName;
	/** Location of the name in the view */
	EPropertyNamePlacement::Type NamePlacement;
};