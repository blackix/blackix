// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FSlateRenderer.h: Declares the FSlateRenderer class.
=============================================================================*/

#pragma once


class SWindow;
class FRHITexture2D;

/**
 * Abstract base class for Slate renderers.
 */
class FSlateRenderer
{
public:

	/**
	 * Virtual destructor.
	 */
	virtual ~FSlateRenderer( ) { }

public:

	/** Returns a draw buffer that can be used by Slate windows to draw window elements */
	virtual FSlateDrawBuffer& GetDrawBuffer() = 0;

	virtual void Initialize() = 0;

	virtual void Destroy() = 0;

	/**
	 * Creates a rendering viewport
	 *
	 * @param InWindow	The window to create the viewport for
	 */ 
	virtual void CreateViewport( const TSharedRef<SWindow> InWindow ) = 0;

	/**
	 * Requests that a rendering viewport be resized
	 *
	 * @param Window		The window to resize
	 * @param Width			The new width of the window
	 * @param Height		The new height of the window
	 */
	virtual void RequestResize( const TSharedPtr<SWindow>& InWindow, uint32 NewSizeX, uint32 NewSizeY ) = 0;

	/**
	 * Sets fullscreen state on the window's rendering viewport
	 *
	 * @param	InWindow		The window to update fullscreen state on
	 * @param	OverrideResX	0 if no override
	 * @param	OverrideResY	0 if no override
	 */
	virtual void UpdateFullscreenState( const TSharedRef<SWindow> InWindow, uint32 OverrideResX = 0, uint32 OverrideResY = 0 ) = 0;

	/** 
	 * Creates necessary resources to render a window and sends draw commands to the rendering thread
	 *
	 * @param WindowDrawBuffer	The buffer containing elements to draw 
	 */
	virtual void DrawWindows( FSlateDrawBuffer& InWindowDrawBuffer ) = 0;
	
	/** 
	 * Renders a window using resources stored from a previous call to DrawWindows, if
	 * the previous call did store that data. Optional implementation.
	 */
	virtual void DrawWindows() {}
	
	/**
	 * Copies all slate windows out to a buffer at half resolution with debug information
	 * like the mouse cursor and any keypresses.
	 */
	virtual void CopyWindowsToDrawBuffer(const TArray<FString>& KeypressBuffer) {}
	
	/** Allows and disallows access to the crash tracker buffer data on the CPU */
	virtual void MapCrashTrackerBuffer(void** OutImageData, int32* OutWidth, int32* OutHeight) {}
	virtual void UnmapCrashTrackerBuffer() {}
	
	/** 
	 * Sets which color vision filter to use
	 */
	virtual void SetColorVisionDeficiencyType( uint32 Type ) {} 

	/** 
	 * Creates a dynamic image resource and returns its size
	 *
	 * @param InTextureName The name of the texture resource
	 * @return The size of the loaded texture
	 */
	virtual FIntPoint GenerateDynamicImageResource(const FName InTextureName) {check(0); return FIntPoint( 0, 0 );}

	/**
	 * Creates a dynamic image resource
	 *
	 * @param ResourceName		The name of the texture resource
	 * @param Width				The width of the resource
	 * @param Height			The height of the image
	 * @param Bytes				The payload for the resource
	 * @return					true if the resource was successfully generated, otherwise false
	 */
	virtual bool GenerateDynamicImageResource( FName ResourceName, uint32 Width, uint32 Height, const TArray< uint8 >& Bytes ) { return false; }

	/** Called when a window is destroyed to give the renderer a chance to free resources */
	virtual void OnWindowDestroyed( const TSharedRef<SWindow>& InWindow ) = 0;
	
	/**
	 * Returns the viewport rendering resource (backbuffer) for the provided window
	 *
	 * @param Window	The window to get the viewport from 
	 */
	virtual void* GetViewportResource( const SWindow& Window ) { return nullptr; }
	
	TSharedRef< class FSlateFontMeasure > GetFontMeasureService() const 
	{
		return FontMeasure.ToSharedRef();
	}

	/**
	 * Flushes all cached data from the font cache
	 */
	void SLATECORE_API FlushFontCache();

	/**
	 * Gives the renderer a chance to wait for any render commands to be completed before returning/
	 */
	virtual void FlushCommands() const {};

	/**
	 * Gives the renderer a chance to synchronize with another thread in the event that the renderer runs 
	 * in a multi-threaded environment.  This function does not return until the sync is complete
	 */
	virtual void Sync() const {};

	/**
	 * Reloads all texture resources from disk
	 */
	virtual void ReloadTextureResources() {}

	/**
	 * Loads all the resources used by the specified SlateStyle
	 */
	virtual void LoadStyleResources( const ISlateStyle& Style ) {}

	/** Creates a window to visualize the texture atlases */
	virtual void DisplayTextureAtlases() {}

	/** Releases a specific resource */
	virtual void ReleaseDynamicResource( const FSlateBrush& Brush ) = 0;

	/**
	 * Returns whether or not a viewport should be in  fullscreen
	 *
	 * @Window	The window to check for fullscreen
	 * @return true if the window's viewport should be fullscreen
	 */
	bool SLATECORE_API IsViewportFullscreen( const SWindow& Window ) const;

	/** Returns whether shaders that Slate depends on have been compiled. */
	virtual bool AreShadersInitialized() const { return true; }

	/** 
	 * Removes references to FViewportRHI's.  
	 * This has to be done explicitly instead of using the FRenderResource mechanism because FViewportRHI's are managed by the game thread.
	 * This is needed before destroying the RHI device. 
	 */
	virtual void InvalidateAllViewports() {}

	/** 
	 * Prepares the renderer to take a screenshot of the UI.  The Rect is portion of the rendered output
	 * that will be stored into the TArray of FColors.
	 */
	virtual void PrepareToTakeScreenshot(const FIntRect& Rect, TArray<FColor>* OutColorData) {}

	/** Saves render target in the renderer. */
	virtual void SetRenderTarget(const SWindow& Window, FRHITexture2D* RT) { }

protected:

	TSharedPtr<class FSlateFontCache> FontCache;
	TSharedPtr<class FSlateFontMeasure> FontMeasure;
};


/**
 * Is this thread valid for sending out rendering commands?
 * If the slate loading thread exists, then yes, it is always safe
 * Otherwise, we have to be on the game thread
 */
bool SLATECORE_API IsThreadSafeForSlateRendering( );
