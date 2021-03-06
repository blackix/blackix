// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "PropertyEditorPrivatePCH.h"
#include "SPropertySceneOutliner.h"
#include "PropertyEditor.h"
#include "PropertyNode.h"
#include "ObjectPropertyNode.h"
#include "Editor/SceneOutliner/Public/SceneOutlinerModule.h"

#define LOCTEXT_NAMESPACE "PropertySceneOutliner"

void SPropertySceneOutliner::Construct( const FArguments& InArgs )
{
	OnActorSelected = InArgs._OnActorSelected;
	OnGetActorFilters = InArgs._OnGetActorFilters;

	ChildSlot
	[
		SNew( SVerticalBox )
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		[
			SAssignNew( SceneOutlinerAnchor, SMenuAnchor )
			.Placement( MenuPlacement_AboveAnchor )
			.OnGetMenuContent( this, &SPropertySceneOutliner::OnGenerateSceneOutliner )
		]
		+ SVerticalBox::Slot()
		.FillHeight(1)
		[
			SNew( SButton )
			.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
			.OnClicked( this, &SPropertySceneOutliner::OnClicked )
			.ToolTipText(LOCTEXT("PickButtonLabel", "Pick Actor").ToString())
			.ContentPadding(0)
			.ForegroundColor( FSlateColor::UseForeground() )
			.IsFocusable(false)
			[ 
				SNew( SImage )
				.Image( FEditorStyle::GetBrush("PropertyWindow.Button_PickActor") )
				.ColorAndOpacity( FSlateColor::UseForeground() )
			]
		]
	];
}
 
FReply SPropertySceneOutliner::OnClicked()
{	
	SceneOutlinerAnchor->SetIsOpen( true );
	return FReply::Handled();
}

TSharedRef<SWidget> SPropertySceneOutliner::OnGenerateSceneOutliner()
{
	TSharedPtr<TFilterCollection<const AActor* const> > ActorFilters = NULL;
	OnGetActorFilters.ExecuteIfBound( ActorFilters );

	FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::Get().LoadModuleChecked<FSceneOutlinerModule>(TEXT("SceneOutliner"));

	FSceneOutlinerInitializationOptions InitOptions;
	InitOptions.Mode = ESceneOutlinerMode::ActorPicker;
	InitOptions.ActorFilters = ActorFilters;

	TSharedRef<SWidget> MenuContent = 
		SNew(SBox)
		.HeightOverride(300)
		.WidthOverride(300)
		[
			SNew( SBorder )
			.BorderImage( FEditorStyle::GetBrush("Menu.Background") )
			[
				SceneOutlinerModule.CreateSceneOutliner(InitOptions, FOnContextMenuOpening(), FOnActorPicked::CreateSP(this, &SPropertySceneOutliner::OnActorSelectedFromOutliner))
			]
		];

	return MenuContent;
}

void SPropertySceneOutliner::OnActorSelectedFromOutliner( AActor* InActor )
{
	// Close the scene outliner
	SceneOutlinerAnchor->SetIsOpen( false );

	OnActorSelected.ExecuteIfBound( InActor );
}

#undef LOCTEXT_NAMESPACE