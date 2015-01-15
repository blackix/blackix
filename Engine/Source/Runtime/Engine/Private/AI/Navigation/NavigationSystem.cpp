// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "MessageLog.h"
#include "NavDataGenerator.h"
#include "NavigationOctree.h"
#include "AI/Navigation/NavMeshBoundsVolume.h"
#include "AI/Navigation/NavRelevantComponent.h"

#if WITH_RECAST
#include "RecastNavMeshGenerator.h"
#endif // WITH_RECAST
#if WITH_EDITOR
#include "UnrealEd.h"
#include "Editor/GeometryMode/Public/GeometryEdMode.h"
#include "Editor/GeometryMode/Public/EditorGeometry.h"
#endif

// @todo this is here only due to circular dependency to AIModule. To be removed
#include "Navigation/CrowdManager.h"
#include "Navigation/PathFollowingComponent.h"
#include "AI/Navigation/NavAreas/NavArea_Null.h"
#include "AI/Navigation/NavAreas/NavArea_Default.h"
#include "AI/Navigation/NavLinkCustomInterface.h"
#include "AI/Navigation/NavigationSystem.h"
#include "AI/Navigation/NavRelevantComponent.h"
#include "AI/Navigation/NavigationPath.h"
#include "AI/Navigation/AbstractNavData.h"

static const uint32 INITIAL_ASYNC_QUERIES_SIZE = 32;
static const uint32 REGISTRATION_QUEUE_SIZE = 16;	// and we'll not reallocate
#if WITH_RECAST
static const uint32 MAX_NAV_SEARCH_NODES = RECAST_MAX_SEARCH_NODES;
#else // WITH_RECAST
static const uint32 MAX_NAV_SEARCH_NODES = 2048;
#endif // WITH_RECAST

#define LOCTEXT_NAMESPACE "Navigation"

DEFINE_LOG_CATEGORY(LogNavigation);
DEFINE_LOG_CATEGORY_STATIC(LogNavOctree, Warning, All);

DECLARE_CYCLE_STAT(TEXT("Rasterize triangles"), STAT_Navigation_RasterizeTriangles,STATGROUP_Navigation);
DECLARE_CYCLE_STAT(TEXT("Nav Tick: area register"), STAT_Navigation_TickNavAreaRegister, STATGROUP_Navigation);
DECLARE_CYCLE_STAT(TEXT("Nav Tick: mark dirty"), STAT_Navigation_TickMarkDirty, STATGROUP_Navigation);
DECLARE_CYCLE_STAT(TEXT("Nav Tick: async build"), STAT_Navigation_TickAsyncBuild, STATGROUP_Navigation);
DECLARE_CYCLE_STAT(TEXT("Nav Tick: async pathfinding"), STAT_Navigation_TickAsyncPathfinding, STATGROUP_Navigation);
DECLARE_CYCLE_STAT(TEXT("Debug NavOctree Time"), STAT_DebugNavOctree, STATGROUP_Navigation);

//----------------------------------------------------------------------//
// Stats
//----------------------------------------------------------------------//

DEFINE_STAT(STAT_Navigation_QueriesTimeSync);
DEFINE_STAT(STAT_Navigation_RequestingAsyncPathfinding);
DEFINE_STAT(STAT_Navigation_PathfindingSync);
DEFINE_STAT(STAT_Navigation_PathfindingAsync);
DEFINE_STAT(STAT_Navigation_AddGeneratedTiles);
DEFINE_STAT(STAT_Navigation_TileNavAreaSorting);
DEFINE_STAT(STAT_Navigation_TileGeometryExportToObjAsync);
DEFINE_STAT(STAT_Navigation_TileVoxelFilteringAsync);
DEFINE_STAT(STAT_Navigation_TileBuildAsync);
DEFINE_STAT(STAT_Navigation_MetaAreaTranslation);
DEFINE_STAT(STAT_Navigation_TileBuildPreparationSync);
DEFINE_STAT(STAT_Navigation_BSPExportSync);
DEFINE_STAT(STAT_Navigation_GatheringNavigationModifiersSync);
DEFINE_STAT(STAT_Navigation_ActorsGeometryExportSync);
DEFINE_STAT(STAT_Navigation_ProcessingActorsForNavMeshBuilding);
DEFINE_STAT(STAT_Navigation_AdjustingNavLinks);
DEFINE_STAT(STAT_Navigation_AddingActorsToNavOctree);
DEFINE_STAT(STAT_Navigation_RecastTick);
DEFINE_STAT(STAT_Navigation_RecastBuildCompressedLayers);
DEFINE_STAT(STAT_Navigation_RecastBuildNavigation);
DEFINE_STAT(STAT_Navigation_DestructiblesShapesExported);
DEFINE_STAT(STAT_Navigation_UpdateNavOctree);
DEFINE_STAT(STAT_Navigation_CollisionTreeMemory);
DEFINE_STAT(STAT_Navigation_NavDataMemory);
DEFINE_STAT(STAT_Navigation_TileCacheMemory);
DEFINE_STAT(STAT_Navigation_OutOfNodesPath);
DEFINE_STAT(STAT_Navigation_PartialPath);
DEFINE_STAT(STAT_Navigation_CumulativeBuildTime);
DEFINE_STAT(STAT_Navigation_BuildTime);
DEFINE_STAT(STAT_Navigation_OffsetFromCorners);
DEFINE_STAT(STAT_Navigation_PathVisibilityOptimisation);

//----------------------------------------------------------------------//
// consts
//----------------------------------------------------------------------//

const uint32 FNavigationQueryFilter::DefaultMaxSearchNodes = MAX_NAV_SEARCH_NODES;

namespace FNavigationSystem
{
	// these are totally arbitrary values, and it should haven happen these are ever used.
	// in any reasonable case UNavigationSystem::SupportedAgents should be filled in ini file
	// and only those values will be used
	const float FallbackAgentRadius = 35.f;
	const float FallbackAgentHeight = 144.f;
		
	FORCEINLINE bool IsValidExtent(const FVector& Extent)
	{
		return Extent != INVALID_NAVEXTENT;
	}
}

namespace NavigationDebugDrawing
{
	const float PathLineThickness = 3.f;
	const FVector PathOffset(0,0,15);
	const FVector PathNodeBoxExtent(16.f);
}

//----------------------------------------------------------------------//
// FNavDataConfig
//----------------------------------------------------------------------//
FNavDataConfig::FNavDataConfig(float Radius, float Height)
	: FNavAgentProperties(Radius, Height)
	, Name(TEXT("Default"))
	, Color(140, 255, 0, 164)
	, DefaultQueryExtent(DEFAULT_NAV_QUERY_EXTENT_HORIZONTAL, DEFAULT_NAV_QUERY_EXTENT_HORIZONTAL, DEFAULT_NAV_QUERY_EXTENT_VERTICAL)
	, NavigationDataClass(ARecastNavMesh::StaticClass())
{
}
//----------------------------------------------------------------------//
// FNavigationLockContext                                                                
//----------------------------------------------------------------------//
void FNavigationLockContext::LockUpdates()
{
#if WITH_EDITOR
	bIsLocked = true;

	if (bSingleWorld)
	{
		UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(MyWorld);
		if (NavSys)
		{
			NavSys->AddNavigationUpdateLock(LockReason);
		}
	}
	else
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(Context.World());
			if (NavSys)
			{
				NavSys->AddNavigationUpdateLock(LockReason);
			}
		}
	}
#endif
}

void FNavigationLockContext::UnlockUpdates()
{
#if WITH_EDITOR
	if (!bIsLocked)
	{
		return;
	}

	if (bSingleWorld)
	{
		UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(MyWorld);
		if (NavSys)
		{
			NavSys->RemoveNavigationUpdateLock(LockReason);
		}
	}
	else
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(Context.World());
			if (NavSys)
			{
				NavSys->RemoveNavigationUpdateLock(LockReason);
			}
		}
	}
#endif
}

//----------------------------------------------------------------------//
// UNavigationSystem                                                                
//----------------------------------------------------------------------//
bool UNavigationSystem::bNavigationAutoUpdateEnabled = true;
TArray<TSubclassOf<ANavigationData> > UNavigationSystem::NavDataClasses;
TArray<const UClass*> UNavigationSystem::NavAreaClasses;
TArray<UClass*> UNavigationSystem::PendingNavAreaRegistration;
TSubclassOf<UNavArea> UNavigationSystem::DefaultWalkableArea = NULL;
TSubclassOf<UNavArea> UNavigationSystem::DefaultObstacleArea = NULL;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
FNavigationSystemExec UNavigationSystem::ExecHandler;
#endif // !UE_BUILD_SHIPPING

/** called after navigation influencing event takes place*/
UNavigationSystem::FOnNavigationDirty UNavigationSystem::NavigationDirtyEvent;

bool UNavigationSystem::bUpdateNavOctreeOnComponentChange = true;
//----------------------------------------------------------------------//
// life cycle stuff                                                                
//----------------------------------------------------------------------//

UNavigationSystem::UNavigationSystem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bWholeWorldNavigable(false)
	, bAddPlayersToGenerationSeeds(true)
	, bSkipAgentHeightCheckWhenPickingNavData(false)
	, DirtyAreasUpdateFreq(60)
	, OperationMode(FNavigationSystem::InvalidMode)
	, NavOctree(NULL)
	, bNavigationBuildingLocked(false)
	, bInitialBuildingLockActive(false)
	, bInitialSetupHasBeenPerformed(false)
	, bInitialLevelsAdded(false)
	, CurrentlyDrawnNavDataIndex(0)
	, DirtyAreasUpdateTime(0)
{
#if WITH_EDITOR
	NavUpdateLockFlags = 0;
#endif

	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		// reserve some arbitrary size
		AsyncPathFindingQueries.Reserve( INITIAL_ASYNC_QUERIES_SIZE );
		NavDataRegistrationQueue.Reserve( REGISTRATION_QUEUE_SIZE );
	
		FWorldDelegates::LevelAddedToWorld.AddUObject(this, &UNavigationSystem::OnLevelAddedToWorld);
		FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &UNavigationSystem::OnLevelRemovedFromWorld);
	}
	else
	{
		DefaultWalkableArea = UNavArea_Default::StaticClass();
		DefaultObstacleArea = UNavArea_Null::StaticClass();
	}

#if WITH_EDITOR
	if (GIsEditor && HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		GLevelEditorModeTools().OnEditorModeChanged().AddUObject(this, &UNavigationSystem::OnEditorModeChanged);
	}
#endif // WITH_EDITOR
}

UNavigationSystem::~UNavigationSystem()
{
	CleanUp();
#if WITH_EDITOR
	if (GIsEditor)
	{
		GLevelEditorModeTools().OnEditorModeChanged().RemoveAll(this);
	}
#endif // WITH_EDITOR
}

void UNavigationSystem::DoInitialSetup()
{
	if (bInitialSetupHasBeenPerformed)
	{
		return;
	}
	
	UpdateAbstractNavData();

	CreateCrowdManager();

	bInitialSetupHasBeenPerformed = true;
}

void UNavigationSystem::UpdateAbstractNavData()
{
	if (AbstractNavData != nullptr)
	{
		return;
	}

	// spawn abstract nav data separately
	// it's responsible for direct paths and shouldn't be picked for any agent type as default one
	UWorld* NavWorld = GetWorld();
	for (TActorIterator<AAbstractNavData> It(NavWorld); It; ++It)
	{
		AAbstractNavData* Nav = (*It);
		if (Nav && !Nav->IsPendingKill())
		{
			AbstractNavData = Nav;
			break;
		}
	}

	if (AbstractNavData == NULL)
	{
		FNavDataConfig DummyConfig;
		DummyConfig.NavigationDataClass = AAbstractNavData::StaticClass();
		AbstractNavData = CreateNavigationDataInstance(DummyConfig);
	}

	if (AbstractNavData->IsRegistered() == false)
	{
		// fake registration since it's a special navigation data type 
		// and it would get discarded for not implementing any particular
		// navigation agent
		AbstractNavData->OnRegistered();
	}
}
void UNavigationSystem::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		// make sure there's at leas one supported navigation agent size
		if (SupportedAgents.Num() == 0)
		{
			SupportedAgents.Add(FNavDataConfig(FNavigationSystem::FallbackAgentRadius, FNavigationSystem::FallbackAgentHeight));
		}

		// gather navigation creators
		NavDataClasses.Empty(RequiredNavigationDataClassNames.Num());
		for (int32 Index = 0; Index < RequiredNavigationDataClassNames.Num(); ++Index)
		{
			TSubclassOf<ANavigationData> NavDataClass = LoadClass<ANavigationData>(NULL, *RequiredNavigationDataClassNames[Index].ToString(), NULL, LOAD_None, NULL);
			if (NavDataClass)
			{
				NavDataClasses.AddUnique(NavDataClass);
			}
			else
			{
				UE_LOG(LogNavigation, Warning, TEXT("Unable to find navigation data class \'%s\' while setting up require navigation types")
					, *RequiredNavigationDataClassNames[Index].ToString());
			}
		}

		if (NavDataClasses.Num() == 0)
		{
			// @note: if you don't want navigation system to be created at all you can disable it by 
			// setting AWorldSettings.bEnableNavigationSystem to false
			UE_LOG(LogNavigation, Error, TEXT("No navigation data types found while setting up required navigation types!"));
		}

		ConditionallyCreateNavOctree();
	
		bInitialBuildingLockActive = bInitialBuildingLocked;
		InitializeLevelCollisions();

		// register for any actor move change
#if WITH_EDITOR
		if ( GIsEditor )
		{
			GEngine->OnActorMoved().AddUObject(this, &UNavigationSystem::OnActorMoved);
		}
#endif
		FCoreUObjectDelegates::PostLoadMap.AddUObject(this, &UNavigationSystem::OnPostLoadMap);
		UNavigationSystem::NavigationDirtyEvent.AddUObject(this, &UNavigationSystem::OnNavigationDirtied);
	}
	
	// update SupportedActors' navigation classes
	for (FNavDataConfig& SupportedAgentConfig : SupportedAgents)
	{
		if (SupportedAgentConfig.NavigationDataClassName.IsValid())
		{
			SupportedAgentConfig.NavigationDataClass = LoadClass<ANavigationData>(NULL, *SupportedAgentConfig.NavigationDataClassName.ToString(), NULL, LOAD_None, NULL);
		}
	}
}

bool UNavigationSystem::ConditionallyCreateNavOctree()
{
	ensure(NavOctree == nullptr);
	if (NavOctree != nullptr)
	{
		return true;
	}

	bSupportRebuilding = GetWorld()->IsGameWorld() == false;
	for (int32 NavDataClassIndex = 0; NavDataClassIndex < NavDataClasses.Num() && bSupportRebuilding == false; ++NavDataClassIndex)
	{
		const ANavigationData* NavDataCDO = GetDefault<ANavigationData>(NavDataClasses[NavDataClassIndex]);
		check(NavDataCDO);
		bSupportRebuilding = NavDataCDO->bRebuildAtRuntime;
	}

	if (bSupportRebuilding)
	{
		NavOctree = new FNavigationOctree(FVector(0, 0, 0), 64000);
#if WITH_RECAST
		NavOctree->ComponentExportDelegate = FNavigationOctree::FNavigableGeometryComponentExportDelegate::CreateStatic(&FRecastNavMeshGenerator::ExportComponentGeometry);
#endif // WITH_RECAST
		NavOctree->SetNavigableGeometryStoringMode(FNavigationOctree::StoreNavGeometry);
	}

	return NavOctree != nullptr;
}


#if WITH_EDITOR
void UNavigationSystem::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName NAME_SupportedAgents = GET_MEMBER_NAME_CHECKED(UNavigationSystem, SupportedAgents);
	static const FName NAME_NavigationDataClass = GET_MEMBER_NAME_CHECKED(FNavDataConfig, NavigationDataClass);

	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		FName PropName = PropertyChangedEvent.Property->GetFName();
		if (PropName == NAME_NavigationDataClass)
		{
			int32 SupportedAgentIndex = PropertyChangedEvent.GetArrayIndex(NAME_SupportedAgents.ToString());
			if (SupportedAgentIndex != INDEX_NONE)
			{
				// reflect the change to SupportedAgent's 
				if (SupportedAgents[SupportedAgentIndex].NavigationDataClass != nullptr)
				{
					SupportedAgents[SupportedAgentIndex].NavigationDataClassName = FStringClassReference::GetOrCreateIDForClass(SupportedAgents[SupportedAgentIndex].NavigationDataClass);
				}
				else
				{
					SupportedAgents[SupportedAgentIndex].NavigationDataClassName.Reset();
				}
			}
		}
	}
}
#endif // WITH_EDITOR

void UNavigationSystem::OnInitializeActors()
{
	
}

void UNavigationSystem::OnWorldInitDone(FNavigationSystem::EMode Mode)
{
	OperationMode = Mode;
	
	if (bSupportRebuilding)
	{
		UWorld* World = GetWorld();

		if (IsThereAnywhereToBuildNavigation() == false
			// Simulation mode is a special case - better not do it in this case
			&& OperationMode != FNavigationSystem::SimulationMode)
		{
			// remove all navigation data instances
			for (TActorIterator<ANavigationData> It(World); It; ++It)
			{
				ANavigationData* Nav = (*It);
				if (Nav != NULL && Nav->IsPendingKill() == false)
				{
					UnregisterNavData(Nav);
					Nav->CleanUpAndMarkPendingKill();
				}
			}

			if (OperationMode == FNavigationSystem::EditorMode)
			{
				bInitialBuildingLockActive = false;
			}

			bNavDataRemovedDueToMissingNavBounds = true;
		}
		else
		{
			InitializeLevelCollisions();
			PopulateNavOctree();

			// gather navigable bounds
			GatherNavigationBounds();

			// gather all navigation data instances and register all not-yet-registered
			// (since it's quite possible navigation system was not ready by the time
			// those instances were serialized-in or spawned)
			RegisterNavigationDataInstances();

			if (OperationMode == FNavigationSystem::EditorMode)
			{
				// don't lock navigation building in editor
				bInitialBuildingLockActive = false;
			}

			if (bAutoCreateNavigationData == true)
			{
				SpawnMissingNavigationData();
				// in case anything spawned has registered
				ProcessRegistrationCandidates();
			}
			else
			{
				if (GetMainNavData(FNavigationSystem::DontCreate) != NULL)
				{
					// trigger navmesh update
					for (TActorIterator<ANavigationData> It(World); It; ++It)
					{
						ANavigationData* NavData = (*It);
						if (NavData != NULL)
						{
							ERegistrationResult Result = RegisterNavData(NavData);

							if (Result == RegistrationSuccessful)
							{
#if WITH_RECAST
								if (Cast<ARecastNavMesh>(NavData) != NULL)
								{
									if (bInitialBuildingLockActive == false && bNavigationAutoUpdateEnabled)
									{
										NavData->RebuildAll();
									}
								}
#endif // WITH_RECAST
							}
							else if (Result != RegistrationFailed_DataPendingKill
								&& Result != RegistrationFailed_AgentNotValid
								)
							{
								NavData->CleanUpAndMarkPendingKill();
							}
						}
					}
				}
			}

			// All navigation actors are registered
			// Add NavMesh parts from all sub-levels that were streamed in prior NavMesh registration
			if (World->IsGameWorld())
			{
				const auto& Levels = World->GetLevels();
				for (ULevel* Level : Levels)
				{
					if (!Level->IsPersistentLevel() && Level->bIsVisible)
					{
						for (ANavigationData* NavData : NavDataSet)
						{
							NavData->OnStreamingLevelAdded(Level);
						}
					}
				}
			}
		}
	}
	else // just register data already present
	{
		RegisterNavigationDataInstances();
		UpdateAbstractNavData();
	}
}

void UNavigationSystem::RegisterNavigationDataInstances()
{
	UWorld* World = GetWorld();

	bool bProcessRegistration = false;
	for (TActorIterator<ANavigationData> It(World); It; ++It)
	{
		ANavigationData* Nav = (*It);
		if (Nav != NULL && Nav->IsPendingKill() == false && Nav->IsRegistered() == false)
		{
			RequestRegistration(Nav, false);
			bProcessRegistration = true;
		}
	}
	if (bProcessRegistration == true)
	{
		ProcessRegistrationCandidates();
	}
}

void UNavigationSystem::CreateCrowdManager()
{
	SetCrowdManager(NewObject<UCrowdManager>(this));
}

void UNavigationSystem::SetCrowdManager(UCrowdManager* NewCrowdManager)
{
	if (NewCrowdManager == CrowdManager.Get())
	{
		return;
	}

	if (CrowdManager.IsValid())
	{
		CrowdManager->RemoveFromRoot();
	}
	CrowdManager = NewCrowdManager;
	if (NewCrowdManager != NULL)
	{
		CrowdManager->AddToRoot();
	}
}

void UNavigationSystem::Tick(float DeltaSeconds)
{
	const bool bIsGame = (GetWorld() && GetWorld()->IsGameWorld());
	
	// Register any pending nav areas
	if (PendingNavAreaRegistration.Num() > 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_Navigation_TickNavAreaRegister);
		ProcessNavAreaPendingRegistration();
	}

	if (PendingNavBoundsUpdates.Num() > 0)
	{
		PerformNavigationBoundsUpdate(PendingNavBoundsUpdates);
		PendingNavBoundsUpdates.Reset();
	}

	if (PendingOctreeUpdates.Num() > 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_Navigation_AddingActorsToNavOctree);

		SCOPE_CYCLE_COUNTER(STAT_Navigation_BuildTime)
		STAT(double ThisTime = 0);
		{
			SCOPE_SECONDS_COUNTER(ThisTime);
			for (TSet<FNavigationDirtyElement>::TIterator It(PendingOctreeUpdates); It; ++It)
			{
				AddElementToNavOctree(*It);
			}
			PendingOctreeUpdates.Empty(32);
		}
		INC_FLOAT_STAT_BY(STAT_Navigation_CumulativeBuildTime,(float)ThisTime*1000);
	}
		
	{
		SCOPE_CYCLE_COUNTER(STAT_Navigation_TickMarkDirty);

		DirtyAreasUpdateTime += DeltaSeconds;
		const float DirtyAreasUpdateDeltaTime = 1.0f / DirtyAreasUpdateFreq;
		const bool bCanRebuildNow = (DirtyAreasUpdateTime >= DirtyAreasUpdateDeltaTime) || !bIsGame;

		if (DirtyAreas.Num() > 0 && bCanRebuildNow)
		{
			for (int32 NavDataIndex = 0; NavDataIndex < NavDataSet.Num(); ++NavDataIndex)
			{
				ANavigationData* NavData = NavDataSet[NavDataIndex];
				if (NavData)
				{
					NavData->RebuildDirtyAreas(DirtyAreas);
				}
			}

			DirtyAreasUpdateTime = 0;
			DirtyAreas.Reset();
		}
	}

	// Tick navigation mesh async builders
	if (!bAsyncBuildPaused && (bNavigationAutoUpdateEnabled || bIsGame))
	{
		SCOPE_CYCLE_COUNTER(STAT_Navigation_TickAsyncBuild);
		for (ANavigationData* NavData : NavDataSet)
		{
			if (NavData)
			{
				NavData->TickAsyncBuild(DeltaSeconds);
			}
		}
	}

	if (AsyncPathFindingQueries.Num() > 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_Navigation_TickAsyncPathfinding);
		TriggerAsyncQueries(AsyncPathFindingQueries);
		AsyncPathFindingQueries.Reset();
	}

	if (CrowdManager.IsValid())
	{
		CrowdManager->Tick(DeltaSeconds);
	}
}

void UNavigationSystem::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	// don't reference NavAreaClasses in editor (unless PIE is active)
	// to allow deleting assets
	if (!GIsEditor || GIsPlayInEditorWorld)
	{
		for (int32 NavAreaIndex = 0; NavAreaIndex < NavAreaClasses.Num(); NavAreaIndex++)
		{
			Collector.AddReferencedObject(NavAreaClasses[NavAreaIndex], InThis);
		}
	}

	for (int32 PendingAreaIndex = 0; PendingAreaIndex < PendingNavAreaRegistration.Num(); PendingAreaIndex++)
	{
		Collector.AddReferencedObject(PendingNavAreaRegistration[PendingAreaIndex], InThis);
	}
	
	UNavigationSystem* This = CastChecked<UNavigationSystem>(InThis);
	UCrowdManager* CrowdManager = This->GetCrowdManager();
	Collector.AddReferencedObject(CrowdManager, InThis);
}

#if WITH_EDITOR
void UNavigationSystem::SetNavigationAutoUpdateEnabled(bool bNewEnable,UNavigationSystem* InNavigationsystem) 
{ 
	if(bNewEnable != bNavigationAutoUpdateEnabled)
	{
		bNavigationAutoUpdateEnabled = bNewEnable; 

		if (InNavigationsystem)
		{
			InNavigationsystem->EnableAllGenerators(bNewEnable, /*bForce=*/true);
		}
	}
}
#endif // WITH_EDITOR

//----------------------------------------------------------------------//
// Public querying interface                                                                
//----------------------------------------------------------------------//
FPathFindingResult UNavigationSystem::FindPathSync(const FNavAgentProperties& AgentProperties, FPathFindingQuery Query, EPathFindingMode::Type Mode)
{
	SCOPE_CYCLE_COUNTER(STAT_Navigation_PathfindingSync);

	if (Query.NavData.IsValid() == false)
	{
		Query.NavData = GetNavDataForProps(AgentProperties);
	}

	FPathFindingResult Result(ENavigationQueryResult::Error);
	if (Query.NavData.IsValid())
	{
		if (Mode == EPathFindingMode::Hierarchical)
		{
			Result = Query.NavData->FindHierarchicalPath(AgentProperties, Query);
		}
		else
		{
			Result = Query.NavData->FindPath(AgentProperties, Query);
		}
	}

	return Result;
}

FPathFindingResult UNavigationSystem::FindPathSync(FPathFindingQuery Query, EPathFindingMode::Type Mode)
{
	SCOPE_CYCLE_COUNTER(STAT_Navigation_PathfindingSync);

	if (Query.NavData.IsValid() == false)
	{
		Query.NavData = GetMainNavData(FNavigationSystem::DontCreate);
	}
	
	FPathFindingResult Result(ENavigationQueryResult::Error);
	if (Query.NavData.IsValid())
	{
		if (Mode == EPathFindingMode::Hierarchical)
		{
			Result = Query.NavData->FindHierarchicalPath(FNavAgentProperties(), Query);
		}
		else
		{
			Result = Query.NavData->FindPath(FNavAgentProperties(), Query);
		}
	}

	return Result;
}

bool UNavigationSystem::TestPathSync(FPathFindingQuery Query, EPathFindingMode::Type Mode, int32* NumVisitedNodes) const
{
	SCOPE_CYCLE_COUNTER(STAT_Navigation_PathfindingSync);

	if (Query.NavData.IsValid() == false)
	{
		Query.NavData = GetMainNavData();
	}
	
	bool bExists = false;
	if (Query.NavData.IsValid())
	{
		if (Mode == EPathFindingMode::Hierarchical)
		{
			bExists = Query.NavData->TestHierarchicalPath(FNavAgentProperties(), Query, NumVisitedNodes);
		}
		else
		{
			bExists = Query.NavData->TestPath(FNavAgentProperties(), Query, NumVisitedNodes);
		}
	}

	return bExists;
}

void UNavigationSystem::AddAsyncQuery(const FAsyncPathFindingQuery& Query)
{
	check(IsInGameThread());
	AsyncPathFindingQueries.Add(Query);
}

uint32 UNavigationSystem::FindPathAsync(const FNavAgentProperties& AgentProperties, FPathFindingQuery Query, const FNavPathQueryDelegate& ResultDelegate, EPathFindingMode::Type Mode)
{
	SCOPE_CYCLE_COUNTER(STAT_Navigation_RequestingAsyncPathfinding);

	if (Query.NavData.IsValid() == false)
	{
		Query.NavData = GetNavDataForProps(AgentProperties);
	}

	if (Query.NavData.IsValid())
	{
		FAsyncPathFindingQuery AsyncQuery(Query, ResultDelegate, Mode);

		if (AsyncQuery.QueryID != INVALID_NAVQUERYID)
		{
			AddAsyncQuery(AsyncQuery);
		}

		return AsyncQuery.QueryID;
	}

	return INVALID_NAVQUERYID;
}

void UNavigationSystem::AbortAsyncFindPathRequest(uint32 AsynPathQueryID)
{
	check(IsInGameThread());
	FAsyncPathFindingQuery* Query = AsyncPathFindingQueries.GetData();
	for (int32 Index = 0; Index < AsyncPathFindingQueries.Num(); ++Index, ++Query)
	{
		if (Query->QueryID == AsynPathQueryID)
		{
			AsyncPathFindingQueries.RemoveAtSwap(Index);
			break;
		}
	}
}

void UNavigationSystem::TriggerAsyncQueries(TArray<FAsyncPathFindingQuery>& PathFindingQueries)
{
	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.NavigationSystem batched async queries"),
		STAT_FSimpleDelegateGraphTask_NavigationSystemBatchedAsyncQueries,
		STATGROUP_TaskGraphTasks);

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateUObject(this, &UNavigationSystem::PerformAsyncQueries, PathFindingQueries),
		GET_STATID(STAT_FSimpleDelegateGraphTask_NavigationSystemBatchedAsyncQueries));
}

static void AsyncQueryDone(FAsyncPathFindingQuery Query)
{
	Query.OnDoneDelegate.ExecuteIfBound(Query.QueryID, Query.Result.Result, Query.Result.Path);
}

void UNavigationSystem::PerformAsyncQueries(TArray<FAsyncPathFindingQuery> PathFindingQueries)
{
	SCOPE_CYCLE_COUNTER(STAT_Navigation_PathfindingAsync);

	if (PathFindingQueries.Num() == 0)
	{
		return;
	}
	
	const int32 QueriesCount = PathFindingQueries.Num();
	FAsyncPathFindingQuery* Query = PathFindingQueries.GetData();

	for (int32 QueryIndex = 0; QueryIndex < QueriesCount; ++QueryIndex, ++Query)
	{
		// @todo this is not necessarily the safest way to use UObjects outside of main thread. 
		//	think about something else.
		const ANavigationData* NavData = Query->NavData.IsValid() ? Query->NavData.Get() : GetMainNavData(FNavigationSystem::DontCreate);

		// perform query
		if (NavData)
		{
			if (Query->Mode == EPathFindingMode::Hierarchical)
			{
				Query->Result = NavData->FindHierarchicalPath(FNavAgentProperties(), *Query);
			}
			else
			{
				Query->Result = NavData->FindPath(FNavAgentProperties(), *Query);
			}
		}
		else
		{
			Query->Result = ENavigationQueryResult::Error;
		}

		// @todo make it return more informative results (bResult == false)
		// trigger calling delegate on main thread - otherwise it may depend too much on stuff being thread safe
		DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.Async nav query finished"),
			STAT_FSimpleDelegateGraphTask_AsyncNavQueryFinished,
			STATGROUP_TaskGraphTasks);

		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(AsyncQueryDone, *Query),
			GET_STATID(STAT_FSimpleDelegateGraphTask_AsyncNavQueryFinished), NULL, ENamedThreads::GameThread);
	}
}

bool UNavigationSystem::GetRandomPoint(FNavLocation& ResultLocation, ANavigationData* NavData, TSharedPtr<const FNavigationQueryFilter> QueryFilter)
{
	SCOPE_CYCLE_COUNTER(STAT_Navigation_QueriesTimeSync);

	if (NavData == NULL)
	{
		NavData = MainNavData;
	}

	if (NavData != NULL)
	{
		ResultLocation = NavData->GetRandomPoint(QueryFilter);
		return true;
	}

	return false;
}

bool UNavigationSystem::GetRandomPointInRadius(const FVector& Origin, float Radius, FNavLocation& ResultLocation, ANavigationData* NavData, TSharedPtr<const FNavigationQueryFilter> QueryFilter) const
{
	SCOPE_CYCLE_COUNTER(STAT_Navigation_QueriesTimeSync);

	if (NavData == nullptr)
	{
		NavData = MainNavData;
	}

	return NavData != nullptr && NavData->GetRandomPointInRadius(Origin, Radius, ResultLocation, QueryFilter);
}

ENavigationQueryResult::Type UNavigationSystem::GetPathCost(const FVector& PathStart, const FVector& PathEnd, float& OutPathCost, const ANavigationData* NavData, TSharedPtr<const FNavigationQueryFilter> QueryFilter) const
{
	SCOPE_CYCLE_COUNTER(STAT_Navigation_QueriesTimeSync);

	if (NavData == NULL)
	{
		NavData = GetMainNavData();
	}

	return NavData != NULL ? NavData->CalcPathCost(PathStart, PathEnd, OutPathCost, QueryFilter) : ENavigationQueryResult::Error;
}

ENavigationQueryResult::Type UNavigationSystem::GetPathLength(const FVector& PathStart, const FVector& PathEnd, float& OutPathLength, const ANavigationData* NavData, TSharedPtr<const FNavigationQueryFilter> QueryFilter) const
{
	SCOPE_CYCLE_COUNTER(STAT_Navigation_QueriesTimeSync);

	if (NavData == NULL)
	{
		NavData = GetMainNavData();
	}

	return NavData != NULL ? NavData->CalcPathLength(PathStart, PathEnd, OutPathLength, QueryFilter) : ENavigationQueryResult::Error;
}

ENavigationQueryResult::Type UNavigationSystem::GetPathLengthAndCost(const FVector& PathStart, const FVector& PathEnd, float& OutPathLength, float& OutPathCost, const ANavigationData* NavData, TSharedPtr<const FNavigationQueryFilter> QueryFilter) const
{
	SCOPE_CYCLE_COUNTER(STAT_Navigation_QueriesTimeSync);

	if (NavData == NULL)
	{
		NavData = GetMainNavData();
	}

	return NavData != NULL ? NavData->CalcPathLengthAndCost(PathStart, PathEnd, OutPathLength, OutPathCost, QueryFilter) : ENavigationQueryResult::Error;
}

bool UNavigationSystem::ProjectPointToNavigation(const FVector& Point, FNavLocation& OutLocation, const FVector& Extent, const ANavigationData* NavData, TSharedPtr<const FNavigationQueryFilter> QueryFilter) const
{
	SCOPE_CYCLE_COUNTER(STAT_Navigation_QueriesTimeSync);

	if (NavData == NULL)
	{
		NavData = GetMainNavData();
	}

	return NavData != NULL && NavData->ProjectPoint(Point, OutLocation
		, FNavigationSystem::IsValidExtent(Extent) ? Extent : NavData->NavDataConfig.DefaultQueryExtent
		, QueryFilter);
}

void UNavigationSystem::SimpleMoveToActor(AController* Controller, const AActor* Goal)
{
	UNavigationSystem* NavSys = Controller ? UNavigationSystem::GetCurrent(Controller->GetWorld()) : nullptr;
	if (NavSys == nullptr || Goal == nullptr || Controller == nullptr || Controller->GetPawn() == nullptr)
	{
		UE_LOG(LogNavigation, Warning, TEXT("UNavigationSystem::SimpleMoveToActor called for NavSys:%s Controller:%s controlling Pawn:%s with goal actor %s (if any of these is None then there's your problem"),
			*GetNameSafe(NavSys), *GetNameSafe(Controller), Controller ? *GetNameSafe(Controller->GetPawn()) : TEXT("NULL"), *GetNameSafe(Goal));
		return;
	}

	UPathFollowingComponent* PFollowComp = nullptr;
	Controller->InitNavigationControl(PFollowComp);

	if (PFollowComp == nullptr)
	{
		FMessageLog("PIE").Warning(FText::Format(
			LOCTEXT("SimpleMoveErrorNoComp", "SimpleMove failed for {0}: missing components"),
			FText::FromName(Controller->GetFName())
			));
		return;
	}

	if (!PFollowComp->IsPathFollowingAllowed())
	{
		FMessageLog("PIE").Warning(FText::Format(
			LOCTEXT("SimpleMoveErrorMovement", "SimpleMove failed for {0}: movement not allowed"),
			FText::FromName(Controller->GetFName())
			));
		return;
	}

	if (PFollowComp->HasReached(*Goal))
	{
		// make sure previous move request gets aborted
		PFollowComp->AbortMove(TEXT("Aborting move due to new move request finishing with AlreadyAtGoal"), FAIRequestID::AnyRequest);
		PFollowComp->SetLastMoveAtGoal(true);
	}
	else
	{
		const ANavigationData* NavData = NavSys->GetNavDataForProps(Controller->GetNavAgentPropertiesRef());
		FPathFindingQuery Query(Controller, NavData, Controller->GetNavAgentLocation(), Goal->GetActorLocation());
		FPathFindingResult Result = NavSys->FindPathSync(Query);
		if (Result.IsSuccessful())
		{
			Result.Path->SetGoalActorObservation(*Goal, 100.0f);

			PFollowComp->RequestMove(Result.Path, Goal);
		}
		else if (PFollowComp->GetStatus() != EPathFollowingStatus::Idle)
		{
			PFollowComp->AbortMove(TEXT("Aborting move due to new move request failing to generate a path"), FAIRequestID::AnyRequest);
			PFollowComp->SetLastMoveAtGoal(false);
		}
	}
}

void UNavigationSystem::SimpleMoveToLocation(AController* Controller, const FVector& GoalLocation)
{
	UNavigationSystem* NavSys = Controller ? UNavigationSystem::GetCurrent(Controller->GetWorld()) : nullptr;
	if (NavSys == nullptr || Controller == nullptr || Controller->GetPawn() == nullptr)
	{
		UE_LOG(LogNavigation, Warning, TEXT("UNavigationSystem::SimpleMoveToActor called for NavSys:%s Controller:%s controlling Pawn:%s (if any of these is None then there's your problem"),
			*GetNameSafe(NavSys), *GetNameSafe(Controller), Controller ? *GetNameSafe(Controller->GetPawn()) : TEXT("NULL"));
		return;
	}

	UPathFollowingComponent* PFollowComp = nullptr;
	Controller->InitNavigationControl(PFollowComp);

	if (PFollowComp == nullptr)
	{
		FMessageLog("PIE").Warning(FText::Format(
			LOCTEXT("SimpleMoveErrorNoComp", "SimpleMove failed for {0}: missing components"),
			FText::FromName(Controller->GetFName())
			));
		return;
	}

	if (!PFollowComp->IsPathFollowingAllowed())
	{
		FMessageLog("PIE").Warning(FText::Format(
			LOCTEXT("SimpleMoveErrorMovement", "SimpleMove failed for {0}: movement not allowed"),
			FText::FromName(Controller->GetFName())
			));
		return;
	}

	if (PFollowComp->HasReached(GoalLocation))
	{
		// make sure previous move request gets aborted
		PFollowComp->AbortMove(TEXT("Aborting move due to new move request finishing with AlreadyAtGoal"), FAIRequestID::AnyRequest);
		PFollowComp->SetLastMoveAtGoal(true);
	}
	else
	{
		const ANavigationData* NavData = NavSys->GetNavDataForProps(Controller->GetNavAgentPropertiesRef());
		FPathFindingQuery Query(Controller, NavData, Controller->GetNavAgentLocation(), GoalLocation);
		FPathFindingResult Result = NavSys->FindPathSync(Query);
		if (Result.IsSuccessful())
		{
			PFollowComp->RequestMove(Result.Path, NULL);
		}
		else if (PFollowComp->GetStatus() != EPathFollowingStatus::Idle)
		{
			PFollowComp->AbortMove(TEXT("Aborting move due to new move request failing to generate a path"), FAIRequestID::AnyRequest);
			PFollowComp->SetLastMoveAtGoal(false);
		}
	}
}

UNavigationPath* UNavigationSystem::FindPathToActorSynchronously(UObject* WorldContext, const FVector& PathStart, AActor* GoalActor, float TetherDistance, AActor* PathfindingContext, TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	if (GoalActor == NULL)
	{
		return NULL; 
	}

	INavAgentInterface* NavAgent = Cast<INavAgentInterface>(GoalActor);
	UNavigationPath* GeneratedPath = FindPathToLocationSynchronously(WorldContext, PathStart, NavAgent ? NavAgent->GetNavAgentLocation() : GoalActor->GetActorLocation(), PathfindingContext, FilterClass);
	if (GeneratedPath != NULL && GeneratedPath->GetPath().IsValid() == true)
	{
		GeneratedPath->GetPath()->SetGoalActorObservation(*GoalActor, TetherDistance);
	}

	return GeneratedPath;
}

UNavigationPath* UNavigationSystem::FindPathToLocationSynchronously(UObject* WorldContext, const FVector& PathStart, const FVector& PathEnd, AActor* PathfindingContext, TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	UWorld* World = NULL;

	if (WorldContext != NULL)
	{
		World = GEngine->GetWorldFromContextObject(WorldContext);
	}
	if (World == NULL && PathfindingContext != NULL)
	{
		World = GEngine->GetWorldFromContextObject(PathfindingContext);
	}

	UNavigationPath* ResultPath = NULL;

	if (World != NULL && World->GetNavigationSystem() != NULL)
	{
		UNavigationSystem* NavSys = World->GetNavigationSystem();

		ResultPath = NewObject<UNavigationPath>(NavSys);
		FPathFindingQuery Query(PathfindingContext, NULL, PathStart, PathEnd);
		bool bValidPathContext = false;
		const ANavigationData* NavigationData = NULL;

		if (PathfindingContext != NULL)
		{
			INavAgentInterface* NavAgent = Cast<INavAgentInterface>(PathfindingContext);
			
			if (NavAgent != NULL)
			{
				const FNavAgentProperties& AgentProps = NavAgent->GetNavAgentPropertiesRef();
				NavigationData = NavSys->GetNavDataForProps(AgentProps);
				bValidPathContext = true;
			}
			else if (Cast<ANavigationData>(PathfindingContext))
			{
				NavigationData = (ANavigationData*)PathfindingContext;
				bValidPathContext = true;
			}
		}
		if (bValidPathContext == false)
		{
			// just use default
			NavigationData = NavSys->GetMainNavData();
		}

		check(NavigationData);
		Query.NavData = NavigationData;		
		Query.QueryFilter = UNavigationQueryFilter::GetQueryFilter(NavigationData, FilterClass);

		FPathFindingResult Result = NavSys->FindPathSync(Query, EPathFindingMode::Regular);
		if (Result.IsSuccessful())
		{
			ResultPath->SetPath(Result.Path);
		}
	}

	return ResultPath;
}

bool UNavigationSystem::NavigationRaycast(UObject* WorldContextObject, const FVector& RayStart, const FVector& RayEnd, FVector& HitLocation, TSubclassOf<UNavigationQueryFilter> FilterClass, AController* Querier)
{
	UWorld* World = NULL;

	if (WorldContextObject != NULL)
	{
		World = GEngine->GetWorldFromContextObject(WorldContextObject);
	}
	if (World == NULL && Querier != NULL)
	{
		World = GEngine->GetWorldFromContextObject(Querier);
	}

	// blocked, i.e. not traversable, by default
	bool bRaycastBlocked = true;
	HitLocation = RayStart;

	if (World != NULL && World->GetNavigationSystem() != NULL)
	{
		const UNavigationSystem* NavSys = World->GetNavigationSystem();

		// figure out which navigation data to use
		const ANavigationData* NavData = NULL;
		INavAgentInterface* MyNavAgent = Cast<INavAgentInterface>(Querier);
		if (MyNavAgent)
		{
			const FNavAgentProperties& AgentProps = MyNavAgent->GetNavAgentPropertiesRef();
			NavData = NavSys->GetNavDataForProps(AgentProps);
		}
		if (NavData == NULL)
		{
			NavData = NavSys->GetMainNavData();
		}

		if (NavData != NULL)
		{
			bRaycastBlocked = NavData->Raycast(RayStart, RayEnd, HitLocation, UNavigationQueryFilter::GetQueryFilter(NavData, FilterClass));
		}
	}

	return bRaycastBlocked;
}

void UNavigationSystem::GetNavAgentPropertiesArray(TArray<FNavAgentProperties>& OutNavAgentProperties) const
{
	AgentToNavDataMap.GetKeys(OutNavAgentProperties);
}

ANavigationData* UNavigationSystem::GetNavDataForProps(const FNavAgentProperties& AgentProperties)
{
	const UNavigationSystem* ConstThis = const_cast<const UNavigationSystem*>(this);
	return const_cast<ANavigationData*>(ConstThis->GetNavDataForProps(AgentProperties));
}

// @todo could optimize this by having "SupportedAgentIndex" in FNavAgentProperties
const ANavigationData* UNavigationSystem::GetNavDataForProps(const FNavAgentProperties& AgentProperties) const
{
	if (SupportedAgents.Num() <= 1)
	{
		return MainNavData;
	}
	
	const ANavigationData* const* NavDataForAgent = AgentToNavDataMap.Find(AgentProperties);

	if (NavDataForAgent == NULL)
	{
		TArray<FNavAgentProperties> AgentPropertiesList;
		int32 NumNavDatas = AgentToNavDataMap.GetKeys(AgentPropertiesList);
		
		FNavAgentProperties BestFitNavAgent;
		float BestExcessHeight = -FLT_MAX;
		float BestExcessRadius = -FLT_MAX;
		float ExcessRadius = -FLT_MAX;
		float ExcessHeight = -FLT_MAX;
		const float AgentHeight = bSkipAgentHeightCheckWhenPickingNavData ? 0.f : AgentProperties.AgentHeight;

		for (TArray<FNavAgentProperties>::TConstIterator It(AgentPropertiesList); It; ++It)
		{
			const FNavAgentProperties& NavIt = *It;
			ExcessRadius = NavIt.AgentRadius - AgentProperties.AgentRadius;
			ExcessHeight = NavIt.AgentHeight - AgentHeight;

			const bool bExcessRadiusIsBetter = ((ExcessRadius == 0) && (BestExcessRadius != 0)) 
				|| ((ExcessRadius > 0) && (BestExcessRadius < 0))
				|| ((ExcessRadius > 0) && (BestExcessRadius > 0) && (ExcessRadius < BestExcessRadius))
				|| ((ExcessRadius < 0) && (BestExcessRadius < 0) && (ExcessRadius > BestExcessRadius));
			const bool bExcessHeightIsBetter = ((ExcessHeight == 0) && (BestExcessHeight != 0))
				|| ((ExcessHeight > 0) && (BestExcessHeight < 0))
				|| ((ExcessHeight > 0) && (BestExcessHeight > 0) && (ExcessHeight < BestExcessHeight))
				|| ((ExcessHeight < 0) && (BestExcessHeight < 0) && (ExcessHeight > BestExcessHeight));
			const bool bBestIsValid = (BestExcessRadius >= 0) && (BestExcessHeight >= 0);
			const bool bRadiusEquals = (ExcessRadius == BestExcessRadius);
			const bool bHeightEquals = (ExcessHeight == BestExcessHeight);

			bool bValuesAreBest = ((bExcessRadiusIsBetter || bRadiusEquals) && (bExcessHeightIsBetter || bHeightEquals));
			if (!bValuesAreBest && !bBestIsValid)
			{
				bValuesAreBest = bExcessRadiusIsBetter || (bRadiusEquals && bExcessHeightIsBetter);
			}

			if (bValuesAreBest)
			{
				BestFitNavAgent = NavIt;
				BestExcessHeight = ExcessHeight;
				BestExcessRadius = ExcessRadius;
			}
		}

		if (BestFitNavAgent.IsValid())
		{
			NavDataForAgent = AgentToNavDataMap.Find(BestFitNavAgent);
		}
	}

	return NavDataForAgent != NULL && *NavDataForAgent != NULL ? *NavDataForAgent : MainNavData;
}

ANavigationData* UNavigationSystem::GetMainNavData(FNavigationSystem::ECreateIfEmpty CreateNewIfNoneFound)
{
	checkSlow(IsInGameThread() == true);

	if (MainNavData == NULL || MainNavData->IsPendingKill())
	{
		MainNavData = NULL;

		// @TODO this should be done a differently. There should be specified a "default agent"
		for (int32 NavDataIndex = 0; NavDataIndex < NavDataSet.Num(); ++NavDataIndex)
		{
			ANavigationData* NavData = NavDataSet[NavDataIndex];
			if (NavData != NULL && NavData->IsPendingKill() == false && !NavData->IsA(AAbstractNavData::StaticClass()))
			{
				MainNavData = NavData;
				break;
			}
		}

#if WITH_RECAST
		if ( /*GIsEditor && */(MainNavData == NULL) && CreateNewIfNoneFound == FNavigationSystem::Create )
		{
			// Spawn a new one if we're in the editor.  In-game, either we loaded one or we don't get one.
			MainNavData = GetWorld()->SpawnActor<ANavigationData>(ARecastNavMesh::StaticClass());
		}
#endif // WITH_RECAST
		// either way make sure it's registered. Registration stores unique
		// navmeshes, so we have nothing to loose
		RegisterNavData(MainNavData);
	}

	return MainNavData;
}

TSharedPtr<FNavigationQueryFilter> UNavigationSystem::CreateDefaultQueryFilterCopy() const 
{ 
	return MainNavData ? MainNavData->GetDefaultQueryFilter()->GetCopy() : NULL; 
}

bool UNavigationSystem::IsNavigationBuilt(const AWorldSettings* Settings) const
{
	if (Settings == nullptr || Settings->bEnableNavigationSystem == false || IsThereAnywhereToBuildNavigation() == false)
	{
		return true;
	}

	bool bIsBuilt = true;

	for (int32 NavDataIndex = 0; NavDataIndex < NavDataSet.Num(); ++NavDataIndex)
	{
		ANavigationData* NavData = NavDataSet[NavDataIndex];
		if (NavData != NULL && NavData->GetWorldSettings() == Settings)
		{
			FNavDataGenerator* Generator = NavData->GetGenerator();
			if ((NavData->bRebuildAtRuntime == true 
#if WITH_EDITOR
				|| GEditor != NULL
#endif // WITH_EDITOR
				) && (Generator == NULL || Generator->IsBuildInProgress(/*bCheckDirtyToo=*/true) == true))
			{
				bIsBuilt = false;
				break;
			}
		}
	}

	return bIsBuilt;
}

bool UNavigationSystem::IsThereAnywhereToBuildNavigation() const
{
	// not check if there are any volumes or other structures requiring/supporting navigation building
	if (bWholeWorldNavigable == true)
	{
		return true;
	}

	// @TODO this should be done more flexible to be able to trigger this from game-specific 
	// code (like Navigation System's subclass maybe)
	bool bCreateNavigation = false;

	for (TActorIterator<ANavMeshBoundsVolume> It(GetWorld()); It; ++It)
	{
		ANavMeshBoundsVolume const* const V = (*It);
		if (V != NULL && !V->IsPendingKill())
		{
			bCreateNavigation = true;
			break;
		}
	}

	return bCreateNavigation;
}

bool UNavigationSystem::IsNavigationRelevant(const AActor* TestActor) const
{
	const INavRelevantInterface* NavInterface = Cast<const INavRelevantInterface>(TestActor);
	if (NavInterface && NavInterface->IsNavigationRelevant())
	{
		return true;
	}

	TArray<UActorComponent*> Components;
	if (TestActor)
	{
		TestActor->GetComponents(Components);
	}

	for (int32 Idx = 0; Idx < Components.Num(); Idx++)
	{
		NavInterface = Cast<const INavRelevantInterface>(Components[Idx]);
		if (NavInterface && NavInterface->IsNavigationRelevant())
		{
			return true;
		}
	}

	return false;
}

FBox UNavigationSystem::GetWorldBounds() const
{
	checkSlow(IsInGameThread() == true);

	NavigableWorldBounds = FBox(0);

	if (GetWorld() != NULL && bWholeWorldNavigable == true)
	{
		// @TODO - super slow! Need to ask tech guys where I can get this from
		for( FActorIterator It(GetWorld()); It; ++It )
		{
			if (IsNavigationRelevant(*It))
			{
				NavigableWorldBounds += (*It)->GetComponentsBoundingBox();
			}
		}
	}

	return NavigableWorldBounds;
}

FBox UNavigationSystem::GetLevelBounds(ULevel* InLevel) const
{
	FBox NavigableLevelBounds(0);

	if (InLevel)
	{
		AActor** Actor = InLevel->Actors.GetData();
		const int32 ActorCount = InLevel->Actors.Num();
		for (int32 ActorIndex = 0; ActorIndex < ActorCount; ++ActorIndex, ++Actor)
		{
			if (IsNavigationRelevant(*Actor))
			{
				NavigableLevelBounds += (*Actor)->GetComponentsBoundingBox();
			}
		}
	}

	return NavigableLevelBounds;
}

const TSet<FNavigationBounds>& UNavigationSystem::GetNavigationBounds() const
{
	return RegisteredNavBounds;
}

void UNavigationSystem::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	for (int32 NavDataIndex = 0; NavDataIndex < NavDataSet.Num(); ++NavDataIndex)
	{
		NavDataSet[NavDataIndex]->ApplyWorldOffset(InOffset, bWorldShift);
	}
}

//----------------------------------------------------------------------//
// Bookkeeping 
//----------------------------------------------------------------------//
void UNavigationSystem::RequestRegistration(ANavigationData* NavData, bool bTriggerRegistrationProcessing)
{
	FScopeLock RegistrationLock(&NavDataRegistrationSection);

	if (NavDataRegistrationQueue.Num() < REGISTRATION_QUEUE_SIZE)
	{
		NavDataRegistrationQueue.AddUnique(NavData);

		if (bTriggerRegistrationProcessing == true)
		{
			// trigger registration candidates processing
			DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.Process registration candidates"),
				STAT_FSimpleDelegateGraphTask_ProcessRegistrationCandidates,
				STATGROUP_TaskGraphTasks);

			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateUObject(this, &UNavigationSystem::ProcessRegistrationCandidates),
				GET_STATID(STAT_FSimpleDelegateGraphTask_ProcessRegistrationCandidates), NULL, ENamedThreads::GameThread);
		}
	}
	else
	{
		UE_LOG(LogNavigation, Error, TEXT("Navigation System: registration queue full!"));
	}
}

void UNavigationSystem::ProcessRegistrationCandidates()
{
	FScopeLock RegistrationLock(&NavDataRegistrationSection);

	if (NavDataRegistrationQueue.Num() == 0)
	{
		return;
	}

	ANavigationData** NavDataPtr = NavDataRegistrationQueue.GetData();
	const int CandidatesCount = NavDataRegistrationQueue.Num();

	for (int32 CandidateIndex = 0; CandidateIndex < CandidatesCount; ++CandidateIndex, ++NavDataPtr)
	{
		if (*NavDataPtr != NULL)
		{
			ERegistrationResult Result = RegisterNavData(*NavDataPtr);

			if (Result == RegistrationSuccessful)
			{
				continue;
			}
			else if (Result != RegistrationFailed_DataPendingKill)
			{
				(*NavDataPtr)->CleanUpAndMarkPendingKill();
				if ((*NavDataPtr) == MainNavData)
				{
					MainNavData = NULL;
				}
			}
		}
	}

	MainNavData = GetMainNavData(FNavigationSystem::DontCreate);
	
	// we processed all candidates so clear the queue
	NavDataRegistrationQueue.Reset();
}

void UNavigationSystem::ProcessNavAreaPendingRegistration()
{
	TArray<UClass*> TempPending = PendingNavAreaRegistration;

	// Clear out old array, will fill in if still pending
	PendingNavAreaRegistration.Empty();

	for (int32 PendingAreaIndex = 0; PendingAreaIndex < TempPending.Num(); PendingAreaIndex++)
	{
		RegisterNavAreaClass(TempPending[PendingAreaIndex]);
	}
}

UNavigationSystem::ERegistrationResult UNavigationSystem::RegisterNavData(ANavigationData* NavData)
{
	if (NavData == NULL)
	{
		return RegistrationError;
	}
	else if (NavData->IsPendingKill() == true)
	{
		return RegistrationFailed_DataPendingKill;
	}
	// still to be seen if this is really true, but feels right
	else if (NavData->IsRegistered() == true)
	{
		return RegistrationSuccessful;
	}

	FScopeLock Lock(&NavDataRegistration);

	UNavigationSystem::ERegistrationResult Result = RegistrationError;

	// find out which, if any, navigation agents are supported by this nav data
	// if none then fail the registration
	FNavDataConfig NavConfig = NavData->GetConfig();

	// not discarding navmesh when there's only one Supported Agent
	if (NavConfig.IsValid() == false && SupportedAgents.Num() == 1)
	{
		// fill in AgentProps with whatever is the instance's setup
		NavConfig = SupportedAgents[0];
		NavData->SetConfig(SupportedAgents[0]);
		NavData->SetSupportsDefaultAgent(true);	
		NavData->ProcessNavAreas(NavAreaClasses, 0);
	}

	if (NavConfig.IsValid() == true)
	{
		// check if this kind of agent has already its navigation implemented
		ANavigationData** NavDataForAgent = AgentToNavDataMap.Find(NavConfig);
		if (NavDataForAgent == NULL || *NavDataForAgent == NULL || (*NavDataForAgent)->IsPendingKill() == true)
		{
			// ok, so this navigation agent doesn't have its navmesh registered yet, but do we want to support it?
			bool bAgentSupported = false;
			
			for (int32 AgentIndex = 0; AgentIndex < SupportedAgents.Num(); ++AgentIndex)
			{
				if (NavData->GetClass() == SupportedAgents[AgentIndex].NavigationDataClass && SupportedAgents[AgentIndex].IsEquivalent(NavConfig) == true)
				{
					// it's supported, then just in case it's not a precise match (IsEquivalent succeeds with some precision) 
					// update NavData with supported Agent
					bAgentSupported = true;

					NavData->SetConfig(SupportedAgents[AgentIndex]);
					if (!NavData->IsA(AAbstractNavData::StaticClass()))
					{
						AgentToNavDataMap.Add(SupportedAgents[AgentIndex], NavData);
					}

					NavData->SetSupportsDefaultAgent(AgentIndex == 0);
					NavData->ProcessNavAreas(NavAreaClasses, AgentIndex);

					OnNavDataRegisteredEvent.Broadcast(NavData);
					break;
				}
			}

			Result = bAgentSupported == true ? RegistrationSuccessful : RegistrationFailed_AgentNotValid;
		}
		else if (*NavDataForAgent == NavData)
		{
			// let's treat double registration of the same nav data with the same agent as a success
			Result = RegistrationSuccessful;
		}
		else
		{
			// otherwise specified agent type already has its navmesh implemented, fail redundant instance
			Result = RegistrationFailed_AgentAlreadySupported;
		}
	}
	else
	{
		Result = RegistrationFailed_AgentNotValid;
	}

	if (Result == RegistrationSuccessful)
	{
		NavDataSet.AddUnique(NavData);
		NavData->OnRegistered();
	}
	// @todo else might consider modifying this NavData to implement navigation for one of the supported agents
	// care needs to be taken to not make it implement navigation for agent who's real implementation has 
	// not been loaded yet.

	return Result;
}

void UNavigationSystem::UnregisterNavData(ANavigationData* NavData)
{
	if (NavData == NULL)
	{
		return;
	}

	FScopeLock Lock(&NavDataRegistration);

	NavDataSet.RemoveSingle(NavData);
	NavData->OnUnregistered();
}

void UNavigationSystem::RegisterCustomLink(INavLinkCustomInterface* CustomLink)
{
	CustomLinksMap.Add(CustomLink->GetLinkId(), CustomLink);
}

void UNavigationSystem::UnregisterCustomLink(INavLinkCustomInterface* CustomLink)
{
	CustomLinksMap.Remove(CustomLink->GetLinkId());
}

INavLinkCustomInterface* UNavigationSystem::GetCustomLink(uint32 UniqueLinkId) const
{
	return CustomLinksMap.FindRef(UniqueLinkId);
}

void UNavigationSystem::UpdateCustomLink(const INavLinkCustomInterface* CustomLink)
{
	for (TMap<FNavAgentProperties, ANavigationData*>::TIterator It(AgentToNavDataMap); It; ++It)
	{
		ANavigationData* NavData = It.Value();
		NavData->UpdateCustomLink(CustomLink);
	}
}

void UNavigationSystem::RequestAreaUnregistering(UClass* NavAreaClass)
{
	check(IsInGameThread());

	if (NavAreaClasses.Contains(NavAreaClass))
	{
		// remove from known areas
		NavAreaClasses.RemoveSingleSwap(NavAreaClass);
		PendingNavAreaRegistration.RemoveSingleSwap(NavAreaClass);

		// notify navigation data
		// notify existing nav data
		for (TObjectIterator<UWorld> It; It; ++It)
		{
			if (It->GetNavigationSystem())
			{
				It->GetNavigationSystem()->OnNavigationAreaEvent(NavAreaClass, ENavAreaEvent::Unregistered);
			}
		}
	}
}

void UNavigationSystem::RequestAreaRegistering(UClass* NavAreaClass)
{
	check(IsInGameThread());

	// can't be null
	if (NavAreaClass == NULL)
	{
		return;
	}

	// can't be abstract
	if (NavAreaClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return;
	}

	// special handling of blueprint based areas
	if (NavAreaClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
	{
		// can't be skeleton of blueprint class
		if (NavAreaClass->GetName().Contains(TEXT("SKEL_")))
		{
			return;
		}

		// can't be class from Developers folder (won't be saved properly anyway)
		const UPackage* Package = NavAreaClass->GetOutermost();
		if (Package && Package->GetName().Contains(TEXT("/Developers/")) )
		{
			return;
		}
	}

	PendingNavAreaRegistration.Add(NavAreaClass);
}

void UNavigationSystem::RegisterNavAreaClass(UClass* AreaClass)
{
#if WITH_EDITORONLY_DATA
	if (AreaClass->ClassGeneratedBy && AreaClass->ClassGeneratedBy->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad))
	{
		// Class isn't done loading, try again later
		PendingNavAreaRegistration.Add(AreaClass);
		return;
	}
#endif //WITH_EDITORONLY_DATA

	// add to know areas
	NavAreaClasses.AddUnique(AreaClass);

	// notify existing nav data
	for (TObjectIterator<UWorld> It; It; ++It)
	{
		if (It->GetNavigationSystem())
		{
			It->GetNavigationSystem()->OnNavigationAreaEvent(AreaClass, ENavAreaEvent::Registered);
		}
	}

#if WITH_EDITOR
	// update area properties
	AreaClass->GetDefaultObject<UNavArea>()->UpdateAgentConfig();
#endif
}

void UNavigationSystem::OnNavigationAreaEvent(UClass* AreaClass, ENavAreaEvent::Type Event)
{
	// notify existing nav data
	for (auto NavigationData : NavDataSet)
	{
		if (NavigationData != NULL && NavigationData->IsPendingKillPending() == false)
		{
			NavigationData->OnNavAreaEvent(AreaClass, Event);
		}
	}
}

int32 UNavigationSystem::GetSupportedAgentIndex(const ANavigationData* NavData) const
{
	if (SupportedAgents.Num() < 2)
	{
		return 0;
	}

	const FNavDataConfig& TestConfig = NavData->GetConfig();
	for (int32 AgentIndex = 0; AgentIndex < SupportedAgents.Num(); AgentIndex++)
	{
		if (SupportedAgents[AgentIndex].IsEquivalent(TestConfig))
		{
			return AgentIndex;
		}
	}
	
	return INDEX_NONE;
}

int32 UNavigationSystem::GetSupportedAgentIndex(const FNavAgentProperties& NavAgent) const
{
	if (SupportedAgents.Num() < 2)
	{
		return 0;
	}

	for (int32 AgentIndex = 0; AgentIndex < SupportedAgents.Num(); AgentIndex++)
	{
		if (SupportedAgents[AgentIndex].IsEquivalent(NavAgent))
		{
			return AgentIndex;
		}
	}

	return INDEX_NONE;
}

void UNavigationSystem::DescribeFilterFlags(UEnum* FlagsEnum) const
{
#if WITH_EDITOR
	TArray<FString> FlagDesc;
	FString EmptyStr;
	FlagDesc.Init(EmptyStr, 16);

	const int32 NumEnums = FMath::Min(16, FlagsEnum->NumEnums() - 1);	// skip _MAX
	for (int32 FlagIndex = 0; FlagIndex < NumEnums; FlagIndex++)
	{
		FlagDesc[FlagIndex] = FlagsEnum->GetEnumText(FlagIndex).ToString();
	}

	DescribeFilterFlags(FlagDesc);
#endif
}

void UNavigationSystem::DescribeFilterFlags(const TArray<FString>& FlagsDesc) const
{
#if WITH_EDITOR
	const int32 MaxFlags = 16;
	TArray<FString> UseDesc = FlagsDesc;

	FString EmptyStr;
	while (UseDesc.Num() < MaxFlags)
	{
		UseDesc.Add(EmptyStr);
	}

	// get special value from recast's navmesh
#if WITH_RECAST
	uint16 NavLinkFlag = ARecastNavMesh::GetNavLinkFlag();
	for (int32 FlagIndex = 0; FlagIndex < MaxFlags; FlagIndex++)
	{
		if ((NavLinkFlag >> FlagIndex) & 1)
		{
			UseDesc[FlagIndex] = TEXT("Navigation link");
			break;
		}
	}
#endif

	// setup properties
	UStructProperty* StructProp1 = FindField<UStructProperty>(UNavigationQueryFilter::StaticClass(), TEXT("IncludeFlags"));
	UStructProperty* StructProp2 = FindField<UStructProperty>(UNavigationQueryFilter::StaticClass(), TEXT("ExcludeFlags"));
	check(StructProp1);
	check(StructProp2);

	UStruct* Structs[] = { StructProp1->Struct, StructProp2->Struct };
	const FString CustomNameMeta = TEXT("DisplayName");

	for (int32 StructIndex = 0; StructIndex < ARRAY_COUNT(Structs); StructIndex++)
	{
		for (int32 FlagIndex = 0; FlagIndex < MaxFlags; FlagIndex++)
		{
			FString PropName = FString::Printf(TEXT("bNavFlag%d"), FlagIndex);
			UProperty* Prop = FindField<UProperty>(Structs[StructIndex], *PropName);
			check(Prop);

			if (UseDesc[FlagIndex].Len())
			{
				Prop->SetPropertyFlags(CPF_Edit);
				Prop->SetMetaData(*CustomNameMeta, *UseDesc[FlagIndex]);
			}
			else
			{
				Prop->ClearPropertyFlags(CPF_Edit);
			}
		}
	}

#endif
}

void UNavigationSystem::ResetCachedFilter(TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	for (int32 NavDataIndex = 0; NavDataIndex < NavDataSet.Num(); NavDataIndex++)
	{
		if (NavDataSet[NavDataIndex])
		{
			NavDataSet[NavDataIndex]->RemoveQueryFilter(FilterClass);
		}
	}
}

void UNavigationSystem::RegisterGenerationSeed(AActor* SeedActor)
{
	GenerationSeeds.Add(SeedActor);
}

void UNavigationSystem::UnregisterGenerationSeed(AActor* SeedActor)
{
	GenerationSeeds.RemoveSingleSwap(SeedActor);
}

void UNavigationSystem::GetGenerationSeeds(TArray<FVector>& SeedLocations) const
{
	if (bAddPlayersToGenerationSeeds)
	{
		for (FConstPlayerControllerIterator it = GetWorld()->GetPlayerControllerIterator(); it ;it++)
		{
			if (*it && (*it)->GetPawn() != NULL)
			{
				const FVector SeedLoc((*it)->GetPawn()->GetActorLocation());
				SeedLocations.Add(SeedLoc);
			}
		}
	}

	for (int32 SeedIndex = 0; SeedIndex < GenerationSeeds.Num(); SeedIndex++)
	{
		if (GenerationSeeds[SeedIndex].IsValid())
		{
			SeedLocations.Add(GenerationSeeds[SeedIndex]->GetActorLocation());
		}
	}
}

UNavigationSystem* UNavigationSystem::CreateNavigationSystem(UWorld* WorldOwner)
{
	UNavigationSystem* NavSys = NULL;

#if WITH_SERVER_CODE || WITH_EDITOR
	// create navigation system for editor and server targets, but remove it from game clients
	if (WorldOwner && (WorldOwner->GetNetMode() != NM_Client) && (*GEngine->NavigationSystemClass != nullptr))
	{
		AWorldSettings* WorldSettings = WorldOwner->GetWorldSettings();
		if (WorldSettings == NULL || WorldSettings->bEnableNavigationSystem)
		{
			NavSys = NewObject<UNavigationSystem>(WorldOwner, GEngine->NavigationSystemClass);		
			WorldOwner->SetNavigationSystem(NavSys);
		}
	}
#endif

	return NavSys;
}

void UNavigationSystem::InitializeForWorld(UWorld* World, FNavigationSystem::EMode Mode)
{
	if (World)
	{
		UNavigationSystem* NavSys = World->GetNavigationSystem();
		if (NavSys == NULL)
		{
			NavSys = CreateNavigationSystem(World);
		}

		// Remove old chunk data from all levels
		// In case navigation system will be created chunks will be regenerated anyway
		if (Mode == FNavigationSystem::EditorMode)
		{
			const auto& Levels = World->GetLevels();
			for (ULevel* Level : Levels)
			{
				Level->NavDataChunks.Empty();
			}
		}

		if (NavSys)
		{
			NavSys->OnWorldInitDone(Mode);
		}
	}
}

UNavigationSystem* UNavigationSystem::GetCurrent(UWorld* World)
{
	return World ? World->GetNavigationSystem() : NULL;
}

UNavigationSystem* UNavigationSystem::GetCurrent(UObject* WorldContextObject)
{
	UWorld* World = NULL;

	if (WorldContextObject != NULL)
	{
		World = GEngine->GetWorldFromContextObject(WorldContextObject);
	}

	return World ? World->GetNavigationSystem() : NULL;
}

ANavigationData* UNavigationSystem::GetNavDataWithID(const uint16 NavDataID) const
{
	for (int32 NavDataIndex = 0; NavDataIndex < NavDataSet.Num(); ++NavDataIndex)
	{
		const ANavigationData* NavData = NavDataSet[NavDataIndex];
		if (NavData != NULL && NavData->GetNavDataUniqueID() == NavDataID)
		{
			return const_cast<ANavigationData*>(NavData);
		}
	}

	return NULL;
}

void UNavigationSystem::AddDirtyArea(const FBox& NewArea, int32 Flags)
{
	if (Flags > 0)
	{
		DirtyAreas.Add(FNavigationDirtyArea(NewArea, Flags));
	}
}

void UNavigationSystem::AddDirtyAreas(const TArray<FBox>& NewAreas, int32 Flags)
{ 
	for (int32 NewAreaIndex = 0; NewAreaIndex < NewAreas.Num(); NewAreaIndex++)
	{
		AddDirtyArea(NewAreas[NewAreaIndex], Flags);
	}
}

int32 GetDirtyFlagHelper(int32 UpdateFlags, int32 DefaultValue)
{
	return ((UpdateFlags & UNavigationSystem::OctreeUpdate_Geometry) != 0) ? ENavigationDirtyFlag::All :
		((UpdateFlags & UNavigationSystem::OctreeUpdate_Modifiers) != 0) ? ENavigationDirtyFlag::DynamicModifier :		
		DefaultValue;
}

FSetElementId UNavigationSystem::RegisterNavOctreeElement(UObject* ElementOwner, INavRelevantInterface* ElementInterface, int32 UpdateFlags)
{
	FSetElementId SetId;

#if WITH_EDITOR
	if (IsNavigationRegisterLocked())
	{
		return SetId;
	}
#endif

	if (NavOctree == NULL || ElementOwner == NULL || ElementInterface == NULL)
	{
		return SetId;
	}

	const bool bIsRelevant = ElementInterface->IsNavigationRelevant();
	UE_LOG(LogNavOctree, Log, TEXT("REG %s %s"), *GetNameSafe(ElementOwner), bIsRelevant ? TEXT("[relevant]") : TEXT(""));

	if (bIsRelevant)
	{
		bool bCanAdd = false;

		UObject* ParentNode = ElementInterface->GetNavigationParent();
		if (ParentNode)
		{
			OctreeChildNodesMap.AddUnique(ParentNode, FWeakObjectPtr(ElementOwner));
			bCanAdd = true;
		}
		else
		{
			const FOctreeElementId* ElementId = GetObjectsNavOctreeId(ElementOwner);
			bCanAdd = (ElementId == NULL);
		}

		if (bCanAdd)
		{
			FNavigationDirtyElement UpdateInfo(ElementOwner, ElementInterface, GetDirtyFlagHelper(UpdateFlags, 0));

			SetId = PendingOctreeUpdates.FindId(UpdateInfo);
			if (SetId.IsValidId())
			{
				// make sure this request stays, in case it has been invalidated already
				PendingOctreeUpdates[SetId] = UpdateInfo;
			}
			else
			{
				SetId = PendingOctreeUpdates.Add(UpdateInfo);
			}
		}
	}

	return SetId;
}

void UNavigationSystem::AddElementToNavOctree(const FNavigationDirtyElement& DirtyElement)
{
	// handle invalidated requests first
	if (DirtyElement.bInvalidRequest)
	{
		if (DirtyElement.bHasPrevData)
		{
			AddDirtyArea(DirtyElement.PrevBounds, DirtyElement.PrevFlags);
		}
		
		return;
	}

	UObject* ElementOwner = DirtyElement.Owner.Get();
	if (ElementOwner == NULL || ElementOwner->IsPendingKill())
	{
		return;
	}

	FNavigationOctreeElement GeneratedData;
	const FBox ElementBounds = DirtyElement.NavInterface->GetNavigationBounds();

	UObject* ParentNode = DirtyElement.NavInterface->GetNavigationParent();
	if (ParentNode)
	{
		// check if parent node is waiting in queue
		const FSetElementId ParentRequestId = PendingOctreeUpdates.FindId(FNavigationDirtyElement(ParentNode));
		const FOctreeElementId* ParentId = GetObjectsNavOctreeId(ParentNode);
		if (ParentRequestId.IsValidId() && ParentId == NULL)
		{
			FNavigationDirtyElement& ParentNode = PendingOctreeUpdates[ParentRequestId];
			AddElementToNavOctree(ParentNode);

			// mark as invalid so it won't be processed twice
			ParentNode.bInvalidRequest = true;
		}

		const FOctreeElementId* UseParentId = ParentId ? ParentId : GetObjectsNavOctreeId(ParentNode);
		if (UseParentId)
		{
			UE_LOG(LogNavOctree, Log, TEXT("ADD %s to %s"), *GetNameSafe(ElementOwner), *GetNameSafe(ParentNode));
			NavOctree->AppendToNode(*UseParentId, DirtyElement.NavInterface, ElementBounds, GeneratedData);
		}
		else 
		{
			UE_LOG(LogNavOctree, Warning, TEXT("Can't add node [%s] - parent [%s] not found in octree!"), *GetNameSafe(ElementOwner), *GetNameSafe(ParentNode));
		}
	}
	else
	{
		UE_LOG(LogNavOctree, Log, TEXT("ADD %s"), *GetNameSafe(ElementOwner));
		NavOctree->AddNode(ElementOwner, DirtyElement.NavInterface, ElementBounds, GeneratedData);
	}

	const FBox BBox = GeneratedData.Bounds.GetBox();
	const bool bValidBBox = BBox.IsValid && !BBox.GetSize().IsNearlyZero();

	if (BBox.GetExtent().X > 400000)
	{
		volatile int32 i = 0;
	}

	if (bValidBBox && !GeneratedData.IsEmpty())
	{
		const int32 DirtyFlag = DirtyElement.FlagsOverride ? DirtyElement.FlagsOverride : GeneratedData.Data.GetDirtyFlag();
		AddDirtyArea(BBox, DirtyFlag);
	}
}

bool UNavigationSystem::GetNavOctreeElementData(UObject* NodeOwner, int32& DirtyFlags, FBox& DirtyBounds)
{
	const FOctreeElementId* ElementId = GetObjectsNavOctreeId(NodeOwner);
	if (ElementId != NULL)
	{
		if (NavOctree->IsValidElementId(*ElementId))
		{
			// mark area occupied by given actor as dirty
			FNavigationOctreeElement& ElementData = NavOctree->GetElementById(*ElementId);
			DirtyFlags = ElementData.Data.GetDirtyFlag();
			DirtyBounds = ElementData.Bounds.GetBox();
			return true;
		}
	}

	return false;
}

void UNavigationSystem::UnregisterNavOctreeElement(UObject* ElementOwner, INavRelevantInterface* ElementInterface, int32 UpdateFlags)
{
#if WITH_EDITOR
	if (IsNavigationUnregisterLocked())
	{
		return;
	}
#endif

	if (NavOctree == NULL || ElementOwner == NULL || ElementInterface == NULL)
	{
		return;
	}

	const FOctreeElementId* ElementId = GetObjectsNavOctreeId(ElementOwner);
	UE_LOG(LogNavOctree, Log, TEXT("UNREG %s %s"), *GetNameSafe(ElementOwner), ElementId ? TEXT("[exists]") : TEXT(""));

	if (ElementId != NULL)
	{
		RemoveNavOctreeElementId(*ElementId, UpdateFlags);
		RemoveObjectsNavOctreeId(ElementOwner);
	}
	else
	{
		const bool bCanRemoveChildNode = (UpdateFlags & OctreeUpdate_ParentChain) == 0;
		UObject* ParentNode = ElementInterface->GetNavigationParent();
		if (ParentNode && bCanRemoveChildNode)
		{
			// if node has navigation parent (= doesn't exists in octree on its own)
			// and it's not part of parent chain update
			// remove it from map and force update on parent to rebuild octree element

			OctreeChildNodesMap.RemoveSingle(ParentNode, FWeakObjectPtr(ElementOwner));
			UpdateNavOctreeParentChain(ParentNode);
		}
	}

	// mark pending update as invalid, it will be dirtied according to currently active settings
	const bool bCanInvalidateQueue = (UpdateFlags & OctreeUpdate_Refresh) == 0;
	if (bCanInvalidateQueue)
	{
		const FSetElementId RequestId = PendingOctreeUpdates.FindId(FNavigationDirtyElement(ElementOwner));
		if (RequestId.IsValidId())
		{
			PendingOctreeUpdates[RequestId].bInvalidRequest = true;
		}
	}
}

void UNavigationSystem::RemoveNavOctreeElementId(const FOctreeElementId& ElementId, int32 UpdateFlags)
{
	if (NavOctree->IsValidElementId(ElementId))
	{
		// mark area occupied by given actor as dirty
		FNavigationOctreeElement& ElementData = NavOctree->GetElementById(ElementId);
		const int32 DirtyFlag = GetDirtyFlagHelper(UpdateFlags, ElementData.Data.GetDirtyFlag());
		AddDirtyArea(ElementData.Bounds.GetBox(), DirtyFlag);
		NavOctree->RemoveNode(ElementId);
	}
}

void UNavigationSystem::UpdateNavOctree(AActor* Actor)
{
	SCOPE_CYCLE_COUNTER(STAT_DebugNavOctree);

	INavRelevantInterface* NavElement = Cast<INavRelevantInterface>(Actor);
	if (NavElement)
	{
		UNavigationSystem* NavSys = Actor ? UNavigationSystem::GetCurrent(Actor->GetWorld()) : NULL;
		if (NavSys)
		{
			NavSys->UpdateNavOctreeElement(Actor, NavElement, OctreeUpdate_Modifiers);
		}
	}
}

void UNavigationSystem::UpdateNavOctree(UActorComponent* Comp)
{
	SCOPE_CYCLE_COUNTER(STAT_DebugNavOctree);

	// special case for early out: use cached nav relevancy
	UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp);
	if (PrimComp && !PrimComp->bNavigationRelevant)
	{
		return;
	}

	INavRelevantInterface* NavElement = Cast<INavRelevantInterface>(Comp);
	if (NavElement)
	{
		AActor* OwnerActor = Comp ? Comp->GetOwner() : NULL;
		if (OwnerActor)
		{
			UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(OwnerActor->GetWorld());
			if (NavSys)
			{
				if (OwnerActor->IsComponentRelevantForNavigation(Comp))
				{
					NavSys->UpdateNavOctreeElement(Comp, NavElement, OctreeUpdate_Default);
				}
				else
				{
					NavSys->UnregisterNavOctreeElement(Comp, NavElement, OctreeUpdate_Default);
				}
			}
		}
	}
}

void UNavigationSystem::UpdateNavOctreeAll(AActor* Actor)
{
	if (Actor)
	{
		UpdateNavOctree(Actor);

		TArray<UActorComponent*> Components;
		Actor->GetComponents(Components);

		for (int32 Idx = 0; Idx < Components.Num(); Idx++)
		{
			UpdateNavOctree(Components[Idx]);
		}
	}
}

void UNavigationSystem::UpdateNavOctreeBounds(AActor* Actor)
{
	TArray<UActorComponent*> Components;
	Actor->GetComponents(Components);

	for (int32 Idx = 0; Idx < Components.Num(); Idx++)
	{
		INavRelevantInterface* NavElement = Cast<INavRelevantInterface>(Components[Idx]);
		if (NavElement)
		{
			NavElement->UpdateNavigationBounds();
		}
	}
}

void UNavigationSystem::ClearNavOctreeAll(AActor* Actor)
{
	if (Actor)
	{
		OnActorUnregistered(Actor);

		TArray<UActorComponent*> Components;
		Actor->GetComponents(Components);

		for (int32 Idx = 0; Idx < Components.Num(); Idx++)
		{
			OnComponentUnregistered(Components[Idx]);
		}
	}
}

void UNavigationSystem::UpdateNavOctreeElement(UObject* ElementOwner, INavRelevantInterface* ElementInterface, int32 UpdateFlags)
{
	INC_DWORD_STAT(STAT_Navigation_UpdateNavOctree);

	// grab existing octree data
	FBox CurrentBounds;
	int32 CurrentFlags;
	const bool bAlreadyExists = GetNavOctreeElementData(ElementOwner, CurrentFlags, CurrentBounds);

	// don't invalidate pending requests
	UpdateFlags |= OctreeUpdate_Refresh;

	// always try to unregister, even if element owner doesn't exists in octree (parent nodes)
	UnregisterNavOctreeElement(ElementOwner, ElementInterface, UpdateFlags);

	const FSetElementId RequestId = RegisterNavOctreeElement(ElementOwner, ElementInterface, UpdateFlags);

	// add original data to pending registration request
	// so it could be dirtied properly when system receive unregister request while actor is still queued
	if (RequestId.IsValidId())
	{
		FNavigationDirtyElement& UpdateInfo = PendingOctreeUpdates[RequestId];
		UpdateInfo.PrevFlags = CurrentFlags;
		UpdateInfo.PrevBounds = CurrentBounds;
		UpdateInfo.bHasPrevData = bAlreadyExists;
	}
}

void UNavigationSystem::UpdateNavOctreeParentChain(UObject* ElementOwner)
{
	INavRelevantInterface* ElementInterface = Cast<INavRelevantInterface>(ElementOwner);
	const int32 UpdateFlags = OctreeUpdate_ParentChain | OctreeUpdate_Refresh;

	TArray<FWeakObjectPtr> ChildNodes;
	OctreeChildNodesMap.MultiFind(ElementOwner, ChildNodes);

	if (ChildNodes.Num() == 0)
	{
		UpdateNavOctreeElement(ElementOwner, ElementInterface, UpdateFlags);
		return;
	}

	TArray<INavRelevantInterface*> ChildNavInterfaces;
	ChildNavInterfaces.AddZeroed(ChildNodes.Num());
	
	for (int32 Idx = 0; Idx < ChildNodes.Num(); Idx++)
	{
		if (ChildNodes[Idx].IsValid())
		{
			UObject* ChildNodeOb = ChildNodes[Idx].Get();
			ChildNavInterfaces[Idx] = Cast<INavRelevantInterface>(ChildNodeOb);
			UnregisterNavOctreeElement(ChildNodeOb, ChildNavInterfaces[Idx], UpdateFlags);
		}
	}

	UnregisterNavOctreeElement(ElementOwner, ElementInterface, UpdateFlags);
	RegisterNavOctreeElement(ElementOwner, ElementInterface, UpdateFlags);

	for (int32 Idx = 0; Idx < ChildNodes.Num(); Idx++)
	{
		if (ChildNodes[Idx].IsValid())
		{
			RegisterNavOctreeElement(ChildNodes[Idx].Get(), ChildNavInterfaces[Idx], UpdateFlags);
		}
	}
}

bool UNavigationSystem::UpdateNavOctreeElementBounds(UActorComponent* Comp, const FBox& NewBounds, const FBox& DirtyArea)
{
	const FOctreeElementId* ElementId = GetObjectsNavOctreeId(Comp);
	if (ElementId && ElementId->IsValidId())
	{
		NavOctree->UpdateNode(*ElementId, NewBounds);
		
		// Add dirty area
		if (DirtyArea.IsValid)
		{
			ElementId = GetObjectsNavOctreeId(Comp);
			if (ElementId && ElementId->IsValidId())
			{
				FNavigationOctreeElement& ElementData = NavOctree->GetElementById(*ElementId);
				AddDirtyArea(DirtyArea, ElementData.Data.GetDirtyFlag());
			}
		}

		return true;
	}
	
	return false;
}

void UNavigationSystem::OnComponentRegistered(UActorComponent* Comp)
{
	SCOPE_CYCLE_COUNTER(STAT_DebugNavOctree);
	INavRelevantInterface* NavInterface = Cast<INavRelevantInterface>(Comp);
	if (NavInterface)
	{
		AActor* OwnerActor = Comp ? Comp->GetOwner() : NULL;
		if (OwnerActor && OwnerActor->IsComponentRelevantForNavigation(Comp))
		{
			UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(OwnerActor->GetWorld());
			if (NavSys)
			{
				NavSys->RegisterNavOctreeElement(Comp, NavInterface, OctreeUpdate_Default);
			}
		}
	}
}

void UNavigationSystem::OnComponentUnregistered(UActorComponent* Comp)
{
	SCOPE_CYCLE_COUNTER(STAT_DebugNavOctree);
	INavRelevantInterface* NavInterface = Cast<INavRelevantInterface>(Comp);
	if (NavInterface)
	{
		AActor* OwnerActor = Comp ? Comp->GetOwner() : NULL;
		if (OwnerActor)
		{
			// skip IsComponentRelevantForNavigation check, it's only for adding new stuff

			UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(OwnerActor->GetWorld());
			if (NavSys)
			{
				NavSys->UnregisterNavOctreeElement(Comp, NavInterface, OctreeUpdate_Default);
			}
		}
	}
}

void UNavigationSystem::OnActorRegistered(AActor* Actor)
{
	SCOPE_CYCLE_COUNTER(STAT_DebugNavOctree);
	INavRelevantInterface* NavInterface = Cast<INavRelevantInterface>(Actor);
	if (NavInterface)
	{
		UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(Actor->GetWorld());
		if (NavSys)
		{
			NavSys->RegisterNavOctreeElement(Actor, NavInterface, OctreeUpdate_Modifiers);
		}
	}
}

void UNavigationSystem::OnActorUnregistered(AActor* Actor)
{
	SCOPE_CYCLE_COUNTER(STAT_DebugNavOctree);
	INavRelevantInterface* NavInterface = Cast<INavRelevantInterface>(Actor);
	if (NavInterface)
	{
		UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(Actor->GetWorld());
		if (NavSys)
		{
			NavSys->UnregisterNavOctreeElement(Actor, NavInterface, OctreeUpdate_Modifiers);
		}
	}
}

void UNavigationSystem::PopulateNavOctree()
{
	UWorld* World = GetWorld();

	check(World && NavOctree);

	// now process all actors on all levels
	for (int32 LevelIndex = 0; LevelIndex < World->GetNumLevels(); ++LevelIndex) 
	{
		ULevel* Level = World->GetLevel(LevelIndex);
		AddLevelCollisionToOctree(Level);

		for (int32 ActorIndex=0; ActorIndex<Level->Actors.Num(); ActorIndex++)
		{
			AActor* Actor = Level->Actors[ActorIndex];

			const bool bLegalActor = Actor && !Actor->IsPendingKill();
			if (bLegalActor)
			{
				UpdateNavOctreeAll(Actor);
			}
		}
	}
}

void UNavigationSystem::FindElementsInNavOctree(const FBox& QueryBox, const FNavigationOctreeFilter& Filter, TArray<FNavigationOctreeElement>& Elements)
{
	for (FNavigationOctree::TConstElementBoxIterator<> It(*NavOctree, QueryBox); It.HasPendingElements(); It.Advance())
	{
		const FNavigationOctreeElement& Element = It.GetCurrentElement();
		if (Element.IsMatchingFilter(Filter))
		{
			Elements.Add(Element);
		}		
	}
}

void UNavigationSystem::ReleaseInitialBuildingLock()
{
	if (bInitialBuildingLocked == false)
	{
		return;
	}

	if (bInitialBuildingLockActive == true)
	{
		bInitialBuildingLockActive = false;
		if (bNavigationBuildingLocked == false)
		{
			// apply pending changes
			{
				SCOPE_CYCLE_COUNTER(STAT_Navigation_AddingActorsToNavOctree);

				SCOPE_CYCLE_COUNTER(STAT_Navigation_BuildTime)
				STAT(double ThisTime = 0);
				{
					SCOPE_SECONDS_COUNTER(ThisTime);
					for (TSet<FNavigationDirtyElement>::TIterator It(PendingOctreeUpdates); It; ++It)
					{
						AddElementToNavOctree(*It);
					}
				}
				INC_FLOAT_STAT_BY(STAT_Navigation_CumulativeBuildTime,(float)ThisTime*1000);
			}

			PendingOctreeUpdates.Empty(32);
			// clear dirty areas - forced navigation unlocking is supposed to rebuild the whole navigation
			DirtyAreas.Reset();

			// if navigation building is not blocked for other reasons then rebuild
			// bForce == true to skip bNavigationBuildingLocked test
			NavigationBuildingUnlock(/*bForce = */true);
		}
	}
}

void UNavigationSystem::InitializeLevelCollisions()
{
	UWorld* World = GetWorld();
	if (!bInitialLevelsAdded && UNavigationSystem::GetCurrent(World) == this)
	{
		// Process all visible levels
		const auto& Levels = World->GetLevels();
		for (ULevel* Level : Levels)
		{
			if (Level->bIsVisible)
			{
				AddLevelCollisionToOctree(Level);
			}
		}

		bInitialLevelsAdded = true;
	}
}

#if WITH_EDITOR
void UNavigationSystem::UpdateLevelCollision(ULevel* InLevel)
{
	if (InLevel != NULL)
	{
		UWorld* World = GetWorld();
		OnLevelRemovedFromWorld(InLevel, World);
		OnLevelAddedToWorld(InLevel, World);
	}
}

void UNavigationSystem::OnEditorModeChanged(FEdMode* Mode, bool IsEntering)
{
	if (Mode == NULL)
	{
		return;
	}

	if (IsEntering == false && Mode->GetID() == FBuiltinEditorModes::EM_Geometry)
	{
		// check if any of modified brushes belongs to an ANavMeshBoundsVolume
		FEdModeGeometry* GeometryMode = (FEdModeGeometry*)Mode;
		for (auto GeomObjectIt = GeometryMode->GeomObjectItor(); GeomObjectIt; GeomObjectIt++)
		{
			ANavMeshBoundsVolume* Volume = Cast<ANavMeshBoundsVolume>((*GeomObjectIt)->GetActualBrush());
			if (Volume)
			{
				OnNavigationBoundsUpdated(Volume);
			}
		}
	}
}
#endif

void UNavigationSystem::OnNavigationBoundsUpdated(ANavMeshBoundsVolume* NavVolume)
{
	if (NavVolume == NULL)
	{
		return;
	}

	FNavigationBoundsUpdateRequest UpdateRequest;
	UpdateRequest.NavBounds.UniqueID	= NavVolume->GetUniqueID();
	UpdateRequest.NavBounds.AreaBox		= NavVolume->GetComponentsBoundingBox(true);
	UpdateRequest.NavBounds.PackageName	= NavVolume->GetOutermost()->GetFName();
	UpdateRequest.UpdateRequest			= FNavigationBoundsUpdateRequest::Updated;
	AddNavigationBoundsUpdateRequest(UpdateRequest);
}

void UNavigationSystem::OnNavigationBoundsAdded(ANavMeshBoundsVolume* NavVolume)
{
	if (NavVolume == NULL)
	{
		return;
	}

	FNavigationBoundsUpdateRequest UpdateRequest;
	UpdateRequest.NavBounds.UniqueID	= NavVolume->GetUniqueID();
	UpdateRequest.NavBounds.AreaBox		= NavVolume->GetComponentsBoundingBox(true);
	UpdateRequest.NavBounds.PackageName	= NavVolume->GetOutermost()->GetFName();
	UpdateRequest.UpdateRequest	= FNavigationBoundsUpdateRequest::Added;
	AddNavigationBoundsUpdateRequest(UpdateRequest);
}

void UNavigationSystem::OnNavigationBoundsRemoved(ANavMeshBoundsVolume* NavVolume)
{
	if (NavVolume == NULL)
	{
		return;
	}
	
	FNavigationBoundsUpdateRequest UpdateRequest;
	UpdateRequest.NavBounds.UniqueID	= NavVolume->GetUniqueID();
	UpdateRequest.NavBounds.AreaBox		= NavVolume->GetComponentsBoundingBox(true);
	UpdateRequest.NavBounds.PackageName	= NavVolume->GetOutermost()->GetFName();
	UpdateRequest.UpdateRequest	= FNavigationBoundsUpdateRequest::Removed;
	AddNavigationBoundsUpdateRequest(UpdateRequest);
}

void UNavigationSystem::AddNavigationBoundsUpdateRequest(const FNavigationBoundsUpdateRequest& UpdateRequest)
{
	int32 ExistingIdx = PendingNavBoundsUpdates.IndexOfByPredicate([&](const FNavigationBoundsUpdateRequest& Element) {
		return UpdateRequest.NavBounds.UniqueID == Element.NavBounds.UniqueID;
	});

	if (ExistingIdx != INDEX_NONE)
	{
		// Overwrite any previous updates
		PendingNavBoundsUpdates[ExistingIdx] = UpdateRequest;
	}
	else
	{
		PendingNavBoundsUpdates.Add(UpdateRequest);
	}
}

void UNavigationSystem::PerformNavigationBoundsUpdate(const TArray<FNavigationBoundsUpdateRequest>& UpdateRequests)
{
	if (bNavDataRemovedDueToMissingNavBounds)
	{
		PopulateNavOctree();
		bNavDataRemovedDueToMissingNavBounds = false;
	}
	
	if (NavDataSet.Num() == 0)
	{
		if (NavDataRegistrationQueue.Num() > 0)
		{
			ProcessRegistrationCandidates();
		}

		if (NavDataSet.Num() == 0)
		{
			SpawnMissingNavigationData();
			ProcessRegistrationCandidates();
		}
	}
	
	// Create list of areas that needs to be updated
	TArray<FBox> UpdatedAreas;
	for (const FNavigationBoundsUpdateRequest& Request : UpdateRequests)
	{
		FSetElementId ExistingElementId = RegisteredNavBounds.FindId(Request.NavBounds);

		switch (Request.UpdateRequest)
		{
		case FNavigationBoundsUpdateRequest::Removed:
			{
				if (ExistingElementId.IsValidId())
				{
					UpdatedAreas.Add(RegisteredNavBounds[ExistingElementId].AreaBox);
					RegisteredNavBounds.Remove(ExistingElementId);
				}
			}
			break;

		case FNavigationBoundsUpdateRequest::Added:
		case FNavigationBoundsUpdateRequest::Updated:
			{
				if (ExistingElementId.IsValidId())
				{
					FBox ExistingBox = RegisteredNavBounds[ExistingElementId].AreaBox;

					if (!(ExistingBox == Request.NavBounds.AreaBox))
					{
						UpdatedAreas.Add(ExistingBox);
						RegisteredNavBounds[ExistingElementId] = Request.NavBounds;
					}
				}
				else
				{
					ExistingElementId = RegisteredNavBounds.Add(Request.NavBounds);
				}
				
				UpdatedAreas.Add(Request.NavBounds.AreaBox);
			}

			break;
		}
	}

#if WITH_RECAST
	if (!IsNavigationBuildingLocked())
	{
		if (UpdatedAreas.Num())
		{
			for (ANavigationData* NavData : NavDataSet)
			{
				if (NavData)
				{
					NavData->OnNavigationBoundsChanged();	
				}
			}
		}
				
		// Propagate to generators areas that needs to be updated
		AddDirtyAreas(UpdatedAreas, ENavigationDirtyFlag::All | ENavigationDirtyFlag::NavigationBounds);
	}
#endif // WITH_RECAST
}

void UNavigationSystem::GatherNavigationBounds()
{
	// Gather all available navigation bounds
	RegisteredNavBounds.Empty();
	for (TActorIterator<ANavMeshBoundsVolume> It(GetWorld()); It; ++It)
	{
		ANavMeshBoundsVolume* V = (*It);
		if (V != nullptr && !V->IsPendingKill())
		{
			FNavigationBounds NavBounds;
			NavBounds.UniqueID		= V->GetUniqueID();
			NavBounds.AreaBox		= V->GetComponentsBoundingBox(true);
			NavBounds.PackageName	= V->GetOutermost()->GetFName();
			RegisteredNavBounds.Add(NavBounds);
		}
	}
}

void UNavigationSystem::Build()
{
	if (IsThereAnywhereToBuildNavigation() == false)
	{
		return;
	}

	const double BuildStartTime = FPlatformTime::Seconds();

	SpawnMissingNavigationData();

	if (NavDataClasses.Num() == 0)
	{
		return;
	}

	// make sure freshly created navigation instances are registered before we try to build them
	ProcessRegistrationCandidates();

	// and now iterate through all registered and just start building them
	RebuildAll();

	// Block until build is finished
	for (ANavigationData* NavData : NavDataSet)
	{
		if (NavData)
		{
			NavData->EnsureBuildCompletion();
		}
	}

	UE_LOG(LogNavigation, Display, TEXT("UNavigationSystem::Build total execution time: %.5f"), float(FPlatformTime::Seconds() - BuildStartTime));
}

void UNavigationSystem::SpawnMissingNavigationData()
{
	DoInitialSetup();

	const int32 SupportedAgentsCount = SupportedAgents.Num();
	check(SupportedAgentsCount >= 0);
	
	// Bit array might be a bit of an overkill here, but this function will be called very rarely
	TBitArray<> AlreadyInstantiated(false, SupportedAgentsCount);
	uint8 NumberFound = 0;
	UWorld* NavWorld = GetWorld();

	// 1. check whether any of required navigation data has already been instantiated
	for (TActorIterator<ANavigationData> It(NavWorld); It && NumberFound < SupportedAgentsCount; ++It)
	{
		ANavigationData* Nav = (*It);
		if (Nav != NULL && Nav->GetTypedOuter<UWorld>() == NavWorld && Nav->IsPendingKill() == false)
		{
			// find out which one it is
			for (int32 AgentIndex = 0; AgentIndex < SupportedAgentsCount; ++AgentIndex)
			{
				if (AlreadyInstantiated[AgentIndex] == true)
				{
					// already present, skip
					continue;
				}

				if (Nav->GetClass() == SupportedAgents[AgentIndex].NavigationDataClass && Nav->DoesSupportAgent(SupportedAgents[AgentIndex]) == true)
				{
					AlreadyInstantiated[AgentIndex] = true;
					++NumberFound;
					break;
				}
			}				
		}
	}

	// 2. for any not already instantiated navigation data call creator functions
	if (NumberFound < SupportedAgentsCount)
	{
		for (int32 AgentIndex = 0; AgentIndex < SupportedAgentsCount; ++AgentIndex)
		{
			if (AlreadyInstantiated[AgentIndex] == false && SupportedAgents[AgentIndex].NavigationDataClass != nullptr)
			{
				bool bHandled = false;

				ANavigationData* Instance = CreateNavigationDataInstance(SupportedAgents[AgentIndex]);

				if (Instance != NULL)
				{
					RequestRegistration(Instance);
				}
				else 
				{
					UE_LOG(LogNavigation, Warning, TEXT("Was not able to create navigation data for SupportedAgent %s (index %d)")
						, *(SupportedAgents[AgentIndex].Name.ToString()), AgentIndex);
				}
			}
		}

		ProcessRegistrationCandidates();
	}
	
	if (MainNavData == NULL || MainNavData->IsPendingKill())
	{
		// update 
		MainNavData = GetMainNavData(FNavigationSystem::DontCreate);
	}
}

ANavigationData* UNavigationSystem::CreateNavigationDataInstance(const FNavDataConfig& NavConfig)
{
	TSubclassOf<ANavigationData> NavDataClass = NavConfig.NavigationDataClass;
	UWorld* World = GetWorld();
	check(World);

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.OverrideLevel = World->PersistentLevel;
	ANavigationData* Instance = World->SpawnActor<ANavigationData>(*NavDataClass, SpawnInfo);

	if (Instance != NULL)
	{
		Instance->SetConfig(NavConfig);
		if (NavConfig.Name != NAME_None)
		{
			FString StrName = FString::Printf(TEXT("%s-%s"), *(Instance->GetFName().GetPlainNameString()), *(NavConfig.Name.ToString()));
			// temporary solution to make sure we don't try to change name while there's already
			// an object with this name
			UObject* ExistingObject = StaticFindObject(/*Class=*/ NULL, Instance->GetOuter(), *StrName, true);
			if (ExistingObject != NULL)
			{
				ExistingObject->Rename(NULL, NULL, REN_DontCreateRedirectors | REN_ForceGlobalUnique | REN_DoNotDirty | REN_NonTransactional);
			}

			// Set descriptive name
			Instance->Rename(*StrName);
#if WITH_EDITOR
			Instance->SetActorLabel(StrName);
#endif // WITH_EDITOR
		}
	}

	return Instance;
}

void UNavigationSystem::OnPIEStart()
{
	// Do not tick async build for editor world while PIE is active
	bAsyncBuildPaused = true;
}

void UNavigationSystem::OnPIEEnd()
{
	bAsyncBuildPaused = false;
}

void UNavigationSystem::EnableAllGenerators(bool bEnable, bool bForce)
{
	if (bEnable)
	{
		NavigationBuildingUnlock(bForce);
	}
	else
	{
		NavigationBuildingLock();
	}
}

void UNavigationSystem::NavigationBuildingLock()
{
	if (bNavigationBuildingLocked == true)
	{
		return;
	}

	GetMainNavData(bAutoCreateNavigationData && NavOctree != nullptr && IsThereAnywhereToBuildNavigation() ? FNavigationSystem::Create : FNavigationSystem::DontCreate);

	bNavigationBuildingLocked = true;
}

void UNavigationSystem::NavigationBuildingUnlock(bool bForce)
{
	if ((bNavigationBuildingLocked == true && bInitialBuildingLockActive == false) || bForce == true)
	{
		bNavigationBuildingLocked = false;
		bInitialBuildingLockActive = false;
		
		if (bNavigationAutoUpdateEnabled)
		{
			RebuildAll();
		}
	}
	else if (bInitialBuildingLockActive == true)
	{
		// remember that other reasons to lock building are no longer there
		// so we can release building lock as soon as bInitialBuildingLockActive 
		// turns true
		bNavigationBuildingLocked = false;
	}
}

void UNavigationSystem::RebuildAll()
{
	const bool bIsInGame = GetWorld()->IsGameWorld();
	
	GatherNavigationBounds();

	// make sure that octree is up to date
	for (TSet<FNavigationDirtyElement>::TIterator It(PendingOctreeUpdates); It; ++It)
	{
		AddElementToNavOctree(*It);
	}
	PendingOctreeUpdates.Empty(32);

	// discard all pending dirty areas, we are going to rebuild navmesh anyway 
	DirtyAreas.Reset();
	PendingNavBoundsUpdates.Reset();
	
	// 
	for (int32 NavDataIndex = 0; NavDataIndex < NavDataSet.Num(); ++NavDataIndex)
	{
		ANavigationData* NavData = NavDataSet[NavDataIndex];

		if (NavData && (NavData->bRebuildAtRuntime || (GIsEditor && !bIsInGame)))
		{
			NavData->RebuildAll();
		}
	}
}

bool UNavigationSystem::IsNavigationBuildInProgress(bool bCheckDirtyToo)
{
	bool bRet = false;

	if (NavDataSet.Num() == 0)
	{
		// update nav data. If none found this is the place to create one
		GetMainNavData(FNavigationSystem::DontCreate);
	}
	
	for (int32 NavDataIndex = 0; NavDataIndex < NavDataSet.Num(); ++NavDataIndex)
	{
		ANavigationData* NavData = NavDataSet[NavDataIndex];
		if (NavData != NULL && NavData->GetGenerator() != NULL 
			&& NavData->GetGenerator()->IsBuildInProgress(bCheckDirtyToo) == true)
		{
			bRet = true;
			break;
		}
	}

	return bRet;
}

void UNavigationSystem::OnNavigationGenerationFinished(ANavigationData& NavData)
{
	OnNavigationGenerationFinishedDelegate.Broadcast(&NavData);
}

int32 UNavigationSystem::GetNumRemainingBuildTasks() const
{
	int32 NumTasks = 0;
	
	for (ANavigationData* NavData : NavDataSet)
	{
		if (NavData && NavData->GetGenerator())
		{
			NumTasks+= NavData->GetGenerator()->GetNumRemaningBuildTasks();
		}
	}
	
	return NumTasks;
}

int32 UNavigationSystem::GetNumRunningBuildTasks() const 
{
	int32 NumTasks = 0;
	
	for (ANavigationData* NavData : NavDataSet)
	{
		if (NavData && NavData->GetGenerator())
		{
			NumTasks+= NavData->GetGenerator()->GetNumRunningBuildTasks();
		}
	}
	
	return NumTasks;
}

void UNavigationSystem::OnLevelAddedToWorld(ULevel* InLevel, UWorld* InWorld)
{
	if (InWorld == GetWorld())
	{
		AddLevelCollisionToOctree(InLevel);

		if (!InLevel->IsPersistentLevel())
		{
			for (ANavigationData* NavData : NavDataSet)
			{
				NavData->OnStreamingLevelAdded(InLevel);
			}
		}
	}
}

void UNavigationSystem::OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld)
{
	if (InWorld == GetWorld())
	{
		RemoveLevelCollisionFromOctree(InLevel);

		if (InLevel && !InLevel->IsPersistentLevel())
		{
			for (ANavigationData* NavData : NavDataSet)
			{
				NavData->OnStreamingLevelRemoved(InLevel);
			}
		}
	}
}

void UNavigationSystem::AddLevelCollisionToOctree(ULevel* Level)
{
#if WITH_RECAST
	if (Level && NavOctree)
	{
		const TArray<FVector>* LevelGeom = Level->GetStaticNavigableGeometry();
		const FOctreeElementId* ElementId = GetObjectsNavOctreeId(Level);

		if (LevelGeom && !ElementId)
		{
			FNavigationOctreeElement BSPElem;
			FRecastNavMeshGenerator::ExportVertexSoupGeometry(*LevelGeom, BSPElem.Data);

			const auto& Bounds = BSPElem.Data.Bounds;
			if (!Bounds.GetExtent().IsNearlyZero())
			{
				NavOctree->AddNode(Level, NULL, Bounds, BSPElem);
				AddDirtyArea(Bounds, ENavigationDirtyFlag::All);

				UE_LOG(LogNavOctree, Log, TEXT("ADD %s"), *GetNameSafe(Level));
			}
		}
	}
#endif// WITH_RECAST
}

void UNavigationSystem::RemoveLevelCollisionFromOctree(ULevel* Level)
{
	const FOctreeElementId* ElementId = GetObjectsNavOctreeId(Level);
	UE_LOG(LogNavOctree, Log, TEXT("UNREG %s %s"), *GetNameSafe(Level), ElementId ? TEXT("[exists]") : TEXT(""));

	if (ElementId != NULL)
	{
		if (NavOctree->IsValidElementId(*ElementId))
		{
			// mark area occupied by given actor as dirty
			FNavigationOctreeElement& ElementData = NavOctree->GetElementById(*ElementId);
			AddDirtyArea(ElementData.Bounds.GetBox(), ENavigationDirtyFlag::All);
		}

		NavOctree->RemoveNode(*ElementId);
		RemoveObjectsNavOctreeId(Level);
	}
}

void UNavigationSystem::OnPostLoadMap()
{
	UE_LOG(LogNavigation, Log, TEXT("UNavigationSystem::OnPostLoadMap"));

	// if map has been loaded and there are some navigation bounds volumes 
	// then create appropriate navigation structured
	ANavigationData* NavData = GetMainNavData(FNavigationSystem::DontCreate);

	// Do this if there's currently no navigation
	if (NavData == NULL && bAutoCreateNavigationData == true && IsThereAnywhereToBuildNavigation() == true)
	{
		NavData = GetMainNavData(FNavigationSystem::Create);
	}
}

#if WITH_EDITOR
void UNavigationSystem::OnActorMoved(AActor* Actor)
{
	if (Cast<ANavMeshBoundsVolume>(Actor) != NULL)
	{
		OnNavigationBoundsUpdated((ANavMeshBoundsVolume*)Actor);
	}
}
#endif // WITH_EDITOR

void UNavigationSystem::OnNavigationDirtied(const FBox& Bounds)
{
	AddDirtyArea(Bounds, ENavigationDirtyFlag::All);
}

void UNavigationSystem::CleanUp(ECleanupMode Mode)
{
	UE_LOG(LogNavigation, Log, TEXT("UNavigationSystem::CleanUp"));

#if WITH_EDITOR
	if (GIsEditor && GEngine)
	{
		GEngine->OnActorMoved().RemoveAll(this);
	}
#endif // WITH_EDITOR

	FCoreUObjectDelegates::PostLoadMap.RemoveAll(this);
	UNavigationSystem::NavigationDirtyEvent.RemoveAll(this);
	FWorldDelegates::LevelAddedToWorld.RemoveAll(this);
	FWorldDelegates::LevelRemovedFromWorld.RemoveAll(this);

	if (NavOctree != NULL)
	{
		NavOctree->Destroy();
		delete NavOctree;
		NavOctree = NULL;
	}

	ObjectToOctreeId.Empty();

	for (int32 NavDataIndex = 0; NavDataIndex < NavDataSet.Num(); ++NavDataIndex)
	{
		if (NavDataSet[NavDataIndex] != NULL)
		{
			NavDataSet[NavDataIndex]->CleanUp();
		}
	}	

	SetCrowdManager(NULL);

	NavDataSet.Reset();

	// reset unique link Id for new map
	const UWorld* MyWorld = (Mode == ECleanupMode::CleanupWithWorld) ? GetWorld() : NULL;
	if (MyWorld && (MyWorld->WorldType == EWorldType::Game || MyWorld->WorldType == EWorldType::Editor))
	{
		INavLinkCustomInterface::NextUniqueId = 1;
	}
}

//----------------------------------------------------------------------//
// Blueprint functions
//----------------------------------------------------------------------//
UNavigationSystem* UNavigationSystem::GetNavigationSystem(UObject* WorldContext)
{
	return GetCurrent(WorldContext);
}

FVector UNavigationSystem::ProjectPointToNavigation(UObject* WorldContextObject, const FVector& Point, ANavigationData* NavData, TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	FNavLocation ProjectedPoint(Point);

	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(World);
	if (NavSys)
	{
		ANavigationData* UseNavData = NavData ? NavData : NavSys->GetMainNavData(FNavigationSystem::DontCreate);
		NavSys->ProjectPointToNavigation(Point, ProjectedPoint, INVALID_NAVEXTENT, UseNavData, UNavigationQueryFilter::GetQueryFilter(UseNavData, FilterClass));
	}

	return ProjectedPoint.Location;
}

FVector UNavigationSystem::GetRandomPoint(UObject* WorldContextObject, ANavigationData* NavData, TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	FNavLocation RandomPoint;

	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(World);
	if (NavSys)
	{
		ANavigationData* UseNavData = NavData ? NavData : NavSys->GetMainNavData(FNavigationSystem::DontCreate);
		NavSys->GetRandomPoint(RandomPoint, UseNavData, UNavigationQueryFilter::GetQueryFilter(UseNavData, FilterClass));
	}

	return RandomPoint.Location;
}


FVector UNavigationSystem::GetRandomPointInRadius(UObject* WorldContextObject, const FVector& Origin, float Radius, ANavigationData* NavData, TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	FNavLocation RandomPoint;

	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(World);
	if (NavSys)
	{
		ANavigationData* UseNavData = NavData ? NavData : NavSys->GetMainNavData(FNavigationSystem::DontCreate);
		NavSys->GetRandomPointInRadius(Origin, Radius, RandomPoint, UseNavData, UNavigationQueryFilter::GetQueryFilter(UseNavData, FilterClass));
	}

	return RandomPoint.Location;
}

ENavigationQueryResult::Type UNavigationSystem::GetPathCost(UObject* WorldContextObject, const FVector& PathStart, const FVector& PathEnd, float& OutPathCost, ANavigationData* NavData, TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(World);
	if (NavSys)
	{
		ANavigationData* UseNavData = NavData ? NavData : NavSys->GetMainNavData(FNavigationSystem::DontCreate);
		return NavSys->GetPathCost(PathStart, PathEnd, OutPathCost, UseNavData, UNavigationQueryFilter::GetQueryFilter(UseNavData, FilterClass));
	}

	return ENavigationQueryResult::Error;
}

ENavigationQueryResult::Type UNavigationSystem::GetPathLength(UObject* WorldContextObject, const FVector& PathStart, const FVector& PathEnd, float& OutPathLength, ANavigationData* NavData, TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	float PathLength = 0.f;

	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(World);
	if (NavSys)
	{
		ANavigationData* UseNavData = NavData ? NavData : NavSys->GetMainNavData(FNavigationSystem::DontCreate);
		return NavSys->GetPathLength(PathStart, PathEnd, OutPathLength, UseNavData, UNavigationQueryFilter::GetQueryFilter(UseNavData, FilterClass));
	}

	return ENavigationQueryResult::Error;
}

bool UNavigationSystem::IsNavigationBeingBuilt(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject); 
	if (World != NULL && World->GetNavigationSystem() != NULL)
	{
		return World->GetNavigationSystem()->IsNavigationBuildInProgress();
	}

	return false;
}

//----------------------------------------------------------------------//
// HACKS!!!
//----------------------------------------------------------------------//
bool UNavigationSystem::ShouldGeneratorRun(const FNavDataGenerator* Generator) const
{
	if (Generator != NULL)
	{
		for (int32 NavDataIndex = 0; NavDataIndex < NavDataSet.Num(); ++NavDataIndex)
		{
			ANavigationData* NavData = NavDataSet[NavDataIndex];
			if (NavData != NULL && NavData->GetGenerator() == Generator)
			{
				return true;
			}
		}
	}

	return false;
}

bool UNavigationSystem::HandleCycleNavDrawnCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	CycleNavigationDataDrawn();

	return true;
}

bool UNavigationSystem::HandleCountNavMemCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	for (int32 NavDataIndex = 0; NavDataIndex < NavDataSet.Num(); ++NavDataIndex)
	{
		ANavigationData* NavData = NavDataSet[NavDataIndex];
		if (NavData != NULL)
		{
			NavData->LogMemUsed();
		}
	}
	return true;
}

//----------------------------------------------------------------------//
// Commands
//----------------------------------------------------------------------//
bool FNavigationSystemExec::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	if (InWorld == NULL || InWorld->GetNavigationSystem() == NULL)
	{
		return false;
	}

	UNavigationSystem*  NavSys = InWorld->GetNavigationSystem();

	if (NavSys->NavDataSet.Num() > 0)
	{
		if (FParse::Command(&Cmd, TEXT("CYCLENAVDRAWN")))
		{
			NavSys->HandleCycleNavDrawnCommand( Cmd, Ar );
			// not returning true to enable all navigation systems to cycle their own data
			return false;
		}
		else if (FParse::Command(&Cmd, TEXT("CountNavMem")))
		{
			NavSys->HandleCountNavMemCommand( Cmd, Ar );
			return false;
		}
	}

	return false;
}

void UNavigationSystem::CycleNavigationDataDrawn()
{
	++CurrentlyDrawnNavDataIndex;
	if (CurrentlyDrawnNavDataIndex >= NavDataSet.Num())
	{
		CurrentlyDrawnNavDataIndex = INDEX_NONE;
	}

	for (int32 NavDataIndex = 0; NavDataIndex < NavDataSet.Num(); ++NavDataIndex)
	{
		ANavigationData* NavData = NavDataSet[NavDataIndex];
		if (NavData != NULL)
		{
			const bool bNewEnabledDrawing = (CurrentlyDrawnNavDataIndex == INDEX_NONE) || (NavDataIndex == CurrentlyDrawnNavDataIndex);
			NavData->SetNavRenderingEnabled(bNewEnabledDrawing);
		}
	}
}

bool UNavigationSystem::IsNavigationDirty() const
{
	for (int32 NavDataIndex=0; NavDataIndex < NavDataSet.Num(); ++NavDataIndex)
	{
		if (NavDataSet[NavDataIndex] && NavDataSet[NavDataIndex]->NeedsRebuild())
		{
			return true;
		}
	}

	return false;
}

bool UNavigationSystem::CanRebuildDirtyNavigation() const
{
	for (int32 NavDataIndex = 0; NavDataIndex < NavDataSet.Num(); ++NavDataIndex)
	{
		const bool bIsDirty = NavDataSet[NavDataIndex]->NeedsRebuild();
		const bool bCanRebuild = NavDataSet[NavDataIndex]->SupportsRuntimeGeneration();

		if (bIsDirty && !bCanRebuild)
		{
			return false;
		}
	}

	return true;
}

bool UNavigationSystem::DoesPathIntersectBox(const FNavigationPath* Path, const FBox& Box, uint32 StartingIndex)
{
	return Path != NULL && Path->DoesIntersectBox(Box, StartingIndex);
}

bool UNavigationSystem::DoesPathIntersectBox(const FNavigationPath* Path, const FBox& Box, const FVector& AgentLocation, uint32 StartingIndex)
{
	return Path != NULL && Path->DoesIntersectBox(Box, AgentLocation, StartingIndex);
}
