// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once


class SLATE_API SOverlay : public SPanel
{
public:	

	/** A slot that support alignment of content and padding and z-order */
	class SLATE_API FOverlaySlot : public TSupportsOneChildMixin<SWidget, FOverlaySlot>
	{
	public:
		FOverlaySlot()
			: HAlignment(HAlign_Fill)
			, VAlignment(VAlign_Fill)
			, SlotPadding( FMargin(0) )
 			, ZOrder( 0 )
		{

		}

		FOverlaySlot& HAlign( EHorizontalAlignment InHAlignment )
		{
			HAlignment = InHAlignment;
			return *this;
		}

		FOverlaySlot& VAlign( EVerticalAlignment InVAlignment )
		{
			VAlignment = InVAlignment;
			return *this;
		}

		FOverlaySlot& Padding( const TAttribute<FMargin> InPadding )
		{
			SlotPadding = InPadding;
			return *this;
		}


		/** The child widget contained in this slot. */
		EHorizontalAlignment HAlignment;
		EVerticalAlignment VAlignment;
		TAttribute< FMargin > SlotPadding;

		/** Slots with larger ZOrder values will draw above slots with smaller ZOrder values.  Slots
		    with the same ZOrder will simply draw in the order they were added.  Currently this only
			works for overlay slots that are added dynamically with AddWidget() and RemoveWidget() */
		int32 ZOrder;
	};


	SLATE_BEGIN_ARGS( SOverlay )
		{
			_Visibility = EVisibility::SelfHitTestInvisible;
		}

		SLATE_SUPPORTS_SLOT( SOverlay::FOverlaySlot )

	SLATE_END_ARGS()

	virtual ~SOverlay()
	{
	}

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	/** Returns the number of child widgets */
	int32 GetNumWidgets() const;

	/**
	 * Removes a widget from this overlay
	 *
	 * @param	Widget	The widget content to remove
	 */
	void RemoveSlot( TSharedRef< SWidget > Widget );

	/** Adds a slot at the specified location (ignores Z-order) */
	FOverlaySlot& AddSlot(int32 ZOrder=INDEX_NONE);

	/** Removes a slot at the specified location */
	void RemoveSlot(int32 ZOrder=INDEX_NONE);

	/** Removes all children from the overlay */
	void ClearChildren();

	/** @return a new slot. Slots contain children for SOverlay */
	static FOverlaySlot& Slot()
	{
		return *(new FOverlaySlot());
	}

	// SWidget interface
	virtual void ArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const OVERRIDE;
	virtual FVector2D ComputeDesiredSize() const OVERRIDE;
	virtual FChildren* GetChildren() OVERRIDE;
	virtual int32 OnPaint( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const OVERRIDE;
	// End of SWidget interface

protected:
	/** The SOverlay's slots; each slot contains a child widget. */
	TPanelChildren<FOverlaySlot> Children;
};
