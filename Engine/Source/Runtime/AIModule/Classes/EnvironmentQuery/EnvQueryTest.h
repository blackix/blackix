// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "DataProviders/AIDataProvider.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvQueryTest.generated.h"

class UEnvQueryItemType;
class UEnvQueryContext;
#if WITH_EDITOR
struct FPropertyChangedEvent;
#endif // WITH_EDITOR

namespace EnvQueryTestVersion
{
	const int32 Initial = 0;
	const int32 DataProviders = 1;

	const int32 Latest = DataProviders;
}

UCLASS(Abstract)
class AIMODULE_API UEnvQueryTest : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Number of test as defined in data asset */
	UPROPERTY()
	int32 TestOrder;

	/** Versioning for updating deprecated properties */
	UPROPERTY()
	int32 VerNum;

	/** The purpose of this test.  Should it be used for filtering possible results, scoring them, or both? */
	UPROPERTY(EditDefaultsOnly, Category=Test)
	TEnumAsByte<EEnvTestPurpose::Type> TestPurpose;
	
	/** Does this test filter out results that are below a lower limit, above an upper limit, or both?  Or does it just look for a matching value? */
	UPROPERTY(EditDefaultsOnly, Category=Filter)
	TEnumAsByte<EEnvTestFilterType::Type> FilterType;

	/** Desired boolean value of the test for scoring to occur or filtering test to pass. */
	UPROPERTY(EditDefaultsOnly, Category=Filter)
	FAIDataProviderBoolValue BoolValue;

	/** Minimum limit (inclusive) of valid values for the raw test value. Lower values will be discarded as invalid. */
	UPROPERTY(EditDefaultsOnly, Category=Filter)
	FAIDataProviderFloatValue FloatValueMin;

	/** Maximum limit (inclusive) of valid values for the raw test value. Higher values will be discarded as invalid. */
	UPROPERTY(EditDefaultsOnly, Category=Filter)
	FAIDataProviderFloatValue FloatValueMax;

	/** Cost of test */
	TEnumAsByte<EEnvTestCost::Type> Cost;

	/** How should the lower bound for normalization of the raw test value before applying the scoring formula be determined?
	    Should it use the lowest value found (tested), the lower threshold for filtering, or a separate specified normalization minimum? */
	UPROPERTY(EditDefaultsOnly, Category=Score)
	TEnumAsByte<EEnvQueryTestClamping::Type> ClampMinType;

	/** How should the upper bound for normalization of the raw test value before applying the scoring formula be determined?
	    Should it use the highest value found (tested), the upper threshold for filtering, or a separate specified normalization maximum? */
	UPROPERTY(EditDefaultsOnly, Category=Score)
	TEnumAsByte<EEnvQueryTestClamping::Type> ClampMaxType;

	/** Minimum value to use to normalize the raw test value before applying scoring formula. */
	UPROPERTY(EditDefaultsOnly, Category=Score)
	FAIDataProviderFloatValue ScoreClampMin;

	/** Maximum value to use to normalize the raw test value before applying scoring formula. */
	UPROPERTY(EditDefaultsOnly, Category=Score)
	FAIDataProviderFloatValue ScoreClampMax;

	/** The shape of the curve equation to apply to the normalized score before multiplying by factor. */
	UPROPERTY(EditDefaultsOnly, Category=Score)
	TEnumAsByte<EEnvTestScoreEquation::Type> ScoringEquation;

	/** The weight (factor) by which to multiply the normalized score after the scoring equation is applied. */
	UPROPERTY(EditDefaultsOnly, Category=Score, Meta=(ClampMin="0.001", UIMin="0.001"))
	FAIDataProviderFloatValue ScoringFactor;

	/** Validation: item type that can be used with this test */
	TSubclassOf<UEnvQueryItemType> ValidItemType;

	void SetWorkOnFloatValues(bool bWorkOnFloats);
	FORCEINLINE bool GetWorkOnFloatValues() const { return bWorkOnFloatValues; }

	FORCEINLINE bool CanRunAsFinalCondition() const 
	{ 
		return (TestPurpose != EEnvTestPurpose::Score)							// We are filtering and...
				&& ((TestPurpose == EEnvTestPurpose::Filter)					// Either we are NOT scoring at ALL or...
					|| (ScoringEquation == EEnvTestScoreEquation::Constant));	// We are giving a constant score value for passing.
	}

	// BEGIN: deprecated properties, do not use them
	UPROPERTY()
	FEnvBoolParam BoolFilter;

	UPROPERTY()
	FEnvFloatParam FloatFilterMin;

	UPROPERTY()
	FEnvFloatParam FloatFilterMax;

	UPROPERTY()
	FEnvFloatParam ScoreClampingMin;

	UPROPERTY()
	FEnvFloatParam ScoreClampingMax;

	UPROPERTY()
	FEnvFloatParam Weight;
	// END: deprecated properties, do not use them

private:
	/** When set, test operates on float values (e.g. distance, with AtLeast, UpTo conditions),
	 *  otherwise it will accept bool values (e.g. visibility, with Equals condition) */
	UPROPERTY()
	uint32 bWorkOnFloatValues : 1;

public:

	/** Function that does the actual work */
	virtual void RunTest(FEnvQueryInstance& QueryInstance) const { checkNoEntry(); }

	/** check if test supports item type */
	bool IsSupportedItem(TSubclassOf<UEnvQueryItemType> ItemType) const;

	/** check if context needs to be updated for every item */
	bool IsContextPerItem(TSubclassOf<UEnvQueryContext> CheckContext) const;

	/** helper: get location of item */
	FVector GetItemLocation(FEnvQueryInstance& QueryInstance, int32 ItemIndex) const;

	/** helper: get location of item */
	FORCEINLINE FVector GetItemLocation(FEnvQueryInstance& QueryInstance, const FEnvQueryInstance::ItemIterator& Iterator) const
	{
		return GetItemLocation(QueryInstance, *Iterator);
	}

	/** helper: get location of item */
	FRotator GetItemRotation(FEnvQueryInstance& QueryInstance, int32 ItemIndex) const;

	/** helper: get location of item */
	FORCEINLINE FRotator GetItemRotation(FEnvQueryInstance& QueryInstance, const FEnvQueryInstance::ItemIterator& Iterator) const
	{
		return GetItemRotation(QueryInstance, *Iterator);
	}

	/** helper: get actor from item */
	AActor* GetItemActor(FEnvQueryInstance& QueryInstance, int32 ItemIndex) const;
		
	/** helper: get actor from item */
	FORCEINLINE AActor* GetItemActor(FEnvQueryInstance& QueryInstance, const FEnvQueryInstance::ItemIterator& Iterator) const
	{
		return GetItemActor(QueryInstance, *Iterator);
	}

	/** normalize scores in range */
	void NormalizeItemScores(FEnvQueryInstance& QueryInstance);

	FORCEINLINE bool IsScoring() const { return (TestPurpose != EEnvTestPurpose::Filter); } 
	FORCEINLINE bool IsFiltering() const { return (TestPurpose != EEnvTestPurpose::Score); }

	/** get description of test */
	virtual FString GetDescriptionTitle() const;
	virtual FText GetDescriptionDetails() const;

	FText DescribeFloatTestParams() const;
	FText DescribeBoolTestParams(const FString& ConditionDesc) const;

	virtual void PostLoad() override;

	/** update to latest version after spawning */
	void UpdateTestVersion();

#if WITH_EDITOR && USE_EQS_DEBUGGER
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif //WITH_EDITOR && USE_EQS_DEBUGGER
};

//////////////////////////////////////////////////////////////////////////
// Inlines

FORCEINLINE bool UEnvQueryTest::IsSupportedItem(TSubclassOf<UEnvQueryItemType> ItemType) const
{
	return ItemType && (ItemType == ValidItemType || ItemType->IsChildOf(ValidItemType));
};
