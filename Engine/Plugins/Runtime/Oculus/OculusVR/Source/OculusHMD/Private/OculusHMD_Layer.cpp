// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_Layer.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
//#include "MediaTexture.h"
//#include "ScreenRendering.h"
//#include "ScenePrivate.h"
//#include "PostProcess/SceneFilterRendering.h"


namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FOvrpLayer
//-------------------------------------------------------------------------------------------------

FOvrpLayer::FOvrpLayer(uint32 InOvrpLayerId) : 
	OvrpLayerId(InOvrpLayerId)
{
}


FOvrpLayer::~FOvrpLayer()
{
	if (InRenderThread())
	{
		ExecuteOnRHIThread_DoNotWait([this]()
		{
			ovrp_DestroyLayer(OvrpLayerId);
		});
	}
	else
	{
		ovrp_DestroyLayer(OvrpLayerId);
	}
}


//-------------------------------------------------------------------------------------------------
// FLayer
//-------------------------------------------------------------------------------------------------

FLayer::FLayer(uint32 InId, const IStereoLayers::FLayerDesc& InDesc) :
	Id(InId),
	Desc(InDesc),
	OvrpLayerId(0),
	bUpdateTexture(false)
{
	FMemory::Memzero(OvrpLayerDesc);
	FMemory::Memzero(OvrpLayerSubmit);
}


FLayer::FLayer(const FLayer& Layer) :
	Id(Layer.Id),
	Desc(Layer.Desc),
	OvrpLayerId(Layer.OvrpLayerId),
	OvrpLayer(Layer.OvrpLayer),
	TextureSetProxy(Layer.TextureSetProxy),
	RightTextureSetProxy(Layer.RightTextureSetProxy),
	bUpdateTexture(Layer.bUpdateTexture)
{
	FMemory::Memcpy(&OvrpLayerDesc, &Layer.OvrpLayerDesc, sizeof(OvrpLayerDesc));
	FMemory::Memcpy(&OvrpLayerSubmit, &Layer.OvrpLayerSubmit, sizeof(OvrpLayerSubmit));
}


FLayer::~FLayer()
{
}


void FLayer::SetDesc(const IStereoLayers::FLayerDesc& InDesc)
{
	if (Desc.Texture != InDesc.Texture || Desc.LeftTexture != InDesc.LeftTexture)
	{
		bUpdateTexture = true;
	}

	Desc = InDesc;
}


void FLayer::SetEyeLayerDesc(const ovrpLayerDesc_EyeFov& InEyeLayerDesc, const ovrpRecti InViewportRect[ovrpEye_Count])
{
	OvrpLayerDesc.EyeFov = InEyeLayerDesc;

	for(int eye = 0; eye < ovrpEye_Count; eye++)
	{
		OvrpLayerSubmit.ViewportRect[eye] = InViewportRect[eye];
	}
}


TSharedPtr<FLayer, ESPMode::ThreadSafe> FLayer::Clone() const
{
	return MakeShareable(new FLayer(*this));
}


void FLayer::Initialize_RenderThread(FCustomPresent* CustomPresent, const FLayer* InLayer)
{
	CheckInRenderThread();

	if (Id == 0)
	{
		// OvrpLayerDesc and OvrpViewportRects already initialized
	}
	else if (Desc.Texture.IsValid())
	{
		FRHITexture2D* Texture2D = Desc.Texture->GetTexture2D();
		FRHITextureCube* TextureCube = Desc.Texture->GetTextureCube();

		uint32 SizeX, SizeY;

		if(Texture2D)
		{
			SizeX = Texture2D->GetSizeX();
			SizeY = Texture2D->GetSizeY();
		}
		else if(TextureCube)
		{
			SizeX = SizeY = TextureCube->GetSize();
		}
		else
		{
			return;
		}

		ovrpShape Shape;
		
		switch (Desc.ShapeType)
		{
		case IStereoLayers::QuadLayer:
			Shape = ovrpShape_Quad;
			break;

		case IStereoLayers::CylinderLayer:
			Shape = ovrpShape_Cylinder;
			break;

		case IStereoLayers::CubemapLayer:
			Shape = ovrpShape_Cubemap;
			break;

		default:
			return;
		}

		EPixelFormat Format = CustomPresent->GetPixelFormat(Desc.Texture->GetFormat());
#if PLATFORM_ANDROID
		uint32 NumMips = 1;
#else
		uint32 NumMips = 0;
#endif
		uint32 NumSamples = 1;
		bool bSRGB = true;
		int LayerFlags = 0;

		if(!(Desc.Flags & IStereoLayers::LAYER_FLAG_TEX_CONTINUOUS_UPDATE))
			LayerFlags |= ovrpLayerFlag_Static;

		// Calculate layer desc
		ovrp_CalculateLayerDesc(
			Shape,
			!Desc.LeftTexture.IsValid() ? ovrpLayout_Mono : ovrpLayout_Stereo,
			ovrpSizei { (int) SizeX, (int) SizeY },
			NumMips,
			NumSamples,
			CustomPresent->GetOvrpTextureFormat(Format, bSRGB),
			LayerFlags,
			&OvrpLayerDesc);

		// Calculate viewport rect
		for (uint32 EyeIndex = 0; EyeIndex < ovrpEye_Count; EyeIndex++)
		{
			ovrpRecti& ViewportRect = OvrpLayerSubmit.ViewportRect[EyeIndex];
			ViewportRect.Pos.x = (int)(Desc.UVRect.Min.X * SizeX + 0.5f);
			ViewportRect.Pos.y = (int)(Desc.UVRect.Min.Y * SizeY + 0.5f);
			ViewportRect.Size.w = (int)(Desc.UVRect.Max.X * SizeX + 0.5f) - ViewportRect.Pos.x;
			ViewportRect.Size.h = (int)(Desc.UVRect.Max.Y * SizeY + 0.5f) - ViewportRect.Pos.y;
		}
	}
	else
	{
		return;
	}
	
	// Reuse/Create texture set
	if (InLayer && InLayer->OvrpLayer.IsValid() && !FMemory::Memcmp(&OvrpLayerDesc, &InLayer->OvrpLayerDesc, sizeof(OvrpLayerDesc)))
	{
		OvrpLayerId = InLayer->OvrpLayerId;
		OvrpLayer = InLayer->OvrpLayer;
		TextureSetProxy = InLayer->TextureSetProxy;
		RightTextureSetProxy = InLayer->RightTextureSetProxy;
		bUpdateTexture = InLayer->bUpdateTexture;
	}
	else
	{
		bool bLayerCreated = false;
		TArray<ovrpTextureHandle> Textures;
		TArray<ovrpTextureHandle> RightTextures;

		ExecuteOnRHIThread([&]()
		{
			// UNDONE Do this in RenderThread once OVRPlugin allows ovrp_SetupLayer to be called asynchronously
			int32 TextureCount;
			if (OVRP_SUCCESS(ovrp_SetupLayer(CustomPresent->GetOvrpDevice(), OvrpLayerDesc.Base, (int*) &OvrpLayerId)) &&
				OVRP_SUCCESS(ovrp_GetLayerTextureStageCount(OvrpLayerId, &TextureCount)))
			{
				// Left
				{
					Textures.SetNum(TextureCount);

					for (int32 TextureIndex = 0; TextureIndex < TextureCount; TextureIndex++)
					{
						ovrp_GetLayerTexture(OvrpLayerId, TextureIndex, ovrpEye_Left, &Textures[TextureIndex]);
					}
				}

				// Right
				if(OvrpLayerDesc.Layout == ovrpLayout_Stereo)
				{
					RightTextures.SetNum(TextureCount);

					for (int32 TextureIndex = 0; TextureIndex < TextureCount; TextureIndex++)
					{
						ovrp_GetLayerTexture(OvrpLayerId, TextureIndex, ovrpEye_Right, &RightTextures[TextureIndex]);
					}
				}

				bLayerCreated = true;
			}
		});

		if(bLayerCreated)
		{
			OvrpLayer = MakeShareable<FOvrpLayer>(new FOvrpLayer(OvrpLayerId));

			uint32 SizeX = OvrpLayerDesc.TextureSize.w;
			uint32 SizeY = OvrpLayerDesc.TextureSize.h;
			EPixelFormat Format = CustomPresent->GetPixelFormat(OvrpLayerDesc.Format);
			uint32 NumMips = OvrpLayerDesc.MipLevels;
			uint32 NumSamples = OvrpLayerDesc.SampleCount;

			TextureSetProxy = CustomPresent->CreateTextureSet_RenderThread(SizeX, SizeY, Format, NumMips, NumSamples, 1, Textures);

			if(OvrpLayerDesc.Layout == ovrpLayout_Stereo)
			{
				RightTextureSetProxy = CustomPresent->CreateTextureSet_RenderThread(SizeX, SizeY, Format, NumMips, NumSamples, 1, RightTextures);
			}
		}

		bUpdateTexture = true;
	}

	if (Desc.Flags & IStereoLayers::LAYER_FLAG_TEX_CONTINUOUS_UPDATE)
	{
		bUpdateTexture = true;
	}
}

void FLayer::UpdateTexture_RenderThread(FCustomPresent* CustomPresent, FRHICommandListImmediate& RHICmdList)
{
	CheckInRenderThread();

	if (bUpdateTexture && TextureSetProxy.IsValid())
	{
		// Copy textures
		if (Desc.Texture.IsValid())
		{
			bool bAlphaPremultiply = true;
			bool bNoAlphaWrite = (Desc.Flags & IStereoLayers::LAYER_FLAG_TEX_NO_ALPHA_CHANNEL) != 0;

			// Left
			{
				FRHITexture2D* SrcTexture = Desc.LeftTexture.IsValid() ? Desc.LeftTexture->GetTexture2D() : Desc.Texture->GetTexture2D();
				FRHITexture2D* DstTexture = TextureSetProxy->GetTexture2D();

				const ovrpRecti& OvrpViewportRect = OvrpLayerSubmit.ViewportRect[ovrpEye_Left];
				FIntRect DstRect(OvrpViewportRect.Pos.x, OvrpViewportRect.Pos.y, OvrpViewportRect.Pos.x + OvrpViewportRect.Size.w, OvrpViewportRect.Pos.y + OvrpViewportRect.Size.h);

				CustomPresent->CopyTexture_RenderThread(RHICmdList, DstTexture, SrcTexture, SrcTexture->GetSizeX(), SrcTexture->GetSizeY(), DstRect, FIntRect(), bAlphaPremultiply, bNoAlphaWrite);
			}

			// Right
			if(OvrpLayerDesc.Layout != ovrpLayout_Mono)
			{
				FRHITexture2D* SrcTexture = Desc.Texture->GetTexture2D();
				FRHITexture2D* DstTexture = RightTextureSetProxy.IsValid() ? RightTextureSetProxy->GetTexture2D() : TextureSetProxy->GetTexture2D();

				const ovrpRecti& OvrpViewportRect = OvrpLayerSubmit.ViewportRect[ovrpEye_Right];
				FIntRect DstRect(OvrpViewportRect.Pos.x, OvrpViewportRect.Pos.y, OvrpViewportRect.Pos.x + OvrpViewportRect.Size.w, OvrpViewportRect.Pos.y + OvrpViewportRect.Size.h);

				CustomPresent->CopyTexture_RenderThread(RHICmdList, DstTexture, SrcTexture, SrcTexture->GetSizeX(), SrcTexture->GetSizeY(), DstRect, FIntRect(), bAlphaPremultiply, bNoAlphaWrite);
			}

			bUpdateTexture = false;
		}

		// Generate mips
		TextureSetProxy->GenerateMips_RenderThread(RHICmdList);

		if (RightTextureSetProxy.IsValid())
		{
			RightTextureSetProxy->GenerateMips_RenderThread(RHICmdList);
		}
	}
}


const ovrpLayerSubmit* FLayer::UpdateLayer_RHIThread(const FSettings* Settings, const FGameFrame* Frame)
{
	OvrpLayerSubmit.LayerId = OvrpLayerId;
	OvrpLayerSubmit.TextureStage = TextureSetProxy.IsValid() ? TextureSetProxy->GetSwapChainIndex_RHIThread() : 0;

	if (Id != 0)
	{
		int SizeX = OvrpLayerDesc.TextureSize.w;
		int SizeY = OvrpLayerDesc.TextureSize.h;

		float AspectRatio = SizeX ? (float)SizeY / (float)SizeX : 3.0f / 4.0f;
		FVector LocationScaleInv = Frame->WorldToMetersScale * Frame->PositionScale;
		FVector LocationScale = LocationScaleInv.Reciprocal();
		ovrpVector3f Scale = ToOvrpVector3f(Desc.Transform.GetScale3D() * LocationScale);

		switch (OvrpLayerDesc.Shape)
		{
		case ovrpShape_Quad:
			{
				float QuadSizeY = (Desc.Flags & IStereoLayers::LAYER_FLAG_QUAD_PRESERVE_TEX_RATIO) ? Desc.QuadSize.X * AspectRatio : Desc.QuadSize.Y;
				OvrpLayerSubmit.Quad.Size = ovrpSizef { Desc.QuadSize.X * Scale.x, QuadSizeY * Scale.y };
			}
			break;
		case ovrpShape_Cylinder:
			{
				float CylinderHeight = (Desc.Flags & IStereoLayers::LAYER_FLAG_QUAD_PRESERVE_TEX_RATIO) ? Desc.CylinderSize.Y * AspectRatio : Desc.CylinderHeight;
				OvrpLayerSubmit.Cylinder.ArcWidth = Desc.CylinderSize.X;
				OvrpLayerSubmit.Cylinder.Height = CylinderHeight * Scale.y;
				OvrpLayerSubmit.Cylinder.Radius = Desc.CylinderSize.Y;
			}
			break;
		}

		FQuat BaseOrientation;
		FVector BaseLocation;

		switch (Desc.PositionType)
		{
		case IStereoLayers::WorldLocked:
			BaseOrientation = Frame->PlayerOrientation;
			BaseLocation = Frame->PlayerLocation;
			break;

		case IStereoLayers::TrackerLocked:
			BaseOrientation = FQuat::Identity;
			BaseLocation = FVector::ZeroVector;
			break;

		case IStereoLayers::FaceLocked:
			BaseOrientation = Settings->BaseOrientation;
			BaseLocation = Settings->BaseOffset * LocationScaleInv;
			break;
		}

		FTransform playerTransform(BaseOrientation, BaseLocation);

		FQuat Orientation = Desc.Transform.Rotator().Quaternion();
		FVector Location = Desc.Transform.GetLocation();

		OvrpLayerSubmit.Pose.Orientation = ToOvrpQuatf(BaseOrientation.Inverse() * Orientation);
		OvrpLayerSubmit.Pose.Position = ToOvrpVector3f((playerTransform.InverseTransformPosition(Location)) * LocationScale);
		OvrpLayerSubmit.LayerSubmitFlags = 0;

		if (Desc.PositionType == IStereoLayers::FaceLocked)
		{
			OvrpLayerSubmit.LayerSubmitFlags |= ovrpLayerSubmitFlag_HeadLocked;
		}
	}

	return &OvrpLayerSubmit.Base;
}


void FLayer::IncrementSwapChainIndex_RHIThread()
{
	CheckInRHIThread();

	if (TextureSetProxy.IsValid())
	{
		TextureSetProxy->IncrementSwapChainIndex_RHIThread();
	}

	if (RightTextureSetProxy.IsValid())
	{
		RightTextureSetProxy->IncrementSwapChainIndex_RHIThread();
	}
}


void FLayer::ReleaseResources_RHIThread()
{
	CheckInRHIThread();

	OvrpLayerId = 0;
	OvrpLayer.Reset();
	TextureSetProxy.Reset();
	RightTextureSetProxy.Reset();
	bUpdateTexture = false;
}


} // namespace OculusHMD

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS
