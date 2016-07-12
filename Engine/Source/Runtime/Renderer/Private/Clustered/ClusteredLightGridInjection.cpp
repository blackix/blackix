// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ClusteredLightInjection.cpp: builds the light grid
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"

#include "Clustered/ClusteredShadingRenderer.h"


/** Injects lights into the clustered light grid */
class FLightGridInjectionCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FLightGridInjectionCS, Global)

public:
	enum
	{
		kGroupSizeX = 8,
		kGroupSizeY = 8,
		kGroupSizeZ = 2,
	};


	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		ClusteredShadingShaderCommon::ModifyCompilationEnvironment(Platform, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("GROUP_SIZE_X"), kGroupSizeX);
		OutEnvironment.SetDefine(TEXT("GROUP_SIZE_Y"), kGroupSizeY);
		OutEnvironment.SetDefine(TEXT("GROUP_SIZE_Z"), kGroupSizeZ);

		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	FLightGridInjectionCS()
	{
	}

	FLightGridInjectionCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		GridSize.Bind(Initializer.ParameterMap, TEXT("GridSize"));
		InvGridSize.Bind(Initializer.ParameterMap, TEXT("InvGridSize"));
		LightCount.Bind(Initializer.ParameterMap, TEXT("LightCount"));
		InvFrameSize.Bind(Initializer.ParameterMap, TEXT("InvFrameSize"));
		NearClipDistance.Bind(Initializer.ParameterMap, TEXT("NearClipDistance"));
		FrustumCornersNear.Bind(Initializer.ParameterMap, TEXT("FrustumCornersNear"));
		InvLightGridZParams.Bind(Initializer.ParameterMap, TEXT("InvLightGridZParams"));
		LightViewPositionAndRadius.Bind(Initializer.ParameterMap, TEXT("LightViewPositionAndRadius"));
		LightDirectionAndDirMask.Bind(Initializer.ParameterMap, TEXT("LightDirectionAndDirMask"));
		LightSpotAnglesAndSpotMask.Bind(Initializer.ParameterMap, TEXT("LightSpotAnglesAndSpotMask"));
		OutputOrigin.Bind(Initializer.ParameterMap, TEXT("GridOutputOrigin"));
		LightGridRW.Bind(Initializer.ParameterMap, TEXT("LightGrid"));
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		const FClusteredLightsSceneInfo& LightInfo, 
		const FUnorderedAccessViewRHIRef& LightGridUAV,
		const FIntVector& InGridSize,
		const FIntVector& InOutputOrigin)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		// Build the light data
		FVector4 lightViewPositionAndRadius[MAX_CLUSTERED_FORWARD_LIGHTS];
		FVector4 lightDirectionAndDirectionalMask[MAX_CLUSTERED_FORWARD_LIGHTS];
		FVector4 lightSpotAnglesAndSpotMask[MAX_CLUSTERED_FORWARD_LIGHTS];

		memset(lightSpotAnglesAndSpotMask, 0, sizeof(lightSpotAnglesAndSpotMask));

		for (int32 i = 0; i < LightInfo.ClusteredLights.Num(); ++i)
		{
			const FLightSceneInfoCompact& InfoCompact = LightInfo.ClusteredLights[i];
			const FLightSceneInfo* const Info = InfoCompact.LightSceneInfo;
			const ELightComponentType LightType = (const ELightComponentType)InfoCompact.LightType;

			// Don't support spots yet
			check(LightType == LightType_Point 
				|| LightType == LightType_Spot 
				|| LightType == LightType_Directional);

			// Convert to view space
			FVector const Position = View.ViewMatrices.ViewMatrix.TransformPosition(Info->Proxy->GetPosition());
			float const Radius = Info->Proxy->GetRadius();
			lightViewPositionAndRadius[i] = FVector4(Position, Radius);

			FVector4 PositionAndInvRadius, ColorAndFalloffExponent;
			FVector NormalizedLightDirection;
			FVector2D SpotAngles;
			float SourceRadius, SourceLength, MinRoughness;
			Info->Proxy->GetParameters(
				PositionAndInvRadius, 
				ColorAndFalloffExponent, 
				NormalizedLightDirection,
				SpotAngles,
				SourceRadius,
				SourceLength,
				MinRoughness);

			NormalizedLightDirection = View.ViewMatrices.ViewMatrix.TransformVector(NormalizedLightDirection).GetSafeNormal();
			lightDirectionAndDirectionalMask[i] = FVector4(-NormalizedLightDirection, LightType == LightType_Directional);

			if (LightType == LightType_Spot)
			{
				float const CosOuterCone = SpotAngles.X;
				float const SinOuterCone = FMath::Sqrt(1.f - CosOuterCone*CosOuterCone);

				lightSpotAnglesAndSpotMask[i] = FVector4(
					SinOuterCone * Radius, 
					CosOuterCone * Radius,
					0.f, 
					1.f
				);
			}
		}

		// Figure out the position of all the near plane frustum corners, in the translated world space
		// We'll translate the lights accordingly in the CS to account for the new origin.
		FVector4 viewCorners[4];
		FVector4 ndcCorners[4] ={
			FVector4(-1, +1, 1, 1),
			FVector4(+1, +1, 1, 1),
			FVector4(+1, -1, 1, 1),
			FVector4(-1, -1, 1, 1),
		};

		FMatrix const InvProjMatrix = View.ViewMatrices.GetInvProjNoAAMatrix();
		for (unsigned i = 0; i < 4; ++i)
		{
			FVector4 Corner = InvProjMatrix.TransformFVector4(ndcCorners[i]);
			viewCorners[i] = Corner / Corner.W;
		}

		SetShaderValue(RHICmdList, ComputeShaderRHI, GridSize, InGridSize);
		SetShaderValue(RHICmdList, ComputeShaderRHI, InvGridSize, FVector(1.f/InGridSize.X, 1.f/InGridSize.Y, 1.f/InGridSize.Z));
		SetShaderValue(RHICmdList, ComputeShaderRHI, LightCount, LightInfo.ClusteredLights.Num());
		SetShaderValue(RHICmdList, ComputeShaderRHI, InvFrameSize, FVector2D(1.f/View.ViewRect.Size().X, 1.f/View.ViewRect.Size().Y));
		SetShaderValue(RHICmdList, ComputeShaderRHI, InvLightGridZParams, FVector(1.f / LightInfo.LightGridZParams.X, -LightInfo.LightGridZParams.Y, 1.f / LightInfo.LightGridZParams.Z));
		SetShaderValue(RHICmdList, ComputeShaderRHI, NearClipDistance, View.NearClippingDistance);
		SetShaderValue(RHICmdList, ComputeShaderRHI, OutputOrigin, InOutputOrigin);

		SetShaderValueArray(RHICmdList, ComputeShaderRHI, FrustumCornersNear, viewCorners, 4);
		SetShaderValueArray(RHICmdList, ComputeShaderRHI, LightViewPositionAndRadius, lightViewPositionAndRadius, LightInfo.ClusteredLights.Num());
		SetShaderValueArray(RHICmdList, ComputeShaderRHI, LightDirectionAndDirMask, lightDirectionAndDirectionalMask, LightInfo.ClusteredLights.Num());
		SetShaderValueArray(RHICmdList, ComputeShaderRHI, LightSpotAnglesAndSpotMask, lightSpotAnglesAndSpotMask, LightInfo.ClusteredLights.Num());

		RHICmdList.SetUAVParameter(ComputeShaderRHI, LightGridRW.GetBaseIndex(), LightGridUAV);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		RHICmdList.SetUAVParameter(ComputeShaderRHI, LightGridRW.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar  << GridSize
			<< InvGridSize
			<< InvFrameSize 
			<< NearClipDistance
			<< FrustumCornersNear 
			<< InvLightGridZParams
			<< LightCount
			<< LightViewPositionAndRadius 
			<< LightDirectionAndDirMask
			<< LightSpotAnglesAndSpotMask
			<< LightGridRW
			<< OutputOrigin;

		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter GridSize;
	FShaderParameter InvGridSize;

	FShaderParameter InvFrameSize;
	FShaderParameter NearClipDistance;
	FShaderParameter FrustumCornersNear;
	FShaderParameter LightCount;
	FShaderParameter LightViewPositionAndRadius;
	FShaderParameter LightDirectionAndDirMask;
	FShaderParameter LightSpotAnglesAndSpotMask;
	FShaderParameter InvLightGridZParams;
	FShaderParameter OutputOrigin;

	FShaderResourceParameter LightGridRW;
};


IMPLEMENT_SHADER_TYPE(,FLightGridInjectionCS,TEXT("ClusteredLightGridInjection"),TEXT("ClusteredLightGridInjectionCS"),SF_Compute);


void FClusteredForwardShadingSceneRenderer::InjectLightsIntoLightGrid(FRHICommandListImmediate& RHICmdList)
{
	if (FeatureLevel < ERHIFeatureLevel::SM5)
	{
		return;
	}

	// Check if we have zero lights and can skip the compute shader.
	bool hasLights = false;
	for (const auto& LightInfo : ClusteredLightInfo)
	{
		hasLights |= LightInfo.ClusteredLights.Num() != 0;
	}

	if (!hasLights)
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, InjectLightsToGrid);

	bool const isInstancedStereo = Views.Num() == 2 && !Views[1].ShouldRenderView();

	TShaderMapRef<FLightGridInjectionCS> LightGridCS(GetGlobalShaderMap(FeatureLevel));
	RHICmdList.SetComputeShader(LightGridCS->GetComputeShader());

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		const FViewInfo& View = Views[ViewIndex];
		const FIntPoint& ViewOrigin = View.ViewRect.Min;
		const FClusteredLightsSceneInfo& LightInfo = ClusteredLightInfo[ViewIndex];

		FIntVector ViewGridSize = CalcLightGridSize(View.ViewRect.Size());
		uint32 GroupsX = (ViewGridSize.X + FLightGridInjectionCS::kGroupSizeX-1) / FLightGridInjectionCS::kGroupSizeX;
		uint32 GroupsY = (ViewGridSize.Y + FLightGridInjectionCS::kGroupSizeY-1) / FLightGridInjectionCS::kGroupSizeY;
		uint32 GroupsZ = (ViewGridSize.Z + FLightGridInjectionCS::kGroupSizeZ-1) / FLightGridInjectionCS::kGroupSizeZ;

		// In instanced stereo, we use a single light grid for the entire view.  Each dispatch writes out to the corresponding
		// section of the grid for each eye, and due to this we must check that the origin is aligned correctly.
		FIntVector OutputOrigin(ViewOrigin.X/kLightGridTileSizeX, ViewOrigin.Y/kLightGridTileSizeY, 0);
		// We allow single views to have offset origins, but otherwise all the rules apply.
		check(Views.Num() == 1 || ((ViewOrigin.X % kLightGridTileSizeX) == 0 && (ViewOrigin.Y % kLightGridTileSizeY) == 0));
		// We also want to make sure the origin is at (0,0) when there are multiple views.
		check(Views.Num() == 1 || Views[0].ViewRect.Min == FIntPoint(0, 0));

		LightGridCS->SetParameters(RHICmdList, Views[ViewIndex], LightInfo, SceneContext.ClusteredLightGridUAV, ViewGridSize, OutputOrigin);
		RHICmdList.DispatchComputeShader(GroupsX, GroupsY, GroupsZ);
	}

	LightGridCS->UnsetParameters(RHICmdList);
}