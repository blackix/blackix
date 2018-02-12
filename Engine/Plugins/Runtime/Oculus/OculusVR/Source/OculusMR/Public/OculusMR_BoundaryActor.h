#pragma once

#include "IOculusMRModule.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "OculusMRFunctionLibrary.h"
#include "OVR_Plugin_MixedReality.h"
#include "GameFramework/Actor.h"

#include "OculusMR_BoundaryActor.generated.h"

class UOculusMR_BoundaryMeshComponent;

UCLASS(ClassGroup = OculusMR, Blueprintable)
class AOculusMR_BoundaryActor : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	UOculusMR_BoundaryMeshComponent* BoundaryMeshComponent;

	bool IsBoundaryValid() const;
};
