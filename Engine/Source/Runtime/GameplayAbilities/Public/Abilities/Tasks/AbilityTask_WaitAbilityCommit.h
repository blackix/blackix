// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "AbilityTask.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AttributeSet.h"
#include "AbilityTask_WaitAbilityCommit.generated.h"

class AActor;

/**
 *	Waits for the actor to activate another ability
 */
UCLASS(MinimalAPI)
class UAbilityTask_WaitAbilityCommit : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitAbilityCommitDelegate, UGameplayAbility*, ActivatedAbility);

	UPROPERTY(BlueprintAssignable)
	FWaitAbilityCommitDelegate	OnCommit;

	virtual void Activate() override;

	UFUNCTION()
	void OnAbilityCommit(UGameplayAbility *ActivatedAbility);

	/** Wait until a new ability (of the same or different type) is commited. Used to gracefully interrupt abilities in specific ways. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "TRUE", FriendlyName = "Wait For New Ability Commit"))
	static UAbilityTask_WaitAbilityCommit* WaitForAbilityCommit(UObject* WorldContextObject, FGameplayTag WithTag, FGameplayTag WithoutTage);

	FGameplayTag WithTag;
	FGameplayTag WithoutTag;

protected:

	virtual void OnDestroy(bool AbilityEnded) override;

	FDelegateHandle OnAbilityCommitDelegateHandle;
};