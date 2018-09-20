// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"

#if WITH_OCULUS_PRIVATE_CODE

class FPassthroughVS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FPassthroughVS, Global, UTILITYSHADERS_API);
	FPassthroughVS() {}

public:
	FPassthroughVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}
};

class FMaskGenerationPS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FMaskGenerationPS, Global, UTILITYSHADERS_API);
public:

	FMaskGenerationPS() { }
	FMaskGenerationPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		FoveatedMaskViewportSize.Bind(Initializer.ParameterMap, TEXT("FoveatedMaskViewportSize"));
		FoveatedMaskRadiusRatioItems.Bind(Initializer.ParameterMap, TEXT("FoveatedMaskRadiusRatioItems"));
		FoveatedMaskEyeFov.Bind(Initializer.ParameterMap, TEXT("FoveatedMaskEyeFov"));
	}

	virtual void SetParameters(FRHICommandList& RHICmdList, const FVector4& Viewport, const FVector4& EyeFov, int FrameIndexMod8);

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << FoveatedMaskViewportSize;
		Ar << FoveatedMaskRadiusRatioItems;
		Ar << FoveatedMaskEyeFov;
		return bShaderHasOutdatedParameters;
	}
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

protected:
	FShaderParameter FoveatedMaskViewportSize;
	FShaderParameter FoveatedMaskRadiusRatioItems;
	FShaderParameter FoveatedMaskEyeFov;
};

class FSimpleMaskReconstructionPS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FSimpleMaskReconstructionPS, Global, UTILITYSHADERS_API);
public:

	FSimpleMaskReconstructionPS() { }
	FSimpleMaskReconstructionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		FoveatedMaskRadiusRatioItems.Bind(Initializer.ParameterMap, TEXT("FoveatedMaskRadiusRatioItems"));
		FoveatedMaskEyeFov.Bind(Initializer.ParameterMap, TEXT("FoveatedMaskEyeFov"));
		SourceTextureParameter.Bind(Initializer.ParameterMap, TEXT("InSourceTexture"), SPF_Mandatory);
	}

	virtual void SetParameters(FRHICommandList& RHICmdList, const FVector4& Viewport, const FVector4& EyeFov, int FrameIndexMod8, FTextureRHIParamRef SourceTexture);

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << FoveatedMaskRadiusRatioItems;
		Ar << FoveatedMaskEyeFov;
		Ar << SourceTextureParameter;
		return bShaderHasOutdatedParameters;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

protected:
	FShaderParameter FoveatedMaskRadiusRatioItems;
	FShaderParameter FoveatedMaskEyeFov;
	FShaderResourceParameter SourceTextureParameter;
};

class FCopyReconstructedPixelsPS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FCopyReconstructedPixelsPS, Global, UTILITYSHADERS_API);
public:

	FCopyReconstructedPixelsPS() { }
	FCopyReconstructedPixelsPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		FoveatedMaskRadiusRatioItems.Bind(Initializer.ParameterMap, TEXT("FoveatedMaskRadiusRatioItems"));
		FoveatedMaskEyeFov.Bind(Initializer.ParameterMap, TEXT("FoveatedMaskEyeFov"));
		SourceTextureParameter.Bind(Initializer.ParameterMap, TEXT("InSourceTexture"), SPF_Mandatory);
	}

	virtual void SetParameters(FRHICommandList& RHICmdList, const FVector4& Viewport, const FVector4& EyeFov, int FrameIndexMod8, FTextureRHIParamRef SourceTexture);

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << FoveatedMaskRadiusRatioItems;
		Ar << FoveatedMaskEyeFov;
		Ar << SourceTextureParameter;
		return bShaderHasOutdatedParameters;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

protected:
	FShaderParameter FoveatedMaskRadiusRatioItems;
	FShaderParameter FoveatedMaskEyeFov;
	FShaderResourceParameter SourceTextureParameter;
};

#endif