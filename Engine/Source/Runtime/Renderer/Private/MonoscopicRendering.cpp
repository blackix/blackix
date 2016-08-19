// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TranslucentRendering.cpp: Translucent rendering implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "ScreenRendering.h"
#include "SceneUtils.h"
#include "ClearReplacementShaders.h"
#include "SceneFilterRendering.h"
#include "ConvexVolume.h"
#include "PostProcess/PostProcessing.h"

/** SHADERS */
/** Pixel Shader to composite monoscopic view into the stereo buffers  */
class FCompositeMonoscopicViewPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCompositeMonoscopicViewPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		//return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
		return true;
	}

	FCompositeMonoscopicViewPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);

		MonoColorTextureParameter.Bind(Initializer.ParameterMap, TEXT("MonoColorTexture"));
		MonoColorTextureParameterSampler.Bind(Initializer.ParameterMap, TEXT("MonoColorTextureSampler"));

		MonoDepthTextureParameter.Bind(Initializer.ParameterMap, TEXT("MonoDepthTexture"));
		MonoDepthTextureParameterSampler.Bind(Initializer.ParameterMap, TEXT("MonoDepthTextureSampler"));
	}
	FCompositeMonoscopicViewPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FGlobalShader::SetParameters(RHICmdList, GetPixelShader(), View);
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		FSamplerStateRHIRef Filter = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		SetTextureParameter(RHICmdList, GetPixelShader(), MonoColorTextureParameter, MonoColorTextureParameterSampler, Filter, SceneContext.GetSceneMonoColorTexture());
		SetTextureParameter(RHICmdList, GetPixelShader(), MonoDepthTextureParameter, MonoDepthTextureParameterSampler, Filter, SceneContext.GetSceneMonoDepthTexture());

		SceneTextureParameters.Set(RHICmdList, GetPixelShader(), View);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << MonoColorTextureParameter;
		Ar << MonoDepthTextureParameter;
		Ar << MonoColorTextureParameterSampler;
		Ar << MonoDepthTextureParameterSampler;
		Ar << SceneTextureParameters;
		return bShaderHasOutdatedParameters;
	}

	FShaderResourceParameter MonoColorTextureParameter;
	FShaderResourceParameter MonoDepthTextureParameter;
	FShaderResourceParameter MonoColorTextureParameterSampler;
	FShaderResourceParameter MonoDepthTextureParameterSampler;
	FSceneTextureShaderParameters SceneTextureParameters;
};

IMPLEMENT_SHADER_TYPE(, FCompositeMonoscopicViewPS, TEXT("MonoscopicRendering"), TEXT("CompositeMonoscopicView"), SF_Pixel);

FGlobalBoundShaderState CompositeMonoscopicViewBoundShaderState;

class FCompositeMonoscopicViewNoDepthPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCompositeMonoscopicViewNoDepthPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		//return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
		return true;
	}

	FCompositeMonoscopicViewNoDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);

		MonoColorTextureParameter.Bind(Initializer.ParameterMap, TEXT("MonoColorTexture"));
		MonoColorTextureParameterSampler.Bind(Initializer.ParameterMap, TEXT("MonoColorTextureSampler"));

		MonoDepthTextureParameter.Bind(Initializer.ParameterMap, TEXT("MonoDepthTexture"));
		MonoDepthTextureParameterSampler.Bind(Initializer.ParameterMap, TEXT("MonoDepthTextureSampler"));
	}
	FCompositeMonoscopicViewNoDepthPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FGlobalShader::SetParameters(RHICmdList, GetPixelShader(), View);
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		FSamplerStateRHIRef Filter = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		SetTextureParameter(RHICmdList, GetPixelShader(), MonoColorTextureParameter, MonoColorTextureParameterSampler, Filter, SceneContext.GetSceneMonoColorTexture());
		SetTextureParameter(RHICmdList, GetPixelShader(), MonoDepthTextureParameter, MonoDepthTextureParameterSampler, Filter, SceneContext.GetSceneMonoDepthTexture());

		SceneTextureParameters.Set(RHICmdList, GetPixelShader(), View);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << MonoColorTextureParameter;
		Ar << MonoDepthTextureParameter;
		Ar << MonoColorTextureParameterSampler;
		Ar << MonoDepthTextureParameterSampler;
		Ar << SceneTextureParameters;
		return bShaderHasOutdatedParameters;
	}

	FShaderResourceParameter MonoColorTextureParameter;
	FShaderResourceParameter MonoDepthTextureParameter;
	FShaderResourceParameter MonoColorTextureParameterSampler;
	FShaderResourceParameter MonoDepthTextureParameterSampler;
	FSceneTextureShaderParameters SceneTextureParameters;
};

IMPLEMENT_SHADER_TYPE(, FCompositeMonoscopicViewNoDepthPS, TEXT("MonoscopicRendering"), TEXT("CompositeMonoscopicViewNoDepth"), SF_Pixel);

FGlobalBoundShaderState CompositeMonoscopicViewNoDepthBoundShaderState;

/** Pixel Shader to composite monoscopic view into the stereo buffers  */
class FGenerateMonoscopicStencilDoublePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FGenerateMonoscopicStencilDoublePS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		//return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
		return true;
	}

	FGenerateMonoscopicStencilDoublePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);

		DepthTextureParameter.Bind(Initializer.ParameterMap, TEXT("DepthTexture"));
		DepthTextureParameterSampler.Bind(Initializer.ParameterMap, TEXT("DepthTextureSampler"));

		LeftViewWidthParameter.Bind(Initializer.ParameterMap, TEXT("LeftViewWidth"));
		OffsetWidthParameter.Bind(Initializer.ParameterMap, TEXT("OffsetWidth"));
		MonoZCullingParameter.Bind(Initializer.ParameterMap, TEXT("MonoZCulling"));
	}
	FGenerateMonoscopicStencilDoublePS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, float LeftViewWidth, float OffsetWidth)
	{
		FGlobalShader::SetParameters(RHICmdList, GetPixelShader(), View);
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		//SceneContext.ResolveSceneDepthTexture(RHICmdList);

		FSamplerStateRHIRef Filter = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		SetTextureParameter(RHICmdList, GetPixelShader(), DepthTextureParameter, DepthTextureParameterSampler, Filter, View.Family->RenderTarget->GetRenderTargetTexture());

		//SetTextureParameter(RHICmdList, GetPixelShader(), DepthTextureParameter, DepthTextureParameterSampler, Filter, SceneContext.GetSceneDepthSurface());

		SetShaderValue(RHICmdList, GetPixelShader(), LeftViewWidthParameter, LeftViewWidth);
		SetShaderValue(RHICmdList, GetPixelShader(), OffsetWidthParameter, OffsetWidth);
		SetShaderValue(RHICmdList, GetPixelShader(), MonoZCullingParameter, View.MaxZViewport);

		SceneTextureParameters.Set(RHICmdList, GetPixelShader(), View);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DepthTextureParameter;
		Ar << DepthTextureParameterSampler;
		Ar << LeftViewWidthParameter;
		Ar << OffsetWidthParameter;
		Ar << MonoZCullingParameter;
		Ar << SceneTextureParameters;
		return bShaderHasOutdatedParameters;
	}

	FShaderResourceParameter DepthTextureParameter;
	FShaderResourceParameter DepthTextureParameterSampler;

	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderParameter LeftViewWidthParameter;
	FShaderParameter OffsetWidthParameter;
	FShaderParameter MonoZCullingParameter;
};

IMPLEMENT_SHADER_TYPE(, FGenerateMonoscopicStencilDoublePS, TEXT("MonoscopicRendering"), TEXT("GenerateMonoscopicStencilDouble"), SF_Pixel);

FGlobalBoundShaderState GenerateMonoscopicStencilDoubleBoundShaderState;

/** Pixel Shader to composite monoscopic view into the stereo buffers  */
class FGenerateMonoscopicStencilPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FGenerateMonoscopicStencilPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		//return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
		return true;
	}

	FGenerateMonoscopicStencilPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);

		MonoDepthTextureParameter.Bind(Initializer.ParameterMap, TEXT("MonoStencilTexture"));

	}
	FGenerateMonoscopicStencilPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FGlobalShader::SetParameters(RHICmdList, GetPixelShader(), View);
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		SetSRVParameter(RHICmdList, GetPixelShader(), MonoDepthTextureParameter, SceneContext.SceneStencilSRV);

		SceneTextureParameters.Set(RHICmdList, GetPixelShader(), View);

	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << MonoDepthTextureParameter;
		Ar << SceneTextureParameters;
		return bShaderHasOutdatedParameters;
	}

	FShaderResourceParameter MonoDepthTextureParameter;
	FSceneTextureShaderParameters SceneTextureParameters;
};

IMPLEMENT_SHADER_TYPE(, FGenerateMonoscopicStencilPS, TEXT("MonoscopicRendering"), TEXT("GenerateMonoscopicStencil"), SF_Pixel);

FGlobalBoundShaderState GenerateMonoscopicStencilBoundShaderState;

void FSceneRenderer::ClearStereoDepthBuffers(FRHICommandListImmediate& RHICmdList)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	if (ViewFamily.MonoParameters.MonoMode != eMonoOff)
	{
		SceneContext.BeginRenderingSceneColor(RHICmdList);

		float ZClear = ViewFamily.MonoParameters.MonoDepthClip;

		auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

		TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
		TShaderMapRef<FClearReplacementVS> PixelShader(ShaderMap);

		extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

		RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA>::GetRHI());
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_Always>::GetRHI());
		RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
		RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
		RHICmdList.SetViewport(Views[0].ViewRect.Min.X, Views[0].ViewRect.Min.Y, ZClear, Views[1].ViewRect.Max.X, Views[1].ViewRect.Max.Y, ZClear);
		DrawRectangle(RHICmdList, 0, 0, Views[1].ViewRect.Max.X, Views[1].ViewRect.Max.Y, 0, 0, Views[1].ViewRect.Max.X, Views[1].ViewRect.Max.Y, Views[1].ViewRect.Max, Views[1].ViewRect.Max, *VertexShader);
	}
}

void FSceneRenderer::GenerateMonoStencil(FRHICommandListImmediate& RHICmdList)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	SceneContext.BeginRenderingSceneMonoColor(RHICmdList, ESimpleRenderTargetMode::EClearColorAndDepth);
	
	if (ViewFamily.MonoParameters.MonoMode == eMonoOff)
	{
		return;
	}

	FViewInfo& LeftView = Views[0];
	FViewInfo& RightView = Views[1];
	FViewInfo& MonoView = Views[2];
	int monowidthdifference = MonoView.ViewRect.Width() - LeftView.ViewRect.Width();
	int offset = round(ViewFamily.MonoParameters.MonoLateralOffset * MonoView.ViewRect.Width());

	float ZClear = ViewFamily.MonoParameters.MonoMonoDepthClip;

	if (monowidthdifference != 0)
	{

		exit(1);
		TShaderMapRef<FScreenVS> ScreenVertexShader(MonoView.ShaderMap);
		TShaderMapRef<FGenerateMonoscopicStencilPS> PixelShader(MonoView.ShaderMap);

		extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

		SetGlobalBoundShaderState(RHICmdList, FeatureLevel, GenerateMonoscopicStencilBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *ScreenVertexShader, *PixelShader);

		RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA>::GetRHI());
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
			true, CF_Always
		>::GetRHI());
		RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
		RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
		PixelShader->SetParameters(RHICmdList, MonoView);

		RHICmdList.SetViewport(MonoView.ViewRect.Min.X, MonoView.ViewRect.Min.Y, ZClear, MonoView.ViewRect.Min.X + monowidthdifference, MonoView.ViewRect.Min.Y + LeftView.ViewRect.Size().Y, ZClear);

		DrawRectangle(
			RHICmdList,
			0, 0,
			monowidthdifference, LeftView.ViewRect.Height(),
			0, 0,
			monowidthdifference, LeftView.ViewRect.Height(),
			FIntPoint(monowidthdifference, LeftView.ViewRect.Height()),
			SceneContext.GetBufferSizeXY(),
			*ScreenVertexShader,
			EDRF_UseTriangleOptimization);

		RHICmdList.SetViewport(MonoView.ViewRect.Max.X - monowidthdifference, MonoView.ViewRect.Min.Y, ZClear, MonoView.ViewRect.Max.X, MonoView.ViewRect.Max.Y, ZClear);

		DrawRectangle(
			RHICmdList,
			0, 0,
			monowidthdifference, LeftView.ViewRect.Height(),
			RightView.ViewRect.Max.X - monowidthdifference, 0,
			monowidthdifference, RightView.ViewRect.Height(),
			FIntPoint(monowidthdifference, RightView.ViewRect.Height()),
			SceneContext.GetBufferSizeXY(),
			*ScreenVertexShader,
			EDRF_UseTriangleOptimization);

	}

	TShaderMapRef<FScreenVS> ScreenVertexShaderDouble(MonoView.ShaderMap);
	TShaderMapRef<FGenerateMonoscopicStencilDoublePS> PixelShaderDouble(MonoView.ShaderMap);
	SetGlobalBoundShaderState(RHICmdList, FeatureLevel, GenerateMonoscopicStencilDoubleBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *ScreenVertexShaderDouble, *PixelShaderDouble);
	RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA>::GetRHI());
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
		true, CF_Always
	>::GetRHI());
	RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
	PixelShaderDouble->SetParameters(RHICmdList, MonoView, (float)(RightView.ViewRect.Min.X - LeftView.ViewRect.Min.X) / SceneContext.GetBufferSizeXY().X, (float)offset / SceneContext.GetBufferSizeXY().X);

	RHICmdList.SetViewport(MonoView.ViewRect.Min.X + monowidthdifference, MonoView.ViewRect.Min.Y, ZClear, MonoView.ViewRect.Max.X - monowidthdifference, MonoView.ViewRect.Max.Y, ZClear);

	DrawRectangle(
		RHICmdList,
		0, 0,
		MonoView.ViewRect.Width() - monowidthdifference * 2, MonoView.ViewRect.Height(),
		LeftView.ViewRect.Min.X + monowidthdifference, LeftView.ViewRect.Min.Y,
		LeftView.ViewRect.Width() - monowidthdifference, LeftView.ViewRect.Height(),
		FIntPoint(MonoView.ViewRect.Width() - monowidthdifference * 2, MonoView.ViewRect.Height()),
		SceneContext.GetBufferSizeXY(),
		*ScreenVertexShaderDouble,
		EDRF_UseTriangleOptimization);

}

void FSceneRenderer::RenderMonoCompositor(FRHICommandListImmediate& RHICmdList)
{
	//return;
	if (ViewFamily.MonoParameters.MonoMode == eMonoOff || ViewFamily.MonoParameters.MonoMode == eMonoStereoOnly || ViewFamily.MonoParameters.MonoMode == eMonoStereoNoCulling) {
		return;
	}

	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.CompositeMonoDepth"));
	const bool bCompositeDepth = CVar ? (CVar->GetValueOnGameThread() != false) : false;

	FViewInfo& LeftView = Views[0];
	FViewInfo& RightView = Views[1];
	FViewInfo& MonoView = Views[2];

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	TShaderMapRef<FScreenVS> ScreenVertexShader(MonoView.ShaderMap);
	TShaderMapRef<FCompositeMonoscopicViewPS> PixelShader(MonoView.ShaderMap);
	TShaderMapRef<FCompositeMonoscopicViewPS> PixelShaderNoDepth(MonoView.ShaderMap);

	extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

	SetGlobalBoundShaderState(RHICmdList, FeatureLevel, CompositeMonoscopicViewBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *ScreenVertexShader, bCompositeDepth ? *PixelShader : *PixelShaderNoDepth);
	RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_InverseDestAlpha, BF_One>::GetRHI());
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
		false, CF_Always>::GetRHI());
	
	if (bCompositeDepth)
	{
		/*RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
			false, CF_Always,
			true, CF_NotEqual, SO_Keep, SO_Keep, SO_Keep,
			false, CF_NotEqual, SO_Keep, SO_Keep, SO_Keep,
			STENCIL_OBJECT_WRITTEN_MASK(1), STENCIL_OBJECT_WRITTEN_MASK(1)
		>::GetRHI(), STENCIL_OBJECT_WRITTEN_MASK(1));*/
	} 
	else
	{
		/*RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
			false, CF_Always,
			true, CF_NotEqual, SO_Keep, SO_Keep, SO_Keep,
			false, CF_NotEqual, SO_Keep, SO_Keep, SO_Keep,
			STENCIL_OBJECT_WRITTEN_MASK(1), STENCIL_OBJECT_WRITTEN_MASK(1)
		>::GetRHI(), STENCIL_OBJECT_WRITTEN_MASK(1));*/
	}

	if (ViewFamily.MonoParameters.MonoMode == eMonoMono)
	{
		RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero>::GetRHI());
	}
	
	RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

	if (bCompositeDepth)
	{
		PixelShader->SetParameters(RHICmdList, MonoView);
	} 
	else
	{
		PixelShaderNoDepth->SetParameters(RHICmdList, MonoView);
	}

	//composite onto left eye
	RHICmdList.SetViewport(LeftView.ViewRect.Min.X, LeftView.ViewRect.Min.Y, 0.0f, LeftView.ViewRect.Max.X, LeftView.ViewRect.Max.Y, 1.0f);

	int offset = round(ViewFamily.MonoParameters.MonoLateralOffset * MonoView.ViewRect.Width());

	DrawRectangle(
		RHICmdList,
		0, 0,
		LeftView.ViewRect.Width(), LeftView.ViewRect.Height(),
		MonoView.ViewRect.Min.X + offset , MonoView.ViewRect.Min.Y,
		LeftView.ViewRect.Width(), LeftView.ViewRect.Height(),
		LeftView.ViewRect.Size(),
		MonoView.ViewRect.Max,
		*ScreenVertexShader,
		EDRF_UseTriangleOptimization);

	//composite onto right eye
	RHICmdList.SetViewport(RightView.ViewRect.Min.X, RightView.ViewRect.Min.Y, 0.0f, RightView.ViewRect.Max.X, RightView.ViewRect.Max.Y, 1.0f);
	int monowidthdifference = MonoView.ViewRect.Width() - LeftView.ViewRect.Width();
	DrawRectangle(
		RHICmdList,
		0, 0,
		LeftView.ViewRect.Width(), LeftView.ViewRect.Height(),
		MonoView.ViewRect.Min.X + monowidthdifference - offset, MonoView.ViewRect.Min.Y,
		LeftView.ViewRect.Width(), LeftView.ViewRect.Height(),
		LeftView.ViewRect.Size(),
		MonoView.ViewRect.Max,
		*ScreenVertexShader,
		EDRF_UseTriangleOptimization);


	TRefCountPtr<IPooledRenderTarget> VelocityRT;
	VelocityRT = SceneContext.GetGBufferVelocityRT();

	GRenderTargetPool.PresentContent(RHICmdList, Views[2]);
	Views.RemoveAt(2);
	ViewFamily.Views.RemoveAt(2);

//	GetViewFrustumBounds(LeftView.ViewFrustum, LeftView.ViewProjectionMatrix, false);
//	GetViewFrustumBounds(RightView.ViewFrustum, RightView.ViewProjectionMatrix, false);
}
