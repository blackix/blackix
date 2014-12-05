// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ForwardShadingRenderer.cpp: Scene rendering code for the ES2 feature level.
=============================================================================*/

#include "RendererPrivate.h"
#include "Engine.h"
#include "ScenePrivate.h"
#include "FXSystem.h"
#include "PostProcessing.h"
#include "SceneFilterRendering.h"
#include "PostProcessMobile.h"
#include "SceneUtils.h"

uint32 GetShadowQuality();


FForwardShadingSceneRenderer::FForwardShadingSceneRenderer(const FSceneViewFamily* InViewFamily,FHitProxyConsumer* HitProxyConsumer)
	:	FSceneRenderer(InViewFamily, HitProxyConsumer)
{
}

/**
 * Initialize scene's views.
 * Check visibility, sort translucent items, etc.
 */
void FForwardShadingSceneRenderer::InitViews(FRHICommandListImmediate& RHICmdList)
{
	SCOPED_DRAW_EVENT(RHICmdList, InitViews);

	SCOPE_CYCLE_COUNTER(STAT_InitViewsTime);

	PreVisibilityFrameSetup(RHICmdList);
	ComputeViewVisibility(RHICmdList);
	PostVisibilityFrameSetup();

	bool bDynamicShadows = ViewFamily.EngineShowFlags.DynamicShadows && GetShadowQuality() > 0;

	if (bDynamicShadows && !IsSimpleDynamicLightingEnabled())
	{
		// Setup dynamic shadows.
		InitDynamicShadows(RHICmdList);		
	}

	// initialize per-view uniform buffer.  Pass in shadow info as necessary.
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>* DirectionalLightShadowInfo = nullptr;

		FViewInfo& ViewInfo = Views[ViewIndex];
		FScene* Scene = (FScene*)ViewInfo.Family->Scene;
		if (bDynamicShadows && Scene->SimpleDirectionalLight)
		{
			int32 LightId = Scene->SimpleDirectionalLight->Id;
			if (VisibleLightInfos.IsValidIndex(LightId))
			{
				const FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightId];
				if (VisibleLightInfo.AllProjectedShadows.Num() > 0)
				{
					DirectionalLightShadowInfo = &VisibleLightInfo.AllProjectedShadows;
				}
			}
		}

		// Initialize the view's RHI resources.
		Views[ViewIndex].InitRHIResources(DirectionalLightShadowInfo);
	}

	OnStartFrame();
}

/** 
* Renders the view family. 
*/
void FForwardShadingSceneRenderer::Render(FRHICommandListImmediate& RHICmdList)
{
	if(!ViewFamily.EngineShowFlags.Rendering)
	{
		return;
	}

	auto FeatureLevel = ViewFamily.GetFeatureLevel();

	// Initialize global system textures (pass-through if already initialized).
	GSystemTextures.InitializeTextures(RHICmdList, FeatureLevel);

	// Allocate the maximum scene render target space for the current view family.
	GSceneRenderTargets.Allocate(ViewFamily);

	// Find the visible primitives.
	InitViews(RHICmdList);
	
	RenderShadowDepthMaps(RHICmdList);

	// Notify the FX system that the scene is about to be rendered.
	if (Scene->FXSystem)
	{
		Scene->FXSystem->PreRender(RHICmdList);
	}

	GRenderTargetPool.VisualizeTexture.OnStartFrame(Views[0]);

	// Dynamic vertex and index buffers need to be committed before rendering.
	FGlobalDynamicVertexBuffer::Get().Commit();
	FGlobalDynamicIndexBuffer::Get().Commit();

	const bool bGammaSpace = !IsMobileHDR();
	if( bGammaSpace )
	{
		SetRenderTarget(RHICmdList, ViewFamily.RenderTarget->GetRenderTargetTexture(), GSceneRenderTargets.GetSceneDepthTexture(), ESimpleRenderTargetMode::EClearToDefault);
	}
	else
	{
		// Begin rendering to scene color
        GSceneRenderTargets.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EClearToDefault);
	}

	RenderForwardShadingBasePass(RHICmdList);

	// Make a copy of the scene depth if the current hardware doesn't support reading and writing to the same depth buffer
	GSceneRenderTargets.ResolveSceneDepthToAuxiliaryTexture(RHICmdList);

	// Notify the FX system that opaque primitives have been rendered.
	if (Scene->FXSystem)
	{
		Scene->FXSystem->PostRenderOpaque(RHICmdList);
	}

	// Draw translucency.
	if (ViewFamily.EngineShowFlags.Translucency)
	{
		SCOPE_CYCLE_COUNTER(STAT_TranslucencyDrawTime);

		// Note: Forward pass has no SeparateTranslucency, so refraction effect order with Transluency is different.
		// Having the distortion applied between two different translucency passes would make it consistent with the deferred pass.
		// This is not done yet.

		if (ViewFamily.EngineShowFlags.Refraction)
		{
			// to apply refraction effect by distorting the scene color
			RenderDistortion(RHICmdList);
		}
		RenderTranslucency(RHICmdList);
	}

	if( !bGammaSpace )
	{
		// This might eventually be a problem with multiple views.
		// Using only view 0 to check to do on-chip transform of alpha.
		FViewInfo& View = Views[0];

		static const auto CVarMobileMSAA = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileMSAA"));

		bool bOnChipSunMask = 
			GSupportsRenderTargetFormat_PF_FloatRGBA && 
			GSupportsShaderFramebufferFetch &&
			ViewFamily.EngineShowFlags.PostProcessing &&
			((View.bLightShaftUse) || (View.FinalPostProcessSettings.DepthOfFieldScale > 0.0) || 
			((ViewFamily.GetShaderPlatform() == SP_METAL) && (CVarMobileMSAA ? CVarMobileMSAA->GetValueOnAnyThread() > 1 : false))
			);

		// Convert alpha from depth to circle of confusion with sunshaft intensity.
		// This is done before resolve on hardware with framebuffer fetch.
		if(bOnChipSunMask)
		{
			// This will break when PrePostSourceViewportSize is not full size.
			FIntPoint PrePostSourceViewportSize = GSceneRenderTargets.GetBufferSizeXY();

			FMemMark Mark(FMemStack::Get());
			FRenderingCompositePassContext CompositeContext(RHICmdList, View);

			FRenderingCompositePass* PostProcessSunMask = CompositeContext.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSunMaskES2(PrePostSourceViewportSize, true));
			CompositeContext.Root->AddDependency(FRenderingCompositeOutputRef(PostProcessSunMask));
			CompositeContext.Process(TEXT("OnChipAlphaTransform"));
		}

		// Resolve the scene color for post processing.
		GSceneRenderTargets.ResolveSceneColor(RHICmdList, FResolveRect(0, 0, ViewFamily.FamilySizeX, ViewFamily.FamilySizeY));

		// Drop depth and stencil before post processing to avoid export.
		RHICmdList.DiscardRenderTargets(true, true, 0);

		// Finish rendering for each view, or the full stereo buffer if enabled
		if (ViewFamily.bResolveScene)
		{
			if (ViewFamily.EngineShowFlags.StereoRendering)
			{
				check(Views.Num() > 1);

				//@todo ES2 stereo post: until we get proper stereo postprocessing for ES2, process the stereo buffer as one view
				FIntPoint OriginalMax0 = Views[0].ViewRect.Max;
				Views[0].ViewRect.Max = Views[1].ViewRect.Max;
				GPostProcessing.ProcessES2(RHICmdList, Views[0], bOnChipSunMask);
				Views[0].ViewRect.Max = OriginalMax0;
			}
			else
			{
				SCOPED_DRAW_EVENT(RHICmdList, PostProcessing);
				SCOPE_CYCLE_COUNTER(STAT_FinishRenderViewTargetTime);
				for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
				{	
					SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
					GPostProcessing.ProcessES2(RHICmdList, Views[ViewIndex], bOnChipSunMask);
				}
			}
		}
	}

	RenderFinish(RHICmdList);
}
