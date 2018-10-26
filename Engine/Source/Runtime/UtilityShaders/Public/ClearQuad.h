// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHIDefinitions.h"
#include "RenderResource.h"

class FRHICommandList;
struct FRWBufferStructured;
struct FRWBuffer;
struct FSceneRenderTargetItem;
class FRHIUnorderedAccessView;
class FGraphicsPipelineStateInitializer;

class FClearVertexBuffer : public FVertexBuffer
{
public:
	/**
	* Initialize the RHI for this rendering resource
	*/
	virtual void InitRHI() override
	{
		// create a static vertex buffer
		FRHIResourceCreateInfo CreateInfo;
		VertexBufferRHI = RHICreateVertexBuffer(sizeof(FVector4) * 4, BUF_Static, CreateInfo);
		void* VoidPtr = RHILockVertexBuffer(VertexBufferRHI, 0, sizeof(FVector4) * 4, RLM_WriteOnly);
		// Generate the vertices used
		FVector4* Vertices = reinterpret_cast<FVector4*>(VoidPtr);
		Vertices[0] = FVector4(-1.0f, 1.0f, 0.0f, 1.0f);
		Vertices[1] = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
		Vertices[2] = FVector4(-1.0f, -1.0f, 0.0f, 1.0f);
		Vertices[3] = FVector4(1.0f, -1.0f, 0.0f, 1.0f);
		RHIUnlockVertexBuffer(VertexBufferRHI);
	}
};
extern UTILITYSHADERS_API TGlobalResource<FClearVertexBuffer> GClearVertexBuffer;

struct FClearQuadCallbacks
{
	TFunction<void(FGraphicsPipelineStateInitializer&)> PSOModifier = nullptr;
	TFunction<void(FRHICommandList&)> PreClear = nullptr;
	TFunction<void(FRHICommandList&)> PostClear = nullptr;
};

extern UTILITYSHADERS_API const uint32 GMaxSizeUAVDMA;
extern UTILITYSHADERS_API void ClearUAV(FRHICommandList& RHICmdList, const FRWBufferStructured& StructuredBuffer, uint32 Value);
extern UTILITYSHADERS_API void ClearUAV(FRHICommandList& RHICmdList, const FRWBuffer& Buffer, uint32 Value);
extern UTILITYSHADERS_API void ClearUAV(FRHICommandList& RHICmdList, FRHIUnorderedAccessView* Buffer, uint32 NumBytes, uint32 Value);

extern UTILITYSHADERS_API void ClearUAV(FRHICommandList& RHICmdList, const FSceneRenderTargetItem& RenderTargetItem, const float(&ClearValues)[4]);
extern UTILITYSHADERS_API void ClearUAV(FRHICommandList& RHICmdList, const FSceneRenderTargetItem& RenderTargetItem, const uint32(&ClearValues)[4]);
extern UTILITYSHADERS_API void ClearUAV(FRHICommandList& RHICmdList, const FSceneRenderTargetItem& RenderTargetItem,  const FLinearColor& ClearColor);

extern UTILITYSHADERS_API void ClearTexture2DUAV(FRHICommandList& RHICmdList, FRHIUnorderedAccessView* UAV, int32 Width, int32 Height, const FLinearColor& ClearColor);

#if WITH_OCULUS_PRIVATE_CODE
extern UTILITYSHADERS_API void DrawClearQuadMRT(FRHICommandList& RHICmdList, bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, uint32 StencilMask);
extern UTILITYSHADERS_API void DrawClearQuadMRT(FRHICommandList& RHICmdList, bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, uint32 StencilMask, FClearQuadCallbacks ClearQuadCallbacks);
extern UTILITYSHADERS_API void DrawClearQuadMRT(FRHICommandList& RHICmdList, bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, uint32 StencilMask, FIntPoint ViewSize, FIntRect ExcludeRect);
#else
extern UTILITYSHADERS_API void DrawClearQuadMRT(FRHICommandList& RHICmdList, bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil);
extern UTILITYSHADERS_API void DrawClearQuadMRT(FRHICommandList& RHICmdList, bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, FClearQuadCallbacks ClearQuadCallbacks);
extern UTILITYSHADERS_API void DrawClearQuadMRT(FRHICommandList& RHICmdList, bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, FIntPoint ViewSize, FIntRect ExcludeRect);
#endif

#if WITH_OCULUS_PRIVATE_CODE
inline void DrawClearQuad(FRHICommandList& RHICmdList, bool bClearColor, const FLinearColor& Color, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, uint32 StencilMask)
{
	DrawClearQuadMRT(RHICmdList, bClearColor, 1, &Color, bClearDepth, Depth, bClearStencil, Stencil, StencilMask);
}
#else
inline void DrawClearQuad(FRHICommandList& RHICmdList, bool bClearColor, const FLinearColor& Color, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil)
{
	DrawClearQuadMRT(RHICmdList, bClearColor, 1, &Color, bClearDepth, Depth, bClearStencil, Stencil);
}
#endif

#if WITH_OCULUS_PRIVATE_CODE
inline void DrawClearQuad(FRHICommandList& RHICmdList, bool bClearColor, const FLinearColor& Color, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, uint32 StencilMask, FIntPoint ViewSize, FIntRect ExcludeRect)
{
	DrawClearQuadMRT(RHICmdList, bClearColor, 1, &Color, bClearDepth, Depth, bClearStencil, Stencil, StencilMask, ViewSize, ExcludeRect);
}
#else
inline void DrawClearQuad(FRHICommandList& RHICmdList, bool bClearColor, const FLinearColor& Color, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, FIntPoint ViewSize, FIntRect ExcludeRect)
{
	DrawClearQuadMRT(RHICmdList, bClearColor, 1, &Color, bClearDepth, Depth, bClearStencil, Stencil, ViewSize, ExcludeRect);
}
#endif

inline void DrawClearQuad(FRHICommandList& RHICmdList, const FLinearColor& Color)
{
#if WITH_OCULUS_PRIVATE_CODE
	DrawClearQuadMRT(RHICmdList, true, 1, &Color, false, 0, false, 0, 0xff);
#else
	DrawClearQuadMRT(RHICmdList, true, 1, &Color, false, 0, false, 0);
#endif
}

inline void DrawClearQuad(FRHICommandList& RHICmdList, const FLinearColor& Color, FClearQuadCallbacks ClearQuadCallbacks)
{
#if WITH_OCULUS_PRIVATE_CODE
	DrawClearQuadMRT(RHICmdList, true, 1, &Color, false, 0, false, 0, 0xff, ClearQuadCallbacks);
#else
	DrawClearQuadMRT(RHICmdList, true, 1, &Color, false, 0, false, 0, ClearQuadCallbacks);
#endif
}
