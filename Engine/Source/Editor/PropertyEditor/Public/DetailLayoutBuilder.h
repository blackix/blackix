// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class IDetailCategoryBuilder;
class IPropertyHandle;
class IArrayPropertyHelper;

namespace ECategoryPriority
{
	enum Type
	{

		// Highest sort priority
		Variable = 0,
		Transform,
		Important,
		TypeSpecific,
		Default,
		// Lowest sort priority
		Uncommon,
	};
}

/**
 * The builder for laying custom details
 */
class IDetailLayoutBuilder
{
	
public:
	virtual ~IDetailLayoutBuilder(){}

	/**
	 * @return the font used for properties and details
	 */ 
	static FSlateFontInfo GetDetailFont() { return FEditorStyle::GetFontStyle( TEXT("PropertyWindow.NormalFont") ); }

	/**
	 * @return the bold font used for properties and details
	 */ 
	static FSlateFontInfo GetDetailFontBold() { return FEditorStyle::GetFontStyle( TEXT("PropertyWindow.BoldFont") ); }
	
	/**
	 * @return the italic font used for properties and details
	 */ 
	static FSlateFontInfo GetDetailFontItalic() { return FEditorStyle::GetFontStyle( TEXT("PropertyWindow.ItalicFont") ); }
	
	/**
	 * @return the parent detail view for this layout builder
	 */
	virtual const class IDetailsView& GetDetailsView() const = 0;

	/**
	 * Gets the current object(s) being customized by this builder
	 *
	 * If this is a sub-object customization it will return those sub objects.  Otherwise the root objects will be returned.
	 */
	virtual void GetObjectsBeingCustomized( TArray< TWeakObjectPtr<UObject> >& OutObjects ) const = 0;

	/**
	 *	@return the utilities various widgets need access to certain features of PropertyDetails
	 */
	virtual const TSharedRef< class IPropertyUtilities >& GetPropertyUtilities() const = 0; 


	/**
	 * Edits an existing category or creates a new one
	 * 
	 * @param CategoryName				The name of the category
	 * @param NewLocalizedDisplayName	The new display name of the category (optional)
	 * @param CategoryType				Category type to define sort order.  Category display order is sorted by this type (optional)
	 */
	virtual IDetailCategoryBuilder& EditCategory( FName CategoryName, const FString& NewLocalizedDisplayName = TEXT(""), ECategoryPriority::Type CategoryType = ECategoryPriority::Default ) = 0;

	
	/**
	 * Gets a handle to a property which can be used to read and write the property value and identify the property in other detail customization interfaces.
	 * Instructions
	 *
	 * @param Path	The path to the property.  Can be just a name of the property or a path in the format outer.outer.value[optional_index_for_static_arrays]
	 * @param ClassOutermost	Optional outer class if accessing a property outside of the current class being customized
	 * @param InstanceName		Optional instance name if multiple UProperty's of the same type exist. such as two identical structs, the instance name is one of the struct variable names)
	    Examples:

		struct MyStruct
		{ 
			int32 StaticArray[3];
			float FloatVar;
		}

		class MyActor
		{ 
			MyStruct Struct1;
			MyStruct Struct2;
			float MyFloat
		}
		
		To access StaticArray at index 2 from Struct2 in MyActor, your path would be MyStruct.StaticArray[2]" and your instance name is "Struct2"
		To access MyFloat in MyActor you can just pass in "MyFloat" because the name of the property is unambiguous
	 */
	virtual TSharedRef<IPropertyHandle> GetProperty( const FName PropertyPath, const UClass* ClassOutermost = NULL, FName InstanceName = NAME_None )  = 0;

	/**
	 * Hides a property from view 
	 *
	 * @param PropertyHandle	The handle of the property to hide from view
	 */
	virtual void HideProperty( const TSharedPtr<IPropertyHandle> PropertyHandle ) = 0;

	/**
	 * Hides a property from view
	 *
	 * @param Path						The path to the property.  Can be just a name of the property or a path in the format outer.outer.value[optional_index_for_static_arrays]
	 * @param NewLocalizedDisplayName	Optional display name to show instead of the default name
	 * @param ClassOutermost			Optional outer class if accessing a property outside of the current class being customized
	 * @param InstanceName				Optional instance name if multiple UProperty's of the same type exist. such as two identical structs, the instance name is one of the struct variable names)
	 * See IDetailCategoryBuilder::GetProperty for clarification of parameters
	 */
	virtual void HideProperty( FName PropertyPath, const UClass* ClassOutermost = NULL, FName InstanceName = NAME_None ) = 0;

	/**
	 * Refreshes the details view and regenerates all the customized layouts
	 * Use only when you need to remove or add complicated dynamic items
	 */
	virtual void ForceRefreshDetails() = 0;

	/**
	 * Gets the thumbnail pool that should be used for rendering thumbnails in the details view                   
	 */
	virtual TSharedPtr<class FAssetThumbnailPool> GetThumbnailPool() const = 0;
};
