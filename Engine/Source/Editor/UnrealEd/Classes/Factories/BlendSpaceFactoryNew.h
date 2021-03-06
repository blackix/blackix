// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// BlendSpaceFactoryNew
//=============================================================================

#pragma once
#include "BlendSpaceFactoryNew.generated.h"

UCLASS(hidecategories=Object, MinimalAPI)
class UBlendSpaceFactoryNew : public UFactory
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	class USkeleton*	TargetSkeleton;

	// Begin UFactory Interface
	virtual bool ConfigureProperties() OVERRIDE;
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) OVERRIDE;
	// Begin UFactory Interface

private:
	void OnTargetSkeletonSelected(const FAssetData& SelectedAsset);

private:
	TSharedPtr<SWindow> PickerWindow;
};



