// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FoveatedMaskShaders.h"

#if WITH_OCULUS_PRIVATE_CODE

#include "ShaderParameterUtils.h"
#include "RenderUtils.h"

void FMaskGenerationPS::SetParameters(FRHICommandList& RHICmdList, const FVector4& Viewport, const FVector4& EyeFov, int FrameIndexMod8)
{
	FPixelShaderRHIParamRef PixelShaderRHI = GetPixelShader();

	if (FoveatedMaskViewportSize.IsBound())
	{
		FVector4 ViewportSize(Viewport.Z, Viewport.W, 1.0f / Viewport.Z, 1.0f / Viewport.W);
		SetShaderValue(RHICmdList, PixelShaderRHI, FoveatedMaskViewportSize, ViewportSize);
	}

	if (FoveatedMaskRadiusRatioItems.IsBound())
	{
		float HighResRadiusRatio = GetMaskBasedFoveatedRenderingHighResSqrTan();
		float MidResRadiusRatio = GetMaskBasedFoveatedRenderingMediumResSqrTan();
		float LowResRadiusRatio = GetMaskBasedFoveatedRenderingLowResSqrTan();
		SetShaderValue(RHICmdList, PixelShaderRHI, FoveatedMaskRadiusRatioItems, FVector4((float)FrameIndexMod8, HighResRadiusRatio, MidResRadiusRatio, LowResRadiusRatio));
	}

	if (FoveatedMaskEyeFov.IsBound())
	{
		SetShaderValue(RHICmdList, PixelShaderRHI, FoveatedMaskEyeFov, EyeFov);
	}
}

void FSimpleMaskReconstructionPS::SetParameters(FRHICommandList& RHICmdList, const FVector4& Viewport, const FVector4& EyeFov, int FrameIndexMod8, FTextureRHIParamRef SourceTexture)
{
	FPixelShaderRHIParamRef PixelShaderRHI = GetPixelShader();
	if (FoveatedMaskRadiusRatioItems.IsBound())
	{
		float HighResRadiusRatio = GetMaskBasedFoveatedRenderingHighResSqrTan();
		float MidResRadiusRatio = GetMaskBasedFoveatedRenderingMediumResSqrTan();
		float LowResRadiusRatio = GetMaskBasedFoveatedRenderingLowResSqrTan();
		SetShaderValue(RHICmdList, PixelShaderRHI, FoveatedMaskRadiusRatioItems, FVector4((float)FrameIndexMod8, HighResRadiusRatio, MidResRadiusRatio, LowResRadiusRatio));
	}

	if (FoveatedMaskEyeFov.IsBound())
	{
		SetShaderValue(RHICmdList, PixelShaderRHI, FoveatedMaskEyeFov, EyeFov);
	}

	SetTextureParameter(RHICmdList, PixelShaderRHI, SourceTextureParameter, SourceTexture);
}

void FCopyReconstructedPixelsPS::SetParameters(FRHICommandList& RHICmdList, const FVector4& Viewport, const FVector4& EyeFov, int FrameIndexMod8, FTextureRHIParamRef SourceTexture)
{
	FPixelShaderRHIParamRef PixelShaderRHI = GetPixelShader();
	if (FoveatedMaskRadiusRatioItems.IsBound())
	{
		float HighResRadiusRatio = GetMaskBasedFoveatedRenderingHighResSqrTan();
		float MidResRadiusRatio = GetMaskBasedFoveatedRenderingMediumResSqrTan();
		float LowResRadiusRatio = GetMaskBasedFoveatedRenderingLowResSqrTan();
		SetShaderValue(RHICmdList, PixelShaderRHI, FoveatedMaskRadiusRatioItems, FVector4((float)FrameIndexMod8, HighResRadiusRatio, MidResRadiusRatio, LowResRadiusRatio));
	}

	if (FoveatedMaskEyeFov.IsBound())
	{
		SetShaderValue(RHICmdList, PixelShaderRHI, FoveatedMaskEyeFov, EyeFov);
	}

	SetTextureParameter(RHICmdList, PixelShaderRHI, SourceTextureParameter, SourceTexture);
}

IMPLEMENT_SHADER_TYPE(, FPassthroughVS, TEXT("/Engine/Private/FoveatedMaskShaders.usf"), TEXT("PassthroughVertexShader"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FMaskGenerationPS, TEXT("/Engine/Private/FoveatedMaskShaders.usf"), TEXT("MaskGenerationPixelShader"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FSimpleMaskReconstructionPS, TEXT("/Engine/Private/FoveatedMaskShaders.usf"), TEXT("SimpleMaskReconstructionPS"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FCopyReconstructedPixelsPS, TEXT("/Engine/Private/FoveatedMaskShaders.usf"), TEXT("CopyReconstructedPixelsPS"), SF_Pixel);

#endif