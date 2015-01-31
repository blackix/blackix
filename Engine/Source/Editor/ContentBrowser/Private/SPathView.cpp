// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ContentBrowserPCH.h"

#include "DragAndDrop/AssetPathDragDropOp.h"

#include "PathViewTypes.h"
#include "ObjectTools.h"
#include "SourcesViewWidgets.h"
#include "SSearchBox.h"
#include "NativeClassHierarchy.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

SPathView::~SPathView()
{
	// Unsubscribe from content path events
	FPackageName::OnContentPathMounted().RemoveAll( this );
	FPackageName::OnContentPathDismounted().RemoveAll( this );

	// Unsubscribe from class events
	if ( bAllowClassesFolder )
	{
		TSharedRef<FNativeClassHierarchy> NativeClassHierarchy = FContentBrowserSingleton::Get().GetNativeClassHierarchy();
		NativeClassHierarchy->OnClassHierarchyUpdated().RemoveAll( this );
	}

	// Load the asset registry module to stop listening for updates
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnPathAdded().RemoveAll( this );
	AssetRegistryModule.Get().OnPathRemoved().RemoveAll( this );
	AssetRegistryModule.Get().OnFilesLoaded().RemoveAll( this );

	SearchBoxFolderFilter->OnChanged().RemoveAll( this );
}

void SPathView::Construct( const FArguments& InArgs )
{
	bNeedsRepopulate = true;

	OnPathSelected = InArgs._OnPathSelected;
	bAllowContextMenu = InArgs._AllowContextMenu;
	OnGetFolderContextMenu = InArgs._OnGetFolderContextMenu;
	OnGetPathContextMenuExtender = InArgs._OnGetPathContextMenuExtender;
	bPendingFocusNextFrame = InArgs._FocusSearchBoxWhenOpened;
	bAllowClassesFolder = InArgs._AllowClassesFolder;
	PreventTreeItemChangedDelegateCount = 0;

	// Listen for when view settings are changed
	UContentBrowserSettings::OnSettingChanged().AddSP(this, &SPathView::HandleSettingChanged);

	//Setup the SearchBox filter
	SearchBoxFolderFilter = MakeShareable( new FolderTextFilter( FolderTextFilter::FItemToStringArray::CreateSP( this, &SPathView::PopulateFolderSearchStrings ) ) );
	SearchBoxFolderFilter->OnChanged().AddSP( this, &SPathView::FilterUpdated );

	// Listen to find out when new game content paths are mounted or dismounted, so that we can refresh our root set of paths
	FPackageName::OnContentPathMounted().AddSP( this, &SPathView::OnContentPathMountedOrDismounted );
	FPackageName::OnContentPathDismounted().AddSP( this, &SPathView::OnContentPathMountedOrDismounted );

	// Listen to find out when the available classes are changed, so that we can refresh our paths
	if ( bAllowClassesFolder )
	{
		TSharedRef<FNativeClassHierarchy> NativeClassHierarchy = FContentBrowserSingleton::Get().GetNativeClassHierarchy();
		NativeClassHierarchy->OnClassHierarchyUpdated().AddSP( this, &SPathView::OnClassHierarchyUpdated );
	}

	ChildSlot
	[
		SNew(SVerticalBox)

		// Search
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 1, 0, 3)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				InArgs._SearchContent.Widget
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(SearchBoxPtr, SSearchBox)
				.HintText( LOCTEXT( "AssetTreeSearchBoxHint", "Search Folders" ) )
				.OnTextChanged( this, &SPathView::OnAssetTreeSearchBoxChanged )
			]
		]

		// Tree title
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Font( FEditorStyle::GetFontStyle("ContentBrowser.SourceTitleFont") )
			.Text( LOCTEXT("AssetTreeTitle", "Asset Tree") )
			.Visibility(InArgs._ShowTreeTitle ? EVisibility::Visible : EVisibility::Collapsed)
		]

		// Separator
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 1)
		[
			SNew(SSeparator)
			.Visibility( ( InArgs._ShowSeparator) ? EVisibility::Visible : EVisibility::Collapsed )
		]
			
		// Tree
		+SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SAssignNew(TreeViewPtr, STreeView< TSharedPtr<FTreeItem> >)
			.TreeItemsSource(&TreeRootItems)
			.OnGenerateRow( this, &SPathView::GenerateTreeRow )
			.OnItemScrolledIntoView( this, &SPathView::TreeItemScrolledIntoView )
			.ItemHeight(18)
			.SelectionMode(InArgs._SelectionMode)
			.OnSelectionChanged(this, &SPathView::TreeSelectionChanged)
			.OnExpansionChanged(this, &SPathView::TreeExpansionChanged)
			.OnGetChildren( this, &SPathView::GetChildrenForTree )
			.OnSetExpansionRecursive( this, &SPathView::SetTreeItemExpansionRecursive )
			.OnContextMenuOpening(this, &SPathView::MakePathViewContextMenu)
			.ClearSelectionOnClick(false)
		]
	];

	// Load the asset registry module to listen for updates
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnPathAdded().AddSP( this, &SPathView::OnAssetRegistryPathAdded );
	AssetRegistryModule.Get().OnPathRemoved().AddSP( this, &SPathView::OnAssetRegistryPathRemoved );
	AssetRegistryModule.Get().OnFilesLoaded().AddSP( this, &SPathView::OnAssetRegistrySearchCompleted );

	// Add all paths currently gathered from the asset registry
	Populate();

	// Always expand the game root initially
	static const FString GameRootName = TEXT("Game");
	for ( auto RootIt = TreeRootItems.CreateConstIterator(); RootIt; ++RootIt )
	{
		if ( (*RootIt)->FolderName == GameRootName )
		{
			TreeViewPtr->SetItemExpansion(*RootIt, true);
		}
	}
}

void SPathView::SetSelectedPaths(const TArray<FString>& Paths)
{
	if ( !ensure(TreeViewPtr.IsValid()) )
	{
		return;
	}

	if ( !SearchBoxPtr->GetText().IsEmpty() )
	{
		// Clear the search box so the selected paths will be visible
		SearchBoxPtr->SetText( FText::GetEmpty() );
	}

	// Prevent the selection changed delegate since the invoking code requested it
	FScopedPreventTreeItemChangedDelegate DelegatePrevention( SharedThis(this) );

	// If the selection was changed before all pending initial paths were found, stop attempting to select them
	PendingInitialPaths.Empty();

	// Clear the selection to start, then add the selected paths as they are found
	TreeViewPtr->ClearSelection();

	for (int32 PathIdx = 0; PathIdx < Paths.Num(); ++PathIdx)
	{
		const FString& Path = Paths[PathIdx];

		TArray<FString> PathItemList;
		Path.ParseIntoArray(&PathItemList, TEXT("/"), /*InCullEmpty=*/true);

		if ( PathItemList.Num() )
		{
			// There is at least one element in the path
			TArray<TSharedPtr<FTreeItem>> TreeItems;

			// Find the first item in the root items list
			for ( int32 RootItemIdx = 0; RootItemIdx < TreeRootItems.Num(); ++RootItemIdx )
			{
				if ( TreeRootItems[RootItemIdx]->FolderName == PathItemList[0] )
				{
					// Found the first item in the path
					TreeItems.Add(TreeRootItems[RootItemIdx]);
					break;
				}
			}

			// If found in the root items list, try to find the childmost item matching the path
			if ( TreeItems.Num() > 0 )
			{
				for ( int32 PathItemIdx = 1; PathItemIdx < PathItemList.Num(); ++PathItemIdx )
				{
					const FString& PathItemName = PathItemList[PathItemIdx];
					const TSharedPtr<FTreeItem> ChildItem = TreeItems.Last()->GetChild(PathItemName);

					if ( ChildItem.IsValid() )
					{
						// Update tree items list
						TreeItems.Add(ChildItem);
					}
					else
					{
						// Could not find the child item
						break;
					}
				}

				// Expand all the tree folders up to but not including the last one.
				for (int32 ItemIdx = 0; ItemIdx < TreeItems.Num() - 1; ++ItemIdx)
				{
					TreeViewPtr->SetItemExpansion(TreeItems[ItemIdx], true);
				}

				// Set the selection to the closest found folder and scroll it into view
				TreeViewPtr->SetItemSelection(TreeItems.Last(), true);
				TreeViewPtr->RequestScrollIntoView(TreeItems.Last());
			}
			else
			{
				// Could not even find the root path... skip
			}
		}
		else
		{
			// No path items... skip
		}
	}
}

void SPathView::ClearSelection()
{
	// Prevent the selection changed delegate since the invoking code requested it
	FScopedPreventTreeItemChangedDelegate DelegatePrevention( SharedThis(this) );

	// If the selection was changed before all pending initial paths were found, stop attempting to select them
	PendingInitialPaths.Empty();

	// Clear the selection to start, then add the selected paths as they are found
	TreeViewPtr->ClearSelection();
}

FString SPathView::GetSelectedPath() const
{
	TArray<TSharedPtr<FTreeItem>> Items = TreeViewPtr->GetSelectedItems();
	if ( Items.Num() > 0 )
	{
		return Items[0]->FolderPath;
	}

	return FString();
}

TArray<FString> SPathView::GetSelectedPaths() const
{
	TArray<FString> RetArray;

	TArray<TSharedPtr<FTreeItem>> Items = TreeViewPtr->GetSelectedItems();
	for ( int32 ItemIdx = 0; ItemIdx < Items.Num(); ++ItemIdx )
	{
		RetArray.Add(Items[ItemIdx]->FolderPath);
	}

	return RetArray;
}

TSharedPtr<FTreeItem> SPathView::AddPath(const FString& Path, bool bUserNamed)
{
	if ( !ensure(TreeViewPtr.IsValid()) )
	{
		// No tree view for some reason
		return TSharedPtr<FTreeItem>();
	}

	TArray<FString> PathItemList;
	Path.ParseIntoArray(&PathItemList, TEXT("/"), /*InCullEmpty=*/true);

	if ( PathItemList.Num() )
	{
		// There is at least one element in the path
		TSharedPtr<FTreeItem> CurrentItem;

		// Find the first item in the root items list
		for ( int32 RootItemIdx = 0; RootItemIdx < TreeRootItems.Num(); ++RootItemIdx )
		{
			if ( TreeRootItems[RootItemIdx]->FolderName == PathItemList[0] )
			{
				// Found the first item in the path
				CurrentItem = TreeRootItems[RootItemIdx];
				break;
			}
		}

		// Roots may or may not exist, add the root here if it doesn't
		if ( !CurrentItem.IsValid() )
		{
			CurrentItem = AddRootItem(PathItemList[0]);
		}

		// Found or added the root item?
		if ( CurrentItem.IsValid() )
		{
			// Now add children as necessary
			const bool bDisplayDev = GetDefault<UContentBrowserSettings>()->GetDisplayDevelopersFolder();
			for ( int32 PathItemIdx = 1; PathItemIdx < PathItemList.Num(); ++PathItemIdx )
			{
				const FString& PathItemName = PathItemList[PathItemIdx];
				TSharedPtr<FTreeItem> ChildItem = CurrentItem->GetChild(PathItemName);

				// If it does not exist, Create the child item
				if ( !ChildItem.IsValid() )
				{
					const FString FolderName = PathItemName;
					const FString FolderPath = CurrentItem->FolderPath + "/" + PathItemName;

					// If this is a developer folder, and we don't want to show them break out here
					if ( !bDisplayDev && ContentBrowserUtils::IsDevelopersFolder(FolderPath) )
					{
						break;
					}

					ChildItem = MakeShareable( new FTreeItem(FText::FromString(FolderName), FolderName, FolderPath, CurrentItem, bUserNamed) );
					CurrentItem->Children.Add(ChildItem);
					CurrentItem->SortChildren();
					TreeViewPtr->RequestTreeRefresh();

					// If we have pending initial paths, and this path added the path, we should select it now
					if ( PendingInitialPaths.Num() > 0 && PendingInitialPaths.Contains(FolderPath) )
					{
						RecursiveExpandParents(ChildItem);
						TreeViewPtr->SetItemSelection(ChildItem, true);
						TreeViewPtr->RequestScrollIntoView(ChildItem);
					}
				}

				CurrentItem = ChildItem;
			}

			if ( bUserNamed && CurrentItem->Parent.IsValid() )
			{
				// If we were creating a new item, select it, scroll it into view, expand the parent
				RecursiveExpandParents(CurrentItem);
				TreeViewPtr->RequestScrollIntoView(CurrentItem);
				TreeViewPtr->SetSelection(CurrentItem);
			}
			else
			{
				CurrentItem->bNamingFolder = false;
			}
		}

		return CurrentItem;
	}

	return TSharedPtr<FTreeItem>();
}

bool SPathView::RemovePath(const FString& Path)
{
	if ( !ensure(TreeViewPtr.IsValid()) )
	{
		// No tree view for some reason
		return false;
	}

	if ( Path.IsEmpty() )
	{
		// There were no elements in the path, cannot remove nothing
		return false;
	}

	// Find the folder in the tree
	TSharedPtr<FTreeItem> ItemToRemove = FindItemRecursive(Path);

	if ( ItemToRemove.IsValid() )
	{
		// Found the folder to remove. Remove it.
		if ( ItemToRemove->Parent.IsValid() )
		{
			// Remove the folder from its parent's list
			ItemToRemove->Parent.Pin()->Children.Remove(ItemToRemove);
		}
		else
		{
			// This is a root item. Remove the folder from the root items list.
			TreeRootItems.Remove(ItemToRemove);
		}

		// Refresh the tree
		TreeViewPtr->RequestTreeRefresh();

		return true;
	}
	else
	{
		// Did not find the folder to remove
		return false;
	}
}

void SPathView::RenameFolder(const FString& FolderToRename)
{
	TArray<TSharedPtr<FTreeItem>> Items = TreeViewPtr->GetSelectedItems();
	for (int32 ItemIdx = 0; ItemIdx < Items.Num(); ++ItemIdx)
	{
		TSharedPtr<FTreeItem>& Item = Items[ItemIdx];
		if (Item.IsValid())
		{
			if (Item->FolderPath == FolderToRename)
			{
				Item->bNamingFolder = true;

				TreeViewPtr->SetSelection(Item);
				TreeViewPtr->RequestScrollIntoView(Item);
				break;
			}
		}
	}
}

void SPathView::SyncToAssets( const TArray<FAssetData>& AssetDataList, const bool bAllowImplicitSync )
{
	TArray<TSharedPtr<FTreeItem>> SyncTreeItems;

	// Clear the filter
	SearchBoxPtr->SetText(FText::GetEmpty());

	for (auto AssetDataIt = AssetDataList.CreateConstIterator(); AssetDataIt; ++AssetDataIt)
	{
		FString Path;
		if ( AssetDataIt->AssetClass == NAME_Class )
		{
			if ( bAllowClassesFolder )
			{
				// Classes are found in the /Classes_ roots
				TSharedRef<FNativeClassHierarchy> NativeClassHierarchy = FContentBrowserSingleton::Get().GetNativeClassHierarchy();
				NativeClassHierarchy->GetClassPath(Cast<UClass>(AssetDataIt->GetAsset()), Path, false/*bIncludeClassName*/);
			}
		}
		else
		{
			// All other assets are found by their package path
			Path = AssetDataIt->PackagePath.ToString();
		}

		if ( !Path.IsEmpty() )
		{
			TSharedPtr<FTreeItem> Item = FindItemRecursive(Path);
			if ( Item.IsValid() )
			{
				SyncTreeItems.Add(Item);
			}
		}
	}

	if ( SyncTreeItems.Num() > 0 )
	{
		if (bAllowImplicitSync)
		{
			// Prune the current selection so that we don't unnecessarily change the path which might disorientate the user.
			// If a parent tree item is currently selected we don't need to clear it and select the child
			auto SelectedTreeItems = TreeViewPtr->GetSelectedItems();

			for (int32 Index = 0; Index < SelectedTreeItems.Num(); ++Index)
			{
				// For each item already selected in the tree
				auto AlreadySelectedTreeItem = SelectedTreeItems[Index];
				if (!AlreadySelectedTreeItem.IsValid())
				{
					continue;
				}

				// Check to see if any of the items to sync are already synced
				for (int32 ToSyncIndex = SyncTreeItems.Num()-1; ToSyncIndex >= 0; --ToSyncIndex)
				{
					auto ToSyncItem = SyncTreeItems[ToSyncIndex];
					if (ToSyncItem == AlreadySelectedTreeItem || ToSyncItem->IsChildOf(*AlreadySelectedTreeItem.Get()))
					{
						// A parent is already selected
						SyncTreeItems.Pop();
					}
					else if (ToSyncIndex == 0)
					{
						// AlreadySelectedTreeItem is not required for SyncTreeItems, so deselect it
						TreeViewPtr->SetItemSelection(AlreadySelectedTreeItem, false);
					}
				}
			}
		}
		else
		{
			// Explicit sync so just clear the selection
			TreeViewPtr->ClearSelection();
		}

		// SyncTreeItems should now only contain items which aren't already shown explicitly or implicitly (as a child)
		for ( auto ItemIt = SyncTreeItems.CreateConstIterator(); ItemIt; ++ItemIt )
		{
			RecursiveExpandParents(*ItemIt);
			TreeViewPtr->SetItemSelection(*ItemIt, true);
		}

		// > 0 as some may have been popped off in the code above
		if (SyncTreeItems.Num() > 0)
		{
			// Scroll the first item into view if applicable
			TreeViewPtr->RequestScrollIntoView(SyncTreeItems[0]);
		}
	}
}

TSharedPtr<FTreeItem> SPathView::FindItemRecursive(const FString& Path) const
{
	for (auto TreeItemIt = TreeRootItems.CreateConstIterator(); TreeItemIt; ++TreeItemIt)
	{
		if ( (*TreeItemIt)->FolderPath == Path)
		{
			// This root item is the path
			return *TreeItemIt;
		}

		// Try to find the item under this root
		TSharedPtr<FTreeItem> Item = (*TreeItemIt)->FindItemRecursive(Path);
		if ( Item.IsValid() )
		{
			// The item was found under this root
			return Item;
		}
	}

	return TSharedPtr<FTreeItem>();
}

void SPathView::ApplyHistoryData ( const FHistoryData& History )
{
	// Prevent the selection changed delegate because it would add more history when we are just setting a state
	FScopedPreventTreeItemChangedDelegate DelegatePrevention( SharedThis(this) );

	// Update paths
	TArray<FString> SelectedPaths;
	for ( auto PathIt = History.SourcesData.PackagePaths.CreateConstIterator(); PathIt; ++PathIt)
	{
		SelectedPaths.Add((*PathIt).ToString());
	}
	SetSelectedPaths(SelectedPaths);
}

void SPathView::SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const
{
	FString SelectedPathsString;
	TArray< TSharedPtr<FTreeItem> > PathItems = TreeViewPtr->GetSelectedItems();
	for ( auto PathIt = PathItems.CreateConstIterator(); PathIt; ++PathIt )
	{
		if ( SelectedPathsString.Len() > 0 )
		{
			SelectedPathsString += TEXT(",");
		}

		SelectedPathsString += (*PathIt)->FolderPath;
	}

	GConfig->SetString(*IniSection, *(SettingsString + TEXT(".SelectedPaths")), *SelectedPathsString, IniFilename);
}

void SPathView::LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString)
{
	// Selected Paths
	FString SelectedPathsString;
	TArray<FString> NewSelectedPaths;
	if ( GConfig->GetString(*IniSection, *(SettingsString + TEXT(".SelectedPaths")), SelectedPathsString, IniFilename) )
	{
		SelectedPathsString.ParseIntoArray(&NewSelectedPaths, TEXT(","), /*bCullEmpty*/true);
	}

	if ( NewSelectedPaths.Num() > 0 )
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		const bool bDiscoveringAssets = AssetRegistryModule.Get().IsLoadingAssets();

		if ( bDiscoveringAssets )
		{
			// Keep track if we changed at least one source so we know to fire the bulk selection changed delegate later
			bool bSelectedAtLeastOnePath = false;

			{
				// Prevent the selection changed delegate since we are selecting one path at a time. A bulk event will be fired later if needed.
				FScopedPreventTreeItemChangedDelegate DelegatePrevention( SharedThis(this) );				

				// Clear any previously selected paths
				TreeViewPtr->ClearSelection();

				// If the selected paths is empty, the path was "All assets"
				// This should handle that case properly
				for (int32 PathIdx = 0; PathIdx < NewSelectedPaths.Num(); ++PathIdx)
				{
					const FString& Path = NewSelectedPaths[PathIdx];
					if ( ExplicitlyAddPathToSelection(Path) )
					{
						bSelectedAtLeastOnePath = true;
					}
					else
					{
						// If we could not initially select these paths, but are still discovering assets, add them to a pending list to select them later
						PendingInitialPaths.Add(Path);
					}
				}
			}

			if ( bSelectedAtLeastOnePath )
			{
				// Signal a single selection changed event to let any listeners know that paths have changed
				TreeSelectionChanged( TSharedPtr<FTreeItem>(), ESelectInfo::Direct );
			}
		}
		else
		{
			// If all assets are already discovered, just select paths the best we can
			SetSelectedPaths(NewSelectedPaths);

			// Signal a single selection changed event to let any listeners know that paths have changed
			TreeSelectionChanged( TSharedPtr<FTreeItem>(), ESelectInfo::Direct );
		}
	}
}

void SPathView::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if ( bPendingFocusNextFrame )
	{
		FWidgetPath WidgetToFocusPath;
		FSlateApplication::Get().GeneratePathToWidgetUnchecked( SearchBoxPtr.ToSharedRef(), WidgetToFocusPath );
		FSlateApplication::Get().SetKeyboardFocus( WidgetToFocusPath, EFocusCause::SetDirectly );
		bPendingFocusNextFrame = false;
	}

	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if( bNeedsRepopulate )
	{
		Populate();
	}
}

TSharedPtr<SWidget> SPathView::MakePathViewContextMenu()
{
	if ( TreeViewPtr->GetSelectedItems().Num() <= 0 || !bAllowContextMenu )
	{
		return NULL;
	}

	if(!OnGetFolderContextMenu.IsBound())
	{
		return NULL;
	}

	const TArray<FString> SelectedPaths = GetSelectedPaths();
	return OnGetFolderContextMenu.Execute(SelectedPaths, OnGetPathContextMenuExtender, FOnCreateNewFolder::CreateSP(this, &SPathView::OnCreateNewFolder));
}

void SPathView::OnCreateNewFolder(const FString& FolderName, const FString& FolderPath)
{
	AddPath(FolderPath / FolderName, /*bUserNamed=*/true);
}

bool SPathView::ExplicitlyAddPathToSelection(const FString& Path)
{
	if ( !ensure(TreeViewPtr.IsValid()) )
	{
		return false;
	}

	TArray<FString> PathItemList;
	Path.ParseIntoArray(&PathItemList, TEXT("/"), /*InCullEmpty=*/true);

	if ( PathItemList.Num() )
	{
		// There is at least one element in the path
		TSharedPtr<FTreeItem> RootItem;

		// Find the first item in the root items list
		for ( int32 RootItemIdx = 0; RootItemIdx < TreeRootItems.Num(); ++RootItemIdx )
		{
			if ( TreeRootItems[RootItemIdx]->FolderName == PathItemList[0] )
			{
				// Found the first item in the path
				RootItem = TreeRootItems[RootItemIdx];
				break;
			}
		}

		// If found in the root items list, try to find the item matching the path
		if ( RootItem.IsValid() )
		{
			TSharedPtr<FTreeItem> FoundItem = RootItem->FindItemRecursive(Path);

			if ( FoundItem.IsValid() )
			{
				// Set the selection to the closest found folder and scroll it into view
				RecursiveExpandParents(FoundItem);
				TreeViewPtr->SetItemSelection(FoundItem, true);
				TreeViewPtr->RequestScrollIntoView(FoundItem);

				return true;
			}
		}
	}

	return false;
}

bool SPathView::ShouldAllowTreeItemChangedDelegate() const
{
	return PreventTreeItemChangedDelegateCount == 0;
}

void SPathView::RecursiveExpandParents(const TSharedPtr<FTreeItem>& Item)
{
	if ( Item->Parent.IsValid() )
	{
		RecursiveExpandParents(Item->Parent.Pin());
		TreeViewPtr->SetItemExpansion(Item->Parent.Pin(), true);
	}
}

TSharedPtr<struct FTreeItem> SPathView::AddRootItem( const FString& InFolderName )
{
	// Make sure the item is not already in the list
	for ( int32 RootItemIdx = 0; RootItemIdx < TreeRootItems.Num(); ++RootItemIdx )
	{
		if ( TreeRootItems[RootItemIdx]->FolderName == InFolderName )
		{
			// The root to add was already in the list return it here
			return TreeRootItems[RootItemIdx];
		}
	}

	TSharedPtr<struct FTreeItem> NewItem = NULL;

	// If this isn't an engine folder or we want to show them, add
	const bool bDisplayEngine = GetDefault<UContentBrowserSettings>()->GetDisplayEngineFolder();
	if ( bDisplayEngine || !ContentBrowserUtils::IsEngineFolder(InFolderName) )
	{
		const bool bDisplayPlugins = GetDefault<UContentBrowserSettings>()->GetDisplayPluginFolders();
		if ( bDisplayPlugins || !ContentBrowserUtils::IsPluginFolder(InFolderName) )
		{
			const FText DisplayName = ContentBrowserUtils::GetRootDirDisplayName(InFolderName);
			NewItem = MakeShareable( new FTreeItem(DisplayName, InFolderName, FString(TEXT("/")) + InFolderName, TSharedPtr<FTreeItem>()));
			TreeRootItems.Add( NewItem );
			TreeViewPtr->RequestTreeRefresh();
		}
	}

	return NewItem;
}

TSharedRef<ITableRow> SPathView::GenerateTreeRow( TSharedPtr<FTreeItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	check(TreeItem.IsValid());

	return
		SNew( STableRow< TSharedPtr<FTreeItem> >, OwnerTable )
		.OnDragDetected( this, &SPathView::OnFolderDragDetected )
		[
			SNew(SAssetTreeItem)
			.TreeItem(TreeItem)
			.OnNameChanged(this, &SPathView::FolderNameChanged)
			.OnVerifyNameChanged(this, &SPathView::VerifyFolderNameChanged)
			.OnAssetsDragDropped(this, &SPathView::TreeAssetsDropped)
			.OnPathsDragDropped(this, &SPathView::TreeFoldersDropped)
			.OnFilesDragDropped(this, &SPathView::TreeFilesDropped)
			.IsItemExpanded(this, &SPathView::IsTreeItemExpanded, TreeItem)
			.HighlightText(this, &SPathView::GetHighlightText)
			.IsSelected(this, &SPathView::IsTreeItemSelected, TreeItem)
		];
}

void SPathView::TreeItemScrolledIntoView( TSharedPtr<FTreeItem> TreeItem, const TSharedPtr<ITableRow>& Widget )
{
	if ( TreeItem->bNamingFolder && Widget.IsValid() && Widget->GetContent().IsValid() )
	{
		TreeItem->OnRenamedRequestEvent.Broadcast();
	}
}

void SPathView::GetChildrenForTree( TSharedPtr< FTreeItem > TreeItem, TArray< TSharedPtr<FTreeItem> >& OutChildren )
{
	OutChildren = TreeItem->Children;
}

void SPathView::SetTreeItemExpansionRecursive( TSharedPtr< FTreeItem > TreeItem, bool bInExpansionState )
{
	TreeViewPtr->SetItemExpansion(TreeItem, bInExpansionState);

	// Recursively go through the children.
	for(auto It = TreeItem->Children.CreateIterator(); It; ++It)
	{
		SetTreeItemExpansionRecursive( *It, bInExpansionState );
	}
}

void SPathView::TreeSelectionChanged( TSharedPtr< FTreeItem > TreeItem, ESelectInfo::Type /*SelectInfo*/ )
{
	if ( ShouldAllowTreeItemChangedDelegate() )
	{
		const TArray<TSharedPtr<FTreeItem>> SelectedItems = TreeViewPtr->GetSelectedItems();

		LastSelectedPaths.Empty();
		for (int32 ItemIdx = 0; ItemIdx < SelectedItems.Num(); ++ItemIdx)
		{
			const TSharedPtr<FTreeItem> Item = SelectedItems[ItemIdx];
			if ( !ensure(Item.IsValid()) )
			{
				// All items must exist
				continue;
			}

			// Keep track of the last paths that we broadcasted for selection reasons when filtering
			LastSelectedPaths.Add(Item->FolderPath);
		}

		if ( OnPathSelected.IsBound() )
		{
			if ( TreeItem.IsValid() )
			{
				OnPathSelected.Execute(TreeItem->FolderPath);
			}
			else
			{
				OnPathSelected.Execute(TEXT(""));
			}
		}
	}

	if (TreeItem.IsValid())
	{
		// Prioritize the asset registry scan for the selected path
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().PrioritizeSearchPath(TreeItem->FolderPath);
	}
}

void SPathView::TreeExpansionChanged( TSharedPtr< FTreeItem > TreeItem, bool bIsExpanded )
{
	if ( ShouldAllowTreeItemChangedDelegate() )
	{
		TSet<TSharedPtr<FTreeItem>> ExpandedItemSet;
		TreeViewPtr->GetExpandedItems(ExpandedItemSet);
		const TArray<TSharedPtr<FTreeItem>> ExpandedItems = ExpandedItemSet.Array();

		LastExpandedPaths.Empty();
		for (int32 ItemIdx = 0; ItemIdx < ExpandedItems.Num(); ++ItemIdx)
		{
			const TSharedPtr<FTreeItem> Item = ExpandedItems[ItemIdx];
			if ( !ensure(Item.IsValid()) )
			{
				// All items must exist
				continue;
			}

			// Keep track of the last paths that we broadcasted for expansion reasons when filtering
			LastExpandedPaths.Add(Item->FolderPath);
		}
	}
}

void SPathView::OnAssetTreeSearchBoxChanged( const FText& InSearchText )
{
	SearchBoxFolderFilter->SetRawFilterText( InSearchText );
}

void SPathView::FilterUpdated()
{
	Populate();
}

FText SPathView::GetHighlightText() const
{
	return SearchBoxFolderFilter->GetRawFilterText();
}

void SPathView::Populate()
{
	// Don't allow the selection changed delegate to be fired here
	FScopedPreventTreeItemChangedDelegate DelegatePrevention( SharedThis(this) );

	// Clear all root items and clear selection
	TreeRootItems.Empty();
	TreeViewPtr->ClearSelection();

	// Load the native class hierarchy to listen for updates
	TSharedRef<FNativeClassHierarchy> NativeClassHierarchy = FContentBrowserSingleton::Get().GetNativeClassHierarchy();

	const bool bFilteringByText = !SearchBoxFolderFilter->GetRawFilterText().IsEmpty();
	
	const bool bDisplayEngine = GetDefault<UContentBrowserSettings>()->GetDisplayEngineFolder();
	const bool bDisplayPlugins = GetDefault<UContentBrowserSettings>()->GetDisplayPluginFolders();

	TArray<FName> ClassRoots;
	TArray<FString> ClassFolders;
	if ( bAllowClassesFolder )
	{
		NativeClassHierarchy->GetClassFolders(ClassRoots, ClassFolders, bDisplayEngine, bDisplayPlugins);
	}

	if ( !bFilteringByText )
	{
		// If we aren't filtering, add default folders to the asset tree

		for(const FName& ClassRoot : ClassRoots)
		{
			AddRootItem(ClassRoot.ToString());
		}

		// Add all of the content paths we know about.  Note that this can change on the fly (if say, a plugin
		// with content becomes loaded), so this SPathView would need to be refreshed if that happened.
		TArray<FString> RootContentPaths;
		FPackageName::QueryRootContentPaths( RootContentPaths );
		for( TArray<FString>::TConstIterator RootPathIt( RootContentPaths ); RootPathIt; ++RootPathIt )
		{
			// Strip off any leading or trailing forward slashes.  We just want a root path name that
			// we can display, and we'll add the path separators back later on
			FString CleanRootPathName = *RootPathIt;
			while( CleanRootPathName.StartsWith( TEXT( "/" ) ) )
			{
				CleanRootPathName = CleanRootPathName.Mid( 1 );
			}
			while( CleanRootPathName.EndsWith( TEXT( "/" ) ) )
			{
				CleanRootPathName = CleanRootPathName.Mid( 0, CleanRootPathName.Len() - 1 );
			}
			AddRootItem(CleanRootPathName);
		}
	}
	
	// Load the asset registry module to listen for updates
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// Add all paths currently gathered from the asset registry
	TArray<FString> PathList;
	AssetRegistryModule.Get().GetAllCachedPaths(PathList);

	// Add any class paths we discovered
	PathList.Append(ClassFolders);

	// Add the user developer folder
	const FString UserDeveloperFolder = FPackageName::FilenameToLongPackageName(FPaths::GameUserDeveloperDir().LeftChop(1));
	PathList.Add(UserDeveloperFolder);

	// we have a text filter, expand all parents of matching folders
	for ( int32 PathIdx = 0; PathIdx < PathList.Num(); ++PathIdx)
	{
		const FString& Path = PathList[PathIdx];

		// by sending the whole path we deliberately include any children
		// of successful hits in the filtered list. 
		if ( SearchBoxFolderFilter->PassesFilter( Path ) )
		{
			TSharedPtr<FTreeItem> Item = AddPath(Path);
			if ( Item.IsValid() )
			{
				const bool bSelectedItem = LastSelectedPaths.Contains(Item->FolderPath);
				const bool bExpandedItem = LastExpandedPaths.Contains(Item->FolderPath);

				if ( bFilteringByText || bSelectedItem )
				{
					RecursiveExpandParents(Item);
				}

				if ( bSelectedItem )
				{
					// Tree items that match the last broadcasted paths should be re-selected them after they are added
					if ( !TreeViewPtr->IsItemSelected(Item) )
					{
						TreeViewPtr->SetItemSelection(Item, true);
					}
					TreeViewPtr->RequestScrollIntoView(Item);
				}

				if ( bExpandedItem )
				{
					// Tree items that were previously expanded should be re-expanded when repopulating
					if ( !TreeViewPtr->IsItemExpanded(Item) )
					{
						TreeViewPtr->SetItemExpansion(Item, true);
					}
				}
			}
		}
	}

	SortRootItems();

	bNeedsRepopulate = false;
}

void SPathView::SortRootItems()
{
	// First sort the root items by their display name, but also making sure that content to appears before classes
	TreeRootItems.Sort([](const TSharedPtr<FTreeItem>& One, const TSharedPtr<FTreeItem>& Two) -> bool
	{
		static const FString ClassesPrefix = TEXT("Classes_");

		FString OneModuleName = One->FolderName;
		const bool bOneIsClass = OneModuleName.StartsWith(ClassesPrefix);
		if(bOneIsClass)
		{
			OneModuleName = OneModuleName.Mid(ClassesPrefix.Len());
		}

		FString TwoModuleName = Two->FolderName;
		const bool bTwoIsClass = TwoModuleName.StartsWith(ClassesPrefix);
		if(bTwoIsClass)
		{
			TwoModuleName = TwoModuleName.Mid(ClassesPrefix.Len());
		}

		// We want to sort content before classes if both items belong to the same module
		if(OneModuleName == TwoModuleName)
		{
			if(!bOneIsClass && bTwoIsClass)
			{
				return true;
			}
			return false;
		}

		return One->DisplayName.ToString() < Two->DisplayName.ToString();
	});

	// We have some manual sorting requirements that game must come before engine, and engine before everything else - we do that here after sorting everything by name
	// The array below is in the inverse order as we iterate through and move each match to the beginning of the root items array
	static const FString InverseSortOrder[] = {
		TEXT("Classes_Engine"),
		TEXT("Engine"),
		TEXT("Classes_Game"),
		TEXT("Game"),
	};
	for(const FString& SortItem : InverseSortOrder)
	{
		const int32 FoundItemIndex = TreeRootItems.IndexOfByPredicate([&SortItem](const TSharedPtr<FTreeItem>& TreeItem) -> bool
		{
			return TreeItem->FolderName == SortItem;
		});
		if(FoundItemIndex != INDEX_NONE)
		{
			TSharedPtr<FTreeItem> ItemToMove = TreeRootItems[FoundItemIndex];
			TreeRootItems.RemoveAt(FoundItemIndex);
			TreeRootItems.Insert(ItemToMove, 0);
		}
	}

	TreeViewPtr->RequestTreeRefresh();
}

void SPathView::PopulateFolderSearchStrings( const FString& FolderName, OUT TArray< FString >& OutSearchStrings ) const
{
	OutSearchStrings.Add( FolderName );
}

FReply SPathView::OnFolderDragDetected(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if ( MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ) )
	{
		TArray<TSharedPtr<FTreeItem>> SelectedItems = TreeViewPtr->GetSelectedItems();
		if (SelectedItems.Num())
		{
			TArray<FString> PathNames;

			for ( auto ItemIt = SelectedItems.CreateConstIterator(); ItemIt; ++ItemIt )
			{
				PathNames.Add((*ItemIt)->FolderPath);
			}

			return FReply::Handled().BeginDragDrop(FAssetPathDragDropOp::New(PathNames));
		}
	}

	return FReply::Unhandled();
}

bool SPathView::VerifyFolderNameChanged(const FText& InName, FText& OutErrorMessage, const FString& InFolderPath) const
{	
	if( !ContentBrowserUtils::IsValidFolderName(InName.ToString(), OutErrorMessage) )
	{
		return false;
	}

	const FString NewPath = FPaths::GetPath(InFolderPath) / InName.ToString();
	if (ContentBrowserUtils::DoesFolderExist(NewPath))
	{
		OutErrorMessage = LOCTEXT("RenameFolderAlreadyExists", "A folder already exists at this location with this name.");
		return false;
	}

	return true;
}

void SPathView::FolderNameChanged( const TSharedPtr< FTreeItem >& TreeItem, const FString& OldPath, const FVector2D& MessageLocation )
{
	// Verify the name of the folder
	FText Reason;
	if ( ContentBrowserUtils::IsValidFolderName(TreeItem->FolderName, Reason) )
	{
		TSharedPtr< FTreeItem > ExistingItem;
		if ( FolderAlreadyExists(TreeItem, ExistingItem) )
		{
			// The folder already exists, remove it so selection is simple
			RemoveFolderItem(ExistingItem);
		}
		
		// The folder did not already exist
		bool bWasItemSelected = TreeViewPtr->IsItemSelected(TreeItem);

		// Reselect the path to notify that selection has changed
		if ( bWasItemSelected )
		{
			FScopedPreventTreeItemChangedDelegate DelegatePrevention( SharedThis(this) );
			TreeViewPtr->SetItemSelection(TreeItem, false);
		}

		// If we weren't a root node, make sure our parent is sorted
		if ( TreeItem->Parent.IsValid() )
		{
			TreeItem->Parent.Pin()->SortChildren();
			TreeViewPtr->RequestTreeRefresh();
		}

		if ( bWasItemSelected )
		{
			// Set the selection again
			TreeViewPtr->SetItemSelection(TreeItem, true);

			// Scroll back into view if position has changed
			TreeViewPtr->RequestScrollIntoView(TreeItem);
		}

		// Update either the asset registry or the native class hierarchy so this folder will persist
		/*
		if (ContentBrowserUtils::IsClassPath(TreeItem->FolderPath))
		{
			// todo: jdale - CLASS - This will need updating to support renaming of class folders (SAssetView has similar logic - needs abstracting)
			TSharedRef<FNativeClassHierarchy> NativeClassHierarchy = FContentBrowserSingleton::Get().GetNativeClassHierarchy();
			NativeClassHierarchy->AddFolder(TreeItem->FolderPath);
		}
		else
		*/
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			if (AssetRegistryModule.Get().AddPath(TreeItem->FolderPath) && TreeItem->FolderPath != OldPath)
			{
				// move any assets in our folder
				TArray<FAssetData> AssetsInFolder;
				AssetRegistryModule.Get().GetAssetsByPath(*OldPath, AssetsInFolder, true);
				TArray<UObject*> ObjectsInFolder;
				ContentBrowserUtils::GetObjectsInAssetData(AssetsInFolder, ObjectsInFolder);
				ContentBrowserUtils::MoveAssets(ObjectsInFolder, TreeItem->FolderPath, OldPath);

				// Now check to see if the original folder is empty, if so we can delete it
				TArray<FAssetData> AssetsInOriginalFolder;
				AssetRegistryModule.Get().GetAssetsByPath(*OldPath, AssetsInOriginalFolder, true);
				if (AssetsInOriginalFolder.Num() == 0)
				{
					TArray<FString> FoldersToDelete;
					FoldersToDelete.Add(OldPath);
					ContentBrowserUtils::DeleteFolders(FoldersToDelete);
				}
			}
		}
	}
	else
	{
		// Remove the item
		RemoveFolderItem(TreeItem);

		// Display the reason why the folder was invalid
		FSlateRect MessageAnchor(MessageLocation.X, MessageLocation.Y, MessageLocation.X, MessageLocation.Y);
		ContentBrowserUtils::DisplayMessage(Reason, MessageAnchor, SharedThis(this));
	}
}



bool SPathView::FolderAlreadyExists(const TSharedPtr< FTreeItem >& TreeItem, TSharedPtr< FTreeItem >& ExistingItem)
{
	ExistingItem.Reset();

	if ( TreeItem.IsValid() )
	{
		if ( TreeItem->Parent.IsValid() )
		{
			// This item has a parent, try to find it in its parent's children
			TSharedPtr<FTreeItem> ParentItem = TreeItem->Parent.Pin();

			for ( auto ChildIt = ParentItem->Children.CreateConstIterator(); ChildIt; ++ChildIt )
			{
				const TSharedPtr<FTreeItem>& Child = *ChildIt;
				if ( Child != TreeItem && Child->FolderName == TreeItem->FolderName )
				{
					// The item is in its parent already
					ExistingItem = Child;
					break;
				}
			}
		}
		else
		{
			// This item is part of the root set
			for ( auto RootIt = TreeRootItems.CreateConstIterator(); RootIt; ++RootIt )
			{
				const TSharedPtr<FTreeItem>& Root = *RootIt;
				if ( Root != TreeItem && Root->FolderName == TreeItem->FolderName )
				{
					// The item is part of the root set already
					ExistingItem = Root;
					break;
				}
			}
		}
	}

	return ExistingItem.IsValid();
}

void SPathView::RemoveFolderItem(const TSharedPtr< FTreeItem >& TreeItem)
{
	if ( TreeItem.IsValid() )
	{
		if ( TreeItem->Parent.IsValid() )
		{
			// Remove this item from it's parent's list
			TreeItem->Parent.Pin()->Children.Remove(TreeItem);
		}
		else
		{
			// This was a root node, remove from the root list
			TreeRootItems.Remove(TreeItem);
		}

		TreeViewPtr->RequestTreeRefresh();
	}
}

void SPathView::TreeAssetsDropped(const TArray<FAssetData>& AssetList, const TSharedPtr<FTreeItem>& TreeItem)
{
	// Do not display the menu if any of the assets are classes as they cannot be moved or copied
	for( int32 AssetIndex = 0; AssetIndex < AssetList.Num(); AssetIndex++ )
	{
		const FAssetData& Asset = AssetList[AssetIndex];
		if ( Asset.AssetClass == "Class" )
		{
			const FText MessageText = LOCTEXT("AssetTreeDropClassError", "The selection contains one or more 'Class' type assets, these cannot be moved or copied.");
			FMessageDialog::Open(EAppMsgType::Ok, MessageText);
			return;
		}
	}

	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, NULL);
	const FText MoveCopyHeaderString = FText::Format( LOCTEXT("AssetTreeDropMenuHeading", "Move/Copy to {0}"), TreeItem->DisplayName );
	MenuBuilder.BeginSection("PathAssetMoveCopy", MoveCopyHeaderString);
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DragDropCopy", "Copy Here"),
			LOCTEXT("DragDropCopyTooltip", "Creates a copy of all dragged files in this folder."),
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateSP( this, &SPathView::ExecuteTreeDropCopy, AssetList, TreeItem ),
			FCanExecuteAction()
			)
			);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("DragDropMove", "Move Here"),
			LOCTEXT("DragDropMoveTooltip", "Moves all dragged files to this folder."),
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateSP( this, &SPathView::ExecuteTreeDropMove, AssetList, TreeItem ),
			FCanExecuteAction()
			)
			);
	}
	MenuBuilder.EndSection();

	TWeakPtr< SWindow > ContextMenuWindow = FSlateApplication::Get().PushMenu(
		SharedThis( this ),
		MenuBuilder.MakeWidget(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu )
		);
}

void SPathView::TreeFoldersDropped(const TArray<FString>& PathNames, const TSharedPtr<FTreeItem>& TreeItem)
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, NULL);
	const FText MoveCopyHeaderString = FText::Format( LOCTEXT("AssetTreeDropMenuHeading", "Move/Copy to {0}"), TreeItem->DisplayName );
	MenuBuilder.BeginSection("PathFolderMoveCopy", MoveCopyHeaderString);
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DragDropCopyFolder", "Copy Folder Here"),
			LOCTEXT("DragDropCopyFolderTooltip", "Creates a copy of all assets in the dragged folders to this folder, preserving folder structure."),
			FSlateIcon(),
			FUIAction( FExecuteAction::CreateSP( this, &SPathView::ExecuteTreeDropCopyFolder, PathNames, TreeItem ) )
			);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("DragDropMoveFolder", "Move Folder Here"),
			LOCTEXT("DragDropMoveFolderTooltip", "Moves all assets in the dragged folders to this folder, preserving folder structure."),
			FSlateIcon(),
			FUIAction( FExecuteAction::CreateSP( this, &SPathView::ExecuteTreeDropMoveFolder, PathNames, TreeItem ) )
			);
	}
	MenuBuilder.EndSection();

	TWeakPtr< SWindow > ContextMenuWindow = FSlateApplication::Get().PushMenu(
		SharedThis( this ),
		MenuBuilder.MakeWidget(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu )
		);
}

void SPathView::TreeFilesDropped(const TArray<FString>& FileNames, const TSharedPtr<FTreeItem>& TreeItem)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().ImportAssets( FileNames, TreeItem->FolderPath );
}

bool SPathView::IsTreeItemExpanded(TSharedPtr<FTreeItem> TreeItem) const
{
	return TreeViewPtr->IsItemExpanded(TreeItem);
}

bool SPathView::IsTreeItemSelected(TSharedPtr<FTreeItem> TreeItem) const
{
	return TreeViewPtr->IsItemSelected(TreeItem);
}

void SPathView::ExecuteTreeDropCopy(TArray<FAssetData> AssetList, TSharedPtr<FTreeItem> TreeItem)
{
	TArray<UObject*> DroppedObjects;
	ContentBrowserUtils::GetObjectsInAssetData(AssetList, DroppedObjects);

	ContentBrowserUtils::CopyAssets(DroppedObjects, TreeItem->FolderPath);
}

void SPathView::ExecuteTreeDropMove(TArray<FAssetData> AssetList, TSharedPtr<FTreeItem> TreeItem)
{
	TArray<UObject*> DroppedObjects;
	ContentBrowserUtils::GetObjectsInAssetData(AssetList, DroppedObjects);

	ContentBrowserUtils::MoveAssets(DroppedObjects, TreeItem->FolderPath);
}

void SPathView::ExecuteTreeDropCopyFolder(TArray<FString> PathNames, TSharedPtr<FTreeItem> TreeItem)
{
	ContentBrowserUtils::CopyFolders(PathNames, TreeItem->FolderPath);

	TreeViewPtr->SetItemExpansion(TreeItem, true);

	// Select all the new folders
	TreeViewPtr->ClearSelection();
	for ( auto PathIt = PathNames.CreateConstIterator(); PathIt; ++PathIt )
	{
		const FString SubFolderName = FPackageName::GetLongPackageAssetName(*PathIt);
		const FString NewPath = TreeItem->FolderPath + TEXT("/") + SubFolderName;
		
		TSharedPtr<FTreeItem> Item = FindItemRecursive(NewPath);
		if ( Item.IsValid() )
		{
			TreeViewPtr->SetItemSelection(Item, true);
			TreeViewPtr->RequestScrollIntoView(Item);
		}
	}
}

void SPathView::ExecuteTreeDropMoveFolder(TArray<FString> PathNames, TSharedPtr<FTreeItem> TreeItem)
{
	ContentBrowserUtils::MoveFolders(PathNames, TreeItem->FolderPath);

	TreeViewPtr->SetItemExpansion(TreeItem, true);
	
	// Select all the new folders
	TreeViewPtr->ClearSelection();
	for ( auto PathIt = PathNames.CreateConstIterator(); PathIt; ++PathIt )
	{
		const FString SubFolderName = FPackageName::GetLongPackageAssetName(*PathIt);
		const FString NewPath = TreeItem->FolderPath + TEXT("/") + SubFolderName;

		TSharedPtr<FTreeItem> Item = FindItemRecursive(NewPath);
		if ( Item.IsValid() )
		{
			TreeViewPtr->SetItemSelection(Item, true);
			TreeViewPtr->RequestScrollIntoView(Item);
		}
	}
}

void SPathView::OnAssetRegistryPathAdded(const FString& Path)
{
	// by sending the whole path we deliberately include any children
	// of successful hits in the filtered list. 
	if ( SearchBoxFolderFilter->PassesFilter( Path ) )
	{
		AddPath(Path);
	}
}

void SPathView::OnAssetRegistryPathRemoved(const FString& Path)
{
	// by sending the whole path we deliberately include any children
	// of successful hits in the filtered list. 
	if ( SearchBoxFolderFilter->PassesFilter( Path ) )
	{
		RemovePath(Path);
	}
}

void SPathView::OnAssetRegistrySearchCompleted()
{
	// If there were any more initial paths, they no longer exist so clear them now.
	PendingInitialPaths.Empty();
}

void SPathView::OnContentPathMountedOrDismounted( const FString& AssetPath, const FString& FilesystemPath )
{
	// A new content path has appeared, so we should refresh out root set of paths
	bNeedsRepopulate = true;
}

void SPathView::OnClassHierarchyUpdated()
{
	// The class hierarchy has changed in some way, so we need to refresh our set of paths
	bNeedsRepopulate = true;
	// @todo 4.7 MERGE: This change should NOT be merged back to main.  This code is relying on a feature that we did not merge to the 4.7 branch.
	// RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SPathView::TriggerRepopulate));
}

void SPathView::HandleSettingChanged(FName PropertyName)
{
	if ((PropertyName == "DisplayDevelopersFolder") ||
		(PropertyName == "DisplayEngineFolder") ||
		(PropertyName == "DisplayPluginFolders") ||
		(PropertyName == NAME_None))	// @todo: Needed if PostEditChange was called manually, for now
	{
		// If the dev or engine folder is no longer visible but we're inside it...
		const bool bDisplayDev = GetDefault<UContentBrowserSettings>()->GetDisplayDevelopersFolder();
		const bool bDisplayEngine = GetDefault<UContentBrowserSettings>()->GetDisplayEngineFolder();
		const bool bDisplayPlugins = GetDefault<UContentBrowserSettings>()->GetDisplayPluginFolders();
		if (!bDisplayDev || !bDisplayEngine || !bDisplayPlugins)
		{
			const FString OldSelectedPath = GetSelectedPath();
			if ((!bDisplayDev && ContentBrowserUtils::IsDevelopersFolder(OldSelectedPath)) || (!bDisplayEngine && ContentBrowserUtils::IsEngineFolder(OldSelectedPath)) || (!bDisplayPlugins && ContentBrowserUtils::IsPluginFolder(OldSelectedPath)))
			{
				// Set the folder back to the root, and refresh the contents
				TSharedPtr<FTreeItem> GameRoot = FindItemRecursive(TEXT("/Game"));
				if ( GameRoot.IsValid() )
				{
					TreeViewPtr->SetSelection(GameRoot);
				}
				else
				{
					TreeViewPtr->ClearSelection();
				}
			}
		}

		// Update our path view so that it can include/exclude the dev folder
		Populate();

		// If the dev or engine folder has become visible and we're inside it...
		if (bDisplayDev || bDisplayEngine || bDisplayPlugins)
		{
			const FString NewSelectedPath = GetSelectedPath();
			if ((bDisplayDev && ContentBrowserUtils::IsDevelopersFolder(NewSelectedPath)) || (bDisplayEngine && ContentBrowserUtils::IsEngineFolder(NewSelectedPath)) || (bDisplayPlugins && ContentBrowserUtils::IsPluginFolder(NewSelectedPath)))
			{
				// Refresh the contents
				OnPathSelected.ExecuteIfBound(NewSelectedPath);
			}
		}
	}
}


#undef LOCTEXT_NAMESPACE
