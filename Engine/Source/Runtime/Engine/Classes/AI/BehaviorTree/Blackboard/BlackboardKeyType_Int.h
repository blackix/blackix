// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "BlackboardKeyType_Int.generated.h"

UCLASS(EditInlineNew, MinimalAPI)
class UBlackboardKeyType_Int : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()

	static int32 GetValue(const uint8* RawData);
	static bool SetValue(uint8* RawData, int32 Value);

	virtual FString DescribeValue(const uint8* RawData) const OVERRIDE;
	virtual int32 Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const OVERRIDE;
};
