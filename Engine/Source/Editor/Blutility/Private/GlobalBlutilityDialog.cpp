// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "BlutilityPrivatePCH.h"
#include "BlutilityClasses.h"
#include "AssetTypeActions_EditorUtilityBlueprint.h"
#include "Toolkits/AssetEditorManager.h"
#include "BlueprintEditorModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "GlobalBlutilityDialog.h"

#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"
#include "AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "GlobalBlutilityDialog"

const FName NAME_DetailsPanel(TEXT("GlobalBlutilityDialog_DetailsPanel"));
const FName NAME_GlobalBlutilityDialogAppIdentifier = FName(TEXT("GlobalBlutilityDialogApp"));

//////////////////////////////////////////////////////////////////////////
// FGlobalBlutilityDialog

void FGlobalBlutilityDialog::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	TabManager->RegisterTabSpawner( NAME_DetailsPanel, FOnSpawnTab::CreateRaw( this, &FGlobalBlutilityDialog::SpawnTab_DetailsPanel ) );
}

void FGlobalBlutilityDialog::UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	TabManager->UnregisterTabSpawner( NAME_DetailsPanel );
}

TSharedRef<SDockTab> FGlobalBlutilityDialog::SpawnTab_DetailsPanel( const FSpawnTabArgs& SpawnTabArgs )
{
	const TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		//@TODO: Add an icon .Icon( FEditorStyle::GetBrush("SoundClassEditor.Tabs.Properties") )
		.Label( LOCTEXT("GlobalBlutilityDetailsTitle", "Blutility Details") )
		[
			DetailsView.ToSharedRef()
		];

	// Make sure the blutility instance is selected
	TArray<UObject*> SelectedObjects;
	SelectedObjects.Add(BlutilityInstance.Get());
	UpdatePropertyWindow(SelectedObjects);

	return SpawnedTab;
}

void FGlobalBlutilityDialog::InitBlutilityDialog(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit)
{
	// Create an instance of the blutility
	check(ObjectToEdit != NULL);
	UEditorUtilityBlueprint* BlutilityBP = CastChecked<UEditorUtilityBlueprint>(ObjectToEdit);
	check(BlutilityBP->GeneratedClass->IsChildOf(UGlobalEditorUtilityBase::StaticClass()));

	UGlobalEditorUtilityBase* Instance = NewObject<UGlobalEditorUtilityBase>(GetTransientPackage(), BlutilityBP->GeneratedClass);
	Instance->AddToRoot();
	BlutilityInstance = Instance;

	//
	CreateInternalWidgets();

	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_GlobalBlutility_Layout" ) 
	->AddArea
	(
		FTabManager::NewPrimaryArea()
		->Split
		(
			FTabManager::NewStack()
			->AddTab( NAME_DetailsPanel, ETabState::OpenedTab)
		)
	);

	const bool bCreateDefaultStandaloneMenu = false;
	const bool bCreateDefaultToolbar = false;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, NAME_GlobalBlutilityDialogAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectToEdit );

	// @todo toolkit world centric editing
	/*if (IsWorldCentricAssetEditor())
	{
		SpawnToolkitTab(NAME_DetailsPanel, FString(), EToolkitTabSpot::Details);
	}*/
}

FGlobalBlutilityDialog::~FGlobalBlutilityDialog()
{
	if (UGlobalEditorUtilityBase* Instance = BlutilityInstance.Get())
	{
		Instance->RemoveFromRoot();
	}

	DetailsView.Reset();
}

void FGlobalBlutilityDialog::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (UGlobalEditorUtilityBase* Instance = BlutilityInstance.Get())
	{
		Collector.AddReferencedObject(Instance);
	}
}

FName FGlobalBlutilityDialog::GetToolkitFName() const
{
	return FName("Blutility");
}

FText FGlobalBlutilityDialog::GetBaseToolkitName() const
{
	return LOCTEXT( "AppLabel", "Blutility" );
}

FString FGlobalBlutilityDialog::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Blutility ").ToString();
}

FLinearColor FGlobalBlutilityDialog::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.3f, 0.2f, 0.5f, 0.5f );
}

void FGlobalBlutilityDialog::CreateInternalWidgets()
{
	// Helper for hiding private blueprint variables
	struct Local
	{
		static bool IsPropertyVisible(const UProperty* InProperty)
		{
			// For details views in the level editor all properties are the instanced versions
			if ((InProperty != NULL) && InProperty->HasAllPropertyFlags(CPF_DisableEditOnInstance))
			{
				return false;
			}
			else
			{
				return true;
			}
		}
	};

	// Create a details view
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	const FDetailsViewArgs DetailsViewArgs(/*bUpdateFromSelection=*/ false, /*bLockable=*/ false, /*bAllowSearch=*/ false, /*bObjectsUseNameArea=*/ false, /*bHideSelectionTip=*/ true);
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	DetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateStatic(&Local::IsPropertyVisible));
}

void FGlobalBlutilityDialog::UpdatePropertyWindow(const TArray<UObject*>& SelectedObjects)
{
	DetailsView->SetObjects(SelectedObjects);
}

#undef LOCTEXT_NAMESPACE