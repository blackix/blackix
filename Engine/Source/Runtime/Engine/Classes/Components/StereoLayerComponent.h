// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "StereoLayerComponent.generated.h"

/** Used by IStereoLayer */
UENUM()
enum EStereoLayerPositionType
{
	/** Location within the world */
	SLPT_WorldLocked UMETA(DisplayName = "World Locked"),

	/** Location within the HMD tracking space */
	SLPT_TorsoLocked UMETA(DisplayName = "Torso Locked"),

	/** Location within the view space */
	SLPT_FaceLocked UMETA(DisplayName = "Face Locked"),

	SLPT_MAX,
};

UENUM()
enum EStereoLayerType
{
	/** Quad layer */
	SLT_QuadLayer UMETA(DisplayName = "Quad Layer"),

	/** Cylinder layer */
	SLT_CylinderLayer UMETA(DisplayName = "Cylinder Layer"),

	/** Cubemap layer */
	SLT_CubemapLayer UMETA(DisplayName = "Cubemap Layer"),

	SLT_MAX,
};

/** 
 * A geometry layer within the stereo rendered viewport.
 */
UCLASS(ClassGroup="HeadMountedDisplay", hidecategories=(Object,LOD,Lighting,TextureStreaming), editinlinenew, meta=(DisplayName="Stereo Layer", BlueprintSpawnableComponent))
class ENGINE_API UStereoLayerComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()
    friend class FStereoLayerComponentVisualizer;
    
public:

	//~ Begin UObject Interface
	void BeginDestroy() override;

	//~ Begin UActorComponent Interface
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	
	/** 
	 * Change the texture displayed on the stereo layer quad
	 * @param	InTexture: new Texture2D
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Stereo Layer")
	void SetTexture(UTexture* InTexture);

	// @return the texture mapped to the stereo layer quad
	UFUNCTION(BlueprintCallable, Category="Components|Stereo Layer")
	UTexture* GetTexture() const { return Texture; }
	
	/** 
	 * Change the quad size. This is the unscaled height and width, before component scale is applied.
	 * @param	InQuadSize: new quad size.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Stereo Layer")
	void SetQuadSize(FVector2D InQuadSize);

	// @return the height and width of the rendered quad
	UFUNCTION(BlueprintCallable, Category="Components|Stereo Layer")
	FVector2D GetQuadSize() const { return QuadSize; }

	/** 
	 * Change the UV coordinates mapped to the quad face
	 * @param	InUVRect: Min and Max UV coordinates
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Stereo Layer")
	void SetUVRect(FBox2D InUVRect);

	// @return the UV coordinates mapped to the quad face
	UFUNCTION(BlueprintCallable, Category="Components|Stereo Layer")
	FBox2D GetUVRect() const { return UVRect; }
	
	/** 
	 * Change the layer's render priority, higher priorities render on top of lower priorities
	 * @param	InPriority: Priority value
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Stereo Layer")
	void SetPriority(int32 InPriority);

	// @return the render priority
	UFUNCTION(BlueprintCallable, Category="Components|Stereo Layer")
	int32 GetPriority() const { return Priority; }

	// Manually mark the stereo layer texture for updating
	UFUNCTION(BlueprintCallable, Category="Components|Stereo Layer")
	void MarkTextureForUpdate();

	/** True if the stereo layer texture needs to update itself every frame(scene capture, video, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= "StereoLayer")
	uint32 bLiveTexture:1;

	/** True if the stereo layer needs to support depth intersections with the scene geometry */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StereoLayer")
	uint32 bSupportsDepth : 1;

	/** True if the texture should not use its own alpha channel (1.0 will be substituted) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= "StereoLayer")
	uint32 bNoAlphaChannel:1;

protected:
	/** Texture displayed on the stereo layer for right eye (if only one texture provided, mono assumed) **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category= "StereoLayer")
	class UTexture* Texture;

	/** Texture displayed on the stereo layer for left eye (if only one texture provided, mono assumed) **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StereoLayer | Cubemap Overlay Properties")
	class UTexture* LeftTexture;

public:
	/** True if the quad should internally set it's Y value based on the set texture's dimensions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StereoLayer")
	uint32 bQuadPreserveTextureRatio : 1;

protected:
	/** Size of the rendered stereo layer quad **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category="StereoLayer | Quad Overlay Properties")
	FVector2D QuadSize;

	/** UV coordinates mapped to the quad face **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category="StereoLayer | Quad Overlay Properties")
	FBox2D UVRect;

	/** Size of the rendered stereo layer quad **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category = "StereoLayer | Cylinder Overlay Properties")
	float CylinderRadius;

	/** UV coordinates mapped to the quad face **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category = "StereoLayer | Cylinder Overlay Properties")
	float CylinderOverlayArc;

	/** UV coordinates mapped to the quad face **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category = "StereoLayer | Cylinder Overlay Properties")
	int CylinderHeight;

	/** Specifies how and where the quad is rendered to the screen **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category="StereoLayer")
    TEnumAsByte<enum EStereoLayerPositionType> StereoLayerPositionType;

	/** Specifies which type of layer it is **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category = "StereoLayer")
	TEnumAsByte<enum EStereoLayerType> StereoLayerType;

	/** Render priority among all stereo layers, higher priority render on top of lower priority **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category= "StereoLayer")
	int32 Priority;

private:
	/** Dirty state determines whether the stereo layer needs updating **/
	bool bIsDirty;

	/** Texture needs to be marked for update **/
	bool bTextureNeedsUpdate;

	/** IStereoLayer id, 0 is unassigned **/
	uint32 LayerId;

	/** Last transform is cached to determine if the new frames transform has changed **/
	FTransform LastTransform;

	/** Last frames visiblity state **/
	bool bLastVisible;
};

