// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/**
 *
 * This thumbnail renderer displays a given static mesh
 */

#pragma once
#include "StaticMeshThumbnailRenderer.generated.h"

UCLASS(config=Editor,MinimalAPI)
class UStaticMeshThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_UCLASS_BODY()


	// Begin UThumbnailRenderer Object
	UNREALED_API virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas) OVERRIDE;
	// End UThumbnailRenderer Object

	// UObject implementation
	UNREALED_API virtual void BeginDestroy() OVERRIDE;

private:
	class FStaticMeshThumbnailScene* ThumbnailScene;
};

