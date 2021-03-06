// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "DirectionalLightComponentDetails.h"

#define LOCTEXT_NAMESPACE "DirectionalLightComponentDetails"


TSharedRef<IDetailCustomization> FDirectionalLightComponentDetails::MakeInstance()
{
	return MakeShareable( new FDirectionalLightComponentDetails );
}

void FDirectionalLightComponentDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	// Grab the Mobility property from SceneComponent
	MobilityProperty = DetailBuilder.GetProperty("Mobility", USceneComponent::StaticClass());

	// Get cascaded shadow map category
	IDetailCategoryBuilder& ShadowMapCategory = DetailBuilder.EditCategory("CascadedShadowMaps", TEXT(""), ECategoryPriority::Default );

	// Add DynamicShadowDistanceMovableLight
	TSharedPtr<IPropertyHandle> MovableShadowRadiusProperty = DetailBuilder.GetProperty("DynamicShadowDistanceMovableLight");
	ShadowMapCategory.AddProperty( MovableShadowRadiusProperty)
		.IsEnabled( TAttribute<bool>( this, &FDirectionalLightComponentDetails::IsLightMovable ) );

	// Add DynamicShadowDistanceStationaryLight
	TSharedPtr<IPropertyHandle> StationaryShadowRadiusProperty = DetailBuilder.GetProperty("DynamicShadowDistanceStationaryLight");
	ShadowMapCategory.AddProperty( StationaryShadowRadiusProperty)
		.IsEnabled( TAttribute<bool>( this, &FDirectionalLightComponentDetails::IsLightStationary ) );

	TSharedPtr<IPropertyHandle> LightIntensityProperty = DetailBuilder.GetProperty("Intensity", ULightComponentBase::StaticClass());
	// Point lights need to override the ui min and max for units of lumens, so we have to undo that
	LightIntensityProperty->GetProperty()->SetMetaData("UIMin",TEXT("0.0f"));
	LightIntensityProperty->GetProperty()->SetMetaData("UIMax",TEXT("20.0f"));
}

bool FDirectionalLightComponentDetails::IsLightMovable() const
{
	uint8 Mobility;
	MobilityProperty->GetValue(Mobility);
	return (Mobility == EComponentMobility::Movable);
}

bool FDirectionalLightComponentDetails::IsLightStationary() const
{
	uint8 Mobility;
	MobilityProperty->GetValue(Mobility);
	return (Mobility == EComponentMobility::Stationary);
}

#undef LOCTEXT_NAMESPACE
