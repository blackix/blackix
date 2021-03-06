// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "ReferenceViewerPrivatePCH.h"
#include "SReferenceNode.h"
#include "AssetThumbnail.h"
#include "AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "ReferenceViewer"

void SReferenceNode::Construct( const FArguments& InArgs, UEdGraphNode_Reference* InNode )
{
	const int32 ThumbnailSize = 128;
	bUsesThumbnail = InNode->UsesThumbnail();

	if (bUsesThumbnail)
	{
		// Create a thumbnail from the graph's thumbnail pool
		TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = InNode->GetReferenceViewerGraph()->GetAssetThumbnailPool();
		AssetThumbnail = MakeShareable( new FAssetThumbnail( InNode->GetAssetData(), ThumbnailSize, ThumbnailSize, AssetThumbnailPool ) );
	}
	else
	{
		// Just make a generic thumbnail
		AssetThumbnail = MakeShareable( new FAssetThumbnail( InNode->GetAssetData(), ThumbnailSize, ThumbnailSize, NULL ) );
	}

	GraphNode = InNode;
	SetCursor( EMouseCursor::CardinalCross );
	UpdateGraphNode();
}

void SReferenceNode::UpdateGraphNode()
{
	OutputPins.Empty();

	// Reset variables that are going to be exposed, in case we are refreshing an already setup node.
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	UpdateErrorInfo();

	//
	//             ______________________
	//            |      TITLE AREA      |
	//            +-------+------+-------+
	//            | (>) L |      | R (>) |
	//            | (>) E |      | I (>) |
	//            | (>) F |      | G (>) |
	//            | (>) T |      | H (>) |
	//            |       |      | T (>) |
	//            |_______|______|_______|
	//
	TSharedPtr<SVerticalBox> MainVerticalBox;
	TSharedPtr<SErrorText> ErrorText;
	TSharedPtr<SNodeTitle> NodeTitle = SNew(SNodeTitle, GraphNode);

	TSharedRef<SWidget> ThumbnailWidget = SNullWidget::NullWidget;
	if ( AssetThumbnail.IsValid() )
	{
		ThumbnailWidget =
			SNew(SBox)
			.WidthOverride(AssetThumbnail->GetSize().X)
			.HeightOverride(AssetThumbnail->GetSize().Y)
			[
				AssetThumbnail->MakeThumbnailWidget(/*bAllowFadeIn=*/bUsesThumbnail, /*bForceGenericThumbnail=*/!bUsesThumbnail)
			];
	}

	ContentScale.Bind( this, &SReferenceNode::GetContentScale );
	ChildSlot
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SAssignNew(MainVerticalBox, SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush( "Graph.Node.Body" ) )
			.Padding(0)
			[
				SNew(SVerticalBox)
				. ToolTipText( this, &SReferenceNode::GetNodeTooltip )
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Top)
				[
					SNew(SOverlay)
					+SOverlay::Slot()
					[
						SNew(SImage)
						.Image( FEditorStyle::GetBrush("Graph.Node.TitleGloss") )
					]
					+SOverlay::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(SBorder)
						.BorderImage( FEditorStyle::GetBrush("Graph.Node.ColorSpill") )
						// The extra margin on the right
						// is for making the color spill stretch well past the node title
						.Padding( FMargin(10,5,30,3) )
						.BorderBackgroundColor( this, &SReferenceNode::GetNodeTitleColor )
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
								.AutoHeight()
							[
								SAssignNew(InlineEditableText, SInlineEditableTextBlock)
								.Style( FEditorStyle::Get(), "Graph.Node.NodeTitleInlineEditableText" )
								.Text( NodeTitle.Get(), &SNodeTitle::GetHeadTitle )
								.OnVerifyTextChanged(this, &SReferenceNode::OnVerifyNameTextChanged)
								.OnTextCommitted(this, &SReferenceNode::OnNameTextCommited)
								.IsReadOnly( this, &SReferenceNode::IsNameReadOnly )
								.IsSelected(this, &SReferenceNode::IsSelectedExclusively)
							]
							+SVerticalBox::Slot()
								.AutoHeight()
							[
								NodeTitle.ToSharedRef()
							]
						]
					]
					+SOverlay::Slot()
					.VAlign(VAlign_Top)
					[
						SNew(SBorder)
						.BorderImage( FEditorStyle::GetBrush( "Graph.Node.TitleHighlight" ) )
						.Visibility(EVisibility::HitTestInvisible)			
						[
							SNew(SSpacer)
							.Size(FVector2D(20,20))
						]
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(1.0f)
				[
					// POPUP ERROR MESSAGE
					SAssignNew(ErrorText, SErrorText )
					.BackgroundColor( this, &SReferenceNode::GetErrorColor )
					.ToolTipText( this, &SReferenceNode::GetErrorMsgToolTip )
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Top)
				[
					// NODE CONTENT AREA
					SNew(SBorder)
					.BorderImage( FEditorStyle::GetBrush("NoBorder") )
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.Padding( FMargin(0,3) )
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							// LEFT
							SNew(SBox)
							.WidthOverride(40)
							[
								SAssignNew(LeftNodeBox, SVerticalBox)
							]
						]
						
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.FillWidth(1.0f)
						[
							// Thumbnail
							ThumbnailWidget
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							// RIGHT
							SNew(SBox)
							.WidthOverride(40)
							[
								SAssignNew(RightNodeBox, SVerticalBox)
							]
						]
					]
				]
			]
		]
	];

	ErrorReporting = ErrorText;
	ErrorReporting->SetError(ErrorMsg);
	CreateBelowWidgetControls(MainVerticalBox);

	CreatePinWidgets();
}

#undef LOCTEXT_NAMESPACE
