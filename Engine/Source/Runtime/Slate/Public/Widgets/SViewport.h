// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once


class SLATE_API SViewport
	: public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS( SViewport )
		: _Content()
		, _ShowEffectWhenDisabled(true)
		, _RenderDirectlyToWindow(false)
		, _EnableGammaCorrection(true)
		, _EnableBlending(false)
		, _EnableStereoRendering(false)
		, _IgnoreTextureAlpha(true)
		, _ViewportSize(FVector2D(320.0f, 240.0f))
	{ }

		SLATE_DEFAULT_SLOT( FArguments, Content )

		/** Whether or not to show the disabled effect when this viewport is disabled */
		SLATE_ATTRIBUTE( bool, ShowEffectWhenDisabled )
		
		/** 
		 * Whether or not to render directly to the window's backbuffer or an offscreen render target that is applied to the window later 
		 * Rendering to an offscreen target is the most common option in the editor where there may be many frames which this viewport's interface may wish to not re-render but use a cached buffer instead
		 * Rendering directly to the backbuffer is the most common option in the game where you want to update each frame without the cost of writing to an intermediate target first.
		 */
		SLATE_ARGUMENT( bool, RenderDirectlyToWindow )

		/** Whether or not to enable gamma correction.  Doesn't apply when rendering directly to a backbuffer . */
		SLATE_ARGUMENT( bool, EnableGammaCorrection )

		/** Allow this viewport to blend with its background. */
		SLATE_ARGUMENT( bool, EnableBlending )

		/** Whether or not to enable stereo rendering. */
		SLATE_ARGUMENT(bool, EnableStereoRendering )

		/**
		 * If true, the viewport's texture alpha is ignored when performing blending.  In this case only the viewport tint opacity is used
		 * If false, the texture alpha is used during blending
		 */
		SLATE_ARGUMENT( bool, IgnoreTextureAlpha )

		/** The interface to be used by this viewport for rendering and I/O. */
		SLATE_ARGUMENT(TSharedPtr<ISlateViewport>, ViewportInterface)
		
		/** Size of the viewport widget. */
		SLATE_ATTRIBUTE(FVector2D, ViewportSize);

	SLATE_END_ARGS()

	/** Default constructor. */
	SViewport();

	/**
	 * Construct the widget.
	 *
	 * @param InArgs  Declaration from which to construct the widget.
	 */
	void Construct(const FArguments& InArgs);

public:

	/** SViewport wants keyboard focus */
	virtual bool SupportsKeyboardFocus() const override { return true; }

	/** 
	 * Computes the ideal size necessary to display this widget.
	 *
	 * @return The desired width and height.
	 */
	virtual FVector2D ComputeDesiredSize() const override
	{
		return ViewportSize.Get();
	}

	/**
	 * Sets the interface to be used by this viewport for rendering and I/O
	 *
	 * @param InViewportInterface The interface to use
	 */
	void SetViewportInterface( TSharedRef<ISlateViewport> InViewportInterface )
	{
		ViewportInterface = InViewportInterface;
	}

	/**
	 * Sets the content for this widget
	 *
	 * @param InContent	The new content (can be null)
	 */
	void SetContent( TSharedPtr<SWidget> InContent );

	void SetCustomHitTestPath( TSharedPtr<ICustomHitTestPath> CustomHitTestPath );

	TSharedPtr<ICustomHitTestPath> GetCustomHitTestPath();

	const TSharedPtr<SWidget> GetContent() const { return ChildSlot.GetWidget(); }

	/**
	 * A delegate called when the viewports top level window is being closed
	 *
	 * @param InWindowBeingClosed	The window that is about to be closed
	 */
	void OnWindowClosed( const TSharedRef<SWindow>& InWindowBeingClosed );


	/** @return Whether or not this viewport renders directly to the backbuffer */
	bool ShouldRenderDirectly() const { return bRenderDirectlyToWindow; }

	/** @return Whether or not this viewport supports stereo rendering */
	bool IsStereoRenderingAllowed() const { return bEnableStereoRendering; }

	/**
	 * Sets a widget that should become focused when this window is next activated
	 *
	 * @param	InWidget	The widget to set focus to when this window is activated
	 */
	void SetWidgetToFocusOnActivate( const TSharedPtr<SWidget>& InWidget )
	{
		
		WidgetToFocusOnActivate = InWidget;
	}

	/** Removes the widget to focus on activate so the viewport will be focused */
	void ClearWidgetToFocusOnActivate()
	{
		WidgetToFocusOnActivate.Reset();
	}

public:

	// SWidget interface

	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual FReply OnTouchStarted( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent ) override;
	virtual FReply OnTouchMoved( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent ) override;
	virtual FReply OnTouchEnded( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent ) override;
	virtual FReply OnTouchGesture( const FGeometry& MyGeometry, const FPointerEvent& GestureEvent ) override;
	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnMouseLeave( const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& KeyEvent ) override;
	virtual FReply OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& KeyEvent ) override;
	virtual FReply OnAnalogValueChanged( const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent ) override;
	virtual FReply OnKeyChar( const FGeometry& MyGeometry, const FCharacterEvent& CharacterEvent ) override;
	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent ) override;
	virtual void OnFocusLost( const FFocusEvent& InFocusEvent ) override;
	virtual FReply OnMotionDetected( const FGeometry& MyGeometry, const FMotionEvent& InMotionEvent ) override;
	virtual void OnFinishedPointerInput() override;
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual TSharedPtr<struct FVirtualPointerPosition> TranslateMouseCoordinateFor3DChild(const TSharedRef<SWidget>& ChildWidget, const FGeometry& MyGeometry, const FVector2D& ScreenSpaceMouseCoordinate, const FVector2D& LastScreenSpaceMouseCoordinate) const;
	virtual FNavigationReply OnNavigation( const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent ) override;

private:

	FSimpleSlot& Decl_GetContent()
	{
		return ChildSlot;
	}

protected:

	/** Interface to the rendering and I/O implementation of the viewport. */
	TWeakPtr<ISlateViewport> ViewportInterface;
	
private:

	/** Whether or not to show the disabled effect when this viewport is disabled. */
	TAttribute<bool> ShowDisabledEffect;

	/** Size of the viewport. */
	TAttribute<FVector2D> ViewportSize;

	/** 
	 * Widget to transfer keyboard focus to when this window becomes active, if any.
	 * This is used to restore focus to a widget after a popup has been dismissed.
	 */
	TWeakPtr< SWidget > WidgetToFocusOnActivate;

	TSharedPtr<ICustomHitTestPath> CustomHitTestPath;

	/** Whether or not this viewport renders directly to the window back-buffer. */
	bool bRenderDirectlyToWindow;

	/** Whether or not to apply gamma correction on the render target supplied by the ISlateViewport. */
	bool bEnableGammaCorrection;

	/** Whether or not to blend this viewport with the background. */
	bool bEnableBlending;

	/** Whether or not to enable stereo rendering. */
	bool bEnableStereoRendering;

	/** Whether or not to allow texture alpha to be used in blending calculations. */
	bool bIgnoreTextureAlpha;
};
