// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class SGraphPinPose : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinPose)	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

protected:
	// Begin SGraphPin interface
	virtual const FSlateBrush* GetPinIcon() const OVERRIDE;
	// End SGraphPin interface
};
