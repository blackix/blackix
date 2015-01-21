// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "InheritableComponentHandler.generated.h"

class USCS_Node;
class UActorComponent;

USTRUCT()
struct ENGINE_API FComponentKey
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	UBlueprintGeneratedClass* OwnerClass;

	UPROPERTY()
	FName VariableName;

	UPROPERTY()
	FGuid VariableGuid;

	FComponentKey()
		: OwnerClass(nullptr)
	{}

	FComponentKey(USCS_Node* SCSNode);

	bool Match(const FComponentKey OtherKey) const;

	bool IsValid() const
	{
		return OwnerClass && (VariableName != NAME_None) && VariableGuid.IsValid();
	}

	USCS_Node* FindSCSNode() const;
	UActorComponent* GetOriginalTemplate() const;
};

USTRUCT()
struct FComponentOverrideRecord
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	UActorComponent* ComponentTemplate;

	UPROPERTY()
	FComponentKey ComponentKey;

	FComponentOverrideRecord()
		: ComponentTemplate(nullptr)
	{}
};

UCLASS()
class ENGINE_API UInheritableComponentHandler : public UObject
{
	GENERATED_BODY()

#if WITH_EDITOR
private:
	bool IsRecordValid(const FComponentOverrideRecord& Record) const;
	bool IsRecordNecessary(const FComponentOverrideRecord& Record) const;

public:
	UActorComponent* CreateOverridenComponentTemplate(FComponentKey Key);
	void UpdateOwnerClass(UBlueprintGeneratedClass* OwnerClass);
	void ValidateTemplates();
	bool IsValid() const;
	UActorComponent* FindBestArchetype(FComponentKey Key) const;

	void GetAllTemplates(TArray<UActorComponent*>& OutArray) const
	{
		for (auto Record : Records)
		{
			OutArray.Add(Record.ComponentTemplate);
		}
	}

	bool IsEmpty() const
	{
		return 0 == Records.Num();
	}

	void PreloadAllTempates();

	bool RenameTemplate(FComponentKey OldKey, FName NewName);

#endif

public:
	UActorComponent* GetOverridenComponentTemplate(FComponentKey Key) const;

private:
	const FComponentOverrideRecord* FindRecord(const FComponentKey Key) const;
	
	UPROPERTY()
	TArray<FComponentOverrideRecord> Records;
};
