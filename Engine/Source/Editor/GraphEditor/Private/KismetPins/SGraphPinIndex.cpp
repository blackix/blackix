// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "GraphEditorCommon.h"
#include "SGraphPinIndex.h"
#include "SPinTypeSelector.h"

void SGraphPinIndex::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SGraphPinIndex::GetDefaultValueWidget()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	return SNew(SPinTypeSelector, FGetPinTypeTree::CreateUObject(Schema, &UEdGraphSchema_K2::GetVariableIndexTypeTree))
		.TargetPinType(this, &SGraphPinIndex::OnGetPinType)
		.OnPinTypeChanged(this, &SGraphPinIndex::OnTypeChanged)
		.Schema(Schema)
		.bAllowExec(false)
		.bAllowWildcard(false)
		.IsEnabled(true)
		.bAllowArrays(false);
}

FEdGraphPinType SGraphPinIndex::OnGetPinType() const
{
	return GraphPinObj->PinType;
}

void SGraphPinIndex::OnTypeChanged(const FEdGraphPinType& PinType)
{
	GraphPinObj->PinType = PinType;
	// Let the node know that one of its' pins had their pin type changed
	if (GraphPinObj && GraphPinObj->GetOwningNode())
	{
		GraphPinObj->GetOwningNode()->PinTypeChanged(GraphPinObj);
	}
}
