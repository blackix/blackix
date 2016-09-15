// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ClusteredForwardShadingRenderer.cpp: Scene rendering code for clustered shading forward renderer.
=============================================================================*/

#include "RendererPrivate.h"
#include "Engine.h"
#include "ScenePrivate.h"
#include "FXSystem.h"
#include "PostProcessing.h"
#include "SceneFilterRendering.h"
#include "PostProcessMobile.h"
#include "SceneUtils.h"
#include "ScreenRendering.h"

#include "PostProcessCompositeEditorPrimitives.h"
#include "PostProcessSelectionOutline.h"
#include "PostProcessUpscale.h"

#include "Clustered/ClusteredShadingRenderer.h"

uint32 GetShadowQuality();


FClusteredForwardShadingSceneRenderer::FClusteredForwardShadingSceneRenderer(const FSceneViewFamily* InViewFamily,FHitProxyConsumer* HitProxyConsumer)
	: FSceneRenderer(InViewFamily, HitProxyConsumer, EShadingPath::ClusteredForward)
	, EarlyZPassMode(DDM_NonMaskedOnly)
{
	// developer override, good for profiling, can be useful as project setting
	{
		static const auto ICVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.EarlyZPass"));
		const int32 CVarValue = ICVar->GetValueOnGameThread();

		switch(CVarValue)
		{
			case 0: EarlyZPassMode = DDM_None; break;
			case 1: EarlyZPassMode = DDM_NonMaskedOnly; break;
			case 2: EarlyZPassMode = DDM_AllOccluders; break;
			case 3: break;	// Note: 3 indicates "default behavior" and does not specify an override
		}
	}

	// Enforce MaxShadowCascades
	for (auto& View : Views)
	{
		View.MaxShadowCascades = FMath::Min<int32>(View.MaxShadowCascades, MAX_FORWARD_SHADOWCASCADES);
	}
}

void FClusteredForwardShadingSceneRenderer::SortBasePassStaticData()
{
	FVector AverageViewPosition(0);
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{		
		FViewInfo& View = Views[ViewIndex];
		AverageViewPosition += View.ViewMatrices.ViewOrigin / Views.Num();
	}

	// If we're not using a depth only pass, sort the static draw list buckets roughly front to back, to maximize HiZ culling
	// Note that this is only a very rough sort, since it does not interfere with state sorting, and each list is sorted separately
	if (EarlyZPassMode == DDM_None)
	{
		SCOPE_CYCLE_COUNTER(STAT_SortStaticDrawLists);

		for (int32 DrawType = 0; DrawType < FScene::EBasePass_MAX; DrawType++)
		{
			Scene->BasePassUniformLightMapPolicyDrawList[DrawType].SortFrontToBack(AverageViewPosition);
		}
	}
}

/**
 * Initialize scene's views.
 * Check visibility, sort translucent items, etc.
 */
void FClusteredForwardShadingSceneRenderer::InitViews(FRHICommandListImmediate& RHICmdList)
{
	SCOPED_DRAW_EVENT(RHICmdList, InitViews);
	SCOPE_CYCLE_COUNTER(STAT_InitViewsTime);

	FILCUpdatePrimTaskData ILCTaskData;
	PreVisibilityFrameSetup(RHICmdList);
	ComputeViewVisibility(RHICmdList);

	// We don't want to support separate translucency, so move all the separate 
	// translucency objects into the normal translucency list, before they get sorted.
	// We could in theory also render them all after so they get "composited" on later
	// and better match the deferred results?
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		Views[ViewIndex].TranslucentPrimSet.MoveSeparateTranslucencyToSorted();
	}

	PostVisibilityFrameSetup(ILCTaskData);

	bool bDynamicShadows = ViewFamily.EngineShowFlags.DynamicShadows && GetShadowQuality() > 0;
	if (bDynamicShadows && !IsSimpleDynamicLightingEnabled())
	{
		// Setup dynamic shadows.
		InitDynamicShadows(RHICmdList);		
	}

	InitClusteredLightInfo(RHICmdList);

	// Now that the indirect lighting cache is updated, we can update the primitive precomputed lighting buffers.
	UpdatePrimitivePrecomputedLightingBuffers();

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
		FClusteredLightsSceneInfo* LightInfo = bHasAnyLights ? &ClusteredLightInfo[ViewIndex] : nullptr;
		Views[ViewIndex].InitRHIResources(DirectionalLightShadowInfo, LightInfo);
	}

	OnStartFrame();
}

static bool NeedsPrePass(FClusteredForwardShadingSceneRenderer* Renderer)
{
	extern int32 GEarlyZPassMovable;
	return Renderer->EarlyZPassMode != DDM_None || GEarlyZPassMovable != 0;
}

/** 
* Renders the view family. 
*/
void FClusteredForwardShadingSceneRenderer::Render(FRHICommandListImmediate& RHICmdList)
{
	bool const bSupportVelocityRendering = false;

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FClusteredForwardShadingSceneRenderer_Render);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	//make sure all the targets we're going to use will be safely writable.
	GRenderTargetPool.TransitionTargetsWritable(RHICmdList);

	// this way we make sure the SceneColor format is the correct one and not the one from the end of frame before
	SceneContext.ReleaseSceneColor();

	if (!ViewFamily.EngineShowFlags.Rendering)
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, Scene);

	// Initialization
	{
		// Initialize global system textures (pass-through if already initialized).
		GSystemTextures.InitializeTextures(RHICmdList, ViewFamily.GetFeatureLevel());

		// Allocate the maximum scene render target space for the current view family.
		SceneContext.Allocate(RHICmdList, Views.Num(), ViewFamily, ShadingPath);
	}

	// Find the visible primitives.
	InitViews(RHICmdList);

	// Build our light grid (compute)
	InjectLightsIntoLightGrid(RHICmdList);

	// Sort all the base pass buckets if necessary
	SortStateBuckets();

	// Dynamic vertex and index buffers need to be committed before rendering.
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FClusteredForwardShadingSceneRenderer_FGlobalDynamicVertexBuffer_Commit);
		FGlobalDynamicVertexBuffer::Get().Commit();
		FGlobalDynamicIndexBuffer::Get().Commit();
	}

	if (bSupportVelocityRendering)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FClusteredForwardShadingSceneRenderer_MotionBlurStartFrame);
		Scene->MotionBlurInfoData.StartFrame(ViewFamily.bWorldIsPaused);
	}

	// Notify the FX system that the scene is about to be rendered.
	if (Scene->FXSystem && Views.IsValidIndex(0))
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FClusteredForwardShadingSceneRenderer_FXSystem_PreRender);
		Scene->FXSystem->PreRender(RHICmdList, &Views[0].GlobalDistanceFieldInfo.ParameterData);
	}

	// Shadow zbuffers
	RenderShadowDepthMaps(RHICmdList);

	bool const bHasBackgroundLayer = RenderBackgroundLayer(RHICmdList);

	GRenderTargetPool.AddPhaseEvent(TEXT("EarlyZPass"));

	// Draw the scene pre-pass / early z pass, populating the scene depth buffer and HiZ
	bool bDepthWasCleared = RenderPrePassHMD(RHICmdList);;
	const bool bNeedsPrePass = NeedsPrePass(this);
	if (bNeedsPrePass)
	{
		RenderPrePass(RHICmdList, SDPG_World, bDepthWasCleared);
		// at this point, the depth was cleared
		bDepthWasCleared = true;
	}

	ESimpleRenderTargetMode TargetClearMode = 
		bDepthWasCleared ?
		ESimpleRenderTargetMode::EClearColorExistingDepth :
		ESimpleRenderTargetMode::EClearColorAndDepth;

	const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;
	#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
		const bool bIsViewFrozen = false;
		const bool bHasViewParent = false;
	#else
		const bool bIsViewFrozen = Views[0].State && ((FSceneViewState*)Views[0].State)->bIsFrozen;
		const bool bHasViewParent = Views[0].State && ((FSceneViewState*)Views[0].State)->HasViewParent();
	#endif

	const bool bIsOcclusionTesting = DoOcclusionQueries(FeatureLevel) && (!bIsWireframe || bIsViewFrozen || bHasViewParent);
	static const auto ICVarLocation = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.OcclusionQueryLocation"));
	bool const bOcclusionBeforeBasePass = (ICVarLocation->GetValueOnRenderThread() == 1) && bNeedsPrePass;
	bool const bOcclusionAfterBasePass = bIsOcclusionTesting && !bOcclusionBeforeBasePass;
	bool const bHZBBeforeBasePass = false;
	bool bNeedsShaderClear = false;

	RenderOcclusion(RHICmdList, bOcclusionBeforeBasePass, bHZBBeforeBasePass);

	// Begin rendering to scene color.  Modify some behavior if we have content and can skip a clear.
	// Normally the background color will be black and we can skip the clear, but in some editors it
	// is darkgrey and we need to clear manually.
	if (!bHasBackgroundLayer)
	{
		bNeedsShaderClear = SceneContext.GetSceneColorSurface()->GetClearColor() != Views[0].BackgroundColor;
	}

	if (bHasBackgroundLayer || bNeedsShaderClear)
	{
		if (TargetClearMode == ESimpleRenderTargetMode::EClearColorExistingDepth)
		{
			TargetClearMode = ESimpleRenderTargetMode::EExistingColorAndDepth;
		}
		else if (TargetClearMode == ESimpleRenderTargetMode::EClearColorAndDepth)
		{
			TargetClearMode = ESimpleRenderTargetMode::EUninitializedColorClearDepth;
		}
		else
		{
			check(0);
		}
	}

	SceneContext.BeginRenderingSceneColor(RHICmdList, TargetClearMode);

	if (bHasBackgroundLayer)
	{
		// Blit the background layer to the views instead of clearing.
		BlitBackgroundToViews(RHICmdList);
	}
	else if (bNeedsShaderClear)
	{
		RHICmdList.Clear(true, Views[0].BackgroundColor, false, 0.f, false, 0, FIntRect());
	}

	GRenderTargetPool.AddPhaseEvent(TEXT("BasePass"));
	RenderForwardShadingBasePass(RHICmdList, SDPG_World);

	// If we need the scene depth texture, resolve it
	// Oculus forward TODO(s):
	//  - Ideally we'd know if any of the shaders use "SceneDepth" (like a DepthFade/SceneDepth node), and thus we need the resolve.
	//  - Doing this in hardware instead of the shader resolve might be faster, need to figure out how to fix all the format mess.
	{
		SceneContext.ResolveSceneDepthTexture(RHICmdList);
		SceneContext.ResolveSceneDepthToAuxiliaryTexture(RHICmdList);
	}

	RenderOcclusion(RHICmdList, bOcclusionAfterBasePass, !bHZBBeforeBasePass);

	// Notify the FX system that opaque primitives have been rendered.
	if (Scene->FXSystem)
	{
		Scene->FXSystem->PostRenderOpaque(
			RHICmdList,
			Views.GetData(),
			SceneContext.GetSceneDepthTexture(),
			FTexture2DRHIParamRef());
	}

	// Custom depth pass, if applicable.
	RenderCustomDepthPass(RHICmdList);

	// Velocities, if we are supporting it
	TRefCountPtr<IPooledRenderTarget> VelocityRT;
	if (bSupportVelocityRendering)
	{
		// Check if we actually need velocities (TAA, etc)
		//if (bShouldRenderVelocities)
		{
			//RenderVelocities(RHICmdList, VelocityRT);
		}
	}

	// Draw translucency.
	if (ViewFamily.EngineShowFlags.Translucency)
	{
		#if CLUSTERED_SUPPORTS_TRANSLUCENT_VOLUME
		extern int32 GUseTranslucentLightingVolumes;
		// Update translucency lighting
		if (ViewFamily.EngineShowFlags.Lighting
			&& FeatureLevel >= ERHIFeatureLevel::SM4
			&& ViewFamily.EngineShowFlags.DeferredLighting
			&& GUseTranslucentLightingVolumes
			&& GSupportsVolumeTextureRendering)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_TranslucencyLighting);

			GRenderTargetPool.AddPhaseEvent(TEXT("TranslucentVolume"));

			// Clear the translucent lighting volumes before we accumulate
			ClearTranslucentVolumeLighting(RHICmdList);

			bool bRenderedAnyLights = InjectLightsIntoTranslucentVolume(RHICmdList);

			// Filter the translucency lighting volume now that it is complete
			FilterTranslucentVolumeLighting(RHICmdList);
		}
		#endif /* CLUSTERED_SUPPORTS_TRANSLUCENT_VOLUME */

		GRenderTargetPool.AddPhaseEvent(TEXT("Translucency"));

		SCOPE_CYCLE_COUNTER(STAT_TranslucencyDrawTime);
		SCOPED_DRAW_EVENT(RHICmdList, Translucency);

		// NOTE: in forward, we render the distortion pass in a different order.  See RenderForwardDistortion
		RenderTranslucency(RHICmdList, SDPG_World);
	}

	/* Render foreground passes! */
	{
		SCOPED_DRAW_EVENT(RHICmdList, Background);

		GRenderTargetPool.AddPhaseEvent(TEXT("Foreground"));

		RenderForwardShadingBasePass(RHICmdList, SDPG_Foreground);
		if (ViewFamily.EngineShowFlags.Translucency)
		{
			RenderTranslucency(RHICmdList, SDPG_Foreground);
		}
	}

	// Resolve the scene color for post processing.
	SceneContext.ResolveSceneColor(RHICmdList, FResolveRect(0, 0, ViewFamily.FamilySizeX, ViewFamily.FamilySizeY));

	// Distortion is rendered in a different order from deferred: we do this so we don't need
	// to resolve the scene twice, or do anything fancy with sampling the MSAA buffer pre-resolve.
	if (ViewFamily.EngineShowFlags.Translucency && ViewFamily.EngineShowFlags.Refraction)
	{
		RenderForwardDistortion(RHICmdList);
	}

	// Finish rendering for each view, or the full stereo buffer if enabled
	if (ViewFamily.bResolveScene)
	{
		SCOPED_DRAW_EVENT(RHICmdList, PostProcessing);
		SCOPE_CYCLE_COUNTER(STAT_FinishRenderViewTargetTime);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
			TRefCountPtr<IPooledRenderTarget> VelocityRT;
			GPostProcessing.Process(RHICmdList, Views[ViewIndex], VelocityRT, ShadingPath);
		}
	}

	//grab the new transform out of the proxies for next frame
	if (VelocityRT)
	{
		Scene->MotionBlurInfoData.UpdateMotionBlurCache(Scene);
		VelocityRT.SafeRelease();
	}


	RenderFinish(RHICmdList);
}

bool FClusteredForwardShadingSceneRenderer::RenderBackgroundLayer(FRHICommandListImmediate& RHICmdList)
{
	static const auto* CVarEnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.BackgroundLayerEnabled"));
	if (CVarEnabled != nullptr && !CVarEnabled->GetValueOnRenderThread())
	{
		return false;
	}

	SCOPED_DRAW_EVENT(RHICmdList, Background);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	static const auto* CVarSP = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.BackgroundLayerSP"));
	int32 const BackgroundSP = CVarSP->GetValueOnRenderThread();
	FIntPoint BackgroundBufferSize = FIntPoint(0, 0);
	for (const FViewInfo& View : Views)
	{
		FIntPoint const ViewSize = View.ViewRect.Size();
		BackgroundBufferSize.X = FMath::Max(BackgroundBufferSize.X, (ViewSize.X * BackgroundSP) / 100);
		BackgroundBufferSize.Y = FMath::Max(BackgroundBufferSize.Y, (ViewSize.Y * BackgroundSP) / 100);
	}

	// Create a new view for the background rendering.
	BackgroundView = Views[0].CreateSnapshot();
	FViewInfo& View = *BackgroundView;

	View.ViewRect.Min = FIntPoint(0, 0);
	View.ViewRect.Max = BackgroundBufferSize;

	if (View.StereoPass != eSSP_FULL)
	{
		check(Views.Num() == 2
			&& View.StereoPass == eSSP_LEFT_EYE
			&& Views[1].StereoPass == eSSP_RIGHT_EYE);

		MergeVisibleBatches(View, Views[1], SDPG_Background);

		// Override the view and projection matrices with the "union eye"
		View.StereoPass = eSSP_FULL;
		View.ViewLocation = View.UnionEyeViewLocation;
		View.ViewMatrices.ProjMatrix = AdjustProjectionMatrixForRHI(View.UnionEyeProjection);
		View.UpdateViewMatrix();
	}

	auto& ViewMatrices = View.ViewMatrices;
	FMatrix TranslatedViewMatrix = FTranslationMatrix(-View.ViewMatrices.PreViewTranslation) * ViewMatrices.ViewMatrix;
	FMatrix InvTranslatedViewMatrix = View.InvViewMatrix * FTranslationMatrix(ViewMatrices.PreViewTranslation);

	FBox VolumeBounds[TVC_MAX];
	View.CreateUniformBuffer(
		View.ViewUniformBuffer, 
		View.FrameUniformBuffer,
		RHICmdList,
		nullptr,				// directional light info
		nullptr,				// clustered shading light info (Oculus forward TODO: support lighting in the background layer)
		TranslatedViewMatrix,
		InvTranslatedViewMatrix,
		VolumeBounds,
		ARRAY_COUNT(VolumeBounds));

	SceneContext.BeginRenderingBackgroundPass(RHICmdList, ESimpleRenderTargetMode::EClearColorAndDepth, FExclusiveDepthStencil::DepthWrite_StencilWrite);

	// Check if we need to clear to a different clear color
	FTexture2DRHIRef BackgroundColorSurface = SceneContext.GetBackgroundSceneColorSurface();
	if (View.BackgroundColor != BackgroundColorSurface->GetClearColor())
	{
		RHICmdList.Clear(true, View.BackgroundColor, false, 0.f, false, 0, FIntRect());
	}

	bool bDirty = false;
	// Bother with a Zpass?
	//bDirty |= RenderPrePassView(RHICmdList, Views[0], SDPG_Background);
	bDirty |= RenderForwardShadingBasePassView(RHICmdList, View, SDPG_Background);
	bDirty |= RenderTranslucencyView(RHICmdList, View, SDPG_Background);

	if (bDirty)
	{
		SceneContext.FinishRenderingBackgroundPass(RHICmdList);
	}

	return bDirty;
}

void FClusteredForwardShadingSceneRenderer::MergeVisibleBatches(FViewInfo& Out, const FViewInfo& In, ESceneDepthPriorityGroup DepthPriorityGroup)
{
	// Merge dynamic mesh elements: pretty straightfoward
	TSet<const FPrimitiveSceneProxy*> ProxySet;
	for (const FMeshBatchAndRelevance& Batch : Out.DynamicMeshElements)
	{
		if (Batch.DepthPriorityGroup == DepthPriorityGroup)
		{
			ProxySet.Add(Batch.PrimitiveSceneProxy);
		}
	}

	for (const FMeshBatchAndRelevance& Batch : In.DynamicMeshElements)
	{
		if (Batch.DepthPriorityGroup == DepthPriorityGroup &&
			!ProxySet.Contains(Batch.PrimitiveSceneProxy))
		{
			ProxySet.Add(Batch.PrimitiveSceneProxy);
			Out.DynamicMeshElements.Add(Batch);
		}
	}

	// Static mesh elements is a little bit trickier.
	// We need to examine the visibility bit for the mesh, and then all the individual elements
	check(In.StaticMeshVisibilityMap.Num() == Out.StaticMeshVisibilityMap.Num());

	FSceneBitArray::FConstIterator InIterator(In.StaticMeshVisibilityMap);
	FSceneBitArray::FIterator OutIterator(Out.StaticMeshVisibilityMap);

	while (InIterator)
	{
		bool inVisible = InIterator.GetValue();
		bool outVisible = OutIterator.GetValue();
		if (inVisible && !outVisible)
		{
			FStaticMesh* StaticMesh = Scene->StaticMeshes[InIterator.GetIndex()];
			if (StaticMesh->DepthPriorityGroup == DepthPriorityGroup)
			{
				Out.StaticMeshVisibilityMap.AccessCorrespondingBit(OutIterator) = true;
				if (StaticMesh->Elements.Num() > 1)
				{
					Out.StaticMeshBatchVisibility[StaticMesh->Id] = In.StaticMeshBatchVisibility[StaticMesh->Id];
				}
			}
		}
		else if (inVisible && outVisible)
		{
			// Still need to merge the batch visibility
			Out.StaticMeshBatchVisibility[InIterator.GetIndex()] |= In.StaticMeshBatchVisibility[InIterator.GetIndex()];
		}

		++InIterator;
		++OutIterator;
	}
}

void FClusteredForwardShadingSceneRenderer::BlitBackgroundToViews(FRHICommandListImmediate& RHICmdList)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	auto BackgroundTex = SceneContext.GetBackgroundSceneColorTexture();
	FIntPoint const BackgroundTexSize = FIntPoint(BackgroundTex->GetSizeX(), BackgroundTex->GetSizeY());
	FIntPoint const BackgroundSize = BackgroundView->ViewRect.Size();
	FMatrix const BackgroundViewProj = BackgroundView->ViewMatrices.GetViewProjMatrix();

	// points "at the far plane".  since we use an infinite projection, can't use 0.
	// also note the unreal's projection is inverted, so 1=near
	const float farZ = .0001;
	FVector4 const ndcCorners[] ={
		FVector4(-1, -1, farZ, 1),
		FVector4(+1, +1, farZ, 1),
	};

	RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	// We've already rendered a zpass, and are going to blit at far z -- so use this to avoid
	// blitting pixels that are behind stuff.
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());

	auto ShaderMap = GetGlobalShaderMap(ViewFamily.GetFeatureLevel());
	TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
	TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

	for (const FViewInfo& View : Views)
	{
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1);

		// Determine the SrcRect we want to copy out our eye's coverage of the background.
		FVector2D texCorners[2];
		FMatrix InvViewProj = View.ViewMatrices.GetInvViewProjMatrix();
		for (unsigned i = 0; i < 2; ++i)
		{
			FVector4 FarCorner = InvViewProj.TransformFVector4(ndcCorners[i]);
			FarCorner = FarCorner / FarCorner.W;

			FVector4 Proj = BackgroundViewProj.TransformFVector4(FarCorner);
			texCorners[i].X = (Proj.X / Proj.W) * .5f + .5f;
			texCorners[i].Y = (Proj.Y / Proj.W) * .5f + .5f;
		}

		FBox2D SrcRect;
		SrcRect.Min = texCorners[0] * BackgroundSize;
		SrcRect.Max = texCorners[1] * BackgroundSize;

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState(RHICmdList, ViewFamily.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
		PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), BackgroundTex);

		DrawRectangle(
			RHICmdList,
			0, 0,
			View.ViewRect.Width(), View.ViewRect.Height(),
			SrcRect.Min.X,
			SrcRect.Min.Y,
			(SrcRect.Max.X - SrcRect.Min.X),
			(SrcRect.Max.Y - SrcRect.Min.Y),
			View.ViewRect.Size(),
			BackgroundTexSize,
			*VertexShader,
			EDRF_Default);
	}
}

/** Renders the scene's prepass and occlusion queries */
bool FClusteredForwardShadingSceneRenderer::RenderPrePass(FRHICommandListImmediate& RHICmdList, ESceneDepthPriorityGroup DepthPriorityGroup, bool bDepthWasCleared)
{
	SCOPED_DRAW_EVENT(RHICmdList, PrePass);
	SCOPE_CYCLE_COUNTER(STAT_DepthDrawTime);

	bool bDirty = false;

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	SceneContext.BeginRenderingPrePass(RHICmdList, !bDepthWasCleared);

	// Draw a depth pass to avoid overdraw in the other passes.
	if(EarlyZPassMode != DDM_None)
	{
		/*
		if (GRHICommandList.UseParallelAlgorithms() && CVarParallelPrePass.GetValueOnRenderThread())
		{
			FScopedCommandListWaitForTasks Flusher(RHICmdList);

			for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
			{
				SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
				const FViewInfo& View = Views[ViewIndex];
				if (View.ShouldRenderView())
				{
					RenderPrePassViewParallel(View, RHICmdList, bDirty);
				}
			}
		}
		else
		*/
		{
			for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
			{
				SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
				const FViewInfo& View = Views[ViewIndex];
				if (View.ShouldRenderView())
				{
					bDirty |= RenderPrePassView(RHICmdList, View, DepthPriorityGroup);
				}
			}
		}
	}

	SceneContext.FinishRenderingPrePass(RHICmdList);

	return bDirty;
}

bool FClusteredForwardShadingSceneRenderer::RenderPrePassView(FRHICommandList& RHICmdList, const FViewInfo& View, ESceneDepthPriorityGroup DPG)
{
	bool bDirty = false;

	SetupPrePassView(RHICmdList, View);

	// Draw the static occluder primitives using a depth drawing policy.
	if (!View.IsInstancedStereoPass())
	{
		{
			// Draw opaque occluders which support a separate position-only
			// vertex buffer to minimize vertex fetch bandwidth, which is
			// often the bottleneck during the depth only pass.
			SCOPED_DRAW_EVENT(RHICmdList, PosOnlyOpaque);
			bDirty |= Scene->PositionOnlyDepthDrawList.DrawVisible(RHICmdList, DPG, View, View.StaticMeshOccluderMap, View.StaticMeshBatchVisibility);
		}
		{
			// Draw opaque occluders, using double speed z where supported.
			SCOPED_DRAW_EVENT(RHICmdList, Opaque);
			bDirty |= Scene->DepthDrawList.DrawVisible(RHICmdList, DPG, View, View.StaticMeshOccluderMap, View.StaticMeshBatchVisibility);
		}

		if (EarlyZPassMode >= DDM_AllOccluders)
		{
			// Draw opaque occluders with masked materials
			SCOPED_DRAW_EVENT(RHICmdList, Opaque);
			bDirty |= Scene->MaskedDepthDrawList.DrawVisible(RHICmdList, DPG, View, View.StaticMeshOccluderMap, View.StaticMeshBatchVisibility);
		}
	}
	else
	{
		const StereoPair StereoView(Views[0], Views[1], Views[0].StaticMeshOccluderMap, Views[1].StaticMeshOccluderMap, Views[0].StaticMeshBatchVisibility, Views[1].StaticMeshBatchVisibility);
		{
			SCOPED_DRAW_EVENT(RHICmdList, PosOnlyOpaque);
			bDirty |= Scene->PositionOnlyDepthDrawList.DrawVisibleInstancedStereo(RHICmdList, DPG, StereoView);
		}
		{
			SCOPED_DRAW_EVENT(RHICmdList, Opaque);
			bDirty |= Scene->DepthDrawList.DrawVisibleInstancedStereo(RHICmdList, DPG, StereoView);
		}

		if (EarlyZPassMode >= DDM_AllOccluders)
		{
			SCOPED_DRAW_EVENT(RHICmdList, Opaque);
			bDirty |= Scene->MaskedDepthDrawList.DrawVisibleInstancedStereo(RHICmdList, DPG, StereoView);
		}
	}

	bDirty |= RenderPrePassViewDynamic(RHICmdList, View, DPG);
	return bDirty;
}

bool FClusteredForwardShadingSceneRenderer::RenderPrePassViewDynamic(FRHICommandList& RHICmdList, const FViewInfo& View, ESceneDepthPriorityGroup DepthPriorityGroup)
{
	extern int32 GEarlyZPassMovable;

	FDepthDrawingPolicyFactory::ContextType Context(EarlyZPassMode);

	for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.DynamicMeshElements.Num(); MeshBatchIndex++)
	{
		const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

		if (MeshBatchAndRelevance.bHasOpaqueOrMaskedMaterial 
			&& MeshBatchAndRelevance.bRenderInMainPass
			&& MeshBatchAndRelevance.DepthPriorityGroup == DepthPriorityGroup)
		{
			const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
			const FPrimitiveSceneProxy* PrimitiveSceneProxy = MeshBatchAndRelevance.PrimitiveSceneProxy;
			bool bShouldUseAsOccluder = true;

			if (EarlyZPassMode < DDM_AllOccluders)
			{
				extern float GMinScreenRadiusForDepthPrepass;
				//@todo - move these proxy properties into FMeshBatchAndRelevance so we don't have to dereference the proxy in order to reject a mesh
				const float LODFactorDistanceSquared = (PrimitiveSceneProxy->GetBounds().Origin - View.ViewMatrices.ViewOrigin).SizeSquared() * FMath::Square(View.LODDistanceFactor);

				// Only render primitives marked as occluders
				bShouldUseAsOccluder = PrimitiveSceneProxy->ShouldUseAsOccluder()
					// Only render static objects unless movable are requested
					&& (!PrimitiveSceneProxy->IsMovable() || GEarlyZPassMovable)
					&& (FMath::Square(PrimitiveSceneProxy->GetBounds().SphereRadius) > GMinScreenRadiusForDepthPrepass * GMinScreenRadiusForDepthPrepass * LODFactorDistanceSquared);
			}

			if (bShouldUseAsOccluder)
			{
				FDepthDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, false, true, PrimitiveSceneProxy, MeshBatch.BatchHitProxyId, View.IsInstancedStereoPass());
			}
		}
	}

	return true;
}

FGraphEventRef FClusteredForwardShadingSceneRenderer::OcclusionSubmittedFence[FOcclusionQueryHelpers::MaxBufferedOcclusionFrames];

void FClusteredForwardShadingSceneRenderer::RenderOcclusion(FRHICommandListImmediate& RHICmdList, bool bRenderQueries, bool bRenderHZB)
{		
	if (bRenderQueries || bRenderHZB)
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		check(!SceneContext.UseDownsizedOcclusionQueries());

		/*
		{
			// Update the quarter-sized depth buffer with the current contents of the scene depth texture.
			// This needs to happen before occlusion tests, which makes use of the small depth buffer.
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_UpdateDownsampledDepthSurface);
			UpdateDownsampledDepthSurface(RHICmdList);
		}
		*/
		
		if (bRenderHZB)
		{
			static const auto ICVarAO		= IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AmbientOcclusionLevels"));
			static const auto ICVarHZBOcc	= IConsoleManager::Get().FindConsoleVariable(TEXT("r.HZBOcclusion"));
			bool bSSAO						= ICVarAO->GetValueOnRenderThread() != 0;
			bool bHzbOcclusion				= ICVarHZBOcc->GetInt() != 0;
			bool bNeedHZB					= false;

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				const uint32 bSSR = false; // DoScreenSpaceReflections(Views[ViewIndex]);

				if (bSSAO || bHzbOcclusion || bSSR)
				{
					bNeedHZB = true;
					//BuildHZB(RHICmdList, Views[ViewIndex]);
				}
			}

			if (!bNeedHZB)
			{
				bRenderHZB = false;
			}
		}

		// Issue occlusion queries
		// This is done after the downsampled depth buffer is created so that it can be used for issuing queries
		BeginOcclusionTests(RHICmdList, bRenderQueries, bRenderHZB);


		// Hint to the RHI to submit commands up to this point to the GPU if possible.  Can help avoid CPU stalls next frame waiting
		// for these query results on some platforms.
		RHICmdList.SubmitCommandsHint();

		if (bRenderQueries && GRHIThread)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_OcclusionSubmittedFence_Dispatch);
			int32 NumFrames = FOcclusionQueryHelpers::GetNumBufferedFrames();
			for (int32 Dest = 1; Dest < NumFrames; Dest++)
			{
				OcclusionSubmittedFence[Dest] = OcclusionSubmittedFence[Dest - 1];
			}
			OcclusionSubmittedFence[0] = RHICmdList.RHIThreadFence();
		}
	}
}


bool FClusteredForwardShadingSceneRenderer::InjectLightsIntoTranslucentVolume(FRHICommandListImmediate& RHICmdList)
{
	extern int32 GUseTranslucentLightingVolumes;
	if (!ViewFamily.EngineShowFlags.DirectLighting ||
		!(GUseTranslucentLightingVolumes && GSupportsVolumeTextureRendering) ||
		!CLUSTERED_SUPPORTS_TRANSLUCENT_VOLUME)
	{
		return false;
	}

	TArray<FSortedLightSceneInfo, SceneRenderingAllocator> SortedLights;
	SortedLights.Empty(Scene->Lights.Num());

	//bool const bDynamicShadows = ViewFamily.EngineShowFlags.DynamicShadows && GetShadowQuality() > 0;
	bool const bDynamicShadows = false;

	// Build a list of visible lights.
	for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

		if (LightSceneInfo == Scene->SimpleDirectionalLight
			&& CLUSTERED_SUPPORTS_TRANSLUCENCY_LIGHTING_DIRECTIONAL_LIGHT)
		{
			continue;
		}

		if (LightSceneInfo->ShouldRenderLightViewIndependent()
			// Reflection override skips direct specular because it tends to be blindingly bright with a perfectly smooth surface
			&& !ViewFamily.EngineShowFlags.ReflectionOverride)
		{
			// Check if the light is visible in any of the views.
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if (LightSceneInfo->ShouldRenderLight(Views[ViewIndex]))
				{
					FSortedLightSceneInfo* SortedLightInfo = new(SortedLights)FSortedLightSceneInfo(LightSceneInfoCompact);

					// Check for shadows and light functions.
					SortedLightInfo->SortKey.Fields.LightType = LightSceneInfoCompact.LightType;
					SortedLightInfo->SortKey.Fields.bTextureProfile = ViewFamily.EngineShowFlags.TexturedLightProfiles && LightSceneInfo->Proxy->GetIESTextureResource();
					SortedLightInfo->SortKey.Fields.bShadowed = bDynamicShadows && CheckForProjectedShadows(LightSceneInfo);
					SortedLightInfo->SortKey.Fields.bLightFunction = false;
					//SortedLightInfo->SortKey.Fields.bLightFunction = ViewFamily.EngineShowFlags.LightFunctions && CheckForLightFunction(LightSceneInfo);
					break;
				}
			}
		}
	}

	if (SortedLights.Num())
	{
		// Don't actually need to sort this for any reason.
		struct FCompareFSortedLightSceneInfo
		{
			FORCEINLINE bool operator()(const FSortedLightSceneInfo& A, const FSortedLightSceneInfo& B) const
			{
				return A.SortKey.Packed < B.SortKey.Packed;
			}
		};
		SortedLights.Sort(FCompareFSortedLightSceneInfo());

		// Inject them all without shadows, light functions, etc.
		InjectTranslucentVolumeLightingArray(RHICmdList, SortedLights, SortedLights.Num());
	}

	return SortedLights.Num() != 0;
}

// Perform simple upscale and/or editor primitive composite if the fully-featured post process is not in use.
void FClusteredForwardShadingSceneRenderer::BasicPostProcess(FRHICommandListImmediate& RHICmdList, FViewInfo &View)
{
	FRenderingCompositePassContext CompositeContext(RHICmdList, View);
	FPostprocessContext Context(RHICmdList, CompositeContext.Graph, View);

	// Composite editor primitives if we had any to draw and compositing is enabled
#if WITH_EDITOR
	if (ShouldCompositeEditorPrimitives(View))
	{
		FRenderingCompositePass* EditorCompNode = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessCompositeEditorPrimitives(ShadingPath));
		EditorCompNode->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
		//Node->SetInput(ePId_Input1, FRenderingCompositeOutputRef(Context.SceneDepth));
		Context.FinalOutput = FRenderingCompositeOutputRef(EditorCompNode);
	}
#endif

	// currently created on the heap each frame but View.Family->RenderTarget could keep this object and all would be cleaner
	TRefCountPtr<IPooledRenderTarget> Temp;
	FSceneRenderTargetItem Item;
	Item.TargetableTexture = (FTextureRHIRef&)View.Family->RenderTarget->GetRenderTargetTexture();
	Item.ShaderResourceTexture = (FTextureRHIRef&)View.Family->RenderTarget->GetRenderTargetTexture();

	FPooledRenderTargetDesc Desc;

	Desc.Extent = View.Family->RenderTarget->GetSizeXY();
	// todo: this should come from View.Family->RenderTarget
	Desc.Format = PF_B8G8R8A8;
	Desc.NumMips = 1;

	GRenderTargetPool.CreateUntrackedElement(Desc, Temp, Item);

	Context.FinalOutput.GetOutput()->PooledRenderTarget = Temp;
	Context.FinalOutput.GetOutput()->RenderTargetDesc = Desc;

	CompositeContext.Process(Context.FinalOutput.GetPass(), TEXT("ES2BasicPostProcess"));
}

FVector FClusteredForwardShadingSceneRenderer::GetLightGridZParams(float NearPlane, float FarPlane)
{
	// Old settings, with m->uu applied.  Might want to update.

	// S = distribution scale
	// B, O are solved for given the z distances of the first+last slice, and the # of slices.
	//
	// slice = log2(z*B + O) * S

	// Don't spent lots of resolution right in front of the near plane
	double NearOffset = .095 * 100;
	// Space out the slices so they aren't all clustered at the near plane
	double S = 4.05;

	double N = NearPlane + NearOffset;
	double F = FarPlane;

	double O = (F - N * exp2((kLightGridSlicesZ-1)/S)) / (F - N);
	double B = (1 - O) / N;

	return FVector(B, O, S);
}

/* static */ FIntVector FClusteredForwardShadingSceneRenderer::CalcLightGridSize(const FIntPoint& ViewportSize)
{
	return FIntVector(
		(ViewportSize.X + kLightGridTileSizeX-1) / kLightGridTileSizeX,
		(ViewportSize.Y + kLightGridTileSizeY-1) / kLightGridTileSizeY,
		kLightGridSlicesZ
		);
}

void FClusteredForwardShadingSceneRenderer::InitClusteredLightInfo(FRHICommandListImmediate& RHICmdList)
{
	check(FMath::IsPowerOfTwo(kLightGridTileSizeX) &&
		FMath::IsPowerOfTwo(kLightGridTileSizeY) &&
		FMath::IsPowerOfTwo(kLightGridSlicesZ));

	bHasAnyLights = false;
	ClusteredLightInfo.AddDefaulted(Views.Num());
	if (ClusteredLightInfo.Num() == 0)
	{
		return;
	}

	const FScene* Scene = (FScene*)Views[0].Family->Scene;
	if (Scene == nullptr)
	{
		return;
	}

	// In instanced stereo, the list of lights must be identical between both eyes.
	bool const bInstancedStereo = Views.Num() == 2 && !Views[1].ShouldRenderView();
	TBitArray<FDefaultBitArrayAllocator> bLightIncluded;
	bLightIncluded.Init(false, Scene->Lights.Num());

	// Initialize the ClusteredForwardLight arrays
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FSceneView& View = Views[ViewIndex];
		FClusteredLightsSceneInfo& Info = bInstancedStereo ? ClusteredLightInfo[0] : ClusteredLightInfo[ViewIndex];

		Info.LightGridTex = FSceneRenderTargets::Get(RHICmdList).GetClusteredLightGrid();
		Info.TileSize.X = kLightGridTileSizeX;
		Info.TileSize.Y = kLightGridTileSizeY;

		// Our goal isn't actually to encompass the whole view, we want to make sure we cover the
		// region that there are lights in the scene.  AND, we want to make sure that there is one
		// Z slice further than the furthest light, so all the geometry past that light doesn't
		// end up running the shader.
		// TODO: do this for the near plane as well (better culling)
		float NearPlane = View.NearClippingDistance;
		float FurthestLight = 1000;

		for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
		{
			if (Info.ClusteredLights.Num() >= MAX_CLUSTERED_FORWARD_LIGHTS)
			{
				break;
			}

			const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
			const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

			if (LightSceneInfoCompact.Color.IsAlmostBlack()
				// Omit the primary directional light
				|| LightSceneInfo == Scene->SimpleDirectionalLight
				// Only render lights with dynamic lighting or unbuilt static lights
				|| (LightSceneInfo->Proxy->HasStaticLighting() && LightSceneInfo->IsPrecomputedLightingValid())
				// Or invisible lights
				|| !LightSceneInfo->ShouldRenderLight(Views[ViewIndex]))
			{
				continue;
			}

			// If you want spotlights, make sure to enable them in the shaders as well.
			bool const bSpotLightsEnabled = false;

			ELightComponentType const LightType = (const ELightComponentType)LightSceneInfoCompact.LightType;
			bool const bSupportedType =
				(LightType == LightType_Point)
				|| (bSpotLightsEnabled && LightType == LightType_Spot)
				// We want to support LightType=Directional only in the editor.  We do this so we can use preview views
				// but disable the code in the shader for the game to save shader cost.
				|| (GIsEditor && LightType == LightType_Directional);

			if (!bSupportedType)
			{
				continue;
			}

			if (bInstancedStereo)
			{
				if (bLightIncluded[LightSceneInfo->Id])
				{
					// Don't add the light multiple times if visible in both views
					continue;
				}
				bLightIncluded[LightSceneInfo->Id] = true;
			}

			// Approximate.
			FSphere const BoundingSphere = LightSceneInfo->Proxy->GetBoundingSphere();
			float Distance = View.ViewMatrices.ViewMatrix.TransformPosition(BoundingSphere.Center).Z + BoundingSphere.W;
			FurthestLight = FMath::Max(FurthestLight, Distance);

			bHasAnyLights = true;
			Info.ClusteredLights.Add(LightSceneInfoCompact);

			// Sort them by type/features, for more coherency
			Info.ClusteredLights.Sort([](const FLightSceneInfoCompact& A, const FLightSceneInfoCompact& B) {
				if (A.LightType == B.LightType) {
					return A.bCastStaticShadow < B.bCastStaticShadow;
				}
				return A.LightType < B.LightType;
			});
		}

		float FarPlane = FurthestLight;
		FVector ZParams = GetLightGridZParams(NearPlane, FarPlane + 10.f);
		Info.LightGridZParams = FVector4(ZParams, ZParams.Z / kLightGridSlicesZ);
	}

	if (bInstancedStereo)
	{
		ClusteredLightInfo[1] = ClusteredLightInfo[0];
	}
}
