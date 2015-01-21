// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "KismetWidgetsPrivatePCH.h"
#include "CreateBlueprintFromActorDialog.h"
#include "SCreateAssetFromActor.h"
#include "UnrealEd.h"
#include "Editor/UnrealEd/Public/Kismet2/KismetEditorUtilities.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "Engine/Selection.h"

#define LOCTEXT_NAMESPACE "CreateBlueprintFromActorDialog"

void FCreateBlueprintFromActorDialog::OpenDialog(bool bInHarvest)
{
	TSharedPtr<SWindow> PickBlueprintPathWidget;
	SAssignNew(PickBlueprintPathWidget, SWindow)
		.Title(LOCTEXT("SelectPath", "Select Path"))
		.ToolTipText(LOCTEXT("SelectPathTooltip", "Select the path where the Blueprint will be created at"))
		.ClientSize(FVector2D(400, 400));

	TSharedPtr<SCreateAssetFromActor> CreateBlueprintFromActorDialog;
	PickBlueprintPathWidget->SetContent
		(
		SAssignNew(CreateBlueprintFromActorDialog, SCreateAssetFromActor, PickBlueprintPathWidget)
		.AssetFilenameSuffix(TEXT("Blueprint"))
		.HeadingText(LOCTEXT("CreateBlueprintFromActor_Heading", "Blueprint Name"))
		.CreateButtonText(LOCTEXT("CreateBlueprintFromActor_ButtonLabel", "Create Blueprint"))
		.OnCreateAssetAction(FOnPathChosen::CreateStatic(FCreateBlueprintFromActorDialog::OnCreateBlueprint, bInHarvest))
		);

	TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
	if (RootWindow.IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(PickBlueprintPathWidget.ToSharedRef(), RootWindow.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(PickBlueprintPathWidget.ToSharedRef());
	}
}

void FCreateBlueprintFromActorDialog::OnCreateBlueprint(const FString& InAssetPath, bool bInHarvest)
{
	UBlueprint* Blueprint = NULL;

	if(bInHarvest)
	{
		TArray<AActor*> Actors;

		USelection* SelectedActors = GEditor->GetSelectedActors();
		for(FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			// We only care about actors that are referenced in the world for literals, and also in the same level as this blueprint
			AActor* Actor = Cast<AActor>(*Iter);
			if(Actor)
			{
				Actors.Add(Actor);
			}
		}
		Blueprint = FKismetEditorUtilities::HarvestBlueprintFromActors(InAssetPath, Actors, true);
	}
	else
	{
		TArray< UObject* > SelectedActors;
		GEditor->GetSelectedActors()->GetSelectedObjects(AActor::StaticClass(), SelectedActors);
		check(SelectedActors.Num());
		Blueprint = FKismetEditorUtilities::CreateBlueprintFromActor(InAssetPath, (AActor*)SelectedActors[0], true);
	}

	if(Blueprint)
	{
		// Rename new instance based on the original actor label rather than the asset name
		USelection* SelectedActors = GEditor->GetSelectedActors();
		if( SelectedActors && SelectedActors->Num() == 1 )
		{
			AActor* Actor = Cast<AActor>(SelectedActors->GetSelectedObject(0));
			if(Actor)
			{
				GEditor->SetActorLabelUnique(Actor, FPackageName::GetShortName(InAssetPath));
			}
		}

		TArray<UObject*> Objects;
		Objects.Add(Blueprint);
		GEditor->SyncBrowserToObjects( Objects );
	}
	else
	{
		FNotificationInfo Info( LOCTEXT("CreateBlueprintFromActorFailed", "Unable to create a blueprint from actor.") );
		Info.ExpireDuration = 3.0f;
		Info.bUseLargeFont = false;
		TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
		if ( Notification.IsValid() )
		{
			Notification->SetCompletionState( SNotificationItem::CS_Fail );
		}
	}
}


#undef LOCTEXT_NAMESPACE
