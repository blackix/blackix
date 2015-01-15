// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "AI/Navigation/NavigationSystem.h"
#include "AI/Navigation/NavAgentInterface.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Pathfinding.h"

#define LOCTEXT_NAMESPACE "EnvQueryGenerator"

UEnvQueryTest_Pathfinding::UEnvQueryTest_Pathfinding(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Context = UEnvQueryContext_Querier::StaticClass();
	Cost = EEnvTestCost::High;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	TestMode = EEnvTestPathfinding::PathExist;
	PathFromContext.DefaultValue = true;
	SkipUnreachable.DefaultValue = true;
	UseHierarchicalPathfinding.DefaultValue = true;
	FloatValueMin.DefaultValue = 1000.0f;
	FloatValueMax.DefaultValue = 1000.0f;

	// keep deprecated properties initialized
	PathToItem.Value = true;
	DiscardUnreachable.Value = true;
	HierarchicalPathfinding.Value = true;
	FloatFilterMin.Value = 1000.0f;
	FloatFilterMax.Value = 1000.0f;
}

void UEnvQueryTest_Pathfinding::RunTest(FEnvQueryInstance& QueryInstance) const
{
	UObject* DataOwner = QueryInstance.Owner.Get();
	BoolValue.BindData(DataOwner, QueryInstance.QueryID);
	PathFromContext.BindData(DataOwner, QueryInstance.QueryID);
	SkipUnreachable.BindData(DataOwner, QueryInstance.QueryID);
	UseHierarchicalPathfinding.BindData(DataOwner, QueryInstance.QueryID);
	FloatValueMin.BindData(DataOwner, QueryInstance.QueryID);
	FloatValueMax.BindData(DataOwner, QueryInstance.QueryID);

	bool bWantsPath = BoolValue.GetValue();
	bool bPathToItem = PathFromContext.GetValue();
	bool bHierarchical = UseHierarchicalPathfinding.GetValue();
	bool bDiscardFailed = SkipUnreachable.GetValue();
	float MinThresholdValue = FloatValueMin.GetValue();
	float MaxThresholdValue = FloatValueMax.GetValue();

	UNavigationSystem* NavSys = QueryInstance.World->GetNavigationSystem();
	ANavigationData* NavData = FindNavigationData(NavSys, QueryInstance.Owner.Get());
	if (!NavData)
	{
		return;
	}

	TArray<FVector> ContextLocations;
	if (!QueryInstance.PrepareContext(Context, ContextLocations))
	{
		return;
	}

	EPathFindingMode::Type PFMode(bHierarchical ? EPathFindingMode::Hierarchical : EPathFindingMode::Regular);

	if (GetWorkOnFloatValues())
	{
		FFindPathSignature FindPathFunc;
		FindPathFunc.BindUObject(this, TestMode == EEnvTestPathfinding::PathLength ?
			(bPathToItem ? &UEnvQueryTest_Pathfinding::FindPathLengthTo : &UEnvQueryTest_Pathfinding::FindPathLengthFrom) :
			(bPathToItem ? &UEnvQueryTest_Pathfinding::FindPathCostTo : &UEnvQueryTest_Pathfinding::FindPathCostFrom) );

		NavData->BeginBatchQuery();
		for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
		{
			const FVector ItemLocation = GetItemLocation(QueryInstance, *It);
			for (int32 ContextIndex = 0; ContextIndex < ContextLocations.Num(); ContextIndex++)
			{
				const float PathValue = FindPathFunc.Execute(ItemLocation, ContextLocations[ContextIndex], PFMode, NavData, NavSys, QueryInstance.Owner.Get());
				It.SetScore(TestPurpose, FilterType, PathValue, MinThresholdValue, MaxThresholdValue);

				if (bDiscardFailed && PathValue >= BIG_NUMBER)
				{
					It.DiscardItem();
				}
			}
		}
		NavData->FinishBatchQuery();
	}
	else
	{
		NavData->BeginBatchQuery();
		if (bPathToItem)
		{
			for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
			{
				const FVector ItemLocation = GetItemLocation(QueryInstance, *It);
				for (int32 ContextIndex = 0; ContextIndex < ContextLocations.Num(); ContextIndex++)
				{
					const bool bFoundPath = TestPathTo(ItemLocation, ContextLocations[ContextIndex], PFMode, NavData, NavSys, QueryInstance.Owner.Get());
					It.SetScore(TestPurpose, FilterType, bFoundPath, bWantsPath);
				}
			}
		}
		else
		{
			for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
			{
				const FVector ItemLocation = GetItemLocation(QueryInstance, *It);
				for (int32 ContextIndex = 0; ContextIndex < ContextLocations.Num(); ContextIndex++)
				{
					const bool bFoundPath = TestPathFrom(ItemLocation, ContextLocations[ContextIndex], PFMode, NavData, NavSys, QueryInstance.Owner.Get());
					It.SetScore(TestPurpose, FilterType, bFoundPath, bWantsPath);
				}
			}
		}
		NavData->FinishBatchQuery();
	}
}

FString UEnvQueryTest_Pathfinding::GetDescriptionTitle() const
{
	FString ModeDesc[] = { TEXT("PathExist"), TEXT("PathCost"), TEXT("PathLength") };

	FString DirectionDesc = PathFromContext.IsDynamic() ?
		FString::Printf(TEXT("%s, direction: %s"), *UEnvQueryTypes::DescribeContext(Context).ToString(), *PathFromContext.ToString()) :
		FString::Printf(TEXT("%s %s"), PathFromContext.DefaultValue ? TEXT("from") : TEXT("to"), *UEnvQueryTypes::DescribeContext(Context).ToString());

	return FString::Printf(TEXT("%s: %s"), *ModeDesc[TestMode], *DirectionDesc);
}

FText UEnvQueryTest_Pathfinding::GetDescriptionDetails() const
{
	FText HPathDesc = LOCTEXT("HierarchicalPathfinding", "hierarchical pathfinding");
	FText Desc1;
	if (UseHierarchicalPathfinding.IsDynamic())
	{
		Desc1 = FText::Format(FText::FromString("{0}: {1}"), HPathDesc, FText::FromString(UseHierarchicalPathfinding.ToString()));
	}
	else if (UseHierarchicalPathfinding.DefaultValue)
	{
		Desc1 = HPathDesc;
	}

	FText DiscardDesc = LOCTEXT("DiscardUnreachable", "discard unreachable");
	FText Desc2;
	if (SkipUnreachable.IsDynamic())
	{
		Desc2 = FText::Format(FText::FromString("{0}: {1}"), DiscardDesc, FText::FromString(SkipUnreachable.ToString()));
	}
	else if (SkipUnreachable.DefaultValue)
	{
		Desc2 = DiscardDesc;
	}

	FText TestParamDesc = GetWorkOnFloatValues() ? DescribeFloatTestParams() : DescribeBoolTestParams("existing path");
	if (!Desc1.IsEmpty() && !Desc2.IsEmpty())
	{
		return FText::Format(FText::FromString("{0}, {1}\n{2}"), Desc1, Desc2, TestParamDesc);
	}
	else if (!Desc1.IsEmpty())
	{
		return FText::Format(FText::FromString("{0}\n{1}"), Desc1, TestParamDesc);
	}
	else if (!Desc2.IsEmpty())
	{
		return FText::Format(FText::FromString("{0}\n{1}"), Desc2, TestParamDesc);
	}

	return TestParamDesc;
}

#if WITH_EDITOR
void UEnvQueryTest_Pathfinding::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UEnvQueryTest_Pathfinding,TestMode))
	{
		SetWorkOnFloatValues(TestMode != EEnvTestPathfinding::PathExist);
	}
}
#endif

void UEnvQueryTest_Pathfinding::PostLoad()
{
	if (VerNum < EnvQueryTestVersion::DataProviders)
	{
		PathToItem.Convert(this, PathFromContext);
		DiscardUnreachable.Convert(this, SkipUnreachable);
		HierarchicalPathfinding.Convert(this, UseHierarchicalPathfinding);
	}

	Super::PostLoad();
	
	SetWorkOnFloatValues(TestMode != EEnvTestPathfinding::PathExist);
}

bool UEnvQueryTest_Pathfinding::TestPathFrom(const FVector& ItemPos, const FVector& ContextPos, EPathFindingMode::Type Mode, const ANavigationData* NavData, UNavigationSystem* NavSys, const UObject* PathOwner) const
{
	const bool bPathExists = NavSys->TestPathSync(FPathFindingQuery(PathOwner, NavData, ItemPos, ContextPos), Mode);
	return bPathExists;
}

bool UEnvQueryTest_Pathfinding::TestPathTo(const FVector& ItemPos, const FVector& ContextPos, EPathFindingMode::Type Mode, const ANavigationData* NavData, UNavigationSystem* NavSys, const UObject* PathOwner) const
{
	const bool bPathExists = NavSys->TestPathSync(FPathFindingQuery(PathOwner, NavData, ContextPos, ItemPos), Mode);
	return bPathExists;
}

float UEnvQueryTest_Pathfinding::FindPathCostFrom(const FVector& ItemPos, const FVector& ContextPos, EPathFindingMode::Type Mode, const ANavigationData* NavData, UNavigationSystem* NavSys, const UObject* PathOwner) const
{
	FPathFindingResult Result = NavSys->FindPathSync(FPathFindingQuery(PathOwner, NavData, ItemPos, ContextPos), Mode);
	return (Result.IsSuccessful()) ? Result.Path->GetCost() : BIG_NUMBER;
}

float UEnvQueryTest_Pathfinding::FindPathCostTo(const FVector& ItemPos, const FVector& ContextPos, EPathFindingMode::Type Mode, const ANavigationData* NavData, UNavigationSystem* NavSys, const UObject* PathOwner) const
{
	FPathFindingResult Result = NavSys->FindPathSync(FPathFindingQuery(PathOwner, NavData, ContextPos, ItemPos), Mode);
	return (Result.IsSuccessful()) ? Result.Path->GetCost() : BIG_NUMBER;
}

float UEnvQueryTest_Pathfinding::FindPathLengthFrom(const FVector& ItemPos, const FVector& ContextPos, EPathFindingMode::Type Mode, const ANavigationData* NavData, UNavigationSystem* NavSys, const UObject* PathOwner) const
{
	FPathFindingResult Result = NavSys->FindPathSync(FPathFindingQuery(PathOwner, NavData, ItemPos, ContextPos), Mode);
	return (Result.IsSuccessful()) ? Result.Path->GetLength() : BIG_NUMBER;
}

float UEnvQueryTest_Pathfinding::FindPathLengthTo(const FVector& ItemPos, const FVector& ContextPos, EPathFindingMode::Type Mode, const ANavigationData* NavData, UNavigationSystem* NavSys, const UObject* PathOwner) const
{
	FPathFindingResult Result = NavSys->FindPathSync(FPathFindingQuery(PathOwner, NavData, ContextPos, ItemPos), Mode);
	return (Result.IsSuccessful()) ? Result.Path->GetLength() : BIG_NUMBER;
}

ANavigationData* UEnvQueryTest_Pathfinding::FindNavigationData(UNavigationSystem* NavSys, UObject* Owner) const
{
	INavAgentInterface* NavAgent = Cast<INavAgentInterface>(Owner);
	if (NavAgent)
	{
		return NavSys->GetNavDataForProps(NavAgent->GetNavAgentPropertiesRef());
	}

	return NavSys->GetMainNavData(FNavigationSystem::DontCreate);
}

#undef LOCTEXT_NAMESPACE
