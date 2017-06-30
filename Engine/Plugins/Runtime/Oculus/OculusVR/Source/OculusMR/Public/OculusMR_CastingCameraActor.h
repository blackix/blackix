#pragma once

#include "IOculusMRModule.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Camera/CastingCameraActor.h"
#include "Engine/Scene.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/CastingViewportClient.h"
#include "OculusMRFunctionLibrary.h"
#include "OVR_Plugin_MixedReality.h"

#include "OculusMR_CastingCameraActor.generated.h"

class UOculusMR_PlaneMeshComponent;
class UMaterial;

UENUM(BlueprintType)
enum class EOculusMR_CameraDeviceEnum : uint8
{
    CD_None         UMETA(DisplayName = "None"),
    CD_WebCamera0   UMETA(DisplayName = "WebCamera0"),
    CD_WebCamera1   UMETA(DisplayName = "WebCamera1"),
};

UENUM(BlueprintType)
enum class EOculusMR_ClippingReference : uint8
{
    CR_TrackingReference    UMETA(DisplayName = "TrackingReference"),
    CR_Head                 UMETA(DisplayName = "Head"),
};

/**
* A CameraActor is a camera viewpoint that can be placed in a level.
*/
UCLASS(ClassGroup=OculusMR, Blueprintable)
class AOculusMR_CastingCameraActor : public ACastingCameraActor
{
    GENERATED_UCLASS_BODY()
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaTime) override;

    virtual void BeginDestroy() override;

    void LoadFromIni();

    /** Automatically starts the MxR casting when the level starts */
    UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
    bool bCastingAutoStart;

    /** Project the MxR casting to the MirrorWindow. 
      * By default the MxR is casted to a standalone window, which offers the most precision in the composition. 
      * However, it could also be casted to the MirrorWindow to simplify the window switching, especially on 
      * a single-monitor configuration.
      * The casting window would be automatically minimized when the bProjectToMirrorWindow set to true
      */
    UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
    bool bProjectToMirrorWindow;

    /** MultiView: The casting window includes the background and foreground view
      * DirectComposition: The game scene would be composited with the camera frame directly	
      */
    UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
    ECastingViewportCompositionMethod CompositionMethod;

    /** Specify the distance to the camera which divide the background and foreground in MxR casting. 
      * Set it to CR_TrackingReference to use the distance to the Tracking Reference, which works better 
      * in the stationary experience. Set it to CR_Head would use the distance to the HMD, which works better 
      * in the room scale experience.
      */
    UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
    EOculusMR_ClippingReference ClippingReference;

    /** Information about the tracked camera which this object is bound to */
    UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
    FTrackedCamera TrackedCamera;

    /** (optional) If a “VROrigin” component is used to setup the origin of the tracking space, 
      * set the component to it. Otherwise, the system would use the location of the first PlayerController 
      * as the tracking reference. */
    UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
    class USceneComponent* TrackingReferenceComponent;

    /** Set to true to let the casting camera follows the movement of the tracking reference automatically. */
    UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
    bool bFollowTrackingReference;

    /** The casting viewports would use the same resolution of the camera which used in the calibration process. */
    UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
    bool bUseTrackedCameraResolution;

    /** When bUseTrackedCameraResolution is false, the width of each casting viewport */
    UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
    int WidthPerView;

    /** When bUseTrackedCameraResolution is false, the height of each casting viewport */
    UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
    int HeightPerView;

    /** When CompositionMethod is DirectComposition, the physical camera device which provide the frame */
    UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
    EOculusMR_CameraDeviceEnum CapturingCamera;

    /** When CompositionMethod is MultiView, the latency of the casting output which could be adjusted to 
      * match the camera latency in the external composition application */
    UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "0.1"))
    float CastingLatency;

    /** [Green-screen removal] When CompositionMethod is DirectComposition, how heavily to weight non-green values in a pixel */
    UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "20.0"))
    float ChromaTorelanceA;

    /** [Green-screen removal] When CompositionMethod is DirectComposition, how heavily to weight the green value. 
      * If mid-range greens don't seem to cut out, increasing B or decreasing A may help. */
    UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "20.0"))
    float ChromaTorelanceB;

    /** [Green-screen removal] When CompositionMethod is DirectComposition, the shadow threshold is to get rid 
      * of really dark pixels to mitigate the shadow casting issues */
    UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "0.1"))
    float ChromaShadows;

    /** [Green-screen removal] When CompositionMethod is DirectComposition, alpha cutoff is evaluated after 
      * the chroma-key evaluation and before the bleed test to take pixels with a low alpha value and fully discard them.*/
    UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "0.1"))
    float ChromaAlphaCutoff;

    UPROPERTY()
    class UOculusMR_CastingWindowComponent* CastingWindowComponent;

    UPROPERTY()
    UTexture2D* CameraColorTexture;

    UPROPERTY()
    UOculusMR_PlaneMeshComponent* PlaneMeshComponent;

    UPROPERTY()
    UMaterial* GreenScreenMaterial;

    UPROPERTY()
    UMaterialInstanceDynamic* GreenScreenMaterialInstance;

    ovrpCameraDevice CurrentCapturingCamera;

    bool TrackedCameraCalibrationRequired;
    bool HasTrackedCameraCalibrationCalibrated;
    FQuat InitialCameraAbsoluteOrientation;
    FVector InitialCameraAbsolutePosition;
    FQuat InitialCameraRelativeOrientation;
    FVector InitialCameraRelativePosition;

    /** Bind the casting camera to the calibrated external camera.
      * If there is no calibrated external camera, the TrackedCamera parameters would be setup to match the placement 
      * of CastingCameraActor. It provides an easy way to directly place a stationary casting camera in the level.
      */
    UFUNCTION(BlueprintCallable, Category = OculusMR)
    void BindToTrackedCameraIndexIfAvailable(int InTrackedCameraIndex);

    /** When bFollowTrackingReference is false, manually call this method to move the casting camera to follow the tracking reference (i.e. player) */
    UFUNCTION(BlueprintCallable, Category = OculusMR)
    void RequestTrackedCameraCalibration();

    bool RefreshExternalCamera();
    void UpdateCameraColorTexture(const ovrpSizei &colorFrameSize, const ovrpByte* frameData, int rowPitch);

    void CalibrateTrackedCameraPose();
    void SetTrackedCameraUserPoseWithCameraTransform();
    void SetTrackedCameraInitialPoseWithPlayerTransform();
    void UpdateTrackedCameraPosition();

public:
    /** Open the casting window */
    UFUNCTION(BlueprintCallable, Category = OculusMR)
    void OpenCastingWindow();

    /** Close the casting window */
    UFUNCTION(BlueprintCallable, Category = OculusMR)
    void CloseCastingWindow();

    /** Toggle the casting window */
    UFUNCTION(BlueprintCallable, Category = OculusMR)
    void ToggleCastingWindow();

    /** Check if the casting window has already been opened */
    UFUNCTION(BlueprintCallable, Category = OculusMR)
    bool HasCastingWindowOpened() const;

protected:
    void RepositionPlaneMesh();
};

