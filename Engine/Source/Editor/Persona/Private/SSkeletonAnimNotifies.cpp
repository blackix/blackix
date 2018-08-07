// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#include "SSkeletonAnimNotifies.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AssetData.h"
#include "Animation/AnimSequenceBase.h"
#include "EditorStyleSet.h"
#include "Animation/EditorSkeletonNotifyObj.h"

#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "ScopedTransaction.h"
#include "IEditableSkeleton.h"
#include "TabSpawners.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "SkeletonAnimNotifies"

//////////////////////////////////////////////////////////////////////////
// SMorphTargetListRow

typedef TSharedPtr< FDisplayedAnimNotifyInfo > FDisplayedAnimNotifyInfoPtr;

/////////////////////////////////////////////////////
// FSkeletonAnimNotifiesSummoner

FSkeletonAnimNotifiesSummoner::FSkeletonAnimNotifiesSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, FOnObjectsSelected InOnObjectsSelected)
	: FWorkflowTabFactory(FPersonaTabs::SkeletonAnimNotifiesID, InHostingApp)
	, EditableSkeleton(InEditableSkeleton)
	, OnObjectsSelected(InOnObjectsSelected)
{
	TabLabel = LOCTEXT("SkeletonAnimNotifiesTabTitle", "Animation Notifies");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.Tabs.AnimationNotifies");

	EnableTabPadding();
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("SkeletonAnimNotifiesMenu", "Animation Notifies");
	ViewMenuTooltip = LOCTEXT("SkeletonAnimNotifies_ToolTip", "Shows the skeletons notifies list");
}

TSharedRef<SWidget> FSkeletonAnimNotifiesSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SSkeletonAnimNotifies, EditableSkeleton.Pin().ToSharedRef())
		.OnObjectsSelected(OnObjectsSelected);
}

/////////////////////////////////////////////////////
// SSkeletonAnimNotifies

void SSkeletonAnimNotifies::Construct(const FArguments& InArgs, const TSharedRef<IEditableSkeleton>& InEditableSkeleton)
{
	OnObjectsSelected = InArgs._OnObjectsSelected;
	OnNotifySelected = InArgs._OnNotifySelected;
	bIsPicker = InArgs._IsPicker;

	EditableSkeleton = InEditableSkeleton;

	EditableSkeleton->RegisterOnNotifiesChanged(FSimpleDelegate::CreateSP(this, &SSkeletonAnimNotifies::OnNotifiesChanged));

	if(GEditor)
	{
		GEditor->RegisterForUndo(this);
	}

	FOnContextMenuOpening OnContextMenuOpening = !bIsPicker ? FOnContextMenuOpening::CreateSP(this, &SSkeletonAnimNotifies::OnGetContextMenuContent) : FOnContextMenuOpening();

	this->ChildSlot
	[
		SNew( SVerticalBox )

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding( FMargin( 0.0f, 0.0f, 0.0f, 4.0f ) )
		[
			SAssignNew( NameFilterBox, SSearchBox )
			.SelectAllTextWhenFocused( true )
			.OnTextChanged( this, &SSkeletonAnimNotifies::OnFilterTextChanged )
			.OnTextCommitted( this, &SSkeletonAnimNotifies::OnFilterTextCommitted )
			.HintText( LOCTEXT( "NotifiesSearchBoxHint", "Search Animation Notifies...") )
		]

		+ SVerticalBox::Slot()
		.FillHeight( 1.0f )		// This is required to make the scrollbar work, as content overflows Slate containers by default
		[
			SAssignNew( NotifiesListView, SAnimNotifyListType )
			.ListItemsSource( &NotifyList )
			.OnGenerateRow( this, &SSkeletonAnimNotifies::GenerateNotifyRow )
			.OnContextMenuOpening( OnContextMenuOpening )
			.OnSelectionChanged( this, &SSkeletonAnimNotifies::OnNotifySelectionChanged )
			.ItemHeight( 22.0f )
			.OnItemScrolledIntoView( this, &SSkeletonAnimNotifies::OnItemScrolledIntoView )
		]
	];

	CreateNotifiesList();
}

SSkeletonAnimNotifies::~SSkeletonAnimNotifies()
{
	if(GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}
}

void SSkeletonAnimNotifies::OnNotifiesChanged()
{
	RefreshNotifiesListWithFilter();
}

void SSkeletonAnimNotifies::OnFilterTextChanged( const FText& SearchText )
{
	FilterText = SearchText;

	RefreshNotifiesListWithFilter();
}

void SSkeletonAnimNotifies::OnFilterTextCommitted( const FText& SearchText, ETextCommit::Type CommitInfo )
{
	// Just do the same as if the user typed in the box
	OnFilterTextChanged( SearchText );
}

TSharedRef<ITableRow> SSkeletonAnimNotifies::GenerateNotifyRow(TSharedPtr<FDisplayedAnimNotifyInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable)
{
	check( InInfo.IsValid() );

	return
		SNew( STableRow<TSharedPtr<FDisplayedAnimNotifyInfo>>, OwnerTable )
		[
			SNew( SVerticalBox )
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 0.0f, 4.0f )
			.VAlign( VAlign_Center )
			[
				SAssignNew(InInfo->InlineEditableText, SInlineEditableTextBlock)
				.Text(FText::FromName(InInfo->Name))
				.OnVerifyTextChanged(this, &SSkeletonAnimNotifies::OnVerifyNotifyNameCommit, InInfo)
				.OnTextCommitted(this, &SSkeletonAnimNotifies::OnNotifyNameCommitted, InInfo)
				.IsSelected(this, &SSkeletonAnimNotifies::IsSelected)
				.HighlightText_Lambda([this](){ return FilterText; })
				.IsReadOnly(bIsPicker)
			]
		];
}

TSharedPtr<SWidget> SSkeletonAnimNotifies::OnGetContextMenuContent() const
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, NULL);

	MenuBuilder.BeginSection("AnimNotifyAction", LOCTEXT( "AnimNotifyActions", "Notifies" ) );
	{
		FUIAction Action = FUIAction( FExecuteAction::CreateSP( this, &SSkeletonAnimNotifies::OnAddAnimNotify ) );
		const FText Label = LOCTEXT("NewAnimNotifyButtonLabel", "New...");
		const FText ToolTipText = LOCTEXT("NewAnimNotifyButtonTooltip", "Creates a new anim notify.");
		MenuBuilder.AddMenuEntry( Label, ToolTipText, FSlateIcon(), Action);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AnimNotifyAction", LOCTEXT( "SelectedAnimNotifyActions", "Selected Notify Actions" ) );
	{
		{
			FUIAction Action = FUIAction( FExecuteAction::CreateSP( this, &SSkeletonAnimNotifies::OnRenameAnimNotify ), 
				FCanExecuteAction::CreateSP( this, &SSkeletonAnimNotifies::CanPerformRename ) );
			const FText Label = LOCTEXT("RenameAnimNotifyButtonLabel", "Rename");
			const FText ToolTipText = LOCTEXT("RenameAnimNotifyButtonTooltip", "Renames the selected anim notifies.");
			MenuBuilder.AddMenuEntry( Label, ToolTipText, FSlateIcon(), Action);
		}

		{
			FUIAction Action = FUIAction( FExecuteAction::CreateSP( this, &SSkeletonAnimNotifies::OnDeleteAnimNotify ), 
				FCanExecuteAction::CreateSP( this, &SSkeletonAnimNotifies::CanPerformDelete ) );
			const FText Label = LOCTEXT("DeleteAnimNotifyButtonLabel", "Delete");
			const FText ToolTipText = LOCTEXT("DeleteAnimNotifyButtonTooltip", "Deletes the selected anim notifies.");
			MenuBuilder.AddMenuEntry( Label, ToolTipText, FSlateIcon(), Action);
		}


	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SSkeletonAnimNotifies::OnNotifySelectionChanged(TSharedPtr<FDisplayedAnimNotifyInfo> Selection, ESelectInfo::Type SelectInfo)
{
	if(Selection.IsValid())
	{
		ShowNotifyInDetailsView(Selection->Name);

		OnNotifySelected.ExecuteIfBound(Selection->Name);
	}
}

bool SSkeletonAnimNotifies::CanPerformDelete() const
{
	return NotifiesListView->GetNumItemsSelected() > 0;
}

bool SSkeletonAnimNotifies::CanPerformRename() const
{
	return NotifiesListView->GetNumItemsSelected() == 1;
}

void SSkeletonAnimNotifies::OnAddAnimNotify()
{
	// Find a unique name for this notify
	const TCHAR* BaseNotifyString = TEXT("NewNotify");
	FString NewNotifyString = BaseNotifyString;
	int32 NumericExtension = 0;

	while(EditableSkeleton->GetSkeleton().AnimationNotifies.ContainsByPredicate([&NewNotifyString](const FName& InNotifyName){ return InNotifyName.ToString() == NewNotifyString; }))
	{
		NewNotifyString = FString::Printf(TEXT("%s_%d"), BaseNotifyString, NumericExtension);
		NumericExtension++;
	}

	// Add an item. The subsequent rename will commit the item.
	TSharedPtr<FDisplayedAnimNotifyInfo> NewItem = FDisplayedAnimNotifyInfo::Make(*NewNotifyString);
	NewItem->bIsNew = true;
	NotifyList.Add(NewItem);

	NotifiesListView->ClearSelection();
	NotifiesListView->RequestListRefresh();
	NotifiesListView->RequestScrollIntoView(NewItem);
}

void SSkeletonAnimNotifies::OnItemScrolledIntoView(TSharedPtr<FDisplayedAnimNotifyInfo> InItem, const TSharedPtr<ITableRow>& InTableRow)
{
	if(InItem.IsValid() && InItem->InlineEditableText.IsValid() && InItem->bIsNew)
	{
		InItem->InlineEditableText->EnterEditingMode();
	}
}

void SSkeletonAnimNotifies::OnDeleteAnimNotify()
{
	TArray< TSharedPtr< FDisplayedAnimNotifyInfo > > SelectedRows = NotifiesListView->GetSelectedItems();

	// this one deletes all notifies with same name. 
	TArray<FName> SelectedNotifyNames;

	for(int Selection = 0; Selection < SelectedRows.Num(); ++Selection)
	{
		SelectedNotifyNames.Add(SelectedRows[Selection]->Name);
	}

	int32 NumAnimationsModified = EditableSkeleton->DeleteAnimNotifies(SelectedNotifyNames);

	if(NumAnimationsModified > 0)
	{
		// Tell the user that the socket is a duplicate
		FFormatNamedArguments Args;
		Args.Add( TEXT("NumAnimationsModified"), NumAnimationsModified );
		FNotificationInfo Info( FText::Format( LOCTEXT( "AnimNotifiesDeleted", "{NumAnimationsModified} animation(s) modified to delete notifications" ), Args ) );

		Info.bUseLargeFont = false;
		Info.ExpireDuration = 5.0f;

		NotifyUser( Info );
	}

	CreateNotifiesList( NameFilterBox->GetText().ToString() );
}

void SSkeletonAnimNotifies::OnRenameAnimNotify()
{
	TArray< TSharedPtr< FDisplayedAnimNotifyInfo > > SelectedRows = NotifiesListView->GetSelectedItems();

	check(SelectedRows.Num() == 1); // Should be guaranteed by CanPerformRename

	SelectedRows[0]->InlineEditableText->EnterEditingMode();
}

bool SSkeletonAnimNotifies::OnVerifyNotifyNameCommit( const FText& NewName, FText& OutErrorMessage, TSharedPtr<FDisplayedAnimNotifyInfo> Item )
{
	bool bValid(true);

	if(NewName.IsEmpty())
	{
		OutErrorMessage = LOCTEXT( "NameMissing_Error", "You must provide a name." );
		bValid = false;
	}

	FName NotifyName( *NewName.ToString() );
	if(NotifyName != Item->Name || Item->bIsNew)
	{
		if(EditableSkeleton->GetSkeleton().AnimationNotifies.Contains(NotifyName))
		{
			OutErrorMessage = FText::Format( LOCTEXT("AlreadyInUseMessage", "'{0}' is already in use."), NewName );
			bValid = false;
		}
	}

	return bValid;
}

void SSkeletonAnimNotifies::OnNotifyNameCommitted( const FText& NewName, ETextCommit::Type, TSharedPtr<FDisplayedAnimNotifyInfo> Item )
{
	FName NewFName = FName(*NewName.ToString());
	if(Item->bIsNew)
	{
		EditableSkeleton->AddNotify(NewFName);
		Item->bIsNew = false;
	}
	else
	{
		if(NewFName != Item->Name)
		{
			int32 NumAnimationsModified = EditableSkeleton->RenameNotify(FName(*NewName.ToString()), Item->Name);

			if(NumAnimationsModified > 0)
			{
				// Tell the user that the socket is a duplicate
				FFormatNamedArguments Args;
				Args.Add( TEXT("NumAnimationsModified"), NumAnimationsModified );
				FNotificationInfo Info( FText::Format( LOCTEXT( "AnimNotifiesRenamed", "{NumAnimationsModified} animation(s) modified to rename notification" ), Args ) );

				Info.bUseLargeFont = false;
				Info.ExpireDuration = 5.0f;

				NotifyUser( Info );
			}

			RefreshNotifiesListWithFilter();
		}
	}
}

void SSkeletonAnimNotifies::RefreshNotifiesListWithFilter()
{
	CreateNotifiesList( NameFilterBox->GetText().ToString() );
}

void SSkeletonAnimNotifies::CreateNotifiesList( const FString& SearchText )
{
	NotifyList.Empty();

	for(int i = 0; i < EditableSkeleton->GetSkeleton().AnimationNotifies.Num(); ++i)
	{
		const FName& NotifyName = EditableSkeleton->GetSkeleton().AnimationNotifies[i];
		if ( !SearchText.IsEmpty() )
		{
			if ( NotifyName.ToString().Contains( SearchText ) )
			{
				NotifyList.Add( FDisplayedAnimNotifyInfo::Make( NotifyName ) );
			}
		}
		else
		{
			NotifyList.Add( FDisplayedAnimNotifyInfo::Make( NotifyName ) );
		}
	}

	NotifiesListView->RequestListRefresh();
}

void SSkeletonAnimNotifies::ShowNotifyInDetailsView(FName NotifyName)
{
	if(OnObjectsSelected.IsBound())
	{
		ClearDetailsView();

		UEditorSkeletonNotifyObj *Obj = Cast<UEditorSkeletonNotifyObj>(ShowInDetailsView(UEditorSkeletonNotifyObj::StaticClass()));
		if(Obj != NULL)
		{
			Obj->EditableSkeleton = EditableSkeleton;
			Obj->Name = NotifyName;
		}
	}
}

UObject* SSkeletonAnimNotifies::ShowInDetailsView( UClass* EdClass )
{
	UObject *Obj = EditorObjectTracker.GetEditorObjectForClass(EdClass);

	if(Obj != NULL)
	{
		TArray<UObject*> Objects;
		Objects.Add(Obj);
		OnObjectsSelected.ExecuteIfBound(Objects);
	}
	return Obj;
}

void SSkeletonAnimNotifies::ClearDetailsView()
{
	TArray<UObject*> Objects;
	OnObjectsSelected.ExecuteIfBound(Objects);
}

void SSkeletonAnimNotifies::PostUndo( bool bSuccess )
{
	RefreshNotifiesListWithFilter();
}

void SSkeletonAnimNotifies::PostRedo( bool bSuccess )
{
	RefreshNotifiesListWithFilter();
}

void SSkeletonAnimNotifies::AddReferencedObjects( FReferenceCollector& Collector )
{
	EditorObjectTracker.AddReferencedObjects(Collector);
}

void SSkeletonAnimNotifies::NotifyUser( FNotificationInfo& NotificationInfo )
{
	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification( NotificationInfo );
	if ( Notification.IsValid() )
	{
		Notification->SetCompletionState( SNotificationItem::CS_Fail );
	}
}
#undef LOCTEXT_NAMESPACE
