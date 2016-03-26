// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class IStereoLayers
{
public:
	virtual uint32 CreateLayer(UTexture2D* InTexture, int32 InPrioirity, bool bInFixedToFace = false) = 0;
	virtual void DestroyLayer(uint32 LayerId) = 0;
	virtual void SetTransform(uint32 LayerId, const FTransform& InTransform) = 0;
	virtual void SetQuadSize(uint32 LayerId, const FVector2D& InSize) = 0;
	virtual void SetTextureViewport(uint32 LayerId, const FBox2D& UVRect) = 0;
};


/** 
 * A proxy resource.  
 *
 * May point to a full resource or point or to a texture resource in an atlas
 * Note: This class does not free any resources.  Resources should be owned and freed elsewhere
 */
class FLayerShaderResourceProxy
{
public:

	/** The start uv of the texture.  If atlased this is some subUV of the atlas, 0,0 otherwise */
	FVector2D StartUV;

	/** The size of the texture in UV space.  If atlas this some sub uv of the atlas.  1,1 otherwise */
	FVector2D SizeUV;

	/** The resource to be used for rendering */
	FSlateShaderResource* Resource;

	/** The size of the texture.  Regardless of atlasing this is the size of the actual texture */
	FIntPoint ActualSize;

	/** Default constructor. */
	FLayerShaderResourceProxy( )
		: StartUV(0.0f, 0.0f)
		, SizeUV(1.0f, 1.0f)
		, Resource(nullptr)
		, ActualSize(0.0f, 0.0f)
	{ }
};


/** 
 * Abstract base class for platform independent texture resource accessible by the shader.
 */
template <typename ResourceType>
class TLayerTexture
	: public FSlateShaderResource
{
public:

	/** Default constructor. */
	TLayerTexture( ) { }

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InShaderResource The resource to use.
	 */
	TLayerTexture( ResourceType& InShaderResource )
		: ShaderResource( InShaderResource )
	{ }

	virtual ~TLayerTexture() { }

public:

	/**
	 * Gets the resource used by the shader.
	 *
	 * @return The resource.
	 */
	ResourceType& GetTypedResource()
	{
		return ShaderResource;
	}

public:


protected:

	// Holds the resource.
	ResourceType ShaderResource;
};

/** A resource for rendering a UTexture object in Slate */
class FLayerUTextureResource : public TLayerTexture<FTexture2DRHIRef>
{
public:
	static TSharedPtr<FLayerUTextureResource> NullResource;

	FLayerUTextureResource(UTexture* InTexture);
	~FLayerUTextureResource();

	/**
	 * Updates the renderng resource with a new UTexture resource
	 */
	void UpdateRenderResource(FTexture* InFTexture);

	/**
	 * FSlateShaderRsourceInterface
	 */
	virtual uint32 GetWidth() const;
	virtual uint32 GetHeight() const;
	//virtual ESlateShaderResource::Type GetType() const;
	
public:
	/** Slate rendering proxy */
	FLayerShaderResourceProxy* Proxy;

	/** Texture UObject.  Note: lifetime is managed externally */
	UTexture* TextureObject;
};
