// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"

#include "Factories/SlateWidgetStyleAssetFactory.h"

#include "ClassViewerModule.h"
#include "SClassPickerDialog.h"
#include "ClassViewerFilter.h"

class FClassFilter : public IClassViewerFilter
{
public:
	/** All children of these classes will be included unless filtered out by another setting. */
	TSet< const UClass* > AllowedChildrenOfClasses;

	TSet< const UClass* > DisallowedClasses;

	/** Disallowed class flags. */
	uint32 DisallowedClassFlags;

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs ) OVERRIDE
	{
		return !InClass->HasAnyClassFlags(DisallowedClassFlags) && InFilterFuncs->IfInClassesSet( DisallowedClasses , InClass ) == EFilterReturn::Failed
			&& InFilterFuncs->IfInChildOfClassesSet( AllowedChildrenOfClasses, InClass) != EFilterReturn::Failed;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) OVERRIDE
	{
		return !InUnloadedClassData->HasAnyClassFlags(DisallowedClassFlags) && InFilterFuncs->IfInClassesSet( DisallowedClasses , InUnloadedClassData ) == EFilterReturn::Failed
			&& InFilterFuncs->IfInChildOfClassesSet( AllowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed;
	}
};


USlateWidgetStyleAssetFactory::USlateWidgetStyleAssetFactory( const class FPostConstructInitializeProperties& PCIP )
 : Super(PCIP)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = USlateWidgetStyleAsset::StaticClass();
	StyleType = NULL;
}

FText USlateWidgetStyleAssetFactory::GetDisplayName() const
{
	return NSLOCTEXT("SlateWidgetStyleAssetFactory", "SlateWidgetStyleAssetFactoryDescription", "Slate Widget Style");
}

bool USlateWidgetStyleAssetFactory::ConfigureProperties()
{
	// NULL the DataAssetClass so we can check for selection
	StyleType = NULL;

	// Load the classviewer module to display a class picker
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	// Fill in options
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;

	TSharedPtr<FClassFilter> Filter = MakeShareable(new FClassFilter);
	Options.ClassFilter = Filter;

	Filter->DisallowedClassFlags = CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists;
	Filter->AllowedChildrenOfClasses.Add( USlateWidgetStyleContainerBase::StaticClass() );
	Filter->DisallowedClasses.Add( USlateWidgetStyleContainerBase::StaticClass() );

	const FText TitleText = NSLOCTEXT("SlateWidgetStyleAssetFactory", "CreateSlateWidgetStyleAssetOptions", "Pick Slate Widget Style Class");
	UClass* ChosenClass = NULL;
	const bool bPressedOk = SClassPickerDialog::PickClass(TitleText, Options, ChosenClass, USlateWidgetStyleAsset::StaticClass());

	if ( bPressedOk )
	{
		StyleType = ChosenClass;
	}

	return bPressedOk;
}


UObject* USlateWidgetStyleAssetFactory::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	USlateWidgetStyleAsset* NewUSlateStyle = CastChecked<USlateWidgetStyleAsset>(StaticConstructObject(USlateWidgetStyleAsset::StaticClass(), InParent, Name, Flags));

	//intialize
	NewUSlateStyle->CustomStyle = CastChecked< USlateWidgetStyleContainerBase >( StaticConstructObject(StyleType, NewUSlateStyle, Name) );

	return NewUSlateStyle;
}