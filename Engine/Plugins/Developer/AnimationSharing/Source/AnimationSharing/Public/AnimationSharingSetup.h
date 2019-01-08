// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "AnimationSharingTypes.h"

#include "AnimationSharingSetup.generated.h"

UCLASS(hidecategories = Object, Blueprintable, config = Engine)
class ANIMATIONSHARING_API UAnimationSharingSetup : public UObject
{
	GENERATED_UCLASS_BODY()
public:

#if WITH_EDITOR
	virtual void PostLoad() override;
#endif // WITH_EDITOR

	UPROPERTY(EditAnywhere, config, Category = AnimationSharing)
	TArray<FPerSkeletonAnimationSharingSetup> SkeletonSetups;

	UPROPERTY(EditAnywhere, config, Category = AnimationSharing)
	FAnimationSharingScalability ScalabilitySettings;
};