// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// TexAlignerPlanar
// Aligns according to which axis the poly is most facing.
// 
//=============================================================================

#pragma once
#include "TexAlignerPlanar.generated.h"

UCLASS(hidecategories=Object)
class UTexAlignerPlanar : public UTexAligner
{
	GENERATED_UCLASS_BODY()


	// Begin UObject Interface
	virtual void PostInitProperties() OVERRIDE;
	// End UObject Interface

	// Begin UTexAligner Interface
	virtual void AlignSurf( ETexAlign InTexAlignType, UModel* InModel, FBspSurfIdx* InSurfIdx, FPoly* InPoly, FVector* InNormal ) OVERRIDE;
	// End UTexAligner Interface
};

