// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeneralProjectSettings.h"

class FGeneralProjectSettingsDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) OVERRIDE;
	// End of IDetailCustomization interface
};

