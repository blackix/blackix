// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ClusteredShadingRenderer.h: clustered forward renderer
=============================================================================*/

#pragma once

#include "TextureLayout.h"
#include "DistortionRendering.h"
#include "CustomDepthRendering.h"
#include "HeightfieldLighting.h"
#include "DepthRendering.h"
#include "LightSceneInfo.h"


/**
 * Renderer that implements clustered forward shading
 */
class FClusteredForwardShadingSceneRenderer : public FSceneRenderer
{
public:
	enum {
		// Size of the screen-space grid tiles, in pixels
		kLightGridTileSizeX = 32,
		kLightGridTileSizeY = 32,
		// Number of depth slices in the froxel grid (x,y resolution depends on screen resolution/TILE_SIZE)
		kLightGridSlicesZ = 32
	};

	/** Defines which objects we want to render in the EarlyZPass. */
	EDepthDrawingMode EarlyZPassMode;

	/** Calculates the size of the light grid for a given viewport size */
	static FIntVector CalcLightGridSize(const FIntPoint& ScreenSize);

public:
	FClusteredForwardShadingSceneRenderer(const FSceneViewFamily* InViewFamily,FHitProxyConsumer* HitProxyConsumer);

	// FSceneRenderer interface

	virtual void Render(FRHICommandListImmediate& RHICmdList) override;

protected:

	virtual void InitViews(FRHICommandListImmediate& RHICmdList) override;

	void SortStateBuckets();
	void SortBasePassStaticData();

	/** Finds the visible dynamic shadows for each view. */
	void InitDynamicShadows(FRHICommandListImmediate& RHICmdList);

	/** Renders the opaque base pass for forward shading. */
	void RenderForwardShadingBasePass(FRHICommandListImmediate& RHICmdList, ESceneDepthPriorityGroup);
	bool RenderForwardShadingBasePassView(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, ESceneDepthPriorityGroup);
	bool RenderEditorPrimitivesView(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, ESceneDepthPriorityGroup DepthPriorityGroup);
	void SetupBasePassView(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, bool bIsEditorPrimitives);


	/** Renders the base pass for translucency. */
	void RenderTranslucency(FRHICommandListImmediate& RHICmdList, ESceneDepthPriorityGroup);
	bool RenderTranslucencyView(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, ESceneDepthPriorityGroup);

	/** Renders any necessary shadowmaps. */
	void RenderShadowDepthMaps(FRHICommandListImmediate& RHICmdList);

	/**
	  * Used by RenderShadowDepthMaps to render shadowmap for the given light.
	  *
	  * @param LightSceneInfo Represents the current light
	  * @return true if anything got rendered
	  */
	bool RenderShadowDepthMap(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo);

	/** Renders the background layer to a separate rendertarget.
	 * Returns if there was anything rendered to the layer and if it can be used.
	 */
	bool RenderBackgroundLayer(FRHICommandListImmediate& RHICmdList);
	void BlitBackgroundToViews(FRHICommandListImmediate& RHICmdList);

	bool RenderPrePass(FRHICommandListImmediate& RHICmdList, ESceneDepthPriorityGroup DepthPriorityGroup, bool bDepthWasCleared);
	bool RenderPrePassView(FRHICommandList& RHICmdList, const FViewInfo& View, ESceneDepthPriorityGroup DepthPriorityGroup);
	bool RenderPrePassViewDynamic(FRHICommandList& RHICmdList, const FViewInfo& View, ESceneDepthPriorityGroup DepthPriorityGroup);

	void RenderForwardDistortion(FRHICommandListImmediate& RHICmdList);

	void RenderOcclusion(FRHICommandListImmediate& RHICmdList, bool bRenderQueries, bool bRenderHZB);

	/** Very basic post-processing for testing */
	void BasicPostProcess(FRHICommandListImmediate& RHICmdList, FViewInfo &View);

	/** Injects lights into the translucent lighting volume */
	bool InjectLightsIntoTranslucentVolume(FRHICommandListImmediate& RHICmdList);

	/** Builds the Clusters */
	void InitClusteredLightInfo(FRHICommandListImmediate& RHICmdList);

	/** Inject lights into the light grid */
	void InjectLightsIntoLightGrid(FRHICommandListImmediate& RHICmdList);

	void MergeVisibleBatches(FViewInfo& Out, const FViewInfo& In, ESceneDepthPriorityGroup DepthPriorityGroup);

	static FVector GetLightGridZParams(float NearPlane, float FarPlane);

private:
	/** fences to make sure the rhi thread has digested the occlusion query renders before we attempt to read them back async */
	static FGraphEventRef OcclusionSubmittedFence[FOcclusionQueryHelpers::MaxBufferedOcclusionFrames];

	// One per view.
	TArray<FClusteredLightsSceneInfo> ClusteredLightInfo;
	bool bHasAnyLights;

	// Stores the background's viewing matrices if it was rendered.
	FViewInfo* BackgroundView = nullptr;
};
