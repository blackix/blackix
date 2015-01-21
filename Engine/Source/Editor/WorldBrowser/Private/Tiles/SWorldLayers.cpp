// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "WorldBrowserPrivatePCH.h"

#include "WorldTileCollectionModel.h"
#include "SWorldLayers.h"
#include "SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "WorldBrowser"

//----------------------------------------------------------------
//
//
//----------------------------------------------------------------
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SNewLayerPopup::Construct(const FArguments& InArgs)
{
	OnCreateLayer = InArgs._OnCreateLayer;
	LayerData.Name = InArgs._DefaultName;
	
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
		.Padding(10)
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2,2,0,0)
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Layer_Name", "Name:"))
				]

				+SHorizontalBox::Slot()
				.Padding(4,0,0,0)
				[
					SNew(SEditableTextBox)
					.Text(this, &SNewLayerPopup::GetLayerName)
					.SelectAllTextWhenFocused(true)
					//.OnTextCommitted(this, &SNewLayerPopup::SetLayerName)
					.OnTextChanged(this, &SNewLayerPopup::SetLayerName)
				]

			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2,2,0,0)
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SCheckBox)
					.IsChecked(this, &SNewLayerPopup::GetDistanceStreamingState)
					.OnCheckStateChanged(this, &SNewLayerPopup::OnDistanceStreamingStateChanged)
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SNumericEntryBox<int32>)
					.IsEnabled(this, &SNewLayerPopup::IsDistanceStreamingEnabled)
					.Value(this, &SNewLayerPopup::GetStreamingDistance)
					.MinValue(1)
					.MaxValue(TNumericLimits<int32>::Max())
					.OnValueChanged(this, &SNewLayerPopup::SetStreamingDistance)
					.LabelPadding(0)
					.Label()
					[
						SNumericEntryBox<int32>::BuildLabel(
							LOCTEXT("LayerStreamingDistance", "Streaming distance"), 
							FLinearColor::White, SNumericEntryBox<int32>::RedLabelBackgroundColor
							)

					]
				]
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2,2,0,0)
			[
				SNew(SButton)
				.OnClicked(this, &SNewLayerPopup::OnClickedCreate)
				.Text(LOCTEXT("Layer_Create", "Create"))
			]

		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FReply SNewLayerPopup::OnClickedCreate()
{
	if (OnCreateLayer.IsBound())
	{
		return OnCreateLayer.Execute(LayerData);
	}
	
	return FReply::Unhandled();
}

/** A class for check boxes in the layer list. 
 *	If you double click a layer checkbox, you will enable it and disable all others 
 *	If you Ctrl+Click a layer checkbox, you will add/remove it from selection list
 */
class SLayerCheckBox : public SCheckBox
{
public:
	void SetOnLayerDoubleClicked(const FOnClicked& NewLayerDoubleClicked)
	{
		OnLayerDoubleClicked = NewLayerDoubleClicked;
	}

	void SetOnLayerCtrlClicked(const FOnClicked& NewLayerCtrlClicked)
	{
		OnLayerCtrlClicked = NewLayerCtrlClicked;
	}
	
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override
	{
		if (OnLayerDoubleClicked.IsBound())
		{
			return OnLayerDoubleClicked.Execute();
		}
		else
		{
			return SCheckBox::OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);
		}
	}

	virtual FReply OnMouseButtonUp(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override
	{
		if (!InMouseEvent.IsControlDown())
		{
			return SCheckBox::OnMouseButtonUp(InMyGeometry, InMouseEvent);
		}
		
		if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			bIsPressed = false;

			if (IsHovered() && HasMouseCapture())
			{
				if (OnLayerCtrlClicked.IsBound())
				{
					return OnLayerCtrlClicked.Execute();
				}
			}
		}

		return FReply::Handled().ReleaseMouseCapture();
	}


private:
	FOnClicked OnLayerDoubleClicked;
	FOnClicked OnLayerCtrlClicked;
};

//----------------------------------------------------------------
//
//
//----------------------------------------------------------------
SWorldLayerButton::~SWorldLayerButton()
{
}

void SWorldLayerButton::Construct(const FArguments& InArgs)
{
	WorldModel = InArgs._InWorldModel;
	WorldLayer = InArgs._WorldLayer;

	TSharedPtr<SLayerCheckBox> CheckBox;
	
	ChildSlot
		[
			SNew(SBorder)
				.BorderBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.2f))
				.BorderImage(FEditorStyle::GetBrush("ContentBrowser.FilterButtonBorder"))
				[
					SAssignNew(CheckBox, SLayerCheckBox)
						.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
						.OnCheckStateChanged(this, &SWorldLayerButton::OnCheckStateChanged)
						.IsChecked(this, &SWorldLayerButton::IsChecked)
						.OnGetMenuContent(this, &SWorldLayerButton::GetRightClickMenu)
						.Padding(3)
						[
							SNew(STextBlock)
								.Font(FEditorStyle::GetFontStyle("ContentBrowser.FilterNameFont"))
								.ShadowOffset(FVector2D(1.f, 1.f))
								.Text(FText::FromString(WorldLayer.Name))
						]
				]
		];

	CheckBox->SetOnLayerCtrlClicked(FOnClicked::CreateSP(this, &SWorldLayerButton::OnCtrlClicked));
	CheckBox->SetOnLayerDoubleClicked(FOnClicked::CreateSP(this, &SWorldLayerButton::OnDoubleClicked));
}

void SWorldLayerButton::OnCheckStateChanged(ECheckBoxState NewState)
{
	if (NewState == ECheckBoxState::Checked)
	{
		WorldModel->SetSelectedLayer(WorldLayer);
	}
	else
	{
		WorldModel->SetSelectedLayers(TArray<FWorldTileLayer>());
	}
}

ECheckBoxState SWorldLayerButton::IsChecked() const
{
	return WorldModel->IsLayerSelected(WorldLayer) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

/** Handler for when the filter checkbox is double clicked */
FReply SWorldLayerButton::OnDoubleClicked()
{
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SWorldLayerButton::OnCtrlClicked()
{
	WorldModel->ToggleLayerSelection(WorldLayer);
	return FReply::Handled().ReleaseMouseCapture();
}

TSharedRef<SWidget> SWorldLayerButton::GetRightClickMenu()
{
	return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE
