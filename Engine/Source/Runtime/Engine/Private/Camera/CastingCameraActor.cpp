// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Camera/CastingCameraActor.h"

#if WITH_OCULUS_PRIVATE_CODE

#define LOCTEXT_NAMESPACE "CastingCameraActor"

//////////////////////////////////////////////////////////////////////////
// ACastingCameraActor

ACastingCameraActor::ACastingCameraActor(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , ClippingPlaneDistance(300.0f)
    , ClippingPlaneDistanceTolerance(20.0f)
    , ForegroundLayerBackgroundColor(FLinearColor::Green)
{
}

void ACastingCameraActor::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
}

void ACastingCameraActor::PostLoadSubobjects(FObjectInstancingGraph* OuterInstanceGraph)
{
    Super::PostLoadSubobjects(OuterInstanceGraph);
}

void ACastingCameraActor::BeginPlay()
{
    Super::BeginPlay();
}

#if WITH_EDITOR
void ACastingCameraActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

#undef LOCTEXT_NAMESPACE

#endif // WITH_OCULUS_PRIVATE_CODE