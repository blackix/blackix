// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories.h"

class FLandscapeSplineTextObjectFactory : protected FCustomizableTextObjectFactory
{
public:
	FLandscapeSplineTextObjectFactory(FFeedbackContext* InWarningContext = GWarn);

	TArray<UObject*> ImportSplines(UObject* InParent, const TCHAR* TextBuffer);

protected:
	TArray<UObject*> OutObjects;

	virtual void ProcessConstructedObject(UObject* CreatedObject) OVERRIDE;
	virtual bool CanCreateClass(UClass* ObjectClass) const OVERRIDE;
};
