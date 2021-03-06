// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "DistributionFloatUniform.generated.h"

UCLASS(collapsecategories, hidecategories=Object, editinlinenew, MinimalAPI)
class UDistributionFloatUniform : public UDistributionFloat
{
	GENERATED_UCLASS_BODY()

	/** Low end of output float distribution. */
	UPROPERTY(EditAnywhere, Category=DistributionFloatUniform)
	float Min;

	/** High end of output float distribution. */
	UPROPERTY(EditAnywhere, Category=DistributionFloatUniform)
	float Max;

	// Begin UObject Interface
	virtual void PostInitProperties() OVERRIDE;
	virtual void PostLoad() OVERRIDE;
	// End UObject Interface

	// Begin UDistributionFloat Interface
	virtual float GetValue( float F = 0.f, UObject* Data = NULL, class FRandomStream* InRandomStream = NULL ) const OVERRIDE;

	//@todo.CONSOLE: Currently, consoles need this? At least until we have some sort of cooking/packaging step!
	virtual ERawDistributionOperation GetOperation() const OVERRIDE;
	virtual uint32 InitializeRawEntry(float Time, float* Values) const OVERRIDE;
	// End UDistributionFloat Interface

	// Begin FCurveEdInterface interface
	virtual int32		GetNumKeys() const OVERRIDE;
	virtual int32		GetNumSubCurves() const OVERRIDE;
	virtual FColor	GetSubCurveButtonColor(int32 SubCurveIndex, bool bIsSubCurveHidden) const OVERRIDE;
	virtual float	GetKeyIn(int32 KeyIndex) OVERRIDE;
	virtual float	GetKeyOut(int32 SubIndex, int32 KeyIndex) OVERRIDE;
	virtual FColor	GetKeyColor(int32 SubIndex, int32 KeyIndex, const FColor& CurveColor) OVERRIDE;
	virtual void	GetInRange(float& MinIn, float& MaxIn) const OVERRIDE;
	virtual void	GetOutRange(float& MinOut, float& MaxOut) const OVERRIDE;
	virtual EInterpCurveMode	GetKeyInterpMode(int32 KeyIndex) const OVERRIDE;
	virtual void	GetTangents(int32 SubIndex, int32 KeyIndex, float& ArriveTangent, float& LeaveTangent) const OVERRIDE;
	virtual float	EvalSub(int32 SubIndex, float InVal) OVERRIDE;
	virtual int32		CreateNewKey(float KeyIn) OVERRIDE;
	virtual void	DeleteKey(int32 KeyIndex) OVERRIDE;
	virtual int32		SetKeyIn(int32 KeyIndex, float NewInVal) OVERRIDE;
	virtual void	SetKeyOut(int32 SubIndex, int32 KeyIndex, float NewOutVal) OVERRIDE;
	virtual void	SetKeyInterpMode(int32 KeyIndex, EInterpCurveMode NewMode) OVERRIDE;
	virtual void	SetTangents(int32 SubIndex, int32 KeyIndex, float ArriveTangent, float LeaveTangent) OVERRIDE;
	// End FCurveEdInterface interface
};

