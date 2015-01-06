// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Scene.cpp: Scene manager implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "ShaderCompiler.h"
#include "StaticMeshResources.h"
#include "ParameterCollection.h"
#include "DistanceFieldSurfaceCacheLighting.h"
#include "EngineModule.h"
#include "PrecomputedLightVolume.h"
#include "FXSystem.h"
#include "DistanceFieldLightingShared.h"

// Enable this define to do slow checks for components being added to the wrong
// world's scene, when using PIE. This can happen if a PIE component is reattached
// while GWorld is the editor world, for example.
#define CHECK_FOR_PIE_PRIMITIVE_ATTACH_SCENE_MISMATCH	0


IMPLEMENT_UNIFORM_BUFFER_STRUCT(FDistanceCullFadeUniformShaderParameters,TEXT("PrimitiveFade"));

/** Global primitive uniform buffer resource containing faded in */
TGlobalResource< FGlobalDistanceCullFadeUniformBuffer > GDistanceCullFadedInUniformBuffer;

SIZE_T FStaticMeshDrawListBase::TotalBytesUsed = 0;

/** Default constructor. */
FSceneViewState::FSceneViewState()
	: OcclusionQueryPool(RQT_Occlusion)
{
	OcclusionFrameCounter = 0;
	LastRenderTime = -FLT_MAX;
	LastRenderTimeDelta = 0.0f;
	MotionBlurTimeScale = 1.0f;
	PrevViewMatrixForOcclusionQuery.SetIdentity();
	PrevViewOriginForOcclusionQuery = FVector::ZeroVector;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bIsFreezing = false;
	bIsFrozen = false;
#endif
	// Register this object as a resource, so it will receive device reset notifications.
	if ( IsInGameThread() )
	{
		BeginInitResource(this);
	}
	else
	{
		InitResource();
	}
	CachedVisibilityChunk = NULL;
	CachedVisibilityHandlerId = INDEX_NONE;
	CachedVisibilityBucketIndex = INDEX_NONE;
	CachedVisibilityChunkIndex = INDEX_NONE;
	MIDUsedCount = 0;
	TemporalAASampleIndex = 0;
	TemporalAASampleCount = 1;
	AOTileIntersectionResources = NULL;
	bBokehDOFHistory = true;
	bBokehDOFHistory2 = true;

	LightPropagationVolume = NULL; 

	for (int32 CascadeIndex = 0; CascadeIndex < ARRAY_COUNT(TranslucencyLightingCacheAllocations); CascadeIndex++)
	{
		TranslucencyLightingCacheAllocations[CascadeIndex] = NULL;
	}

#if BUFFERED_OCCLUSION_QUERIES
	NumBufferedFrames = FOcclusionQueryHelpers::GetNumBufferedFrames();
	ShadowOcclusionQueryMaps.Empty(NumBufferedFrames);
	ShadowOcclusionQueryMaps.AddZeroed(NumBufferedFrames);	
#endif
}

FDistanceFieldSceneData::FDistanceFieldSceneData() 
	: NumObjectsInBuffer(0)
	, ObjectBuffers(NULL)
	, AtlasGeneration(0)
{

}

FDistanceFieldSceneData::~FDistanceFieldSceneData() 
{
	delete ObjectBuffers;
}

void FDistanceFieldSceneData::AddPrimitive(FPrimitiveSceneInfo* InPrimitive)
{
	const FPrimitiveSceneProxy* Proxy = InPrimitive->Proxy;

	if (Proxy->CastsDynamicShadow()
		&& Proxy->AffectsDistanceFieldLighting())
	{
		if (Proxy->SupportsHeightfieldRepresentation())
		{
			HeightfieldPrimitives.Add(InPrimitive);
		}

		if (Proxy->SupportsDistanceFieldRepresentation())
		{
			checkSlow(!PendingAddOperations.Contains(InPrimitive));
			checkSlow(!PendingUpdateOperations.Contains(InPrimitive));
			PendingAddOperations.Add(InPrimitive);
		}
	}
}

void FDistanceFieldSceneData::UpdatePrimitive(FPrimitiveSceneInfo* InPrimitive)
{
	const FPrimitiveSceneProxy* Proxy = InPrimitive->Proxy;

	if (Proxy->CastsDynamicShadow() 
		&& Proxy->AffectsDistanceFieldLighting()
		&& Proxy->SupportsDistanceFieldRepresentation() 
		&& !PendingAddOperations.Contains(InPrimitive)
		// This can happen when the primitive fails to allocate from the SDF atlas
		&& InPrimitive->DistanceFieldInstanceIndices.Num() > 0)
	{
		PendingUpdateOperations.Add(InPrimitive);
	}
}

void FDistanceFieldSceneData::RemovePrimitive(FPrimitiveSceneInfo* InPrimitive)
{
	const FPrimitiveSceneProxy* Proxy = InPrimitive->Proxy;

	if (Proxy->SupportsDistanceFieldRepresentation() && Proxy->AffectsDistanceFieldLighting())
	{
		PendingAddOperations.Remove(InPrimitive);
		PendingUpdateOperations.Remove(InPrimitive);

		for (int32 InstanceIndex = 0; InstanceIndex < InPrimitive->DistanceFieldInstanceIndices.Num(); InstanceIndex++)
		{
			int32 RemoveIndex = InPrimitive->DistanceFieldInstanceIndices[InstanceIndex];

			// Sanity check that scales poorly
			if (PendingRemoveOperations.Num() < 1000)
			{
				checkSlow(!PendingRemoveOperations.Contains(RemoveIndex));
			}
			
			PendingRemoveOperations.Add(RemoveIndex);
		}

		InPrimitive->DistanceFieldInstanceIndices.Empty();
	}

	if (Proxy->SupportsHeightfieldRepresentation() && Proxy->AffectsDistanceFieldLighting())
	{
		HeightfieldPrimitives.Remove(InPrimitive);
	}
}

void FDistanceFieldSceneData::Release()
{
	if (ObjectBuffers)
	{
		ObjectBuffers->Release();
	}
}

void FDistanceFieldSceneData::VerifyIntegrity()
{
	check(NumObjectsInBuffer == PrimitiveInstanceMapping.Num());

	for (int32 PrimitiveInstanceIndex = 0; PrimitiveInstanceIndex < PrimitiveInstanceMapping.Num(); PrimitiveInstanceIndex++)
	{
		const FPrimitiveAndInstance& PrimitiveAndInstance = PrimitiveInstanceMapping[PrimitiveInstanceIndex];

		check(PrimitiveAndInstance.Primitive && PrimitiveAndInstance.Primitive->DistanceFieldInstanceIndices.Num() > 0);
		check(PrimitiveAndInstance.Primitive->DistanceFieldInstanceIndices.IsValidIndex(PrimitiveAndInstance.InstanceIndex));

		const int32 InstanceIndex = PrimitiveAndInstance.Primitive->DistanceFieldInstanceIndices[PrimitiveAndInstance.InstanceIndex];
		check(InstanceIndex == PrimitiveInstanceIndex);
	}
}

/**
 * Sets the FX system associated with the scene.
 */
void FScene::SetFXSystem( class FFXSystemInterface* InFXSystem )
{
	FXSystem = InFXSystem;
}

/**
 * Get the FX system associated with the scene.
 */
FFXSystemInterface* FScene::GetFXSystem()
{
	return FXSystem;
}

void FScene::SetClearMotionBlurInfoGameThread()
{
	check(IsInGameThread());

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ShouldClearMBInfoCommand,
		FScene*,Scene,this,
	{
		Scene->MotionBlurInfoData.SetClearMotionBlurInfo();
	});
}

void FScene::UpdateParameterCollections(const TArray<FMaterialParameterCollectionInstanceResource*>& InParameterCollections)
{
	// Empy the scene's map so any unused uniform buffers will be released
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ClearParameterCollectionsCommand,
		FScene*,Scene,this,
	{
		Scene->ParameterCollections.Empty();
	});

	// Add each existing parameter collection id and its uniform buffer
	for (int32 CollectionIndex = 0; CollectionIndex < InParameterCollections.Num(); CollectionIndex++)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			AddParameterCollectionCommand,
			FScene*,Scene,this,
			FMaterialParameterCollectionInstanceResource*,InstanceResource,InParameterCollections[CollectionIndex],
		{
			Scene->ParameterCollections.Add(InstanceResource->GetId(), InstanceResource->GetUniformBuffer());
		});
	}
}

SIZE_T FScene::GetSizeBytes() const
{
	return sizeof(*this) 
		+ Primitives.GetAllocatedSize()
		+ Lights.GetAllocatedSize()
		+ StaticMeshes.GetAllocatedSize()
		+ ExponentialFogs.GetAllocatedSize()
		+ WindSources.GetAllocatedSize()
		+ SpeedTreeVertexFactoryMap.GetAllocatedSize()
		+ SpeedTreeWindComputationMap.GetAllocatedSize()
		+ LightOctree.GetSizeBytes()
		+ PrimitiveOctree.GetSizeBytes();
}

void FScene::CheckPrimitiveArrays()
{
	check(Primitives.Num() == PrimitiveBounds.Num());
	check(Primitives.Num() == PrimitiveVisibilityIds.Num());
	check(Primitives.Num() == PrimitiveOcclusionFlags.Num());
	check(Primitives.Num() == PrimitiveComponentIds.Num());
	check(Primitives.Num() == PrimitiveOcclusionBounds.Num());
}

void FScene::AddPrimitiveSceneInfo_RenderThread(FRHICommandListImmediate& RHICmdList, FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	SCOPE_CYCLE_COUNTER(STAT_AddScenePrimitiveRenderThreadTime);
	
	CheckPrimitiveArrays();

	int32 PrimitiveIndex = Primitives.Add(PrimitiveSceneInfo);
	PrimitiveSceneInfo->PackedIndex = PrimitiveIndex;

	PrimitiveBounds.AddUninitialized();
	PrimitiveVisibilityIds.AddUninitialized();
	PrimitiveOcclusionFlags.AddUninitialized();
	PrimitiveComponentIds.AddUninitialized();
	PrimitiveOcclusionBounds.AddUninitialized();

	CheckPrimitiveArrays();

	// Add the primitive to its shadow parent's linked list of children.
	// Note: must happen before AddToScene because AddToScene depends on LightingAttachmentRoot
	PrimitiveSceneInfo->LinkAttachmentGroup();

	// Add the primitive to the scene.
	PrimitiveSceneInfo->AddToScene(RHICmdList, true);

	DistanceFieldSceneData.AddPrimitive(PrimitiveSceneInfo);
}

/**
 * Verifies that a component is added to the proper scene
 *
 * @param Component Component to verify
 * @param World World who's scene the primitive is being attached to
 */
FORCEINLINE static void VerifyProperPIEScene(UPrimitiveComponent* Component, UWorld* World)
{
#if CHECK_FOR_PIE_PRIMITIVE_ATTACH_SCENE_MISMATCH
	checkf(Component->GetOuter() == GetTransientPackage() || 
		(FPackageName::GetLongPackageAssetName(Component->GetOutermost()->GetName()).StartsWith(PLAYWORLD_PACKAGE_PREFIX) == 
		FPackageName::GetLongPackageAssetName(World->GetOutermost()->GetName()).StartsWith(PLAYWORLD_PACKAGE_PREFIX)),
		TEXT("The component %s was added to the wrong world's scene (due to PIE). The callstack should tell you why"), 
		*Component->GetFullName()
		);
#endif
}

FScene::FScene(UWorld* InWorld, bool bInRequiresHitProxies, bool bInIsEditorScene, bool bCreateFXSystem, ERHIFeatureLevel::Type InFeatureLevel)
:	World(InWorld)
,	FXSystem(NULL)
,	bStaticDrawListsMobileHDR(false)
,	bStaticDrawListsMobileHDR32bpp(false)
,	StaticDrawListsEarlyZPassMode(0)
,	bScenesPrimitivesNeedStaticMeshElementUpdate(false)
,	SkyLight(NULL)
,	SimpleDirectionalLight(NULL)
,	SunLight(NULL)
,	ReflectionSceneData(InFeatureLevel)
,	IndirectLightingCache(InFeatureLevel)
,	SurfaceCacheResources(NULL)
,	PreshadowCacheLayout(0, 0, 0, 0, false, false)
,	AtmosphericFog(NULL)
,	PrecomputedVisibilityHandler(NULL)
,	LightOctree(FVector::ZeroVector,HALF_WORLD_MAX)
,	PrimitiveOctree(FVector::ZeroVector,HALF_WORLD_MAX)
,	bRequiresHitProxies(bInRequiresHitProxies)
,	bIsEditorScene(bInIsEditorScene)
,	NumUncachedStaticLightingInteractions(0)
,	UpperDynamicSkylightColor(FLinearColor::Black)
,	LowerDynamicSkylightColor(FLinearColor::Black)
,	NumVisibleLights(0)
,	bHasSkyLight(false)
{
	check(World);
	World->Scene = this;

	FeatureLevel = World->FeatureLevel;

	static auto* MobileHDRCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR"));
	static auto* MobileHDR32bppCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR32bpp"));
	bStaticDrawListsMobileHDR = MobileHDRCvar->GetValueOnAnyThread() == 1;
	bStaticDrawListsMobileHDR32bpp = bStaticDrawListsMobileHDR && (GSupportsRenderTargetFormat_PF_FloatRGBA == false || MobileHDR32bppCvar->GetValueOnAnyThread() == 1);

	static auto* EarlyZPassCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.EarlyZPass"));
	StaticDrawListsEarlyZPassMode = EarlyZPassCvar->GetValueOnAnyThread();

	if (World->FXSystem)
	{
		FFXSystemInterface::Destroy(World->FXSystem);
	}

	if (bCreateFXSystem)
	{
		World->CreateFXSystem();
	}
	else
	{
		World->FXSystem = NULL;
		SetFXSystem(NULL);
	}

	World->UpdateParameterCollectionInstances(false);
}

FScene::~FScene()
{
#if 0 // if you have component that has invalid scene, try this code to see this is reason. 
	for (FObjectIterator Iter(UActorComponent::StaticClass()); Iter; ++Iter)
	{
		UActorComponent * ActorComp = CastChecked<UActorComponent>(*Iter);
		if (ActorComp->GetScene() == this)
		{
			UE_LOG(LogRenderer, Log, TEXT("%s's scene is going to get invalidated"), *ActorComp->GetName());
		}
	}
#endif

	ReflectionSceneData.CubemapArray.ReleaseResource();
	IndirectLightingCache.ReleaseResource();
	DistanceFieldSceneData.Release();

	if (SurfaceCacheResources)
	{
		SurfaceCacheResources->ReleaseResource();
		delete SurfaceCacheResources;
		SurfaceCacheResources = NULL;
	}

	if (AtmosphericFog)
	{
		delete AtmosphericFog;
		AtmosphericFog = NULL;
	}
}

void FScene::AddPrimitive(UPrimitiveComponent* Primitive)
{
	SCOPE_CYCLE_COUNTER(STAT_AddScenePrimitiveGT);

	checkf(!Primitive->HasAnyFlags(RF_Unreachable), TEXT("%s"), *Primitive->GetFullName());


	// Save the world transform for next time the primitive is added to the scene
	float DeltaTime = GetWorld()->GetTimeSeconds() - Primitive->LastSubmitTime;
	if ( DeltaTime < -0.0001f || Primitive->LastSubmitTime < 0.0001f )
	{
		// Time was reset?
		Primitive->LastSubmitTime = GetWorld()->GetTimeSeconds();
	}
	else if ( DeltaTime > 0.0001f )
	{
		// First call for the new frame?
		Primitive->LastSubmitTime = GetWorld()->GetTimeSeconds();
	}

	// Create the primitive's scene proxy.
	FPrimitiveSceneProxy* PrimitiveSceneProxy = Primitive->CreateSceneProxy();
	Primitive->SceneProxy = PrimitiveSceneProxy;
	if(!PrimitiveSceneProxy)
	{
		// Primitives which don't have a proxy are irrelevant to the scene manager.
		return;
	}

	// Cache the primitive's initial transform.
	FMatrix RenderMatrix = Primitive->GetRenderMatrix();
	FVector OwnerPosition(0);

	AActor* Owner = Primitive->GetOwner();
	if (Owner)
	{
		OwnerPosition = Owner->GetActorLocation();
	}

	struct FCreateRenderThreadParameters
	{
		FPrimitiveSceneProxy* PrimitiveSceneProxy;
		FMatrix RenderMatrix;
		FBoxSphereBounds WorldBounds;
		FVector OwnerPosition;
		FBoxSphereBounds LocalBounds;
	};
	FCreateRenderThreadParameters Params =
	{
		PrimitiveSceneProxy,
		RenderMatrix,
		Primitive->Bounds,
		OwnerPosition,
		Primitive->CalcBounds(FTransform::Identity)
	};
	// Create any RenderThreadResources required.
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		FCreateRenderThreadResourcesCommand,
		FCreateRenderThreadParameters, Params, Params,
	{
		FPrimitiveSceneProxy* PrimitiveSceneProxy = Params.PrimitiveSceneProxy;
		FScopeCycleCounter Context(PrimitiveSceneProxy->GetStatId());
		PrimitiveSceneProxy->SetTransform(Params.RenderMatrix, Params.WorldBounds, Params.LocalBounds, Params.OwnerPosition);

		// Create any RenderThreadResources required.
		PrimitiveSceneProxy->CreateRenderThreadResources();
	});

	// Create the primitive scene info.
	FPrimitiveSceneInfo* PrimitiveSceneInfo = new FPrimitiveSceneInfo(Primitive,this);
	PrimitiveSceneProxy->PrimitiveSceneInfo = PrimitiveSceneInfo;

	INC_DWORD_STAT_BY( STAT_GameToRendererMallocTotal, PrimitiveSceneProxy->GetMemoryFootprint() + PrimitiveSceneInfo->GetMemoryFootprint() );

	// Verify the primitive is valid (this will compile away to a nop without CHECK_FOR_PIE_PRIMITIVE_ATTACH_SCENE_MISMATCH)
	VerifyProperPIEScene(Primitive, World);

	// Increment the attachment counter, the primitive is about to be attached to the scene.
	Primitive->AttachmentCounter.Increment();

	// Send a command to the rendering thread to add the primitive to the scene.
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FAddPrimitiveCommand,
		FScene*,Scene,this,
		FPrimitiveSceneInfo*,PrimitiveSceneInfo,PrimitiveSceneInfo,
		{
			FScopeCycleCounter Context(PrimitiveSceneInfo->Proxy->GetStatId());
			Scene->AddPrimitiveSceneInfo_RenderThread(RHICmdList, PrimitiveSceneInfo);
		});

}

void FScene::UpdatePrimitiveTransform_RenderThread(FRHICommandListImmediate& RHICmdList, FPrimitiveSceneProxy* PrimitiveSceneProxy, const FBoxSphereBounds& WorldBounds, const FBoxSphereBounds& LocalBounds, const FMatrix& LocalToWorld, const FVector& OwnerPosition)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdatePrimitiveTransformRenderThreadTime);

	const bool bUpdateStaticDrawLists = !PrimitiveSceneProxy->StaticElementsAlwaysUseProxyPrimitiveUniformBuffer();

	// Remove the primitive from the scene at its old location
	// (note that the octree update relies on the bounds not being modified yet).
	PrimitiveSceneProxy->GetPrimitiveSceneInfo()->RemoveFromScene(bUpdateStaticDrawLists);
	
	// Update the primitive motion blur information.
	// hack
	FScene* Scene = (FScene*)&PrimitiveSceneProxy->GetScene();

	Scene->MotionBlurInfoData.UpdatePrimitiveMotionBlur(PrimitiveSceneProxy->GetPrimitiveSceneInfo());
	
	// Update the primitive transform.
	PrimitiveSceneProxy->SetTransform(LocalToWorld, WorldBounds, LocalBounds, OwnerPosition);

	DistanceFieldSceneData.UpdatePrimitive(PrimitiveSceneProxy->GetPrimitiveSceneInfo());

	// If the primitive has static mesh elements, it should have returned true from ShouldRecreateProxyOnUpdateTransform!
	check(!(bUpdateStaticDrawLists && PrimitiveSceneProxy->GetPrimitiveSceneInfo()->StaticMeshes.Num()));

	// Re-add the primitive to the scene with the new transform.
	PrimitiveSceneProxy->GetPrimitiveSceneInfo()->AddToScene(RHICmdList, bUpdateStaticDrawLists);
}

void FScene::UpdatePrimitiveTransform(UPrimitiveComponent* Primitive)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdatePrimitiveTransformGT);

	// Save the world transform for next time the primitive is added to the scene
	float DeltaTime = GetWorld()->GetTimeSeconds() - Primitive->LastSubmitTime;
	if ( DeltaTime < -0.0001f || Primitive->LastSubmitTime < 0.0001f )
	{
		// Time was reset?
		Primitive->LastSubmitTime = GetWorld()->GetTimeSeconds();
	}
	else if ( DeltaTime > 0.0001f )
	{
		// First call for the new frame?
		Primitive->LastSubmitTime = GetWorld()->GetTimeSeconds();
	}

	AActor* Owner = Primitive->GetOwner();

	// If the root component of an actor is being moved, update all the actor position of the other components sharing that actor
	if (Owner && Owner->GetRootComponent() == Primitive)
	{
		TArray<UPrimitiveComponent*> Components;
		Owner->GetComponents(Components);
		for (int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++)
		{
			UPrimitiveComponent* PrimitiveComponent = Components[ComponentIndex];

			// Only update components that are already attached
			if (PrimitiveComponent 
				&& PrimitiveComponent->SceneProxy 
				&& PrimitiveComponent != Primitive
				// Don't bother if it is going to have its transform updated anyway
				&& !PrimitiveComponent->IsRenderTransformDirty()
				&& !PrimitiveComponent->IsRenderStateDirty())
			{
				PrimitiveComponent->SceneProxy->UpdateActorPosition(Owner->GetActorLocation());
			}
		}
	}

	if(Primitive->SceneProxy)
	{
		// Check if the primitive needs to recreate its proxy for the transform update.
		if(Primitive->ShouldRecreateProxyOnUpdateTransform())
		{
			// Re-add the primitive from scratch to recreate the primitive's proxy.
			RemovePrimitive(Primitive);
			AddPrimitive(Primitive);
		}
		else
		{
			FVector OwnerPosition(0);

			AActor* Actor = Primitive->GetOwner();
			if (Actor != NULL)
			{
				OwnerPosition = Actor->GetActorLocation();
			}

			struct FPrimitiveUpdateParams
			{
				FScene* Scene;
				FPrimitiveSceneProxy* PrimitiveSceneProxy;
				FBoxSphereBounds WorldBounds;
				FBoxSphereBounds LocalBounds;
				FMatrix LocalToWorld;
				FVector OwnerPosition;
			};

			FPrimitiveUpdateParams UpdateParams;
			UpdateParams.Scene = this;
			UpdateParams.PrimitiveSceneProxy = Primitive->SceneProxy;
			UpdateParams.WorldBounds = Primitive->Bounds;
			UpdateParams.LocalToWorld = Primitive->GetRenderMatrix();
			UpdateParams.OwnerPosition = OwnerPosition;
			UpdateParams.LocalBounds = Primitive->CalcBounds(FTransform::Identity);

			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				UpdateTransformCommand,
				FPrimitiveUpdateParams,UpdateParams,UpdateParams,
				{
					FScopeCycleCounter Context(UpdateParams.PrimitiveSceneProxy->GetStatId());
					UpdateParams.Scene->UpdatePrimitiveTransform_RenderThread(RHICmdList, UpdateParams.PrimitiveSceneProxy, UpdateParams.WorldBounds, UpdateParams.LocalBounds, UpdateParams.LocalToWorld, UpdateParams.OwnerPosition);
				});
		}
	}
	else
	{
		// If the primitive doesn't have a scene info object yet, it must be added from scratch.
		AddPrimitive(Primitive);
	}
}

void FScene::UpdatePrimitiveLightingAttachmentRoot(UPrimitiveComponent* Primitive)
{
	const UPrimitiveComponent* NewLightingAttachmentRoot = Cast<UPrimitiveComponent>(Primitive->GetAttachmentRoot());

	if (NewLightingAttachmentRoot == Primitive)
	{
		NewLightingAttachmentRoot = NULL;
	}

	FPrimitiveComponentId NewComponentId = NewLightingAttachmentRoot ? NewLightingAttachmentRoot->ComponentId : FPrimitiveComponentId();

	if (Primitive->SceneProxy)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			UpdatePrimitiveAttachment,
			FPrimitiveSceneProxy*,Proxy,Primitive->SceneProxy,
			FPrimitiveComponentId,NewComponentId,NewComponentId,
		{
			FPrimitiveSceneInfo* PrimitiveInfo = Proxy->GetPrimitiveSceneInfo();
			PrimitiveInfo->UnlinkAttachmentGroup();
			PrimitiveInfo->LightingAttachmentRoot = NewComponentId;
			PrimitiveInfo->LinkAttachmentGroup();
		});
	}
}

void FScene::UpdatePrimitiveAttachment(UPrimitiveComponent* Primitive)
{
	TArray<USceneComponent*, TInlineAllocator<1> > ProcessStack;
	ProcessStack.Push(Primitive);

	// Walk down the tree updating, because the scene's attachment data structures must be updated if the root of the attachment tree changes
	while (ProcessStack.Num() > 0)
	{
		USceneComponent* Current = ProcessStack.Pop();
		UPrimitiveComponent* CurrentPrimitive = Cast<UPrimitiveComponent>(Current);
		check(Current);

		if (CurrentPrimitive
			&& CurrentPrimitive->GetWorld() 
			&& CurrentPrimitive->GetWorld()->Scene 
			&& CurrentPrimitive->GetWorld()->Scene == this
			&& CurrentPrimitive->ShouldComponentAddToScene())
		{
			UpdatePrimitiveLightingAttachmentRoot(CurrentPrimitive);
		}

		for (int32 ChildIndex = 0; ChildIndex < Current->AttachChildren.Num(); ChildIndex++)
		{
			USceneComponent* ChildComponent = Current->AttachChildren[ChildIndex];
			if (ChildComponent)
			{
				ProcessStack.Push(ChildComponent);
			}
		}
	}
}

void FScene::RemovePrimitiveSceneInfo_RenderThread(FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	SCOPE_CYCLE_COUNTER(STAT_RemoveScenePrimitiveTime);

	CheckPrimitiveArrays();

	int32 PrimitiveIndex = PrimitiveSceneInfo->PackedIndex;
	Primitives.RemoveAtSwap(PrimitiveIndex);
	PrimitiveBounds.RemoveAtSwap(PrimitiveIndex);
	PrimitiveVisibilityIds.RemoveAtSwap(PrimitiveIndex);
	PrimitiveOcclusionFlags.RemoveAtSwap(PrimitiveIndex);
	PrimitiveComponentIds.RemoveAtSwap(PrimitiveIndex);
	PrimitiveOcclusionBounds.RemoveAtSwap(PrimitiveIndex);
	if (Primitives.IsValidIndex(PrimitiveIndex))
	{
		FPrimitiveSceneInfo* OtherPrimitive = Primitives[PrimitiveIndex];
		OtherPrimitive->PackedIndex = PrimitiveIndex;
	}
	
	CheckPrimitiveArrays();

	// Update the primitive's motion blur information.
	MotionBlurInfoData.RemovePrimitiveMotionBlur(PrimitiveSceneInfo);

	// Unlink the primitive from its shadow parent.
	PrimitiveSceneInfo->UnlinkAttachmentGroup();

	// Remove the primitive from the scene.
	PrimitiveSceneInfo->RemoveFromScene(true);

	DistanceFieldSceneData.RemovePrimitive(PrimitiveSceneInfo);

	// free the primitive scene proxy.
	delete PrimitiveSceneInfo->Proxy;
}

void FScene::RemovePrimitive( UPrimitiveComponent* Primitive )
{
	SCOPE_CYCLE_COUNTER(STAT_RemoveScenePrimitiveGT);

	FPrimitiveSceneProxy* PrimitiveSceneProxy = Primitive->SceneProxy;

	if(PrimitiveSceneProxy)
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveSceneProxy->GetPrimitiveSceneInfo();

		// Disassociate the primitive's scene proxy.
		Primitive->SceneProxy = NULL;

		// Send a command to the rendering thread to remove the primitive from the scene.
		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
			FRemovePrimitiveCommand,
			FScene*,Scene,this,
			FPrimitiveSceneInfo*,PrimitiveSceneInfo,PrimitiveSceneProxy->GetPrimitiveSceneInfo(),
			FThreadSafeCounter*,AttachmentCounter,&Primitive->AttachmentCounter,
			{
				FScopeCycleCounter Context(PrimitiveSceneInfo->Proxy->GetStatId());
				Scene->RemovePrimitiveSceneInfo_RenderThread(PrimitiveSceneInfo);
				AttachmentCounter->Decrement();
			});

		// Delete the PrimitiveSceneInfo on the game thread after the rendering thread has processed its removal.
		// This must be done on the game thread because the hit proxy references (and possibly other members) need to be freed on the game thread.
		BeginCleanup(PrimitiveSceneInfo);
	}
}

void FScene::ReleasePrimitive( UPrimitiveComponent* PrimitiveComponent )
{
	// Send a command to the rendering thread to clean up any state dependent on this primitive
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FReleasePrimitiveCommand,
		FScene*,Scene,this,
		FPrimitiveComponentId,PrimitiveComponentId,PrimitiveComponent->ComponentId,
	{
		// Free the space in the indirect lighting cache
		Scene->IndirectLightingCache.ReleasePrimitive(PrimitiveComponentId);
	});
}

void FScene::AddLightSceneInfo_RenderThread(FLightSceneInfo* LightSceneInfo)
{
	SCOPE_CYCLE_COUNTER(STAT_AddSceneLightTime);

	check(LightSceneInfo->bVisible);

	// Add the light to the light list.
	LightSceneInfo->Id = Lights.Add(FLightSceneInfoCompact(LightSceneInfo));
	const FLightSceneInfoCompact& LightSceneInfoCompact = Lights[LightSceneInfo->Id];

	if (!SimpleDirectionalLight && 
		LightSceneInfo->Proxy->GetLightType() == LightType_Directional &&
		// Only use a stationary or movable light
		!LightSceneInfo->Proxy->HasStaticLighting())
	{
		SimpleDirectionalLight = LightSceneInfo;

		// if we are forward rendered and this light is a dynamic shadowcast then we need to update the static draw lists to pick a new lightingpolicy
		bScenesPrimitivesNeedStaticMeshElementUpdate = bScenesPrimitivesNeedStaticMeshElementUpdate || (!ShouldUseDeferredRenderer() && !SimpleDirectionalLight->Proxy->HasStaticShadowing());		
	}

	if (LightSceneInfo->Proxy->IsUsedAsAtmosphereSunLight() &&
		(!SunLight || LightSceneInfo->Proxy->GetColor().ComputeLuminance() > SunLight->Proxy->GetColor().ComputeLuminance()) ) // choose brightest sun light...
	{
		SunLight = LightSceneInfo;
	}

	// Add the light to the scene.
	LightSceneInfo->AddToScene();
}

void FScene::AddLight(ULightComponent* Light)
{
	// Create the light's scene proxy.
	FLightSceneProxy* Proxy = Light->CreateSceneProxy();
	if(Proxy)
	{
		// Associate the proxy with the light.
		Light->SceneProxy = Proxy;

		// Update the light's transform and position.
		Proxy->SetTransform(Light->ComponentToWorld.ToMatrixNoScale(),Light->GetLightPosition());

		// Create the light scene info.
		Proxy->LightSceneInfo = new FLightSceneInfo(Proxy, true);

		INC_DWORD_STAT(STAT_SceneLights);

		// Adding a new light
		++NumVisibleLights;

		// Send a command to the rendering thread to add the light to the scene.
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			FAddLightCommand,
			FScene*,Scene,this,
			FLightSceneInfo*,LightSceneInfo,Proxy->LightSceneInfo,
			{
				FScopeCycleCounter Context(LightSceneInfo->Proxy->GetStatId());
				Scene->AddLightSceneInfo_RenderThread(LightSceneInfo);
			});
	}
}

void FScene::AddInvisibleLight(ULightComponent* Light)
{
	// Create the light's scene proxy.
	FLightSceneProxy* Proxy = Light->CreateSceneProxy();

	if(Proxy)
	{
		// Associate the proxy with the light.
		Light->SceneProxy = Proxy;

		// Update the light's transform and position.
		Proxy->SetTransform(Light->ComponentToWorld.ToMatrixNoScale(),Light->GetLightPosition());

		// Create the light scene info.
		Proxy->LightSceneInfo = new FLightSceneInfo(Proxy, false);

		INC_DWORD_STAT(STAT_SceneLights);

		// Send a command to the rendering thread to add the light to the scene.
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			FAddLightCommand,
			FScene*,Scene,this,
			FLightSceneInfo*,LightSceneInfo,Proxy->LightSceneInfo,
		{
			FScopeCycleCounter Context(LightSceneInfo->Proxy->GetStatId());
			LightSceneInfo->Id = Scene->InvisibleLights.Add(FLightSceneInfoCompact(LightSceneInfo));
		});
	}
}

void FScene::SetSkyLight(FSkyLightSceneProxy* LightProxy)
{
	bHasSkyLight = LightProxy != NULL;

	// Send a command to the rendering thread to add the light to the scene.
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FSetSkyLightCommand,
		FScene*,Scene,this,
		FSkyLightSceneProxy*,LightProxy,LightProxy,
	{
		// Mark the scene as needing static draw lists to be recreated if needed
		// The base pass chooses shaders based on whether there's a skylight in the scene, and that is cached in static draw lists
		if ((Scene->SkyLight == NULL) != (LightProxy == NULL))
		{
			Scene->bScenesPrimitivesNeedStaticMeshElementUpdate = true;
		}
		Scene->SkyLight = LightProxy;
	});
}

void FScene::AddOrRemoveDecal_RenderThread(FDeferredDecalProxy* Proxy, bool bAdd)
{
	if(bAdd)
	{
		Decals.Add(Proxy);
	}
	else
	{
		// can be optimized
		for(TSparseArray<FDeferredDecalProxy*>::TIterator It(Decals); It; ++It)
		{
			FDeferredDecalProxy* CurrentProxy = *It;

			if (CurrentProxy == Proxy)
			{
				It.RemoveCurrent();
				delete CurrentProxy;
				break;
			}
		}
	}
}

void FScene::AddDecal(UDecalComponent* Component)
{
	if(!Component->SceneProxy)
	{
		// Create the decals's scene proxy.
		Component->SceneProxy = Component->CreateSceneProxy();

		INC_DWORD_STAT(STAT_SceneDecals);

		// Send a command to the rendering thread to add the light to the scene.
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			FAddDecalCommand,
			FScene*,Scene,this,
			FDeferredDecalProxy*,Proxy,Component->SceneProxy,
		{
			Scene->AddOrRemoveDecal_RenderThread(Proxy, true);
		});
	}
}

void FScene::RemoveDecal(UDecalComponent* Component)
{
	if(Component->SceneProxy)
	{
		DEC_DWORD_STAT(STAT_SceneDecals);

		// Send a command to the rendering thread to remove the light from the scene.
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			FRemoveDecalCommand,
			FScene*,Scene,this,
			FDeferredDecalProxy*,Proxy,Component->SceneProxy,
		{
			Scene->AddOrRemoveDecal_RenderThread(Proxy, false);
		});

		// Disassociate the primitive's scene proxy.
		Component->SceneProxy = NULL;
	}
}

void FScene::UpdateDecalTransform(UDecalComponent* Decal)
{
	if(Decal->SceneProxy)
	{
		//Send command to the rendering thread to update the decal's transform.
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			UpdateTransformCommand,
			FDeferredDecalProxy*,DecalSceneProxy,Decal->SceneProxy,
			FTransform,ComponentToWorld,Decal->GetComponentToWorld(),
		{
			// Update the primitive's transform.
			DecalSceneProxy->SetTransform(ComponentToWorld);
		});
	}
}

void FScene::AddReflectionCapture(UReflectionCaptureComponent* Component)
{
	if (!Component->SceneProxy)
	{
		Component->SceneProxy = Component->CreateSceneProxy();

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			FAddCaptureCommand,
			FScene*,Scene,this,
			FReflectionCaptureProxy*,Proxy,Component->SceneProxy,
		{
			Scene->ReflectionSceneData.bRegisteredReflectionCapturesHasChanged = true;
			const int32 PackedIndex = Scene->ReflectionSceneData.RegisteredReflectionCaptures.Add(Proxy);

			Proxy->PackedIndex = PackedIndex;
			Scene->ReflectionSceneData.RegisteredReflectionCapturePositions.Add(Proxy->Position);

			checkSlow(Scene->ReflectionSceneData.RegisteredReflectionCaptures.Num() == Scene->ReflectionSceneData.RegisteredReflectionCapturePositions.Num());
		});
	}
}

void FScene::RemoveReflectionCapture(UReflectionCaptureComponent* Component)
{
	if (Component->SceneProxy)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			FRemoveCaptureCommand,
			FScene*,Scene,this,
			FReflectionCaptureProxy*,Proxy,Component->SceneProxy,
		{
			Scene->ReflectionSceneData.bRegisteredReflectionCapturesHasChanged = true;

			int32 CaptureIndex = Proxy->PackedIndex;
			Scene->ReflectionSceneData.RegisteredReflectionCaptures.RemoveAtSwap(CaptureIndex);
			Scene->ReflectionSceneData.RegisteredReflectionCapturePositions.RemoveAtSwap(CaptureIndex);

			if (Scene->ReflectionSceneData.RegisteredReflectionCaptures.IsValidIndex(CaptureIndex))
			{
				FReflectionCaptureProxy* OtherCapture = Scene->ReflectionSceneData.RegisteredReflectionCaptures[CaptureIndex];
				OtherCapture->PackedIndex = CaptureIndex;
			}

			delete Proxy;

			checkSlow(Scene->ReflectionSceneData.RegisteredReflectionCaptures.Num() == Scene->ReflectionSceneData.RegisteredReflectionCapturePositions.Num());
		});

		// Disassociate the primitive's scene proxy.
		Component->SceneProxy = NULL;
	}
}

void FScene::UpdateReflectionCaptureTransform(UReflectionCaptureComponent* Component)
{
	if (Component->SceneProxy)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
			UpdateTransformCommand,
			FReflectionCaptureProxy*,Proxy,Component->SceneProxy,
			FMatrix,Transform,Component->ComponentToWorld.ToMatrixWithScale(),
			FScene*,Scene,this,
		{
			Scene->ReflectionSceneData.bRegisteredReflectionCapturesHasChanged = true;
			Proxy->SetTransform(Transform);
		});
	}
}

void FScene::ReleaseReflectionCubemap(UReflectionCaptureComponent* CaptureComponent)
{
	for (TSparseArray<UReflectionCaptureComponent*>::TIterator It(ReflectionSceneData.AllocatedReflectionCapturesGameThread); It; ++It)
	{
		UReflectionCaptureComponent* CurrentCapture = *It;

		if (CurrentCapture == CaptureComponent)
		{
			It.RemoveCurrent();
			break;
		}
	}

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		RemoveCaptureCommand,
		UReflectionCaptureComponent*,Component,CaptureComponent,
		FScene*,Scene,this,
	{
		Scene->ReflectionSceneData.AllocatedReflectionCaptureState.Remove(Component);
	});
}

const FReflectionCaptureProxy* FScene::FindClosestReflectionCapture(FVector Position) const
{
	checkSlow(IsInParallelRenderingThread());
	int32 ClosestCaptureIndex = INDEX_NONE;
	float ClosestDistanceSquared = FLT_MAX;

	// Linear search through the scene's reflection captures
	// ReflectionSceneData.RegisteredReflectionCapturePositions has been packed densely to make this coherent in memory
	for (int32 CaptureIndex = 0; CaptureIndex < ReflectionSceneData.RegisteredReflectionCapturePositions.Num(); CaptureIndex++)
	{
		const float DistanceSquared = (ReflectionSceneData.RegisteredReflectionCapturePositions[CaptureIndex] - Position).SizeSquared();

		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestCaptureIndex = CaptureIndex;
		}
	}

	return ClosestCaptureIndex != INDEX_NONE ? ReflectionSceneData.RegisteredReflectionCaptures[ClosestCaptureIndex] : NULL;
}

void FScene::GetCaptureParameters(const FReflectionCaptureProxy* ReflectionProxy, FTextureRHIParamRef& ReflectionCubemapArray, int32& ArrayIndex) const
{
	ERHIFeatureLevel::Type LocalFeatureLevel = GetFeatureLevel();

	if (LocalFeatureLevel >= ERHIFeatureLevel::SM5)
	{
		const FCaptureComponentSceneState* FoundState = ReflectionSceneData.AllocatedReflectionCaptureState.Find(ReflectionProxy->Component);

		if (FoundState)
		{
			ReflectionCubemapArray = ReflectionSceneData.CubemapArray.GetRenderTarget().ShaderResourceTexture;
			ArrayIndex = FoundState->CaptureIndex;
		}
	}
	else if (ReflectionProxy->SM4FullHDRCubemap)
	{
		ReflectionCubemapArray = ReflectionProxy->SM4FullHDRCubemap->TextureRHI;
		ArrayIndex = 0;
	}
}

void FScene::AddPrecomputedLightVolume(const FPrecomputedLightVolume* Volume)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		AddVolumeCommand,
		const FPrecomputedLightVolume*,Volume,Volume,
		FScene*,Scene,this,
	{
		Scene->PrecomputedLightVolumes.Add(Volume);
		Scene->IndirectLightingCache.SetLightingCacheDirty();
	});
}

void FScene::RemovePrecomputedLightVolume(const FPrecomputedLightVolume* Volume)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		RemoveVolumeCommand,
		const FPrecomputedLightVolume*,Volume,Volume,
		FScene*,Scene,this,
	{
		Scene->PrecomputedLightVolumes.Remove(Volume);
		Scene->IndirectLightingCache.SetLightingCacheDirty();
	});
}

struct FUpdateLightTransformParameters
{
	FMatrix LightToWorld;
	FVector4 Position;
};

void FScene::UpdateLightTransform_RenderThread(FLightSceneInfo* LightSceneInfo, const FUpdateLightTransformParameters& Parameters)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateSceneLightTime);
	if( LightSceneInfo && LightSceneInfo->bVisible )
	{
		// Don't remove directional lights when their transform changes as nothing in RemoveFromScene() depends on their transform
		if (!(LightSceneInfo->Proxy->GetLightType() == LightType_Directional))
		{
			// Remove the light from the scene.
			LightSceneInfo->RemoveFromScene();
		}

		// Update the light's transform and position.
		LightSceneInfo->Proxy->SetTransform(Parameters.LightToWorld,Parameters.Position);

		// Also update the LightSceneInfoCompact
		if( LightSceneInfo->Id != INDEX_NONE )
		{
			LightSceneInfo->Scene->Lights[LightSceneInfo->Id].Init(LightSceneInfo);

			// Don't re-add directional lights when their transform changes as nothing in AddToScene() depends on their transform
			if (!(LightSceneInfo->Proxy->GetLightType() == LightType_Directional))
			{
				// Add the light to the scene at its new location.
				LightSceneInfo->AddToScene();
			}
		}
	}
}

void FScene::UpdateLightTransform(ULightComponent* Light)
{
	if(Light->SceneProxy)
	{
		FUpdateLightTransformParameters Parameters;
		Parameters.LightToWorld = Light->ComponentToWorld.ToMatrixNoScale();
		Parameters.Position = Light->GetLightPosition();
		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
			UpdateLightTransform,
			FScene*,Scene,this,
			FLightSceneInfo*,LightSceneInfo,Light->SceneProxy->GetLightSceneInfo(),
			FUpdateLightTransformParameters,Parameters,Parameters,
			{
				FScopeCycleCounter Context(LightSceneInfo->Proxy->GetStatId());
				Scene->UpdateLightTransform_RenderThread(LightSceneInfo, Parameters);
			});
	}
}

/** 
 * Updates the color and brightness of a light which has already been added to the scene. 
 *
 * @param Light - light component to update
 */
void FScene::UpdateLightColorAndBrightness(ULightComponent* Light)
{
	if(Light->SceneProxy)
	{
		struct FUpdateLightColorParameters
		{
			FLinearColor NewColor;
			float NewIndirectLightingScale;
		};

		FUpdateLightColorParameters NewParameters;
		NewParameters.NewColor = FLinearColor(Light->LightColor) * Light->ComputeLightBrightness();
		NewParameters.NewIndirectLightingScale = Light->IndirectLightingIntensity;
	
		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
			UpdateLightColorAndBrightness,
			FLightSceneInfo*,LightSceneInfo,Light->SceneProxy->GetLightSceneInfo(),
			FScene*,Scene,this,
			FUpdateLightColorParameters,Parameters,NewParameters,
			{
				if( LightSceneInfo && LightSceneInfo->bVisible )
				{
					LightSceneInfo->Proxy->SetColor(Parameters.NewColor);
					LightSceneInfo->Proxy->IndirectLightingScale = Parameters.NewIndirectLightingScale;

					// Also update the LightSceneInfoCompact
					if( LightSceneInfo->Id != INDEX_NONE )
					{
						Scene->Lights[ LightSceneInfo->Id ].Color = Parameters.NewColor;
					}
				}
			});
	}
}

/** Updates the scene's dynamic skylight. */
void FScene::UpdateDynamicSkyLight(const FLinearColor& UpperColor, const FLinearColor& LowerColor)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		UpdateDynamicSkyLight,
		FScene*,Scene,this,
		FLinearColor,UpperColor,UpperColor,
		FLinearColor,LowerColor,LowerColor,
	{
		Scene->UpperDynamicSkylightColor = UpperColor;
		Scene->LowerDynamicSkylightColor = LowerColor;
	});
}

void FScene::RemoveLightSceneInfo_RenderThread(FLightSceneInfo* LightSceneInfo)
{
	SCOPE_CYCLE_COUNTER(STAT_RemoveSceneLightTime);

	if (LightSceneInfo->bVisible)
	{
		if (LightSceneInfo == SimpleDirectionalLight)
		{
			// if we are forward rendered and this light is a dynamic shadowcast then we need to update the static draw lists to pick a new lightingpolicy
			bScenesPrimitivesNeedStaticMeshElementUpdate = bScenesPrimitivesNeedStaticMeshElementUpdate  || (!ShouldUseDeferredRenderer() && !SimpleDirectionalLight->Proxy->HasStaticShadowing());
			SimpleDirectionalLight = NULL;
		}

		if (LightSceneInfo == SunLight)
		{
			SunLight = NULL;
			// Search for new sun light...
			for (TSparseArray<FLightSceneInfoCompact>::TConstIterator It(Lights); It; ++It)
			{
				const FLightSceneInfoCompact& LightInfo = *It;
				if (LightInfo.LightSceneInfo != LightSceneInfo
					&& LightInfo.LightSceneInfo->Proxy->bUsedAsAtmosphereSunLight
					&& (!SunLight || SunLight->Proxy->GetColor().ComputeLuminance() < LightInfo.LightSceneInfo->Proxy->GetColor().ComputeLuminance()) )
				{
					SunLight = LightInfo.LightSceneInfo;
				}
			}
		}

		// Remove the light from the scene.
		LightSceneInfo->RemoveFromScene();

		// Remove the light from the lights list.
		Lights.RemoveAt(LightSceneInfo->Id);
	}
	else
	{
		InvisibleLights.RemoveAt(LightSceneInfo->Id);
	}

	// Free the light scene info and proxy.
	delete LightSceneInfo->Proxy;
	delete LightSceneInfo;
}

void FScene::RemoveLight(ULightComponent* Light)
{
	if(Light->SceneProxy)
	{
		FLightSceneInfo* LightSceneInfo = Light->SceneProxy->GetLightSceneInfo();

		DEC_DWORD_STAT(STAT_SceneLights);

		// Removing one visible light
		--NumVisibleLights;

		// Disassociate the primitive's render info.
		Light->SceneProxy = NULL;

		// Send a command to the rendering thread to remove the light from the scene.
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			FRemoveLightCommand,
			FScene*,Scene,this,
			FLightSceneInfo*,LightSceneInfo,LightSceneInfo,
			{
				FScopeCycleCounter Context(LightSceneInfo->Proxy->GetStatId());
				Scene->RemoveLightSceneInfo_RenderThread(LightSceneInfo);
			});
	}
}

void FScene::AddExponentialHeightFog(UExponentialHeightFogComponent* FogComponent)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FAddFogCommand,
		FScene*,Scene,this,
		FExponentialHeightFogSceneInfo,HeightFogSceneInfo,FExponentialHeightFogSceneInfo(FogComponent),
		{
			// Create a FExponentialHeightFogSceneInfo for the component in the scene's fog array.
			new(Scene->ExponentialFogs) FExponentialHeightFogSceneInfo(HeightFogSceneInfo);
		});
}

void FScene::RemoveExponentialHeightFog(UExponentialHeightFogComponent* FogComponent)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FRemoveFogCommand,
		FScene*,Scene,this,
		UExponentialHeightFogComponent*,FogComponent,FogComponent,
		{
			// Remove the given component's FExponentialHeightFogSceneInfo from the scene's fog array.
			for(int32 FogIndex = 0;FogIndex < Scene->ExponentialFogs.Num();FogIndex++)
			{
				if(Scene->ExponentialFogs[FogIndex].Component == FogComponent)
				{
					Scene->ExponentialFogs.RemoveAt(FogIndex);
					break;
				}
			}
		});
}

void FScene::AddAtmosphericFog(UAtmosphericFogComponent* FogComponent)
{
	check(FogComponent);

	FAtmosphericFogSceneInfo* FogSceneInfo = new FAtmosphericFogSceneInfo(FogComponent, this);

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FAddAtmosphericFogCommand,
		FScene*,Scene,this,
		FAtmosphericFogSceneInfo*,FogSceneInfo,FogSceneInfo,
	{
		if (Scene->AtmosphericFog && Scene->AtmosphericFog->Component != FogSceneInfo->Component)
		{
			delete Scene->AtmosphericFog;
			Scene->AtmosphericFog = NULL;
		}

		if (Scene->AtmosphericFog == NULL)
		{
			Scene->AtmosphericFog = FogSceneInfo;
		}
		else
		{
			delete FogSceneInfo;
		}
	});
}

void FScene::RemoveAtmosphericFog(UAtmosphericFogComponent* FogComponent)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FRemoveAtmosphericFogCommand,
		FScene*,Scene,this,
		UAtmosphericFogComponent*,FogComponent,FogComponent,
	{
		// Remove the given component's FExponentialHeightFogSceneInfo from the scene's fog array.
		if (Scene->AtmosphericFog && Scene->AtmosphericFog->Component == FogComponent)
		{
			delete Scene->AtmosphericFog;
			Scene->AtmosphericFog = NULL;
		}
	});
}

void FScene::AddWindSource(UWindDirectionalSourceComponent* WindComponent)
{
	// if this wind component is not activated (or Auto Active is set to false), then don't add to WindSources
	if(!WindComponent->IsActive())
	{
		return;
	}

	FWindSourceSceneProxy* SceneProxy = WindComponent->CreateSceneProxy();
	WindComponent->SceneProxy = SceneProxy;

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FAddWindSourceCommand,
		FScene*,Scene,this,
		FWindSourceSceneProxy*,SceneProxy,SceneProxy,
		{
			Scene->WindSources.Add(SceneProxy);
		});
}

void FScene::RemoveWindSource(UWindDirectionalSourceComponent* WindComponent)
{
	FWindSourceSceneProxy* SceneProxy = WindComponent->SceneProxy;
	WindComponent->SceneProxy = NULL;

	if(SceneProxy)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			FRemoveWindSourceCommand,
			FScene*,Scene,this,
			FWindSourceSceneProxy*,SceneProxy,SceneProxy,
			{
				Scene->WindSources.Remove(SceneProxy);

				delete SceneProxy;
			});
	}
}

const TArray<FWindSourceSceneProxy*>& FScene::GetWindSources_RenderThread() const
{
	checkSlow(IsInRenderingThread());
	return WindSources;
}

FVector4 FScene::GetWindParameters(const FVector& Position) const
{
	int32 NumActiveWindSources = 0;
	FVector4 AccumulatedDirectionAndSpeed(0,0,0,0);
	float TotalWeight = 0.0f;
	for (int32 i = 0; i < WindSources.Num(); i++)
	{
		FVector4 CurrentDirectionAndSpeed;
		float Weight;
		const FWindSourceSceneProxy* CurrentSource = WindSources[i];
		if (CurrentSource->GetWindParameters(Position, CurrentDirectionAndSpeed, Weight))
		{
			AccumulatedDirectionAndSpeed.X += CurrentDirectionAndSpeed.X * Weight;
			AccumulatedDirectionAndSpeed.Y += CurrentDirectionAndSpeed.Y * Weight;
			AccumulatedDirectionAndSpeed.Z += CurrentDirectionAndSpeed.Z * Weight;
			AccumulatedDirectionAndSpeed.W += CurrentDirectionAndSpeed.W * Weight;
			TotalWeight += Weight;
			NumActiveWindSources++;
		}
	}

	if (TotalWeight > 0)
	{
		AccumulatedDirectionAndSpeed.X /= TotalWeight;
		AccumulatedDirectionAndSpeed.Y /= TotalWeight;
		AccumulatedDirectionAndSpeed.Z /= TotalWeight;
		AccumulatedDirectionAndSpeed.W /= TotalWeight;
	}

	// Normalize averaged direction and speed
	return NumActiveWindSources > 0 ? AccumulatedDirectionAndSpeed / NumActiveWindSources : FVector4(0,0,0,0);
}

FVector4 FScene::GetDirectionalWindParameters(void) const
{
	int32 NumActiveWindSources = 0;
	FVector4 AccumulatedDirectionAndSpeed(0,0,0,0);
	float TotalWeight = 0.0f;
	for (int32 i = 0; i < WindSources.Num(); i++)
	{
		FVector4 CurrentDirectionAndSpeed;
		float Weight;
		const FWindSourceSceneProxy* CurrentSource = WindSources[i];
		if (CurrentSource->GetDirectionalWindParameters(CurrentDirectionAndSpeed, Weight))
		{
			AccumulatedDirectionAndSpeed.X += CurrentDirectionAndSpeed.X * Weight;
			AccumulatedDirectionAndSpeed.Y += CurrentDirectionAndSpeed.Y * Weight;
			AccumulatedDirectionAndSpeed.Z += CurrentDirectionAndSpeed.Z * Weight;
			AccumulatedDirectionAndSpeed.W += CurrentDirectionAndSpeed.W * Weight;
			TotalWeight += Weight;
			NumActiveWindSources++;
		}
	}

	if (TotalWeight > 0)
	{
		AccumulatedDirectionAndSpeed.X /= TotalWeight;
		AccumulatedDirectionAndSpeed.Y /= TotalWeight;
		AccumulatedDirectionAndSpeed.Z /= TotalWeight;
		AccumulatedDirectionAndSpeed.W /= TotalWeight;
	}

	// Normalize averaged direction and speed
	return NumActiveWindSources > 0 ? AccumulatedDirectionAndSpeed / NumActiveWindSources : FVector4(0,0,1,0);
}

void FScene::AddSpeedTreeWind(FVertexFactory* VertexFactory, const UStaticMesh* StaticMesh)
{
	if (StaticMesh != NULL && StaticMesh->SpeedTreeWind.IsValid() && StaticMesh->RenderData.IsValid())
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
			FAddSpeedTreeWindCommand,
			FScene*,Scene,this,
			const UStaticMesh*,StaticMesh,StaticMesh,
			FVertexFactory*,VertexFactory,VertexFactory,
			{
				Scene->SpeedTreeVertexFactoryMap.Add(VertexFactory, StaticMesh);

				if (Scene->SpeedTreeWindComputationMap.Contains(StaticMesh))
				{
					(*(Scene->SpeedTreeWindComputationMap.Find(StaticMesh)))->ReferenceCount++;
				}
				else
				{
					UE_LOG(LogRenderer, Log, TEXT("Adding SpeedTree wind for static mesh %s"), *StaticMesh->GetName());
					FSpeedTreeWindComputation* WindComputation = new FSpeedTreeWindComputation;
					WindComputation->Wind = *(StaticMesh->SpeedTreeWind.Get( ));
					WindComputation->UniformBuffer.InitResource();
					Scene->SpeedTreeWindComputationMap.Add(StaticMesh, WindComputation);
				}
			});
	}
}

void FScene::RemoveSpeedTreeWind(class FVertexFactory* VertexFactory, const class UStaticMesh* StaticMesh)
{
	if (StaticMesh != NULL && StaticMesh->SpeedTreeWind.IsValid() && StaticMesh->RenderData.IsValid())
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
			FRemoveSpeedTreeWindCommand,
			FScene*,Scene,this,
			const UStaticMesh*, StaticMesh, StaticMesh,
			FVertexFactory*,VertexFactory,VertexFactory,
			{
				Scene->RemoveSpeedTreeWind_RenderThread(VertexFactory, StaticMesh);
			});
	}
}

void FScene::RemoveSpeedTreeWind_RenderThread(class FVertexFactory* VertexFactory, const class UStaticMesh* StaticMesh)
{
	FSpeedTreeWindComputation** WindComputationRef = SpeedTreeWindComputationMap.Find(StaticMesh);
	if (WindComputationRef != NULL)
	{
		FSpeedTreeWindComputation* WindComputation = *WindComputationRef;

		WindComputation->ReferenceCount--;
		if (WindComputation->ReferenceCount < 1)
		{
			for (auto Iter = SpeedTreeVertexFactoryMap.CreateIterator(); Iter; ++Iter )
			{
				if (Iter.Value() == StaticMesh)
				{
					Iter.RemoveCurrent();
				}
			}

			SpeedTreeWindComputationMap.Remove(StaticMesh);
			WindComputation->UniformBuffer.ReleaseResource();
			delete WindComputation;
		}
	}
}

void FScene::UpdateSpeedTreeWind(double CurrentTime)
{
	#define SET_SPEEDTREE_TABLE_FLOAT4V(name, offset) UniformParameters.name = *(FVector4*)(WindShaderValues + FSpeedTreeWind::offset);

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FUpdateSpeedTreeWindCommand,
		FScene*,Scene,this,
		double,CurrentTime,CurrentTime,
		{
			FVector4 WindInfo = Scene->GetDirectionalWindParameters();

			for (TMap<const UStaticMesh*, FSpeedTreeWindComputation*>::TIterator It(Scene->SpeedTreeWindComputationMap); It; ++It )
			{
				const UStaticMesh* StaticMesh = It.Key();
				FSpeedTreeWindComputation* WindComputation = It.Value();

				if( !StaticMesh->RenderData )
				{
					It.RemoveCurrent();
					continue;
				}

				if (GIsEditor && StaticMesh->SpeedTreeWind->NeedsReload( ))
				{
					// reload the wind since it may have changed or been scaled differently during reimport
					StaticMesh->SpeedTreeWind->SetNeedsReload(false);
					WindComputation->Wind = *(StaticMesh->SpeedTreeWind.Get( ));

					// make sure the vertex factories are registered (sometimes goes wrong during a reimport)
					for (int32 LODIndex = 0; LODIndex < StaticMesh->RenderData->LODResources.Num(); ++LODIndex)
					{
						Scene->SpeedTreeVertexFactoryMap.Add(&StaticMesh->RenderData->LODResources[LODIndex].VertexFactory, StaticMesh);
					}
				}

				// advance the wind object
				WindComputation->Wind.SetDirection(FVector(WindInfo));
				WindComputation->Wind.SetStrength(WindInfo.W);
				WindComputation->Wind.Advance(true, CurrentTime);

				// copy data into uniform buffer
				const float* WindShaderValues = WindComputation->Wind.GetShaderTable();

				FSpeedTreeUniformParameters UniformParameters;
				UniformParameters.WindAnimation.Set(CurrentTime, 0.0f, 0.0f, 0.0f);
			
				SET_SPEEDTREE_TABLE_FLOAT4V(WindVector, SH_WIND_DIR_X);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindGlobal, SH_GLOBAL_TIME);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindBranch, SH_BRANCH_1_TIME);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindBranchTwitch, SH_BRANCH_1_TWITCH);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindBranchWhip, SH_BRANCH_1_WHIP);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindBranchAnchor, SH_WIND_ANCHOR_X);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindBranchAdherences, SH_GLOBAL_DIRECTION_ADHERENCE);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindTurbulences, SH_BRANCH_1_TURBULENCE);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindLeaf1Ripple, SH_LEAF_1_RIPPLE_TIME);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindLeaf1Tumble, SH_LEAF_1_TUMBLE_TIME);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindLeaf1Twitch, SH_LEAF_1_TWITCH_THROW);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindLeaf2Ripple, SH_LEAF_2_RIPPLE_TIME);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindLeaf2Tumble, SH_LEAF_2_TUMBLE_TIME);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindLeaf2Twitch, SH_LEAF_2_TWITCH_THROW);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindFrondRipple, SH_FROND_RIPPLE_TIME);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindRollingBranch, SH_ROLLING_BRANCH_FIELD_MIN);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindRollingLeafAndDirection, SH_ROLLING_LEAF_RIPPLE_MIN);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindRollingNoise, SH_ROLLING_NOISE_PERIOD);

				WindComputation->UniformBuffer.SetContents(UniformParameters);
			}
		});
	
	#undef SET_SPEEDTREE_TABLE_FLOAT4V
}

FUniformBufferRHIParamRef FScene::GetSpeedTreeUniformBuffer(const FVertexFactory* VertexFactory)
{
	if (VertexFactory != NULL)
	{
		const UStaticMesh** StaticMesh = SpeedTreeVertexFactoryMap.Find(VertexFactory);
		if (StaticMesh != NULL)
		{
			FSpeedTreeWindComputation** WindComputation = SpeedTreeWindComputationMap.Find(*StaticMesh);
			if (WindComputation != NULL)
			{
				return (*WindComputation)->UniformBuffer.GetUniformBufferRHI();
			}
		}
	}

	return FUniformBufferRHIParamRef();
}

/**
 * Retrieves the lights interacting with the passed in primitive and adds them to the out array.
 *
 * Render thread version of function.
 * 
 * @param	Primitive				Primitive to retrieve interacting lights for
 * @param	RelevantLights	[out]	Array of lights interacting with primitive
 */
void FScene::GetRelevantLights_RenderThread( UPrimitiveComponent* Primitive, TArray<const ULightComponent*>* RelevantLights ) const
{
	check( Primitive );
	check( RelevantLights );
	if( Primitive->SceneProxy )
	{
		for( const FLightPrimitiveInteraction* Interaction=Primitive->SceneProxy->GetPrimitiveSceneInfo()->LightList; Interaction; Interaction=Interaction->GetNextLight() )
		{
			RelevantLights->Add( Interaction->GetLight()->Proxy->GetLightComponent() );
		}
	}
}

/**
 * Retrieves the lights interacting with the passed in primitive and adds them to the out array.
 *
 * @param	Primitive				Primitive to retrieve interacting lights for
 * @param	RelevantLights	[out]	Array of lights interacting with primitive
 */
void FScene::GetRelevantLights( UPrimitiveComponent* Primitive, TArray<const ULightComponent*>* RelevantLights ) const
{
	if( Primitive && RelevantLights )
	{
		// Add interacting lights to the array.
		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
			FGetRelevantLightsCommand,
			const FScene*,Scene,this,
			UPrimitiveComponent*,Primitive,Primitive,
			TArray<const ULightComponent*>*,RelevantLights,RelevantLights,
			{
				Scene->GetRelevantLights_RenderThread( Primitive, RelevantLights );
			});

		// We need to block the main thread as the rendering thread needs to finish modifying the array before we can continue.
		FlushRenderingCommands();
	}
}

/** Sets the precomputed visibility handler for the scene, or NULL to clear the current one. */
void FScene::SetPrecomputedVisibility(const FPrecomputedVisibilityHandler* PrecomputedVisibilityHandler)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		UpdatePrecomputedVisibility,
		FScene*,Scene,this,
		const FPrecomputedVisibilityHandler*,PrecomputedVisibilityHandler,PrecomputedVisibilityHandler,
	{
		Scene->PrecomputedVisibilityHandler = PrecomputedVisibilityHandler;
	});
}

void FScene::SetShaderMapsOnMaterialResources_RenderThread(FRHICommandListImmediate& RHICmdList, const FMaterialsToUpdateMap& MaterialsToUpdate)
{
	SCOPE_CYCLE_COUNTER(STAT_Scene_SetShaderMapsOnMaterialResources_RT);

	TArray<const FMaterial*> MaterialArray;

	for (FMaterialsToUpdateMap::TConstIterator It(MaterialsToUpdate); It; ++It)
	{
		FMaterial* Material = It.Key();
		FMaterialShaderMap* ShaderMap = It.Value();
		Material->SetRenderingThreadShaderMap(ShaderMap);
		check(!ShaderMap || ShaderMap->IsValidForRendering());
		MaterialArray.Add(Material);
	}

	const auto FeatureLevel = GetFeatureLevel();
	bool bFoundAnyInitializedMaterials = false;

	// Iterate through all loaded material render proxies and recache their uniform expressions if needed
	// This search does not scale well, but is only used when uploading async shader compile results
	for (TSet<FMaterialRenderProxy*>::TConstIterator It(FMaterialRenderProxy::GetMaterialRenderProxyMap()); It; ++It)
	{
		FMaterialRenderProxy* MaterialProxy = *It;
		FMaterial* Material = MaterialProxy->GetMaterialNoFallback(FeatureLevel);

		if (Material && MaterialsToUpdate.Contains(Material))
		{
			// Materials used as async fallbacks can't be updated through this mechanism and should have been updated synchronously earlier
			check(!Material->RequiresSynchronousCompilation());
			MaterialProxy->CacheUniformExpressions();
			bFoundAnyInitializedMaterials = true;

			const FMaterial& MaterialForRendering = *MaterialProxy->GetMaterial(FeatureLevel);
			check(MaterialForRendering.GetRenderingThreadShaderMap());

			check(!MaterialProxy->UniformExpressionCache[FeatureLevel].bUpToDate
				|| MaterialProxy->UniformExpressionCache[FeatureLevel].CachedUniformExpressionShaderMap == MaterialForRendering.GetRenderingThreadShaderMap());

			check(MaterialForRendering.GetRenderingThreadShaderMap()->IsValidForRendering());
		}
	}

	// Update static draw lists, which cache shader references from materials, but the shader map has now changed
	if (bFoundAnyInitializedMaterials)
	{
		UpdateStaticDrawListsForMaterials_RenderThread(RHICmdList, MaterialArray);
	}
}

void FScene::SetShaderMapsOnMaterialResources(const TMap<FMaterial*, class FMaterialShaderMap*>& MaterialsToUpdate)
{
	for (TMap<FMaterial*, FMaterialShaderMap*>::TConstIterator It(MaterialsToUpdate); It; ++It)
	{
		FMaterial* Material = It.Key();
		check(!Material->RequiresSynchronousCompilation());
	}

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FSetShaderMapOnMaterialResources,
		FScene*,Scene,this,
		FMaterialsToUpdateMap,MaterialsToUpdate,MaterialsToUpdate,
	{
		Scene->SetShaderMapsOnMaterialResources_RenderThread(RHICmdList, MaterialsToUpdate);
	});
}

void FScene::UpdateStaticDrawListsForMaterials_RenderThread(FRHICommandListImmediate& RHICmdList, const TArray<const FMaterial*>& Materials)
{
	SCOPE_CYCLE_COUNTER(STAT_Scene_UpdateStaticDrawListsForMaterials_RT);

	// Warning: if any static draw lists are missed here, there will be a crash when trying to render with shaders that have been deleted!
	TArray<FPrimitiveSceneInfo*> PrimitivesToUpdate;
	auto FeatureLevel = GetFeatureLevel();
	for (int32 DrawType = 0; DrawType < EBasePass_MAX; DrawType++)
	{
		BasePassNoLightMapDrawList[DrawType].GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
		BasePassSimpleDynamicLightingDrawList[DrawType].GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
		BasePassCachedVolumeIndirectLightingDrawList[DrawType].GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
		BasePassCachedPointIndirectLightingDrawList[DrawType].GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
		BasePassHighQualityLightMapDrawList[DrawType].GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
		BasePassDistanceFieldShadowMapLightMapDrawList[DrawType].GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
		BasePassLowQualityLightMapDrawList[DrawType].GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
		BasePassSelfShadowedTranslucencyDrawList[DrawType].GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
		BasePassSelfShadowedCachedPointIndirectTranslucencyDrawList[DrawType].GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);

		BasePassForForwardShadingNoLightMapDrawList[DrawType].GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
		BasePassForForwardShadingLowQualityLightMapDrawList[DrawType].GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
		BasePassForForwardShadingDistanceFieldShadowMapLightMapDrawList[DrawType].GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
		BasePassForForwardShadingDirectionalLightAndSHIndirectDrawList[DrawType].GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
		BasePassForForwardShadingMovableDirectionalLightDrawList[DrawType].GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
		BasePassForForwardShadingMovableDirectionalLightCSMDrawList[DrawType].GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
		BasePassForForwardShadingMovableDirectionalLightLightmapDrawList[DrawType].GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
		BasePassForForwardShadingMovableDirectionalLightCSMLightmapDrawList[DrawType].GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
	}

	PositionOnlyDepthDrawList.GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
	DepthDrawList.GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
	MaskedDepthDrawList.GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
	HitProxyDrawList.GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
	HitProxyDrawList_OpaqueOnly.GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
	VelocityDrawList.GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
	WholeSceneShadowDepthDrawList.GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);
	WholeSceneReflectiveShadowMapDrawList.GetUsedPrimitivesBasedOnMaterials(FeatureLevel, Materials, PrimitivesToUpdate);

	for (int32 PrimitiveIndex = 0; PrimitiveIndex < PrimitivesToUpdate.Num(); PrimitiveIndex++)
	{
		FPrimitiveSceneInfo* Primitive = PrimitivesToUpdate[PrimitiveIndex];

		Primitive->RemoveStaticMeshes();
		Primitive->AddStaticMeshes(RHICmdList);
	}
}

void FScene::UpdateStaticDrawListsForMaterials(const TArray<const FMaterial*>& Materials)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FUpdateDrawLists,
		FScene*,Scene,this,
		TArray<const FMaterial*>,Materials,Materials,
	{
		Scene->UpdateStaticDrawListsForMaterials_RenderThread(RHICmdList, Materials);
	});
}

/**
 * @return		true if hit proxies should be rendered in this scene.
 */
bool FScene::RequiresHitProxies() const
{
	return (GIsEditor && bRequiresHitProxies);
}

void FScene::Release()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Verify that no components reference this scene being destroyed
	static bool bTriggeredOnce = false;

	if (!bTriggeredOnce)
	{
		for (auto* ActorComponent : TObjectRange<UActorComponent>())
		{
			if ( !ensureMsg(!ActorComponent->IsRegistered() || ActorComponent->GetScene() != this, 
					*FString::Printf(TEXT("Component Name: %s World Name: %s Component Mesh: %s"), 
										*ActorComponent->GetFullName(), 
										*GetWorld()->GetFullName(), 
										Cast<UStaticMeshComponent>(ActorComponent) ? *CastChecked<UStaticMeshComponent>(ActorComponent)->StaticMesh->GetFullName() : TEXT("Not a static mesh"))) )
			{
				bTriggeredOnce = true;
				break;	
			}
		}
	}
#endif

	GetRendererModule().RemoveScene(this);

	// Send a command to the rendering thread to release the scene.
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		FReleaseCommand,
		FScene*,Scene,this,
		{
			delete Scene;
		});
}

void FScene::ConditionalMarkStaticMeshElementsForUpdate()
{
	static auto* EarlyZPassCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.EarlyZPass"));

	bool bMobileHDR = IsMobileHDR();
	bool bMobileHDR32bpp = IsMobileHDR32bpp();
	int32 DesiredStaticDrawListsEarlyZPassMode = EarlyZPassCvar->GetValueOnRenderThread();

	if (bScenesPrimitivesNeedStaticMeshElementUpdate
		|| bStaticDrawListsMobileHDR != bMobileHDR
		|| bStaticDrawListsMobileHDR32bpp != bMobileHDR32bpp
		|| StaticDrawListsEarlyZPassMode != DesiredStaticDrawListsEarlyZPassMode)
	{
		// Mark all primitives as needing an update
		// Note: Only visible primitives will actually update their static mesh elements
		for (int32 PrimitiveIndex = 0; PrimitiveIndex < Primitives.Num(); PrimitiveIndex++)
		{
			Primitives[PrimitiveIndex]->BeginDeferredUpdateStaticMeshes();
		}

		bScenesPrimitivesNeedStaticMeshElementUpdate = false;
		bStaticDrawListsMobileHDR = bMobileHDR;
		bStaticDrawListsMobileHDR32bpp = bMobileHDR32bpp;
		StaticDrawListsEarlyZPassMode = DesiredStaticDrawListsEarlyZPassMode;
	}
}

void FScene::DumpUnbuiltLightIteractions( FOutputDevice& Ar ) const
{
	FlushRenderingCommands();

	TArray<FString> LightsWithUnbuiltInteractions;
	TArray<FString> PrimitivesWithUnbuiltInteractions;

	// if want to print out all of the lights
	for( TSparseArray<FLightSceneInfoCompact>::TConstIterator It(Lights); It; ++It )
	{
		const FLightSceneInfoCompact& LightCompactInfo = *It;
		FLightSceneInfo* LightSceneInfo = LightCompactInfo.LightSceneInfo;

		bool bLightHasUnbuiltInteractions = false;

		for(FLightPrimitiveInteraction* Interaction = LightSceneInfo->DynamicPrimitiveList;
			Interaction;
			Interaction = Interaction->GetNextPrimitive())
		{
			if (Interaction->IsUncachedStaticLighting())
			{
				bLightHasUnbuiltInteractions = true;
				PrimitivesWithUnbuiltInteractions.AddUnique(Interaction->GetPrimitiveSceneInfo()->ComponentForDebuggingOnly->GetFullName());
			}
		}

		if (bLightHasUnbuiltInteractions)
		{
			LightsWithUnbuiltInteractions.AddUnique(LightSceneInfo->Proxy->GetComponentName().ToString());
		}
	}

	Ar.Logf( TEXT( "DumpUnbuiltLightIteractions" ) );
	Ar.Logf( TEXT( "Lights with unbuilt interactions: %d" ), LightsWithUnbuiltInteractions.Num() );
	for (int Index = 0; Index < LightsWithUnbuiltInteractions.Num(); Index++)
	{
		Ar.Logf(*(FString(TEXT("    Light ")) + LightsWithUnbuiltInteractions[Index]));
	}

	Ar.Logf( TEXT( "" ) );
	Ar.Logf( TEXT( "Primitives with unbuilt interactions: %d" ), PrimitivesWithUnbuiltInteractions.Num() );
	for (int Index = 0; Index < PrimitivesWithUnbuiltInteractions.Num(); Index++)
	{
		Ar.Logf(*(FString(TEXT("    Primitive ")) + PrimitivesWithUnbuiltInteractions[Index]));
	}
}

/**
 * Logs the provided draw list stats.
 */
static void LogDrawListStats(FDrawListStats Stats, const TCHAR* DrawListName)
{
	if (Stats.NumDrawingPolicies == 0 || Stats.NumMeshes == 0)
	{
		UE_LOG(LogRenderer,Log,TEXT("%s: empty"), DrawListName);
	}
	else
	{
		UE_LOG(LogRenderer,Log,
			TEXT("%s: %d policies %d meshes\n")
			TEXT("  - %d median meshes/policy\n")
			TEXT("  - %f mean meshes/policy\n")
			TEXT("  - %d max meshes/policy\n")
			TEXT("  - %d policies with one mesh"),
			DrawListName,
			Stats.NumDrawingPolicies,
			Stats.NumMeshes,
			Stats.MedianMeshesPerDrawingPolicy,
			(float)Stats.NumMeshes / (float)Stats.NumDrawingPolicies,
			Stats.MaxMeshesPerDrawingPolicy,
			Stats.NumSingleMeshDrawingPolicies
			);
	}
}

void FScene::DumpStaticMeshDrawListStats() const
{
	UE_LOG(LogRenderer,Log,TEXT("Static mesh draw lists for %s:"),
		World ? *World->GetFullName() : TEXT("[no world]")
		);
#define DUMP_DRAW_LIST(Name) LogDrawListStats(Name.GetStats(), TEXT(#Name))
	DUMP_DRAW_LIST(PositionOnlyDepthDrawList);
	DUMP_DRAW_LIST(DepthDrawList);
	DUMP_DRAW_LIST(MaskedDepthDrawList);
	DUMP_DRAW_LIST(BasePassNoLightMapDrawList[EBasePass_Default]);
	DUMP_DRAW_LIST(BasePassNoLightMapDrawList[EBasePass_Masked]);
	DUMP_DRAW_LIST(BasePassSimpleDynamicLightingDrawList[EBasePass_Default]);
	DUMP_DRAW_LIST(BasePassSimpleDynamicLightingDrawList[EBasePass_Masked]);
	DUMP_DRAW_LIST(BasePassCachedVolumeIndirectLightingDrawList[EBasePass_Default]);
	DUMP_DRAW_LIST(BasePassCachedVolumeIndirectLightingDrawList[EBasePass_Masked]);
	DUMP_DRAW_LIST(BasePassCachedPointIndirectLightingDrawList[EBasePass_Default]);
	DUMP_DRAW_LIST(BasePassCachedPointIndirectLightingDrawList[EBasePass_Masked]);
	DUMP_DRAW_LIST(BasePassHighQualityLightMapDrawList[EBasePass_Default]);
	DUMP_DRAW_LIST(BasePassHighQualityLightMapDrawList[EBasePass_Masked]);
	DUMP_DRAW_LIST(BasePassDistanceFieldShadowMapLightMapDrawList[EBasePass_Default]);
	DUMP_DRAW_LIST(BasePassDistanceFieldShadowMapLightMapDrawList[EBasePass_Masked]);
	DUMP_DRAW_LIST(BasePassLowQualityLightMapDrawList[EBasePass_Default]);
	DUMP_DRAW_LIST(BasePassLowQualityLightMapDrawList[EBasePass_Masked]);
	DUMP_DRAW_LIST(BasePassSelfShadowedTranslucencyDrawList[EBasePass_Default]);
	DUMP_DRAW_LIST(BasePassSelfShadowedTranslucencyDrawList[EBasePass_Masked]);
	DUMP_DRAW_LIST(BasePassSelfShadowedCachedPointIndirectTranslucencyDrawList[EBasePass_Default]);
	DUMP_DRAW_LIST(BasePassSelfShadowedCachedPointIndirectTranslucencyDrawList[EBasePass_Masked]);

	DUMP_DRAW_LIST(BasePassForForwardShadingNoLightMapDrawList[EBasePass_Default]);
	DUMP_DRAW_LIST(BasePassForForwardShadingNoLightMapDrawList[EBasePass_Masked]);
	DUMP_DRAW_LIST(BasePassForForwardShadingLowQualityLightMapDrawList[EBasePass_Default]);
	DUMP_DRAW_LIST(BasePassForForwardShadingLowQualityLightMapDrawList[EBasePass_Masked]);
	DUMP_DRAW_LIST(BasePassForForwardShadingDistanceFieldShadowMapLightMapDrawList[EBasePass_Default]);
	DUMP_DRAW_LIST(BasePassForForwardShadingDistanceFieldShadowMapLightMapDrawList[EBasePass_Masked]);
	DUMP_DRAW_LIST(BasePassForForwardShadingDirectionalLightAndSHIndirectDrawList[EBasePass_Default]);
	DUMP_DRAW_LIST(BasePassForForwardShadingDirectionalLightAndSHIndirectDrawList[EBasePass_Masked]);
	DUMP_DRAW_LIST(BasePassForForwardShadingMovableDirectionalLightDrawList[EBasePass_Default]);
	DUMP_DRAW_LIST(BasePassForForwardShadingMovableDirectionalLightDrawList[EBasePass_Masked]);
	DUMP_DRAW_LIST(BasePassForForwardShadingMovableDirectionalLightCSMDrawList[EBasePass_Default]);
	DUMP_DRAW_LIST(BasePassForForwardShadingMovableDirectionalLightCSMDrawList[EBasePass_Masked]);
	DUMP_DRAW_LIST(HitProxyDrawList);
	DUMP_DRAW_LIST(HitProxyDrawList_OpaqueOnly);
	DUMP_DRAW_LIST(VelocityDrawList);
	DUMP_DRAW_LIST(WholeSceneShadowDepthDrawList);
#undef DUMP_DRAW_LIST
}

/**
 * Dumps stats for all scenes to the log.
 */
static void DumpDrawListStats()
{
	for (TObjectIterator<UWorld> It; It; ++It)
	{
		UWorld* World = *It;
		if (World && World->Scene)
		{
			World->Scene->DumpStaticMeshDrawListStats();
		}
	}
}

static FAutoConsoleCommand GDumpDrawListStatsCmd(
	TEXT("r.DumpDrawListStats"),
	TEXT("Dumps static mesh draw list statistics for all scenes associated with ")
	TEXT("world objects."),
	FConsoleCommandDelegate::CreateStatic(&DumpDrawListStats)
	);

/**
 * Exports the scene.
 *
 * @param	Ar		The Archive used for exporting.
 **/
void FScene::Export( FArchive& Ar ) const
{
	
}

void FScene::ApplyWorldOffset(FVector InOffset)
{
	// Send a command to the rendering thread to shift scene data
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FApplyWorldOffset,
		FScene*,Scene,this,
		FVector,InOffset,InOffset,
	{
		Scene->ApplyWorldOffset_RenderThread(InOffset);
	});
}

// StaticMeshDrawList elements shifting
template<typename T>
static void StaticMeshDrawListApplyWorldOffset(T& InList, FVector InOffset)
{
	InList.ApplyWorldOffset(InOffset);
}

// StaticMeshDrawList elements shifting: specialization for an arrays
template<typename T, int32 N>
static void StaticMeshDrawListApplyWorldOffset(T(&InList)[N], FVector InOffset)
{
	for (int32 i = 0; i < N; i++)
	{
		InList[i].ApplyWorldOffset(InOffset);
	}
}

void FScene::ApplyWorldOffset_RenderThread(FVector InOffset)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_SceneApplyWorldOffset);
	
	// Primitives
	for (auto It = Primitives.CreateIterator(); It; ++It)
	{
		(*It)->ApplyWorldOffset(InOffset);
	}

	// Precomputed light volumes
	for (const FPrecomputedLightVolume* It : PrecomputedLightVolumes)
	{
		const_cast<FPrecomputedLightVolume*>(It)->ApplyWorldOffset(InOffset);
	}

	// Precomputed visibility
	if (PrecomputedVisibilityHandler)
	{
		const_cast<FPrecomputedVisibilityHandler*>(PrecomputedVisibilityHandler)->ApplyWorldOffset(InOffset);
	}
	
	// Invalidate indirect lighting cache
	IndirectLightingCache.SetLightingCacheDirty();

	// Primitives octree
	PrimitiveOctree.ApplyOffset(InOffset);

	// Primitive bounds
	for (auto It = PrimitiveBounds.CreateIterator(); It; ++It)
	{
		(*It).Origin+= InOffset;
	}

	// Primitive occlusion bounds
	for (auto It = PrimitiveOcclusionBounds.CreateIterator(); It; ++It)
	{
		(*It).Origin+= InOffset;
	}

	// Lights
	VectorRegister OffsetReg = VectorLoadFloat3_W0(&InOffset);
	for (auto It = Lights.CreateIterator(); It; ++It)
	{
		(*It).BoundingSphereVector = VectorAdd((*It).BoundingSphereVector, OffsetReg);
		(*It).LightSceneInfo->Proxy->ApplyWorldOffset(InOffset);
	}

	// Lights octree
	LightOctree.ApplyOffset(InOffset);

	// Cached preshadows
	for (auto It = CachedPreshadows.CreateIterator(); It; ++It)
	{
		(*It)->PreShadowTranslation-= InOffset;
		(*It)->ShadowBounds.Center+= InOffset;
	}

	// Decals
	for (auto It = Decals.CreateIterator(); It; ++It)
	{
		(*It)->ComponentTrans.AddToTranslation(InOffset);
	}

	// Wind sources
	for (auto It = WindSources.CreateIterator(); It; ++It)
	{
		(*It)->ApplyWorldOffset(InOffset);
	}

	// Reflection captures
	for (auto It = ReflectionSceneData.RegisteredReflectionCaptures.CreateIterator(); It; ++It)
	{
		FMatrix NewTransform = (*It)->BoxTransform.Inverse().ConcatTranslation(InOffset);
		(*It)->SetTransform(NewTransform);
	}

	// StaticMeshDrawLists
	StaticMeshDrawListApplyWorldOffset(PositionOnlyDepthDrawList, InOffset);
	StaticMeshDrawListApplyWorldOffset(DepthDrawList, InOffset);
	StaticMeshDrawListApplyWorldOffset(MaskedDepthDrawList, InOffset);
	StaticMeshDrawListApplyWorldOffset(BasePassNoLightMapDrawList, InOffset);
	StaticMeshDrawListApplyWorldOffset(BasePassCachedVolumeIndirectLightingDrawList, InOffset);
	StaticMeshDrawListApplyWorldOffset(BasePassCachedPointIndirectLightingDrawList, InOffset);
	StaticMeshDrawListApplyWorldOffset(BasePassSimpleDynamicLightingDrawList, InOffset);
	StaticMeshDrawListApplyWorldOffset(BasePassHighQualityLightMapDrawList, InOffset);
	StaticMeshDrawListApplyWorldOffset(BasePassDistanceFieldShadowMapLightMapDrawList, InOffset);
	StaticMeshDrawListApplyWorldOffset(BasePassLowQualityLightMapDrawList, InOffset);
	StaticMeshDrawListApplyWorldOffset(BasePassSelfShadowedTranslucencyDrawList, InOffset);
	StaticMeshDrawListApplyWorldOffset(BasePassSelfShadowedCachedPointIndirectTranslucencyDrawList, InOffset);
	StaticMeshDrawListApplyWorldOffset(HitProxyDrawList, InOffset);
	StaticMeshDrawListApplyWorldOffset(HitProxyDrawList_OpaqueOnly, InOffset);
	StaticMeshDrawListApplyWorldOffset(VelocityDrawList, InOffset);
	StaticMeshDrawListApplyWorldOffset(WholeSceneShadowDepthDrawList, InOffset);
	StaticMeshDrawListApplyWorldOffset(BasePassForForwardShadingNoLightMapDrawList, InOffset);
	StaticMeshDrawListApplyWorldOffset(BasePassForForwardShadingLowQualityLightMapDrawList, InOffset);
	StaticMeshDrawListApplyWorldOffset(BasePassForForwardShadingDirectionalLightAndSHIndirectDrawList, InOffset);
	StaticMeshDrawListApplyWorldOffset(BasePassForForwardShadingMovableDirectionalLightDrawList, InOffset);
	StaticMeshDrawListApplyWorldOffset(BasePassForForwardShadingMovableDirectionalLightCSMDrawList, InOffset);
	StaticMeshDrawListApplyWorldOffset(BasePassForForwardShadingMovableDirectionalLightLightmapDrawList, InOffset);
	StaticMeshDrawListApplyWorldOffset(BasePassForForwardShadingMovableDirectionalLightCSMLightmapDrawList, InOffset);

	// Motion blur 
	MotionBlurInfoData.ApplyOffset(InOffset);
}

void FScene::OnLevelAddedToWorld(FName InLevelName)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FLevelAddedToWorld,
		class FScene*, Scene, this,
		FName, LevelName, InLevelName,
	{
		Scene->OnLevelAddedToWorld_RenderThread(LevelName);
	});
}

void FScene::OnLevelAddedToWorld_RenderThread(FName InLevelName)
{
	// Mark level primitives
	for (auto It = Primitives.CreateIterator(); It; ++It)
	{
		FPrimitiveSceneProxy* Proxy = (*It)->Proxy;
		if (Proxy->LevelName == InLevelName)
		{
			Proxy->bIsComponentLevelVisible = true;
		}
	}
}

/**
 * Dummy NULL scene interface used by dedicated servers.
 */
class FNULLSceneInterface : public FSceneInterface
{
public:
	FNULLSceneInterface(UWorld* InWorld, bool bCreateFXSystem )
		:	World( InWorld )
		,	FXSystem( NULL )
	{
		World->Scene = this;

		if (bCreateFXSystem)
		{
			World->CreateFXSystem();
		}
		else
		{
			World->FXSystem = NULL;
			SetFXSystem(NULL);
		}
	}

	virtual void AddPrimitive(UPrimitiveComponent* Primitive){}
	virtual void RemovePrimitive(UPrimitiveComponent* Primitive){}
	virtual void ReleasePrimitive(UPrimitiveComponent* Primitive){}

	/** Updates the transform of a primitive which has already been added to the scene. */
	virtual void UpdatePrimitiveTransform(UPrimitiveComponent* Primitive){}
	virtual void UpdatePrimitiveAttachment(UPrimitiveComponent* Primitive) {};

	virtual void AddLight(ULightComponent* Light){}
	virtual void RemoveLight(ULightComponent* Light){}
	virtual void AddInvisibleLight(ULightComponent* Light){}
	virtual void SetSkyLight(FSkyLightSceneProxy* Light) {}

	virtual void AddDecal(UDecalComponent*){}
	virtual void RemoveDecal(UDecalComponent*){}
	virtual void UpdateDecalTransform(UDecalComponent* Decal) override {}

	/** Updates the transform of a light which has already been added to the scene. */
	virtual void UpdateLightTransform(ULightComponent* Light){}
	virtual void UpdateLightColorAndBrightness(ULightComponent* Light){}

	virtual void AddExponentialHeightFog(class UExponentialHeightFogComponent* FogComponent){}
	virtual void RemoveExponentialHeightFog(class UExponentialHeightFogComponent* FogComponent){}
	virtual void AddAtmosphericFog(class UAtmosphericFogComponent* FogComponent) {}
	virtual void RemoveAtmosphericFog(class UAtmosphericFogComponent* FogComponent) {}
	virtual FAtmosphericFogSceneInfo* GetAtmosphericFogSceneInfo() override { return NULL; }
	virtual void AddWindSource(class UWindDirectionalSourceComponent* WindComponent) {}
	virtual void RemoveWindSource(class UWindDirectionalSourceComponent* WindComponent) {}
	virtual const TArray<class FWindSourceSceneProxy*>& GetWindSources_RenderThread() const
	{
		static TArray<class FWindSourceSceneProxy*> NullWindSources;
		return NullWindSources;
	}
	virtual FVector4 GetWindParameters(const FVector& Position) const { return FVector4(0,0,1,0); }
	virtual FVector4 GetDirectionalWindParameters() const { return FVector4(0,0,1,0); }
	virtual void AddSpeedTreeWind(class FVertexFactory* VertexFactory, const class UStaticMesh* StaticMesh) {}
	virtual void RemoveSpeedTreeWind(class FVertexFactory* VertexFactory, const class UStaticMesh* StaticMesh) {}
	virtual void RemoveSpeedTreeWind_RenderThread(class FVertexFactory* VertexFactory, const class UStaticMesh* StaticMesh) {}
	virtual void UpdateSpeedTreeWind(double CurrentTime) {}
	virtual FUniformBufferRHIParamRef GetSpeedTreeUniformBuffer(const FVertexFactory* VertexFactory) { return FUniformBufferRHIParamRef(); }

	virtual void Release(){}

	/**
	 * Retrieves the lights interacting with the passed in primitive and adds them to the out array.
	 *
	 * @param	Primitive				Primitive to retrieve interacting lights for
	 * @param	RelevantLights	[out]	Array of lights interacting with primitive
	 */
	virtual void GetRelevantLights( UPrimitiveComponent* Primitive, TArray<const ULightComponent*>* RelevantLights ) const {}

	/**
	 * @return		true if hit proxies should be rendered in this scene.
	 */
	virtual bool RequiresHitProxies() const 
	{
		return false;
	}

	// Accessors.
	virtual class UWorld* GetWorld() const
	{
		return World;
	}

	/**
	* Return the scene to be used for rendering
	*/
	virtual class FScene* GetRenderScene()
	{
		return NULL;
	}

	/**
	 * Sets the FX system associated with the scene.
	 */
	virtual void SetFXSystem( class FFXSystemInterface* InFXSystem )
	{
		FXSystem = InFXSystem;
	}

	/**
	 * Get the FX system associated with the scene.
	 */
	virtual class FFXSystemInterface* GetFXSystem()
	{
		return FXSystem;
	}

	virtual bool HasAnyLights() const { return false; }
private:
	UWorld* World;
	class FFXSystemInterface* FXSystem;
};

FSceneInterface* FRendererModule::AllocateScene(UWorld* World, bool bInRequiresHitProxies, bool bCreateFXSystem, ERHIFeatureLevel::Type InFeatureLevel)
{
	check(IsInGameThread());

	// Create a full fledged scene if we have something to render.
	if( GIsClient && !IsRunningCommandlet() && !GUsingNullRHI )
	{
		FScene* NewScene = new FScene(World, bInRequiresHitProxies, GIsEditor && !World->IsGameWorld(), bCreateFXSystem, InFeatureLevel);
		AllocatedScenes.Add(NewScene);
		return NewScene;
	}
	// And fall back to a dummy/ NULL implementation for commandlets and dedicated server.
	else
	{
		return new FNULLSceneInterface(World, bCreateFXSystem);
	}
}

void FRendererModule::RemoveScene(FSceneInterface* Scene)
{
	check(IsInGameThread());
	AllocatedScenes.Remove(Scene);
}

void FRendererModule::UpdateStaticDrawListsForMaterials(const TArray<const FMaterial*>& Materials)
{
	for (TSet<FSceneInterface*>::TConstIterator SceneIt(AllocatedScenes); SceneIt; ++SceneIt)
	{
		(*SceneIt)->UpdateStaticDrawListsForMaterials(Materials);
	}
}

FSceneViewStateInterface* FRendererModule::AllocateViewState()
{
	return new FSceneViewState();
}

/** Maps the no light-map case to the appropriate base pass draw list. */
template<>
TStaticMeshDrawList<TBasePassDrawingPolicy<FNoLightMapPolicy> >& FScene::GetBasePassDrawList<FNoLightMapPolicy>(EBasePassDrawListType DrawType)
{
	return BasePassNoLightMapDrawList[DrawType];
}

/** Maps the directional light-map texture case to the appropriate base pass draw list. */
template<>
TStaticMeshDrawList<TBasePassDrawingPolicy< TLightMapPolicy<HQ_LIGHTMAP> > >& FScene::GetBasePassDrawList< TLightMapPolicy<HQ_LIGHTMAP> >(EBasePassDrawListType DrawType)
{
	return BasePassHighQualityLightMapDrawList[DrawType];
}

/**  */
template<>
TStaticMeshDrawList<TBasePassDrawingPolicy< TDistanceFieldShadowsAndLightMapPolicy<HQ_LIGHTMAP> > >& FScene::GetBasePassDrawList< TDistanceFieldShadowsAndLightMapPolicy<HQ_LIGHTMAP> >(EBasePassDrawListType DrawType)
{
	return BasePassDistanceFieldShadowMapLightMapDrawList[DrawType];
}

/** Maps the simple light-map texture case to the appropriate base pass draw list. */
template<>
TStaticMeshDrawList<TBasePassDrawingPolicy< TLightMapPolicy<LQ_LIGHTMAP> > >& FScene::GetBasePassDrawList< TLightMapPolicy<LQ_LIGHTMAP> >(EBasePassDrawListType DrawType)
{
	return BasePassLowQualityLightMapDrawList[DrawType];
}

/**  */
template<>
TStaticMeshDrawList<TBasePassDrawingPolicy<FSelfShadowedTranslucencyPolicy> >& FScene::GetBasePassDrawList<FSelfShadowedTranslucencyPolicy>(EBasePassDrawListType DrawType)
{
	return BasePassSelfShadowedTranslucencyDrawList[DrawType];
}

/**  */
template<>
TStaticMeshDrawList<TBasePassDrawingPolicy<FSelfShadowedCachedPointIndirectLightingPolicy> >& FScene::GetBasePassDrawList<FSelfShadowedCachedPointIndirectLightingPolicy>(EBasePassDrawListType DrawType)
{
	return BasePassSelfShadowedCachedPointIndirectTranslucencyDrawList[DrawType];
}

/**  */
template<>
TStaticMeshDrawList<TBasePassDrawingPolicy<FCachedVolumeIndirectLightingPolicy> >& FScene::GetBasePassDrawList<FCachedVolumeIndirectLightingPolicy>(EBasePassDrawListType DrawType)
{
	return BasePassCachedVolumeIndirectLightingDrawList[DrawType];
}

/**  */
template<>
TStaticMeshDrawList<TBasePassDrawingPolicy<FCachedPointIndirectLightingPolicy> >& FScene::GetBasePassDrawList<FCachedPointIndirectLightingPolicy>(EBasePassDrawListType DrawType)
{
	return BasePassCachedPointIndirectLightingDrawList[DrawType];
}

/**  */
template<>
TStaticMeshDrawList<TBasePassDrawingPolicy<FSimpleDynamicLightingPolicy> >& FScene::GetBasePassDrawList<FSimpleDynamicLightingPolicy>(EBasePassDrawListType DrawType)
{
	return BasePassSimpleDynamicLightingDrawList[DrawType];
}


/** Maps the no light-map case to the appropriate base pass draw list. */
template<>
TStaticMeshDrawList<TBasePassForForwardShadingDrawingPolicy<FNoLightMapPolicy> >& FScene::GetForwardShadingBasePassDrawList<FNoLightMapPolicy>(EBasePassDrawListType DrawType)
{
	return BasePassForForwardShadingNoLightMapDrawList[DrawType];
}

/** Maps the simple light-map texture case to the appropriate base pass draw list. */
template<>
TStaticMeshDrawList< TBasePassForForwardShadingDrawingPolicy< TLightMapPolicy<LQ_LIGHTMAP> > >& FScene::GetForwardShadingBasePassDrawList< TLightMapPolicy<LQ_LIGHTMAP> >(EBasePassDrawListType DrawType)
{
	return BasePassForForwardShadingLowQualityLightMapDrawList[DrawType];
}

template<>
TStaticMeshDrawList< TBasePassForForwardShadingDrawingPolicy< TDistanceFieldShadowsAndLightMapPolicy<LQ_LIGHTMAP> > >& FScene::GetForwardShadingBasePassDrawList< TDistanceFieldShadowsAndLightMapPolicy<LQ_LIGHTMAP> >(EBasePassDrawListType DrawType)
{
	return BasePassForForwardShadingDistanceFieldShadowMapLightMapDrawList[DrawType];
}

template<>
TStaticMeshDrawList<TBasePassForForwardShadingDrawingPolicy<FSimpleDirectionalLightAndSHIndirectPolicy> >& FScene::GetForwardShadingBasePassDrawList<FSimpleDirectionalLightAndSHIndirectPolicy>(EBasePassDrawListType DrawType)
{
	return BasePassForForwardShadingDirectionalLightAndSHIndirectDrawList[DrawType];
}

template<>
TStaticMeshDrawList<TBasePassForForwardShadingDrawingPolicy<FSimpleDirectionalLightAndSHDirectionalIndirectPolicy> >& FScene::GetForwardShadingBasePassDrawList<FSimpleDirectionalLightAndSHDirectionalIndirectPolicy>(EBasePassDrawListType DrawType)
{
	return BasePassForForwardShadingDirectionalLightAndSHDirectionalIndirectDrawList[DrawType];
}

template<>
TStaticMeshDrawList<TBasePassForForwardShadingDrawingPolicy<FSimpleDirectionalLightAndSHDirectionalCSMIndirectPolicy> >& FScene::GetForwardShadingBasePassDrawList<FSimpleDirectionalLightAndSHDirectionalCSMIndirectPolicy>(EBasePassDrawListType DrawType)
{
	return BasePassForForwardShadingDirectionalLightAndSHDirectionalCSMIndirectDrawList[DrawType];
}

template<>
TStaticMeshDrawList<TBasePassForForwardShadingDrawingPolicy<FMovableDirectionalLightLightingPolicy> >& FScene::GetForwardShadingBasePassDrawList<FMovableDirectionalLightLightingPolicy>(EBasePassDrawListType DrawType)
{
	return BasePassForForwardShadingMovableDirectionalLightDrawList[DrawType];
}

template<>
TStaticMeshDrawList<TBasePassForForwardShadingDrawingPolicy<FMovableDirectionalLightCSMLightingPolicy> >& FScene::GetForwardShadingBasePassDrawList<FMovableDirectionalLightCSMLightingPolicy>(EBasePassDrawListType DrawType)
{
	return BasePassForForwardShadingMovableDirectionalLightCSMDrawList[DrawType];
}

template<>
TStaticMeshDrawList<TBasePassForForwardShadingDrawingPolicy<FMovableDirectionalLightWithLightmapLightingPolicy> >& FScene::GetForwardShadingBasePassDrawList<FMovableDirectionalLightWithLightmapLightingPolicy>(EBasePassDrawListType DrawType)
{
	return BasePassForForwardShadingMovableDirectionalLightLightmapDrawList[DrawType];
}

template<>
TStaticMeshDrawList<TBasePassForForwardShadingDrawingPolicy<FMovableDirectionalLightCSMWithLightmapLightingPolicy> >& FScene::GetForwardShadingBasePassDrawList<FMovableDirectionalLightCSMWithLightmapLightingPolicy>(EBasePassDrawListType DrawType)
{
	return BasePassForForwardShadingMovableDirectionalLightCSMLightmapDrawList[DrawType];
}

/*-----------------------------------------------------------------------------
	MotionBlurInfoData
-----------------------------------------------------------------------------*/

FMotionBlurInfoData::FMotionBlurInfoData()
	: bShouldClearMotionBlurInfo(false)
{

}

void FMotionBlurInfoData::UpdatePrimitiveMotionBlur(FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	check(PrimitiveSceneInfo && IsInRenderingThread());

	const FPrimitiveSceneProxy* Proxy = PrimitiveSceneInfo->Proxy; 
	FPrimitiveComponentId ComponentId = PrimitiveSceneInfo->PrimitiveComponentId;

	if (Proxy != NULL && ComponentId.IsValid() && Proxy->IsMovable())
	{
		FMotionBlurInfo* MotionBlurInfo = FindMBInfoIndex(ComponentId);

		if(MotionBlurInfo)
		{
			if(!MotionBlurInfo->GetPrimitiveSceneInfo())
			{
				MotionBlurInfo->SetPrimitiveSceneInfo(PrimitiveSceneInfo);
			}
		}
		else
		{
			// add to the end
			MotionBlurInfo = &MotionBlurInfos.Add(ComponentId, FMotionBlurInfo(ComponentId, PrimitiveSceneInfo));
		}

		//request that this primitive scene info caches its transform at the end of the frame
		MotionBlurInfo->SetKeepAndUpdateThisFrame();
	}
}

void FMotionBlurInfoData::RemovePrimitiveMotionBlur(FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	check(PrimitiveSceneInfo && IsInRenderingThread());

	const FPrimitiveSceneProxy* Proxy = PrimitiveSceneInfo->Proxy; 

	if (Proxy != NULL && PrimitiveSceneInfo->PrimitiveComponentId.IsValid() && Proxy->IsMovable())
	{
		FMotionBlurInfo* MotionBlurInfo = FindMBInfoIndex(PrimitiveSceneInfo->PrimitiveComponentId);

		if(MotionBlurInfo)
		{ 
			// in case someone called SetKeepAndUpdateThisFrame() before
			MotionBlurInfo->SetKeepAndUpdateThisFrame(false);
			MotionBlurInfo->SetPrimitiveSceneInfo(0);
		}
	}
}

void FMotionBlurInfo::UpdateMotionBlurInfo()
{
	if(MBPrimitiveSceneInfo && MBPrimitiveSceneInfo->Proxy)
	{
		PausedLocalToWorld = PreviousLocalToWorld;
		// only if the proxy is still there
		PreviousLocalToWorld = MBPrimitiveSceneInfo->Proxy->GetLocalToWorld();
	}

	bKeepAndUpdateThisFrame = false;
}

void FMotionBlurInfo::RestoreForPausedMotionBlur()
{
	PreviousLocalToWorld = PausedLocalToWorld;
}

// Doxygen has trouble parsing these functions because the header declaring them is in Engine, not Renderer
#if !UE_BUILD_DOCS

void FMotionBlurInfoData::RestoreForPausedMotionBlur()
{
	check(IsInRenderingThread());

	for (TMap<FPrimitiveComponentId, FMotionBlurInfo>::TIterator It(MotionBlurInfos); It; ++It)
	{
		FMotionBlurInfo& MotionBlurInfo = It.Value();

		MotionBlurInfo.RestoreForPausedMotionBlur();
	}
}

void FMotionBlurInfoData::UpdateMotionBlurCache(FScene* InScene)
{
	check(InScene && IsInRenderingThread());

	if (InScene->GetFeatureLevel() >= ERHIFeatureLevel::SM4)
	{
		if(bShouldClearMotionBlurInfo)
		{
			// Clear the motion blur information for this frame.		
			MotionBlurInfos.Empty();
			bShouldClearMotionBlurInfo = false;
		}
		else
		{
			for (TMap<FPrimitiveComponentId, FMotionBlurInfo>::TIterator It(MotionBlurInfos); It; ++It)
			{
				FMotionBlurInfo& MotionBlurInfo = It.Value();

				if (MotionBlurInfo.GetKeepAndUpdateThisFrame())
				{
					MotionBlurInfo.UpdateMotionBlurInfo();
				}
				else
				{
					It.RemoveCurrent();
				}
			}
		}
	}
}

void FMotionBlurInfoData::SetClearMotionBlurInfo()
{
	bShouldClearMotionBlurInfo = true;
}

void FMotionBlurInfoData::ApplyOffset(FVector InOffset)
{
	for (auto It = MotionBlurInfos.CreateIterator(); It; ++It)
	{
		It.Value().ApplyOffset(InOffset);
	}
}

FMotionBlurInfo* FMotionBlurInfoData::FindMBInfoIndex(FPrimitiveComponentId ComponentId)
{
	return MotionBlurInfos.Find(ComponentId);
}

bool FMotionBlurInfoData::GetPrimitiveMotionBlurInfo(const FPrimitiveSceneInfo* PrimitiveSceneInfo, FMatrix& OutPreviousLocalToWorld)
{
	check(IsInParallelRenderingThread());

	if (PrimitiveSceneInfo && PrimitiveSceneInfo->PrimitiveComponentId.IsValid())
	{
		FMotionBlurInfo* MotionBlurInfo = FindMBInfoIndex(PrimitiveSceneInfo->PrimitiveComponentId);

		if(MotionBlurInfo)
		{
			OutPreviousLocalToWorld = MotionBlurInfo->GetPreviousLocalToWorld();
			return true;
		}
	}
	return false;
}

#endif
