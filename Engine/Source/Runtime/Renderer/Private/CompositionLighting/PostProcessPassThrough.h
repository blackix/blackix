// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessPassThrough.h: Post processing pass through implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"

// ePId_Input0: Input image
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessPassThrough : public TRenderingCompositePassBase<1, 1>
{
public:
	// constructor
	// @param InDest - 0 if a new intermediate target should be created
	FRCPassPostProcessPassThrough(IPooledRenderTarget* InDest, bool bInAdditiveBlend = false);
	// constructor
	FRCPassPostProcessPassThrough(FPooledRenderTargetDesc InNewDesc);

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context);
	virtual void Release() OVERRIDE { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const;

private:
	// 0 if a new intermediate should be created
	IPooledRenderTarget* Dest;
	bool bAdditiveBlend;
	FPooledRenderTargetDesc NewDesc;
};
