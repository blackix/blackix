// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ClusteredBasePassRendering.cpp: Base pass rendering implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"

#include "Clustered/ClusteredBasePassRendering.h"
#include "Clustered/ClusteredShadingRenderer.h"

#define IMPLEMENT_CLUSTERED_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName) \
	typedef TClusteredShadingBasePassVS< LightMapPolicyType > TClusteredShadingBasePassVS##LightMapPolicyName; \
	typedef TClusteredShadingBasePassHS< LightMapPolicyType > TClusteredShadingBasePassHS##LightMapPolicyName; \
	typedef TClusteredShadingBasePassDS< LightMapPolicyType > TClusteredShadingBasePassDS##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TClusteredShadingBasePassVS##LightMapPolicyName, TEXT("ClusteredShadingVertexShader"), TEXT("Main"), SF_Vertex); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TClusteredShadingBasePassHS##LightMapPolicyName, TEXT("ClusteredShadingTessellationShaders"), TEXT("MainHull"), SF_Hull); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TClusteredShadingBasePassDS##LightMapPolicyName, TEXT("ClusteredShadingTessellationShaders"), TEXT("MainDomain"), SF_Domain); \
	typedef TClusteredShadingBasePassPS< LightMapPolicyType, false, false > TClusteredShadingBasePassPS##LightMapPolicyName; \
	typedef TClusteredShadingBasePassPS< LightMapPolicyType, true, false > TClusteredShadingBasePassPS##LightMapPolicyName##Skylight; \
	typedef TClusteredShadingBasePassPS< LightMapPolicyType, false, true > TClusteredShadingBasePassPS##LightMapPolicyName##Refl; \
	typedef TClusteredShadingBasePassPS< LightMapPolicyType, true, true > TClusteredShadingBasePassPS##LightMapPolicyName##Skylight##Refl; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TClusteredShadingBasePassPS##LightMapPolicyName, TEXT("ClusteredShadingPixelShader"), TEXT("Main"), SF_Pixel); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TClusteredShadingBasePassPS##LightMapPolicyName##Skylight, TEXT("ClusteredShadingPixelShader"), TEXT("Main"), SF_Pixel); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TClusteredShadingBasePassPS##LightMapPolicyName##Refl, TEXT("ClusteredShadingPixelShader"), TEXT("Main"), SF_Pixel); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TClusteredShadingBasePassPS##LightMapPolicyName##Skylight##Refl, TEXT("ClusteredShadingPixelShader"), TEXT("Main"), SF_Pixel);

// Implement shader types per lightmap policy
IMPLEMENT_CLUSTERED_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_NO_LIGHTMAP>, FNoLightMapPolicy); 
IMPLEMENT_CLUSTERED_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_SHINDPT>, FCachedPointIndirectLightingPolicy );
IMPLEMENT_CLUSTERED_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MDL_SHINDPT>, FSimpleDirectionalLightAndSHDirectionalIndirectPolicy );
IMPLEMENT_CLUSTERED_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MDL_CSM_SHINDPT>, FSimpleDirectionalLightAndSHDirectionalCSMIndirectPolicy );
IMPLEMENT_CLUSTERED_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MDL>, FMovableDirectionalLightLightingPolicy );
IMPLEMENT_CLUSTERED_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MDL_CSM>, FMovableDirectionalLightCSMLightingPolicy );
IMPLEMENT_CLUSTERED_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MDL_CSM_DFS_HQLM>, FMovableDirectionalLightCSMWithDFShadowWithLightmapLightingPolicyHQ );
IMPLEMENT_CLUSTERED_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MDL_CSM_HQLM>, FMovableDirectionalLightCSMWithLightmapLightingPolicyHQ);
IMPLEMENT_CLUSTERED_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MDL_DFS_HQLM>, FMovableDirectionalLightWithDFShadowWithLightmapLightingPolicyHQ);

#if CLUSTERED_FAST_ITERATION
// In this mode, we enable lightmaps, lights, csms, even if not needed.
#else
IMPLEMENT_CLUSTERED_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_HQLM>, TLightMapPolicyHQ);
IMPLEMENT_CLUSTERED_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_DFS_HQLM>, TDistanceFieldShadowsAndLightMapPolicyHQ);
IMPLEMENT_CLUSTERED_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MDL_HQLM>, FMovableDirectionalLightWithLightmapLightingPolicyHQ);
#endif


/** The action used to draw a base pass static mesh element. */
class FDrawBasePassClusteredShadingStaticMeshAction
{
public:

	FScene* Scene;
	FStaticMesh* StaticMesh;

	/** Initialization constructor. */
	FDrawBasePassClusteredShadingStaticMeshAction(FScene* InScene,FStaticMesh* InStaticMesh):
		Scene(InScene),
		StaticMesh(InStaticMesh)
	{}

	inline bool ShouldPackAmbientSH() const
	{
		return false;
	}

	const FLightSceneInfo* GetSimpleDirectionalLight() const 
	{ 
		return Scene->SimpleDirectionalLight;
	}

	const bool GetSimpleDirectionalLightHasCSM() const
	{
		return Scene->bSimpleDirectionalLightHasCSM;
	}

	/** Draws the translucent mesh with a specific light-map type, and fog volume type */
	template<typename LightMapPolicyType>
	void Process(
		FRHICommandList& RHICmdList, 
		const FProcessBasePassMeshParameters& Parameters,
		const LightMapPolicyType& LightMapPolicy,
		const typename LightMapPolicyType::ElementDataType& LightMapElementData
		) const
	{
		FScene::EBasePassDrawListType DrawType = FScene::EBasePass_Default;		
 
		if (StaticMesh->IsMaskedOrAlphaToCoverage(Parameters.FeatureLevel))
		{
			DrawType = FScene::EBasePass_Masked;	
		}

		if ( Scene )
		{
			// Find the appropriate draw list for the static mesh based on the light-map policy type.
			TStaticMeshDrawList<TBasePassForClusteredShadingDrawingPolicy<LightMapPolicyType> >& DrawList =
				Scene->GetClusteredShadingBasePassDrawList<LightMapPolicyType>(DrawType);

			//ERHIFeatureLevel::Type FeatureLevel = Scene->GetFeatureLevel();
			// Add the static mesh to the draw list.
			DrawList.AddMesh(
				StaticMesh,
				typename TBasePassForClusteredShadingDrawingPolicy<LightMapPolicyType>::ElementDataType(LightMapElementData),
				TBasePassForClusteredShadingDrawingPolicy<LightMapPolicyType>(
					StaticMesh->VertexFactory,
					StaticMesh->MaterialRenderProxy,
					*Parameters.Material,
					LightMapPolicy,
					Parameters.BlendMode,
					Parameters.TextureMode,
					Parameters.ShadingModel != MSM_Unlit && Scene->ShouldRenderSkylight(),
					Parameters.ShadingModel != MSM_Unlit && Scene->ShouldRenderReflectionProbe(),
					false,
					Parameters.FeatureLevel,
					Parameters.bEditorCompositeDepthTest
				),
				Parameters.FeatureLevel
			);
		}
	}
};

void FBasePassClusteredOpaqueDrawingPolicyFactory::AddStaticMesh(FRHICommandList& RHICmdList, FScene* Scene, FStaticMesh* StaticMesh)
{
	// Determine the mesh's material and blend mode.
	const auto FeatureLevel = Scene->GetFeatureLevel();
	const FMaterial* Material = StaticMesh->MaterialRenderProxy->GetMaterial(FeatureLevel);
	const EBlendMode BlendMode = Material->GetBlendMode();

	// Don't composite static meshes
	const bool bEditorCompositeDepthTest = false;

	// Only draw opaque materials.
	if( !IsTranslucentBlendMode(BlendMode) )
	{
		ProcessBasePassMeshForClusteredShading(
			RHICmdList,
			FProcessBasePassMeshParameters(
				*StaticMesh,
				Material,
				StaticMesh->PrimitiveSceneInfo->Proxy,
				true,
				bEditorCompositeDepthTest,
				ESceneRenderTargetsMode::DontSet,
				FeatureLevel
				),
			FDrawBasePassClusteredShadingStaticMeshAction(Scene,StaticMesh)
			);
	}
}

/** The action used to draw a base pass dynamic mesh element. */
class FDrawBasePassClusteredShadingDynamicMeshAction
{
public:

	const FViewInfo& View;
	bool bBackFace;
	float DitheredLODTransitionValue;
	FHitProxyId HitProxyId;

	inline bool ShouldPackAmbientSH() const
	{
		return false;
	}

	const FLightSceneInfo* GetSimpleDirectionalLight() const 
	{
		auto* Scene = (FScene*)View.Family->Scene;
		return Scene ? Scene->SimpleDirectionalLight : nullptr;
	}

	const bool GetSimpleDirectionalLightHasCSM() const
	{
		auto* Scene = (FScene*)View.Family->Scene;
		return Scene->bSimpleDirectionalLightHasCSM;
	}

	/** Initialization constructor. */
	FDrawBasePassClusteredShadingDynamicMeshAction(
		const FViewInfo& InView,
		const bool bInBackFace,
		const float InDitheredLODTransitionValue,
		const FHitProxyId InHitProxyId
		)
		: View(InView)
		, bBackFace(bInBackFace)
		, DitheredLODTransitionValue(InDitheredLODTransitionValue)
		, HitProxyId(InHitProxyId)
	{
	}

	/** Draws the translucent mesh with a specific light-map type, and shader complexity predicate. */
	template<typename LightMapPolicyType>
	void Process(
		FRHICommandList& RHICmdList, 
		const FProcessBasePassMeshParameters& Parameters,
		const LightMapPolicyType& LightMapPolicy,
		const typename LightMapPolicyType::ElementDataType& LightMapElementData
		) const
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// When rendering masked materials in the shader complexity viewmode, 
		// We want to overwrite complexity for the pixels which get depths written,
		// And accumulate complexity for pixels which get killed due to the opacity mask being below the clip value.
		// This is accomplished by forcing the masked materials to render depths in the depth only pass, 
		// Then rendering in the base pass with additive complexity blending, depth tests on, and depth writes off.
		if (View.Family->EngineShowFlags.ShaderComplexity)
		{
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
		}
#endif

		const bool bIsLitMaterial = Parameters.ShadingModel != MSM_Unlit;
		const FScene* Scene = Parameters.PrimitiveSceneProxy ? Parameters.PrimitiveSceneProxy->GetPrimitiveSceneInfo()->Scene : NULL;

		typename TBasePassForClusteredShadingDrawingPolicy<LightMapPolicyType>::ContextDataType PolicyContext(Parameters.bIsInstancedStereo, false);
		TBasePassForClusteredShadingDrawingPolicy<LightMapPolicyType> DrawingPolicy(
			Parameters.Mesh.VertexFactory,
			Parameters.Mesh.MaterialRenderProxy,
			*Parameters.Material,
			LightMapPolicy,
			Parameters.BlendMode,
			Parameters.TextureMode,
			Parameters.ShadingModel != MSM_Unlit && Scene && Scene->ShouldRenderSkylight(),
			Parameters.ShadingModel != MSM_Unlit && Scene && Scene->ShouldRenderReflectionProbe(),
			View.Family->EngineShowFlags.ShaderComplexity,
			View.GetFeatureLevel(),
			Parameters.bEditorCompositeDepthTest
		);
		RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
		DrawingPolicy.SetSharedState(RHICmdList, &View, PolicyContext);

		for( int32 BatchElementIndex = 0, Num = Parameters.Mesh.Elements.Num(); BatchElementIndex < Num; BatchElementIndex++ )
		{
			// We draw instanced static meshes twice when rendering with instanced stereo. Once for each eye.
			const bool bIsInstancedMesh = Parameters.Mesh.Elements[BatchElementIndex].bIsInstancedMesh;
			const uint32 InstancedStereoDrawCount = (Parameters.bIsInstancedStereo && bIsInstancedMesh) ? 2 : 1;
			for (uint32 DrawCountIter = 0; DrawCountIter < InstancedStereoDrawCount; ++DrawCountIter)
			{
				DrawingPolicy.SetInstancedEyeIndex(RHICmdList, DrawCountIter);

				TDrawEvent<FRHICommandList> MeshEvent;
				BeginMeshDrawEvent(RHICmdList, Parameters.PrimitiveSceneProxy, Parameters.Mesh, MeshEvent);

				DrawingPolicy.SetMeshRenderState(
					RHICmdList,
					View,
					Parameters.PrimitiveSceneProxy,
					Parameters.Mesh,
					BatchElementIndex,
					bBackFace,
					DitheredLODTransitionValue,
					typename TBasePassForClusteredShadingDrawingPolicy<LightMapPolicyType>::ElementDataType(LightMapElementData),
					PolicyContext
				);
				DrawingPolicy.DrawMesh(RHICmdList, Parameters.Mesh, BatchElementIndex, Parameters.bIsInstancedStereo);
			}
		}

		TBasePassForClusteredShadingDrawingPolicy<LightMapPolicyType>::CleanPolicyRenderState(RHICmdList, PolicyContext);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (View.Family->EngineShowFlags.ShaderComplexity)
		{
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
		}
#endif
	}
};

bool FBasePassClusteredOpaqueDrawingPolicyFactory::DrawDynamicMesh(
	FRHICommandList& RHICmdList, 
	const FViewInfo& View,
	const ContextType& DrawingContext,
	const FMeshBatch& Mesh,
	bool bBackFace,
	bool bPreFog,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId,
	const bool bIsInstancedStereo
	)
{
	// Determine the mesh's material and blend mode.
	const FMaterial* Material = Mesh.MaterialRenderProxy->GetMaterial(View.GetFeatureLevel());
	const EBlendMode BlendMode = Material->GetBlendMode();

	// Only draw opaque materials.
	if(!IsTranslucentBlendMode(BlendMode))
	{
		ProcessBasePassMeshForClusteredShading(
			RHICmdList,
			FProcessBasePassMeshParameters(
				Mesh,
				Material,
				PrimitiveSceneProxy,
				!bPreFog,
				DrawingContext.bEditorCompositeDepthTest,
				DrawingContext.TextureMode,
				View.GetFeatureLevel(),
				bIsInstancedStereo
				),
			FDrawBasePassClusteredShadingDynamicMeshAction(
				View,
				bBackFace,
				Mesh.DitheredLODTransitionAlpha,
				HitProxyId
				)
			);
		return true;
	}
	else
	{
		return false;
	}
}

/** Base pass sorting modes. */
namespace EBasePassSort
{
	enum Type
	{
		/** Automatically select based on hardware/platform. */
		Auto = 0,
		/** No sorting. */
		None = 1,
		/** Sorts state buckets, not individual meshes. */
		SortStateBuckets = 2,
		/** Per mesh sorting. */
		SortPerMesh = 3,

		/** Useful range of sort modes. */
		FirstForcedMode = None,
		LastForcedMode = SortPerMesh
	};
};
extern TAutoConsoleVariable<int32> GSortBasePass;
extern TAutoConsoleVariable<int32> GMaxBasePassDraws;

static EBasePassSort::Type GetSortMode(bool bHasZPass)
{
	int32 SortMode = GSortBasePass.GetValueOnRenderThread();
	if (SortMode >= EBasePassSort::FirstForcedMode && SortMode <= EBasePassSort::LastForcedMode)
	{
		return (EBasePassSort::Type)SortMode;
	}

	// Determine automatically.
	if (GHardwareHiddenSurfaceRemoval)
	{
		return EBasePassSort::None;
	}
	else
	{
		if (bHasZPass)
		{
			return EBasePassSort::SortStateBuckets;
		}
		else
		{
			return EBasePassSort::SortPerMesh;
		}
	}
}

void FClusteredForwardShadingSceneRenderer::SortStateBuckets()
{
	EBasePassSort::Type SortMode = GetSortMode(EarlyZPassMode != DDM_None);
	if (SortMode == EBasePassSort::SortStateBuckets)
	{
		SCOPE_CYCLE_COUNTER(STAT_SortStaticDrawLists);

		for (int32 DrawType = 0; DrawType < FScene::EBasePass_MAX; DrawType++)
		{
			Scene->BasePassForClusteredShadingUniformLightMapPolicyDrawList[DrawType].SortFrontToBack(Views[0].ViewLocation);
		}
	}
}

void FClusteredForwardShadingSceneRenderer::RenderForwardShadingBasePass(FRHICommandListImmediate& RHICmdList, ESceneDepthPriorityGroup DepthPriorityGroup)
{
	SCOPED_DRAW_EVENT(RHICmdList, BasePass);
	SCOPE_CYCLE_COUNTER(STAT_BasePassDrawTime);

	// Draw the scene's emissive and light-map color.
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
		const FViewInfo& View = Views[ViewIndex];

		if (View.ShouldRenderView())
		{
			RenderForwardShadingBasePassView(RHICmdList, View, DepthPriorityGroup);
		}

		// Always render editor primitives for each view/eye
		RenderEditorPrimitivesView(RHICmdList, View, DepthPriorityGroup);
	}
}

void FClusteredForwardShadingSceneRenderer::SetupBasePassView(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, bool bIsEditorPrimitivePass)
{
	if (View.Family->EngineShowFlags.ShaderComplexity)
	{
		// Additive blending when shader complexity viewmode is enabled.
		RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA,BO_Add,BF_One,BF_One,BO_Add,BF_Zero,BF_One>::GetRHI());
		// Disable depth writes as we have a full depth prepass.
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false,CF_DepthNearOrEqual>::GetRHI());
	}
	else
	{

		// Opaque blending
		RHICmdList.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA>::GetRHI());
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
	}

	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
	RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());

	if (!View.IsInstancedStereoPass() && !bIsEditorPrimitivePass)
	{
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1);
	}
	else
	{
		RHICmdList.SetViewport(0, 0, 0, View.Family->FamilySizeX, View.ViewRect.Max.Y, 1);
	}
}

bool FClusteredForwardShadingSceneRenderer::RenderForwardShadingBasePassView(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, ESceneDepthPriorityGroup DepthPriorityGroup)
{
	EBasePassSort::Type const SortMode = GetSortMode(EarlyZPassMode != DDM_None);

	auto DrawStaticPrimitives = [this, &RHICmdList, DepthPriorityGroup, SortMode](const FViewInfo& View, FScene::EBasePassDrawListType DrawType) -> bool
	{
		bool bDirty = false;

		// Render the base pass static data
		if (SortMode == EBasePassSort::SortPerMesh
			// Oculus forward TODO: support sort per mesh and instanced stereo
			&& !View.IsInstancedStereoPass())
		{
			SCOPE_CYCLE_COUNTER(STAT_StaticDrawListDrawTime);

			int32 StartMaxDraws = GMaxBasePassDraws.GetValueOnRenderThread();
			if (StartMaxDraws <= 0)
			{
				StartMaxDraws = MAX_int32;
			}

			int32 MaxDraws = StartMaxDraws;
			MaxDraws -= Scene->BasePassForClusteredShadingUniformLightMapPolicyDrawList[DrawType].DrawVisibleFrontToBack(RHICmdList, DepthPriorityGroup, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility, MaxDraws);
			bDirty |= MaxDraws != StartMaxDraws;
		}
		else
		{
			SCOPE_CYCLE_COUNTER(STAT_StaticDrawListDrawTime);
			if (View.IsInstancedStereoPass())
			{
				const StereoPair StereoView(Views[0], Views[1], Views[0].StaticMeshVisibilityMap, Views[1].StaticMeshVisibilityMap, Views[0].StaticMeshBatchVisibility, Views[1].StaticMeshBatchVisibility);
				bDirty |= Scene->BasePassForClusteredShadingUniformLightMapPolicyDrawList[DrawType].DrawVisibleInstancedStereo(RHICmdList, DepthPriorityGroup, StereoView);
			}
			else
			{
				bDirty |= Scene->BasePassForClusteredShadingUniformLightMapPolicyDrawList[DrawType].DrawVisible(RHICmdList, DepthPriorityGroup, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility);
			}
		}

		return bDirty;
	};

	auto DrawDynamicPrimitives = [this, &RHICmdList, DepthPriorityGroup](const FViewInfo& View) -> bool
	{
		SCOPE_CYCLE_COUNTER(STAT_DynamicPrimitiveDrawTime);
		bool bDirty = false;

		// Oculus forward TODO: we don't support stencil ref changing on dynamic mesh elements.
		{
			SCOPED_DRAW_EVENT(RHICmdList, Dynamic);

			FBasePassClusteredOpaqueDrawingPolicyFactory::ContextType Context(false, ESceneRenderTargetsMode::DontSet);

			for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.DynamicMeshElements.Num(); MeshBatchIndex++)
			{
				const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

				if ((MeshBatchAndRelevance.bHasOpaqueOrMaskedMaterial || ViewFamily.EngineShowFlags.Wireframe) &&
					(MeshBatchAndRelevance.DepthPriorityGroup == DepthPriorityGroup))
				{
					const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
					bDirty |= FBasePassClusteredOpaqueDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, false, true, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId, View.IsInstancedStereoPass());
				}
			}
		}

		if (!View.Family->EngineShowFlags.CompositeEditorPrimitives)
		{
			SCOPED_DRAW_EVENT(RHICmdList, EditorPrimitives);

			const bool bNeedToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(GShaderPlatformForFeatureLevel[FeatureLevel]);

			// Draw the base pass for the view's batched mesh elements.
			bDirty |= DrawViewElements<FBasePassClusteredOpaqueDrawingPolicyFactory>(RHICmdList, View, FBasePassClusteredOpaqueDrawingPolicyFactory::ContextType(false, ESceneRenderTargetsMode::DontSet), DepthPriorityGroup, true);

			// Draw the view's batched simple elements(lines, sprites, etc).
			bDirty |= View.BatchedViewElements[DepthPriorityGroup].Draw(RHICmdList, FeatureLevel, bNeedToSwitchVerticalAxis, View.ViewProjectionMatrix, View.ViewRect.Width(), View.ViewRect.Height(), false);
		}

		return bDirty;
	};

	SetupBasePassView(RHICmdList, View, false /* editor primitives */);

	bool bDirty = false;
	if (EarlyZPassMode != DDM_None)
	{
		// We can render the geometry that didn't make it into the depth buffer first, so we add in more to the depth buffer before rendering opaque
		bDirty |= DrawDynamicPrimitives(View);

		{
			SCOPED_DRAW_EVENT(RHICmdList, StaticMasked);
			bDirty |= DrawStaticPrimitives(View, FScene::EBasePass_Masked);
		}
		{
			SCOPED_DRAW_EVENT(RHICmdList, Static);
			bDirty |= DrawStaticPrimitives(View, FScene::EBasePass_Default);
		}
	}
	else
	{
		// Else static (unmasked first) then dynamic
		{
			SCOPED_DRAW_EVENT(RHICmdList, Static);
			bDirty |= DrawStaticPrimitives(View, FScene::EBasePass_Default);
		}
		{
			SCOPED_DRAW_EVENT(RHICmdList, StaticMasked);
			bDirty |= DrawStaticPrimitives(View, FScene::EBasePass_Masked);
		}

		bDirty |= DrawDynamicPrimitives(View);
	}

	return bDirty;
}

bool FClusteredForwardShadingSceneRenderer::RenderEditorPrimitivesView(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, ESceneDepthPriorityGroup DepthPriorityGroup)
{
	SetupBasePassView(RHICmdList, View, true /* editor primitives */);

	View.SimpleElementCollector.DrawBatchedElements(RHICmdList, View, NULL, EBlendModeFilter::OpaqueAndMasked, DepthPriorityGroup);

	bool bDirty = false;
	if (!View.Family->EngineShowFlags.CompositeEditorPrimitives)
	{
		SCOPED_DRAW_EVENT(RHICmdList, EditorPrimitives);

		const bool bNeedToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(GShaderPlatformForFeatureLevel[FeatureLevel]);

		// Draw the base pass for the view's batched mesh elements.
		bDirty |= DrawViewElements<FBasePassClusteredOpaqueDrawingPolicyFactory>(RHICmdList, View, FBasePassClusteredOpaqueDrawingPolicyFactory::ContextType(false, ESceneRenderTargetsMode::DontSet), DepthPriorityGroup, true);

		// Draw the view's batched simple elements(lines, sprites, etc).
		bDirty |= View.BatchedViewElements[DepthPriorityGroup].Draw(RHICmdList, FeatureLevel, bNeedToSwitchVerticalAxis, View.ViewProjectionMatrix, View.ViewRect.Width(), View.ViewRect.Height(), false);
	}

	return bDirty;
}

template <>
void
GetBasePassShadersForClusteredShading<FUniformLightMapPolicy>(
	const FMaterial& Material,
	FVertexFactoryType* VertexFactoryType,
	FUniformLightMapPolicy LightMapPolicy,
	bool bNeedsHSDS,
	bool bEnableSkyLight,
	bool bEnableReflectionProbe,
	FBaseHS*& HullShader,
	FBaseDS*& DomainShader,
	TBasePassForClusteredShadingVSPolicyParamType<typename FUniformLightMapPolicy::VertexParametersType>*& VertexShader,
	TBasePassForClusteredShadingPSPolicyParamType<typename FUniformLightMapPolicy::PixelParametersType>*& PixelShader
)
{
	switch (LightMapPolicy.GetIndirectPolicy())
	{
#define POLICY_TYPE(TYPE) \
	case (TYPE): \
		GetBasePassShadersForClusteredShading<TUniformLightMapPolicy<TYPE>>(Material, VertexFactoryType, TUniformLightMapPolicy<TYPE>(), bNeedsHSDS, bEnableSkyLight, bEnableReflectionProbe, HullShader, DomainShader, VertexShader, PixelShader); \
		break;

	// See ELightmapPolicyType and ProcessBasePassMeshForClusteredShading
	POLICY_TYPE( LMP_MDL_HQLM )
	POLICY_TYPE( LMP_MDL_CSM_HQLM )
	POLICY_TYPE( LMP_MDL_CSM_DFS_HQLM )
	POLICY_TYPE( LMP_MDL_DFS_HQLM )
	POLICY_TYPE( LMP_DFS_HQLM )
	POLICY_TYPE( LMP_HQLM )
	POLICY_TYPE( LMP_MDL_CSM )
	POLICY_TYPE( LMP_MDL )
	POLICY_TYPE( LMP_MDL_CSM_SHINDPT )
	POLICY_TYPE( LMP_MDL_SHINDPT )
	POLICY_TYPE( LMP_SHINDPT )
	POLICY_TYPE( LMP_NO_LIGHTMAP )

#undef POLICY_TYPE

	default:	
		checkf(false, TEXT("Unknown LightmapPolicyType: %i"), LightMapPolicy.GetIndirectPolicy());
	}
}