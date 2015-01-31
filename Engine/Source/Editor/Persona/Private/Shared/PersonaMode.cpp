// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PersonaPrivatePCH.h"
#include "PersonaMode.h"
#include "Persona.h"
#include "SSkeletonAnimNotifies.h"
#include "IDocumentation.h"
#include "SAnimBlueprintParentPlayerList.h"
#include "SSkeletonSlotNames.h"
#include "SSkeletonSmartNameManager.h"

#define LOCTEXT_NAMESPACE "PersonaModes"

/////////////////////////////////////////////////////
// FPersonaTabs

// Tab constants
const FName FPersonaTabs::MorphTargetsID("MorphTargetsTab");
const FName FPersonaTabs::SkeletonTreeViewID("SkeletonTreeView");		//@TODO: Name
// Skeleton Pose manager
const FName FPersonaTabs::RetargetManagerID("RetargetManager");
// Anim Blueprint params
// Explorer
// Class Defaults
const FName FPersonaTabs::AnimBlueprintDefaultsEditorID("AnimBlueprintDefaultsEditor");

const FName FPersonaTabs::AnimBlueprintParentPlayerEditorID("AnimBlueprintParentPlayerEditor");
// Anim Document
const FName FPersonaTabs::ScrubberID("ScrubberTab");

// Toolbar
const FName FPersonaTabs::PreviewViewportID("Viewport");		//@TODO: Name
const FName FPersonaTabs::AssetBrowserID("SequenceBrowser");	//@TODO: Name
const FName FPersonaTabs::MirrorSetupID("MirrorSetupTab");
const FName FPersonaTabs::AnimBlueprintDebugHistoryID("AnimBlueprintDebugHistoryTab");
const FName FPersonaTabs::AnimAssetPropertiesID("AnimAssetPropertiesTab");
const FName FPersonaTabs::MeshAssetPropertiesID("MeshAssetPropertiesTab");
const FName FPersonaTabs::PreviewManagerID("AnimPreviewSetup");		//@TODO: Name
const FName FPersonaTabs::SkeletonAnimNotifiesID("SkeletonAnimNotifies");
const FName FPersonaTabs::SkeletonSlotNamesID("SkeletonSlotNames");
const FName FPersonaTabs::SkeletonSlotGroupNamesID("SkeletonSlotGroupNames");
const FName FPersonaTabs::CurveNameManagerID("CurveNameManager");

/////////////////////////////////////////////////////
// FPersonaMode

// Mode constants
const FName FPersonaModes::SkeletonDisplayMode( "SkeletonName" );
const FName FPersonaModes::MeshEditMode( "MeshName" );
const FName FPersonaModes::PhysicsEditMode( "PhysicsName" );
const FName FPersonaModes::AnimationEditMode( "AnimationName" );
const FName FPersonaModes::AnimBlueprintEditMode( "GraphName" );

/////////////////////////////////////////////////////
// FPersonaAppMode

FPersonaAppMode::FPersonaAppMode(TSharedPtr<class FPersona> InPersona, FName InModeName)
	: FApplicationMode(InModeName, FPersonaModes::GetLocalizedMode)
{
	MyPersona = InPersona;

	PersonaTabFactories.RegisterFactory(MakeShareable(new FSkeletonTreeSummoner(InPersona)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FAnimationAssetBrowserSummoner(InPersona)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FPreviewViewportSummoner(InPersona)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FMorphTargetTabSummoner(InPersona)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FSkeletonAnimNotifiesSummoner(InPersona)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FRetargetManagerTabSummoner(InPersona)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FSkeletonSlotNamesSummoner(InPersona)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FSkeletonCurveNameManagerSummoner(InPersona)));
}

void FPersonaAppMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FBlueprintEditor> BP = MyPersona.Pin();
	
	BP->RegisterToolbarTab(InTabManager.ToSharedRef());

	// Mode-specific setup
	BP->PushTabFactories(PersonaTabFactories);
}

void FPersonaAppMode::PostActivateMode()
{
	FApplicationMode::PostActivateMode();

	if(MyPersona.IsValid())
	{
		MyPersona.Pin()->ReinitMode();
	}
}
/////////////////////////////////////////////////////
// FPersonaModeSharedData

FPersonaModeSharedData::FPersonaModeSharedData()
	: OrthoZoom(1.0f)
	, bCameraLock(true)
	, bCameraFollow(false)
	, bShowReferencePose(false)
	, bShowBones(false)
	, bShowBoneNames(false)
	, bShowSockets(false)
	, bShowBound(false)
	, ViewportType(0)
	, PlaybackSpeedMode(0)
	, LocalAxesMode(0)
{}

/////////////////////////////////////////////////////
// FSkeletonTreeSummoner

#include "SSkeletonTree.h"

FSkeletonTreeSummoner::FSkeletonTreeSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
	: FWorkflowTabFactory(FPersonaTabs::SkeletonTreeViewID, InHostingApp)
{
	TabLabel = LOCTEXT("SkeletonTreeTabTitle", "Skeleton Tree");

	EnableTabPadding();
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("SkeletonTreeView", "Skeleton Tree");
	ViewMenuTooltip = LOCTEXT("SkeletonTreeView_ToolTip", "Shows the skeleton tree");
}

TSharedRef<SWidget> FSkeletonTreeSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SSkeletonTree)
		.Persona(StaticCastSharedPtr<FPersona>(HostingApp.Pin()));
}

/////////////////////////////////////////////////////
// FMorphTargetTabSummoner

#include "SMorphTargetViewer.h"

FMorphTargetTabSummoner::FMorphTargetTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
	: FWorkflowTabFactory(FPersonaTabs::MorphTargetsID, InHostingApp)
{
	TabLabel = LOCTEXT("MorphTargetTabTitle", "Morph Target Previewer");

	EnableTabPadding();
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("MorphTargetTabView", "Morph Target Previewer");
	ViewMenuTooltip = LOCTEXT("MorphTargetTabView_ToolTip", "Shows the morph target viewer");
}

TSharedRef<SWidget> FMorphTargetTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SMorphTargetViewer)
		.Persona(StaticCastSharedPtr<FPersona>(HostingApp.Pin()));
}

/////////////////////////////////////////////////////
// FAnimationAssetBrowserSummoner

#include "SAnimationSequenceBrowser.h"

FAnimationAssetBrowserSummoner::FAnimationAssetBrowserSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
	: FWorkflowTabFactory(FPersonaTabs::AssetBrowserID, InHostingApp)
{
	TabLabel = LOCTEXT("AssetBrowserTabTitle", "Asset Browser");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.TabIcon");

	EnableTabPadding();
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("AssetBrowser", "Asset Browser");
	ViewMenuTooltip = LOCTEXT("AssetBrowser_ToolTip", "Shows the animation asset browser");
}

TSharedRef<SWidget> FAnimationAssetBrowserSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SAnimationSequenceBrowser)
		.Persona(StaticCastSharedPtr<FPersona>(HostingApp.Pin()));
}

/////////////////////////////////////////////////////
// FPreviewViewportSummoner

#include "SAnimationEditorViewport.h"

FPreviewViewportSummoner::FPreviewViewportSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
	: FWorkflowTabFactory(FPersonaTabs::PreviewViewportID, InHostingApp)
{
	TabLabel = LOCTEXT("ViewportTabTitle", "Viewport");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Viewports");

	bIsSingleton = true;

	EnableTabPadding();

	ViewMenuDescription = LOCTEXT("ViewportView", "Viewport");
	ViewMenuTooltip = LOCTEXT("ViewportView_ToolTip", "Shows the viewport");
}

TSharedRef<SWidget> FPreviewViewportSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<FPersona> PersonaPtr = StaticCastSharedPtr<FPersona>(HostingApp.Pin());

	TSharedRef<SAnimationEditorViewportTabBody> NewViewport = SNew(SAnimationEditorViewportTabBody)
		.Persona(PersonaPtr)
		.Skeleton(PersonaPtr->GetSkeleton())
		.AddMetaData<FTagMetaData>(TEXT("Persona.Viewport"));

	//@TODO:MODES:check(!PersonaPtr->Viewport.IsValid());
	// mode switch data sharing
	// we saves data from previous viewport
	// and restore after switched
	bool bRestoreData = PersonaPtr->Viewport.IsValid();
	if (bRestoreData)
	{
		NewViewport.Get().SaveData(PersonaPtr->Viewport.Pin().Get());
	}

	PersonaPtr->SetViewport(NewViewport);

	if (bRestoreData)
	{
		NewViewport.Get().RestoreData();
	}

	return NewViewport;
}

/////////////////////////////////////////////////////
// FRetargetManagerTabSummoner

#include "SRetargetManager.h"

FRetargetManagerTabSummoner::FRetargetManagerTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
	: FWorkflowTabFactory(FPersonaTabs::RetargetManagerID, InHostingApp)
{
	TabLabel = LOCTEXT("RetargetManagerTabTitle", "Retarget Manager");

	EnableTabPadding();
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("RetargetManagerTabView", "Retarget Manager");
	ViewMenuTooltip = LOCTEXT("RetargetManagerTabView_ToolTip", "Manages different options for retargeting");
}

TSharedRef<SWidget> FRetargetManagerTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SRetargetManager)
		.Persona(StaticCastSharedPtr<FPersona>(HostingApp.Pin()));
}


/////////////////////////////////////////////////////
// FAnimBlueprintDefaultsEditorSummoner

#include "SKismetInspector.h"

FAnimBlueprintDefaultsEditorSummoner::FAnimBlueprintDefaultsEditorSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
	: FWorkflowTabFactory(FPersonaTabs::AnimBlueprintDefaultsEditorID, InHostingApp)
{
	TabLabel = LOCTEXT("AnimBlueprintDefaultsTabTitle", "Anim Blueprint Editor");

	bIsSingleton = true;

	CurrentMode = EAnimBlueprintEditorMode::PreviewMode;

	ViewMenuDescription = LOCTEXT("AnimBlueprintDefaultsView", "Defaults");
	ViewMenuTooltip = LOCTEXT("AnimBlueprintDefaultsView_ToolTip", "Shows the animation class defaults/preview editor view");
}

TSharedRef<SWidget> FAnimBlueprintDefaultsEditorSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<FPersona> PersonaPtr = StaticCastSharedPtr<FPersona>(HostingApp.Pin());

	return	SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding(FMargin( 0.f, 0.f, 2.f, 0.f ))
				[
					SNew(SBorder)
					.BorderImage(this, &FAnimBlueprintDefaultsEditorSummoner::GetBorderBrushByMode, EAnimBlueprintEditorMode::PreviewMode )
					.Padding(0)
					[
						SNew(SCheckBox)
						.Style(FEditorStyle::Get(), "RadioButton")
						.IsChecked( this, &FAnimBlueprintDefaultsEditorSummoner::IsChecked, EAnimBlueprintEditorMode::PreviewMode )
						.OnCheckStateChanged( this, &FAnimBlueprintDefaultsEditorSummoner::OnCheckedChanged, EAnimBlueprintEditorMode::PreviewMode )
						.ToolTip(IDocumentation::Get()->CreateToolTip(	LOCTEXT("AnimBlueprintPropertyEditorPreviewMode", "Switch to editing the preview instance properties"),
																		NULL,
																		TEXT("Shared/Editors/Persona"),
																		TEXT("AnimBlueprintPropertyEditorPreviewMode")))
						[
							SNew( STextBlock )
							.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 9 ) )
							.Text( LOCTEXT("AnimBlueprintDefaultsPreviewMode", "Edit Preview") )
						]
					]
				]
				+SHorizontalBox::Slot()
				.Padding(FMargin( 2.f, 0.f, 0.f, 0.f ))
				[
					SNew(SBorder)
					.BorderImage(this, &FAnimBlueprintDefaultsEditorSummoner::GetBorderBrushByMode, EAnimBlueprintEditorMode::DefaultsMode )
					.Padding(0)
					[
						SNew(SCheckBox)
						.Style(FEditorStyle::Get(), "RadioButton")
						.IsChecked( this, &FAnimBlueprintDefaultsEditorSummoner::IsChecked, EAnimBlueprintEditorMode::DefaultsMode )
						.OnCheckStateChanged( this, &FAnimBlueprintDefaultsEditorSummoner::OnCheckedChanged, EAnimBlueprintEditorMode::DefaultsMode )
						.ToolTip(IDocumentation::Get()->CreateToolTip(	LOCTEXT("AnimBlueprintPropertyEditorDefaultMode", "Switch to editing the class defaults"),
																		NULL,
																		TEXT("Shared/Editors/Persona"),
																		TEXT("AnimBlueprintPropertyEditorDefaultMode")))
						[
							SNew( STextBlock )
							.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 9 ) )
							.Text( LOCTEXT("AnimBlueprintDefaultsDefaultsMode", "Edit Defaults") )
						]
					]
				]
			]
			+SVerticalBox::Slot()
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SBorder)
					.Padding(0)
					.BorderImage( FEditorStyle::GetBrush("NoBorder") )
					.Visibility( this, &FAnimBlueprintDefaultsEditorSummoner::IsEditorVisible, EAnimBlueprintEditorMode::PreviewMode)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 8.f, 0.f, 0.f)
						[
							SNew(SBorder)
							.BorderImage(FEditorStyle::GetBrush("Persona.PreviewPropertiesWarning"))
							[
								SNew(STextBlock)
								.Text(LOCTEXT("AnimBlueprintEditPreviewText", "Changes to preview options are not saved in the asset."))
								.Font(FEditorStyle::GetFontStyle("PropertyWindow.NormalFont"))
								.ShadowColorAndOpacity(FLinearColor::Black.CopyWithNewOpacity(0.3f))
								.ShadowOffset(FVector2D::UnitVector)
							]
						]
						+SVerticalBox::Slot()
						[
							PersonaPtr->GetPreviewEditor()
						]
					]
				]
				+SOverlay::Slot()
				[
					SNew(SBorder)
					.Padding(0)
					.BorderImage( FEditorStyle::GetBrush("NoBorder") )
					.Visibility( this, &FAnimBlueprintDefaultsEditorSummoner::IsEditorVisible, EAnimBlueprintEditorMode::DefaultsMode)
					[
						PersonaPtr->GetDefaultEditor()
					]
				]
			];
}

FText FAnimBlueprintDefaultsEditorSummoner::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const
{
	return LOCTEXT("AnimBlueprintDefaultsEditorTooltip", "The editor lets you set the default values for all variables in your Blueprint or lets you change the values of the preview instance, depending on mode");
}

EVisibility FAnimBlueprintDefaultsEditorSummoner::IsEditorVisible(EAnimBlueprintEditorMode::Type Mode) const
{
	return CurrentMode == Mode ? EVisibility::Visible: EVisibility::Hidden;
}

ECheckBoxState FAnimBlueprintDefaultsEditorSummoner::IsChecked(EAnimBlueprintEditorMode::Type Mode) const
{
	return CurrentMode == Mode ? ECheckBoxState::Checked: ECheckBoxState::Unchecked;
}

const FSlateBrush* FAnimBlueprintDefaultsEditorSummoner::GetBorderBrushByMode(EAnimBlueprintEditorMode::Type Mode) const
{
	if(Mode == CurrentMode)
	{
		return FEditorStyle::GetBrush("ModeSelector.ToggleButton.Pressed");
	}
	else
	{
		return FEditorStyle::GetBrush("ModeSelector.ToggleButton.Normal");
	}
}

void FAnimBlueprintDefaultsEditorSummoner::OnCheckedChanged(ECheckBoxState NewType, EAnimBlueprintEditorMode::Type Mode)
{
	if(NewType == ECheckBoxState::Checked)
	{
		CurrentMode = Mode;
	}
}

//////////////////////////////////////////////////////////////////////////
// FAnimBlueprintParentPlayerEditorSummoner

FAnimBlueprintParentPlayerEditorSummoner::FAnimBlueprintParentPlayerEditorSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp) 
: FWorkflowTabFactory(FPersonaTabs::AnimBlueprintParentPlayerEditorID, InHostingApp)
{
	TabLabel = LOCTEXT("ParentPlayerOverrideEditor", "Asset Override Editor");
	bIsSingleton = true;
}

TSharedRef<SWidget> FAnimBlueprintParentPlayerEditorSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TWeakPtr<FPersona> PersonaPtr = StaticCastSharedPtr<FPersona>(HostingApp.Pin());
	return SNew(SAnimBlueprintParentPlayerList).Persona(PersonaPtr);
}

FText FAnimBlueprintParentPlayerEditorSummoner::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const
{
	return LOCTEXT("AnimSubClassTabToolTip", "Editor for overriding the animation assets referenced by the parent animation graph.");
}

#undef LOCTEXT_NAMESPACE
