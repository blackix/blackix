// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLBridge.h: Bridge to low-level OpenGL layer
=============================================================================*/

#pragma once

#include "Core.h"
#include "RHI.h"

class FOpenGLBridge : public FRHIBridge
{
public:
	// Init bridge. 
	virtual void Init() = 0;
	// Resets all D3D pointers, called before shutdown
	virtual void Reset() = 0;

	// Resets Viewport-specific pointers (BackBufferRT, SwapChain).
	virtual void ReleaseBackBuffer() = 0;

	// Returns 'true' if Engine Renderer should do its own Present; 
	// false otherwise.
	virtual bool FinishFrame(int SyncInterval) = 0;
};
