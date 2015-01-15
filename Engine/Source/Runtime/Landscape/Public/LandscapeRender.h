// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
LandscapeRender.h: New terrain rendering
=============================================================================*/

#pragma once

#include "UniformBuffer.h"

#include "LandscapeComponent.h"
#include "LandscapeProxy.h"

#include "PrimitiveSceneProxy.h"

#include "LightMap.h"
#include "MeshBatch.h"
#include "ShadowMap.h"

// This defines the number of border blocks to surround terrain by when generating lightmaps
#define TERRAIN_PATCH_EXPAND_SCALAR	1

#define LANDSCAPE_NEIGHBOR_NUM	4

#define LANDSCAPE_LOD_LEVELS 8
#define LANDSCAPE_MAX_SUBSECTION_NUM 2

// Forward declarations
class FLandscapeComponentSceneProxy;

#if WITH_EDITOR
namespace ELandscapeViewMode
{
	enum Type
	{
		Invalid = -1,
		/** Color only */
		Normal = 0,
		EditLayer,
		/** Layer debug only */
		DebugLayer,
		LayerDensity,
		LOD,
		WireframeOnTop,
	};
}

extern LANDSCAPE_API ELandscapeViewMode::Type GLandscapeViewMode;

namespace ELandscapeEditRenderMode
{
	enum Type
	{
		None = 0x0,
		Gizmo = 0x1,
		SelectRegion = 0x2,
		SelectComponent = 0x4,
		Select = SelectRegion | SelectComponent,
		Mask = 0x8,
		InvertedMask = 0x10, // Should not be overlapped with other bits 
		BitMaskForMask = Mask | InvertedMask,

	};
}

LANDSCAPE_API extern bool GLandscapeEditModeActive;
LANDSCAPE_API extern int32 GLandscapeEditRenderMode;
LANDSCAPE_API extern int32 GLandscapePreviewMeshRenderMode;
LANDSCAPE_API extern UMaterial* GLayerDebugColorMaterial;
LANDSCAPE_API extern UMaterialInstanceConstant* GSelectionColorMaterial;
LANDSCAPE_API extern UMaterialInstanceConstant* GSelectionRegionMaterial;
LANDSCAPE_API extern UMaterialInstanceConstant* GMaskRegionMaterial;
LANDSCAPE_API extern UTexture2D* GLandscapeBlackTexture;
#endif


/** The uniform shader parameters for a landscape draw call. */
BEGIN_UNIFORM_BUFFER_STRUCT(FLandscapeUniformShaderParameters, LANDSCAPE_API)
/** vertex shader parameters */
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, HeightmapUVScaleBias)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, WeightmapUVScaleBias)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, LandscapeLightmapScaleBias)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, SubsectionSizeVertsLayerUVPan)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, SubsectionOffsetParams)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, LightmapSubsectionOffsetParams)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FMatrix, LocalToWorldNoScaling)
END_UNIFORM_BUFFER_STRUCT(FLandscapeUniformShaderParameters)

/* Data needed for the landscape vertex factory to set the render state for an individual batch element */
struct FLandscapeBatchElementParams
{
	const TUniformBuffer<FLandscapeUniformShaderParameters>* LandscapeUniformShaderParametersResource;
	const FMatrix* LocalToWorldNoScalingPtr;

	// LOD calculation-related params
	const FLandscapeComponentSceneProxy* SceneProxy;
	int32 SubX;
	int32 SubY;
	int32	CurrentLOD;
};

class FLandscapeElementParamArray : public FOneFrameResource
{
public:
	TArray<FLandscapeBatchElementParams, SceneRenderingAllocator> ElementParams;
};

/** Pixel shader parameters for use with FLandscapeVertexFactory */
class FLandscapeVertexFactoryPixelShaderParameters : public FVertexFactoryShaderParameters
{
public:
	/**
	* Bind shader constants by name
	* @param	ParameterMap - mapping of named shader constants to indices
	*/
	virtual void Bind(const FShaderParameterMap& ParameterMap);

	/**
	* Serialize shader params to an archive
	* @param	Ar - archive to serialize to
	*/
	virtual void Serialize(FArchive& Ar);

	/**
	* Set any shader data specific to this vertex factory
	*/
	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* PixelShader, const FVertexFactory* VertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const override;

	virtual uint32 GetSize() const override
	{
		return sizeof(*this);
	}

private:
	FShaderResourceParameter NormalmapTextureParameter;
	FShaderResourceParameter NormalmapTextureParameterSampler;
	FShaderParameter LocalToWorldNoScalingParameter;
};

/** vertex factory for VTF-heightmap terrain  */
class FLandscapeVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FLandscapeVertexFactory);

public:

	FLandscapeVertexFactory() {}

	virtual ~FLandscapeVertexFactory()
	{
		// can only be destroyed from the render thread
		ReleaseResource();
	}

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

	struct DataType
	{
		/** The stream to read the vertex position from. */
		FVertexStreamComponent PositionComponent;
	};

	/**
	* Should we cache the material's shadertype on this platform with this vertex factory?
	*/
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FShaderType* ShaderType)
	{
		// only compile landscape materials for landscape vertex factory
		// The special engine materials must be compiled for the landscape vertex factory because they are used with it for wireframe, etc.
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) &&
			(Material->IsUsedWithLandscape() || Material->IsSpecialEngineMaterial());
	}

	/**
	* Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	*/
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);

	/**
	* Copy the data from another vertex factory
	* @param Other - factory to copy from
	*/
	void Copy(const FLandscapeVertexFactory& Other);

	// FRenderResource interface.
	virtual void InitRHI() override;

	static bool SupportsTessellationShaders() { return true; }

	/**
	 * An implementation of the interface used by TSynchronizedResource to update the resource with new data from the game thread.
	 */
	void SetData(const DataType& InData)
	{
		Data = InData;
		UpdateRHI();
	}

	virtual uint64 GetStaticBatchElementVisibility(const FSceneView& View, const FMeshBatch* Batch) const override;

	/** stream component data bound to this vertex factory */
	DataType Data;
};


/** vertex factory for VTF-heightmap terrain  */
class FLandscapeXYOffsetVertexFactory : public FLandscapeVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FLandscapeXYOffsetVertexFactory);

public:
	FLandscapeXYOffsetVertexFactory() {}
	virtual ~FLandscapeXYOffsetVertexFactory() {}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);
};

struct FLandscapeVertex
{
	float VertexX;
	float VertexY;
	float SubX;
	float SubY;
};

//
// FLandscapeVertexBuffer
//
class FLandscapeVertexBuffer : public FVertexBuffer
{
	int32 SubsectionSizeVerts;
	int32 NumSubsections;
public:

	/** Constructor. */
	FLandscapeVertexBuffer(int32 InSubsectionSizeVerts, int32 InNumSubsections)
		: SubsectionSizeVerts(InSubsectionSizeVerts)
		, NumSubsections(InNumSubsections)
	{
		InitResource();
	}

	/** Destructor. */
	virtual ~FLandscapeVertexBuffer()
	{
		ReleaseResource();
	}

	/**
	* Initialize the RHI for this rendering resource
	*/
	virtual void InitRHI() override;
};


//
// FLandscapeSharedAdjacencyIndexBuffer
//
class FLandscapeSharedAdjacencyIndexBuffer : public FRefCountedObject
{
public:
	FLandscapeSharedAdjacencyIndexBuffer(class FLandscapeSharedBuffers* SharedBuffer);
	virtual ~FLandscapeSharedAdjacencyIndexBuffer();

	TArray<FIndexBuffer*> IndexBuffers; // For tessellation
};

//
// FLandscapeSharedBuffers
//
class FLandscapeSharedBuffers : public FRefCountedObject
{
public:
	struct FLandscapeIndexRanges
	{
		int32 MinIndex[LANDSCAPE_MAX_SUBSECTION_NUM][LANDSCAPE_MAX_SUBSECTION_NUM];
		int32 MaxIndex[LANDSCAPE_MAX_SUBSECTION_NUM][LANDSCAPE_MAX_SUBSECTION_NUM];
		int32 MinIndexFull;
		int32 MaxIndexFull;
	};

	int32 SharedBuffersKey;
	int32 NumIndexBuffers;
	int32 SubsectionSizeVerts;
	int32 NumSubsections;

	FLandscapeVertexFactory* VertexFactory;
	FLandscapeVertexBuffer* VertexBuffer;
	FIndexBuffer** IndexBuffers;
	FLandscapeIndexRanges* IndexRanges;
	FLandscapeSharedAdjacencyIndexBuffer* AdjacencyIndexBuffers;
	bool bUse32BitIndices;

	FLandscapeSharedBuffers(int32 SharedBuffersKey, int32 SubsectionSizeQuads, int32 NumSubsections, ERHIFeatureLevel::Type InFeatureLevel, bool bRequiresAdjacencyInformation);

	template <typename INDEX_TYPE>
	void CreateIndexBuffers(ERHIFeatureLevel::Type InFeatureLevel, bool bRequiresAdjacencyInformation);

	virtual ~FLandscapeSharedBuffers();
};

//
// FLandscapeEditToolRenderData
//
struct FLandscapeEditToolRenderData
{
	enum
	{
		ST_NONE = 0,
		ST_COMPONENT = 1,
		ST_REGION = 2,
		// = 4...
	};

	FLandscapeEditToolRenderData(ULandscapeComponent* InComponent)
		: ToolMaterial(NULL),
		GizmoMaterial(NULL),
		LandscapeComponent(InComponent),
		SelectedType(ST_NONE),
		DebugChannelR(INDEX_NONE),
		DebugChannelG(INDEX_NONE),
		DebugChannelB(INDEX_NONE),
		DataTexture(NULL)
	{}

	// Material used to render the tool.
	UMaterialInterface* ToolMaterial;
	// Material used to render the gizmo selection region...
	UMaterialInterface* GizmoMaterial;

	ULandscapeComponent* LandscapeComponent;

	// Component is selected
	int32 SelectedType;
	int32 DebugChannelR, DebugChannelG, DebugChannelB;
	UTexture2D* DataTexture; // Data texture other than height/weight

#if WITH_EDITOR
	void UpdateDebugColorMaterial();
	void UpdateSelectionMaterial(int32 InSelectedType);
#endif

	// Game thread update
	LANDSCAPE_API void Update(UMaterialInterface* InNewToolMaterial);
	LANDSCAPE_API void UpdateGizmo(UMaterialInterface* InNewGizmoMaterial);
	// Allows game thread to queue the deletion by the render thread
	void Cleanup();
};

//
// FLandscapeComponentSceneProxy
//
class FLandscapeComponentSceneProxy : public FPrimitiveSceneProxy
{
	friend class FLandscapeSharedBuffers;

	// Key to uniquely identify the landscape to find the correct render proxy map
	class FLandscapeKey
	{
		const UWorld* World;
		const FGuid Guid;
	public:
		FLandscapeKey(const UWorld* InWorld, const FGuid& InGuid)
		: World(InWorld)
		, Guid(InGuid)
		{}

		friend inline uint32 GetTypeHash(const FLandscapeKey& LandscapeKey)
		{
			return HashCombine(GetTypeHash(LandscapeKey.World), GetTypeHash(LandscapeKey.Guid));
		}

		friend bool operator==(const FLandscapeKey& A, const FLandscapeKey& B)
		{
			return A.World == B.World && A.Guid == B.Guid;
		}
	};

	class FLandscapeLCI final : public FLightCacheInterface
	{
	public:
		/** Initialization constructor. */
		FLandscapeLCI(const ULandscapeComponent* InComponent)
		{
			LightMap = InComponent->LightMap;
			ShadowMap = InComponent->ShadowMap;
			IrrelevantLights = InComponent->IrrelevantLights;
		}

		// FLightCacheInterface
		virtual FLightInteraction GetInteraction(const FLightSceneProxy* LightSceneProxy) const;

		virtual FLightMapInteraction GetLightMapInteraction(ERHIFeatureLevel::Type InFeatureLevel) const
		{
			return LightMap ? LightMap->GetInteraction(InFeatureLevel) : FLightMapInteraction();
		}

		virtual FShadowMapInteraction GetShadowMapInteraction() const
		{
			return ShadowMap ? ShadowMap->GetInteraction() : FShadowMapInteraction();
		}

	private:
		/** The light-map used by the element. */
		const FLightMap* LightMap;

		/** The shadowmap used by the element. */
		const FShadowMap* ShadowMap;

		TArray<FGuid> IrrelevantLights;
	};

protected:
	FLandscapeKey				LandscapeKey;
	bool						bAddedToSceneProxyMap;
	int8						MaxLOD;
	int8						NumSubsections;
	int16						SubsectionSizeQuads;
	int16						SubsectionSizeVerts;
	int16						ComponentSizeQuads;	// Size of component in quads
	int16						ComponentSizeVerts;
	uint8						StaticLightingLOD;
	float						StaticLightingResolution;
	FIntPoint					SectionBase;
	FIntPoint					ComponentBase;
	FMatrix						LocalToWorldNoScaling;

	// Storage for static draw list batch params
	TArray<FLandscapeBatchElementParams> StaticBatchParamArray;

	// Precomputed values
	float					LODDistance;
	float					DistDiff;

	FVector4 WeightmapScaleBias;
	float WeightmapSubsectionOffset;
	TArray<UTexture2D*> WeightmapTextures;
	int8 NumWeightmapLayerAllocations;

	UTexture2D* NormalmapTexture; // PC : Heightmap, Mobile : Weightmap

	UTexture2D* HeightmapTexture; // PC : Heightmap, Mobile : Weightmap
	FVector4 HeightmapScaleBias;
	float HeightmapSubsectionOffsetU;
	float HeightmapSubsectionOffsetV;

	UTexture2D* XYOffsetmapTexture;

	bool						bRequiresAdjacencyInformation;
	uint32						SharedBuffersKey;
	FLandscapeSharedBuffers*	SharedBuffers;
	FLandscapeVertexFactory*	VertexFactory;

	UMaterialInterface* MaterialInterface;
	FMaterialRelevance MaterialRelevance;

	// Reference counted vertex and index buffer shared among all landscape scene proxies of the same component size
	// Key is the component size and number of subsections.
	static TMap<uint32, FLandscapeSharedBuffers*> SharedBuffersMap;
	static TMap<uint32, FLandscapeSharedAdjacencyIndexBuffer*> SharedAdjacencyIndexBufferMap;

	// Map of currently registered landscape proxies, used to register with our neighbors
	static TMap<FLandscapeKey, TMap<FIntPoint, const FLandscapeComponentSceneProxy*> > SharedSceneProxyMap;

	FLandscapeEditToolRenderData* EditToolRenderData;

	// FLightCacheInterface
	TUniquePtr<FLandscapeLCI> ComponentLightInfo;

	const ULandscapeComponent* LandscapeComponent;

	int8					ForcedLOD;
	int8					LODBias;

	// Pointer to our neighbor's scene proxies in NWES order (nullptr if there is currently no neighbor)
	mutable const FLandscapeComponentSceneProxy* Neighbors[4];

	ELandscapeLODFalloff::Type LODFalloff;

	TUniformBuffer<FLandscapeUniformShaderParameters> LandscapeUniformShaderParameters;

	// Cached versions of these
	FMatrix					WorldToLocal;

	virtual ~FLandscapeComponentSceneProxy();

public:
	// constructor
	FLandscapeComponentSceneProxy(ULandscapeComponent* InComponent, FLandscapeEditToolRenderData* InEditToolRenderData);

	// FPrimitiveSceneProxy interface.
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	virtual uint32 GetMemoryFootprint() const override { return(sizeof(*this) + GetAllocatedSize()); }
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) override;
	virtual bool CanBeOccluded() const override;
	virtual void GetLightRelevance(const FLightSceneProxy* LightSceneProxy, bool& bDynamic, bool& bRelevant, bool& bLightMapped, bool& bShadowMapped) const override;
	virtual void OnTransformChanged() override;
	virtual void CreateRenderThreadResources() override;

	friend class ULandscapeComponent;
	friend class FLandscapeVertexFactoryVertexShaderParameters;
	friend class FLandscapeXYOffsetVertexFactoryVertexShaderParameters;
	friend class FLandscapeVertexFactoryPixelShaderParameters;
	friend struct FLandscapeBatchElementParams;
	friend class FLandscapeVertexFactoryMobileVertexShaderParameters;
	friend class FLandscapeVertexFactoryMobilePixelShaderParameters;

	// FLandscapeComponentSceneProxy interface.
	float CalcDesiredLOD(const FSceneView& View, const FVector2D& CameraLocalPos, int32 SubX, int32 SubY) const;
	int32 CalcLODForSubsection(const FSceneView& View, int32 SubX, int32 SubY, const FVector2D& CameraLocalPos) const;
	void CalcLODParamsForSubsection(const FSceneView& View, const FVector2D& CameraLocalPos, int32 SubX, int32 SubY, int32 BatchLOD, float& OutfLOD, FVector4& OutNeighborLODs) const;
	uint64 GetStaticBatchElementVisibility(const FSceneView& View, const FMeshBatch* Batch) const;

	// FLandcapeSceneProxy
	void ChangeLODDistanceFactor_RenderThread(float InLODDistanceFactor);

	virtual void GetHeightfieldRepresentation(UTexture2D*& OutHeightmapTexture, FVector4& OutHeightfieldScaleBias, FVector4& OutMinMaxUV) override
	{
		OutHeightmapTexture = HeightmapTexture;
		OutHeightfieldScaleBias = HeightmapScaleBias;
		// Section base is in terms of quads, convert into texels
		int32 TexelsSectionBaseX = SectionBase.X / SubsectionSizeQuads * SubsectionSizeVerts;
		int32 TexelsSectionBaseY = SectionBase.Y / SubsectionSizeQuads * SubsectionSizeVerts;
		OutMinMaxUV = FVector4(TexelsSectionBaseX * HeightmapScaleBias.X, TexelsSectionBaseY * HeightmapScaleBias.Y, (TexelsSectionBaseX + SubsectionSizeVerts - 1) * HeightmapScaleBias.X, (TexelsSectionBaseY + SubsectionSizeVerts - 1) * HeightmapScaleBias.Y);
	}
};

class FLandscapeDebugMaterialRenderProxy : public FMaterialRenderProxy
{
public:
	const FMaterialRenderProxy* const Parent;
	const UTexture2D* RedTexture;
	const UTexture2D* GreenTexture;
	const UTexture2D* BlueTexture;
	const FLinearColor R;
	const FLinearColor G;
	const FLinearColor B;

	/** Initialization constructor. */
	FLandscapeDebugMaterialRenderProxy(const FMaterialRenderProxy* InParent, const UTexture2D* TexR, const UTexture2D* TexG, const UTexture2D* TexB,
		const FLinearColor& InR, const FLinearColor& InG, const FLinearColor& InB) :
		Parent(InParent),
		RedTexture(TexR),
		GreenTexture(TexG),
		BlueTexture(TexB),
		R(InR),
		G(InG),
		B(InB)
	{}

	// FMaterialRenderProxy interface.
	virtual const FMaterial* GetMaterial(ERHIFeatureLevel::Type FeatureLevel) const
	{
		return Parent->GetMaterial(FeatureLevel);
	}
	virtual bool GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
	{
		if (ParameterName == FName(TEXT("Landscape_RedMask")))
		{
			*OutValue = R;
			return true;
		}
		else if (ParameterName == FName(TEXT("Landscape_GreenMask")))
		{
			*OutValue = G;
			return true;
		}
		else if (ParameterName == FName(TEXT("Landscape_BlueMask")))
		{
			*OutValue = B;
			return true;
		}
		else
		{
			return Parent->GetVectorValue(ParameterName, OutValue, Context);
		}
	}
	virtual bool GetScalarValue(const FName ParameterName, float* OutValue, const FMaterialRenderContext& Context) const
	{
		return Parent->GetScalarValue(ParameterName, OutValue, Context);
	}
	virtual bool GetTextureValue(const FName ParameterName, const UTexture** OutValue, const FMaterialRenderContext& Context) const
	{
		// NOTE: These should be returning black textures when NULL. The material will
		// use a white texture if they are.
		if (ParameterName == FName(TEXT("Landscape_RedTexture")))
		{
			*OutValue = RedTexture;
			return true;
		}
		else if (ParameterName == FName(TEXT("Landscape_GreenTexture")))
		{
			*OutValue = GreenTexture;
			return true;
		}
		else if (ParameterName == FName(TEXT("Landscape_BlueTexture")))
		{
			*OutValue = BlueTexture;
			return true;
		}
		else
		{
			return Parent->GetTextureValue(ParameterName, OutValue, Context);
		}
	}
};

class FLandscapeSelectMaterialRenderProxy : public FMaterialRenderProxy
{
public:
	const FMaterialRenderProxy* const Parent;
	const UTexture2D* SelectTexture;

	/** Initialization constructor. */
	FLandscapeSelectMaterialRenderProxy(const FMaterialRenderProxy* InParent, const UTexture2D* InTexture) :
		Parent(InParent),
		SelectTexture(InTexture)
	{}

	// FMaterialRenderProxy interface.
	virtual const FMaterial* GetMaterial(ERHIFeatureLevel::Type FeatureLevel) const
	{
		return Parent->GetMaterial(FeatureLevel);
	}
	virtual bool GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
	{
		if (ParameterName == FName(TEXT("HighlightColor")))
		{
			*OutValue = FLinearColor(1.f, 0.5f, 0.5f);
			return true;
		}
		else
		{
			return Parent->GetVectorValue(ParameterName, OutValue, Context);
		}
	}
	virtual bool GetScalarValue(const FName ParameterName, float* OutValue, const FMaterialRenderContext& Context) const
	{
		return Parent->GetScalarValue(ParameterName, OutValue, Context);
	}
	virtual bool GetTextureValue(const FName ParameterName, const UTexture** OutValue, const FMaterialRenderContext& Context) const
	{
		if (ParameterName == FName(TEXT("SelectedData")))
		{
			*OutValue = SelectTexture;
			return true;
		}
		else
		{
			return Parent->GetTextureValue(ParameterName, OutValue, Context);
		}
	}
};

class FLandscapeMaskMaterialRenderProxy : public FMaterialRenderProxy
{
public:
	const FMaterialRenderProxy* const Parent;
	const UTexture2D* SelectTexture;
	const bool bInverted;

	/** Initialization constructor. */
	FLandscapeMaskMaterialRenderProxy(const FMaterialRenderProxy* InParent, const UTexture2D* InTexture, const bool InbInverted) :
		Parent(InParent),
		SelectTexture(InTexture),
		bInverted(InbInverted)
	{}

	// FMaterialRenderProxy interface.
	virtual const FMaterial* GetMaterial(ERHIFeatureLevel::Type FeatureLevel) const
	{
		return Parent->GetMaterial(FeatureLevel);
	}
	virtual bool GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
	{
		return Parent->GetVectorValue(ParameterName, OutValue, Context);
	}
	virtual bool GetScalarValue(const FName ParameterName, float* OutValue, const FMaterialRenderContext& Context) const
	{
		if (ParameterName == FName(TEXT("bInverted")))
		{
			*OutValue = bInverted;
			return true;
		}
		return Parent->GetScalarValue(ParameterName, OutValue, Context);
	}
	virtual bool GetTextureValue(const FName ParameterName, const UTexture** OutValue, const FMaterialRenderContext& Context) const
	{
		if (ParameterName == FName(TEXT("SelectedData")))
		{
			*OutValue = SelectTexture;
			return true;
		}
		else
		{
			return Parent->GetTextureValue(ParameterName, OutValue, Context);
		}
	}
};

namespace
{
	// LightmapRes: Multiplier of lightmap size relative to landscape size
	// X: (Output) PatchExpandCountX (at Lighting LOD)
	// Y: (Output) PatchExpandCountY (at Lighting LOD)
	// ComponentSize: Component size in patches (at LOD 0)
	// LigtmapSize: Size desired for lightmap (texels)
	// DesiredSize: (Output) Recommended lightmap size (texels)
	// return: LightMapRatio
	static float GetTerrainExpandPatchCount(float LightMapRes, int32& X, int32& Y, int32 ComponentSize, int32 LightmapSize, int32& DesiredSize, uint32 LightingLOD)
	{
		if (LightMapRes <= 0) return 0.f;

		// Assuming DXT_1 compression at the moment...
		int32 PixelPaddingX = GPixelFormats[PF_DXT1].BlockSizeX;
		int32 PixelPaddingY = GPixelFormats[PF_DXT1].BlockSizeY;
		int32 PatchExpandCountX = (LightMapRes >= 1.f) ? (PixelPaddingX) / LightMapRes : (PixelPaddingX);
		int32 PatchExpandCountY = (LightMapRes >= 1.f) ? (PixelPaddingY) / LightMapRes : (PixelPaddingY);

		X = FMath::Max<int32>(1, PatchExpandCountX >> LightingLOD);
		Y = FMath::Max<int32>(1, PatchExpandCountY >> LightingLOD);

		DesiredSize = (LightMapRes >= 1.f) ? FMath::Min<int32>((int32)((ComponentSize + 1) * LightMapRes), 4096) : FMath::Min<int32>((int32)((LightmapSize)* LightMapRes), 4096);
		int32 CurrentSize = (LightMapRes >= 1.f) ? FMath::Min<int32>((int32)((2 * (X << LightingLOD) + ComponentSize + 1) * LightMapRes), 4096) : FMath::Min<int32>((int32)((2 * (X << LightingLOD) + LightmapSize) * LightMapRes), 4096);

		// Find proper Lightmap Size
		if (CurrentSize > DesiredSize)
		{
			// Find maximum bit
			int32 PriorSize = DesiredSize;
			while (DesiredSize > 0)
			{
				PriorSize = DesiredSize;
				DesiredSize = DesiredSize & ~(DesiredSize & ~(DesiredSize - 1));
			}

			DesiredSize = PriorSize << 1; // next bigger size
			if (CurrentSize * CurrentSize <= ((PriorSize * PriorSize) << 1))
			{
				DesiredSize = PriorSize;
			}
		}

		int32 DestSize = (float)DesiredSize / CurrentSize * (ComponentSize*LightMapRes);
		float LightMapRatio = (float)DestSize / (ComponentSize*LightMapRes) * CurrentSize / DesiredSize;
		return LightMapRatio;
	}
}
