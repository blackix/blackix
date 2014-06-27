// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Canvas.h: Unreal canvas definition.
=============================================================================*/

#pragma once

#include "BatchedElements.h"
#include "CanvasItem.h"

/**
 * Encapsulates the canvas state.
 */
class FCanvas
{
public:	

	/** 
	 * Enum that describes what type of element we are currently batching.
	 */
	enum EElementType
	{
		ET_Line,
		ET_Triangle,
		ET_MAX
	};

	/**
	 * Enum for canvas features that are allowed
	 **/
	enum ECanvasAllowModes
	{
		// flushing and rendering
		Allow_Flush			= 1<<0,
		// delete the render batches when rendering
		Allow_DeleteOnRender= 1<<1
	};

	/** 
	* Constructor.
	*/
	ENGINE_API FCanvas(FRenderTarget* InRenderTarget,FHitProxyConsumer* InHitProxyConsumer, UWorld* InWorld);

	/** 
	* Constructor. For situations where a world is not available, but time information is
	*/
	ENGINE_API FCanvas(FRenderTarget* InRenderTarget,FHitProxyConsumer* InHitProxyConsumer, float InRealTime, float InWorldTime, float InWorldDeltaTime);

	/** 
	* Destructor.
	*/
	ENGINE_API ~FCanvas();

	
	static ESimpleElementBlendMode BlendToSimpleElementBlend(EBlendMode BlendMode);
	/**
	* Returns a FBatchedElements pointer to be used for adding vertices and primitives for rendering.
	* Adds a new render item to the sort element entry based on the current sort key.
	*
	* @param InElementType - Type of element we are going to draw.
	* @param InBatchedElementParameters - Parameters for this element
	* @param InTexture - New texture that will be set.
	* @param InBlendMode - New blendmode that will be set.
	* @param GlowInfo - info for optional glow effect when using depth field rendering
	* @return Returns a pointer to a FBatchedElements object.
	*/
	ENGINE_API FBatchedElements* GetBatchedElements(EElementType InElementType, FBatchedElementParameters* InBatchedElementParameters=NULL, const FTexture* Texture=NULL, ESimpleElementBlendMode BlendMode=SE_BLEND_MAX,const FDepthFieldGlowInfo& GlowInfo = FDepthFieldGlowInfo());
	
	/**
	* Generates a new FCanvasTileRendererItem for the current sortkey and adds it to the sortelement list of itmes to render
	*/
	void AddTileRenderItem(float X,float Y,float SizeX,float SizeY,float U,float V,float SizeU,float SizeV,const FMaterialRenderProxy* MaterialRenderProxy,FHitProxyId HitProxyId,bool bFreezeTime);

	/** 
	* Sends a message to the rendering thread to draw the batched elements. 
	* @param bForce - force the flush even if Allow_Flush is not enabled
	*/
	ENGINE_API void Flush(bool bForce=false);

	/**
	 * Pushes a transform onto the canvas's transform stack, multiplying it with the current top of the stack.
	 * @param Transform - The transform to push onto the stack.
	 */
	ENGINE_API void PushRelativeTransform(const FMatrix& Transform);

	/**
	 * Pushes a transform onto the canvas's transform stack.
	 * @param Transform - The transform to push onto the stack.
	 */
	ENGINE_API void PushAbsoluteTransform(const FMatrix& Transform);

	/**
	 * Removes the top transform from the canvas's transform stack.
	 */
	ENGINE_API void PopTransform();

	/**
	* Replace the base (ie. TransformStack(0)) transform for the canvas with the given matrix
	*
	* @param Transform - The transform to use for the base
	*/
	void SetBaseTransform(const FMatrix& Transform);

	/**
	* Generate a 2D projection for the canvas. Use this if you only want to transform in 2D on the XY plane
	*
	* @param ViewSizeX - Viewport width
	* @param ViewSizeY - Viewport height
	* @return Matrix for canvas projection
	*/
	ENGINE_API static FMatrix CalcBaseTransform2D(uint32 ViewSizeX, uint32 ViewSizeY);

	/**
	* Generate a 3D projection for the canvas. Use this if you want to transform in 3D 
	*
	* @param ViewSizeX - Viewport width
	* @param ViewSizeY - Viewport height
	* @param fFOV - Field of view for the projection
	* @param NearPlane - Distance to the near clip plane
	* @return Matrix for canvas projection
	*/
	static FMatrix CalcBaseTransform3D(uint32 ViewSizeX, uint32 ViewSizeY, float fFOV, float NearPlane);
	
	/**
	* Generate a view matrix for the canvas. Used for CalcBaseTransform3D
	*
	* @param ViewSizeX - Viewport width
	* @param ViewSizeY - Viewport height
	* @param fFOV - Field of view for the projection
	* @return Matrix for canvas view orientation
	*/
	static FMatrix CalcViewMatrix(uint32 ViewSizeX, uint32 ViewSizeY, float fFOV);
	
	/**
	* Generate a projection matrix for the canvas. Used for CalcBaseTransform3D
	*
	* @param ViewSizeX - Viewport width
	* @param ViewSizeY - Viewport height
	* @param fFOV - Field of view for the projection
	* @param NearPlane - Distance to the near clip plane
	* @return Matrix for canvas projection
	*/
	static FMatrix CalcProjectionMatrix(uint32 ViewSizeX, uint32 ViewSizeY, float fFOV, float NearPlane);

	/**
	* Get the current top-most transform entry without the canvas projection
	* @return matrix from transform stack. 
	*/
	FMatrix GetTransform() const 
	{ 
		return TransformStack.Top().GetMatrix() * TransformStack[0].GetMatrix().Inverse(); 
	}

	/** 
	* Get the bottom-most element of the transform stack. 
	* @return matrix from transform stack. 
	*/
	const FMatrix& GetBottomTransform() const 
	{ 
		return TransformStack[0].GetMatrix(); 
	}

	/**
	* Get the current top-most transform entry 
	* @return matrix from transform stack. 
	*/
	const FMatrix& GetFullTransform() const 
	{ 
		return TransformStack.Top().GetMatrix(); 
	}

	/**
	 * Copy the conents of the TransformStack from an existing canvas
	 *
	 * @param Copy	canvas to copy from
	 **/
	void CopyTransformStack(const FCanvas& Copy);

	/** 
	 * Set the current masked region on the canvas
	 * All rendering from this point on will be masked to this region.
	 * The region being masked uses the current canvas transform
	 *
	 * @param X - x offset in canvas coords
	 * @param Y - y offset in canvas coords
	 * @param SizeX - x size in canvas coords
	 * @param SizeY - y size in canvas coords
	 */
	void PushMaskRegion( float X, float Y, float SizeX, float SizeY );

	/**
	 * Remove the current masking region; if other masking regions were previously pushed onto the stack,
	 * the next one down will be activated.
	 */
	void PopMaskRegion();

	/**
	 * Sets the render target which will be used for subsequent canvas primitives.
	 */
	ENGINE_API void SetRenderTarget(FRenderTarget* NewRenderTarget);	

	/**
	* Get the current render target for the canvas
	*/	
	FORCEINLINE FRenderTarget* GetRenderTarget() const 
	{ 
		return RenderTarget; 
	}

	/**
	 * Sets a rect that should be used to offset rendering into the viewport render target
	 * If not set the canvas will render to the full target
	 *
 	 * @param ViewRect The rect to use
	 */
	ENGINE_API void SetRenderTargetRect( const FIntRect& ViewRect );

	/**
	* Marks render target as dirty so that it will be resolved to texture
	*/
	void SetRenderTargetDirty(bool bDirty) 
	{ 
		bRenderTargetDirty = bDirty; 
	}

	/**
	* Sets the hit proxy which will be used for subsequent canvas primitives.
	*/ 
	ENGINE_API void SetHitProxy(HHitProxy* HitProxy);

	// HitProxy Accessors.	

	FHitProxyId GetHitProxyId() const { return CurrentHitProxy ? CurrentHitProxy->Id : FHitProxyId(); }
	FHitProxyConsumer* GetHitProxyConsumer() const { return HitProxyConsumer; }
	bool IsHitTesting() const { return HitProxyConsumer != NULL; }
	
	/**
	* Push sort key onto the stack. Rendering is done with the current sort key stack entry.
	*
	* @param InSortKey - key value to push onto the stack
	*/
	void PushDepthSortKey(int32 InSortKey)
	{
		DepthSortKeyStack.Push(InSortKey);
	};

	/**
	* Pop sort key off of the stack.
	*
	* @return top entry of the sort key stack
	*/
	int32 PopDepthSortKey()
	{
		int32 Result = 0;
		if( DepthSortKeyStack.Num() > 0 )
		{
			Result = DepthSortKeyStack.Pop();
		}
		else
		{
			// should always have one entry
			PushDepthSortKey(0);
		}
		return Result;		
	};

	/**
	* Return top sort key of the stack.
	*
	* @return top entry of the sort key stack
	*/
	int32 TopDepthSortKey()
	{
		checkSlow(DepthSortKeyStack.Num() > 0);
		return DepthSortKeyStack.Top();
	}

	/**
	 * Toggle allowed canvas modes
	 *
	 * @param InAllowedModes	New modes to set
	 */
	void SetAllowedModes(uint32 InAllowedModes)
	{
		AllowedModes = InAllowedModes;
	}
	/**
	 * Accessor for allowed canvas modes
	 *
	 * @return current allowed modes
	 */
	uint32 GetAllowedModes() const
	{
		return AllowedModes;
	}

	/**
	 * Determine if the canvas has dirty batches that need to be rendered
	 *
	 * @return true if the canvas has any element to render
	 */
	bool HasBatchesToRender() const;

public:
	float AlphaModulate;

	/** Entry for the transform stack which stores a matrix and its CRC for faster comparisons */
	class FTransformEntry
	{
	public:
		FTransformEntry(const FMatrix& InMatrix)
			:	Matrix(InMatrix)
		{			
			MatrixCRC = FCrc::MemCrc_DEPRECATED(&Matrix,sizeof(FMatrix));
		}
		FORCEINLINE void SetMatrix(const FMatrix& InMatrix)
		{
			Matrix = InMatrix;
			MatrixCRC = FCrc::MemCrc_DEPRECATED(&Matrix,sizeof(FMatrix));
		}
		FORCEINLINE const FMatrix& GetMatrix() const
		{
			return Matrix;
		}
		FORCEINLINE uint32 GetMatrixCRC() const
		{
			return MatrixCRC;
		}
	private:
		FMatrix Matrix;
		uint32 MatrixCRC;
	};

	/** returns the transform stack */	
	FORCEINLINE const TArray<FTransformEntry>& GetTransformStack() const
	{
		return TransformStack;
	}
	FORCEINLINE const FIntRect& GetViewRect() const
	{
		return ViewRect;
	}

	FORCEINLINE void SetScaledToRenderTarget(bool scale = true)
	{
		bScaledToRenderTarget = scale;
	}
	FORCEINLINE bool IsScaledToRenderTarget() const { return bScaledToRenderTarget; }
private:
	/** Stack of SortKeys. All rendering is done using the top most sort key */
	TArray<int32> DepthSortKeyStack;	
	/** Stack of matrices. Bottom most entry is the canvas projection */
	TArray<FTransformEntry> TransformStack;	
	/** View rect for the render target */
	FIntRect ViewRect;
	/** Current render target used by the canvas */
	FRenderTarget* RenderTarget;
	/** Current hit proxy consumer */
	FHitProxyConsumer* HitProxyConsumer;
	/** Current hit proxy object */
	TRefCountPtr<HHitProxy> CurrentHitProxy;
	/** Toggles for various canvas rendering functionality **/
	uint32 AllowedModes;
	/** true if the render target has been rendered to since last calling SetRenderTarget() */
	bool bRenderTargetDirty;	
	/** Current real time in seconds */
	float CurrentRealTime;
	/** Current world time in seconds */
	float CurrentWorldTime;
	/** Current world time in seconds */
	float CurrentDeltaWorldTime;
	/** true, if Canvas should be scaled to whole render target */
	bool bScaledToRenderTarget;

	/** 
	* Shared construction function
	*/
	void Construct();

	/** 
	* Region on the canvas that should be masked
	*/
	struct FMaskRegion
	{
		FMaskRegion(float InX=-1,float InY=-1,float InSizeX=-1,float InSizeY=-1, const FMatrix& InTransform=FMatrix::Identity) 
			: X(InX), Y(InY), SizeX(InSizeX), SizeY(InSizeY), Transform(InTransform) 
		{}
		FORCEINLINE bool IsEqual(const FMaskRegion& R) const		
		{ 
			return(	FMath::Abs(X-R.X) < KINDA_SMALL_NUMBER && 
					FMath::Abs(Y-R.Y) < KINDA_SMALL_NUMBER && 
					FMath::Abs(SizeX-R.SizeX) < KINDA_SMALL_NUMBER && 
					FMath::Abs(SizeY-R.SizeY) < KINDA_SMALL_NUMBER );
		}
		bool IsValid() const 
		{ 
			return X >= -DELTA && Y >= -DELTA && SizeX >= -DELTA && SizeY >= -DELTA; 
		}
		FORCEINLINE bool IsZero(float Tolerance=DELTA) const
		{
			//@todo - do we need to check tranform?
			return FMath::Abs(X) < FMath::Abs(Tolerance)
				&& FMath::Abs(Y) < FMath::Abs(Tolerance)
				&& FMath::Abs(SizeX) < FMath::Abs(Tolerance)
				&& FMath::Abs(SizeY) < FMath::Abs(Tolerance);
		}
		float X,Y,SizeX,SizeY;
		FMatrix Transform;
	};

	/**
	 * Stack of mask regions - top of stack (last element) is current canvas mask
	 */
	TArray<FMaskRegion> MaskRegionStack;

public:	

	/*
	 * Access current real time 
	 */
	float GetCurrentRealTime() const { return CurrentRealTime; }

	/*
	 * Access current world time 
	 */
	float GetCurrentWorldTime() const { return CurrentWorldTime; }

	/*
	 * Access current delta time 
	 */
	float GetCurrentDeltaWorldTime() const { return CurrentDeltaWorldTime; }

	/* 
	 * Draw a CanvasItem
	 *
	 * @param Item			Item to draw
	 */
	FORCEINLINE void DrawItem( FCanvasItem& Item )
	{
		Item.Draw( this );
	}
	/* 
	 * Draw a CanvasItem at the given coordinates
	 *
	 * @param Item			Item to draw
	 * @param InPosition	Position to draw item
	 */
	FORCEINLINE void DrawItem( FCanvasItem& Item, const FVector2D& InPosition )
	{
		Item.Draw( this, InPosition );
	}
	
	/* 
	 * Draw a CanvasItem at the given coordinates
	 *
	 * @param Item			Item to draw
	 * @param X				X Position to draw item
	 * @param Y				Y Position to draw item
	 */
	FORCEINLINE void DrawItem( FCanvasItem& Item, float X, float Y  )
	{
		Item.Draw( this, X, Y );
	}

	/**
	 * Get the top-most canvas masking region from the stack.
	 */
	FMaskRegion GetCurrentMaskRegion() const;

	/**
	* Clear the canvas
	*
	* @param	Color		Color to clear with.
	*/
	ENGINE_API void Clear(const FLinearColor& Color);



	/** 
	* Draw arbitrary aligned rectangle.
	*
	* @param X - X position to draw tile at
	* @param Y - Y position to draw tile at
	* @param SizeX - Width of tile
	* @param SizeY - Height of tile
	* @param U - Horizontal position of the upper left corner of the portion of the texture to be shown(texels)
	* @param V - Vertical position of the upper left corner of the portion of the texture to be shown(texels)
	* @param SizeU - The width of the portion of the texture to be drawn. This value is in texels. 
	* @param SizeV - The height of the portion of the texture to be drawn. This value is in texels. 
	* @param Color - tint applied to tile
	* @param Texture - Texture to draw
	* @param AlphaBlend - true to alphablend
	*/
	ENGINE_API void DrawTile( float X, float Y, float SizeX, float SizeY, float U, float V,  float SizeU, float SizeV, const FLinearColor& Color, const FTexture* Texture = NULL, bool AlphaBlend = true );

	/** 
	* Draw an string centered on given location. 
	* This function is being deprecated. a FCanvasTextItem should be used instead.
	* 
	* @param StartX - X point
	* @param StartY - Y point
	* @param Text - Text to draw
	* @param Font - Font to use
	* @param Color - Color of the text
	* @param ShadowColor - Shadow color to draw underneath the text (ignored for distance field fonts)
	* @return total size in pixels of text drawn
	*/
	ENGINE_API int32 DrawShadowedString( float StartX, float StartY, const TCHAR* Text, class UFont* Font, const FLinearColor& Color, const FLinearColor& ShadowColor = FLinearColor::Black );
	
	ENGINE_API int32 DrawShadowedText( float StartX, float StartY, const FText& Text, class UFont* Font, const FLinearColor& Color, const FLinearColor& ShadowColor = FLinearColor::Black );

	ENGINE_API void DrawNGon(const FVector2D& Center, const FColor& Color, int32 NumSides, float Radius);

	/** 
	* Contains all of the batched elements that need to be rendered at a certain depth sort key
	*/
	class FCanvasSortElement
	{
	public:
		/** 
		* Init constructor 
		*/
		FCanvasSortElement(int32 InDepthSortKey=0)
			:	DepthSortKey(InDepthSortKey)
		{}

		/** 
		* Equality is based on sort key 
		*
		* @param Other - instance to compare against
		* @return true if equal
		*/
		bool operator==(const FCanvasSortElement& Other) const
		{
			return DepthSortKey == Other.DepthSortKey;
		}

		/** sort key for this set of render batch elements */
		int32 DepthSortKey;
		/** list of batches that should be rendered at this sort key level */
		TArray<class FCanvasBaseRenderItem*> RenderBatchArray;
	};
	
	/** Batched canvas elements to be sorted for rendering. Sort order is back-to-front */
	TArray<FCanvasSortElement> SortedElements;
	/** Map from sortkey to array index of SortedElements for faster lookup of existing entries */
	TMap<int32,int32> SortedElementLookupMap;

	/** Store index of last Element off to avoid semi expensive Find() */
	int32 LastElementIndex;
	
	/**
	* Get the sort element for the given sort key. Allocates a new entry if one does not exist
	*
	* @param DepthSortKey - the key used to find the sort element entry
	* @return sort element entry
	*/
	FCanvasSortElement& GetSortElement(int32 DepthSortKey);

};



/**
* Base interface for canvas items which can be batched for rendering
*/
class FCanvasBaseRenderItem
{
public:
	virtual ~FCanvasBaseRenderItem()
	{}

	/**
	* Renders the canvas item
	*
	* @param Canvas - canvas currently being rendered
	* @return true if anything rendered
	*/
	virtual bool Render( const FCanvas* Canvas ) =0;
	/**
	* FCanvasBatchedElementRenderItem instance accessor
	*
	* @return FCanvasBatchedElementRenderItem instance
	*/
	virtual class FCanvasBatchedElementRenderItem* GetCanvasBatchedElementRenderItem() { return NULL; }
	/**
	* FCanvasTileRendererItem instance accessor
	*
	* @return FCanvasTileRendererItem instance
	*/
	virtual class FCanvasTileRendererItem* GetCanvasTileRendererItem() { return NULL; }
};


/**
* Info needed to render a batched element set
*/
class FCanvasBatchedElementRenderItem : public FCanvasBaseRenderItem
{
public:
	/** 
	* Init constructor 
	*/
	FCanvasBatchedElementRenderItem(
		FBatchedElementParameters* InBatchedElementParameters=NULL,
		const FTexture* InTexture=NULL,
		ESimpleElementBlendMode InBlendMode=SE_BLEND_MAX,
		FCanvas::EElementType InElementType=FCanvas::ET_MAX,
		const FCanvas::FTransformEntry& InTransform=FCanvas::FTransformEntry(FMatrix::Identity),
		const FDepthFieldGlowInfo& InGlowInfo=FDepthFieldGlowInfo() )
		// this data is deleted after rendering has completed
		: Data(new FRenderData(InBatchedElementParameters, InTexture, InBlendMode, InElementType, InTransform, InGlowInfo))
	{}

	/**
	* Destructor to delete data in case nothing rendered
	*/
	virtual ~FCanvasBatchedElementRenderItem()
	{
		delete Data;
	}

	/**
	* FCanvasBatchedElementRenderItem instance accessor
	*
	* @return this instance
	*/
	virtual class FCanvasBatchedElementRenderItem* GetCanvasBatchedElementRenderItem() 
	{ 
		return this; 
	}

	/**
	* Renders the canvas item. 
	* Iterates over all batched elements and draws them with their own transforms
	*
	* @param Canvas - canvas currently being rendered
	* @return true if anything rendered
	*/
	virtual bool Render( const FCanvas* Canvas );

	/**
	* Determine if this is a matching set by comparing texture,blendmode,elementype,transform. All must match
	*
	* @param BatchedElementParameters - parameters for this batched element
	* @param InTexture - texture resource for the item being rendered
	* @param InBlendMode - current alpha blend mode 
	* @param InElementType - type of item being rendered: triangle,line,etc
	* @param InTransform - the transform for the item being rendered
	* @param InGlowInfo - the depth field glow of the item being rendered
	* @return true if the parameters match this render item
	*/
	bool IsMatch(FBatchedElementParameters* BatchedElementParameters, const FTexture* InTexture, ESimpleElementBlendMode InBlendMode, FCanvas::EElementType InElementType, const FCanvas::FTransformEntry& InTransform, const FDepthFieldGlowInfo& InGlowInfo)
	{
		return(	Data->BatchedElementParameters.GetReference() == BatchedElementParameters &&
				Data->Texture == InTexture &&
				Data->BlendMode == InBlendMode &&
				Data->ElementType == InElementType &&
				Data->Transform.GetMatrixCRC() == InTransform.GetMatrixCRC() &&
				Data->GlowInfo == InGlowInfo );
	}

	/**
	* Accessor for the batched elements. This can be used for adding triangles and primitives to the batched elements
	*
	* @return pointer to batched elements struct
	*/
	FORCEINLINE FBatchedElements* GetBatchedElements()
	{
		return &Data->BatchedElements;
	}

private:
	class FRenderData
	{
	public:
		/**
		* Init constructor
		*/
		FRenderData(
			FBatchedElementParameters* InBatchedElementParameters=NULL,
			const FTexture* InTexture=NULL,
			ESimpleElementBlendMode InBlendMode=SE_BLEND_MAX,
			FCanvas::EElementType InElementType=FCanvas::ET_MAX,
			const FCanvas::FTransformEntry& InTransform=FCanvas::FTransformEntry(FMatrix::Identity),
			const FDepthFieldGlowInfo& InGlowInfo=FDepthFieldGlowInfo() )
			:	BatchedElementParameters(InBatchedElementParameters)
			,	Texture(InTexture)
			,	BlendMode(InBlendMode)
			,	ElementType(InElementType)
			,	Transform(InTransform)
			,	GlowInfo(InGlowInfo)
		{}
		/** Current batched elements, destroyed once rendering completes. */
		FBatchedElements BatchedElements;
		/** Batched element parameters */
		TRefCountPtr<FBatchedElementParameters> BatchedElementParameters;
		/** Current texture being used for batching, set to NULL if it hasn't been used yet. */
		const FTexture* Texture;
		/** Current blend mode being used for batching, set to BLEND_MAX if it hasn't been used yet. */
		ESimpleElementBlendMode BlendMode;
		/** Current element type being used for batching, set to ET_MAX if it hasn't been used yet. */
		FCanvas::EElementType ElementType;
		/** Transform used to render including projection */
		FCanvas::FTransformEntry Transform;
		/** info for optional glow effect when using depth field rendering */
		FDepthFieldGlowInfo GlowInfo;
	};
	/**
	* Render data which is allocated when a new FCanvasBatchedElementRenderItem is added for rendering.
	* This data is only freed on the rendering thread once the item has finished rendering
	*/
	FRenderData* Data;		
};


/**
* Info needed to render a single FTileRenderer
*/
class FCanvasTileRendererItem : public FCanvasBaseRenderItem
{
public:
	/** 
	* Init constructor 
	*/
	FCanvasTileRendererItem( 
		const FMaterialRenderProxy* InMaterialRenderProxy=NULL,
		const FCanvas::FTransformEntry& InTransform=FCanvas::FTransformEntry(FMatrix::Identity),
		bool bInFreezeTime=false)
		// this data is deleted after rendering has completed
		:	Data(new FRenderData(InMaterialRenderProxy,InTransform))
		,	bFreezeTime(bInFreezeTime)
	{}

	/**
	* Destructor to delete data in case nothing rendered
	*/
	virtual ~FCanvasTileRendererItem()
	{
		delete Data;
	}

	/**
	* FCanvasTileRendererItem instance accessor
	*
	* @return this instance
	*/
	virtual class FCanvasTileRendererItem* GetCanvasTileRendererItem() 
	{ 
		return this; 
	}

	/**
	* Renders the canvas item. 
	* Iterates over each tile to be rendered and draws it with its own transforms
	*
	* @param Canvas - canvas currently being rendered
	* @return true if anything rendered
	*/
	virtual bool Render( const FCanvas* Canvas );

	/**
	* Determine if this is a matching set by comparing material,transform. All must match
	*
	* @param IInMaterialRenderProxy - material proxy resource for the item being rendered
	* @param InTransform - the transform for the item being rendered
	* @return true if the parameters match this render item
	*/
	bool IsMatch( const FMaterialRenderProxy* InMaterialRenderProxy, const FCanvas::FTransformEntry& InTransform )
	{
		return( Data->MaterialRenderProxy == InMaterialRenderProxy && 
				Data->Transform.GetMatrixCRC() == InTransform.GetMatrixCRC() );
	};

	/**
	* Add a new tile to the render data. These tiles all use the same transform and material proxy
	*
	* @param X - tile X offset
	* @param Y - tile Y offset
	* @param SizeX - tile X size
	* @param SizeY - tile Y size
	* @param U - tile U offset
	* @param V - tile V offset
	* @param SizeU - tile U size
	* @param SizeV - tile V size
	* @param return number of tiles added
	*/
	FORCEINLINE int32 AddTile(float X,float Y,float SizeX,float SizeY,float U,float V,float SizeU,float SizeV,FHitProxyId HitProxyId)
	{
		return Data->AddTile(X,Y,SizeX,SizeY,U,V,SizeU,SizeV,HitProxyId);
	};

private:
	class FRenderData
	{
	public:
		FRenderData(
			const FMaterialRenderProxy* InMaterialRenderProxy=NULL,
			const FCanvas::FTransformEntry& InTransform=FCanvas::FTransformEntry(FMatrix::Identity) )
			:	MaterialRenderProxy(InMaterialRenderProxy)
			,	Transform(InTransform)
		{}
		const FMaterialRenderProxy* MaterialRenderProxy;
		FCanvas::FTransformEntry Transform;

		struct FTileInst
		{
			float X,Y;
			float SizeX,SizeY;
			float U,V;
			float SizeU,SizeV;
			FHitProxyId HitProxyId;
		};
		TArray<FTileInst> Tiles;

		FORCEINLINE int32 AddTile(float X,float Y,float SizeX,float SizeY,float U,float V,float SizeU,float SizeV,FHitProxyId HitProxyId)
		{
			FTileInst NewTile = {X,Y,SizeX,SizeY,U,V,SizeU,SizeV,HitProxyId};
			return Tiles.Add(NewTile);
		};
	};
	/**
	* Render data which is allocated when a new FCanvasTileRendererItem is added for rendering.
	* This data is only freed on the rendering thread once the item has finished rendering
	*/
	FRenderData* Data;	

	const bool bFreezeTime;
};

/**
* Render string using both a font and a material. The material should have a font exposed as a 
* parameter so that the correct font page can be set based on the character being drawn.
*
* @param Font - font containing texture pages of character glyphs
* @param XL - out width
* @param YL - out height
* @param Text - string of text to be measured
*/
extern ENGINE_API void StringSize( UFont* Font, int32& XL, int32& YL, const TCHAR* Text );


