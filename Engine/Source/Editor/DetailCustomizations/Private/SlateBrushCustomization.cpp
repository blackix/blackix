// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "SlateBrushCustomization.h"

/**
 * Slate Brush Preview widget
 */
class SSlateBrushPreview : public SBorder
{
	/**
	 * The widget zone the user is interacting with
	 */
	enum EWidgetZone
	{
		WZ_NotInWidget			= 0,
		WZ_InWidget				= 1,
		WZ_RightBorder			= 2,
		WZ_BottomBorder			= 3,
		WZ_BottomRightBorder	= 4
	};

public:
	SLATE_BEGIN_ARGS( SSlateBrushPreview )
		{}
		SLATE_ARGUMENT(	TSharedPtr<IPropertyHandle>, DrawAsProperty )
		SLATE_ARGUMENT(	TSharedPtr<IPropertyHandle>, TilingProperty )
		SLATE_ARGUMENT(	TSharedPtr<IPropertyHandle>, ImageSizeProperty )
		SLATE_ARGUMENT(	TSharedPtr<IPropertyHandle>, MarginProperty )
		SLATE_ARGUMENT(	TSharedPtr<IPropertyHandle>, ResourceObjectProperty )
		SLATE_ARGUMENT( FSlateBrush*, SlateBrush )
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 */
	void Construct( const FArguments& InArgs )
	{
		DrawAsProperty = InArgs._DrawAsProperty;
		TilingProperty = InArgs._TilingProperty;
		ImageSizeProperty = InArgs._ImageSizeProperty;
		MarginProperty = InArgs._MarginProperty;
		ResourceObjectProperty = InArgs._ResourceObjectProperty;

		FSimpleDelegate OnDrawAsChangedDelegate = FSimpleDelegate::CreateSP( this, &SSlateBrushPreview::OnDrawAsChanged );
		DrawAsProperty->SetOnPropertyValueChanged( OnDrawAsChangedDelegate );

		FSimpleDelegate OnTilingChangedDelegate = FSimpleDelegate::CreateSP( this, &SSlateBrushPreview::OnTilingChanged );
		TilingProperty->SetOnPropertyValueChanged( OnTilingChangedDelegate );

		FSimpleDelegate OnBrushResourceChangedDelegate = FSimpleDelegate::CreateSP( this, &SSlateBrushPreview::OnBrushResourceChanged );
		ResourceObjectProperty->SetOnPropertyValueChanged( OnBrushResourceChangedDelegate );

		FSimpleDelegate OnImageSizeChangedDelegate = FSimpleDelegate::CreateSP( this, &SSlateBrushPreview::OnImageSizeChanged );
		ImageSizeProperty->SetOnPropertyValueChanged( OnImageSizeChangedDelegate );

		uint32 NumChildren = 0;
		ImageSizeProperty->GetNumChildren( NumChildren );
		for( uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
		{
			TSharedPtr<IPropertyHandle> Child = ImageSizeProperty->GetChildHandle( ChildIndex );
			Child->SetOnPropertyValueChanged( OnImageSizeChangedDelegate );
		}

		FSimpleDelegate OnMarginChangedDelegate = FSimpleDelegate::CreateSP( this, &SSlateBrushPreview::OnMarginChanged );
		MarginProperty->SetOnPropertyValueChanged( OnMarginChangedDelegate );

		MarginProperty->GetNumChildren( NumChildren );
		for( uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
		{
			TSharedPtr<IPropertyHandle> Child = MarginProperty->GetChildHandle( ChildIndex );
			Child->SetOnPropertyValueChanged( OnMarginChangedDelegate );
		}

		HorizontalAlignment = EHorizontalAlignment::HAlign_Fill;
		VerticalAlignment = EVerticalAlignment::VAlign_Fill;
		bUserIsResizing = false;
		MouseZone = WZ_NotInWidget;

		SBorder::Construct(
			SBorder::FArguments()
			.BorderImage( FEditorStyle::GetBrush( "PropertyEditor.SlateBrushPreview" ) )
			.Padding( FMargin( 4.0f, 4.0f, 4.0f, 14.0f ) )
			[
				SNew( SBox )
				.WidthOverride( this, &SSlateBrushPreview::GetPreviewWidth ) 
				.HeightOverride( this, &SSlateBrushPreview::GetPreviewHeight )
				[
					SNew( SOverlay )
					+SOverlay::Slot()
					[
						SNew( SImage )
						.Image( FEditorStyle::GetBrush( "Checkerboard" ) )
					]

					+SOverlay::Slot()
					.Padding( FMargin( ImagePadding ) )
					.Expose( OverlaySlot )
					[
						SNew( SImage )
						.Image( InArgs._SlateBrush )
					]

					+SOverlay::Slot()
					.HAlign( HAlign_Left )
					.VAlign( VAlign_Fill )
					[
						SNew( SHorizontalBox )
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew( SSpacer )
							.Size( this, &SSlateBrushPreview::GetLeftMarginLinePosition )
						]
						+SHorizontalBox::Slot()
						[
							SNew( SImage )
							.Image( FEditorStyle::GetBrush( "PropertyEditor.VerticalDottedLine" ) )
							.Visibility( this, &SSlateBrushPreview::GetMarginLineVisibility )
						]
					]

					+SOverlay::Slot()
					.HAlign( HAlign_Left )
					.VAlign( VAlign_Fill )
					[
						SNew( SHorizontalBox )
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew( SSpacer )
							.Size( this, &SSlateBrushPreview::GetRightMarginLinePosition )
						]
						+SHorizontalBox::Slot()
						[
							SNew( SImage )
							.Image( FEditorStyle::GetBrush( "PropertyEditor.VerticalDottedLine" ) )
							.Visibility( this, &SSlateBrushPreview::GetMarginLineVisibility )
						]
					]

					+SOverlay::Slot()
					.HAlign( HAlign_Fill )
					.VAlign( VAlign_Top )
					[
						SNew( SVerticalBox )
						+SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew( SSpacer )
							.Size( this, &SSlateBrushPreview::GetTopMarginLinePosition )
						]
						+SVerticalBox::Slot()
						[
							SNew( SImage )
							.Image( FEditorStyle::GetBrush( "PropertyEditor.HorizontalDottedLine" ) )
							.Visibility( this, &SSlateBrushPreview::GetMarginLineVisibility )
						]
					]

					+SOverlay::Slot()
					.HAlign( HAlign_Fill )
					.VAlign( VAlign_Top )
					[
						SNew( SVerticalBox )
						+SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew( SSpacer )
							.Size( this, &SSlateBrushPreview::GetBottomMarginLinePosition )
						]
						+SVerticalBox::Slot()
						[
							SNew( SImage )
							.Image( FEditorStyle::GetBrush( "PropertyEditor.HorizontalDottedLine" ) )
							.Visibility( this, &SSlateBrushPreview::GetMarginLineVisibility )
						]
					]
				]
			]
		);

		CachePropertyValues();
		SetDefaultAlignment();
		UpdatePreviewImageSize();
		UpdateMarginLinePositions();
	}

	/**
	 * Generate the alignment combo box widgets
	 */
	TSharedRef<SWidget> GenerateAlignmentComboBoxes()
	{
		HorizontalAlignmentComboItems.Add( MakeShareable( new EHorizontalAlignment( EHorizontalAlignment::HAlign_Fill ) ) );
		HorizontalAlignmentComboItems.Add( MakeShareable( new EHorizontalAlignment( EHorizontalAlignment::HAlign_Left ) ) );
		HorizontalAlignmentComboItems.Add( MakeShareable( new EHorizontalAlignment( EHorizontalAlignment::HAlign_Center ) ) );
		HorizontalAlignmentComboItems.Add( MakeShareable( new EHorizontalAlignment( EHorizontalAlignment::HAlign_Right ) ) );
		VerticalAlignmentComboItems.Add( MakeShareable( new EVerticalAlignment( EVerticalAlignment::VAlign_Fill ) ) );
		VerticalAlignmentComboItems.Add( MakeShareable( new EVerticalAlignment( EVerticalAlignment::VAlign_Top ) ) );
		VerticalAlignmentComboItems.Add( MakeShareable( new EVerticalAlignment( EVerticalAlignment::VAlign_Center ) ) );
		VerticalAlignmentComboItems.Add( MakeShareable( new EVerticalAlignment( EVerticalAlignment::VAlign_Bottom ) ) );

		return
			SNew( SUniformGridPanel )
			.SlotPadding( FEditorStyle::GetMargin( "StandardDialog.SlotPadding" ) )
			+SUniformGridPanel::Slot( 0, 0 )
			.HAlign( HAlign_Right )
			.VAlign( VAlign_Center )
			[
				SNew( STextBlock )
				.Font( IDetailLayoutBuilder::GetDetailFont() )
				.Text( NSLOCTEXT( "UnrealEd", "HorizontalAlignment", "Horizontal Alignment" ) )
				.ToolTipText( NSLOCTEXT( "UnrealEd", "PreviewHorizontalAlignment", "Horizontal alignment for the preview" ) )
			]

			+SUniformGridPanel::Slot( 1, 0 )
			[
				SAssignNew( HorizontalAlignmentCombo, SComboBox< TSharedPtr<EHorizontalAlignment> > )
				.OptionsSource( &HorizontalAlignmentComboItems )
				.OnGenerateWidget( this, &SSlateBrushPreview::MakeHorizontalAlignmentComboButtonItemWidget )
				.InitiallySelectedItem( HorizontalAlignmentComboItems[ 0 ] )
				.OnSelectionChanged( this, &SSlateBrushPreview::OnHorizontalAlignmentComboSelectionChanged )
				.Content()
				[
					SNew( STextBlock )
					.Font( IDetailLayoutBuilder::GetDetailFont() )
					.Text( this, &SSlateBrushPreview::GetHorizontalAlignmentComboBoxContent )
					.ToolTipText( this, &SSlateBrushPreview::GetHorizontalAlignmentComboBoxContentToolTip )
				]
			]

			+SUniformGridPanel::Slot( 2, 0 )
			.HAlign( HAlign_Right )
			.VAlign( VAlign_Center )
			[
				SNew( STextBlock )
				.Font( IDetailLayoutBuilder::GetDetailFont() )
				.Text( NSLOCTEXT( "UnrealEd", "VerticalAlignment", "Vertical Alignment" ) )
				.ToolTipText( NSLOCTEXT( "UnrealEd", "PreviewVerticalAlignment", "Vertical alignment for the preview" ) )
			]

			+SUniformGridPanel::Slot( 3, 0 )
			[
				SAssignNew( VerticalAlignmentCombo, SComboBox< TSharedPtr<EVerticalAlignment> > )
				.OptionsSource( &VerticalAlignmentComboItems )
				.OnGenerateWidget( this, &SSlateBrushPreview::MakeVerticalAlignmentComboButtonItemWidget )
				.InitiallySelectedItem( VerticalAlignmentComboItems[ 0 ] )
				.OnSelectionChanged( this, &SSlateBrushPreview::OnVerticalAlignmentComboSelectionChanged )
				.Content()
				[
					SNew( STextBlock )
					.Font( IDetailLayoutBuilder::GetDetailFont() )
					.Text( this, &SSlateBrushPreview::GetVerticalAlignmentComboBoxContent )
					.ToolTipText( this, &SSlateBrushPreview::GetVerticalAlignmentComboBoxContentToolTip )
				]
			];
	}

private:

	/** Margin line types */
	enum EMarginLine
	{
		MarginLine_Left,
		MarginLine_Top,
		MarginLine_Right,
		MarginLine_Bottom,
		MarginLine_Count
	};
	
	/**
	 * SWidget interface
	 */
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE
	{
		if( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton ) 
		{
			bUserIsResizing = true;
			ResizeAnchorPosition = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );
			ResizeAnchorSize = PreviewImageSize;
			return FReply::Handled().CaptureMouse( SharedThis( this ) );
		}
		else
		{
			return FReply::Unhandled();
		}
	}

	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE
	{
		if( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bUserIsResizing )
		{
			bUserIsResizing = false;
			return FReply::Handled().ReleaseMouseCapture();
		}
		else
		{
			return FReply::Unhandled();
		}
	}

	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE
	{
		const FVector2D LocalMouseCoordinates( MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() ) );

		if( bUserIsResizing )
		{
			if( MouseZone >= WZ_RightBorder && MouseZone <= WZ_BottomRightBorder )
			{
				FVector2D Delta( LocalMouseCoordinates - ResizeAnchorPosition );

				if( MouseZone == WZ_RightBorder )
				{
					Delta.Y = 0.0f;
				}
				else if( MouseZone == WZ_BottomBorder )
				{
					Delta.X = 0.0f;
				}

				PreviewImageSize.Set( FMath::Max( ResizeAnchorSize.X + Delta.X, 16.0f ), FMath::Max( ResizeAnchorSize.Y + Delta.Y, 16.0f ) );
				UpdateMarginLinePositions();
			}
		}
		else
		{
			MouseZone = FindMouseZone( LocalMouseCoordinates );
		}
		
		return FReply::Unhandled();
	}
	
	virtual void OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE
	{
		const FVector2D LocalMouseCoordinates( MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() ) );
		MouseZone = FindMouseZone( LocalMouseCoordinates );
		SBorder::OnMouseEnter( MyGeometry, MouseEvent );
	}

	virtual void OnMouseLeave( const FPointerEvent& MouseEvent ) OVERRIDE
	{
		if( !bUserIsResizing )
		{
			MouseZone = WZ_NotInWidget;
			SBorder::OnMouseLeave( MouseEvent );
		}
	}

	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const OVERRIDE
	{
		if( MouseZone == WZ_RightBorder )
		{
			return FCursorReply::Cursor( EMouseCursor::ResizeLeftRight );
		}
		else if( MouseZone == WZ_BottomBorder)
		{
			return FCursorReply::Cursor( EMouseCursor::ResizeUpDown );
		}
		else if( MouseZone == WZ_BottomRightBorder )
		{
			return FCursorReply::Cursor( EMouseCursor::ResizeSouthEast );
		}
		else
		{
			return FCursorReply::Unhandled();
		}
	}

	/**
	 * End of SWidget interface
	 */

	/**
	 * Determine which zone of the widget that the mouse is in
	 */
	EWidgetZone FindMouseZone( const FVector2D& LocalMouseCoordinates ) const
	{
		EWidgetZone InMouseZone = WZ_NotInWidget;
		const FVector2D DesiredSize( GetDesiredSize() );

		if( LocalMouseCoordinates.X > DesiredSize.X - BorderHitSize )
		{
			InMouseZone = LocalMouseCoordinates.Y > DesiredSize.Y - BorderHitSize ? WZ_BottomRightBorder : WZ_RightBorder;
		}
		else if( LocalMouseCoordinates.Y > DesiredSize.Y - BorderHitSize )
		{
			InMouseZone = WZ_BottomBorder;
		}
		else if( LocalMouseCoordinates.X >= BorderHitSize && LocalMouseCoordinates.Y >= BorderHitSize )
		{
			InMouseZone = WZ_InWidget;
		}

		return InMouseZone;
	}

	/**
	 * Return the text for the specified horizontal alignment
	 */
	FString MakeHorizontalAlignmentComboText( EHorizontalAlignment Alignment ) const
	{
		FString AlignmentText;
	
		switch( Alignment )
		{
			case EHorizontalAlignment::HAlign_Fill:
				AlignmentText = NSLOCTEXT( "UnrealEd", "AlignmentFill", "Fill" ).ToString();
				break;
	
			case EHorizontalAlignment::HAlign_Left:
				AlignmentText = NSLOCTEXT( "UnrealEd", "AlignmentLeft", "Left" ).ToString();
				break;
	
			case EHorizontalAlignment::HAlign_Center:
				AlignmentText = NSLOCTEXT( "UnrealEd", "AlignmentCenter", "Center" ).ToString();
				break;
	
			case EHorizontalAlignment::HAlign_Right:
				AlignmentText = NSLOCTEXT( "UnrealEd", "AlignmentRight", "Right" ).ToString();
				break;
		}
	
		return AlignmentText;
	}

	/**
	 * Return the text for the specified vertical alignment
	 */
	FString MakeVerticalAlignmentComboText( EVerticalAlignment Alignment ) const
	{
		FString AlignmentText;
	
		switch( Alignment )
		{
			case EVerticalAlignment::VAlign_Fill:
				AlignmentText = NSLOCTEXT( "UnrealEd", "AlignmentFill", "Fill" ).ToString();
				break;
	
			case EVerticalAlignment::VAlign_Top:
				AlignmentText = NSLOCTEXT( "UnrealEd", "AlignmentTop", "Top" ).ToString();
				break;
	
			case EVerticalAlignment::VAlign_Center:
				AlignmentText = NSLOCTEXT( "UnrealEd", "AlignmentCenter", "Center" ).ToString();
				break;
	
			case EVerticalAlignment::VAlign_Bottom:
				AlignmentText = NSLOCTEXT( "UnrealEd", "AlignmentBottom", "Bottom" ).ToString();
				break;
		}
	
		return AlignmentText;
	}

	/**
	 * Return the tooltip text for the specified horizontal alignment
	 */
	FString MakeHorizontalAlignmentComboToolTipText( EHorizontalAlignment Alignment ) const
	{
		FString ToolTipText;
	
		switch( Alignment )
		{
			case EHorizontalAlignment::HAlign_Fill:
				ToolTipText = NSLOCTEXT( "UnrealEd", "AlignmentFillToolTip", "The image will fill the preview" ).ToString();
				break;
	
			case EHorizontalAlignment::HAlign_Left:
				ToolTipText = NSLOCTEXT( "UnrealEd", "AlignmentLeftToolTip", "The image will be aligned to the left of the preview" ).ToString();
				break;
	
			case EHorizontalAlignment::HAlign_Center:
				ToolTipText = NSLOCTEXT( "UnrealEd", "AlignmentCenterToolTip", "The image will be positioned in the centre of the preview" ).ToString();
				break;
	
			case EHorizontalAlignment::HAlign_Right:
				ToolTipText = NSLOCTEXT( "UnrealEd", "AlignmentRightToolTip", "The image will be aligned from the right of the preview" ).ToString();
				break;
		}
	
		return ToolTipText;
	}

	/**
	 * Return the tooltip text for the specified vertical alignment
	 */
	FString MakeVerticalAlignmentComboToolTipText( EVerticalAlignment Alignment ) const
	{
		FString ToolTipText;
	
		switch( Alignment )
		{
			case EVerticalAlignment::VAlign_Fill:
				ToolTipText = NSLOCTEXT( "UnrealEd", "AlignmentFillToolTip", "The image will fill the preview" ).ToString();
				break;
	
			case EVerticalAlignment::VAlign_Top:
				ToolTipText = NSLOCTEXT( "UnrealEd", "AlignmentTopToolTip", "The image will be aligned to the top of the preview" ).ToString();
				break;
	
			case EVerticalAlignment::VAlign_Center:
				ToolTipText = NSLOCTEXT( "UnrealEd", "AlignmentCenterToolTip", "The image will be positioned in the centre of the preview" ).ToString();
				break;
	
			case EVerticalAlignment::VAlign_Bottom:
				ToolTipText = NSLOCTEXT( "UnrealEd", "AlignmentBottomToolTip", "The image will be aligned from the bottom of the preview" ).ToString();
				break;
		}
	
		return ToolTipText;
	}

	/**
	 * Make the horizontal alignment combo button item widget
	 */
	TSharedRef<SWidget> MakeHorizontalAlignmentComboButtonItemWidget( TSharedPtr<EHorizontalAlignment> Alignment )
	{
		return
			SNew( STextBlock )
			.Text( MakeHorizontalAlignmentComboText( *Alignment ) )
			.ToolTipText( MakeHorizontalAlignmentComboToolTipText( *Alignment ) )
			.Font( IDetailLayoutBuilder::GetDetailFont() );
	}

	/**
	 * Make the vertical alignment combo button item widget
	 */
	TSharedRef<SWidget> MakeVerticalAlignmentComboButtonItemWidget( TSharedPtr<EVerticalAlignment> Alignment )
	{
		return
			SNew( STextBlock )
			.Text( MakeVerticalAlignmentComboText( *Alignment ) )
			.ToolTipText( MakeVerticalAlignmentComboToolTipText( *Alignment ) )
			.Font( IDetailLayoutBuilder::GetDetailFont() );
	}

	/**
	 * Get the horizontal alignment combo box content text
	 */
	FString GetHorizontalAlignmentComboBoxContent() const
	{
		return MakeHorizontalAlignmentComboText( HorizontalAlignment );
	}
	
	/**
	 * Get the vertical alignment combo box content text
	 */
	FString GetVerticalAlignmentComboBoxContent() const
	{
		return MakeVerticalAlignmentComboText( VerticalAlignment );
	}

	/**
	 * Get the horizontal alignment combo box content tooltip text
	 */
	FString GetHorizontalAlignmentComboBoxContentToolTip() const
	{
		return MakeHorizontalAlignmentComboToolTipText( HorizontalAlignment );
	}
	
	/**
	 * Get the vertical alignment combo box content tooltip text
	 */
	FString GetVerticalAlignmentComboBoxContentToolTip() const
	{
		return MakeVerticalAlignmentComboToolTipText( VerticalAlignment );
	}

	/**
	 * Cache the slate brush property values
	 */
	void CachePropertyValues()
	{
		UObject* ResourceObject;
		FPropertyAccess::Result Result = ResourceObjectProperty->GetValue( ResourceObject );
		if( Result == FPropertyAccess::Success )
		{
			UTexture2D* BrushTexture = Cast<UTexture2D>(ResourceObject);
			CachedTextureSize = BrushTexture ? FVector2D( BrushTexture->GetSizeX(), BrushTexture->GetSizeY() ) : FVector2D( 32.0f, 32.0f );

			TArray<void*> RawData;
			ImageSizeProperty->AccessRawData( RawData );
			if( RawData.Num() > 0 && RawData[ 0 ] != NULL )
			{
				CachedImageSizeValue = *static_cast<FVector2D*>(RawData[ 0 ]);
			}

			uint8 DrawAsType;
			Result = DrawAsProperty->GetValue( DrawAsType );
			if( Result == FPropertyAccess::Success )
			{
				CachedDrawAsType = static_cast<ESlateBrushDrawType::Type>(DrawAsType);
			}

			uint8 TilingType;
			Result = TilingProperty->GetValue( TilingType );
			if( Result == FPropertyAccess::Success )
			{
				CachedTilingType = static_cast<ESlateBrushTileType::Type>(TilingType);
			}

			MarginProperty->AccessRawData( RawData );
			if( RawData.Num() > 0 && RawData[ 0 ] != NULL )
			{
				CachedMarginPropertyValue = *static_cast<FMargin*>(RawData[ 0 ]);
			}
		}
	}

	/**
	 * On horizontal alignment selection change
	 */
	void OnHorizontalAlignmentComboSelectionChanged( TSharedPtr<EHorizontalAlignment> NewSelection, ESelectInfo::Type /*SelectInfo*/ )
	{
		HorizontalAlignment = *NewSelection;
		UpdateOverlayAlignment();
		UpdateMarginLinePositions();
	}

	/**
	 * On vertical alignment selection change
	 */
	void OnVerticalAlignmentComboSelectionChanged( TSharedPtr<EVerticalAlignment> NewSelection, ESelectInfo::Type /*SelectInfo*/ )
	{
		VerticalAlignment = *NewSelection;
		UpdateOverlayAlignment();
		UpdateMarginLinePositions();
	}

	/**
	 * Get the horizontal alignment
	 */
	EHorizontalAlignment GetHorizontalAlignment() const
	{
		return HorizontalAlignment;
	}

	/**
	 * Get the vertical alignment
	 */
	EVerticalAlignment GetVerticalAlignment() const
	{
		return VerticalAlignment;
	}

	/**
	 * Update the margin line positions
	 */
	void UpdateMarginLinePositions()
	{
		const FVector2D DrawSize( (HorizontalAlignment == EHorizontalAlignment::HAlign_Fill || PreviewImageSize.X < CachedImageSizeValue.X) ? PreviewImageSize.X : CachedImageSizeValue.X,
								  (VerticalAlignment == EVerticalAlignment::VAlign_Fill || PreviewImageSize.Y < CachedImageSizeValue.Y) ? PreviewImageSize.Y : CachedImageSizeValue.Y );

		FVector2D Position( 0.0f, 0.0f );

		if( PreviewImageSize.X > DrawSize.X )
		{
			if( HorizontalAlignment == EHorizontalAlignment::HAlign_Center )
			{
				Position.X = (PreviewImageSize.X - DrawSize.X) * 0.5f;
			}
			else if( HorizontalAlignment == EHorizontalAlignment::HAlign_Right )
			{
				Position.X = PreviewImageSize.X - DrawSize.X;
			}
		}

		if( PreviewImageSize.Y > DrawSize.Y )
		{
			if( VerticalAlignment == EVerticalAlignment::VAlign_Center )
			{
				Position.Y = (PreviewImageSize.Y - DrawSize.Y) * 0.5f;
			}
			else if( VerticalAlignment == EVerticalAlignment::VAlign_Bottom )
			{
				Position.Y = PreviewImageSize.Y - DrawSize.Y;
			}
		}

		float LeftMargin = CachedTextureSize.X * CachedMarginPropertyValue.Left;
		float RightMargin = DrawSize.X - CachedTextureSize.X * CachedMarginPropertyValue.Right;
		float TopMargin = CachedTextureSize.Y * CachedMarginPropertyValue.Top;
		float BottomMargin = DrawSize.Y - CachedTextureSize.Y * CachedMarginPropertyValue.Bottom;

		if( RightMargin < LeftMargin )
		{
			LeftMargin = DrawSize.X * 0.5f;
			RightMargin = LeftMargin;
		}

		if( BottomMargin < TopMargin )
		{
			TopMargin = DrawSize.Y * 0.5f;
			BottomMargin = TopMargin;
		}

		MarginLinePositions[ MarginLine_Left ] = FVector2D( ImagePadding + Position.X + LeftMargin, 1.0f );
		MarginLinePositions[ MarginLine_Right ] = FVector2D( ImagePadding + Position.X + RightMargin, 1.0f );
		MarginLinePositions[ MarginLine_Top ] = FVector2D( 1.0f, ImagePadding + Position.Y + TopMargin );
		MarginLinePositions[ MarginLine_Bottom ] = FVector2D( 1.0f, ImagePadding + Position.Y + BottomMargin );
	}

	/**
	 * Set the default preview alignment based on the DrawAs and Tiling properties
	 */
	void SetDefaultAlignment()
	{
		HorizontalAlignment = EHorizontalAlignment::HAlign_Fill;
		VerticalAlignment = EVerticalAlignment::VAlign_Fill;

		if( CachedDrawAsType == ESlateBrushDrawType::Image )
		{
			switch( CachedTilingType )
			{
			case ESlateBrushTileType::NoTile:
				HorizontalAlignment = EHorizontalAlignment::HAlign_Center;
				VerticalAlignment = EVerticalAlignment::VAlign_Center;
				break;

			case ESlateBrushTileType::Horizontal:
				VerticalAlignment = EVerticalAlignment::VAlign_Center;
				break;

			case ESlateBrushTileType::Vertical:
				HorizontalAlignment = EHorizontalAlignment::HAlign_Center;
				break;
			}
		}

		UpdateOverlayAlignment();

		if( HorizontalAlignmentCombo.IsValid() )
		{
			HorizontalAlignmentCombo->SetSelectedItem( HorizontalAlignmentComboItems[ HorizontalAlignment ] );
			HorizontalAlignmentCombo->RefreshOptions();
			VerticalAlignmentCombo->SetSelectedItem( VerticalAlignmentComboItems[ VerticalAlignment ] );
			VerticalAlignmentCombo->RefreshOptions();
		}
	}

	/**
	 * Update the preview image overlay slot alignment
	 */
	void UpdateOverlayAlignment()
	{
		OverlaySlot->HAlign( HorizontalAlignment );
		OverlaySlot->VAlign( VerticalAlignment );
	}

	/**
	 * Update the preview image size
	 */
	void UpdatePreviewImageSize()
	{
		PreviewImageSize = CachedTextureSize;
	}

	/**
	 * Called on change of Slate Brush DrawAs property
	 */
	void OnDrawAsChanged()
	{
		CachePropertyValues();
		
		if( CachedDrawAsType != ESlateBrushDrawType::Box && CachedDrawAsType != ESlateBrushDrawType::Border )
		{
			TArray<void*> RawData;
			MarginProperty->AccessRawData( RawData );
			check( RawData[ 0 ] != NULL );
			*static_cast<FMargin*>(RawData[ 0 ]) = FMargin();
		}
		else
		{
			CachedTilingType = ESlateBrushTileType::NoTile;
			FPropertyAccess::Result Result = TilingProperty->SetValue( static_cast<uint8>(ESlateBrushTileType::NoTile) );
			check( Result == FPropertyAccess::Success );
		}

		SetDefaultAlignment();
		UpdateMarginLinePositions();
	}

	/**
	 * Called on change of Slate Brush Tiling property
	 */
	void OnTilingChanged()
	{
		CachePropertyValues();
		SetDefaultAlignment();
		UpdateMarginLinePositions();
	}

	/**
	 * Called on change of Slate Brush ResourceObject property
	 */
	void OnBrushResourceChanged()
	{
		CachePropertyValues();
		ImageSizeProperty->SetValue( CachedTextureSize );
		UpdatePreviewImageSize();
		UpdateMarginLinePositions();
	}

	/**
	 * Called on change of Slate Brush ImageSize property
	 */
	void OnImageSizeChanged()
	{
		CachePropertyValues();
		UpdateMarginLinePositions();
	}

	/**
	 * Called on change of Slate Brush Margin property
	 */
	void OnMarginChanged()
	{
		CachePropertyValues();
		UpdateMarginLinePositions();
	}

	/**
	 * Get the preview width
	 */
	FOptionalSize GetPreviewWidth() const
	{
		return PreviewImageSize.X + ImagePadding * 2.0f;
	}

	/**
	 * Get the preview height
	 */
	FOptionalSize GetPreviewHeight() const
	{
		return PreviewImageSize.Y + ImagePadding * 2.0f;
	}

	/**
	 * Get the margin line visibility
	 */
	EVisibility GetMarginLineVisibility() const
	{
		return (CachedDrawAsType == ESlateBrushDrawType::Box || CachedDrawAsType == ESlateBrushDrawType::Border) ? EVisibility::Visible : EVisibility::Hidden;
	}

	/**
	 * Get the left margin line position
	 */
	FVector2D GetLeftMarginLinePosition() const
	{
		return MarginLinePositions[ MarginLine_Left ];
	}

	/**
	 * Get the right margin line position
	 */
	FVector2D GetRightMarginLinePosition() const
	{
		return MarginLinePositions[ MarginLine_Right ];
	}

	/**
	 * Get the top margin line position
	 */
	FVector2D GetTopMarginLinePosition() const
	{
		return MarginLinePositions[ MarginLine_Top ];
	}

	/**
	 * Get the bottom margin line position
	 */
	FVector2D GetBottomMarginLinePosition() const
	{
		return MarginLinePositions[ MarginLine_Bottom ];
	}

	/** Padding between the preview image and the checkerboard background */
	static const float ImagePadding;

	/** The thickness of the border for mouse hit testing */
	static const float BorderHitSize;

	/** Alignment combo items */
	TArray< TSharedPtr<EHorizontalAlignment> > HorizontalAlignmentComboItems;
	TArray< TSharedPtr<EVerticalAlignment> > VerticalAlignmentComboItems;

	/** Alignment combos */
	TSharedPtr< SComboBox< TSharedPtr<EHorizontalAlignment> > > HorizontalAlignmentCombo;
	TSharedPtr< SComboBox< TSharedPtr<EVerticalAlignment> > > VerticalAlignmentCombo;

	/** Overlay slot which contains the preview image */
	SOverlay::FOverlaySlot* OverlaySlot;

	/** Slate Brush properties */
	TSharedPtr<IPropertyHandle> DrawAsProperty;
	TSharedPtr<IPropertyHandle> TilingProperty;
	TSharedPtr<IPropertyHandle> ImageSizeProperty;
	TSharedPtr<IPropertyHandle> MarginProperty;
	TSharedPtr<IPropertyHandle> ResourceObjectProperty;

	/** Cached Slate Brush property values */
	FVector2D CachedTextureSize;
	FVector2D CachedImageSizeValue;
	ESlateBrushDrawType::Type CachedDrawAsType;
	ESlateBrushTileType::Type CachedTilingType;
	FMargin CachedMarginPropertyValue;

	/** Preview alignment */
	EHorizontalAlignment HorizontalAlignment;
	EVerticalAlignment VerticalAlignment;

	/** Preview image size */
	FVector2D PreviewImageSize;

	/** Margin line positions */
	FVector2D MarginLinePositions[ MarginLine_Count ];

	/** The current widget zone the mouse is in */
	EWidgetZone MouseZone;

	/** If true the user is resizing the preview */
	bool bUserIsResizing;

	/** Preview resize anchor position */
	FVector2D ResizeAnchorPosition;

	/** Size of the preview image on begin of resize */
	FVector2D ResizeAnchorSize;
};

const float SSlateBrushPreview::ImagePadding = 5.0f;
const float SSlateBrushPreview::BorderHitSize = 8.0f;


TSharedRef<IStructCustomization> FSlateBrushStructCustomization::MakeInstance() 
{
	return MakeShareable( new FSlateBrushStructCustomization() );
}

void FSlateBrushStructCustomization::CustomizeStructHeader( TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IStructCustomizationUtils& StructCustomizationUtils )
{
	bool ShowOnlyInnerProperties = StructPropertyHandle->GetProperty()->HasMetaData(TEXT("ShowOnlyInnerProperties"));

	if ( !ShowOnlyInnerProperties )
	{
		HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		];
	}
}

void FSlateBrushStructCustomization::CustomizeStructChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IStructCustomizationUtils& StructCustomizationUtils )
{
	// Add the child properties
	TSharedPtr<IPropertyHandle> ImageSizeProperty = StructPropertyHandle->GetChildHandle( TEXT("ImageSize") );
	DrawAsProperty = StructPropertyHandle->GetChildHandle( TEXT("DrawAs") );
	TSharedPtr<IPropertyHandle> TilingProperty = StructPropertyHandle->GetChildHandle( TEXT("Tiling") );
	TSharedPtr<IPropertyHandle> MarginProperty = StructPropertyHandle->GetChildHandle( TEXT("Margin") );
	TSharedPtr<IPropertyHandle> TintProperty = StructPropertyHandle->GetChildHandle( TEXT("TintColor") );
	TSharedPtr<IPropertyHandle> ResourceObjectProperty = StructPropertyHandle->GetChildHandle( TEXT("ResourceObject") );

	StructBuilder.AddChildContent( NSLOCTEXT( "SlateBrushCustomization", "ResourceObjectFilterString", "Resource" ).ToString() )
	.NameContent()
	[
		ResourceObjectProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(250.0f)
	.MaxDesiredWidth(0.0f)
	[
		SNew( SObjectPropertyEntryBox )
		.PropertyHandle( ResourceObjectProperty )
		.ThumbnailPool( StructCustomizationUtils.GetThumbnailPool() )
		.AllowedClass( UTexture2D::StaticClass() )
	];

	StructBuilder.AddChildProperty( ImageSizeProperty.ToSharedRef() );
	StructBuilder.AddChildProperty( DrawAsProperty.ToSharedRef() );
	StructBuilder.AddChildProperty( TilingProperty.ToSharedRef() )
	.Visibility( TAttribute<EVisibility>::Create( TAttribute<EVisibility>::FGetter::CreateSP( this, &FSlateBrushStructCustomization::GetTilingPropertyVisibility ) ) );
	StructBuilder.AddChildProperty( TintProperty.ToSharedRef() );
	StructBuilder.AddChildProperty( MarginProperty.ToSharedRef() )
	.Visibility( TAttribute<EVisibility>::Create( TAttribute<EVisibility>::FGetter::CreateSP( this, &FSlateBrushStructCustomization::GetMarginPropertyVisibility ) ) );

	// Create the Slate Brush Preview widget and add the Preview group
	TArray<void*> RawData;
	StructPropertyHandle->AccessRawData( RawData );

	// Can only display the preview with one brush
	if( RawData.Num() == 1 )
	{
		FSlateBrush* Brush = static_cast<FSlateBrush*>(RawData[ 0 ]);

		TSharedRef<SSlateBrushPreview> Preview = SNew( SSlateBrushPreview )
			.DrawAsProperty( DrawAsProperty )
			.TilingProperty( TilingProperty )
			.ImageSizeProperty( ImageSizeProperty )
			.MarginProperty( MarginProperty )
			.ResourceObjectProperty( ResourceObjectProperty )
			.SlateBrush( Brush );

		IDetailGroup& PreviewGroup = StructBuilder.AddChildGroup( TEXT( "Preview" ), TEXT("") );

		PreviewGroup
			.HeaderRow()
			.NameContent()
			[
				StructPropertyHandle->CreatePropertyNameWidget( NSLOCTEXT( "UnrealEd", "Preview", "Preview" ).ToString(), false )
			]
		.ValueContent()
			.MinDesiredWidth( 1 )
			.MaxDesiredWidth( 4096 )
			[
				Preview->GenerateAlignmentComboBoxes()
			];

		PreviewGroup
			.AddWidgetRow()
			.ValueContent()
			.MinDesiredWidth( 1 )
			.MaxDesiredWidth( 4096 )
			[
				Preview
			];
	}
}

EVisibility FSlateBrushStructCustomization::GetTilingPropertyVisibility() const
{
	uint8 DrawAsType;
	FPropertyAccess::Result Result = DrawAsProperty->GetValue( DrawAsType );

	return (Result == FPropertyAccess::MultipleValues || DrawAsType == ESlateBrushDrawType::Image) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FSlateBrushStructCustomization::GetMarginPropertyVisibility() const
{
	uint8 DrawAsType;
	FPropertyAccess::Result Result = DrawAsProperty->GetValue( DrawAsType );

	return (Result == FPropertyAccess::MultipleValues || DrawAsType == ESlateBrushDrawType::Box || DrawAsType == ESlateBrushDrawType::Border) ? EVisibility::Visible : EVisibility::Collapsed;
}
