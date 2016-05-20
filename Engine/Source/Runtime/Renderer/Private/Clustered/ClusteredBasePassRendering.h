// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ClusteredBasePassRendering.h: base pass rendering definitions.
=============================================================================*/

#pragma once

#include "LightMapRendering.h"
#include "ShaderBaseClasses.h"
#include "EditorCompositeParams.h"


// Define if you want to cut out a bunch of shader branches from the shader.
// More features will be active when not needed, but there will be net fewer shaders
#define CLUSTERED_FAST_ITERATION 0

// If translucency should support translucent volume lighting.
// By default this is disabled by the Oculus integration, so we don't support it by default either.
#define CLUSTERED_SUPPORTS_TRANSLUCENT_VOLUME 0

// If translucency should have per-pixel directional lighting (including sampling the CSM!)
// Otherwise the scene's directional light is added to the translucent volume, if enabled.
#define CLUSTERED_SUPPORTS_TRANSLUCENCY_LIGHTING_DIRECTIONAL_LIGHT 1

// If clustered support sky reflections or not.
// This needs extra work to support both reflection probes and the sky light.
#define CLUSTERED_SUPPORTS_SKY_LIGHT_REFLECTIONS 0

// Clustered shading uses box captures (otherwise sphere captures will be used)
#define CLUSTERED_USE_BOX_REFLECTION_CAPTURE 0



/**
 * The base shader type for hull shaders.
 */
template<typename LightMapPolicyType>
class TClusteredShadingBasePassHS : public FBaseHS
{
	DECLARE_SHADER_TYPE(TClusteredShadingBasePassHS, MeshMaterial);

protected:

	TClusteredShadingBasePassHS() {}

	TClusteredShadingBasePassHS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) :
		FBaseHS(Initializer)
	{}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Re-use vertex shader gating
		return FBaseHS::ShouldCache(Platform, Material, VertexFactoryType)
			&& TClusteredShadingBasePassVS<LightMapPolicyType>::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Re-use vertex shader compilation environment
		TClusteredShadingBasePassVS<LightMapPolicyType>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}
};

/**
 * The base shader type for Domain shaders.
 */
template<typename LightMapPolicyType>
class TClusteredShadingBasePassDS : public FBaseDS
{
	DECLARE_SHADER_TYPE(TClusteredShadingBasePassDS,MeshMaterial);

protected:

	TClusteredShadingBasePassDS() {}

	TClusteredShadingBasePassDS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) :
		FBaseDS(Initializer)
	{
	}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Re-use vertex shader gating
		return FBaseDS::ShouldCache(Platform, Material, VertexFactoryType)
			&& TClusteredShadingBasePassVS<LightMapPolicyType>::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Re-use vertex shader compilation environment
		TClusteredShadingBasePassVS<LightMapPolicyType>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

public:
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FBaseDS::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FVertexFactory* VertexFactory,
		const FSceneView& View
		)
	{
		FBaseDS::SetParameters(RHICmdList, MaterialRenderProxy, View);
	}
};



/**
 * Root of all ClusteredForward vertex shaders.
 */
template<typename VertexParametersType>
class TBasePassForClusteredShadingVSPolicyParamType : public FMeshMaterialShader, public VertexParametersType
{
protected:

	TBasePassForClusteredShadingVSPolicyParamType() {}
	TBasePassForClusteredShadingVSPolicyParamType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		VertexParametersType::Bind(Initializer.ParameterMap);
		InstancedEyeIndexParameter.Bind(Initializer.ParameterMap, TEXT("InstancedEyeIndex"));
		IsInstancedStereoParameter.Bind(Initializer.ParameterMap, TEXT("bIsInstancedStereo"));
	}

public:

	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		return !IsMobilePlatform(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		VertexParametersType::Serialize(Ar);
		Ar << InstancedEyeIndexParameter;
		Ar << IsInstancedStereoParameter;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FVertexFactory* VertexFactory,
		const FMaterial& InMaterialResource,
		const FSceneView& View,
		ESceneRenderTargetsMode::Type TextureMode,
		const bool bIsInstancedStereo
		)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(),MaterialRenderProxy,InMaterialResource,View,TextureMode);

		if (IsInstancedStereoParameter.IsBound())
		{
			SetShaderValue(RHICmdList, GetVertexShader(), IsInstancedStereoParameter, bIsInstancedStereo);
		}
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement,const FMeshDrawingRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(),VertexFactory,View,Proxy,BatchElement,DrawRenderState);
	}

	void SetInstancedEyeIndex(FRHICommandList& RHICmdList, const uint32 EyeIndex)
	{
		if (InstancedEyeIndexParameter.IsBound())
		{
			SetShaderValue(RHICmdList, GetVertexShader(), InstancedEyeIndexParameter, EyeIndex);
		}
	}

private:
	FShaderParameter InstancedEyeIndexParameter;
	FShaderParameter IsInstancedStereoParameter;
};




/**
 * Base vertex shader for clustered shading
 */
template< typename LightMapPolicyType >
class TClusteredShadingBasePassVS : public TBasePassForClusteredShadingVSPolicyParamType<typename LightMapPolicyType::VertexParametersType>
{
	typedef TBasePassForClusteredShadingVSPolicyParamType<typename LightMapPolicyType::VertexParametersType> Super;

	DECLARE_SHADER_TYPE(TClusteredShadingBasePassVS,MeshMaterial);
public:
	
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{		
		return Super::ShouldCache(Platform, Material, VertexFactoryType)
			&& LightMapPolicyType::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}
	
	/** Initialization constructor. */
	TClusteredShadingBasePassVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		Super(Initializer)
	{}

	/** Default constructor. */
	TClusteredShadingBasePassVS() {}
};


/**
 * Root of all ClusteredForward pixel shaders.
 */
template<typename PixelParametersType>
class TBasePassForClusteredShadingPSPolicyParamType : public FMeshMaterialShader, public PixelParametersType
{
public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return !IsMobilePlatform(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("MAX_CLUSTERED_FORWARD_LIGHTS"), uint32(MAX_CLUSTERED_FORWARD_LIGHTS));
		OutEnvironment.SetDefine(TEXT("CLUSTERED_USE_TRANSLUCENT_VOLUMES"), uint32(CLUSTERED_SUPPORTS_TRANSLUCENT_VOLUME));
		OutEnvironment.SetDefine(TEXT("LIGHT_GRID_TILE_SIZE_X"), FClusteredForwardShadingSceneRenderer::kLightGridTileSizeX);
		OutEnvironment.SetDefine(TEXT("LIGHT_GRID_TILE_SIZE_Y"), FClusteredForwardShadingSceneRenderer::kLightGridTileSizeY);
		OutEnvironment.SetDefine(TEXT("LIGHT_GRID_SLICES_Z"), FClusteredForwardShadingSceneRenderer::kLightGridSlicesZ);
		OutEnvironment.SetDefine(TEXT("CLUSTERED_USE_BOX_REFLECTION_CAPTURE"), uint32(CLUSTERED_USE_BOX_REFLECTION_CAPTURE));

		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	/** Default constructor. */
	TBasePassForClusteredShadingPSPolicyParamType() {}

	/** Initialization constructor. */
	TBasePassForClusteredShadingPSPolicyParamType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer, bool bEnableSkyLight = false, bool bEnableReflectionProbe = false) :
		FMeshMaterialShader(Initializer)
		, bSetSkyLight(bEnableSkyLight)
		, bSetReflectionProbe(bEnableReflectionProbe)
	{
		PixelParametersType::Bind(Initializer.ParameterMap);
		EditorCompositeParams.Bind(Initializer.ParameterMap);

		/* See SetMesh(...)
		if (bEnableReflectionProbe)
		{
			ReflectionCubemap.Bind(Initializer.ParameterMap, TEXT("ReflectionCubemap"));
			ReflectionSampler.Bind(Initializer.ParameterMap, TEXT("ReflectionCubemapSampler"));
		}
		*/

		#if CLUSTERED_SUPPORTS_SKY_LIGHT_REFLECTIONS
			if (bEnableSkyLight)
			{
				SkyLightParameters.Bind(Initializer.ParameterMap);
			}
		#endif

		#if CLUSTERED_SUPPORTS_TRANSLUCENT_VOLUME
			TranslucentVolumeLightingParameters.Bind(Initializer.ParameterMap);
		#endif
	}

	void SetParameters(FRHICommandList& RHICmdList, const FMaterialRenderProxy* MaterialRenderProxy, const FMaterial& MaterialResource, const FSceneView* View, EBlendMode BlendMode, ESceneRenderTargetsMode::Type TextureMode, bool bEnableEditorPrimitveDepthTest)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetPixelShader(),MaterialRenderProxy,MaterialResource,*View,TextureMode);

		#if CLUSTERED_SUPPORTS_SKY_LIGHT_REFLECTIONS
			if (bSetSkyLight)
			{
				SkyLightParameters.SetParameters(RHICmdList, GetPixelShader(), (const FScene*)(View->Family->Scene), true);
			}
		#endif

		#if CLUSTERED_SUPPORTS_TRANSLUCENT_VOLUME
			if (IsTranslucentBlendMode(BlendMode))
			{
				TranslucentVolumeLightingParameters.SetParameters(RHICmdList, GetPixelShader());
			}
		#endif

		#if WITH_EDITOR
			// Avoid the function call when not in editor
			EditorCompositeParams.SetParameters(RHICmdList, MaterialResource, View, bEnableEditorPrimitveDepthTest, GetPixelShader());
		#endif
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement, const FMeshDrawingRenderState& DrawRenderState)
	{
		FRHIPixelShader* PixelShader = GetPixelShader();

		/*
		 * Oculus forward not supporting individual cubemaps yet.  And when/if we do, we want to do this by
		 * using the global cubemap array, not by setting individual textures.  Ideally we would not need this, 
		 * and lookup in the cubemap array via a clustered grid like lights, but this is certainly cheaper.
		if (bSetReflectionProbe && ReflectionCubemap.IsBound())
		{
			FTexture* ReflectionTexture = GBlackTextureCube;
			FPrimitiveSceneInfo* PrimitiveSceneInfo = Proxy ? Proxy->GetPrimitiveSceneInfo() : NULL;

			// TODO: we will want to use the full HDR one, not the encoded one
			if (PrimitiveSceneInfo 
				&& PrimitiveSceneInfo->CachedReflectionCaptureProxy
				&& PrimitiveSceneInfo->CachedReflectionCaptureProxy->EncodedHDRCubemap
				&& PrimitiveSceneInfo->CachedReflectionCaptureProxy->EncodedHDRCubemap->IsInitialized())
			{
				ReflectionTexture = PrimitiveSceneInfo->CachedReflectionCaptureProxy->EncodedHDRCubemap;
			}

			// Set the reflection cubemap
			SetTextureParameter(RHICmdList, PixelShader, ReflectionCubemap, ReflectionSampler, ReflectionTexture);
		}
		*/

		FMeshMaterialShader::SetMesh(RHICmdList, PixelShader, VertexFactory, View, Proxy, BatchElement, DrawRenderState);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		PixelParametersType::Serialize(Ar);
		Ar << bSetSkyLight;
		Ar << bSetReflectionProbe;
		/*
		if (bSetReflectionProbe)
		{
			Ar << ReflectionCubemap;
			Ar << ReflectionSampler;
		}
		*/
		Ar << EditorCompositeParams;
		#if CLUSTERED_SUPPORTS_SKY_LIGHT_REFLECTIONS
			if (bSetSkyLight)
			{
				Ar << SkyLightParameters;
			}
		#endif
		#if CLUSTERED_SUPPORTS_TRANSLUCENT_VOLUME
			Ar << TranslucentVolumeLightingParameters;
		#endif
		return bShaderHasOutdatedParameters;
	}

private:
	//FShaderResourceParameter ReflectionCubemap;
	//FShaderResourceParameter ReflectionSampler;
	FEditorCompositingParameters EditorCompositeParams;
	#if CLUSTERED_SUPPORTS_TRANSLUCENT_VOLUME
		FTranslucentVolumeLightingParameters TranslucentVolumeLightingParameters;
	#endif
	#if CLUSTERED_SUPPORTS_SKY_LIGHT_REFLECTIONS
		FSkyLightReflectionParameters SkyLightParameters;
	#endif
	uint8 bSetSkyLight;
	uint8 bSetReflectionProbe;
};


template< typename LightMapPolicyType, bool bEnableSkyLight, bool bEnableReflectionProbe>
class TClusteredShadingBasePassPS : public TBasePassForClusteredShadingPSPolicyParamType<typename LightMapPolicyType::PixelParametersType>
{
	typedef TBasePassForClusteredShadingPSPolicyParamType<typename LightMapPolicyType::PixelParametersType> Super;

	DECLARE_SHADER_TYPE(TClusteredShadingBasePassPS,MeshMaterial);
public:

	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{		
		// Only compile skylight version for lit materials
		const bool bIsLit = (Material->GetShadingModel() != MSM_Unlit);
		const bool bShouldCacheBySkylight = !bEnableSkyLight || bIsLit;
		const bool bShouldCacheByReflections = !bEnableReflectionProbe || bIsLit;

		return bShouldCacheBySkylight
			&& bShouldCacheByReflections 
			&& LightMapPolicyType::ShouldCache(Platform,Material,VertexFactoryType)
			&& Super::ShouldCache(Platform, Material, VertexFactoryType);
	}
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{		
		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("ENABLE_REFLECTION_PROBE"), uint32(bEnableReflectionProbe));
		OutEnvironment.SetDefine(TEXT("ENABLE_SKY_LIGHT"), uint32(bEnableSkyLight));
		OutEnvironment.SetDefine(TEXT("ENABLE_SKY_LIGHT_REFLECTIONS"), uint32(bEnableSkyLight && CLUSTERED_SUPPORTS_SKY_LIGHT_REFLECTIONS));
		OutEnvironment.SetDefine(TEXT("TRANSLUCENCY_LIGHTING_DIRECTIONAL_LIGHT"), uint32(CLUSTERED_SUPPORTS_TRANSLUCENCY_LIGHTING_DIRECTIONAL_LIGHT));
	}

	/** Default constructor. */
	TClusteredShadingBasePassPS() {}
	
	/** Initialization constructor. */
	TClusteredShadingBasePassPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		Super(Initializer, bEnableSkyLight, bEnableReflectionProbe)
	{
	}
};

/**
 * Get shader templates allowing to redirect between compatible shaders.
 */
template <typename LightMapPolicyType>
void
GetBasePassShadersForClusteredShading(
	const FMaterial& Material,
	FVertexFactoryType* VFType,
	LightMapPolicyType LightMapPolicy,
	bool bNeedsHSDS,
	bool bEnableSkyLight,
	bool bEnableReflectionProbe,
	FBaseHS*& HullShader,
	FBaseDS*& DomainShader,
	TBasePassForClusteredShadingVSPolicyParamType<typename LightMapPolicyType::VertexParametersType>*& VertexShader,
	TBasePassForClusteredShadingPSPolicyParamType<typename LightMapPolicyType::PixelParametersType>*& PixelShader
	)
{
	if (bNeedsHSDS)
	{
		HullShader = Material.GetShader<TClusteredShadingBasePassHS<LightMapPolicyType>>(VFType);
		DomainShader = Material.GetShader<TClusteredShadingBasePassDS<LightMapPolicyType>>(VFType);
	}

	VertexShader = Material.GetShader<TClusteredShadingBasePassVS<LightMapPolicyType>>(VFType);

	if (bEnableSkyLight)
	{
		if (bEnableReflectionProbe)
		{
			PixelShader = Material.GetShader< TClusteredShadingBasePassPS<LightMapPolicyType, true, true>>(VFType);
		}
		else
		{
			PixelShader = Material.GetShader< TClusteredShadingBasePassPS<LightMapPolicyType, true, false>>(VFType);
		}
	}
	else
	{
		if (bEnableReflectionProbe)
		{
			PixelShader = Material.GetShader< TClusteredShadingBasePassPS<LightMapPolicyType, false, true>>(VFType);
		}
		else
		{
			PixelShader = Material.GetShader< TClusteredShadingBasePassPS<LightMapPolicyType, false, false>>(VFType);
		}
	}
}

// Specialization to switch on the underlying lightmap mode.
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
);

/**
 * Draws the emissive color and the light-map of a mesh.
 */
template<typename LightMapPolicyType>
class TBasePassForClusteredShadingDrawingPolicy : public FMeshDrawingPolicy
{
public:

	struct ContextDataType : public FMeshDrawingPolicy::ContextDataType
	{
		uint8 CurrentStencilRef;
		bool AlphaToCoverageEnabled;

		ContextDataType(const bool InbIsInstancedStereo, const bool InbNeedsInstancedStereoBias) 
			: FMeshDrawingPolicy::ContextDataType(InbIsInstancedStereo, InbNeedsInstancedStereoBias)
			, CurrentStencilRef(0)
			, AlphaToCoverageEnabled(false)
		{
		}

		ContextDataType() 
			: CurrentStencilRef(0)
			, AlphaToCoverageEnabled(false)
		{
		}
	};

	/** The data the drawing policy uses for each mesh element. */
	class ElementDataType
	{
	public:

		/** The element's light-map data. */
		typename LightMapPolicyType::ElementDataType LightMapElementData;

		/** Default constructor. */
		ElementDataType()
		{}

		/** Initialization constructor. */
		ElementDataType(const typename LightMapPolicyType::ElementDataType& InLightMapElementData)
		:	LightMapElementData(InLightMapElementData)
		{}
	};

	/** Initialization constructor. */
	TBasePassForClusteredShadingDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		LightMapPolicyType InLightMapPolicy,
		EBlendMode InBlendMode,
		ESceneRenderTargetsMode::Type InSceneTextureMode,
		bool bInEnableSkyLight,
		bool bInEnableReflectionProbe,
		bool bOverrideWithShaderComplexity,
		ERHIFeatureLevel::Type InFeatureLevel,
		bool bInEnableEditorPrimitiveDepthTest = false,
		EQuadOverdrawMode InQuadOverdrawMode = QOM_None
		):
		FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,InMaterialResource,bOverrideWithShaderComplexity, false, false, false, AllowRuntimeQuadOverdraw(InFeatureLevel) ? InQuadOverdrawMode : QOM_None),
		LightMapPolicy(InLightMapPolicy),
		BlendMode(InBlendMode),
		SceneTextureMode(InSceneTextureMode),
		bEnableEditorPrimitiveDepthTest(bInEnableEditorPrimitiveDepthTest),
		bEnableSkyLight(bInEnableSkyLight),
		bEnableReflectionProbe(bInEnableReflectionProbe),
		bEnableAlphaToCoverage(InMaterialResource.IsAlphaToCoverage()),
		HullShader(NULL),
		DomainShader(NULL)
	{
		FVertexFactoryType* const VFType = InVertexFactory->GetType();

		const EMaterialTessellationMode MaterialTessellationMode = InMaterialResource.GetTessellationMode();
		const bool bNeedsHSDS = 
			MaterialTessellationMode != MTM_NoTessellation
			&& RHISupportsTessellation(GShaderPlatformForFeatureLevel[InFeatureLevel])
			&& VFType->SupportsTessellationShaders();

		GetBasePassShadersForClusteredShading<LightMapPolicyType>(
			InMaterialResource,
			InVertexFactory->GetType(),
			InLightMapPolicy,
			bNeedsHSDS,
			bInEnableSkyLight,
			bInEnableReflectionProbe,
			HullShader,
			DomainShader,
			VertexShader,
			PixelShader
		);
	
#if DO_GUARD_SLOW
		// Somewhat hacky
		if (SceneTextureMode == ESceneRenderTargetsMode::DontSet && !bEnableEditorPrimitiveDepthTest && InMaterialResource.IsUsedWithEditorCompositing())
		{
			SceneTextureMode = ESceneRenderTargetsMode::DontSetIgnoreBoundByEditorCompositing;
		}
#endif
	}

	// FMeshDrawingPolicy interface.

	bool Matches(const TBasePassForClusteredShadingDrawingPolicy& Other) const
	{
		return FMeshDrawingPolicy::Matches(Other) &&
			VertexShader == Other.VertexShader &&
			PixelShader == Other.PixelShader &&
			HullShader == Other.HullShader &&
			DomainShader == Other.DomainShader &&
			SceneTextureMode == Other.SceneTextureMode &&
			LightMapPolicy == Other.LightMapPolicy &&
			BlendMode == Other.BlendMode &&
			bEnableSkyLight == Other.bEnableSkyLight &&
			bEnableReflectionProbe == Other.bEnableReflectionProbe &&
			bEnableAlphaToCoverage == Other.bEnableAlphaToCoverage;
	}

	void SetSharedState(FRHICommandList& RHICmdList, const FViewInfo* View, ContextDataType& PolicyContext) const
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// If QuadOverdraw is allowed, different VS/DS/HS must be used (with only SV_POSITION as PS interpolant).
		if (bOverrideWithShaderComplexity && AllowRuntimeQuadOverdraw(View->GetFeatureLevel()))
		{
			SetNonPSParametersForQuadOverdraw(RHICmdList, MaterialRenderProxy, MaterialResource, *View, VertexFactory, HullShader && DomainShader);
		}
		else
#endif
		{
			// Set the light-map policy.
			LightMapPolicy.Set(RHICmdList, VertexShader, bOverrideWithShaderComplexity ? NULL : PixelShader, VertexShader, PixelShader, VertexFactory, MaterialRenderProxy, View);

			VertexShader->SetParameters(RHICmdList, MaterialRenderProxy, VertexFactory, *MaterialResource, *View, SceneTextureMode, PolicyContext.bIsInstancedStereo);

			if (HullShader)
			{
				HullShader->SetParameters(RHICmdList, MaterialRenderProxy, *View);
			}

			if (DomainShader)
			{
				DomainShader->SetParameters(RHICmdList, MaterialRenderProxy, *View);
			}
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (bOverrideWithShaderComplexity)
		{
			// If we are in the translucent pass then override the blend mode, otherwise maintain additive blending.
			if (IsTranslucentBlendMode(BlendMode))
			{
				RHICmdList.SetBlendState( TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI());
			}

			const uint32 NumPixelShaderInstructions = PixelShader->GetNumInstructions();
			const uint32 NumVertexShaderInstructions = VertexShader->GetNumInstructions();
			FShaderComplexityAccumulatePS::SetParameters(View->ShaderMap,RHICmdList,NumVertexShaderInstructions,NumPixelShaderInstructions,GetQuadOverdrawMode(),View->GetFeatureLevel());
		}
		else
#endif
		{
			PixelShader->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, View, BlendMode, SceneTextureMode, bEnableEditorPrimitiveDepthTest);

			switch(BlendMode)
			{
			default:
			case BLEND_Opaque:
			case BLEND_Masked:
				// Opaque and Masked materials are rendered together in the base pass, where the blend state is set at a higher level

				if (bEnableAlphaToCoverage != PolicyContext.AlphaToCoverageEnabled)
				{
					PolicyContext.AlphaToCoverageEnabled = bEnableAlphaToCoverage;
					if (bEnableAlphaToCoverage) {
						RHICmdList.SetBlendState(TStaticBlendStateA2CWriteMask<CW_RGBA>::GetRHI());
					} else {
						RHICmdList.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA>::GetRHI());
					}
				}

				break;
			case BLEND_Translucent:
				RHICmdList.SetBlendState( TStaticBlendState<CW_RGB, BO_Add,BF_SourceAlpha,BF_InverseSourceAlpha,BO_Add,BF_Zero,BF_InverseSourceAlpha>::GetRHI());
				break;
			case BLEND_Additive:
				// Add to the existing scene color
				RHICmdList.SetBlendState( TStaticBlendState<CW_RGB, BO_Add,BF_One,BF_One,BO_Add,BF_Zero,BF_InverseSourceAlpha>::GetRHI());
				break;
			case BLEND_Modulate:
				// Modulate with the existing scene color
				RHICmdList.SetBlendState( TStaticBlendState<CW_RGB,BO_Add,BF_DestColor,BF_Zero>::GetRHI());
				break;
			};
		}
	}

	void SetInstancedEyeIndex(FRHICommandList& RHICmdList, const uint32 EyeIndex) const
	{
		VertexShader->SetInstancedEyeIndex(RHICmdList, EyeIndex);
	}

	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @return new bound shader state object
	*/
	FBoundShaderStateInput GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel)
	{
		FBoundShaderStateInput BoundShaderStateInput(
			FMeshDrawingPolicy::GetVertexDeclaration(), 
			VertexShader->GetVertexShader(),
			GETSAFERHISHADER_HULL(HullShader), 
			GETSAFERHISHADER_DOMAIN(DomainShader), 
			PixelShader->GetPixelShader(),
			FGeometryShaderRHIRef()
			);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (bOverrideWithShaderComplexity)
		{
			if (AllowRuntimeQuadOverdraw(InFeatureLevel))
			{
				PatchBoundShaderStateInputForQuadOverdraw(BoundShaderStateInput, MaterialResource, VertexFactory, InFeatureLevel, GetQuadOverdrawMode());
			}
			else
			{
				TShaderMapRef<TShaderComplexityAccumulatePS> ShaderComplexityAccumulatePixelShader(GetGlobalShaderMap(InFeatureLevel));
				BoundShaderStateInput.PixelShaderRHI = ShaderComplexityAccumulatePixelShader->FGlobalShader::GetPixelShader();
			}
		}
#endif
		return BoundShaderStateInput;
	}

	static void CleanPolicyRenderState(FRHICommandList& RHICmdList, ContextDataType& PolicyContext)
	{
		if (PolicyContext.CurrentStencilRef != 0)
		{
			PolicyContext.CurrentStencilRef = 0;
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI(), 0);
		}

		if (PolicyContext.AlphaToCoverageEnabled)
		{
			PolicyContext.AlphaToCoverageEnabled = false;
			RHICmdList.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA>::GetRHI());
		}
	}

	void SetMeshRenderState(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& Mesh,
		int32 BatchElementIndex,
		bool bBackFace,
		const FMeshDrawingRenderState& DrawRenderState,
		const ElementDataType& ElementData,
		ContextDataType& PolicyContext
		) const
	{
		const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// If QuadOverdraw is allowed, different VS/DS/HS must be used (with only SV_POSITION as PS interpolant).
		if (bOverrideWithShaderComplexity && AllowRuntimeQuadOverdraw(View.GetFeatureLevel()))
		{
			SetMeshForQuadOverdraw(RHICmdList, MaterialResource, View, VertexFactory, HullShader && DomainShader, PrimitiveSceneProxy, BatchElement, DrawRenderState);
		}
		else
#endif
		{
			// Set the light-map policy's mesh-specific settings.
			LightMapPolicy.SetMesh(
				RHICmdList,
				View,
				PrimitiveSceneProxy,
				VertexShader,
				bOverrideWithShaderComplexity ? NULL : PixelShader,
				VertexShader,
				PixelShader,
				VertexFactory,
				MaterialRenderProxy,
				ElementData.LightMapElementData);

			VertexShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState);

			if (HullShader && DomainShader)
			{
				HullShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState);
				DomainShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState);
			}
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (bOverrideWithShaderComplexity)
		{
			// If we are in the translucent pass or rendering a masked material then override the blend mode, otherwise maintain opaque blending
			if (BlendMode != BLEND_Opaque)
			{
				// Add complexity to existing, keep alpha
				RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI());
			}

			const auto FeatureLevel = View.GetFeatureLevel();
			const uint32 NumPixelShaderInstructions = PixelShader->GetNumInstructions();
			const uint32 NumVertexShaderInstructions = VertexShader->GetNumInstructions();
			FShaderComplexityAccumulatePS::SetParameters(View.ShaderMap, RHICmdList, NumVertexShaderInstructions, NumPixelShaderInstructions, GetQuadOverdrawMode(), FeatureLevel);
		}
		else
#endif
		{
			PixelShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState);
		}

		FMeshDrawingPolicy::SetMeshRenderState(RHICmdList, View, PrimitiveSceneProxy, Mesh, BatchElementIndex, bBackFace, DrawRenderState, FMeshDrawingPolicy::ElementDataType(), PolicyContext);
	}

	friend int32 CompareDrawingPolicy(const TBasePassForClusteredShadingDrawingPolicy& A,const TBasePassForClusteredShadingDrawingPolicy& B)
	{
		COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
		COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
		COMPAREDRAWINGPOLICYMEMBERS(HullShader);
		COMPAREDRAWINGPOLICYMEMBERS(DomainShader);
		COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
		COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
		COMPAREDRAWINGPOLICYMEMBERS(SceneTextureMode);
		COMPAREDRAWINGPOLICYMEMBERS(BlendMode);
		COMPAREDRAWINGPOLICYMEMBERS(bEnableSkyLight);
		COMPAREDRAWINGPOLICYMEMBERS(bEnableReflectionProbe);
		COMPAREDRAWINGPOLICYMEMBERS(bEnableAlphaToCoverage);

		return CompareDrawingPolicy(A.LightMapPolicy,B.LightMapPolicy);
	}

protected:
	// Here we don't store the most derived type of shaders, for instance TBasePassVertexShaderBaseType<LightMapPolicyType>.
	// This is to allow any shader using the same parameters to be used, and is required to allow FUniformLightMapPolicy to use shaders derived from TUniformLightMapPolicy.
	TBasePassForClusteredShadingVSPolicyParamType<typename LightMapPolicyType::VertexParametersType>* VertexShader;
	TBasePassForClusteredShadingPSPolicyParamType<typename LightMapPolicyType::PixelParametersType>* PixelShader;
	FBaseHS* HullShader;
	FBaseDS* DomainShader;

	LightMapPolicyType LightMapPolicy;
	EBlendMode BlendMode;
	ESceneRenderTargetsMode::Type SceneTextureMode;
	/** Whether or not this policy is compositing editor primitives and needs to depth test against the scene geometry in the base pass pixel shader */
	uint32 bEnableEditorPrimitiveDepthTest : 1;
	/** If we should enable sky light / sky reflections */
	uint32 bEnableSkyLight : 1;
	/** If we should enable the global reflection probe */
	uint32 bEnableReflectionProbe : 1;
	/** If we should enable alpha-to-coverage */
	uint32 bEnableAlphaToCoverage : 1;
};

/**
 * A drawing policy factory for the base pass drawing policy.
 */
class FBasePassClusteredOpaqueDrawingPolicyFactory
{
public:

	enum { bAllowSimpleElements = true };
	struct ContextType 
	{
		ESceneRenderTargetsMode::Type const TextureMode;

		/** Whether or not to perform depth test in the pixel shader */
		bool const bEditorCompositeDepthTest;

		ContextType(bool bInEditorCompositeDepthTest, ESceneRenderTargetsMode::Type InTextureMode) :
			bEditorCompositeDepthTest(bInEditorCompositeDepthTest),
			TextureMode(InTextureMode)
		{}
	};

	static void AddStaticMesh(FRHICommandList& RHICmdList, FScene* Scene, FStaticMesh* StaticMesh);
	static bool DrawDynamicMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		const ContextType& DrawingContext,
		const FMeshBatch& Mesh,
		bool bBackFace,
		bool bPreFog,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId,
		const bool bIsInstancedStereo = false
		);
};

/** Processes a base pass mesh using an unknown light map policy */
template<typename ProcessActionType>
void ProcessBasePassMeshForClusteredShading(
	FRHICommandList& RHICmdList,
	const FProcessBasePassMeshParameters& Parameters,
	const ProcessActionType& Action
	)
{
	// Check for a cached light-map.
	const bool bIsLitMaterial = Parameters.ShadingModel != MSM_Unlit;
	if (bIsLitMaterial)
	{
		bool const bAllowStaticLighting = true;

		const FLightSceneInfo* SimpleDirectionalLight = Action.GetSimpleDirectionalLight();
		// Force these on in "fast iteration mode" to reduce the # of shaders we need to compile.
		bool const bHasLight = SimpleDirectionalLight != nullptr || CLUSTERED_FAST_ITERATION;
		bool const bHasCSM = bHasLight && Action.GetSimpleDirectionalLightHasCSM() || CLUSTERED_FAST_ITERATION;

		// Lightmap path
		if (bAllowStaticLighting 
			&& Parameters.Mesh.LCI != nullptr
			&& Parameters.Mesh.LCI->GetLightMapInteraction(Parameters.FeatureLevel).GetType() == LMIT_Texture)
		{
			// Clustered currently only does HQ
			checkSlow(AllowHighQualityLightmaps(Parameters.FeatureLevel) 
				&& Parameters.Mesh.LCI->GetLightMapInteraction(Parameters.FeatureLevel).AllowsHighQualityLightmaps());

			if (Parameters.Mesh.LCI != nullptr
				&& Parameters.Mesh.LCI->GetShadowMapInteraction().GetType() == SMIT_Texture)
			{
				if (bHasCSM)
				{
					// Light+CSM+Lightmap+DFShadows
					Action.template Process<FUniformLightMapPolicy>( RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MDL_CSM_DFS_HQLM), Parameters.Mesh.LCI );
				}
				else if (bHasLight)
				{
					// Light+Lightmap+DFShadows
					Action.template Process<FUniformLightMapPolicy>( RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MDL_DFS_HQLM), Parameters.Mesh.LCI);
				}
				else
				{
					// Lightmap+DFShadows
					Action.template Process<FUniformLightMapPolicy>( RHICmdList, Parameters, FUniformLightMapPolicy(LMP_DFS_HQLM), Parameters.Mesh.LCI);
				}
			}
			else
			{
				if (bHasCSM)
				{
					// Light+CSM+Lightmap
					Action.template Process<FUniformLightMapPolicy>( RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MDL_CSM_HQLM), Parameters.Mesh.LCI );
				}
				else if (bHasLight)
				{
					// Light+Lightmap
					Action.template Process<FUniformLightMapPolicy>( RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MDL_HQLM), Parameters.Mesh.LCI);
				}
				else
				{
					// Lightmap
					Action.template Process<FUniformLightMapPolicy>( RHICmdList, Parameters, FUniformLightMapPolicy(LMP_HQLM), Parameters.Mesh.LCI);
				}
			}

			// Exit to avoid NoLightmapPolicy
			return;
		}
		else if (IsIndirectLightingCacheAllowed(Parameters.FeatureLevel)
			&& Parameters.PrimitiveSceneProxy
			// Movable objects need to get their GI from the indirect lighting cache
			&& Parameters.PrimitiveSceneProxy->IsMovable())
		{
			// Oculus forward TODO: volume indirect support for large objects... so far not a lot of win from 
			// some simple tests.  Maybe if we have more dramatic environments with more noticeable bounce.

			if (bHasCSM)
			{
				// Light+CSM+SHIndirectPoint
				Action.template Process<FUniformLightMapPolicy>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MDL_CSM_SHINDPT), Parameters.Mesh.LCI);
			}
			else if (bHasLight)
			{
				// Light+SHIndirectPoint
				Action.template Process<FUniformLightMapPolicy>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MDL_SHINDPT), Parameters.Mesh.LCI);
			}
			else
			{
				// SHIndirectPoint
				Action.template Process<FUniformLightMapPolicy>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_SHINDPT), Parameters.Mesh.LCI);
			}

			// Exit to avoid NoLightmapPolicy
			return;
		}
		else if (bHasLight)
		{
			// final determination of whether CSMs are rendered can be view dependent, thus we always need to clear the CSMs even if we're not going to render to them based on the condition below.
			if (bHasCSM)
			{
				// Light+CSM
				Action.template Process<FUniformLightMapPolicy>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MDL_CSM), Parameters.Mesh.LCI);
			}
			else
			{
				// Light
				Action.template Process<FUniformLightMapPolicy>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MDL), Parameters.Mesh.LCI);
			}

			// Exit to avoid NoLightmapPolicy
			return;
		}
	}

	// Default to NoLightmapPolicy
	Action.template Process<FUniformLightMapPolicy>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_NO_LIGHTMAP), Parameters.Mesh.LCI);
}
