// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "SceneInterface.h"


static TAutoConsoleVariable<int32> CVarUseClusteredForward(
	TEXT("r.UseClusteredForward"),
	0,
	TEXT("Determines if the forward renderer should be used")
	TEXT(" 0: Use the default renderer based on feature level")
	TEXT(" 1: Use the clustered forward renderer"),
	ECVF_Default);

FSceneInterface::FSceneInterface()
{
}

FSceneInterface::~FSceneInterface()
{
}


EShadingPath FSceneInterface::GetShadingPath() const
{
	auto const FeatureLevel = GetFeatureLevel();
	if (FeatureLevel == ERHIFeatureLevel::SM5 
		&& CVarUseClusteredForward.GetValueOnGameThread())
	{
		return EShadingPath::ClusteredForward;
	}
	else
	{
		return FeatureLevel >= ERHIFeatureLevel::SM4
			? EShadingPath::Deferred
			: EShadingPath::Forward;
	}
}