// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LayersCommon.h"

TSharedPtr<FLayerUTextureResource> FLayerUTextureResource::NullResource = MakeShareable(new FLayerUTextureResource(nullptr));

FLayerUTextureResource::FLayerUTextureResource(UTexture* InTexture)
	: Proxy(new FSlateShaderResourceProxy)
	, TextureObject(InTexture)
{
	if (TextureObject)
	{
		Proxy->ActualSize = FIntPoint(InTexture->GetSurfaceWidth(), InTexture->GetSurfaceHeight());
		Proxy->Resource = this;
	}
}

FLayerUTextureResource::~FLayerUTextureResource()
{
	if (Proxy)
	{
		delete Proxy;
	}
}

void FLayerUTextureResource::UpdateRenderResource(FTexture* InFTexture)
{
	if (InFTexture)
	{
		// If the RHI data has changed, it's possible the underlying size of the texture has changed,
		// if that's true we need to update the actual size recorded on the proxy as well, otherwise 
		// the texture will continue to render using the wrong size.
		Proxy->ActualSize = FIntPoint(InFTexture->GetSizeX(), InFTexture->GetSizeY());

		ShaderResource = FTexture2DRHIRef(InFTexture->TextureRHI->GetTexture2D());
	}
	else
	{
		ShaderResource = FTexture2DRHIRef();
	}
}

uint32 FLayerUTextureResource::GetWidth() const
{
	return TextureObject->GetSurfaceWidth();
}

uint32 FLayerUTextureResource::GetHeight() const
{
	return TextureObject->GetSurfaceHeight();
}

// ESlateShaderResource::Type FLayerUTextureResource::GetType() const
// {
// 	return ESlateShaderResource::TextureObject;
// }
