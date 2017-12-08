// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// The WITH_OCULUS_PRIVATE_CODE tag is kept for reference
//#if WITH_OCULUS_PRIVATE_CODE

#include "UObject/ObjectMacros.h"
#include "InputCoreTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "UObject/ScriptMacros.h"
#include "Input/PopupMethodReply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SOverlay.h"
#include "ShowFlags.h"
#include "Engine/ScriptViewportClient.h"
#include "Engine/ViewportSplitScreen.h"
#include "Engine/TitleSafeZone.h"
#include "Engine/GameViewportDelegates.h"
#include "Engine/DebugDisplayProperty.h"
#include "SceneTypes.h"

#include "CastingViewportClient.generated.h"

class FCanvas;
class FSceneView;
class FSceneViewport;
class IGameLayerManager;
class SViewport;
class SWindow;
class UCanvas;
class UGameInstance;
//class ULocalPlayer;
class ACastingCameraActor;
class UNetDriver;
class FSceneInterface;
class FSceneViewFamily;

UENUM()
enum class ECastingLayer : uint8
{
    Full,
    Background,
    Foreground
};

UENUM()
enum class ECastingViewportCompositionMethod : uint8
{
    /* Generate both foreground and background views for compositing with 3rd-party software like OBS. */
    MultiView,
    /* Composite the camera stream directly to the output with the proper depth.*/
    DirectComposition,
};

/**
 * A casting viewport (FViewport) is a high-level abstract interface for the
 * platform specific rendering and audio subsystems for live casting the game.
 * CastingViewportClient is the engine's interface to a casting viewport.
 *
 * Responsibilities:
 * Live-casting the gameplay through a CastingCameraActor
 *
 * @see UCastingViewportClient
 */
UCLASS(Within=Engine, transient, config=Engine)
class ENGINE_API UCastingViewportClient : public UScriptViewportClient
{
	GENERATED_UCLASS_BODY()

public:
	/** DO NOT USE. This constructor is for internal usage only for hot-reload purposes. */
    UCastingViewportClient(FVTableHelper& Helper);

	virtual ~UCastingViewportClient();

    /** set to disable world rendering */
	uint32 bDisableWorldRendering:1;

protected:
	/* The relative world context for this viewport */
	UPROPERTY()
	UWorld* World;

	UPROPERTY()
	UGameInstance* GameInstance;

    UPROPERTY()
    ACastingCameraActor* CastingCameraActor;

    /** The viewport's scene view state. */
    FSceneViewStateReference FullLayerViewState;
    FSceneViewStateReference BackgroundLayerViewState;
    FSceneViewStateReference ForegroundLayerViewState;

public:

	/** see enum EViewModeIndex */
	int32 ViewModeIndex;

    /** Mixed Reality: how the casting output would be composited */
    ECastingViewportCompositionMethod CompositionMethod;

    /** Mixed Reality: whether we want the output be projected to mirror window */
    bool bProjectToMirrorWindow;

    /**
    * @return The scene being rendered in this viewport
    */
    virtual FSceneInterface* GetScene() const;

    /** Returns a relative world context for this viewport.	 */
	virtual UWorld* GetWorld() const override;

	/* Returns the game viewport */
	FSceneViewport* GetCastingViewport();

	/* Returns the widget for this viewport */
	TSharedPtr<SViewport> GetCastingViewportWidget();

	/* Returns the relevant game instance for this viewport */
	UGameInstance* GetGameInstance() const;

    /* Returns the casting camera actor */
    ACastingCameraActor* GetCastingCameraActor() const;

	virtual void Init(struct FWorldContext& WorldContext, UGameInstance* OwningGameInstance, ACastingCameraActor* CastingCameraActor, ECastingViewportCompositionMethod InCompositionMethod);

    /**
    * Configures the specified FSceneView object with the view and projection matrices for this viewport.
    * @param	View		The view to be configured.  Must be valid.
    * @param	CastingLayer
    * @param	RowIndex
    * @param	ColumnIndex
    * @param	TotalRows
    * @param	TotalColumns
    * @return	A pointer to the view within the view family which represents the viewport's primary view.
    */
    void CalcAndAddSceneView(FSceneViewFamily* ViewFamily, ECastingLayer CastingLayer, uint8 RowIndex, uint8 ColumnIndex, uint8 TotalRows, uint8 TotalColumns, FName BufferVisualizationMode);

public:
	//~ Begin UObject Interface
	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;
	//~ End UObject Interface

    //~ Begin FViewportClient Interface.
	virtual void RedrawRequested(FViewport* InViewport) override {}

	virtual void Draw(FViewport* Viewport,FCanvas* SceneCanvas) override;
	virtual void ProcessScreenShots(FViewport* Viewport) override;
	virtual TOptional<bool> QueryShowFocus(const EFocusCause InFocusCause) const override;
	virtual void LostFocus(FViewport* Viewport) override;
	virtual void ReceivedFocus(FViewport* Viewport) override;
	virtual bool IsFocused(FViewport* Viewport) override;
	virtual void Activated(FViewport* InViewport, const FWindowActivateEvent& InActivateEvent) override;
	virtual void Deactivated(FViewport* InViewport, const FWindowActivateEvent& InActivateEvent) override;
	virtual bool WindowCloseRequested() override;
	virtual void CloseRequested(FViewport* Viewport) override;
	virtual bool RequiresHitProxyStorage() override { return 0; }
	virtual bool IsOrtho() const override;
	//~ End FViewportClient Interface.

	/**
	 * Adds a widget to the Slate viewport's overlay (i.e for in game UI or tools) at the specified Z-order
	 *
	 * @param  ViewportContent	The widget to add.  Must be valid.
	 * @param  ZOrder  The Z-order index for this widget.  Larger values will cause the widget to appear on top of widgets with lower values.
	 */
	virtual void AddViewportWidgetContent( TSharedRef<class SWidget> ViewportContent, const int32 ZOrder = 0 );

	/**
	 * Removes a previously-added widget from the Slate viewport
	 *
	 * @param	ViewportContent  The widget to remove.  Must be valid.
	 */
	virtual void RemoveViewportWidgetContent( TSharedRef<class SWidget> ViewportContent );

	/**
	 * This function removes all widgets from the viewport overlay
	 */
	void RemoveAllViewportWidgets();

	/**
	 * Cleans up all rooted or referenced objects created or managed by the GameViewportClient.  This method is called
	 * when this GameViewportClient has been disassociated with the game engine (i.e. is no longer the engine's GameViewport).
	 */
	virtual void DetachViewportClient();

	/**
	 * Called every frame to allow the game viewport to update time based state.
	 * @param	DeltaTime	The time since the last call to Tick.
	 */
	virtual void Tick( float DeltaTime );

	/**
	 * Determines whether this viewport client should receive calls to InputAxis() if the game's window is not currently capturing the mouse.
	 * Used by the UI system to easily receive calls to InputAxis while the viewport's mouse capture is disabled.
	 */
	virtual bool RequiresUncapturedAxisInput() const override;

	/**
	 * Set this GameViewportClient's viewport and viewport frame to the viewport specified
	 * @param	InViewportFrame	The viewportframe to set
	 */
	virtual void SetViewportFrame( FViewportFrame* InViewportFrame );

	/**
	 * Set this GameViewportClient's viewport to the viewport specified
	 * @param	InViewportFrame	The viewport to set
	 */
	virtual void SetViewport( FViewport* InViewportFrame );

	/** Assigns the viewport overlay widget to use for this viewport client.  Should only be called when first created */
	void SetViewportOverlayWidget( TSharedPtr< SWindow > InWindow, TSharedRef<SOverlay> InViewportOverlayWidget )
	{
		Window = InWindow;
		ViewportOverlayWidget = InViewportOverlayWidget;
	}

	/** Returns access to this viewport's Slate window */
	TSharedPtr< SWindow > GetWindow()
	{
		 return Window.Pin();
	}

	/**
	 * Retrieve the size of the main viewport.
	 *
	 * @param	out_ViewportSize	[out] will be filled in with the size of the main viewport
	 */
	void GetViewportSize( FVector2D& ViewportSize ) const;

	/** @return Whether or not the main viewport is fullscreen or windowed. */
	bool IsFullScreenViewport() const;

	/** @return mouse position in game viewport coordinates (does not account for splitscreen) */
	DEPRECATED(4.5, "Use GetMousePosition that returns a boolean if mouse is in window instead.")
	FVector2D GetMousePosition() const;

	/** @return mouse position in game viewport coordinates (does not account for splitscreen) */
	bool GetMousePosition(FVector2D& MousePosition) const;

	/**
	 * Determine whether a fullscreen viewport should be used in cases where there are multiple players.
	 *
	 * @return	true to use a fullscreen viewport; false to allow each player to have their own area of the viewport.
	 */
	bool ShouldForceFullscreenViewport() const;

	/**
	 * Called after rendering the player views and HUDs to render menus, the console, etc.
	 * This is the last rendering call in the render loop
	 *
	 * @param Canvas	The canvas to use for rendering.
	 */
	virtual void PostRender( UCanvas* Canvas );

	/* Accessor for the delegate called when a viewport is asked to close. */
	FOnCloseRequested& OnCloseRequested()
	{
		return CloseRequestedDelegate;
	}

	/** Accessor for the delegate called when the window owning the viewport is asked to close. */
	FOnWindowCloseRequested& OnWindowCloseRequested()
	{
		return WindowCloseRequestedDelegate;
	}

	/** Accessor for the delegate called when the game viewport is created. */
	static FSimpleMulticastDelegate& OnViewportCreated()
	{
		return CreatedDelegate;
	}

	// Accessor for the delegate called when the engine starts drawing a game viewport
	FSimpleMulticastDelegate& OnBeginDraw()
	{
		return BeginDrawDelegate;
	}

	// Accessor for the delegate called when the game viewport is drawn, before drawing the console
	FSimpleMulticastDelegate& OnDrawn()
	{
		return DrawnDelegate;
	}

	// Accessor for the delegate called when the engine finishes drawing a game viewport
	FSimpleMulticastDelegate& OnEndDraw()
	{
		return EndDrawDelegate;
	}

	// Accessor for the delegate called when ticking the game viewport
	FOnGameViewportTick& OnTick()
	{
		return TickDelegate;
	}

	/** Return the engine show flags for this viewport */
	virtual FEngineShowFlags* GetEngineShowFlags() override
	{
		return &EngineShowFlags;
	}

public:
	/** The show flags used by the viewport's players. */
	FEngineShowFlags EngineShowFlags;

	/** The platform-specific viewport which this viewport client is attached to. */
	FViewport* Viewport;

	/** The platform-specific viewport frame which this viewport is contained by. */
	FViewportFrame* ViewportFrame;

	/**
	 * Should we make new windows for popups or create an overlay in the current window.
	 */
	virtual FPopupMethodReply OnQueryPopupMethod() const override;

	/** Accessor for delegate called when the engine toggles fullscreen */
	FOnToggleFullscreen& OnToggleFullscreen()
	{
		return ToggleFullscreenDelegate;
	}

private:
	/** Slate window associated with this viewport client.  The same window may host more than one viewport client. */
	TWeakPtr<SWindow> Window;

	/** Overlay widget that contains widgets to draw on top of the game viewport */
	TWeakPtr<SOverlay> ViewportOverlayWidget;

	/** Current buffer visualization mode for this game viewport */
	FName CurrentBufferVisualizationMode;

	/**
	 * Applies requested changes to display configuration
	 * @param Dimensions Pointer to new dimensions of the display. nullptr for no change.
	 * @param WindowMode What window mode do we want to st the display to.
	 */
	bool SetDisplayConfiguration( const FIntPoint* Dimensions, EWindowMode::Type WindowMode);

	/** Delegate called at the end of the frame when a screenshot is captured */
	static FOnScreenshotCaptured ScreenshotCapturedDelegate;

	/** Delegate called when a request to close the viewport is received */
	FOnCloseRequested CloseRequestedDelegate;

	/** Delegate called when the window owning the viewport is requested to close */
	FOnWindowCloseRequested WindowCloseRequestedDelegate;

	/** Delegate called when the game viewport is created. */
	static FSimpleMulticastDelegate CreatedDelegate;

	/** Delegate called when a player is added to the game viewport */
	FOnGameViewportClientPlayerAction PlayerAddedDelegate;

	/** Delegate called when a player is removed from the game viewport */
	FOnGameViewportClientPlayerAction PlayerRemovedDelegate;

	/** Delegate called when the engine starts drawing a game viewport */
	FSimpleMulticastDelegate BeginDrawDelegate;

	/** Delegate called when the game viewport is drawn, before drawing the console */
	FSimpleMulticastDelegate DrawnDelegate;

	/** Delegate called when the engine finishes drawing a game viewport */
	FSimpleMulticastDelegate EndDrawDelegate;

	/** Delegate called when ticking the game viewport */
	FOnGameViewportTick TickDelegate;

	/** Delegate called when the engine toggles fullscreen */
	FOnToggleFullscreen ToggleFullscreenDelegate;
};

//#endif // WITH_OCULUS_PRIVATE_CODE