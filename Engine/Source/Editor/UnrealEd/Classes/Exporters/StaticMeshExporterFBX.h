// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// StaticMeshExporterFBX
//=============================================================================

#pragma once
#include "StaticMeshExporterFBX.generated.h"

UCLASS()
class UStaticMeshExporterFBX : public UExporter
{
	GENERATED_UCLASS_BODY()


	// Begin UExporter Interface
	virtual bool ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex = 0, uint32 PortFlags=0 ) OVERRIDE;
	// End UExporter Interface
};



