// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneRendering.cpp: Scene rendering.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "RefCounting.h"
#include "SceneOcclusion.h"
#include "ScreenRendering.h"
#include "SceneFilterRendering.h"
#include "SceneUtils.h"

/*-----------------------------------------------------------------------------
	Globals
-----------------------------------------------------------------------------*/

int32 GAllowPrecomputedVisibility = 1;
static FAutoConsoleVariableRef CVarAllowPrecomputedVisibility(
	TEXT("r.AllowPrecomputedVisibility"),
	GAllowPrecomputedVisibility,
	TEXT("If zero, precomputed visibility will not be used to cull primitives."),
	ECVF_RenderThreadSafe
	);

static int32 GShowPrecomputedVisibilityCells = 0;
static FAutoConsoleVariableRef CVarShowPrecomputedVisibilityCells(
	TEXT("r.ShowPrecomputedVisibilityCells"),
	GShowPrecomputedVisibilityCells,
	TEXT("If not zero, draw all precomputed visibility cells."),
	ECVF_RenderThreadSafe
	);

static int32 GShowRelevantPrecomputedVisibilityCells = 0;
static FAutoConsoleVariableRef CVarShowRelevantPrecomputedVisibilityCells(
	TEXT("r.ShowRelevantPrecomputedVisibilityCells"),
	GShowRelevantPrecomputedVisibilityCells,
	TEXT("If not zero, draw relevant precomputed visibility cells only."),
	ECVF_RenderThreadSafe
	);

#define NUM_CUBE_VERTICES 36

/** Random table for occlusion **/
FOcclusionRandomStream GOcclusionRandomStream;

// default, non-instanced shader implementation
IMPLEMENT_SHADER_TYPE(,FOcclusionQueryVS,TEXT("OcclusionQueryVertexShader"),TEXT("Main"),SF_Vertex);

FRenderQueryPool::~FRenderQueryPool()
{
	Release();
}

void FRenderQueryPool::Release()
{
	Queries.Empty();
}

FRenderQueryRHIRef FRenderQueryPool::AllocateQuery()
{
	// Are we out of available render queries?
	if ( Queries.Num() == 0 )
	{
		// Create a new render query.
		return RHICreateRenderQuery(QueryType);
	}

	return Queries.Pop();
}

void FRenderQueryPool::ReleaseQuery(FRHICommandListImmediate& RHICmdList, FRenderQueryRHIRef &Query)
{
	if ( IsValidRef(Query) )
	{
		// Is no one else keeping a refcount to the query?
		if ( Query.GetRefCount() == 1 )
		{
			// Return it to the pool.
			Queries.Add( Query );

#if !PLATFORM_SUPPORTS_RHI_THREAD
			// Tell RHI we don't need the result anymore.
			RHICmdList.ResetRenderQuery(Query);
#endif
		}

		// De-ref without deleting.
		Query = NULL;
	}
}

FGlobalBoundShaderState FDeferredShadingSceneRenderer::OcclusionTestBoundShaderState;

/** 
 * Returns an array of visibility data for the given view position, or NULL if none exists. 
 * The data bits are indexed by VisibilityId of each primitive in the scene.
 * This method decompresses data if necessary and caches it based on the bucket and chunk index in the view state.
 */
const uint8* FSceneViewState::GetPrecomputedVisibilityData(FViewInfo& View, const FScene* Scene)
{
	const uint8* PrecomputedVisibilityData = NULL;
	if (Scene->PrecomputedVisibilityHandler && GAllowPrecomputedVisibility && View.Family->EngineShowFlags.PrecomputedVisibility)
	{
		const FPrecomputedVisibilityHandler& Handler = *Scene->PrecomputedVisibilityHandler;
		FViewElementPDI VisibilityCellsPDI(&View, NULL);

		// Draw visibility cell bounds for debugging if enabled
		if ((GShowPrecomputedVisibilityCells || View.Family->EngineShowFlags.PrecomputedVisibilityCells) && !GShowRelevantPrecomputedVisibilityCells)
		{
			for (int32 BucketIndex = 0; BucketIndex < Handler.PrecomputedVisibilityCellBuckets.Num(); BucketIndex++)
			{
				for (int32 CellIndex = 0; CellIndex < Handler.PrecomputedVisibilityCellBuckets[BucketIndex].Cells.Num(); CellIndex++)
				{
					const FPrecomputedVisibilityCell& CurrentCell = Handler.PrecomputedVisibilityCellBuckets[BucketIndex].Cells[CellIndex];
					// Construct the cell's bounds
					const FBox CellBounds(CurrentCell.Min, CurrentCell.Min + FVector(Handler.PrecomputedVisibilityCellSizeXY, Handler.PrecomputedVisibilityCellSizeXY, Handler.PrecomputedVisibilityCellSizeZ));
					if (View.ViewFrustum.IntersectBox(CellBounds.GetCenter(), CellBounds.GetExtent()))
					{
						DrawWireBox(&VisibilityCellsPDI, CellBounds, FColor(50, 50, 255), SDPG_World);
					}
				}
			}
		}

		// Calculate the bucket that ViewOrigin falls into
		// Cells are hashed into buckets to reduce search time
		const float FloatOffsetX = (View.ViewMatrices.ViewOrigin.X - Handler.PrecomputedVisibilityCellBucketOriginXY.X) / Handler.PrecomputedVisibilityCellSizeXY;
		// FMath::TruncToInt rounds toward 0, we want to always round down
		const int32 BucketIndexX = FMath::Abs((FMath::TruncToInt(FloatOffsetX) - (FloatOffsetX < 0.0f ? 1 : 0)) / Handler.PrecomputedVisibilityCellBucketSizeXY % Handler.PrecomputedVisibilityNumCellBuckets);
		const float FloatOffsetY = (View.ViewMatrices.ViewOrigin.Y -Handler.PrecomputedVisibilityCellBucketOriginXY.Y) / Handler.PrecomputedVisibilityCellSizeXY;
		const int32 BucketIndexY = FMath::Abs((FMath::TruncToInt(FloatOffsetY) - (FloatOffsetY < 0.0f ? 1 : 0)) / Handler.PrecomputedVisibilityCellBucketSizeXY % Handler.PrecomputedVisibilityNumCellBuckets);
		const int32 PrecomputedVisibilityBucketIndex = BucketIndexY * Handler.PrecomputedVisibilityCellBucketSizeXY + BucketIndexX;

		check(PrecomputedVisibilityBucketIndex < Handler.PrecomputedVisibilityCellBuckets.Num());
		const FPrecomputedVisibilityBucket& CurrentBucket = Handler.PrecomputedVisibilityCellBuckets[PrecomputedVisibilityBucketIndex];
		for (int32 CellIndex = 0; CellIndex < CurrentBucket.Cells.Num(); CellIndex++)
		{
			const FPrecomputedVisibilityCell& CurrentCell = CurrentBucket.Cells[CellIndex];
			// Construct the cell's bounds
			const FBox CellBounds(CurrentCell.Min, CurrentCell.Min + FVector(Handler.PrecomputedVisibilityCellSizeXY, Handler.PrecomputedVisibilityCellSizeXY, Handler.PrecomputedVisibilityCellSizeZ));
			// Check if ViewOrigin is inside the current cell
			if (CellBounds.IsInside(View.ViewMatrices.ViewOrigin))
			{
				// Reuse a cached decompressed chunk if possible
				if (CachedVisibilityChunk
					&& CachedVisibilityHandlerId == Scene->PrecomputedVisibilityHandler->GetId()
					&& CachedVisibilityBucketIndex == PrecomputedVisibilityBucketIndex
					&& CachedVisibilityChunkIndex == CurrentCell.ChunkIndex)
				{
					checkSlow(CachedVisibilityChunk->Num() >= CurrentCell.DataOffset + CurrentBucket.CellDataSize);
					PrecomputedVisibilityData = &(*CachedVisibilityChunk)[CurrentCell.DataOffset];
				}
				else
				{
					const FCompressedVisibilityChunk& CompressedChunk = Handler.PrecomputedVisibilityCellBuckets[PrecomputedVisibilityBucketIndex].CellDataChunks[CurrentCell.ChunkIndex];
					CachedVisibilityBucketIndex = PrecomputedVisibilityBucketIndex;
					CachedVisibilityChunkIndex = CurrentCell.ChunkIndex;
					CachedVisibilityHandlerId = Scene->PrecomputedVisibilityHandler->GetId();

					if (CompressedChunk.bCompressed)
					{
						// Decompress the needed visibility data chunk
						DecompressedVisibilityChunk.Reset();
						DecompressedVisibilityChunk.AddUninitialized(CompressedChunk.UncompressedSize);
						verify(FCompression::UncompressMemory(
							COMPRESS_ZLIB, 
							DecompressedVisibilityChunk.GetData(),
							CompressedChunk.UncompressedSize,
							CompressedChunk.Data.GetData(),
							CompressedChunk.Data.Num()));
						CachedVisibilityChunk = &DecompressedVisibilityChunk;
					}
					else
					{
						CachedVisibilityChunk = &CompressedChunk.Data;
					}

					checkSlow(CachedVisibilityChunk->Num() >= CurrentCell.DataOffset + CurrentBucket.CellDataSize);
					// Return a pointer to the cell containing ViewOrigin's decompressed visibility data
					PrecomputedVisibilityData = &(*CachedVisibilityChunk)[CurrentCell.DataOffset];
				}

				if (GShowRelevantPrecomputedVisibilityCells)
				{
					// Draw the currently used visibility cell with green wireframe for debugging
					DrawWireBox(&VisibilityCellsPDI, CellBounds, FColor(50, 255, 50), SDPG_Foreground);
				}
				else
				{
					break;
				}
			}
			else if (GShowRelevantPrecomputedVisibilityCells)
			{
				// Draw all cells in the current visibility bucket as blue wireframe
				DrawWireBox(&VisibilityCellsPDI, CellBounds, FColor(50, 50, 255), SDPG_World);
			}
		}
	}
	return PrecomputedVisibilityData;
}

void FSceneViewState::TrimOcclusionHistory(FRHICommandListImmediate& RHICmdList, float MinHistoryTime, float MinQueryTime, int32 FrameNumber)
{
	// Only trim every few frames, since stale entries won't cause problems
	if (FrameNumber % 6 == 0)
	{
		for(TSet<FPrimitiveOcclusionHistory,FPrimitiveOcclusionHistoryKeyFuncs>::TIterator PrimitiveIt(PrimitiveOcclusionHistorySet);
			PrimitiveIt;
			++PrimitiveIt
			)
		{
			// If the primitive has an old pending occlusion query, release it.
			if(PrimitiveIt->LastConsideredTime < MinQueryTime)
			{
				PrimitiveIt->ReleaseQueries(RHICmdList, OcclusionQueryPool);
			}

			// If the primitive hasn't been considered for visibility recently, remove its history from the set.
			if(PrimitiveIt->LastConsideredTime < MinHistoryTime)
			{
				PrimitiveIt.RemoveCurrent();
			}
		}
	}
}

bool FSceneViewState::IsShadowOccluded(FRHICommandListImmediate& RHICmdList, FPrimitiveComponentId PrimitiveId, const ULightComponent* Light, int32 SplitIndex, bool bTranslucentShadow) const
{
	// Find the shadow's occlusion query from the previous frame.
	const FSceneViewState::FProjectedShadowKey Key(PrimitiveId, Light, SplitIndex, bTranslucentShadow);

#if BUFFERED_OCCLUSION_QUERIES
	// Get the oldest occlusion query	
	const uint32 QueryIndex = FOcclusionQueryHelpers::GetQueryLookupIndex(PendingPrevFrameNumber, NumBufferedFrames);
	const FSceneViewState::ShadowKeyOcclusionQueryMap& ShadowOcclusionQueryMap = ShadowOcclusionQueryMaps[QueryIndex];	
#endif


	const FRenderQueryRHIRef* Query = ShadowOcclusionQueryMap.Find(Key);

	// Read the occlusion query results.
	uint64 NumSamples = 0;
	// Only block on the query if not running SLI
	const bool bWaitOnQuery = GNumActiveGPUsForRendering == 1;

	if (Query && RHICmdList.GetRenderQueryResult(*Query, NumSamples, bWaitOnQuery))
	{
		// If the shadow's occlusion query didn't have any pixels visible the previous frame, it's occluded.
		return NumSamples == 0;
	}
	else
	{
		// If the shadow wasn't queried the previous frame, it isn't occluded.

		return false;
	}
}

void FSceneViewState::Destroy()
{
	if ( IsInGameThread() )
	{
		// Release the occlusion query data.
		BeginReleaseResource(this);

		// Defer deletion of the view state until the rendering thread is done with it.
		BeginCleanup(this);
	}
	else
	{
		ReleaseResource();
		FinishCleanup();
	}
}

SIZE_T FSceneViewState::GetSizeBytes() const
{
#if BUFFERED_OCCLUSION_QUERIES
	uint32 ShadowOcclusionQuerySize = ShadowOcclusionQueryMaps.GetAllocatedSize();
	for (int32 i = 0; i < ShadowOcclusionQueryMaps.Num(); ++i)
	{
		ShadowOcclusionQuerySize += ShadowOcclusionQueryMaps[i].GetAllocatedSize();
	}
#else
	uint32 ShadowOcclusionQuerySize = ShadowOcclusionQueryMap.GetAllocatedSize();
#endif

	return sizeof(*this) 
		+ ShadowOcclusionQuerySize
		+ ParentPrimitives.GetAllocatedSize() 
		+ PrimitiveFadingStates.GetAllocatedSize()
		+ PrimitiveOcclusionHistorySet.GetAllocatedSize();
}

class FOcclusionQueryIndexBuffer : public FIndexBuffer
{
public:
	virtual void InitRHI() override
	{
		const uint32 MaxBatchedPrimitives = FOcclusionQueryBatcher::OccludedPrimitiveQueryBatchSize;
		const uint32 Stride = sizeof(uint16);
		const uint32 SizeInBytes = MaxBatchedPrimitives * NUM_CUBE_VERTICES * Stride;

		FRHIResourceCreateInfo CreateInfo;
		IndexBufferRHI = RHICreateIndexBuffer(Stride,SizeInBytes,BUF_Static,CreateInfo);
		uint16* RESTRICT Indices = (uint16*)RHILockIndexBuffer(IndexBufferRHI,0,SizeInBytes,RLM_WriteOnly);
		for(uint32 PrimitiveIndex = 0;PrimitiveIndex < MaxBatchedPrimitives;PrimitiveIndex++)
		{
			for(int32 Index = 0;Index < NUM_CUBE_VERTICES;Index++)
			{
				Indices[PrimitiveIndex * NUM_CUBE_VERTICES + Index] = PrimitiveIndex * 8 + GCubeIndices[Index];
			}
		}
		RHIUnlockIndexBuffer(IndexBufferRHI);
	}
};
TGlobalResource<FOcclusionQueryIndexBuffer> GOcclusionQueryIndexBuffer;

FOcclusionQueryBatcher::FOcclusionQueryBatcher(class FSceneViewState* ViewState,uint32 InMaxBatchedPrimitives)
:	CurrentBatchOcclusionQuery(NULL)
,	MaxBatchedPrimitives(InMaxBatchedPrimitives)
,	NumBatchedPrimitives(0)
,	OcclusionQueryPool(ViewState ? &ViewState->OcclusionQueryPool : NULL)
{}

FOcclusionQueryBatcher::~FOcclusionQueryBatcher()
{
	check(!BatchOcclusionQueries.Num());
}

void FOcclusionQueryBatcher::Flush(FRHICommandListImmediate& RHICmdList)
{
	if(BatchOcclusionQueries.Num())
	{
		FMemMark MemStackMark(FMemStack::Get());

		// Create the indices for MaxBatchedPrimitives boxes.
		FIndexBufferRHIParamRef IndexBufferRHI = GOcclusionQueryIndexBuffer.IndexBufferRHI;

		// Draw the batches.
		for(int32 BatchIndex = 0, NumBatches = BatchOcclusionQueries.Num();BatchIndex < NumBatches;BatchIndex++)
		{
			FOcclusionBatch& Batch = BatchOcclusionQueries[BatchIndex];
			FRenderQueryRHIParamRef BatchOcclusionQuery = Batch.Query;
			FVertexBufferRHIParamRef VertexBufferRHI = Batch.VertexAllocation.VertexBuffer->VertexBufferRHI;
			uint32 VertexBufferOffset = Batch.VertexAllocation.VertexOffset;
			const int32 NumPrimitivesThisBatch = (BatchIndex != (NumBatches-1)) ? MaxBatchedPrimitives : NumBatchedPrimitives;
				
			RHICmdList.BeginRenderQuery(BatchOcclusionQuery);
			RHICmdList.SetStreamSource(0, VertexBufferRHI, sizeof(FVector), VertexBufferOffset);
			RHICmdList.DrawIndexedPrimitive(
				IndexBufferRHI,
				PT_TriangleList,
				/*BaseVertexIndex=*/ 0,
				/*MinIndex=*/ 0,
				/*NumVertices=*/ 8 * NumPrimitivesThisBatch,
				/*StartIndex=*/ 0,
				/*NumPrimitives=*/ 12 * NumPrimitivesThisBatch,
				/*NumInstances=*/ 1
				);
			RHICmdList.EndRenderQuery(BatchOcclusionQuery);
		}
		INC_DWORD_STAT_BY(STAT_OcclusionQueries,BatchOcclusionQueries.Num());

		// Reset the batch state.
		BatchOcclusionQueries.Empty(BatchOcclusionQueries.Num());
		CurrentBatchOcclusionQuery = NULL;
	}
}

FRenderQueryRHIParamRef FOcclusionQueryBatcher::BatchPrimitive(const FVector& BoundsOrigin,const FVector& BoundsBoxExtent)
{
	// Check if the current batch is full.
	if(CurrentBatchOcclusionQuery == NULL || NumBatchedPrimitives >= MaxBatchedPrimitives)
	{
		check(OcclusionQueryPool);
		CurrentBatchOcclusionQuery = new(BatchOcclusionQueries) FOcclusionBatch;
		CurrentBatchOcclusionQuery->Query = OcclusionQueryPool->AllocateQuery();
		CurrentBatchOcclusionQuery->VertexAllocation = FGlobalDynamicVertexBuffer::Get().Allocate(MaxBatchedPrimitives * 8 * sizeof(FVector), true);
		check(CurrentBatchOcclusionQuery->VertexAllocation.IsValid());
		NumBatchedPrimitives = 0;
	}

	// Add the primitive's bounding box to the current batch's vertex buffer.
	const FVector PrimitiveBoxMin = BoundsOrigin - BoundsBoxExtent;
	const FVector PrimitiveBoxMax = BoundsOrigin + BoundsBoxExtent;
	float* RESTRICT Vertices = (float*)CurrentBatchOcclusionQuery->VertexAllocation.Buffer;
	Vertices[ 0] = PrimitiveBoxMin.X; Vertices[ 1] = PrimitiveBoxMin.Y; Vertices[ 2] = PrimitiveBoxMin.Z;
	Vertices[ 3] = PrimitiveBoxMin.X; Vertices[ 4] = PrimitiveBoxMin.Y; Vertices[ 5] = PrimitiveBoxMax.Z;
	Vertices[ 6] = PrimitiveBoxMin.X; Vertices[ 7] = PrimitiveBoxMax.Y; Vertices[ 8] = PrimitiveBoxMin.Z;
	Vertices[ 9] = PrimitiveBoxMin.X; Vertices[10] = PrimitiveBoxMax.Y; Vertices[11] = PrimitiveBoxMax.Z;
	Vertices[12] = PrimitiveBoxMax.X; Vertices[13] = PrimitiveBoxMin.Y; Vertices[14] = PrimitiveBoxMin.Z;
	Vertices[15] = PrimitiveBoxMax.X; Vertices[16] = PrimitiveBoxMin.Y; Vertices[17] = PrimitiveBoxMax.Z;
	Vertices[18] = PrimitiveBoxMax.X; Vertices[19] = PrimitiveBoxMax.Y; Vertices[20] = PrimitiveBoxMin.Z;
	Vertices[21] = PrimitiveBoxMax.X; Vertices[22] = PrimitiveBoxMax.Y; Vertices[23] = PrimitiveBoxMax.Z;

	// Bump the batches buffer pointer.
	Vertices += 24;
	CurrentBatchOcclusionQuery->VertexAllocation.Buffer = (uint8*)Vertices;
	NumBatchedPrimitives++;

	return CurrentBatchOcclusionQuery->Query;
}

static void IssueProjectedShadowOcclusionQuery(FRHICommandListImmediate& RHICmdList, FViewInfo& View, const FProjectedShadowInfo& ProjectedShadowInfo, FOcclusionQueryVS* VertexShader)
{	
	FSceneViewState* ViewState = (FSceneViewState*)View.State;

#if BUFFERED_OCCLUSION_QUERIES		
	const uint32 QueryIndex = FOcclusionQueryHelpers::GetQueryIssueIndex(ViewState->PendingPrevFrameNumber, ViewState->NumBufferedFrames);
	FSceneViewState::ShadowKeyOcclusionQueryMap& ShadowOcclusionQueryMap = ViewState->ShadowOcclusionQueryMaps[QueryIndex];
#else
	FSceneViewState::ShadowKeyOcclusionQueryMap& ShadowOcclusionQueryMap = ViewState->ShadowOcclusionQueryMap;
#endif

	// The shadow transforms and view transforms are relative to different origins, so the world coordinates need to
	// be translated.
	const FVector4 PreShadowToPreViewTranslation(View.ViewMatrices.PreViewTranslation - ProjectedShadowInfo.PreShadowTranslation,0);

	// If the shadow frustum is farther from the view origin than the near clipping plane,
	// it can't intersect the near clipping plane.
	const bool bIntersectsNearClippingPlane = ProjectedShadowInfo.ReceiverFrustum.IntersectSphere(
		View.ViewMatrices.ViewOrigin + ProjectedShadowInfo.PreShadowTranslation,
		View.NearClippingDistance * FMath::Sqrt(3.0f)
		);
	if( !bIntersectsNearClippingPlane )
	{

		// Allocate an occlusion query for the primitive from the occlusion query pool.
		const FRenderQueryRHIRef ShadowOcclusionQuery = ViewState->OcclusionQueryPool.AllocateQuery();
		
		VertexShader->SetParameters(RHICmdList, View);

		// Draw the primitive's bounding box, using the occlusion query.
		RHICmdList.BeginRenderQuery(ShadowOcclusionQuery);

		void* VerticesPtr;
		void* IndicesPtr;
		// preallocate memory to fill out with vertices and indices
		RHICmdList.BeginDrawIndexedPrimitiveUP(PT_TriangleList, 12, 8, sizeof(FVector), VerticesPtr, 0, NUM_CUBE_VERTICES, sizeof(uint16), IndicesPtr);
		FVector* Vertices = (FVector*)VerticesPtr;
		uint16* Indices = (uint16*)IndicesPtr;

		// Generate vertices for the shadow's frustum.
		for(uint32 Z = 0;Z < 2;Z++)
		{
			for(uint32 Y = 0;Y < 2;Y++)
			{
				for(uint32 X = 0;X < 2;X++)
				{
					const FVector4 UnprojectedVertex = ProjectedShadowInfo.InvReceiverMatrix.TransformFVector4(
						FVector4(
						(X ? -1.0f : 1.0f),
						(Y ? -1.0f : 1.0f),
						(Z ?  1.0f : 0.0f),
						1.0f
						)
						);
					const FVector ProjectedVertex = UnprojectedVertex / UnprojectedVertex.W + PreShadowToPreViewTranslation;
					Vertices[GetCubeVertexIndex(X,Y,Z)] = ProjectedVertex;
				}
			}
		}

		// we just copy the indices right in
		FMemory::Memcpy(Indices, GCubeIndices, sizeof(GCubeIndices));

		FSceneViewState::FProjectedShadowKey Key(
			ProjectedShadowInfo.ParentSceneInfo ? 
				ProjectedShadowInfo.ParentSceneInfo->PrimitiveComponentId :
				FPrimitiveComponentId(),
			ProjectedShadowInfo.LightSceneInfo->Proxy->GetLightComponent(),
			ProjectedShadowInfo.SplitIndex,
			ProjectedShadowInfo.bTranslucentShadow
			);
		checkSlow(ShadowOcclusionQueryMap.Find(Key) == NULL);
		ShadowOcclusionQueryMap.Add(Key, ShadowOcclusionQuery);

		RHICmdList.EndDrawIndexedPrimitiveUP();
		RHICmdList.EndRenderQuery(ShadowOcclusionQuery);
	}
}




FHZBOcclusionTester::FHZBOcclusionTester()
	: ResultsBuffer( NULL )
{
	SetInvalidFrameNumber();
}

bool FHZBOcclusionTester::IsValidFrame(uint32 FrameNumber) const
{
	return (FrameNumber & FrameNumberMask) == ValidFrameNumber;
}

void FHZBOcclusionTester::SetValidFrameNumber(uint32 FrameNumber)
{
	ValidFrameNumber = FrameNumber & FrameNumberMask;

	checkSlow(!IsInvalidFrame());
}

bool FHZBOcclusionTester::IsInvalidFrame() const
{
	return ValidFrameNumber == InvalidFrameNumber;
}

void FHZBOcclusionTester::SetInvalidFrameNumber()
{
	// this number cannot be set by SetValidFrameNumber()
	ValidFrameNumber = InvalidFrameNumber;

	checkSlow(IsInvalidFrame());
}

void FHZBOcclusionTester::InitDynamicRHI()
{
	if (GetFeatureLevel() >= ERHIFeatureLevel::SM4)
	{
#if PLATFORM_MAC // Workaround radr://16096028 Texture Readback via glReadPixels + PBOs stalls on Nvidia GPUs
		FPooledRenderTargetDesc Desc( FPooledRenderTargetDesc::Create2DDesc( FIntPoint( SizeX, SizeY ), PF_R8G8B8A8, TexCreate_CPUReadback | TexCreate_HideInVisualizeTexture, TexCreate_None, false ) );
#else
		FPooledRenderTargetDesc Desc( FPooledRenderTargetDesc::Create2DDesc( FIntPoint( SizeX, SizeY ), PF_B8G8R8A8, TexCreate_CPUReadback | TexCreate_HideInVisualizeTexture, TexCreate_None, false ) );
#endif
		GRenderTargetPool.FindFreeElement( Desc, ResultsTextureCPU, TEXT("HZBResultsCPU") );
	}
}

void FHZBOcclusionTester::ReleaseDynamicRHI()
{
	if (GetFeatureLevel() >= ERHIFeatureLevel::SM4)
	{
		GRenderTargetPool.FreeUnusedResource( ResultsTextureCPU );
	}
}

uint32 FHZBOcclusionTester::AddBounds( const FVector& BoundsCenter, const FVector& BoundsExtent )
{
	uint32 Index = Primitives.AddUninitialized();
	check( Index < SizeX * SizeY );
	Primitives[ Index ].Center = BoundsCenter;
	Primitives[ Index ].Extent = BoundsExtent;
	return Index;
}

void FHZBOcclusionTester::MapResults(FRHICommandListImmediate& RHICmdList)
{
	check( !ResultsBuffer );

	// hacky (we point to some buffer that is not the right size but we prevent reads from it be having a invalid frame number)
	// First frame
	static uint8 FirstFrameBuffer[] = { 255 };

	if( IsInvalidFrame() )
	{
		ResultsBuffer = FirstFrameBuffer;
	}
	else
	{
		uint32 IdleStart = FPlatformTime::Cycles();

		int32 Width = 0;
		int32 Height = 0;

		RHICmdList.MapStagingSurface(ResultsTextureCPU->GetRenderTargetItem().ShaderResourceTexture, (void*&)ResultsBuffer, Width, Height);

		// Can happen because of device removed, we might crash later but this occlusion culling system can behave gracefully.
		if(!ResultsBuffer)
		{
			ResultsBuffer = FirstFrameBuffer;
			SetInvalidFrameNumber();
		}

		// RHIMapStagingSurface will block until the results are ready (from the previous frame) so we need to consider this RT idle time
		GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUQuery] += FPlatformTime::Cycles() - IdleStart;
		GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUQuery]++;
	}
	check( ResultsBuffer );
}

void FHZBOcclusionTester::UnmapResults(FRHICommandListImmediate& RHICmdList)
{
	check( ResultsBuffer );
	if(!IsInvalidFrame())
	{
		RHICmdList.UnmapStagingSurface(ResultsTextureCPU->GetRenderTargetItem().ShaderResourceTexture);
	}
	ResultsBuffer = NULL;
}

bool FHZBOcclusionTester::IsVisible( uint32 Index ) const
{
	checkSlow( ResultsBuffer );
	checkSlow( Index < SizeX * SizeY );
	
	// TODO shader compress to bits
	uint32 x = FMath::ReverseMortonCode2( Index >> 0 );
	uint32 y = FMath::ReverseMortonCode2( Index >> 1 );
	uint32 m = x + y * SizeX;
	return ResultsBuffer[ 4 * m ] != 0;
}

class FHZBTestPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FHZBTestPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FHZBTestPS() {}

public:
	FShaderParameter				InvSizeParameter;
	FShaderResourceParameter		HZBTexture;
	FShaderResourceParameter		HZBSampler;
	FShaderResourceParameter		BoundsCenterTexture;
	FShaderResourceParameter		BoundsCenterSampler;
	FShaderResourceParameter		BoundsExtentTexture;
	FShaderResourceParameter		BoundsExtentSampler;

	FHZBTestPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		InvSizeParameter.Bind( Initializer.ParameterMap, TEXT("InvSize") );
		HZBTexture.Bind( Initializer.ParameterMap, TEXT("HZBTexture") );
		HZBSampler.Bind( Initializer.ParameterMap, TEXT("HZBSampler") );
		BoundsCenterTexture.Bind( Initializer.ParameterMap, TEXT("BoundsCenterTexture") );
		BoundsCenterSampler.Bind( Initializer.ParameterMap, TEXT("BoundsCenterSampler") );
		BoundsExtentTexture.Bind( Initializer.ParameterMap, TEXT("BoundsExtentTexture") );
		BoundsExtentSampler.Bind( Initializer.ParameterMap, TEXT("BoundsExtentSampler") );
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FHZB& HZB, FTextureRHIParamRef BoundsCenter, FTextureRHIParamRef BoundsExtent )
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View );

		const FVector2D InvSize( 1.0f / HZB.Size.X, 1.0f / HZB.Size.Y );
		SetShaderValue(RHICmdList, ShaderRHI, InvSizeParameter, InvSize );

		SetTextureParameter(RHICmdList, ShaderRHI, HZBTexture, HZBSampler, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), HZB.Texture->GetRenderTargetItem().ShaderResourceTexture );

		SetTextureParameter(RHICmdList, ShaderRHI, BoundsCenterTexture, BoundsCenterSampler, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), BoundsCenter );
		SetTextureParameter(RHICmdList, ShaderRHI, BoundsExtentTexture, BoundsExtentSampler, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), BoundsExtent );
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InvSizeParameter;
		Ar << HZBTexture;
		Ar << HZBSampler;
		Ar << BoundsCenterTexture;
		Ar << BoundsCenterSampler;
		Ar << BoundsExtentTexture;
		Ar << BoundsExtentSampler;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FHZBTestPS,TEXT("HZBOcclusion"),TEXT("HZBTestPS"),SF_Pixel);

void FHZBOcclusionTester::Submit(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
	SCOPED_DRAW_EVENT(RHICmdList, SubmitHZB);

	FSceneViewState* ViewState = (FSceneViewState*)View.State;
	if( !ViewState )
	{
		return;
	}

	TRefCountPtr< IPooledRenderTarget >	BoundsCenterTexture;
	TRefCountPtr< IPooledRenderTarget >	BoundsExtentTexture;
	{
		uint32 Flags = TexCreate_ShaderResource | TexCreate_Dynamic;
		FPooledRenderTargetDesc Desc( FPooledRenderTargetDesc::Create2DDesc( FIntPoint( SizeX, SizeY ), PF_A32B32G32R32F, Flags, TexCreate_None, false ) );

		GRenderTargetPool.FindFreeElement( Desc, BoundsCenterTexture, TEXT("HZBBoundsCenter") );
		GRenderTargetPool.FindFreeElement( Desc, BoundsExtentTexture, TEXT("HZBBoundsExtent") );
	}

	TRefCountPtr< IPooledRenderTarget >	ResultsTextureGPU;
	{
#if PLATFORM_MAC // Workaround radr://16096028 Texture Readback via glReadPixels + PBOs stalls on Nvidia GPUs
		FPooledRenderTargetDesc Desc( FPooledRenderTargetDesc::Create2DDesc( FIntPoint( SizeX, SizeY ), PF_R8G8B8A8, TexCreate_None, TexCreate_RenderTargetable, false ) );

#else
		FPooledRenderTargetDesc Desc( FPooledRenderTargetDesc::Create2DDesc( FIntPoint( SizeX, SizeY ), PF_B8G8R8A8, TexCreate_None, TexCreate_RenderTargetable, false ) );
#endif
		GRenderTargetPool.FindFreeElement( Desc, ResultsTextureGPU, TEXT("HZBResultsGPU") );
	}

	{
		static float CenterBuffer[ SizeX * SizeY ][4];
		static float ExtentBuffer[ SizeX * SizeY ][4];

		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_HZBPackPrimitiveData);
			
			FMemory::Memset( CenterBuffer, 0, sizeof( CenterBuffer ) );
			FMemory::Memset( ExtentBuffer, 0, sizeof( ExtentBuffer ) );

			const uint32 NumPrimitives = Primitives.Num();
			for( uint32 i = 0; i < NumPrimitives; i++ )
			{
				const FOcclusionPrimitive& Primitive = Primitives[i];

				uint32 x = FMath::ReverseMortonCode2( i >> 0 );
				uint32 y = FMath::ReverseMortonCode2( i >> 1 );
				uint32 m = x + y * SizeX;

				CenterBuffer[m][0] = Primitive.Center.X;
				CenterBuffer[m][1] = Primitive.Center.Y;
				CenterBuffer[m][2] = Primitive.Center.Z;
				CenterBuffer[m][3] = 0.0f;

				ExtentBuffer[m][0] = Primitive.Extent.X;
				ExtentBuffer[m][1] = Primitive.Extent.Y;
				ExtentBuffer[m][2] = Primitive.Extent.Z;
				ExtentBuffer[m][3] = 1.0f;
			}
		}
		
		QUICK_SCOPE_CYCLE_COUNTER(STAT_HZBUpdateTextures);
		FUpdateTextureRegion2D Region( 0, 0, 0, 0, SizeX, SizeY );
		RHIUpdateTexture2D( (FTexture2DRHIRef&)BoundsCenterTexture->GetRenderTargetItem().ShaderResourceTexture, 0, Region, SizeX * 4 * sizeof( float ), (uint8*)CenterBuffer );
		RHIUpdateTexture2D( (FTexture2DRHIRef&)BoundsExtentTexture->GetRenderTargetItem().ShaderResourceTexture, 0, Region, SizeX * 4 * sizeof( float ), (uint8*)ExtentBuffer );
		Primitives.Empty();
	}

	// Draw test
	{
		SCOPED_DRAW_EVENT(RHICmdList, TestHZB);

		SetRenderTarget(RHICmdList, ResultsTextureGPU->GetRenderTargetItem().TargetableTexture, NULL);

		TShaderMapRef< FScreenVS >	VertexShader(View.ShaderMap);
		TShaderMapRef< FHZBTestPS >	PixelShader(View.ShaderMap);

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

		PixelShader->SetParameters(RHICmdList, View,  ViewState->HZB, BoundsCenterTexture->GetRenderTargetItem().ShaderResourceTexture, BoundsExtentTexture->GetRenderTargetItem().ShaderResourceTexture );

		RHICmdList.SetViewport(0, 0, 0.0f, SizeX, SizeY, 1.0f);

		// TODO draw quads covering blocks added above
		DrawRectangle(
			RHICmdList,
			0, 0,
			SizeX, SizeY,
			0, 0,
			SizeX, SizeY,
			FIntPoint( SizeX, SizeY ),
			FIntPoint( SizeX, SizeY ),
			*VertexShader,
			EDRF_UseTriangleOptimization);
	}

	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, ResultsTextureGPU);

	// Transfer memory GPU -> CPU
	RHICmdList.CopyToResolveTarget(ResultsTextureGPU->GetRenderTargetItem().TargetableTexture, ResultsTextureCPU->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
}

template< uint32 Stage >
class THZBBuildPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(THZBBuildPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine( TEXT("STAGE"), Stage );
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_R32_FLOAT);
	}

	THZBBuildPS() {}

public:
	FShaderParameter				InvSizeParameter;
	FSceneTextureShaderParameters	SceneTextureParameters;
	FShaderResourceParameter		TextureParameter;
	FShaderResourceParameter		TextureParameterSampler;

	THZBBuildPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		InvSizeParameter.Bind( Initializer.ParameterMap, TEXT("InvSize") );
		SceneTextureParameters.Bind( Initializer.ParameterMap );
		TextureParameter.Bind( Initializer.ParameterMap, TEXT("Texture") );
		TextureParameterSampler.Bind( Initializer.ParameterMap, TEXT("TextureSampler") );
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FIntPoint& Size )
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View );

		const FVector2D InvSize( 1.0f / Size.X, 1.0f / Size.Y );
		SetShaderValue(RHICmdList, ShaderRHI, InvSizeParameter, InvSize );
		
		SceneTextureParameters.Set(RHICmdList, ShaderRHI, View );
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FIntPoint& Size, FShaderResourceViewRHIParamRef ShaderResourceView )
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View );

		const FVector2D InvSize( 1.0f / Size.X, 1.0f / Size.Y );
		SetShaderValue(RHICmdList, ShaderRHI, InvSizeParameter, InvSize );

		//SetTextureParameter( ShaderRHI, TextureParameter, TextureParameterSampler, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), Texture );

		SetSRVParameter(RHICmdList, ShaderRHI, TextureParameter, ShaderResourceView );
		SetSamplerParameter(RHICmdList, ShaderRHI, TextureParameterSampler, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI() );
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InvSizeParameter;
		Ar << SceneTextureParameters;
		Ar << TextureParameter;
		Ar << TextureParameterSampler;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(template<>,THZBBuildPS<0>,TEXT("HZBOcclusion"),TEXT("HZBBuildPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,THZBBuildPS<1>,TEXT("HZBOcclusion"),TEXT("HZBBuildPS"),SF_Pixel);

void BuildHZB(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_BuildHZB);
	SCOPED_DRAW_EVENT(RHICmdList, BuildHZB);

	FSceneViewState* ViewState = (FSceneViewState*)View.State;

	if(!ViewState)
	{
		// not view state (e.g. thumbnail rendering?), no HZB (no screen space reflections or occlusion culling)
		return;
	}

	ViewState->HZB.AllocHZB();

	if(ViewState->HZB.bDataIsValid)
	{
		// data was already computed, no need to do it again
		return;
	}

	ViewState->HZB.bDataIsValid = true;

	// Must be power of 2
	const FIntPoint HZBSize = ViewState->HZB.Size;
	const uint32 NumMips = ViewState->HZB.NumMips;
	
	FSceneRenderTargetItem& HZBRenderTarget = ViewState->HZB.Texture->GetRenderTargetItem();
	
	RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState< false, CF_Always >::GetRHI());
	
	// Mip 0
	{
		SCOPED_DRAW_EVENTF(RHICmdList, BuildHZB, TEXT("HZB#%d"), 0);

		SetRenderTarget(RHICmdList, HZBRenderTarget.TargetableTexture, 0, NULL);

		TShaderMapRef< FScreenVS >		VertexShader(View.ShaderMap);
		TShaderMapRef< THZBBuildPS<0> >	PixelShader(View.ShaderMap);

		static FGlobalBoundShaderState BoundShaderState;
		
		SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

		// Imperfect sampling, doesn't matter too much
		PixelShader->SetParameters( RHICmdList, View, HZBSize );

		RHICmdList.SetViewport(0, 0, 0.0f, HZBSize.X, HZBSize.Y, 1.0f);

		DrawRectangle(
			RHICmdList,
			0, 0,
			HZBSize.X, HZBSize.Y,
			View.ViewRect.Min.X, View.ViewRect.Min.Y,
			View.ViewRect.Width(), View.ViewRect.Height(),
			HZBSize,
			GSceneRenderTargets.GetBufferSizeXY(),
			*VertexShader,
			EDRF_UseTriangleOptimization);

		RHICmdList.CopyToResolveTarget(HZBRenderTarget.TargetableTexture, HZBRenderTarget.ShaderResourceTexture, false, FResolveParams(FResolveRect(), CubeFace_PosX, 0));
	}

	FIntPoint SrcSize = HZBSize;
	FIntPoint DstSize = SrcSize / 2;
	
	// Mip 1-7
	for( uint8 MipIndex = 1; MipIndex < NumMips; MipIndex++ )
	{
		SCOPED_DRAW_EVENTF(RHICmdList, BuildHZB, TEXT("HZB#%d"), MipIndex);

		SetRenderTarget(RHICmdList, HZBRenderTarget.TargetableTexture, MipIndex, NULL);

		TShaderMapRef< FScreenVS >		VertexShader(View.ShaderMap);
		TShaderMapRef< THZBBuildPS<1> >	PixelShader(View.ShaderMap);

		static FGlobalBoundShaderState BoundShaderState;
		
		SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

		PixelShader->SetParameters( RHICmdList, View, DstSize, ViewState->HZB.MipSRVs[ MipIndex - 1 ] );

		RHICmdList.SetViewport(0, 0, 0.0f, DstSize.X, DstSize.Y, 1.0f);

		DrawRectangle(
			RHICmdList,
			0, 0,
			DstSize.X, DstSize.Y,
			0, 0,
			SrcSize.X, SrcSize.Y,
			DstSize,
			SrcSize,
			*VertexShader,
			EDRF_UseTriangleOptimization);

		RHICmdList.CopyToResolveTarget(HZBRenderTarget.TargetableTexture, HZBRenderTarget.ShaderResourceTexture, false, FResolveParams(FResolveRect(), CubeFace_PosX, MipIndex));

		SrcSize /= 2;
		DstSize /= 2;
	}

	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, ViewState->HZB.Texture);
}

void FDeferredShadingSceneRenderer::BeginOcclusionTests(FRHICommandListImmediate& RHICmdList)
{
	SCOPE_CYCLE_COUNTER(STAT_BeginOcclusionTestsTime);
	const bool bUseDownsampledDepth = IsValidRef(GSceneRenderTargets.GetSmallDepthSurface()) && GSceneRenderTargets.UseDownsizedOcclusionQueries();

	if (bUseDownsampledDepth)
	{
		SetRenderTarget(RHICmdList, NULL, GSceneRenderTargets.GetSmallDepthSurface());
	}
	else
	{
		SetRenderTarget(RHICmdList, NULL, GSceneRenderTargets.GetSceneDepthSurface());
	}

	// Perform occlusion queries for each view
	for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		SCOPED_DRAW_EVENT(RHICmdList, BeginOcclusionTests);
		FViewInfo& View = Views[ViewIndex];

		if (bUseDownsampledDepth)
		{
			const uint32 DownsampledX = FMath::TruncToInt(View.ViewRect.Min.X / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor());
			const uint32 DownsampledY = FMath::TruncToInt(View.ViewRect.Min.Y / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor());
			const uint32 DownsampledSizeX = FMath::TruncToInt(View.ViewRect.Width() / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor());
			const uint32 DownsampledSizeY = FMath::TruncToInt(View.ViewRect.Height() / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor());

			// Setup the viewport for rendering to the downsampled depth buffer
			RHICmdList.SetViewport(DownsampledX, DownsampledY, 0.0f, DownsampledX + DownsampledSizeX, DownsampledY + DownsampledSizeY, 1.0f);
		}
		else
		{
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
		}
	
		FSceneViewState* ViewState = (FSceneViewState*)View.State;
	
		if(ViewState && !View.bDisableQuerySubmissions)
		{
			// Depth tests, no depth writes, no color writes, opaque, solid rasterization wo/ backface culling.
			// Note, this is a reversed Z depth surface, using CF_GreaterEqual.
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_GreaterEqual>::GetRHI());
			// We only need to render the front-faces of the culling geometry (this halves the amount of pixels we touch)
			RHICmdList.SetRasterizerState(View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI());
			RHICmdList.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());

			// Lookup the vertex shader.
			TShaderMapRef<FOcclusionQueryVS> VertexShader(View.ShaderMap);
			SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), OcclusionTestBoundShaderState, GetVertexDeclarationFVector3(), *VertexShader, NULL);
			VertexShader->SetParameters(RHICmdList, View);

			// Issue this frame's occlusion queries (occlusion queries from last frame may still be in flight)
#if BUFFERED_OCCLUSION_QUERIES				
			const uint32 QueryIndex = FOcclusionQueryHelpers::GetQueryIssueIndex(ViewState->PendingPrevFrameNumber, ViewState->NumBufferedFrames);
			FSceneViewState::ShadowKeyOcclusionQueryMap& ShadowOcclusionQueryMap = ViewState->ShadowOcclusionQueryMaps[QueryIndex];
#else
			FSceneViewState::ShadowKeyOcclusionQueryMap& ShadowOcclusionQueryMap = ViewState->ShadowOcclusionQueryMap;
#endif			

			// Clear primitives which haven't been visible recently out of the occlusion history, and reset old pending occlusion queries.
			ViewState->TrimOcclusionHistory(RHICmdList, ViewFamily.CurrentRealTime - GEngine->PrimitiveProbablyVisibleTime, ViewFamily.CurrentRealTime, ViewState->OcclusionFrameCounter);

			// Give back all these occlusion queries to the pool.
			for ( TMap<FSceneViewState::FProjectedShadowKey, FRenderQueryRHIRef>::TIterator QueryIt(ShadowOcclusionQueryMap); QueryIt; ++QueryIt )
			{
				//FRenderQueryRHIParamRef Query = QueryIt.Value();
				//check( Query.GetRefCount() == 1 );
				ViewState->OcclusionQueryPool.ReleaseQuery(RHICmdList, QueryIt.Value());
			}
			ShadowOcclusionQueryMap.Reset();

			{
				SCOPED_DRAW_EVENT(RHICmdList, ShadowFrustumQueries);

				for(TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights);LightIt;++LightIt)
				{
					const FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightIt.GetIndex()];

					for(int32 ShadowIndex = 0;ShadowIndex < VisibleLightInfo.AllProjectedShadows.Num();ShadowIndex++)
					{
						const FProjectedShadowInfo& ProjectedShadowInfo = *VisibleLightInfo.AllProjectedShadows[ShadowIndex];

						if (ProjectedShadowInfo.DependentView && ProjectedShadowInfo.DependentView != &View)
						{
							continue;
						}

						if (ProjectedShadowInfo.bOnePassPointLightShadow)
						{
							FLightSceneProxy* Proxy = ProjectedShadowInfo.LightSceneInfo->Proxy;

							// Query one pass point light shadows separately because they don't have a shadow frustum, they have a bounding sphere instead.
							FSphere LightBounds = Proxy->GetBoundingSphere();

							const bool bCameraInsideLightGeometry = ((FVector)View.ViewMatrices.ViewOrigin - LightBounds.Center).SizeSquared() < FMath::Square(LightBounds.W * 1.05f + View.NearClippingDistance * 2.0f);
							if (!bCameraInsideLightGeometry)
							{
								const FRenderQueryRHIRef ShadowOcclusionQuery = ViewState->OcclusionQueryPool.AllocateQuery();
								RHICmdList.BeginRenderQuery(ShadowOcclusionQuery);

								FSceneViewState::FProjectedShadowKey Key(
									ProjectedShadowInfo.ParentSceneInfo ? 
										ProjectedShadowInfo.ParentSceneInfo->PrimitiveComponentId :
										FPrimitiveComponentId(),
									ProjectedShadowInfo.LightSceneInfo->Proxy->GetLightComponent(),
									ProjectedShadowInfo.SplitIndex,
									ProjectedShadowInfo.bTranslucentShadow
									);
								checkSlow(ShadowOcclusionQueryMap.Find(Key) == NULL);
								ShadowOcclusionQueryMap.Add(Key, ShadowOcclusionQuery);

								// Draw bounding sphere
								VertexShader->SetParametersWithBoundingSphere(RHICmdList, View, ProjectedShadowInfo.LightSceneInfo->Proxy->GetBoundingSphere());
								StencilingGeometry::DrawSphere(RHICmdList);

								RHICmdList.EndRenderQuery(ShadowOcclusionQuery);
							}
						}
						// Don't query preshadows, since they are culled if their subject is occluded.
						// Don't query if any subjects are visible because the shadow frustum will be definitely unoccluded
						else if (!ProjectedShadowInfo.IsWholeSceneDirectionalShadow() && !ProjectedShadowInfo.bPreShadow && !ProjectedShadowInfo.SubjectsVisible(View))
						{
							IssueProjectedShadowOcclusionQuery(RHICmdList, View, ProjectedShadowInfo, *VertexShader);
						}
					}

					// Issue occlusion queries for all per-object projected shadows that we would have rendered but were occluded last frame.
					for(int32 ShadowIndex = 0;ShadowIndex < VisibleLightInfo.OccludedPerObjectShadows.Num();ShadowIndex++)
					{
						const FProjectedShadowInfo& ProjectedShadowInfo = *VisibleLightInfo.OccludedPerObjectShadows[ShadowIndex];
						IssueProjectedShadowOcclusionQuery(RHICmdList, View, ProjectedShadowInfo, *VertexShader);
					}
				}
			}
	
			// Don't do primitive occlusion if we have a view parent or are frozen.
	#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if ( !ViewState->HasViewParent() && !ViewState->bIsFrozen )
	#endif
			{
				VertexShader->SetParameters(RHICmdList, View);

				{
					SCOPED_DRAW_EVENT(RHICmdList, IndividualQueries);
					View.IndividualOcclusionQueries.Flush(RHICmdList);
				}
				{
					SCOPED_DRAW_EVENT(RHICmdList, GroupedQueries);
					View.GroupedOcclusionQueries.Flush(RHICmdList);
				}
			}
		}
	}

	for( int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++ )
	{
		FViewInfo& View = Views[ViewIndex];
		FSceneViewState* ViewState = (FSceneViewState*)View.State;
		
		if( ViewState && ViewState->HZBOcclusionTests.GetNum() != 0 )
		{
			check( ViewState->HZBOcclusionTests.IsValidFrame(ViewState->OcclusionFrameCounter) );

			SCOPED_DRAW_EVENT(RHICmdList, HZB);

			BuildHZB(RHICmdList, View);
			ViewState->HZBOcclusionTests.Submit(RHICmdList, View);
		}
	}

	if (bUseDownsampledDepth)
	{
		// Restore default render target
		GSceneRenderTargets.BeginRenderingSceneColor(RHICmdList);
	}
}
