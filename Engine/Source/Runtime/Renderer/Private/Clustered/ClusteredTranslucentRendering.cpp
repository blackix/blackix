// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ForwardTranslucentRendering.cpp: translucent rendering implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "ScreenRendering.h"
#include "SceneFilterRendering.h"
#include "SceneUtils.h"
#include "SceneRenderTargets.h"
#include "DepthRendering.h"

#include "Clustered/ClusteredBasePassRendering.h"
#include "Clustered/ClusteredShadingRenderer.h"

#include "PostProcess/SceneRenderTargets.h"

/** The parameters used to draw a translucent mesh. */
class FDrawTranslucentMeshClusteredShadingAction
{
public:

	const FViewInfo& View;
	bool bBackFace;
	bool bNondirectionalLighting;
	FMeshDrawingRenderState DrawRenderState;
	FHitProxyId HitProxyId;

	/** Initialization constructor. */
	FDrawTranslucentMeshClusteredShadingAction(
		const FViewInfo& InView,
		bool bInBackFace,
		const FMeshDrawingRenderState& InDrawRenderState,
		ETranslucencyLightingMode InTranslucentLightingMode,
		FHitProxyId InHitProxyId
		):
		View(InView),
		bBackFace(bInBackFace),
		bNondirectionalLighting(InTranslucentLightingMode == ETranslucencyLightingMode::TLM_VolumetricNonDirectional),
		DrawRenderState(InDrawRenderState),
		HitProxyId(InHitProxyId)
	{}
	
	inline bool ShouldPackAmbientSH() const
	{
		// So shader code can read a single constant to get the ambient term
		return bNondirectionalLighting;
	}

	const FLightSceneInfo* GetSimpleDirectionalLight() const 
	{ 
		return ((FScene*)View.Family->Scene)->SimpleDirectionalLight;
	}

	const bool GetSimpleDirectionalLightHasCSM() const 
	{ 
		return ((FScene*)View.Family->Scene)->bSimpleDirectionalLightHasCSM;
	}

	// Oculus forward TODO
	//bool AllowIndirectLightingCache() const
	//bool AllowIndirectLightingCacheVolumeTexture() const

	/** Draws the translucent mesh with a specific light-map type, and fog volume type */
	template<typename LightMapPolicyType>
	void Process(
		FRHICommandList& RHICmdList, 
		const FProcessBasePassMeshParameters& Parameters,
		const LightMapPolicyType& LightMapPolicy,
		const typename LightMapPolicyType::ElementDataType& LightMapElementData
		) const
	{
		const bool bIsLitMaterial = Parameters.ShadingModel != MSM_Unlit;
		const FScene* Scene = Parameters.PrimitiveSceneProxy ? Parameters.PrimitiveSceneProxy->GetPrimitiveSceneInfo()->Scene : NULL;

		typename TBasePassForClusteredShadingDrawingPolicy<LightMapPolicyType>::ContextDataType PolicyContext;
		TBasePassForClusteredShadingDrawingPolicy<LightMapPolicyType> DrawingPolicy(
			Parameters.Mesh.VertexFactory,
			Parameters.Mesh.MaterialRenderProxy,
			*Parameters.Material,
			LightMapPolicy,
			Parameters.BlendMode,
			Parameters.TextureMode,
			bIsLitMaterial && Scene && Scene->ShouldRenderSkylight(),
			bIsLitMaterial && Scene && Scene->ShouldRenderReflectionProbe(),
			View.Family->EngineShowFlags.ShaderComplexity,
			View.GetFeatureLevel()
		);

		RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
		DrawingPolicy.SetSharedState(RHICmdList, &View, PolicyContext);

		int32 BatchElementIndex = 0;
		uint64 BatchElementMask = Parameters.BatchElementMask;
		do
		{
			if(BatchElementMask & 1)
			{
				TDrawEvent<FRHICommandList> MeshEvent;
				BeginMeshDrawEvent(RHICmdList, Parameters.PrimitiveSceneProxy, Parameters.Mesh, MeshEvent);

				DrawingPolicy.SetMeshRenderState(
					RHICmdList, 
					View,
					Parameters.PrimitiveSceneProxy,
					Parameters.Mesh,
					BatchElementIndex,
					bBackFace,
					DrawRenderState,
					typename TBasePassForClusteredShadingDrawingPolicy<LightMapPolicyType>::ElementDataType(LightMapElementData),
					PolicyContext
				);
				DrawingPolicy.DrawMesh(RHICmdList, Parameters.Mesh, BatchElementIndex);
			}

			BatchElementMask >>= 1;
			BatchElementIndex++;
		} while(BatchElementMask);
	}
};

/**
 * Render a dynamic mesh using a translucent draw policy
 * @return true if the mesh rendered
 */

bool FTranslucencyClusteredShadingDrawingPolicyFactory::DrawDynamicMesh(
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	ContextType DrawingContext,
	const FMeshBatch& Mesh,
	bool bBackFace,
	bool bPreFog,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId
	)
{
	return DrawMesh(
		RHICmdList,
		View,
		DrawingContext,
		Mesh,
		Mesh.Elements.Num() == 1 ? 1 : (1 << Mesh.Elements.Num()) - 1,	// 1 bit set for each mesh element
		FMeshDrawingRenderState(Mesh.DitheredLODTransitionAlpha),
		bBackFace,
		bPreFog,
		PrimitiveSceneProxy,
		HitProxyId
		);
}

bool FTranslucencyClusteredShadingDrawingPolicyFactory::DrawStaticMesh(
	FRHICommandList& RHICmdList, 
	const FViewInfo& View,
	ContextType DrawingContext,
	const FStaticMesh& StaticMesh,
	bool bPreFog,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId
	)
{
	return DrawMesh(
		RHICmdList, 
		View,
		DrawingContext,
		StaticMesh,
		StaticMesh.Elements.Num() == 1 ? 1 : (1 << StaticMesh.Elements.Num()) - 1,	// 1 bit set for each mesh element
		FMeshDrawingRenderState(StaticMesh.DitheredLODTransitionAlpha),
		false,		// backface
		bPreFog,
		PrimitiveSceneProxy,
		HitProxyId
	);
}


bool FTranslucencyClusteredShadingDrawingPolicyFactory::DrawMesh(
	FRHICommandList& RHICmdList, 
	const FViewInfo& View,
	ContextType DrawingContext,
	const FMeshBatch& Mesh,
	uint64 BatchElementMask,
	const FMeshDrawingRenderState& DrawRenderState,
	bool bBackFace,
	bool bPreFog,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId
	)
{
	bool bDirty = false;

	// Determine the mesh's material and blend mode.
	const auto FeatureLevel = View.GetFeatureLevel();
	const auto ShaderPlatform = View.GetShaderPlatform();
	const FMaterial* Material = Mesh.MaterialRenderProxy->GetMaterial(FeatureLevel);
	const EBlendMode BlendMode = Material->GetBlendMode();

	// Only render translucent materials.
	if (IsTranslucentBlendMode(BlendMode))
	{
		bool bRestoreDepthState = false;
		if (Material->ShouldPerformTranslucentDepthPrepass())
		{
			bRestoreDepthState = true;
			// No writes, check for equal
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Equal>::GetRHI());
		}
		else if (Material->ShouldDisableDepthTest())
		{
			bRestoreDepthState = true;
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
		}

		ProcessBasePassMeshForClusteredShading(
			RHICmdList,
			FProcessBasePassMeshParameters(
				Mesh,
				BatchElementMask,
				Material,
				PrimitiveSceneProxy,
				!bPreFog,	// allow fog
				false,		// editor composite depth test
				ESceneRenderTargetsMode::SetTextures,
				FeatureLevel
				),
			FDrawTranslucentMeshClusteredShadingAction(
				View,
				bBackFace,
				DrawRenderState,
				Material->GetTranslucencyLightingMode(),
				HitProxyId
				)
			);

		if (bRestoreDepthState)
		{
			// Restore default depth state
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
		}

		bDirty = true;
	}
	return bDirty;
}

bool FTranslucencyClusteredShadingDrawingPolicyFactory::DrawMeshPrepass(
    FRHICommandList& RHICmdList,
    const FViewInfo& View,
    ContextType DrawingContext,
    const FMeshBatch& Mesh,
    const FPrimitiveSceneProxy* PrimitiveSceneProxy)
{
    bool bDirty = false;

	// Determine the mesh's material and blend mode.
	const auto FeatureLevel = View.GetFeatureLevel();
	const auto ShaderPlatform = View.GetShaderPlatform();
	const FMaterial* Material = Mesh.MaterialRenderProxy->GetMaterial(FeatureLevel);

	if (Material->ShouldPerformTranslucentDepthPrepass() 
        && IsTranslucentBlendMode(Material->GetBlendMode()))
	{
        bDirty = true;
        FDepthDrawingPolicyFactory::DrawDynamicMesh(
            RHICmdList,
            View,
            FDepthDrawingPolicyFactory::ContextType(DDM_AllOpaque, true),
            Mesh,
            false,	// backface
            true,	// prefog
            PrimitiveSceneProxy,
            Mesh.BatchHitProxyId);
	}

    return bDirty;
}

/*-----------------------------------------------------------------------------
FTranslucentPrimSet
-----------------------------------------------------------------------------*/

bool FTranslucentPrimSet::DrawPrimitivesForClusteredShading(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, FSceneRenderer& Renderer, ESceneDepthPriorityGroup DepthPriorityGroup) const
{
	// Draw sorted scene prims
	bool bDirty = false;
	for (int32 PrimIdx = 0; PrimIdx < SortedPrims.Num(); PrimIdx++)
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo = SortedPrims[PrimIdx].PrimitiveSceneInfo;
		int32 PrimitiveId = PrimitiveSceneInfo->GetIndex();
		const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveId];

		checkSlow(ViewRelevance.HasTranslucency());

		if(ViewRelevance.bDrawRelevance)
		{
			FTranslucencyClusteredShadingDrawingPolicyFactory::ContextType Context;

			//@todo parallelrendering - come up with a better way to filter these by primitive
			for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.DynamicMeshElements.Num(); MeshBatchIndex++)
			{
				const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

				if (MeshBatchAndRelevance.PrimitiveSceneProxy == PrimitiveSceneInfo->Proxy
					&& MeshBatchAndRelevance.Mesh->DepthPriorityGroup == DepthPriorityGroup)
				{
					const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;

					// Render everything to depth before rendering the transparency?
					if (ViewRelevance.bUsesTranslucencyDepthPrepass)
					{
						// Disable color writes, enable depth tests and writes.
						//
						// Requires rebinding the rendertargets with depth writing enabled!  Argh!
						// We do something tricky: we know we don't read from the rendertarget if it is multisampled,
						// so we avoid switching targets if so.
						FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
						bool bNeedTargetSwitch = !SceneContext.GetSceneColorSurface()->IsMultisampled();
						if (bNeedTargetSwitch)
						{
							SceneContext.BeginRenderingPrePass(RHICmdList, false /* clear */);
						}

						RHICmdList.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());
						RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());

						if (MeshBatch.IsTranslucent(View.GetFeatureLevel()))
						{
							FTranslucencyClusteredShadingDrawingPolicyFactory::DrawMeshPrepass(
								RHICmdList,
								View,
								FTranslucencyClusteredShadingDrawingPolicyFactory::ContextType(),
								MeshBatch,
								PrimitiveSceneInfo->Proxy
								);
						}

						if (bNeedTargetSwitch)
						{
							SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);
						}
					}

					bDirty |= FTranslucencyClusteredShadingDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, false, false, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
				}
			}

			// Render static scene prim
			if( ViewRelevance.bStaticRelevance )
			{
				// Render everything to depth before rendering the transparency?
				if (ViewRelevance.bUsesTranslucencyDepthPrepass)
				{
					// Disable color writes, enable depth tests and writes.
					//
					// Requires rebinding the rendertargets with depth writing enabled!  Argh!
					// We do something tricky: we know we don't read from the rendertarget if it is multisampled,
					// so we avoid switching targets if so.
					FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
					bool bNeedTargetSwitch = !SceneContext.GetSceneColorSurface()->IsMultisampled();
					if (bNeedTargetSwitch)
					{
						SceneContext.BeginRenderingPrePass(RHICmdList, false /* clear */);
					}
					RHICmdList.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());
					RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
					// Render static meshes from static scene prim
					for (int32 StaticMeshIdx=0; StaticMeshIdx < PrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx++)
					{
						FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes[StaticMeshIdx];
						if (View.StaticMeshVisibilityMap[StaticMesh.Id]
							// Only render static mesh elements using translucent materials
							// PBD-DPG
							//&& StaticMesh.DepthPriorityGroup == DepthPriorityGroup
							&& StaticMesh.IsTranslucent(View.GetFeatureLevel()))
						{
							FTranslucencyClusteredShadingDrawingPolicyFactory::DrawMeshPrepass(
								RHICmdList,
								View,
								FTranslucencyClusteredShadingDrawingPolicyFactory::ContextType(),
								StaticMesh,
								PrimitiveSceneInfo->Proxy
							);
						}
					}
					if (bNeedTargetSwitch)
					{
						SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);
					}
				}

                for (int32 StaticMeshIdx=0; StaticMeshIdx < PrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx++)
                {
                    FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes[StaticMeshIdx];
                    if (View.StaticMeshVisibilityMap[StaticMesh.Id]
                        // PBD-DPG
						//&& StaticMesh.DepthPriorityGroup == DepthPriorityGroup
                        // Only render static mesh elements using translucent materials
						&& StaticMesh.IsTranslucent(View.GetFeatureLevel())
						&& StaticMesh.DepthPriorityGroup == DepthPriorityGroup)
					{
						bDirty |= FTranslucencyClusteredShadingDrawingPolicyFactory::DrawStaticMesh(
							RHICmdList, 
							View,
							FTranslucencyClusteredShadingDrawingPolicyFactory::ContextType(),
							StaticMesh,
							false,
							PrimitiveSceneInfo->Proxy,
							StaticMesh.BatchHitProxyId
							);
					}
				}
			}
		}
	}

	View.SimpleElementCollector.DrawBatchedElements(RHICmdList, View, FTexture2DRHIRef(), EBlendModeFilter::Translucent, DepthPriorityGroup);

	return bDirty;
}

void FClusteredForwardShadingSceneRenderer::RenderTranslucency(FRHICommandListImmediate& RHICmdList, ESceneDepthPriorityGroup DepthPriorityGroup)
{
	if (ShouldRenderTranslucency())
	{
		SCOPED_DRAW_EVENT(RHICmdList, Translucency);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];

			// Instanced stereo doesn't support translucency (?)
			//if (View.ShouldRenderView())
			{
				SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1 && !View.IsInstancedStereoPass(), TEXT("View%d"), ViewIndex);

				FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
				SceneContext.BeginRenderingTranslucency(RHICmdList, View);

				RenderTranslucencyView(RHICmdList, View, DepthPriorityGroup);
			}
		}
	}
}

bool FClusteredForwardShadingSceneRenderer::RenderTranslucencyView(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, ESceneDepthPriorityGroup DepthPriorityGroup)
{
	bool bDirty = false;

	// Enable depth test, disable depth writes.
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());

	// Draw only translucent prims that don't read from scene color
	bDirty |= View.TranslucentPrimSet.DrawPrimitivesForClusteredShading(RHICmdList, View, *this, DepthPriorityGroup);

	// Draw the view's mesh elements with the translucent drawing policy.
	bDirty |= DrawViewElements<FTranslucencyClusteredShadingDrawingPolicyFactory>(RHICmdList, View, FTranslucencyClusteredShadingDrawingPolicyFactory::ContextType(), DepthPriorityGroup, false);

	return bDirty;
}
