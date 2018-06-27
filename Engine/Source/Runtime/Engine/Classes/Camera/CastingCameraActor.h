#pragma once

#include "CoreMinimal.h"

// The WITH_OCULUS_PRIVATE_CODE tag is kept for reference
//#if WITH_OCULUS_PRIVATE_CODE

#include "UObject/ObjectMacros.h"
#include "Camera/CameraActor.h"
#include "Engine/Scene.h"

#include "CastingCameraActor.generated.h"

/**
* A CameraActor is a camera viewpoint that can be placed in a level.
*/
UCLASS(ClassGroup = Common, hideCategories = (Rendering), Blueprintable)
class ENGINE_API ACastingCameraActor : public ACameraActor
{
    GENERATED_UCLASS_BODY()

protected:
    UPROPERTY(Category = CastingCameraActor, EditAnywhere, meta = (AllowPrivateAccess = "true"))
    float ClippingPlaneDistance;

    // Relax the background clipping distance a little bit to prevent "seams" in the composition
    UPROPERTY(Category = CastingCameraActor, EditAnywhere, meta = (AllowPrivateAccess = "true"))
    float ClippingPlaneDistanceTolerance;

    UPROPERTY(Category = CastingCameraActor, EditAnywhere, meta = (AllowPrivateAccess = "true"))
    FLinearColor ForegroundLayerBackgroundColor;

public:
    //~ Begin UObject Interface
    virtual void Serialize(FArchive& Ar) override;
    virtual void PostLoadSubobjects(FObjectInstancingGraph* OuterInstanceGraph) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
    //~ End UObject Interface

    float GetClippingPlaneDistance() const { return ClippingPlaneDistance; }

    float GetClippingPlaneDistanceTolerance() const { return ClippingPlaneDistanceTolerance; }

    const FLinearColor& GetForegroundLayerBackgroundColor() const { return ForegroundLayerBackgroundColor; }

protected:
    //~ Begin AActor Interface
    virtual void BeginPlay() override;
    //~ End AActor Interface
};

//#endif // WITH_OCULUS_PRIVATE_CODE