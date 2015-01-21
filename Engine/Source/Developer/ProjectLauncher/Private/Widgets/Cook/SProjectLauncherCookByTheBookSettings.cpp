// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ProjectLauncherPrivatePCH.h"
#include "SHyperlink.h"
#include "SExpandableArea.h"


#define LOCTEXT_NAMESPACE "SProjectLauncherCookByTheBookSettings"


/* SProjectLauncherCookByTheBookSettings structors
 *****************************************************************************/

SProjectLauncherCookByTheBookSettings::~SProjectLauncherCookByTheBookSettings( )
{
	if (Model.IsValid())
	{
		Model->OnProfileSelected().RemoveAll(this);
	}
}


/* SProjectLauncherCookByTheBookSettings interface
 *****************************************************************************/

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SProjectLauncherCookByTheBookSettings::Construct( const FArguments& InArgs, const FProjectLauncherModelRef& InModel, bool InShowSimple )
{
	Model = InModel;

	ChildSlot
	[
		InShowSimple ? MakeSimpleWidget() : MakeComplexWidget()
	];

	Model->OnProfileSelected().AddSP(this, &SProjectLauncherCookByTheBookSettings::HandleProfileManagerProfileSelected);

	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();
	if (SelectedProfile.IsValid())
	{
		SelectedProfile->OnProjectChanged().AddSP(this, &SProjectLauncherCookByTheBookSettings::HandleProfileProjectChanged);
	}

	ShowMapsChoice = EShowMapsChoices::ShowAllMaps;

	RefreshMapList();
	RefreshCultureList();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


/* SProjectLauncherCookByTheBookSettings implementation
 *****************************************************************************/

TSharedRef<SWidget> SProjectLauncherCookByTheBookSettings::MakeComplexWidget()
{
	TSharedRef<SWidget> Widget = SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(256.0f)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SProjectLauncherFormLabel)
					.ErrorToolTipText(NSLOCTEXT("ProjectLauncherBuildValidation", "NoCookedPlatformSelectedError", "At least one Platform must be selected when cooking by the book."))
					.ErrorVisibility(this, &SProjectLauncherCookByTheBookSettings::HandleValidationErrorIconVisibility, ELauncherProfileValidationErrors::NoPlatformSelected)
					.LabelText(LOCTEXT("CookedPlatformsLabel", "Cooked Platforms:"))
				]

				+ SVerticalBox::Slot()
					.FillHeight(1.0)
					.Padding(0.0f, 2.0f, 0.0f, 0.0f)
					[
						SNew(SProjectLauncherCookedPlatforms, Model.ToSharedRef())
					]
			]
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(256.0f)
		.Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SProjectLauncherFormLabel)
					.ErrorToolTipText(NSLOCTEXT("ProjectLauncherBuildValidation", "NoCookedCulturesSelectedError", "At least one Culture must be selected when cooking by the book."))
					.ErrorVisibility(this, &SProjectLauncherCookByTheBookSettings::HandleValidationErrorIconVisibility, ELauncherProfileValidationErrors::NoCookedCulturesSelected)
					.LabelText(LOCTEXT("CookedCulturesLabel", "Cooked Cultures:"))
				]

				+ SVerticalBox::Slot()
					.FillHeight(1.0)
					.Padding(0.0f, 2.0f, 0.0f, 0.0f)
					[
						// culture menu
						SAssignNew(CultureListView, SListView<TSharedPtr<FString> >)
						.HeaderRow
						(
						SNew(SHeaderRow)
						.Visibility(EVisibility::Collapsed)

						+ SHeaderRow::Column("Culture")
						.DefaultLabel(LOCTEXT("CultureListMapNameColumnHeader", "Culture"))
						.FillWidth(1.0f)
						)
						.ItemHeight(16.0f)
						.ListItemsSource(&CultureList)
						.OnGenerateRow(this, &SProjectLauncherCookByTheBookSettings::HandleCultureListViewGenerateRow)
						.SelectionMode(ESelectionMode::None)
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 6.0f, 0.0f, 4.0f)
					[
						SNew(SSeparator)
						.Orientation(Orient_Horizontal)
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.HAlign(HAlign_Right)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SelectLabel", "Select:"))
						]

						+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(8.0f, 0.0f)
							[
								// all cultures hyper link
								SNew(SHyperlink)
								.OnNavigate(this, &SProjectLauncherCookByTheBookSettings::HandleAllCulturesHyperlinkNavigate, true)
								.Text(LOCTEXT("AllPlatformsHyperlinkLabel", "All"))
								.ToolTipText(LOCTEXT("AllPlatformsButtonTooltip", "Select all available platforms."))
								.Visibility(this, &SProjectLauncherCookByTheBookSettings::HandleAllCulturesHyperlinkVisibility)									
							]

						+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								// no cultures hyper link
								SNew(SHyperlink)
								.OnNavigate(this, &SProjectLauncherCookByTheBookSettings::HandleAllCulturesHyperlinkNavigate, false)
								.Text(LOCTEXT("NoCulturesHyperlinkLabel", "None"))
								.ToolTipText(LOCTEXT("NoCulturesHyperlinkTooltip", "Deselect all platforms."))
								.Visibility(this, &SProjectLauncherCookByTheBookSettings::HandleAllCulturesHyperlinkVisibility)									
							]
					]
			]
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(256.0f)
		.Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SProjectLauncherFormLabel)
					.LabelText(LOCTEXT("CookedMapsLabel", "Cooked Maps:"))
				]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							// all maps radio button
							SNew(SCheckBox)
							.IsChecked(this, &SProjectLauncherCookByTheBookSettings::HandleShowCheckBoxIsChecked, EShowMapsChoices::ShowAllMaps)
							.OnCheckStateChanged(this, &SProjectLauncherCookByTheBookSettings::HandleShowCheckBoxCheckStateChanged, EShowMapsChoices::ShowAllMaps)
							.Style(FEditorStyle::Get(), "RadioButton")
							[
								SNew(STextBlock)
								.Text(LOCTEXT("AllMapsCheckBoxText", "Show all"))
							]
						]

						+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(8.0f, 0.0f, 0.0f, 0.0f)
							[
								// cooked maps radio button
								SNew(SCheckBox)
								.IsChecked(this, &SProjectLauncherCookByTheBookSettings::HandleShowCheckBoxIsChecked, EShowMapsChoices::ShowCookedMaps)
								.OnCheckStateChanged(this, &SProjectLauncherCookByTheBookSettings::HandleShowCheckBoxCheckStateChanged, EShowMapsChoices::ShowCookedMaps)
								.Style(FEditorStyle::Get(), "RadioButton")
								[
									SNew(STextBlock)
									.Text(LOCTEXT("CookedMapsCheckBoxText", "Show cooked"))
								]
							]
					]

				+ SVerticalBox::Slot()
					.FillHeight(1.0)
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						// map list
						SAssignNew(MapListView, SListView<TSharedPtr<FString> >)
						.HeaderRow
						(
						SNew(SHeaderRow)
						.Visibility(EVisibility::Collapsed)

						+ SHeaderRow::Column("MapName")
						.DefaultLabel(LOCTEXT("MapListMapNameColumnHeader", "Map"))
						.FillWidth(1.0f)
						)
						.ItemHeight(16.0f)
						.ListItemsSource(&MapList)
						.OnGenerateRow(this, &SProjectLauncherCookByTheBookSettings::HandleMapListViewGenerateRow)
						.SelectionMode(ESelectionMode::None)
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						.Visibility(this, &SProjectLauncherCookByTheBookSettings::HandleNoMapSelectedBoxVisibility)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush(TEXT("Icons.Warning")))
						]

						+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(4.0f, 0.0f)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(this, &SProjectLauncherCookByTheBookSettings::HandleNoMapsTextBlockText)
							]
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 6.0f, 0.0f, 4.0f)
					[
						SNew(SSeparator)
						.Orientation(Orient_Horizontal)
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.HAlign(HAlign_Right)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SelectLabel", "Select:"))
							.Visibility(this, &SProjectLauncherCookByTheBookSettings::HandleMapSelectionHyperlinkVisibility)
						]

						+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(8.0f, 0.0f)
							[
								// all maps hyper link
								SNew(SHyperlink)
								.OnNavigate(this, &SProjectLauncherCookByTheBookSettings::HandleAllMapsHyperlinkNavigate, true)
								.Text(LOCTEXT("AllMapsHyperlinkLabel", "All"))
								.ToolTipText(LOCTEXT("AllMapsHyperlinkTooltip", "Select all available maps."))
								.Visibility(this, &SProjectLauncherCookByTheBookSettings::HandleMapSelectionHyperlinkVisibility)									
							]

						+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								// no maps hyper link
								SNew(SHyperlink)
								.OnNavigate(this, &SProjectLauncherCookByTheBookSettings::HandleAllMapsHyperlinkNavigate, false)
								.Text(LOCTEXT("NoMapsHyperlinkLabel", "None"))
								.ToolTipText(LOCTEXT("NoMapsHyperlinkTooltip", "Deselect all maps."))
								.Visibility(this, &SProjectLauncherCookByTheBookSettings::HandleMapSelectionHyperlinkVisibility)
							]
					]
			]
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			SNew(SExpandableArea)
			.AreaTitle(LOCTEXT("AdvancedAreaTitle", "Advanced Settings"))
			.InitiallyCollapsed(true)
			.Padding(8.0f)
			.BodyContent()
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					// incremental cook check box
					SNew(SCheckBox)
					.IsChecked(this, &SProjectLauncherCookByTheBookSettings::HandleIncrementalCheckBoxIsChecked)
					.OnCheckStateChanged(this, &SProjectLauncherCookByTheBookSettings::HandleIncrementalCheckBoxCheckStateChanged)
					.Padding(FMargin(4.0f, 0.0f))
					.ToolTipText(LOCTEXT("IncrementalCheckBoxTooltip", "If checked, only modified content will be cooked, resulting in much faster cooking times. It is recommended to enable this option whenever possible."))
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("IncrementalCheckBoxText", "Only cook modified content"))
					]
				]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						// incremental cook check box
						SNew(SCheckBox)
						.IsChecked(this, &SProjectLauncherCookByTheBookSettings::HandleUnversionedCheckBoxIsChecked)
						.OnCheckStateChanged(this, &SProjectLauncherCookByTheBookSettings::HandleUnversionedCheckBoxCheckStateChanged)
						.Padding(FMargin(4.0f, 0.0f))
						.ToolTipText(LOCTEXT("UnversionedCheckBoxTooltip", "If checked, the version is assumed to be current at load. This is potentially dangerous, but results in smaller patch sizes."))
						.Content()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("UnversionedCheckBoxText", "Save packages without versions"))
						]
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						// unreal pak check box
						SNew(SCheckBox)
						.IsChecked(this, &SProjectLauncherCookByTheBookSettings::HandleUnrealPakCheckBoxIsChecked)
						.OnCheckStateChanged(this, &SProjectLauncherCookByTheBookSettings::HandleUnrealPakCheckBoxCheckStateChanged)
						.Padding(FMargin(4.0f, 0.0f))
						.ToolTipText(LOCTEXT("UnrealPakCheckBoxTooltip", "If checked, the content will be deployed as a single UnrealPak file instead of many separate files."))
						.Content()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("UnrealPakCheckBoxText", "Store all content in a single file (UnrealPak)"))
						]
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 12.0f, 0.0f, 0.0f)
					[
						SNew(SProjectLauncherFormLabel)
						.LabelText(LOCTEXT("CookConfigurationSelectorLabel", "Cooker build configuration:"))							
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						// cooker build configuration selector
						SNew(SProjectLauncherBuildConfigurationSelector)
						.OnConfigurationSelected(this, &SProjectLauncherCookByTheBookSettings::HandleCookConfigurationSelectorConfigurationSelected)
						.Text(this, &SProjectLauncherCookByTheBookSettings::HandleCookConfigurationSelectorText)
						.ToolTipText(LOCTEXT("CookConfigurationToolTipText", "Sets the build configuration to use for the cooker commandlet."))
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 8.0f, 0.0f, 0.0f)
					[
						SNew(SProjectLauncherFormLabel)
						.LabelText(LOCTEXT("CookerOptionsTextBoxLabel", "Additional Cooker Options:"))
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						// cooker command line options
						SNew(SEditableTextBox)
						.ToolTipText(LOCTEXT("CookerOptionsTextBoxTooltip", "Additional cooker command line parameters can be specified here."))
						.Text(this, &SProjectLauncherCookByTheBookSettings::HandleCookOptionsTextBlockText)
						.OnTextCommitted(this, &SProjectLauncherCookByTheBookSettings::HandleCookerOptionsCommitted)
					]
			]
		];

	return Widget;
}

TSharedRef<SWidget> SProjectLauncherCookByTheBookSettings::MakeSimpleWidget()
{
	TSharedRef<SWidget> Widget = SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(256.0f)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SProjectLauncherFormLabel)
					.ErrorToolTipText(NSLOCTEXT("ProjectLauncherBuildValidation", "NoCookedPlatformSelectedError", "At least one Platform must be selected when cooking by the book."))
					.ErrorVisibility(this, &SProjectLauncherCookByTheBookSettings::HandleValidationErrorIconVisibility, ELauncherProfileValidationErrors::NoPlatformSelected)
					.LabelText(LOCTEXT("CookedPlatformsLabel", "Cooked Platforms:"))
				]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 2.0f, 0.0f, 0.0f)
					[
						SNew(SProjectLauncherCookedPlatforms, Model.ToSharedRef())
					]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(256.0f)
		.Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SProjectLauncherFormLabel)
					.LabelText(LOCTEXT("CookedMapsLabel", "Cooked Maps:"))
				]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							// all maps radio button
							SNew(SCheckBox)
							.IsChecked(this, &SProjectLauncherCookByTheBookSettings::HandleShowCheckBoxIsChecked, EShowMapsChoices::ShowAllMaps)
							.OnCheckStateChanged(this, &SProjectLauncherCookByTheBookSettings::HandleShowCheckBoxCheckStateChanged, EShowMapsChoices::ShowAllMaps)
							.Style(FEditorStyle::Get(), "RadioButton")
							[
								SNew(STextBlock)
								.Text(LOCTEXT("AllMapsCheckBoxText", "Show all"))
							]
						]

						+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(8.0f, 0.0f, 0.0f, 0.0f)
							[
								// cooked maps radio button
								SNew(SCheckBox)
								.IsChecked(this, &SProjectLauncherCookByTheBookSettings::HandleShowCheckBoxIsChecked, EShowMapsChoices::ShowCookedMaps)
								.OnCheckStateChanged(this, &SProjectLauncherCookByTheBookSettings::HandleShowCheckBoxCheckStateChanged, EShowMapsChoices::ShowCookedMaps)
								.Style(FEditorStyle::Get(), "RadioButton")
								[
									SNew(STextBlock)
									.Text(LOCTEXT("CookedMapsCheckBoxText", "Show cooked"))
								]
							]
					]

				+ SVerticalBox::Slot()
					.FillHeight(1.0)
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						// map list
						SAssignNew(MapListView, SListView<TSharedPtr<FString> >)
						.HeaderRow
						(
						SNew(SHeaderRow)
						.Visibility(EVisibility::Collapsed)

						+ SHeaderRow::Column("MapName")
						.DefaultLabel(LOCTEXT("MapListMapNameColumnHeader", "Map"))
						.FillWidth(1.0f)
						)
						.ItemHeight(16.0f)
						.ListItemsSource(&MapList)
						.OnGenerateRow(this, &SProjectLauncherCookByTheBookSettings::HandleMapListViewGenerateRow)
						.SelectionMode(ESelectionMode::None)
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						.Visibility(this, &SProjectLauncherCookByTheBookSettings::HandleNoMapSelectedBoxVisibility)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush(TEXT("Icons.Warning")))
						]

						+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(4.0f, 0.0f)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(this, &SProjectLauncherCookByTheBookSettings::HandleNoMapsTextBlockText)
							]
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 6.0f, 0.0f, 4.0f)
					[
						SNew(SSeparator)
						.Orientation(Orient_Horizontal)
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.HAlign(HAlign_Right)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SelectLabel", "Select:"))
							.Visibility(this, &SProjectLauncherCookByTheBookSettings::HandleMapSelectionHyperlinkVisibility)
						]

						+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(8.0f, 0.0f)
							[
								// all maps hyper link
								SNew(SHyperlink)
								.OnNavigate(this, &SProjectLauncherCookByTheBookSettings::HandleAllMapsHyperlinkNavigate, true)
								.Text(LOCTEXT("AllMapsHyperlinkLabel", "All"))
								.ToolTipText(LOCTEXT("AllMapsHyperlinkTooltip", "Select all available maps."))
								.Visibility(this, &SProjectLauncherCookByTheBookSettings::HandleMapSelectionHyperlinkVisibility)									
							]

						+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								// no maps hyper link
								SNew(SHyperlink)
								.OnNavigate(this, &SProjectLauncherCookByTheBookSettings::HandleAllMapsHyperlinkNavigate, false)
								.Text(LOCTEXT("NoMapsHyperlinkLabel", "None"))
								.ToolTipText(LOCTEXT("NoMapsHyperlinkTooltip", "Deselect all maps."))
								.Visibility(this, &SProjectLauncherCookByTheBookSettings::HandleMapSelectionHyperlinkVisibility)
							]
					]
			]
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			SNew(SExpandableArea)
			.AreaTitle(LOCTEXT("AdvancedAreaTitle", "Advanced Settings"))
			.InitiallyCollapsed(true)
			.Padding(8.0f)
			.BodyContent()
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					// incremental cook check box
					SNew(SCheckBox)
					.IsChecked(this, &SProjectLauncherCookByTheBookSettings::HandleIncrementalCheckBoxIsChecked)
					.OnCheckStateChanged(this, &SProjectLauncherCookByTheBookSettings::HandleIncrementalCheckBoxCheckStateChanged)
					.Padding(FMargin(4.0f, 0.0f))
					.ToolTipText(LOCTEXT("IncrementalCheckBoxTooltip", "If checked, only modified content will be cooked, resulting in much faster cooking times. It is recommended to enable this option whenever possible."))
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("IncrementalCheckBoxText", "Only cook modified content"))
					]
				]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						// incremental cook check box
						SNew(SCheckBox)
						.IsChecked(this, &SProjectLauncherCookByTheBookSettings::HandleUnversionedCheckBoxIsChecked)
						.OnCheckStateChanged(this, &SProjectLauncherCookByTheBookSettings::HandleUnversionedCheckBoxCheckStateChanged)
						.Padding(FMargin(4.0f, 0.0f))
						.ToolTipText(LOCTEXT("UnversionedCheckBoxTooltip", "If checked, the version is assumed to be current at load. This is potentially dangerous, but results in smaller patch sizes."))
						.Content()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("UnversionedCheckBoxText", "Save packages without versions"))
						]
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						// unreal pak check box
						SNew(SCheckBox)
						.IsChecked(this, &SProjectLauncherCookByTheBookSettings::HandleUnrealPakCheckBoxIsChecked)
						.OnCheckStateChanged(this, &SProjectLauncherCookByTheBookSettings::HandleUnrealPakCheckBoxCheckStateChanged)
						.Padding(FMargin(4.0f, 0.0f))
						.ToolTipText(LOCTEXT("UnrealPakCheckBoxTooltip", "If checked, the content will be deployed as a single UnrealPak file instead of many separate files."))
						.Content()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("UnrealPakCheckBoxText", "Store all content in a single file (UnrealPak)"))
						]
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 12.0f, 0.0f, 0.0f)
					[
						SNew(SProjectLauncherFormLabel)
						.LabelText(LOCTEXT("CookConfigurationSelectorLabel", "Cooker build configuration:"))							
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						// cooker build configuration selector
						SNew(SProjectLauncherBuildConfigurationSelector)
						.OnConfigurationSelected(this, &SProjectLauncherCookByTheBookSettings::HandleCookConfigurationSelectorConfigurationSelected)
						.Text(this, &SProjectLauncherCookByTheBookSettings::HandleCookConfigurationSelectorText)
						.ToolTipText(LOCTEXT("CookConfigurationToolTipText", "Sets the build configuration to use for the cooker commandlet."))
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 8.0f, 0.0f, 0.0f)
					[
						SNew(SProjectLauncherFormLabel)
						.LabelText(LOCTEXT("CookerOptionsTextBoxLabel", "Additional Cooker Options:"))
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						// cooker command line options
						SNew(SEditableTextBox)
						.ToolTipText(LOCTEXT("CookerOptionsTextBoxTooltip", "Additional cooker command line parameters can be specified here."))
						.Text(this, &SProjectLauncherCookByTheBookSettings::HandleCookOptionsTextBlockText)
						.OnTextCommitted(this, &SProjectLauncherCookByTheBookSettings::HandleCookerOptionsCommitted)
					]
			]
		];

	return Widget;
}

void SProjectLauncherCookByTheBookSettings::RefreshCultureList()
{
	CultureList.Reset();

	TArray<FString> CultureNames;
	FInternationalization::Get().GetCultureNames(CultureNames);

	if (CultureNames.Num() > 0)
	{
		for (int32 Index = 0; Index < CultureNames.Num(); ++Index)
		{
			FString CultureName = CultureNames[Index];
			CultureList.Add(MakeShareable(new FString(CultureName)));
		}
	}

	if (CultureListView.IsValid())
	{
		CultureListView->RequestListRefresh();
	}
}

void SProjectLauncherCookByTheBookSettings::RefreshMapList( )
{
	MapList.Reset();

	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		TArray<FString> AvailableMaps = FGameProjectHelper::GetAvailableMaps(SelectedProfile->GetProjectBasePath(), SelectedProfile->SupportsEngineMaps(), true);

		for (int32 AvailableMapIndex = 0; AvailableMapIndex < AvailableMaps.Num(); ++AvailableMapIndex)
		{
			FString& Map = AvailableMaps[AvailableMapIndex];

			if ((ShowMapsChoice == EShowMapsChoices::ShowAllMaps) || SelectedProfile->GetCookedMaps().Contains(Map))
			{
				MapList.Add(MakeShareable(new FString(Map)));
			}
		}
	}

	MapListView->RequestListRefresh();
}


/* SProjectLauncherCookByTheBookSettings callbacks
 *****************************************************************************/

void SProjectLauncherCookByTheBookSettings::HandleAllCulturesHyperlinkNavigate( bool AllPlatforms )
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		if (AllPlatforms)
		{
			TArray<FString> CultureNames;
			FInternationalization::Get().GetCultureNames(CultureNames);

			for (int32 ExtensionIndex = 0; ExtensionIndex < CultureNames.Num(); ++ExtensionIndex)
			{
				SelectedProfile->AddCookedCulture(CultureNames[ExtensionIndex]);
			}
		}
		else
		{
			SelectedProfile->ClearCookedCultures();
		}
	}
}


EVisibility SProjectLauncherCookByTheBookSettings::HandleAllCulturesHyperlinkVisibility( ) const
{
	TArray<FString> CultureNames;
	FInternationalization::Get().GetCultureNames(CultureNames);

	if (CultureNames.Num() > 1)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}


void SProjectLauncherCookByTheBookSettings::HandleAllMapsHyperlinkNavigate( bool AllPlatforms )
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		if (AllPlatforms)
		{
			TArray<FString> AvailableMaps = FGameProjectHelper::GetAvailableMaps(SelectedProfile->GetProjectBasePath(), SelectedProfile->SupportsEngineMaps(), false);

			for (int32 AvailableMapIndex = 0; AvailableMapIndex < AvailableMaps.Num(); ++AvailableMapIndex)
			{
				SelectedProfile->AddCookedMap(AvailableMaps[AvailableMapIndex]);
			}
		}
		else
		{
			SelectedProfile->ClearCookedMaps();
		}
	}
}


EVisibility SProjectLauncherCookByTheBookSettings::HandleMapSelectionHyperlinkVisibility( ) const
{
	if (MapList.Num() > 1)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}


void SProjectLauncherCookByTheBookSettings::HandleCookConfigurationSelectorConfigurationSelected( EBuildConfigurations::Type Configuration )
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		SelectedProfile->SetCookConfiguration(Configuration);
	}
}


FText SProjectLauncherCookByTheBookSettings::HandleCookConfigurationSelectorText( ) const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		return FText::FromString(EBuildConfigurations::ToString(SelectedProfile->GetCookConfiguration()));
	}

	return FText::GetEmpty();
}


void SProjectLauncherCookByTheBookSettings::HandleIncrementalCheckBoxCheckStateChanged( ECheckBoxState NewState )
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		SelectedProfile->SetIncrementalCooking(NewState == ECheckBoxState::Checked);
	}
}


ECheckBoxState SProjectLauncherCookByTheBookSettings::HandleIncrementalCheckBoxIsChecked( ) const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		if (SelectedProfile->IsCookingIncrementally())
		{
			return ECheckBoxState::Checked;
		}
	}

	return ECheckBoxState::Unchecked;
}


TSharedRef<ITableRow> SProjectLauncherCookByTheBookSettings::HandleMapListViewGenerateRow( TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(SProjectLauncherMapListRow, Model.ToSharedRef())
		.MapName(InItem)
		.OwnerTableView(OwnerTable);
}


TSharedRef<ITableRow> SProjectLauncherCookByTheBookSettings::HandleCultureListViewGenerateRow( TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(SProjectLauncherCultureListRow, Model.ToSharedRef())
		.CultureName(InItem)
		.OwnerTableView(OwnerTable);
}


EVisibility SProjectLauncherCookByTheBookSettings::HandleNoMapSelectedBoxVisibility( ) const
{
	if (MapList.Num() == 0)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}


FText SProjectLauncherCookByTheBookSettings::HandleNoMapsTextBlockText( ) const
{
	if (MapList.Num() == 0)
	{
		if (ShowMapsChoice == EShowMapsChoices::ShowAllMaps)
		{
			return LOCTEXT("NoMapsFoundText", "No available maps were found.");
		}
		else if (ShowMapsChoice == EShowMapsChoices::ShowCookedMaps)
		{
			return LOCTEXT("NoMapsSelectedText", "No map selected. Only startup packages will be cooked!");
		}
	}

	return FText();
}


void SProjectLauncherCookByTheBookSettings::HandleProfileManagerProfileSelected( const ILauncherProfilePtr& SelectedProfile, const ILauncherProfilePtr& PreviousProfile )
{
	if (PreviousProfile.IsValid())
	{
		PreviousProfile->OnProjectChanged().RemoveAll(this);
	}
	if (SelectedProfile.IsValid())
	{
		SelectedProfile->OnProjectChanged().AddSP(this, &SProjectLauncherCookByTheBookSettings::HandleProfileProjectChanged);
	}
	RefreshMapList();
	RefreshCultureList();
}

void SProjectLauncherCookByTheBookSettings::HandleProfileProjectChanged()
{
	RefreshMapList();
	RefreshCultureList();
}

ECheckBoxState SProjectLauncherCookByTheBookSettings::HandleShowCheckBoxIsChecked( EShowMapsChoices::Type Choice ) const
{
	if (ShowMapsChoice == Choice)
	{
		return ECheckBoxState::Checked;
	}

	return ECheckBoxState::Unchecked;
}


void SProjectLauncherCookByTheBookSettings::HandleShowCheckBoxCheckStateChanged( ECheckBoxState NewState, EShowMapsChoices::Type Choice )
{
	if (NewState == ECheckBoxState::Checked)
	{
		ShowMapsChoice = Choice;
		RefreshMapList();
	}
}


void SProjectLauncherCookByTheBookSettings::HandleUnversionedCheckBoxCheckStateChanged( ECheckBoxState NewState )
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		SelectedProfile->SetUnversionedCooking((NewState == ECheckBoxState::Checked));
	}
}


ECheckBoxState SProjectLauncherCookByTheBookSettings::HandleUnversionedCheckBoxIsChecked( ) const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		if (SelectedProfile->IsCookingUnversioned())
		{
			return ECheckBoxState::Checked;
		}
	}

	return ECheckBoxState::Unchecked;
}


EVisibility SProjectLauncherCookByTheBookSettings::HandleValidationErrorIconVisibility( ELauncherProfileValidationErrors::Type Error ) const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		if (SelectedProfile->HasValidationError(Error))
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Hidden;
}

FText SProjectLauncherCookByTheBookSettings::HandleCookOptionsTextBlockText() const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	FText result;

	if (SelectedProfile.IsValid())
	{
		result = FText::FromString(SelectedProfile->GetCookOptions());
	}

	return result;
}

void SProjectLauncherCookByTheBookSettings::HandleCookerOptionsCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		FString useOptions = NewText.ToString();
		switch (CommitType)
		{
		case ETextCommit::Default:
		case ETextCommit::OnCleared:
			useOptions = TEXT("");
			break;
		default:
			break;
		}
		SelectedProfile->SetCookOptions(useOptions);
	}
}

void SProjectLauncherCookByTheBookSettings::HandleUnrealPakCheckBoxCheckStateChanged( ECheckBoxState NewState )
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		SelectedProfile->SetDeployWithUnrealPak(NewState == ECheckBoxState::Checked);
	}
}


ECheckBoxState SProjectLauncherCookByTheBookSettings::HandleUnrealPakCheckBoxIsChecked( ) const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		if (SelectedProfile->IsPackingWithUnrealPak())
		{
			return ECheckBoxState::Checked;
		}
	}

	return ECheckBoxState::Unchecked;
}

#undef LOCTEXT_NAMESPACE
