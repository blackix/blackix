// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11Bridge.h: Bridge to low-level D3D11 layer
=============================================================================*/

#pragma once

#include "Core.h"
#include "RHI.h"

class FD3D11Bridge : public FRHIBridge
{
public:
	virtual ~FD3D11Bridge() {}

	// Init bridge with D3DDevice and Context. 
	virtual void Init(ID3D11Device* InD3DDevice, ID3D11DeviceContext* InD3DDeviceContext) = 0;
	// Resets all D3D pointers, called before shutdown
	virtual void Reset() = 0;

	// Resets Viewport-specific pointers (BackBufferRT, SwapChain).
	virtual void ReleaseBackBuffer() = 0;

	// Returns 'true' if Engine Renderer should do its own Present; 
	// false otherwise.
	virtual bool FinishFrame(int SyncInterval) = 0;
};

