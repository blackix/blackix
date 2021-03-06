// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "StaticMeshComponentDetails.h"

#define LOCTEXT_NAMESPACE "StaticMeshComponentDetails"


TSharedRef<IDetailCustomization> FStaticMeshComponentDetails::MakeInstance()
{
	return MakeShareable( new FStaticMeshComponentDetails );
}

void FStaticMeshComponentDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	// Create a category so this is displayed early in the properties
	DetailBuilder.EditCategory( "StaticMesh", TEXT(""), ECategoryPriority::Important);
}


#undef LOCTEXT_NAMESPACE
