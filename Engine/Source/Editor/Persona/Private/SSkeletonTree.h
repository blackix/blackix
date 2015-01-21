// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Persona.h"
#include "Engine/SkeletalMeshSocket.h"

/** Enum which determines what type a tree row is. Value is 
	used as a flag for filtering tree items, so each goes up 
	to the next bit value*/
namespace ESkeletonTreeRowType
{
	enum Type
	{
		Bone=1,
		Socket=2,
		AttachedAsset=4
	};
};

/** Enum which tells us whether the parent of a socket is the skeleton or skeletal mesh */
namespace ESocketParentType
{
	enum Type
	{
		Skeleton,
		Mesh
	};
};

/** Enum which tells us what type of bones we should be showing */
namespace EBoneFilter
{
	enum Type
	{
		All,
		Mesh,
		Weighted,
		None,
		Count
	};
}

/** Enum which tells us what type of sockets we should be showing */
namespace ESocketFilter
{
	enum Type
	{
		Active,
		Mesh,
		Skeleton,
		All,
		None,
		Count
	};
}

struct FDisplayedTreeRowInfo;
typedef STreeView< TSharedPtr<FDisplayedTreeRowInfo> > SMeshSkeletonTreeRowType;
typedef TSharedPtr< FDisplayedTreeRowInfo > FDisplayedTreeRowInfoPtr;

//////////////////////////////////////////////////////////////////////////
// FDisplayedTreeRowInfo

struct FDisplayedTreeRowInfo : public TSharedFromThis<FDisplayedTreeRowInfo>
{
public:
	TArray< TSharedPtr<FDisplayedTreeRowInfo> > Children;

	// I don't like this, but our derived classes both hold completely different types of data
	// so void* seems the only sensible thing without RTTI (which isn't sensible either!)
	virtual void* GetData() = 0;
	virtual ESkeletonTreeRowType::Type GetType() const = 0;

	/** Builds the table row widget to display this info */
	virtual TSharedRef<ITableRow> MakeTreeRowWidget(
		const TSharedRef<STableViewBase>& InOwnerTable,
		FText InFilterText ) = 0;

	/** Builds the slate widget for the name column */
	virtual void GenerateWidgetForNameColumn( TSharedPtr< SHorizontalBox > Box, FText& FilterText, FIsSelected InIsSelected ) = 0;

	/** Builds the slate widget for the data column */
	virtual TSharedRef< SWidget > GenerateWidgetForDataColumn() = 0;

	/** Get the name of the item that this row represents */
	virtual FName GetRowItemName() const PURE_VIRTUAL(FDisplayedTreeRowInfo::GetRowItemName, return FName(););

	/** Return the name used to attach to this item */
	virtual FName GetAttachName() const { return GetRowItemName(); }

	/** Requests a rename on the the tree row item */
	virtual void RequestRename() {};

	/** Handler for when the user double clicks on this item in the tree */
	virtual void OnItemDoubleClicked() {}

protected:
	FDisplayedTreeRowInfo() {};

	/** Skeleton we're based on */
	USkeleton* TargetSkeleton;

	/** SkeletonTree that owns us */
	TWeakPtr< class SSkeletonTree > SkeletonTree;

	/** Persona that (also) owns us */
	TWeakPtr< FPersona > PersonaPtr;
};

//////////////////////////////////////////////////////////////////////////
// FDisplayedMeshBoneInfo

struct FDisplayedMeshBoneInfo : public FDisplayedTreeRowInfo
{
public:
	/** Static function for creating a new item, but ensures that you can only have a TSharedRef to one */
	static TSharedRef<FDisplayedMeshBoneInfo> Make(
		const FName& BoneName,
		USkeleton* InTargetSkeleton,
		TWeakPtr<FPersona> InPersona,
		TWeakPtr<SSkeletonTree> InSkeletonTree )
	{
		FDisplayedMeshBoneInfo* DisplayedMeshBoneInfo = new FDisplayedMeshBoneInfo(BoneName);
		DisplayedMeshBoneInfo->TargetSkeleton = InTargetSkeleton;
		DisplayedMeshBoneInfo->PersonaPtr = InPersona;
		DisplayedMeshBoneInfo->SkeletonTree = InSkeletonTree;

		return MakeShareable( DisplayedMeshBoneInfo );
	}

	// Manual RTTI - not particularly elegant! :-(
	virtual void* GetData() { return &BoneName; }
	virtual ESkeletonTreeRowType::Type GetType() const { return ESkeletonTreeRowType::Bone; }

	/** Builds the table row widget to display this info */
	virtual TSharedRef<ITableRow> MakeTreeRowWidget(
		const TSharedRef<STableViewBase>& InOwnerTable,
		FText InFilterText );

	/** Builds the slate widget for the name column */
	virtual void GenerateWidgetForNameColumn( TSharedPtr< SHorizontalBox > Box, FText& FilterText, FIsSelected InIsSelected );

	/** Builds the slate widget for the data column */
	virtual TSharedRef< SWidget > GenerateWidgetForDataColumn();

	/** Handle dragging a bone */
	FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Return the name of the bone */
	virtual FName GetRowItemName() const override {return BoneName;}

	/** Set Translation Retargeting Mode for this bone. */
	void SetBoneTranslationRetargetingMode(EBoneTranslationRetargetingMode::Type NewRetargetingMode);

	virtual ~FDisplayedMeshBoneInfo() {}

protected:
	/** Hidden constructor, always use Make above */
	FDisplayedMeshBoneInfo(const FName& InSource)
		: BoneName(InSource)
	{}

	/** Hidden constructor, always use Make above */
	FDisplayedMeshBoneInfo() {}

private:
	/** Gets the font for displaying bone text in the skeletal tree */
	FSlateFontInfo GetBoneTextFont( UDebugSkelMeshComponent* PreviewComponent ) const;

	/** Get the text color based on bone part of skeleton or part of mesh */
	FLinearColor GetBoneTextColor( UDebugSkelMeshComponent* PreviewComponent ) const;

	/** Function that returns the current tooltip for this bone, depending on how it's used by the mesh */
	FText GetBoneToolTip();

	/** The actual bone data that we create Slate widgets to display */
	FName BoneName;

	/** Create menu for Bone Translation Retargeting Mode. */
	TSharedRef< SWidget > CreateBoneTranslationRetargetingModeMenu();

	/** Get Title for Bone Translation Retargeting Mode menu. */
	FText GetTranslationRetargetingModeMenuTitle() const;
};

//////////////////////////////////////////////////////////////////////////
// FDisplayedSocketInfo

struct FDisplayedSocketInfo : public FDisplayedTreeRowInfo
{
public:
	/** Static function for creating a new item, but ensures that you can only have a TSharedRef to one */
	static TSharedRef<FDisplayedSocketInfo> Make(
		USkeletalMeshSocket* Source,
		ESocketParentType::Type InParentType,
		USkeleton* InTargetSkeleton,
		TWeakPtr<FPersona> InPersona,
		TWeakPtr<SSkeletonTree> InSkeletonTree,
		bool bIsCustomized )
	{
		FDisplayedSocketInfo* DisplayedSocketInfo = new FDisplayedSocketInfo( Source, InParentType );
		DisplayedSocketInfo->TargetSkeleton = InTargetSkeleton;
		DisplayedSocketInfo->PersonaPtr = InPersona;
		DisplayedSocketInfo->SkeletonTree = InSkeletonTree;
		DisplayedSocketInfo->bIsCustomized = bIsCustomized;

		return MakeShareable( DisplayedSocketInfo );
	}

	// Manual RTTI - not particularly elegant! :-(
	virtual void* GetData() { return SocketData; }
	virtual ESkeletonTreeRowType::Type GetType() const { return ESkeletonTreeRowType::Socket; }

	/** Builds the table row widget to display this info */
	virtual TSharedRef<ITableRow> MakeTreeRowWidget(
		const TSharedRef<STableViewBase>& InOwnerTable,
		FText InFilterText );

	/** Builds the slate widget for the name column */
	virtual void GenerateWidgetForNameColumn( TSharedPtr< SHorizontalBox > Box, FText& FilterText, FIsSelected InIsSelected );

	/** Builds the slate widget for the data column */
	virtual TSharedRef< SWidget > GenerateWidgetForDataColumn();

	ESocketParentType::Type GetParentType() const { return ParentType; }

	/** Handle dragging a socket */
	FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Return the name of the socket */
	virtual FName GetRowItemName() const override {return SocketData->SocketName;}

	/** Handle double clicking a socket */
	virtual void OnItemDoubleClicked();

	/** Is this socket customized */
	bool IsSocketCustomized() const { return bIsCustomized; }

	/** Delegate for when the context menu requests a rename */
	DECLARE_DELEGATE(FOnRenameRequested);
	FOnRenameRequested OnRenameRequested;

	/** Requests a rename on the socket item */
	virtual void RequestRename() override;

	/** Return socket name as FText for display in skeleton tree */
	FText GetSocketNameAsText() const { return FText::FromName(SocketData->SocketName); }

	virtual ~FDisplayedSocketInfo() {}

protected:
	/** Hidden constructor, always use Make above */
	FDisplayedSocketInfo( USkeletalMeshSocket* InSource, ESocketParentType::Type InParentType )
		: SocketData( InSource )
		, ParentType( InParentType )
	{}

	/** Hidden constructor, always use Make above */
	FDisplayedSocketInfo() {}

	/** Called when user is renaming a socket to verify the name **/
	bool OnVerifySocketNameChanged( const FText& InText, FText& OutErrorMessage );

	/** Called when user renames a socket **/
	void OnCommitSocketName( const FText& InText, ETextCommit::Type CommitInfo );

	/** Function that returns the current tooltip for this socket */
	FText GetSocketToolTip();

	/** Pointer to the socket */
	USkeletalMeshSocket*	SocketData;

	/** This enum tells us whether the socket is on the skeleton or the mesh */
	ESocketParentType::Type	ParentType;

	/** Box for the user to type in the name - stored here so that SSkeletonTree can set the keyboard focus in Slate */
	TSharedPtr<SEditableText>	NameEntryBox;

	/** True for sockets which exist on both the skeleton and mesh */
	bool bIsCustomized;
};

//////////////////////////////////////////////////////////////////////////
// FDisplayedAttachedAssetInfo

struct FDisplayedAttachedAssetInfo : public FDisplayedTreeRowInfo
{
public:
	/** Static function for creating a new item, but ensures that you can only have a TSharedRef to one */
	static TSharedRef<FDisplayedAttachedAssetInfo> Make(
		const FName& InAttachedTo,
		UObject* InAsset,
		USkeleton* InTargetSkeleton,
		TWeakPtr<FPersona> InPersona,
		TWeakPtr<SSkeletonTree> InSkeletonTree)
	{
		FDisplayedAttachedAssetInfo* Info = new FDisplayedAttachedAssetInfo( InAttachedTo, InAsset );
		Info->TargetSkeleton = InTargetSkeleton;
		Info->PersonaPtr = InPersona;
		Info->SkeletonTree = InSkeletonTree;

		Info->AssetComponent = InPersona.Pin()->GetComponentForAttachedObject(InAsset, InAttachedTo);

		return MakeShareable( Info );
	}

	// Manual RTTI - not particularly elegant! :-(
	virtual void* GetData() { return NULL; }
	virtual ESkeletonTreeRowType::Type GetType() const { return ESkeletonTreeRowType::AttachedAsset; }

	/** Builds the table row widget to display this info */
	virtual TSharedRef<ITableRow> MakeTreeRowWidget(
		const TSharedRef<STableViewBase>& InOwnerTable,
		FText InFilterText );

	/** Builds the slate widget for the name column */
	virtual void GenerateWidgetForNameColumn( TSharedPtr< SHorizontalBox > Box, FText& FilterText, FIsSelected InIsSelected );

	/** Builds the slate widget for the data column */
	virtual TSharedRef< SWidget > GenerateWidgetForDataColumn();

	/** Return the name of the asset */
	virtual FName GetRowItemName() const override {return FName( *Asset->GetName() ) ;}

	/** Return the name used to attach to this item (in assets case return the item we are attached to */
	virtual FName GetAttachName() const { return GetParentName(); }

	/** Returns the name of the socket/bone this asset is attached to */
	const FName& GetParentName() const { return AttachedTo; }

	/** Return the asset this info represents */
	UObject* GetAsset() const { return Asset; }

	/** Accessor for SCheckBox **/
	ECheckBoxState IsAssetDisplayed() const;

	/** Called when user toggles checkbox **/
	void OnToggleAssetDisplayed( ECheckBoxState InCheckboxState );

	/** Called when we need to get the state-based-image to show for the asset displayed checkbox */
	const FSlateBrush* OnGetAssetDisplayedButtonImage() const;

	/** Handler for when the user double clicks on this item in the tree */
	virtual void OnItemDoubleClicked();

	virtual ~FDisplayedAttachedAssetInfo() {}

protected:
	/** Hidden constructor, always use Make above */
	FDisplayedAttachedAssetInfo( const FName& InAttachedTo, UObject* InAsset ) :
		 AttachedTo(InAttachedTo), Asset(InAsset)
	{}

	/** The name of the socket/bone this asset is attached to */
	const FName AttachedTo;

	/** The attached asset that this tree item represents */
	UObject* Asset;

	/** The component of the attached asset */
	TWeakObjectPtr<USceneComponent> AssetComponent;
};
//////////////////////////////////////////////////////////////////////////
// SSkeletonTree

class SSkeletonTree : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SSkeletonTree )
		: _Persona()
		, _IsEditable(true)
		{}

		SLATE_ARGUMENT( TWeakPtr<FPersona>, Persona )
		SLATE_ATTRIBUTE( bool, IsEditable )
	SLATE_END_ARGS()
public:
	void Construct(const FArguments& InArgs);

	virtual ~SSkeletonTree();

	/** Creates the tree control and then populates using CreateFromSkeleton */
	void CreateTreeColumns();

	/** Function to build the skeleton tree widgets from the source skeleton tree */
	void CreateFromSkeleton( const TArray<FBoneNode>& SourceSkeleton, USkeletalMeshSocket* SocketToRename = NULL );

	/** This triggers a rebuild of the tree after undo to make the UI consistent with the real data */
	void PostUndo();

	/** Utility function to print notifications to the user */
	void NotifyUser( FNotificationInfo& NotificationInfo );

	/** Handle dropping something onto a skeleton bone tree item */
	FReply OnDropAssetToSkeletonTree(const FDisplayedTreeRowInfoPtr TargetItem, const FDragDropEvent& DragDropEvent);

	/** Attached the supplied assets to the tree to the specified attach item (bone/socket) */
	void AttachAssetsToSkeletonTree(const FDisplayedTreeRowInfoPtr TargetItem, const TArray<FAssetData>& AssetData);

	/** Returns true if a bone has vertices weighted against it */
	bool IsBoneWeighted( int32 MeshBoneIndex, UDebugSkelMeshComponent* PreviewComponent ) const;

	/** Called when the preview mesh is changed - simply rebuilds the skeleton tree for the new mesh */
	void OnPreviewMeshChanged(class USkeletalMesh* NewPreviewMesh);

	/** Callback when an item is scrolled into view, handling calls to rename items */
	void OnItemScrolledIntoView( FDisplayedTreeRowInfoPtr InItem, const TSharedPtr<ITableRow>& InWidget);

	/** Callback for when the user double-clicks on an item in the tree */
	void OnTreeDoubleClick( FDisplayedTreeRowInfoPtr InItem );

	/** Handle recursive expansion/contraction of the tree */
	void SetTreeItemExpansionRecursive(TSharedPtr< FDisplayedTreeRowInfo > TreeItem, bool bInExpansionState) const;

	/** Called when a socket has been renamed */
	void RenameSocketAttachments(FName& OldSocketName, FName& NewSocketName);

	/** Set Bone Translation Retargeting Mode for bone selection, and their children. */
	void SetBoneTranslationRetargetingModeRecursive(EBoneTranslationRetargetingMode::Type NewRetargetingMode);

	/** Remove the selected bones from LOD of LODIndex when using simplygon **/
	void RemoveFromLOD(int32 LODIndex);
	/** Add the selected bones to LOD of LODIndex when using simplygon **/
	void AddToLOD(int32 LODIndex);

private:
	/** Binds the commands in FSkeletonTreeCommands to functions in this class */
	void BindCommands();

	/** Create a widget for an entry in the tree from an info */
	TSharedRef<ITableRow> MakeTreeRowWidget(TSharedPtr<FDisplayedTreeRowInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable);

	/** Get all children for a given entry in the list */
	void GetChildrenForInfo(TSharedPtr<FDisplayedTreeRowInfo> InInfo, TArray< TSharedPtr<FDisplayedTreeRowInfo> >& OutChildren);

	/** Called to display context menu when right clicking on the widget */
	TSharedPtr< SWidget > CreateContextMenu();

	/** Called to display the bone filter menu */
	TSharedRef< SWidget > CreateBoneFilterMenu();

	/** Called to display the socket filter menu */
	TSharedRef< SWidget > CreateSocketFilterMenu();

	/** Function to copy selected bone name to the clipboard */
	void OnCopyBoneNames();

	/** Function to reset the transforms of selected bones */
	void OnResetBoneTransforms();

	/** Function to copy selected sockets to the clipboard */
	void OnCopySockets() const;

	/** Function to serialize a single socket to a string */
	FString SerializeSocketToString( USkeletalMeshSocket* Socket, const FDisplayedSocketInfo* DisplayedSocketInfo ) const;

	/** Function to paste selected sockets from the clipboard */
	void OnPasteSockets();

	/** Function to add a socket to the selected bone (skeleton, not mesh) */
	void OnAddSocket();

	/** Function to check if it is possible to rename the selected item */
	bool CanRenameSelected() const;

	/** Function to start renaming a socket */
	void OnRenameSocket();

	/** Function to customize a socket - this essentially copies a socket from the skeleton to the mesh */
	void OnCustomizeSocket();

	/** Function to promote a socket - this essentially copies a socket from the mesh to the skeleton */
	void OnPromoteSocket();

	/** Create content picker sub menu to allow users to pick an asset to attach */
	void FillAttachAssetSubmenu(FMenuBuilder& MenuBuilder, const FDisplayedTreeRowInfoPtr TargetItem);

	/** Helper function for asset picker that handles users choice */
	void OnAssetSelectedFromPicker(const FAssetData& AssetData, const FDisplayedTreeRowInfoPtr TargetItem);

	/** Context menu function to remove all attached assets */
	void OnRemoveAllAssets();

	/** Context menu function to control enabled/disabled status of remove all assets menu item */
	bool CanRemoveAllAssets() const;

	/** Functions to copy sockets from the skeleton to the mesh */
	void OnCopySocketToMesh() {};

	/** Callback function to be called when selection changes in the tree view widget. */
	void OnSelectionChanged(TSharedPtr<FDisplayedTreeRowInfo> Selection, ESelectInfo::Type SelectInfo);

	/** Filters the SListView when the user changes the search text box (NameFilterBox)	*/
	void OnFilterTextChanged( const FText& SearchText );

	/** Notifies SSkeletonTree that something else (i.e. the socket hit points in the preview) has selected a socket */
	void OnExternalSelectSocket( const struct FSelectedSocketInfo& SocketInfo );

	/** Notifies SSkeletonTree that something else (i.e. the socket hit points in the preview) has selected a bone */
	void OnExternalSelectBone( const FName& BoneName );

	/** Called when the user single clicks in the viewport, deselecting everything */
	void OnExternalDeselectAll();

	/** Attach the given item to its parent */
	bool AttachToParent( TSharedRef<FDisplayedTreeRowInfo> ItemToAttach, FName ParentName, int32 ItemsToInclude);

	/** Add sockets from a TArray - separate function to avoid duplicating for skeleton and mesh */
	void AddSocketsFromData(
		const TArray< USkeletalMeshSocket* >& SocketArray,
		ESocketParentType::Type ParentType,
		USkeletalMeshSocket* SocketToRename );

	/** Sets which types of bone to show */
	void SetBoneFilter( EBoneFilter::Type InBoneFilter );

	/** Queries the bone filter */
	bool IsBoneFilter( EBoneFilter::Type InBoneFilter ) const;

	/** Sets which types of socket to show */
	void SetSocketFilter( ESocketFilter::Type InSocketFilter );

	/** Queries the bone filter */
	bool IsSocketFilter( ESocketFilter::Type InSocketFilter ) const;

	/** Returns the current text for the bone filter button - "All", "Mesh" or "Weighted" */
	FText GetBoneFilterMenuTitle() const;

	/** Returns the current text for the socket filter button - "All", "Mesh" or "Skeleton" */
	FText GetSocketFilterMenuTitle() const;

	/** We can only add sockets in Active, Skeleton or All mode (otherwise they just disappear) */
	bool IsAddingSocketsAllowed() const;

	/** Handler for "Show Retargeting Options" check box IsChecked functionality */
	ECheckBoxState IsShowingRetargetingOptions() const;

	/**  Handler for when we change the "Show Retargeting Options" check box */
	void OnChangeShowingRetargetingOptions(ECheckBoxState NewState);

	/** This replicates the socket filter to the previewcomponent so that the viewport can use the same settings */
	void SetPreviewComponentSocketFilter() const;

	/** Override OnKeyDown */
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent );

	/** Function to delete all the selected sockets/assets */
	void OnDeleteSelectedRows();

	/** Function to remove attached assets from the skeleton/mesh */
	void DeleteAttachedAssets( TArray<TSharedPtr<FDisplayedAttachedAssetInfo>> InDisplayedAttachedAssetInfos );

	/** Function to remove sockets from the skeleton/mesh */
	void DeleteSockets( TArray<TSharedPtr<FDisplayedSocketInfo>> InDisplayedSocketInfos );

	/** Add attached assets from a given TArray of them */
	void AddAttachedAssets( const FPreviewAssetAttachContainer& AttachedObjects );

	/** Deletes a set of attached objects from a FPreviewAssetAttachContainer and notifies Persona*/
	void DeleteAttachedObjects( FPreviewAssetAttachContainer& AttachedAssets );

private:
	/** Pointer back to the kismet 2 tool that owns us */
	TWeakPtr<FPersona> PersonaPtr;

	/** SSearchBox to filter the tree */
	TSharedPtr<SSearchBox>	NameFilterBox;

	USkeleton* TargetSkeleton;

	/** Widget user to hold the skeleton tree */
	TSharedPtr<SOverlay> TreeHolder;

	/** Widget used to display the skeleton hierarchy */
	TSharedPtr<SMeshSkeletonTreeRowType> SkeletonTreeView;

	/** A tree of bone info. Used by the BoneTreeView. */
	TArray< TSharedPtr<FDisplayedTreeRowInfo> > SkeletonRowList;

	/** A "mirror" of the tree as a flat array for easier searching */
	TArray< TSharedRef<FDisplayedTreeRowInfo> > DisplayMirror;

	/** Is this view editable */
	TAttribute<bool> IsEditable;

	/** Current text typed into NameFilterBox */
	FText FilterText;

	/** Commands that are bound to delegates*/
	TSharedPtr<FUICommandList> UICommandList;

	/** Current type of bones to show */
	EBoneFilter::Type BoneFilter;

	/** Current type of sockets to show */
	ESocketFilter::Type SocketFilter;

	bool bShowingRetargetingOptions;

	/** Points to an item that is being requested to be renamed */
	TSharedPtr<FDisplayedTreeRowInfo> DeferredRenameRequest;

	/** String used as a header for text based copy-paste of sockets */
	static const FString SocketCopyPasteHeader;
}; 
