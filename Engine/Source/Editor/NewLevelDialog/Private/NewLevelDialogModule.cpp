// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "NewLevelDialogPrivatePCH.h"
#include "ModuleManager.h"
#include "Slate.h"

#define LOCTEXT_NAMESPACE "NewLevelDialog"

/**
 * Widget class for rendering a UTexture2D in Slate
 * Work-in-progress idea that is defined here so that others don't use it yet
 */
class STexture2DView : public SLeafWidget, public ISlateViewport, private TSlateTexture<FTexture2DRHIRef>
{
public:
	SLATE_BEGIN_ARGS(STexture2DView) {}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, UTexture2D* InTexture )
	{
		Size = FIntPoint(InTexture->GetSizeX(), InTexture->GetSizeY());

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			UpdateSTexture2DView,
			STexture2DView*,TextureView,this,
			UTexture2D*,Texture,InTexture,
			{
				TextureView->ShaderResource = ((FTexture2DResource*)(Texture->Resource))->GetTexture2DRHI();
			});
	}

	virtual int32 OnPaint( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
	{
		bool bEnableGammaCorrection = true;
		bool bAllowBlending = false;

		FSlateDrawElement::MakeViewport(
			OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), SharedThis( this ), MyClippingRect, bEnableGammaCorrection, bAllowBlending, ESlateDrawEffect::None, InWidgetStyle.GetColorAndOpacityTint());
		return LayerId;
	}

	virtual FVector2D ComputeDesiredSize() const
	{
		return FVector2D(Size.X, Size.Y);
	}

	virtual FIntPoint GetSize() const { return Size; }
	virtual class FSlateShaderResource* GetViewportRenderTargetTexture() const { return ShaderResource ? (FSlateShaderResource*)this : NULL; }
	virtual bool RequiresVsync() const OVERRIDE { return false; }

	// FSlateShaderResource implementation.
	virtual uint32 GetWidth() const { return Size.X; }
	virtual uint32 GetHeight() const { return Size.Y; }
private:
	FIntPoint Size;
};

/**
 * Main widget class showing a table of level templates as labeled thumbnails
 * for the user to select by clicking.
 */
class SNewLevelDialog : public SCompoundWidget
{
private:
	struct FTemplateListItem
	{
		FTemplateMapInfo TemplateMapInfo;
		bool bIsNewLevelItem;
	};

public:
	SLATE_BEGIN_ARGS(SNewLevelDialog)
		{}
		/** A pointer to the parent window */
		SLATE_ATTRIBUTE(TSharedPtr<SWindow>, ParentWindow)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		ParentWindowPtr = InArgs._ParentWindow.Get();

		OutTemplateMapPackageName = TEXT("");
		bUserClickedOkay = false;

		if ( GUnrealEd )
		{
			const TArray<FTemplateMapInfo>& TemplateInfoList = GUnrealEd->TemplateMapInfos;

			// Build a list of items - one for each template
			for (int32 i=0; i < TemplateInfoList.Num(); i++)
			{
				TSharedPtr<FTemplateListItem> Item = MakeShareable(new FTemplateListItem());
				Item->TemplateMapInfo = TemplateInfoList[i];
				Item->bIsNewLevelItem = false;
				TemplateItemsList.Add(Item);
			}
		}

		// Add an extra item for creating a new, blank level
		TSharedPtr<FTemplateListItem> NewItem = MakeShareable(new FTemplateListItem());
		NewItem->bIsNewLevelItem = true;
		TemplateItemsList.Add(NewItem);

		TSharedRef<SButton> CancelButton = SNew(SButton)
			.ContentPadding(FMargin(10,3))
			.Text(LOCTEXT("Cancel", "Cancel"))
			.OnClicked(this, &SNewLevelDialog::OnCancelClicked);

		this->ChildSlot
		[
			SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.FillHeight(1)
				[
					SNew(SScrollBox)
					+SScrollBox::Slot()
					.Padding(15)
					[
						SAssignNew(TemplatesWrapBox, SWrapBox)
						.PreferredWidth(DEFAULT_WINDOW_SIZE.X - 35.0)   // apparently no way to auto size the width of wrap boxes
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.Padding(6,2)
				[
					CancelButton
				]
			]
		];

		// Give the cancel button initial focus so that the escape key can be checked for
		ParentWindowPtr.Pin().Get()->SetWidgetToFocusOnActivate( CancelButton );

		// Insert items into slots in the wrap box, TemplatesWrapBox
		AddItemsToWrapBox();
	}

	/** A default window size for the dialog */
	static const FVector2D DEFAULT_WINDOW_SIZE;

	/** Level thumbnail image size in pixels */
	static const float THUMBNAIL_SIZE;

	FString GetChosenTemplate() const { return OutTemplateMapPackageName; }
	bool IsTemplateChosen() const { return bUserClickedOkay; }

private:
	void AddItemsToWrapBox()
	{
		for (int32 i = 0; i < TemplateItemsList.Num(); i++)
		{
			TSharedPtr<FTemplateListItem> Template = TemplateItemsList[i];

			TemplatesWrapBox->AddSlot()
			[
				GetWidgetForTemplate(TemplateItemsList[i])
			];
		}
	}

	TSharedRef<SWidget> GetWidgetForTemplate(TSharedPtr<FTemplateListItem> Template)
	{
		TSharedPtr<SWidget> Image;
		FString Text;
		if (Template->bIsNewLevelItem)
		{
			// New level item
			Image = SNew(SImage).Image(FEditorStyle::GetBrush(TEXT("NewLevelDialog.Blank")));
			Text = LOCTEXT("NewLevelItemLabel", "Empty Level").ToString();
		}
		else if (Template->TemplateMapInfo.ThumbnailTexture)
		{
			// Level with thumbnail
			Image = SNew(STexture2DView, Template->TemplateMapInfo.ThumbnailTexture);
			Text = Template->TemplateMapInfo.ThumbnailTexture->GetName();
		}
		else
		{
			// Level with no thumbnail
			Image = SNew(SImage).Image(FEditorStyle::GetBrush(TEXT("NewLevelDialog.Default")));
			Text = FPackageName::GetShortName(Template->TemplateMapInfo.Map);
		}

		Text.ReplaceInline(TEXT("_"), TEXT(" "));
		Image->SetCursor(EMouseCursor::Hand);

		return SNew(SBox)
			.HeightOverride(THUMBNAIL_SIZE)
			.WidthOverride(THUMBNAIL_SIZE)
			.Padding(5)
			[
				SNew(SButton)
				.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.OnClicked( this, &SNewLevelDialog::OnTemplateClicked, Template )
				[
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush(TEXT("NewLevelDialog.BlackBorder")))
					.ColorAndOpacity(this, &SNewLevelDialog::GetTemplateColor, Image )
					.Padding(6)
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						[
							Image.ToSharedRef()
						]
						+SOverlay::Slot()
						.VAlign(VAlign_Bottom)
						.HAlign(HAlign_Right)
						.Padding(FMargin(0, 0, 5, 5))
						[
							SNew(STextBlock)
							.Visibility(EVisibility::HitTestInvisible)
							.ShadowOffset(FVector2D(1.0f, 1.0f))
							.ColorAndOpacity(FLinearColor(1,1,1))
							.Text(Text)
						]
					]
				]
			];
	}

	FReply OnTemplateClicked(TSharedPtr<FTemplateListItem> Template)
	{
		if (!Template->bIsNewLevelItem)
		{
			OutTemplateMapPackageName = Template->TemplateMapInfo.Map;
		}
		bUserClickedOkay = true;

		ParentWindowPtr.Pin()->RequestDestroyWindow();
		return FReply::Handled();
	}

	FReply OnCancelClicked()
	{
		bUserClickedOkay = false;

		ParentWindowPtr.Pin()->RequestDestroyWindow();
		return FReply::Handled();
	}

	FLinearColor GetTemplateColor(TSharedPtr<SWidget> TemplateWidget) const
	{
		if (TemplateWidget->IsHovered())
		{
			return FLinearColor(1,1,1);
		}
		else
		{
			return FLinearColor(0.75,0.75,0.75);
		}
	}

	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent ) OVERRIDE
	{
		if( InKeyboardEvent.GetKey() == EKeys::Escape )
		{
			return OnCancelClicked();
		}

		return SCompoundWidget::OnKeyDown( MyGeometry, InKeyboardEvent );
	}

	/** Pointer to the parent window, so we know to destroy it when done */
	TWeakPtr<SWindow> ParentWindowPtr;

	TArray<TSharedPtr<FTemplateListItem>> TemplateItemsList;
	TSharedPtr<SWrapBox> TemplatesWrapBox;
	FString OutTemplateMapPackageName;
	bool bUserClickedOkay;
};

const FVector2D SNewLevelDialog::DEFAULT_WINDOW_SIZE = FVector2D(527, 418);
const float SNewLevelDialog::THUMBNAIL_SIZE = 160.0f;

IMPLEMENT_MODULE( FNewLevelDialogModule, NewLevelDialog );

const FName FNewLevelDialogModule::NewLevelDialogAppIdentifier( TEXT( "NewLevelDialogApp" ) );

void FNewLevelDialogModule::StartupModule()
{
}

void FNewLevelDialogModule::ShutdownModule()
{
}

bool FNewLevelDialogModule::CreateAndShowNewLevelDialog( const TSharedPtr<const SWidget> ParentWidget, FString& OutTemplateMapPackageName )
{
	
	TSharedPtr<SWindow> NewLevelWindow =
		SNew(SWindow)
		.Title(LOCTEXT("WindowHeader", "New Level"))
		.ClientSize(SNewLevelDialog::DEFAULT_WINDOW_SIZE)
		.SizingRule( ESizingRule::FixedSize )
		.SupportsMinimize(false) .SupportsMaximize(false);

	TSharedRef<SNewLevelDialog> NewLevelDialog =
		SNew(SNewLevelDialog)
		.ParentWindow(NewLevelWindow);

	NewLevelWindow->SetContent(NewLevelDialog);

	FSlateApplication::Get().AddModalWindow(NewLevelWindow.ToSharedRef(), ParentWidget);

	OutTemplateMapPackageName = NewLevelDialog->GetChosenTemplate();
	return NewLevelDialog->IsTemplateChosen();
}

#undef LOCTEXT_NAMESPACE
