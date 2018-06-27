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
class ASceneCapture2D;
class AOculusMR_BoundaryActor;

UENUM(BlueprintType)
enum class EOculusMR_CameraDeviceEnum : uint8
{
	CD_None         UMETA(DisplayName = "None"),
	CD_WebCamera0   UMETA(DisplayName = "Web Camera 0"),
	CD_WebCamera1   UMETA(DisplayName = "Web Camera 1"),
	CD_ZEDCamera    UMETA(DisplayName = "ZED Camera"),
};

UENUM(BlueprintType)
enum class EOculusMR_ClippingReference : uint8
{
	CR_TrackingReference    UMETA(DisplayName = "Tracking Reference"),
	CR_Head                 UMETA(DisplayName = "Head"),
};

UENUM(BlueprintType)
enum class EOculusMR_VirtualGreenScreenType : uint8
{
	VGS_Off              UMETA(DisplayName = "Off"),
	VGS_OuterBoundary    UMETA(DisplayName = "Outer Boundary"),
	VGS_PlayArea         UMETA(DisplayName = "Play Area")
};

UENUM(BlueprintType)
enum class EOculusMR_DepthQuality : uint8
{
	DQ_Low              UMETA(DisplayName = "Low"),
	DQ_Medium           UMETA(DisplayName = "Medium"),
	DQ_High             UMETA(DisplayName = "High")
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

	void SaveToIni();
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

	/** (optional) If a "VROrigin" component is used to setup the origin of the tracking space, 
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

	/** When CompositionMethod is Direct Composition, you could adjust this latency to delay the virtual
	* hand movement by a small amount of time to match the camera latency */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "0.5"))
	float HandPoseStateLatency;

	/** [Green-screen removal] Chroma Key Color. Apply when CompositionMethod is DirectComposition */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
	FColor ChromaKeyColor;

	/** [Green-screen removal] Chroma Key Similarity. Apply when CompositionMethod is DirectComposition */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "1.0"))
	float ChromaKeySimilarity;

	/** [Green-screen removal] Chroma Key Smooth Range. Apply when CompositionMethod is DirectComposition */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "0.2"))
	float ChromaKeySmoothRange;

	/** [Green-screen removal] Chroma Key Spill Range. Apply when CompositionMethod is DirectComposition */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "0.2"))
	float ChromaKeySpillRange;

	/** The type of virtual green screen */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
	EOculusMR_VirtualGreenScreenType VirtualGreenScreenType;

	/** Use the in-game lights on the camera frame */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
	bool bUseDynamicLighting;

	/** The quality level of the depth sensor */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
	EOculusMR_DepthQuality DepthQuality;

	/** Larger values make dynamic lighting effects smoother, but values that are too large make the lighting look flat. */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "16"))
	float DynamicLightingDepthSmoothFactor;

	/** Sets the maximum depth variation across edges (smaller values set smoother edges) */
	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "0.1"))
	float DynamicLightingDepthVariationClampingValue;

	UPROPERTY()
	class UOculusMR_CastingWindowComponent* CastingWindowComponent;

	UPROPERTY()
	class UVRNotificationsComponent* VRNotificationComponent;

	UPROPERTY()
	UTexture2D* CameraColorTexture;

	UPROPERTY()
	UTexture2D* CameraDepthTexture;

	UPROPERTY()
	UOculusMR_PlaneMeshComponent* PlaneMeshComponent;

	UPROPERTY()
	UMaterial* ChromaKeyMaterial;

	UPROPERTY()
	UMaterial* ChromaKeyLitMaterial;

	UPROPERTY()
	UMaterial* OpaqueColoredMaterial;

	UPROPERTY()
	UMaterialInstanceDynamic* ChromaKeyMaterialInstance;

	UPROPERTY()
	UMaterialInstanceDynamic* ChromaKeyLitMaterialInstance;

	UPROPERTY()
	UMaterialInstanceDynamic* CameraFrameMaterialInstance;

	UPROPERTY()
	UMaterialInstanceDynamic* BackdropMaterialInstance;

	UPROPERTY()
	AOculusMR_BoundaryActor* BoundaryActor;

	UPROPERTY()
	ASceneCapture2D* BoundarySceneCaptureActor;

	UPROPERTY()
	class UTexture2D* DefaultTexture_White;

	ovrpCameraDevice CurrentCapturingCamera;

	bool TrackedCameraCalibrationRequired;
	bool HasTrackedCameraCalibrationCalibrated;
	FQuat InitialCameraAbsoluteOrientation;
	FVector InitialCameraAbsolutePosition;
	FQuat InitialCameraRelativeOrientation;
	FVector InitialCameraRelativePosition;

	int32 RefreshBoundaryMeshCounter;

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
	void UpdateCameraDepthTexture(const ovrpSizei &depthFrameSize, const float* frameData, int rowPitch);

	void CalibrateTrackedCameraPose();
	void SetTrackedCameraUserPoseWithCameraTransform();
	void SetTrackedCameraInitialPoseWithPlayerTransform();
	void UpdateTrackedCameraPosition();
	void UpdateBoundaryCapture();

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

	UFUNCTION(BlueprintCallable, Category = OculusMR)
	void OnHMDRecentered();

protected:
	void SetupCameraFrameMaterialInstance();
	void SetupBackdropMaterialInstance();
	void RepositionPlaneMesh();
	void RefreshBoundaryMesh();

	void Execute_BindToTrackedCameraIndexIfAvailable();

	bool BindToTrackedCameraIndexRequested;
	int BindToTrackedCameraIndex;
};
