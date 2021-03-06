// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "CullDistanceVolume.generated.h"

/**
 * Helper structure containing size and cull distance pair.
 */
USTRUCT()
struct FCullDistanceSizePair
{
	GENERATED_USTRUCT_BODY()

	/** Size to associate with cull distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CullDistanceSizePair)
	float Size;

	/** Cull distance associated with size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CullDistanceSizePair)
	float CullDistance;


	FCullDistanceSizePair()
		: Size(0)
		, CullDistance(0)
	{
	}

	FCullDistanceSizePair(float InSize, float InCullDistance)
		: Size(InSize)
		, CullDistance(InCullDistance)
	{
	}


};

UCLASS(hidecategories=(Advanced, Attachment, Collision, Volume))
class ACullDistanceVolume : public AVolume
{
	GENERATED_UCLASS_BODY()

	/**
	 * Array of size and cull distance pairs. The code will calculate the sphere diameter of a primitive's BB and look for a best
	 * fit in this array to determine which cull distance to use.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CullDistanceVolume)
	TArray<struct FCullDistanceSizePair> CullDistances;

	/**
	 * Whether the volume is currently enabled or not.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CullDistanceVolume)
	uint32 bEnabled:1;



	// Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
	virtual void PostEditMove(bool bFinished) OVERRIDE;
#endif // WITH_EDITOR
	// End UObject Interface

	/** 
	 * Override Destroyed so that we can re-calculate primitive draw distances after this volume has been deleted.
	 */
	virtual void Destroyed();

	/**
	 * Returns whether the passed in primitive can be affected by cull distance volumes.
	 *
	 * @param	PrimitiveComponent	Component to test
	 * @return	true if tested component can be affected, false otherwise
	 */
	static bool CanBeAffectedByVolumes( UPrimitiveComponent* PrimitiveComponent );

	/** Get the set of primitives and new max draw distances defined by this volume. */
	void GetPrimitiveMaxDrawDistances(TMap<UPrimitiveComponent*,float>& OutCullDistances);
};



