// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

class FEpicSurveyCommands : public TCommands<FEpicSurveyCommands>
{
public:
	FEpicSurveyCommands();

	virtual void RegisterCommands() OVERRIDE;

public:

	TSharedPtr< FUICommandInfo > OpenEpicSurvey;
};