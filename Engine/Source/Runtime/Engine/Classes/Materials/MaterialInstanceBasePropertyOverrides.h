// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "MaterialInstanceBasePropertyOverrides.generated.h"

class UMaterialInstance;

/** Properties from the base material that can be overridden in material instances. */
USTRUCT()
struct ENGINE_API FMaterialInstanceBasePropertyOverrides
{
	GENERATED_USTRUCT_BODY()

	/** Enables override of the opacity mask clip value. */
	UPROPERTY(EditAnywhere, Category = Material)
	bool bOverride_OpacityMaskClipValue;

	/** Enables override of the blend mode. */
	UPROPERTY(EditAnywhere, Category = Material)
	bool bOverride_BlendMode;

	/** Enables override of the shading model. */
	UPROPERTY(EditAnywhere, Category = Material)
	bool bOverride_ShadingModel;

	/** Enables override of the two sided property. */
	UPROPERTY(EditAnywhere, Category = Material)
	bool bOverride_DitheredLODTransition;

	/** Enables override of the two sided property. */
	UPROPERTY(EditAnywhere, Category = Material)
	bool bOverride_TwoSided;

	/** Enables override of the fully rough property. */
	UPROPERTY(EditAnywhere, Category = Material)
	bool bOverride_FullyRough;

	/** Enables override of the cheap shading property. */
	UPROPERTY(EditAnywhere, Category = Material)
	bool bOverride_CheapShading;

	/** If BlendMode is BLEND_Masked, the surface is not rendered where OpacityMask < OpacityMaskClipValue. */
	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_OpacityMaskClipValue", NoSpinbox = true))
	float OpacityMaskClipValue;

	/** The blend mode */
	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_BlendMode"))
	TEnumAsByte<EBlendMode> BlendMode;

	/** The shading model */
	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_ShadingModel"))
	TEnumAsByte<EMaterialShadingModel> ShadingModel;

	/** Indicates that the material should be rendered without backface culling and the normal should be flipped for backfaces. */
	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_TwoSided"))
	uint32 TwoSided : 1;

	/** Whether the material should support a dithered LOD transition when used with the foliage system. */
	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_DitheredLODTransition"))
	uint32 DitheredLODTransition : 1;

	/** Whether fully rough should be enabled for this material. */
	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_FullyRough"))
	uint32 FullyRough : 1;

	/** Whether cheap shading should be enabled for this material. */
	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_CheapShading"))
	uint32 CheapShading : 1;

	FMaterialInstanceBasePropertyOverrides();

	bool operator==(const FMaterialInstanceBasePropertyOverrides& Other)const;
	bool operator!=(const FMaterialInstanceBasePropertyOverrides& Other)const;
};