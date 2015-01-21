// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"
#include "SSkeletonTree.h"

#include "ScopedTransaction.h"
#include "BoneDragDropOp.h"
#include "SocketDragDropOp.h"
#include "SkeletonTreeCommands.h"
#include "DragAndDrop/AssetDragDropOp.h"

#include "SAnimationEditorViewport.h"
#include "AnimationEditorViewportClient.h"

#include "AssetSelection.h"

#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"
#include "ComponentAssetBroker.h"

#include "ClassIconFinder.h"

#include "Editor/UnrealEd/Public/AssetNotifications.h"

#include "Animation/PreviewAssetAttachComponent.h"
#include "AnimPreviewInstance.h"

#include "Factories.h"
#include "Developer/MeshUtilities/Public/MeshUtilities.h"
#include "UnrealExporter.h"
#include "SSearchBox.h"
#include "SInlineEditableTextBlock.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "GenericCommands.h"

#define LOCTEXT_NAMESPACE "SSkeletonTree"

static const FName	ColumnID_BoneLabel( "BoneName" );
static const FName	ColumnID_RetargetingLabel( "TranslationRetargeting" );
// see if mesh reduction is supported
static bool	bMeshReductionSupported = false;

DECLARE_DELEGATE_RetVal_TwoParams(FReply, FOnDraggingBoneItem, const FGeometry&, const FPointerEvent&);

//////////////////////////////////////////////////////////////////////////
// FSocketTextObjectFactory - constructs sockets from clipboard data

class FSocketTextObjectFactory : public FCustomizableTextObjectFactory
{
public:
	FSocketTextObjectFactory( USkeletalMeshSocket** InDestinationSocket )
	: FCustomizableTextObjectFactory( GWarn )
	, DestinationSocket( InDestinationSocket )
	{
		
	};

private:
	virtual bool CanCreateClass(UClass* ObjectClass) const { return true; }

	virtual void ProcessConstructedObject( UObject* CreatedObject )
	{
		*DestinationSocket = (USkeletalMeshSocket*)CreatedObject;
	}

	// Pointer back to the outside world that will hold the final imported socket
	USkeletalMeshSocket** DestinationSocket;
};

//////////////////////////////////////////////////////////////////////////
// SSkeletonTreeRow

DECLARE_DELEGATE_RetVal_TwoParams(FReply, FOnDraggingTreeItem, const FGeometry&, const FPointerEvent&);

typedef TSharedPtr< FDisplayedTreeRowInfo > FDisplayedTreeRowInfoPtr;

class SSkeletonTreeRow
	: public SMultiColumnTableRow< FDisplayedTreeRowInfoPtr >
{
public:

	SLATE_BEGIN_ARGS( SSkeletonTreeRow ){}

	/** The item for this row **/
	SLATE_ARGUMENT( FDisplayedTreeRowInfoPtr, Item )

		/** Pointer to the Skeleton so we can mark it dirty */
		SLATE_ARGUMENT( USkeleton*, TargetSkeleton );

		/** Pointer to the parent SSkeletonTree so we can request a tree refresh when needed */
		SLATE_ARGUMENT( TWeakPtr< SSkeletonTree >, SkeletonTree );

		/** Pointer to the owning Persona so it can be used to copy sockets/etc */
		SLATE_ARGUMENT( TWeakPtr< FPersona >, PersonaPtr );

		/** Filter text typed by the user into the parent tree's search widget */
		SLATE_ARGUMENT( FText, FilterText );

		/** Delegate for dragging items **/
		SLATE_EVENT( FOnDraggingTreeItem, OnDraggingItem );

	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView );

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the tree row. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override;

	/** Override OnDragEnter for drag and drop of sockets onto bones */
	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;

	/** Override OnDragLeave for drag and drop of sockets onto bones */
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;

	/** Override OnDrop for drag and drop of sockets and meshes onto bones */
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;

protected:

	/** Handler for starting a drag/drop action */
	virtual FReply OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

private:

	/** The item this row is holding */
	FDisplayedTreeRowInfoPtr	Item;

	/** The skeleton the bone is part of */
	USkeleton*					TargetSkeleton;

	/** Text the user typed into the search box - used for text highlighting */
	FText						FilterText;

	/** Weak pointer to the parent skeleton tree */
	TWeakPtr<SSkeletonTree>		SkeletonTree;

	/** Weak pointer to the owning Persona */
	TWeakPtr<FPersona>			PersonaPtr;

	/** Item that we're dragging */
	FOnDraggingTreeItem			OnDraggingItem;

	/** Was the user pressing "Alt" when the drag was started? */
	bool						bIsAltDrag;
};


void SSkeletonTreeRow::Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
{
	Item = InArgs._Item;
	OnDraggingItem = InArgs._OnDraggingItem;
	TargetSkeleton = InArgs._TargetSkeleton;
	FilterText = InArgs._FilterText;
	SkeletonTree = InArgs._SkeletonTree;
	PersonaPtr = InArgs._PersonaPtr;

	check( Item.IsValid() );

	SMultiColumnTableRow< FDisplayedTreeRowInfoPtr >::Construct( FSuperRowType::FArguments(), InOwnerTableView );
}

TSharedRef< SWidget > SSkeletonTreeRow::GenerateWidgetForColumn( const FName& ColumnName )
{
	if ( ColumnName == ColumnID_BoneLabel )
	{
		TSharedPtr< SHorizontalBox > Box;

		SAssignNew( Box, SHorizontalBox );

		Box->AddSlot()
			.AutoWidth()
			[
				SNew( SExpanderArrow, SharedThis(this) )
			];

		Item->GenerateWidgetForNameColumn( Box, FilterText, FIsSelected::CreateSP(this, &STableRow::IsSelectedExclusively ) );

		return Box.ToSharedRef();
	}
	else
	{
		return Item->GenerateWidgetForDataColumn();
	}
}

void SSkeletonTreeRow::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FSocketDragDropOp> DragConnectionOp = DragDropEvent.GetOperationAs<FSocketDragDropOp>();

	// Is someone dragging a socket onto a bone?
	if (DragConnectionOp.IsValid())
	{
		if ( Item->GetType() == ESkeletonTreeRowType::Bone &&
			( *static_cast<FName*>( Item->GetData() ) != DragConnectionOp->GetSocketInfo().Socket->BoneName ) )
		{
			// The socket can be dropped here if we're a bone and NOT the socket's existing parent
			DragConnectionOp->SetIcon( FEditorStyle::GetBrush( TEXT( "Graph.ConnectorFeedback.Ok" ) ) );
		}
		else if ( Item->GetType() == ESkeletonTreeRowType::Bone && DragConnectionOp->IsAltDrag() )
		{
			// For Alt-Drag, dropping onto the existing parent is fine, as we're going to copy, not move the socket
			DragConnectionOp->SetIcon( FEditorStyle::GetBrush( TEXT( "Graph.ConnectorFeedback.Ok" ) ) );
		}
		else
		{
			DragConnectionOp->SetIcon( FEditorStyle::GetBrush( TEXT( "Graph.ConnectorFeedback.Error" ) ) );
		}
	}
}

void SSkeletonTreeRow::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FSocketDragDropOp> DragConnectionOp = DragDropEvent.GetOperationAs<FSocketDragDropOp>();
	if (DragConnectionOp.IsValid())
	{
		// Reset the drag/drop icon when leaving this row
		DragConnectionOp->SetIcon( FEditorStyle::GetBrush( TEXT( "Graph.ConnectorFeedback.Error" ) ) );
	}
}

FReply SSkeletonTreeRow::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FSocketDragDropOp> DragConnectionOp = DragDropEvent.GetOperationAs<FSocketDragDropOp>();
	if (DragConnectionOp.IsValid())
	{
		FSelectedSocketInfo SocketInfo = DragConnectionOp->GetSocketInfo();

		if ( DragConnectionOp->IsAltDrag() && Item->GetType() == ESkeletonTreeRowType::Bone )
		{
			// In an alt-drag, the socket can be dropped on any bone
			// (including its existing parent) to create a uniquely named copy
			if ( PersonaPtr.IsValid() )
			{
				PersonaPtr.Pin()->DuplicateAndSelectSocket( SocketInfo, *static_cast<FName*>( Item->GetData() ) );
			}
		}
		else if ( Item->GetType() == ESkeletonTreeRowType::Bone &&
			*static_cast<FName*>( Item->GetData() ) != SocketInfo.Socket->BoneName )
		{
			// The socket can be dropped here if we're a bone and NOT the socket's existing parent

			// Create an undo transaction, re-parent the socket and rebuild the skeleton tree view
			const FScopedTransaction Transaction( LOCTEXT( "ReparentSocket", "Re-parent Socket" ) );

			SocketInfo.Socket->SetFlags( RF_Transactional );	// Undo doesn't work without this!
			SocketInfo.Socket->Modify();

			SocketInfo.Socket->BoneName = *static_cast<FName*>( Item->GetData() );

			SkeletonTree.Pin()->CreateFromSkeleton( TargetSkeleton->GetBoneTree() );

			return FReply::Handled();
		}
	}
	else
	{
		if (DragDropEvent.GetOperationAs<FAssetDragDropOp>().IsValid())
		{
			SkeletonTree.Pin()->OnDropAssetToSkeletonTree( Item, DragDropEvent );
		}
	}

	return FReply::Unhandled();
}

FReply SSkeletonTreeRow::OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( OnDraggingItem.IsBound() )
	{
		return OnDraggingItem.Execute( MyGeometry, MouseEvent );
	}
	else
	{
		return FReply::Unhandled();
	}
}

//////////////////////////////////////////////////////////////////////////
// FDisplayedMeshBoneInfo

TSharedRef<ITableRow> FDisplayedMeshBoneInfo::MakeTreeRowWidget(
	const TSharedRef<STableViewBase>& OwnerTable,
	FText FilterText )
{
	return
		SNew( SSkeletonTreeRow, OwnerTable )
		.Item( SharedThis(this) )
		.TargetSkeleton( TargetSkeleton )
		.FilterText( FilterText )
		.SkeletonTree( SkeletonTree )
		.PersonaPtr( PersonaPtr )
		.OnDraggingItem( this, &FDisplayedMeshBoneInfo::OnDragDetected );
}

void FDisplayedMeshBoneInfo::GenerateWidgetForNameColumn( TSharedPtr< SHorizontalBox > Box, FText& FilterText, FIsSelected InIsSelected )
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->GetPreviewMeshComponent();

	const FSlateFontInfo TextFont = GetBoneTextFont( PreviewComponent );
	const FLinearColor TextColor = GetBoneTextColor( PreviewComponent );

	FText ToolTip = GetBoneToolTip();

	Box->AddSlot()
		.AutoWidth()
		[
			SNew( STextBlock )
			.ColorAndOpacity(TextColor)
			.Text( FText::FromName(BoneName) )
			.HighlightText( FilterText )
			.Font(TextFont)
			.ToolTipText( ToolTip )
		];
}

TSharedRef< SWidget > FDisplayedMeshBoneInfo::GenerateWidgetForDataColumn()
{
	return SNew( SComboButton )
		.ContentPadding(3)
		.OnGetMenuContent( this, &FDisplayedMeshBoneInfo::CreateBoneTranslationRetargetingModeMenu )
		.ToolTip(IDocumentation::Get()->CreateToolTip(
		LOCTEXT("RetargetingToolTip", "Set bone translation retargeting mode"),
		NULL,
		TEXT("Shared/Editors/Persona"),
		TEXT("TranslationRetargeting")))
		.ButtonContent()
		[
			SNew( STextBlock )
			.Text( this, &FDisplayedMeshBoneInfo::GetTranslationRetargetingModeMenuTitle )
		];
}

TSharedRef< SWidget > FDisplayedMeshBoneInfo::CreateBoneTranslationRetargetingModeMenu()
{
	FMenuBuilder MenuBuilder(true, NULL);

	MenuBuilder.BeginSection("BoneTranslationRetargetingMode", LOCTEXT( "BoneTranslationRetargetingModeMenuHeading", "Bone Translation Retargeting Mode" ) );
	{
		FUIAction ActionRetargetingAnimation = FUIAction(FExecuteAction::CreateSP(this, &FDisplayedMeshBoneInfo::SetBoneTranslationRetargetingMode, EBoneTranslationRetargetingMode::Animation));
		MenuBuilder.AddMenuEntry( FText::FromString( TargetSkeleton->GetRetargetingModeString(EBoneTranslationRetargetingMode::Animation) ), LOCTEXT( "BoneTranslationRetargetingAnimationToolTip", "Use translation from animation." ), FSlateIcon(), ActionRetargetingAnimation);

		FUIAction ActionRetargetingSkeleton = FUIAction(FExecuteAction::CreateSP(this, &FDisplayedMeshBoneInfo::SetBoneTranslationRetargetingMode, EBoneTranslationRetargetingMode::Skeleton));
		MenuBuilder.AddMenuEntry( FText::FromString( TargetSkeleton->GetRetargetingModeString(EBoneTranslationRetargetingMode::Skeleton) ), LOCTEXT( "BoneTranslationRetargetingSkeletonToolTip", "Use translation from Skeleton." ), FSlateIcon(), ActionRetargetingSkeleton);

		FUIAction ActionRetargetingLengthScale = FUIAction(FExecuteAction::CreateSP(this, &FDisplayedMeshBoneInfo::SetBoneTranslationRetargetingMode, EBoneTranslationRetargetingMode::AnimationScaled));
		MenuBuilder.AddMenuEntry( FText::FromString( TargetSkeleton->GetRetargetingModeString(EBoneTranslationRetargetingMode::AnimationScaled) ), LOCTEXT( "BoneTranslationRetargetingAnimationScaledToolTip", "Use translation from animation, scale length by Skeleton's proportions." ), FSlateIcon(), ActionRetargetingLengthScale);

		FUIAction ActionRetargetingAnimationRelative = FUIAction(FExecuteAction::CreateSP(this, &FDisplayedMeshBoneInfo::SetBoneTranslationRetargetingMode, EBoneTranslationRetargetingMode::AnimationRelative));
		MenuBuilder.AddMenuEntry(FText::FromString(TargetSkeleton->GetRetargetingModeString(EBoneTranslationRetargetingMode::AnimationRelative)), LOCTEXT("BoneTranslationRetargetingAnimationRelativeToolTip", "Use relative translation from animation similar to an additive animation."), FSlateIcon(), ActionRetargetingAnimationRelative);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

FText FDisplayedMeshBoneInfo::GetTranslationRetargetingModeMenuTitle() const
{
	const int32 BoneIndex = TargetSkeleton->GetReferenceSkeleton().FindBoneIndex( BoneName );
	if( BoneIndex != INDEX_NONE )
	{
		const EBoneTranslationRetargetingMode::Type RetargetingMode = TargetSkeleton->GetBoneTranslationRetargetingMode(BoneIndex);
		return FText::FromString(TargetSkeleton->GetRetargetingModeString(RetargetingMode));
	}

	return LOCTEXT("None", "None");
}

void FDisplayedMeshBoneInfo::SetBoneTranslationRetargetingMode(EBoneTranslationRetargetingMode::Type NewRetargetingMode)
{
	const FScopedTransaction Transaction( LOCTEXT( "SetBoneTranslationRetargetingMode", "Set Bone Translation Retargeting Mode" ) );
	TargetSkeleton->Modify();

	const int32 BoneIndex = TargetSkeleton->GetReferenceSkeleton().FindBoneIndex( BoneName );
	TargetSkeleton->SetBoneTranslationRetargetingMode(BoneIndex, NewRetargetingMode);
	FAssetNotifications::SkeletonNeedsToBeSaved(TargetSkeleton);
}

FSlateFontInfo FDisplayedMeshBoneInfo::GetBoneTextFont( UDebugSkelMeshComponent* PreviewComponent ) const
{
	if ( PreviewComponent )
	{
		int32 BoneIndex = PreviewComponent->GetBoneIndex(BoneName);

		if( BoneIndex != INDEX_NONE )
		{
			if( SkeletonTree.Pin()->IsBoneWeighted( BoneIndex, PreviewComponent ) )
			{
				//Bone is vertex weighted
				FSlateFontInfo BoldFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 10 );
				return BoldFont;
			}
		}
	}

	//Bone is not vertex weighted
	FSlateFontInfo RegularFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 10 );
	return RegularFont;
}

FLinearColor FDisplayedMeshBoneInfo::GetBoneTextColor( UDebugSkelMeshComponent* PreviewComponent ) const
{
	if ( PreviewComponent )
	{
		//Check whether this bone in skeleton bone tree is present in skeletal mesh's reference bone list.
		int32 BoneIndex = PreviewComponent->GetBoneIndex(BoneName);
		if( BoneIndex != INDEX_NONE )
		{
			return FLinearColor::White;
		}
	}

	return FLinearColor::Gray;
}

FReply FDisplayedMeshBoneInfo::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ) )
	{
		return FReply::Handled().BeginDragDrop( FBoneDragDropOp::New( TargetSkeleton, BoneName ) );
	}

	return FReply::Unhandled();
}

FText FDisplayedMeshBoneInfo::GetBoneToolTip()
{
	bool bIsMeshBone = false;
	bool bIsWeightedBone = false;
	bool bMeshExists = false;

	FText ToolTip;

	if ( PersonaPtr.IsValid() )
	{
		UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->GetPreviewMeshComponent();

		if ( PreviewComponent )
		{
			bMeshExists = true;

			int32 BoneIndex = PreviewComponent->GetBoneIndex( BoneName );

			if( BoneIndex != INDEX_NONE )
			{
				bIsMeshBone = true;

				bIsWeightedBone = SkeletonTree.Pin()->IsBoneWeighted( BoneIndex, PreviewComponent );
			}
		}
	}

	if ( !bMeshExists )
	{
		ToolTip = LOCTEXT( "BoneToolTipNoMeshAvailable", "This bone exists only on the skeleton as there is no current mesh set" );
	}
	else
	{
		if ( !bIsMeshBone )
		{
			ToolTip = LOCTEXT( "BoneToolTipSkeletonOnly", "This bone exists only on the skeleton, but not on the current mesh" );
		}
		else
		{
			if ( !bIsWeightedBone )
			{
				ToolTip = LOCTEXT( "BoneToolTipSkeletonAndMesh", "This bone is used by the current mesh, but has no vertices weighted against it" );
			}
			else
			{
				ToolTip = LOCTEXT( "BoneToolTipWeighted", "This bone has vertices weighted against it" );
			}
		}
	}

	return ToolTip;
}

//////////////////////////////////////////////////////////////////////////
// FDisplayedSocketInfo

TSharedRef<ITableRow> FDisplayedSocketInfo::MakeTreeRowWidget(
	const TSharedRef<STableViewBase>& InOwnerTable,
	FText InFilterText )
{
	return
		SNew( SSkeletonTreeRow, InOwnerTable )
		.Item( SharedThis(this) )
		.FilterText( InFilterText )
		.SkeletonTree( SkeletonTree )
		.TargetSkeleton( TargetSkeleton )
		.OnDraggingItem( this, &FDisplayedSocketInfo::OnDragDetected );
}

void FDisplayedSocketInfo::GenerateWidgetForNameColumn( TSharedPtr< SHorizontalBox > Box, FText& FilterText, FIsSelected InIsSelected )
{
	const FSlateBrush* SocketIcon = ( ParentType == ESocketParentType::Mesh ) ?
		FEditorStyle::GetBrush( "SkeletonTree.MeshSocket" ) :
		FEditorStyle::GetBrush( "SkeletonTree.SkeletonSocket" );

	Box->AddSlot()
	.AutoWidth()
	[
		SNew( SImage )
		.Image( SocketIcon )
	];

	const FSlateFontInfo TextFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf" ), 10 );

	FLinearColor TextColor;

	if ( ParentType == ESocketParentType::Skeleton && bIsCustomized )
	{
		TextColor = FLinearColor::Gray;
	}
	else
	{
		TextColor = FLinearColor::White;
	}

	FText ToolTip = GetSocketToolTip();

	TAttribute<FText> SocketNameAttr = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &FDisplayedSocketInfo::GetSocketNameAsText));
	TSharedPtr< SInlineEditableTextBlock > InlineWidget;

	Box->AddSlot()
	.AutoWidth()
	[
		SAssignNew( InlineWidget, SInlineEditableTextBlock )
			.ColorAndOpacity( TextColor )
			.Text( SocketNameAttr )
			.HighlightText( FilterText )
			.Font( TextFont )
			.ToolTipText( ToolTip )
			.OnVerifyTextChanged( this, &FDisplayedSocketInfo::OnVerifySocketNameChanged )
			.OnTextCommitted( this, &FDisplayedSocketInfo::OnCommitSocketName )
			.IsSelected( InIsSelected )
	];

	OnRenameRequested.BindSP( InlineWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode);

	if ( ParentType == ESocketParentType::Mesh )
	{
		FText SocketSuffix = IsSocketCustomized() ?
			LOCTEXT( "CustomizedSuffix", " [Mesh]" ) :
			LOCTEXT( "MeshSuffix", " [Mesh Only]" );

		Box->AddSlot()
		.AutoWidth()
		[
			SNew( STextBlock )
			.ColorAndOpacity( FLinearColor::Gray )
			.Text( SocketSuffix )
			.Font(TextFont)
			.ToolTipText( ToolTip )
		];
	}
}

TSharedRef< SWidget > FDisplayedSocketInfo::GenerateWidgetForDataColumn()
{
	return SNullWidget::NullWidget;
}

void FDisplayedSocketInfo::OnItemDoubleClicked()
{
	OnRenameRequested.ExecuteIfBound();
}

void FDisplayedSocketInfo::RequestRename()
{
	OnRenameRequested.ExecuteIfBound();
}

bool FDisplayedSocketInfo::OnVerifySocketNameChanged( const FText& InText, FText& OutErrorMessage )
{
	// You can't have two sockets with the same name on the mesh, nor on the skeleton,
	// but you can have a socket with the same name on the mesh *and* the skeleton.
	bool bVerifyName = true;

	FText NewText = FText::TrimPrecedingAndTrailing(InText);

	if (NewText.IsEmpty())
	{
		OutErrorMessage = LOCTEXT( "EmptySocketName_Error", "Sockets must have a name!");
		bVerifyName = false;
	}
	else if ( PersonaPtr.IsValid() && SkeletonTree.IsValid() )
	{
		if ( ParentType == ESocketParentType::Mesh )
		{
			// If we're on the mesh, check the mesh for duplicates...
			USkeletalMesh* Mesh = PersonaPtr.Pin()->GetMesh();

			if ( Mesh )
			{
				bVerifyName = !PersonaPtr.Pin()->DoesSocketAlreadyExist( SocketData, NewText, Mesh->GetMeshOnlySocketList() );
			}
		}
		else
		{
			// ...and if we're on the skeleton, check the skeleton for dupes
			bVerifyName = !PersonaPtr.Pin()->DoesSocketAlreadyExist( SocketData, NewText, TargetSkeleton->Sockets );
		}

		// Needs to be checked on verify.
		if ( !bVerifyName )
		{

			// Tell the user that the socket is a duplicate
			OutErrorMessage = LOCTEXT( "DuplicateSocket_Error", "Socket name in use!");
		}
	}

	return bVerifyName;
}

void FDisplayedSocketInfo::OnCommitSocketName( const FText& InText, ETextCommit::Type CommitInfo )
{
	FText NewText = FText::TrimPrecedingAndTrailing(InText);

	const FScopedTransaction Transaction(LOCTEXT("RenameSocket", "Rename Socket"));
	SocketData->SetFlags( RF_Transactional );	// Undo doesn't work without this!
	SocketData->Modify();

	FName OldSocketName = SocketData->SocketName;
	SocketData->SocketName = FName( *NewText.ToString() );

	if ( SkeletonTree.IsValid() )
	{
		// Notify skeleton tree of socket rename
		SkeletonTree.Pin()->RenameSocketAttachments(OldSocketName, SocketData->SocketName);
	}
}

FReply FDisplayedSocketInfo::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ) )
	{
		FSelectedSocketInfo SocketInfo( SocketData, ParentType == ESocketParentType::Skeleton );

		return FReply::Handled().BeginDragDrop( FSocketDragDropOp::New( SocketInfo, MouseEvent.IsAltDown() ) );
	}

	return FReply::Unhandled();
}

FText FDisplayedSocketInfo::GetSocketToolTip()
{
	FText ToolTip;

	if ( ParentType == ESocketParentType::Skeleton && bIsCustomized == false )
	{
		ToolTip = LOCTEXT( "SocketToolTipSkeletonOnly", "This socket is on the skeleton only. It is shared with all meshes that use this skeleton" );
	}
	else if ( ParentType == ESocketParentType::Mesh && bIsCustomized == false )
	{
		ToolTip = LOCTEXT( "SocketToolTipMeshOnly", "This socket is on the current mesh only" );
	}
	else if ( ParentType == ESocketParentType::Skeleton )
	{
		ToolTip = LOCTEXT( "SocketToolTipSkeleton", "This socket is on the skeleton (shared with all meshes that use the skeleton), and the current mesh has duplciated version of it" );
	}
	else
	{
		ToolTip = LOCTEXT( "SocketToolTipCustomized", "This socket is on the current mesh, customizing the socket of the same name on the skeleton" );
	}

	return ToolTip;
}

//////////////////////////////////////////////////////////////////////////
// FDisplayedAttachedAssetInfo

TSharedRef<ITableRow> FDisplayedAttachedAssetInfo::MakeTreeRowWidget(
	const TSharedRef<STableViewBase>& InOwnerTable,
	FText InFilterText )
{
	return
		SNew( SSkeletonTreeRow, InOwnerTable )
		.Item( SharedThis(this) )
		.FilterText( InFilterText )
		.SkeletonTree( SkeletonTree )
		.TargetSkeleton( TargetSkeleton );
}

void FDisplayedAttachedAssetInfo::GenerateWidgetForNameColumn( TSharedPtr< SHorizontalBox > Box, FText& FilterText, FIsSelected InIsSelected )
{
	UActorFactory* ActorFactory = FActorFactoryAssetProxy::GetFactoryForAssetObject( Asset );
	const FSlateBrush* IconBrush = FClassIconFinder::FindIconForClass( ActorFactory->GetDefaultActorClass( FAssetData() ) );		
	
	Box->AddSlot()
		.AutoWidth()
		[
			SNew( SImage )
			.Image( IconBrush )
		];

	const FSlateFontInfo TextFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 10 );
	const FLinearColor TextColor(FLinearColor::White);

	Box->AddSlot()
		.AutoWidth()
		[
			SNew( STextBlock )
			.ColorAndOpacity(TextColor)
			.Text( FText::FromString(Asset->GetName()) )
			.HighlightText( FilterText )
			.Font(TextFont)
		];

	Box->AddSlot()
		.AutoWidth()
		.Padding(5.0f,0.0f)
		[
			SNew( STextBlock )
			.ColorAndOpacity( FLinearColor::Gray )
			.Text(LOCTEXT( "AttachedAssetPreviewText", "[Preview Only]" ) )
			.Font(TextFont)
			.ToolTipText( LOCTEXT( "AttachedAssetPreviewText_ToolTip", "Attached assets in Persona are preview only and do not carry through to the game." ) )
		];
}

TSharedRef< SWidget > FDisplayedAttachedAssetInfo::GenerateWidgetForDataColumn()
{
	return SNew( SHorizontalBox )
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign( HAlign_Left )
		[
			SNew( SCheckBox )
			.ToolTipText( LOCTEXT( "TranslationCheckBoxToolTip", "Click to toggle visibility of this asset" ) )
			.OnCheckStateChanged( this, &FDisplayedAttachedAssetInfo::OnToggleAssetDisplayed )
			.IsChecked( this, &FDisplayedAttachedAssetInfo::IsAssetDisplayed )
			.Style( FEditorStyle::Get(), "CheckboxLookToggleButtonCheckbox" )
			[
				SNew( SImage )
				.Image( this, &FDisplayedAttachedAssetInfo::OnGetAssetDisplayedButtonImage )
				.ColorAndOpacity( FLinearColor::Black )
			]
		];
}

ECheckBoxState FDisplayedAttachedAssetInfo::IsAssetDisplayed() const
{
	if(AssetComponent.IsValid())
	{
		return AssetComponent->IsVisible() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Undetermined;
}

void FDisplayedAttachedAssetInfo::OnToggleAssetDisplayed( ECheckBoxState InCheckboxState )
{
	if(AssetComponent.IsValid())
	{
		AssetComponent->SetVisibility(InCheckboxState == ECheckBoxState::Checked);
	}
}

const FSlateBrush* FDisplayedAttachedAssetInfo::OnGetAssetDisplayedButtonImage() const
{
	return IsAssetDisplayed() == ECheckBoxState::Checked ?
		FEditorStyle::GetBrush( "Kismet.VariableList.ExposeForInstance" ) :
		FEditorStyle::GetBrush( "Kismet.VariableList.HideForInstance" );
}

void FDisplayedAttachedAssetInfo::OnItemDoubleClicked()
{
	TArray<UObject*> AssetsToSync;
	AssetsToSync.Add(Asset);

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	ContentBrowserModule.Get().SyncBrowserToAssets( AssetsToSync );
}

//////////////////////////////////////////////////////////////////////////
// SSkeletonTree

const FString SSkeletonTree::SocketCopyPasteHeader = TEXT( "SocketCopyPasteBuffer" );

void SSkeletonTree::Construct(const FArguments& InArgs)
{
	static bool bMeshReductionSupportedInitialized = false;
	if ( !bMeshReductionSupportedInitialized )
	{
		IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
		IMeshReduction* MeshReduction = MeshUtilities.GetMeshReductionInterface();
		bMeshReductionSupported = MeshReduction && MeshReduction->IsSupported();
		bMeshReductionSupportedInitialized = true;
	}

	BoneFilter = EBoneFilter::All;
	SocketFilter = ESocketFilter::Active;
	bShowingRetargetingOptions = false;

	PersonaPtr = InArgs._Persona;
	IsEditable = InArgs._IsEditable;
	TargetSkeleton = PersonaPtr.Pin()->GetSkeleton();

	SetPreviewComponentSocketFilter();

	// Register a few delegates with Persona
	PersonaPtr.Pin()->RegisterOnPostUndo(FPersona::FOnPostUndo::CreateSP( this, &SSkeletonTree::PostUndo ) );
	PersonaPtr.Pin()->RegisterOnPreviewMeshChanged( FPersona::FOnPreviewMeshChanged::CreateSP( this, &SSkeletonTree::OnPreviewMeshChanged ) );
	PersonaPtr.Pin()->RegisterOnBoneSelected(FPersona::FOnBoneSelected::CreateSP( this, &SSkeletonTree::OnExternalSelectBone ) );
	PersonaPtr.Pin()->RegisterOnSocketSelected(FPersona::FOnSocketSelected::CreateSP( this, &SSkeletonTree::OnExternalSelectSocket ) );
	PersonaPtr.Pin()->RegisterOnDeselectAll(FPersona::FOnAllDeselected::CreateSP( this, &SSkeletonTree::OnExternalDeselectAll ) );
	PersonaPtr.Pin()->RegisterOnChangeSkeletonTree(FPersona::FOnSkeletonTreeChanged::CreateSP( this, &SSkeletonTree::PostUndo ) );

	// Register and bind all our menu commands
	FSkeletonTreeCommands::Register();
	BindCommands();

	this->ChildSlot
	[
		SNew( SVerticalBox )
		
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding( FMargin( 0.0f, 0.0f, 0.0f, 4.0f ) )
		[
			SAssignNew( NameFilterBox, SSearchBox )
			.SelectAllTextWhenFocused( true )
			.OnTextChanged( this, &SSkeletonTree::OnFilterTextChanged )
			.HintText( LOCTEXT( "SearchBoxHint", "Search Skeleton Tree...") )
			.AddMetaData<FTagMetaData>(TEXT("SkelTree.Search"))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew( SHorizontalBox )

			+ SHorizontalBox::Slot()
			.Padding( 0.0f, 0.0f, 2.0f, 0.0f )
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew( SComboButton )
				.ContentPadding(3)
				.OnGetMenuContent( this, &SSkeletonTree::CreateBoneFilterMenu )
				.ToolTipText( LOCTEXT( "BoneFilterToolTip", "Change which types of bones are shown" ) )
				.AddMetaData<FTagMetaData>(TEXT("SkelTree.Bones"))
				.ButtonContent()
				[
					SNew( STextBlock )
					.Text( this, &SSkeletonTree::GetBoneFilterMenuTitle )
				]
			]

			+ SHorizontalBox::Slot()
			.Padding( 0.0f, 0.0f, 2.0f, 0.0f )
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew( SComboButton )
				.ContentPadding(3)
				.OnGetMenuContent( this, &SSkeletonTree::CreateSocketFilterMenu )
				.ToolTipText( LOCTEXT( "SocketFilterToolTip", "Change which types of sockets are shown" ) )
				.AddMetaData<FTagMetaData>(TEXT("SkelTree.Sockets"))
				.ButtonContent()
				[
					SNew( STextBlock )
					.Text( this, &SSkeletonTree::GetSocketFilterMenuTitle )
				]
			]

			+ SHorizontalBox::Slot()
			.Padding(0.0f, 0.0f, 2.0f, 0.0f)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SCheckBox)
				.IsChecked(this, &SSkeletonTree::IsShowingRetargetingOptions)
				.ToolTipText(LOCTEXT("SocketFilterToolTip", "Change which types of sockets are shown"))
				.OnCheckStateChanged(this, &SSkeletonTree::OnChangeShowingRetargetingOptions)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("ShowRetargetingOptions", "Show Retargeting Options"))
				]
			]
		]

		+ SVerticalBox::Slot()
		.Padding( FMargin( 0.0f, 4.0f, 0.0f, 0.0f ) )
		[
			SAssignNew(TreeHolder, SOverlay)
		]
	];

	if (PersonaPtr.IsValid())
	{
		CreateTreeColumns();
	}
}

SSkeletonTree::~SSkeletonTree()
{
	if ( PersonaPtr.IsValid() )
	{
		PersonaPtr.Pin()->UnregisterOnPostUndo( this );
		PersonaPtr.Pin()->UnregisterOnPreviewMeshChanged( this );
		PersonaPtr.Pin()->UnregisterOnBoneSelected( this );
		PersonaPtr.Pin()->UnregisterOnSocketSelected( this );
		PersonaPtr.Pin()->UnregisterOnDeselectAll( this );
		PersonaPtr.Pin()->UnregisterOnChangeSkeletonTree( this );
		PersonaPtr.Pin()->UnregisterOnCreateViewport( this );
	}
}

void SSkeletonTree::BindCommands()
{
	// This should not be called twice on the same instance
	check( !UICommandList.IsValid() );

	UICommandList = MakeShareable( new FUICommandList );

	FUICommandList& CommandList = *UICommandList;

	// Grab the list of menu commands to bind...
	const FSkeletonTreeCommands& MenuActions = FSkeletonTreeCommands::Get();

	// ...and bind them all

	// Bone Filter commands
	CommandList.MapAction(
		MenuActions.ShowAllBones,
		FExecuteAction::CreateSP( this, &SSkeletonTree::SetBoneFilter, EBoneFilter::All ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SSkeletonTree::IsBoneFilter, EBoneFilter::All ) );

	CommandList.MapAction(
		MenuActions.ShowMeshBones,
		FExecuteAction::CreateSP( this, &SSkeletonTree::SetBoneFilter, EBoneFilter::Mesh ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SSkeletonTree::IsBoneFilter, EBoneFilter::Mesh ) );

	CommandList.MapAction(
		MenuActions.ShowWeightedBones,
		FExecuteAction::CreateSP( this, &SSkeletonTree::SetBoneFilter, EBoneFilter::Weighted ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SSkeletonTree::IsBoneFilter, EBoneFilter::Weighted ) );

	CommandList.MapAction(
		MenuActions.HideBones,
		FExecuteAction::CreateSP( this, &SSkeletonTree::SetBoneFilter, EBoneFilter::None ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SSkeletonTree::IsBoneFilter, EBoneFilter::None ) );

	// Socket filter commands
	CommandList.MapAction(
		MenuActions.ShowActiveSockets,
		FExecuteAction::CreateSP( this, &SSkeletonTree::SetSocketFilter, ESocketFilter::Active ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SSkeletonTree::IsSocketFilter, ESocketFilter::Active ) );

	CommandList.MapAction(
		MenuActions.ShowMeshSockets,
		FExecuteAction::CreateSP( this, &SSkeletonTree::SetSocketFilter, ESocketFilter::Mesh ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SSkeletonTree::IsSocketFilter, ESocketFilter::Mesh ) );

	CommandList.MapAction(
		MenuActions.ShowSkeletonSockets,
		FExecuteAction::CreateSP( this, &SSkeletonTree::SetSocketFilter, ESocketFilter::Skeleton ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SSkeletonTree::IsSocketFilter, ESocketFilter::Skeleton ) );

	CommandList.MapAction(
		MenuActions.ShowAllSockets,
		FExecuteAction::CreateSP( this, &SSkeletonTree::SetSocketFilter, ESocketFilter::All ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SSkeletonTree::IsSocketFilter, ESocketFilter::All ) );

	CommandList.MapAction(
		MenuActions.HideSockets,
		FExecuteAction::CreateSP( this, &SSkeletonTree::SetSocketFilter, ESocketFilter::None ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SSkeletonTree::IsSocketFilter, ESocketFilter::None ) );

	// Socket manipulation commands
	CommandList.MapAction(
		MenuActions.AddSocket,
		FExecuteAction::CreateSP( this, &SSkeletonTree::OnAddSocket ),
		FCanExecuteAction::CreateSP( this, &SSkeletonTree::IsAddingSocketsAllowed ) );

	CommandList.MapAction(
		FGenericCommands::Get().Rename,
		FExecuteAction::CreateSP( this, &SSkeletonTree::OnRenameSocket ),
		FCanExecuteAction::CreateSP( this, &SSkeletonTree::CanRenameSelected ) );

	CommandList.MapAction(
		MenuActions.CreateMeshSocket,
		FExecuteAction::CreateSP( this, &SSkeletonTree::OnCustomizeSocket ) );

	CommandList.MapAction(
		MenuActions.RemoveMeshSocket,
		FExecuteAction::CreateSP( this, &SSkeletonTree::OnDeleteSelectedRows ) ); // Removing customization just deletes the mesh socket

	CommandList.MapAction(
		MenuActions.PromoteSocketToSkeleton,
		FExecuteAction::CreateSP( this, &SSkeletonTree::OnPromoteSocket ) ); // Adding customization just deletes the mesh socket

	CommandList.MapAction(
		MenuActions.DeleteSelectedRows,
		FExecuteAction::CreateSP( this, &SSkeletonTree::OnDeleteSelectedRows ) );

	CommandList.MapAction(
		MenuActions.CopyBoneNames,
		FExecuteAction::CreateSP( this, &SSkeletonTree::OnCopyBoneNames ) );

	CommandList.MapAction(
		MenuActions.ResetBoneTransforms,
		FExecuteAction::CreateSP( this, &SSkeletonTree::OnResetBoneTransforms ) );

	CommandList.MapAction(
		MenuActions.CopySockets,
		FExecuteAction::CreateSP( this, &SSkeletonTree::OnCopySockets ) );

	CommandList.MapAction(
		MenuActions.PasteSockets,
		FExecuteAction::CreateSP( this, &SSkeletonTree::OnPasteSockets ) );
}

TSharedRef<ITableRow> SSkeletonTree::MakeTreeRowWidget(TSharedPtr<FDisplayedTreeRowInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable)
{
	check( InInfo.IsValid() );
	
	return InInfo->MakeTreeRowWidget( OwnerTable, FilterText );
}

void SSkeletonTree::GetChildrenForInfo(TSharedPtr<FDisplayedTreeRowInfo> InInfo, TArray< TSharedPtr<FDisplayedTreeRowInfo> >& OutChildren)
{
	check(InInfo.IsValid());
	OutChildren = InInfo->Children;
}

bool SSkeletonTree::AttachToParent( TSharedRef<FDisplayedTreeRowInfo> ItemToAttach, FName ParentName, int32 ItemsToInclude)
{
	// Find the parent info
	for (int32 BoneIndex = 0; BoneIndex < DisplayMirror.Num(); ++BoneIndex)
	{
		TSharedRef<FDisplayedTreeRowInfo> CurrentItem = DisplayMirror[BoneIndex];
		
		// does the item match our filter
		if ( (ItemsToInclude & CurrentItem->GetType()) != 0 )
		{
			if ( CurrentItem->GetRowItemName() == ParentName )
			{
				CurrentItem->Children.Insert( ItemToAttach, 0 );
				return true;
			}
		}
	}
	return false;
}

void SSkeletonTree::CreateTreeColumns()
{
	TSharedRef<SHeaderRow> TreeHeaderRow = SNew(SHeaderRow)
		+ SHeaderRow::Column(ColumnID_BoneLabel)
		.DefaultLabel(LOCTEXT("SkeletonBoneNameLabel", "Name"))
		.FillWidth(0.75f);

	if (bShowingRetargetingOptions)
	{
		TreeHeaderRow->AddColumn(
			SHeaderRow::Column(ColumnID_RetargetingLabel)
			.DefaultLabel(LOCTEXT("SkeletonBoneTranslationRetargetingLabel", "Translation Retargeting"))
			.FillWidth(0.25f)
			);
	}

	TreeHolder->ClearChildren();
	TreeHolder->AddSlot()
		[
			SAssignNew(SkeletonTreeView, SMeshSkeletonTreeRowType)
			.TreeItemsSource(&SkeletonRowList)
			.OnGenerateRow(this, &SSkeletonTree::MakeTreeRowWidget)
			.OnGetChildren(this, &SSkeletonTree::GetChildrenForInfo)
			.OnContextMenuOpening(this, &SSkeletonTree::CreateContextMenu)
			.OnSelectionChanged(this, &SSkeletonTree::OnSelectionChanged)
			.OnItemScrolledIntoView(this, &SSkeletonTree::OnItemScrolledIntoView)
			.OnMouseButtonDoubleClick(this, &SSkeletonTree::OnTreeDoubleClick)
			.OnSetExpansionRecursive(this, &SSkeletonTree::SetTreeItemExpansionRecursive)
			.ItemHeight(24)
			.HeaderRow
			(
			TreeHeaderRow
			)
		];

	CreateFromSkeleton(TargetSkeleton->GetBoneTree());
}

void SSkeletonTree::CreateFromSkeleton( const TArray<FBoneNode>& SourceSkeleton, USkeletalMeshSocket* SocketToRename )
{
	SkeletonRowList.Empty();

	DisplayMirror.Empty( SourceSkeleton.Num() );

	if( BoneFilter != EBoneFilter::None )
	{
		const FReferenceSkeleton& RefSkeleton = TargetSkeleton->GetReferenceSkeleton();
		for (int32 BoneIndex = 0; BoneIndex < SourceSkeleton.Num(); ++BoneIndex)
		{
			const FName& BoneName = RefSkeleton.GetBoneName(BoneIndex);
			if ( !FilterText.IsEmpty() && !BoneName.ToString().Contains( FilterText.ToString()) )
			{
				continue;
			}
		
			UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->GetPreviewMeshComponent();

			if ( PreviewComponent )
			{
				int32 BoneMeshIndex = PreviewComponent->GetBoneIndex( BoneName );

				// Remove non-mesh bones if we're filtering
				if ( ( BoneFilter == EBoneFilter::Mesh || BoneFilter == EBoneFilter::Weighted ) &&
					BoneMeshIndex == INDEX_NONE )
				{
					continue;
				}

				// Remove non-vertex-weighted bones if we're filtering
				if ( BoneFilter == EBoneFilter::Weighted && !IsBoneWeighted( BoneMeshIndex, PreviewComponent ) )
				{
					continue;
				}
			}

			int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);

			TSharedRef<FDisplayedMeshBoneInfo> DisplayBone = FDisplayedMeshBoneInfo::Make( RefSkeleton.GetBoneName(BoneIndex), TargetSkeleton, PersonaPtr, SharedThis( this ) );
		
			if ( BoneIndex > 0 && FilterText.IsEmpty() && DisplayMirror.Num() != 0 )	// No hierarchy when filtertext is non-empty
			{
				check(ParentIndex < BoneIndex);

				// We support filtering the list, so ParentIndex isn't necessarily correct in the DisplayMirror any more, so we need to search for it by name
				FName ParentName = RefSkeleton.GetBoneName(ParentIndex);
				bool bFoundRemappedParentIndex = false;

				for ( int32 i = 0; i < DisplayMirror.Num(); ++i )
				{
					FDisplayedTreeRowInfoPtr TreeRowInfo = DisplayMirror[i];

					// At this point, we can assume that *all* of DisplayMirror contains bones, not sockets
					check( TreeRowInfo->GetType() == ESkeletonTreeRowType::Bone );

					if ( *static_cast<FName*>( TreeRowInfo->GetData() ) == ParentName )
					{
						ParentIndex = i;
						bFoundRemappedParentIndex = true;
						break;
					}
				}

				if ( bFoundRemappedParentIndex )
				{
					DisplayMirror[ParentIndex]->Children.Add(DisplayBone);
				}
				else
				{
					// The parent bone didn't pass the filter, so just add this bone to the base of the tree
					SkeletonRowList.Add(DisplayBone);
				}
			}
			else
			{
				SkeletonRowList.Add(DisplayBone);
			}

			DisplayMirror.Add(DisplayBone);
			SkeletonTreeView->SetItemExpansion(DisplayBone, true);
		}
	}

	// Add the sockets for the skeleton
	if ( SocketFilter == ESocketFilter::Active || SocketFilter == ESocketFilter::All || SocketFilter == ESocketFilter::Skeleton )
	{
		AddSocketsFromData( TargetSkeleton->Sockets, ESocketParentType::Skeleton, SocketToRename );
	}
	
	if ( SocketFilter == ESocketFilter::Active || SocketFilter == ESocketFilter::All || SocketFilter == ESocketFilter::Mesh )
	{
		// Add the sockets for the mesh
		if ( PersonaPtr.IsValid() )
		{
			if ( USkeletalMesh* SkeletalMesh = PersonaPtr.Pin()->GetMesh() )
			{
				AddSocketsFromData( SkeletalMesh->GetMeshOnlySocketList(), ESocketParentType::Mesh, SocketToRename );
			}
		}
	}

	//Add the attached mesh items last, these are the most child like of all the items that can go in the skeleton tree

	// Mesh attached items...
	if ( PersonaPtr.IsValid() )
		{
		if ( USkeletalMesh* SkeletalMesh = PersonaPtr.Pin()->GetMesh() )
		{
			AddAttachedAssets( SkeletalMesh->PreviewAttachedAssetContainer );
		}
	}

	// ...skeleton attached items
	AddAttachedAssets( TargetSkeleton->PreviewAttachedAssetContainer );

	SkeletonTreeView->RequestTreeRefresh();
}

void SSkeletonTree::AddSocketsFromData(
	const TArray< USkeletalMeshSocket* >& SocketArray, 
	ESocketParentType::Type ParentType,
	USkeletalMeshSocket* SocketToRename )
{
	for ( auto SocketIt = SocketArray.CreateConstIterator(); SocketIt; ++SocketIt )
	{
		USkeletalMeshSocket* Socket = *( SocketIt );

		if ( !FilterText.IsEmpty() && !Socket->SocketName.ToString().Contains( FilterText.ToString() ) )
		{
			continue;
		}

		bool bIsCustomized = false;

		if ( ParentType == ESocketParentType::Mesh )
		{
			bIsCustomized = PersonaPtr.Pin()->DoesSocketAlreadyExist( NULL, FText::FromName( Socket->SocketName ), TargetSkeleton->Sockets );
		}
		else
		{
			if ( PersonaPtr.IsValid() )
			{
				if ( USkeletalMesh* Mesh = PersonaPtr.Pin()->GetMesh() )
				{
					bIsCustomized = PersonaPtr.Pin()->DoesSocketAlreadyExist( NULL, FText::FromName( Socket->SocketName ), Mesh->GetMeshOnlySocketList() );

					if ( SocketFilter == ESocketFilter::Active && bIsCustomized )
					{
						// Don't add the skeleton socket if it's already added for the mesh
						continue;
					}
				}
			}
		}

		TSharedRef<FDisplayedSocketInfo> DisplaySocket = FDisplayedSocketInfo::Make( Socket, ParentType, TargetSkeleton, PersonaPtr, SharedThis( this ), bIsCustomized );

		if(Socket == SocketToRename)
		{
			SkeletonTreeView->SetSelection(DisplaySocket);
			OnRenameSocket();
		}
		DisplayMirror.Add( DisplaySocket );
		
		if ( !AttachToParent( DisplaySocket, Socket->BoneName, ESkeletonTreeRowType::Bone ) )
		{
			// Just add it to the list if the parent bone isn't currently displayed
			SkeletonRowList.Add( DisplaySocket );
		}

		SkeletonTreeView->SetItemExpansion(DisplaySocket, true);
	}
}

class FBoneTreeSelection
{
public:
	TArray<TSharedPtr<FDisplayedTreeRowInfo>> SelectedItems;

	TArray<TSharedPtr<FDisplayedMeshBoneInfo>> SelectedBones;
	TArray<TSharedPtr<FDisplayedSocketInfo>> SelectedSockets;
	TArray<TSharedPtr<FDisplayedAttachedAssetInfo>> SelectedAssets;

	FBoneTreeSelection(TArray<TSharedPtr<FDisplayedTreeRowInfo>> InSelectedItems) : SelectedItems(InSelectedItems)
	{
		for ( auto ItemIt = SelectedItems.CreateConstIterator(); ItemIt; ++ItemIt )
		{
			FDisplayedTreeRowInfoPtr Item = *(ItemIt);

			switch(Item->GetType())
			{
				case ESkeletonTreeRowType::Bone:
				{
					SelectedBones.Add( StaticCastSharedPtr< FDisplayedMeshBoneInfo >(Item) );
					break;
				}
				case ESkeletonTreeRowType::Socket:
				{
					SelectedSockets.Add( StaticCastSharedPtr< FDisplayedSocketInfo >(Item) );
					break;
				}
				case ESkeletonTreeRowType::AttachedAsset:
				{
					SelectedAssets.Add( StaticCastSharedPtr< FDisplayedAttachedAssetInfo >(Item) );
					break;
				}
				default:
				{
					check(false); // Unknown row type!
				}
			}
		}
	}

	bool IsMultipleItemsSelected() const
	{
		return SelectedItems.Num() > 1;
	}

	bool IsSingleItemSelected() const
	{
		return SelectedItems.Num() == 1;
	}

	bool IsSingleOfTypeSelected(ESkeletonTreeRowType::Type ItemType) const
	{
		if(IsSingleItemSelected())
		{
			switch (ItemType)
			{
				case ESkeletonTreeRowType::Bone:
				{
					return SelectedBones.Num() == 1;
				}
				case ESkeletonTreeRowType::Socket:
				{
					return SelectedSockets.Num() == 1;
				}
				case ESkeletonTreeRowType::AttachedAsset:
				{
					return SelectedAssets.Num() == 1;
				}
				default:
				{
					check(false); // Unknown type
				}
			}
		}
		return false;
	}

	TSharedPtr<FDisplayedTreeRowInfo> GetSingleSelectedItem()
	{
		check(IsSingleItemSelected());
		return SelectedItems[0];
	}

	bool HasSelectedOfType(ESkeletonTreeRowType::Type ItemType) const
	{
		switch (ItemType)
		{
			case ESkeletonTreeRowType::Bone:
			{
				return SelectedBones.Num() > 0;
			}
			case ESkeletonTreeRowType::Socket:
			{
				return SelectedSockets.Num() > 0;
			}
			case ESkeletonTreeRowType::AttachedAsset:
			{
				return SelectedAssets.Num() > 0;
			}
			default:
			{
				check(false); // Unknown type
			}
		}
		return false;
	}
};

TSharedPtr< SWidget > SSkeletonTree::CreateContextMenu()
{
	const FSkeletonTreeCommands& Actions = FSkeletonTreeCommands::Get();

	FBoneTreeSelection BoneTreeSelection(SkeletonTreeView->GetSelectedItems());

	const bool CloseAfterSelection = true;
	FMenuBuilder MenuBuilder( CloseAfterSelection, UICommandList );
	{
		if(BoneTreeSelection.HasSelectedOfType(ESkeletonTreeRowType::AttachedAsset) || BoneTreeSelection.HasSelectedOfType(ESkeletonTreeRowType::Socket))
		{
			MenuBuilder.BeginSection("SkeletonTreeSelectedItemsActions", LOCTEXT( "SelectedActions", "Selected Item Actions" ) );
			MenuBuilder.AddMenuEntry( Actions.DeleteSelectedRows );
			MenuBuilder.EndSection();
		}

		if(BoneTreeSelection.HasSelectedOfType(ESkeletonTreeRowType::Bone))
		{
			MenuBuilder.BeginSection("SkeletonTreeBonesAction", LOCTEXT( "BoneActions", "Selected Bone Actions" ) );
			MenuBuilder.AddMenuEntry( Actions.CopyBoneNames );
			MenuBuilder.AddMenuEntry( Actions.ResetBoneTransforms );

			if(BoneTreeSelection.IsSingleOfTypeSelected(ESkeletonTreeRowType::Bone))
			{
				MenuBuilder.AddMenuEntry( Actions.AddSocket );
				MenuBuilder.AddMenuEntry( Actions.PasteSockets );
			}

			MenuBuilder.EndSection();

			if(bShowingRetargetingOptions)
			{
				MenuBuilder.BeginSection("SkeletonTreeBoneTranslationRetargeting", LOCTEXT("BoneTranslationRetargetingHeader", "Bone Translation Retargeting"));
				{
					FUIAction RecursiveRetargetingSkeletonAction = FUIAction(FExecuteAction::CreateSP(this, &SSkeletonTree::SetBoneTranslationRetargetingModeRecursive, EBoneTranslationRetargetingMode::Skeleton));
					FUIAction RecursiveRetargetingAnimationAction = FUIAction(FExecuteAction::CreateSP(this, &SSkeletonTree::SetBoneTranslationRetargetingModeRecursive, EBoneTranslationRetargetingMode::Animation));
					FUIAction RecursiveRetargetingAnimationScaledAction = FUIAction(FExecuteAction::CreateSP(this, &SSkeletonTree::SetBoneTranslationRetargetingModeRecursive, EBoneTranslationRetargetingMode::AnimationScaled));
					FUIAction RecursiveRetargetingAnimationRelativeAction = FUIAction(FExecuteAction::CreateSP(this, &SSkeletonTree::SetBoneTranslationRetargetingModeRecursive, EBoneTranslationRetargetingMode::AnimationRelative));

					MenuBuilder.AddMenuEntry
						(LOCTEXT("SetTranslationRetargetingSkeletonChildrenAction", "Recursively Set Translation Retargeting Skeleton")
						, LOCTEXT("BoneTranslationRetargetingSkeletonToolTip", "Use translation from Skeleton.")
						, FSlateIcon()
						, RecursiveRetargetingSkeletonAction
						);

					MenuBuilder.AddMenuEntry
						(LOCTEXT("SetTranslationRetargetingAnimationChildrenAction", "Recursively Set Translation Retargeting Animation")
						, LOCTEXT("BoneTranslationRetargetingAnimationToolTip", "Use translation from animation.")
						, FSlateIcon()
						, RecursiveRetargetingAnimationAction
						);

					MenuBuilder.AddMenuEntry
						(LOCTEXT("SetTranslationRetargetingAnimationScaledChildrenAction", "Recursively Set Translation Retargeting AnimationScaled")
						, LOCTEXT("BoneTranslationRetargetingAnimationScaledToolTip", "Use translation from animation, scale length by Skeleton's proportions.")
						, FSlateIcon()
						, RecursiveRetargetingAnimationScaledAction
						);

					MenuBuilder.AddMenuEntry
						(LOCTEXT("SetTranslationRetargetingAnimationRelativeChildrenAction", "Recursively Set Translation Retargeting AnimationRelative")
						, LOCTEXT("BoneTranslationRetargetingAnimationRelativeToolTip", "Use relative translation from animation similar to an additive animation.")
						, FSlateIcon()
						, RecursiveRetargetingAnimationRelativeAction
						);
				}
				MenuBuilder.EndSection();
			}
		}

		if(BoneTreeSelection.HasSelectedOfType(ESkeletonTreeRowType::Socket))
		{
			MenuBuilder.BeginSection("SkeletonTreeSocketsActions", LOCTEXT( "SocketActions", "Selected Socket Actions" ) );

			MenuBuilder.AddMenuEntry( Actions.CopySockets );

			if(BoneTreeSelection.IsSingleOfTypeSelected(ESkeletonTreeRowType::Socket))
			{
				MenuBuilder.AddMenuEntry( FGenericCommands::Get().Rename, NAME_None, LOCTEXT("RenameSocket_Label", "Rename Socket"), LOCTEXT("RenameSocket_Tooltip", "Rename this socket") );

				FDisplayedSocketInfo* DisplayedSocketInfo = static_cast< FDisplayedSocketInfo* >( BoneTreeSelection.GetSingleSelectedItem().Get() );

				if ( DisplayedSocketInfo->IsSocketCustomized() && DisplayedSocketInfo->GetParentType() == ESocketParentType::Mesh )
				{
					MenuBuilder.AddMenuEntry( Actions.RemoveMeshSocket );
				}

				USkeletalMeshSocket* SelectedSocket = static_cast< USkeletalMeshSocket* >( DisplayedSocketInfo->GetData() );
				USkeletalMesh* Mesh = PersonaPtr.Pin()->GetMesh();

				// If the socket is on the skeleton, we have a valid mesh
				// and there isn't one of the same name on the mesh, we can customize it
				if ( Mesh && !DisplayedSocketInfo->IsSocketCustomized() )
				{
					if ( DisplayedSocketInfo->GetParentType() == ESocketParentType::Skeleton )
					{
						MenuBuilder.AddMenuEntry( Actions.CreateMeshSocket );
					}
					else if ( DisplayedSocketInfo->GetParentType() == ESocketParentType::Mesh )
					{
						// If a socket is on the mesh only, then offer to promote it to the skeleton
						MenuBuilder.AddMenuEntry( Actions.PromoteSocketToSkeleton );
					}
				}
			}

			MenuBuilder.EndSection();
		}

		MenuBuilder.BeginSection("SkeletonTreeAttachedAssets", LOCTEXT( "AttachedAssetsActionsHeader", "Attached Assets Actions" ) );

		if ( BoneTreeSelection.IsSingleItemSelected() )
		{
			MenuBuilder.AddSubMenu(	LOCTEXT( "AttachNewAsset", "Add Preview Asset" ),
				LOCTEXT ( "AttachNewAsset_ToolTip", "Attaches an asset to this part of the skeleton. Assets can also be dragged onto the skeleton from a content browser to attach" ),
				FNewMenuDelegate::CreateSP( this, &SSkeletonTree::FillAttachAssetSubmenu, BoneTreeSelection.GetSingleSelectedItem() ) );
		}

		FUIAction RemoveAllAttachedAssets = FUIAction(	FExecuteAction::CreateSP( this, &SSkeletonTree::OnRemoveAllAssets ),
			FCanExecuteAction::CreateSP( this, &SSkeletonTree::CanRemoveAllAssets ));

		MenuBuilder.AddMenuEntry( LOCTEXT( "RemoveAllAttachedAssets", "Remove All Attached Assets" ),
			LOCTEXT ( "RemoveAllAttachedAssets_ToolTip", "Removes all the attached assets from the skeleton and mesh." ),
			FSlateIcon(), RemoveAllAttachedAssets );

		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

/*
void SSkeletonTree::CreateMenuForBoneReduction(FMenuBuilder& MenuBuilder, SSkeletonTree * Widget, USkeleton* Skeleton, bool bAdd)
{
	if (bMeshReductionSupported)
	{
		int32 LODSettingIndex=0;
		for(; LODSettingIndex<Skeleton->BoneReductionSettingsForLODs.Num(); ++LODSettingIndex)
		{
			if(Skeleton->BoneReductionSettingsForLODs[LODSettingIndex].BonesToRemove.Num() > 0)
			{
				if(bAdd)
				{
					FUIAction AddBonesToLOD1Action = FUIAction(FExecuteAction::CreateSP(Widget, &SSkeletonTree::AddToLOD, LODSettingIndex+1));
					MenuBuilder.AddMenuEntry
						(FText::FromString(FString::Printf(TEXT("LOD %d"), LODSettingIndex+1))
						, FText::FromString(FString::Printf(TEXT("LOD %d"), LODSettingIndex+1))
						, FSlateIcon()
						, AddBonesToLOD1Action
						);

				}
				else
				{
					FUIAction RemoveBonesFromLOD1Action = FUIAction(FExecuteAction::CreateSP(Widget, &SSkeletonTree::RemoveFromLOD, LODSettingIndex+1));
					MenuBuilder.AddMenuEntry
						(FText::FromString(FString::Printf(TEXT("LOD %d"), LODSettingIndex+1))
						, FText::FromString(FString::Printf(TEXT("LOD %d"), LODSettingIndex+1))
						, FSlateIcon()
						, RemoveBonesFromLOD1Action
						);
				}
			}
		}

		if(!bAdd)
		{
			FUIAction RemoveBonesFromLOD1Action = FUIAction(FExecuteAction::CreateSP(Widget, &SSkeletonTree::RemoveFromLOD, LODSettingIndex+1));
			MenuBuilder.AddMenuEntry
				(FText::FromString(FString::Printf(TEXT("LOD %d"), LODSettingIndex+1))
				, FText::FromString(FString::Printf(TEXT("LOD %d"), LODSettingIndex+1))
				, FSlateIcon()
				, RemoveBonesFromLOD1Action
				);
		}
	}
}
*/

void SSkeletonTree::SetBoneTranslationRetargetingModeRecursive(EBoneTranslationRetargetingMode::Type NewRetargetingMode)
{
	const FScopedTransaction Transaction( LOCTEXT( "SetBoneTranslationRetargetingModeRecursive", "Set Bone Translation Retargeting Mode Recursive" ) );
	TargetSkeleton->Modify();

	FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

	for ( auto ItemIt = TreeSelection.SelectedBones.CreateConstIterator(); ItemIt; ++ItemIt )
	{
		FName BoneName = *static_cast<FName*>( (*ItemIt)->GetData() );
		int32 BoneIndex = TargetSkeleton->GetReferenceSkeleton().FindBoneIndex( BoneName );
		TargetSkeleton->SetBoneTranslationRetargetingMode(BoneIndex, NewRetargetingMode, true);
	}
	FAssetNotifications::SkeletonNeedsToBeSaved(TargetSkeleton);
}

void SSkeletonTree::RemoveFromLOD(int32 LODIndex)
{
	if (bMeshReductionSupported)
	{
		const FScopedTransaction Transaction(LOCTEXT("RemoveBoneFromLOD", "Remove Selected Bones from LOD"));
		TargetSkeleton->Modify();

		FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

		for ( auto ItemIt = TreeSelection.SelectedBones.CreateConstIterator(); ItemIt; ++ItemIt )
		{
			FName BoneName = *static_cast<FName*>( (*ItemIt)->GetData() );
			int32 BoneIndex = TargetSkeleton->GetReferenceSkeleton().FindBoneIndex( BoneName );
			TargetSkeleton->RemoveBoneFromLOD(LODIndex, BoneIndex);
		}

		FAssetNotifications::SkeletonNeedsToBeSaved(TargetSkeleton);
	}
}

void SSkeletonTree::AddToLOD(int32 LODIndex)
{
	if (bMeshReductionSupported)
	{
		const FScopedTransaction Transaction(LOCTEXT("AddBoneToLOD", "Add Selected Bones to LOD"));
		TargetSkeleton->Modify();

		FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

		for ( auto ItemIt = TreeSelection.SelectedBones.CreateConstIterator(); ItemIt; ++ItemIt )
		{
			FName BoneName = *static_cast<FName*>( (*ItemIt)->GetData() );
			int32 BoneIndex = TargetSkeleton->GetReferenceSkeleton().FindBoneIndex( BoneName );
			TargetSkeleton->AddBoneToLOD(LODIndex, BoneIndex);
		}

		FAssetNotifications::SkeletonNeedsToBeSaved(TargetSkeleton);
	}
}

void SSkeletonTree::OnCopyBoneNames()
{
	FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

	if( TreeSelection.SelectedBones.Num() > 0 )
	{
		FString BoneNames;
		for( auto ItemIt = TreeSelection.SelectedBones.CreateConstIterator(); ItemIt; ++ItemIt )
		{
			FName* BoneName = static_cast<FName*>( (*ItemIt)->GetData() );

			BoneNames += BoneName->ToString();
			BoneNames += "\r\n";
		}
		FPlatformMisc::ClipboardCopy( *BoneNames );
	}
}

void SSkeletonTree::OnResetBoneTransforms()
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->GetPreviewMeshComponent();
	check(PreviewComponent);
	UAnimPreviewInstance* PreviewInstance = PreviewComponent->PreviewInstance;
	check(PreviewInstance);

	FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

	if( TreeSelection.SelectedBones.Num() > 0 )
	{
		bool bModified = false;
		GEditor->BeginTransaction( LOCTEXT("SkeletonTree_ResetBoneTransforms", "Reset Bone Transforms" ) );

		for( auto ItemIt = TreeSelection.SelectedBones.CreateConstIterator(); ItemIt; ++ItemIt )
		{
			const FName* BoneName = static_cast<FName*>( (*ItemIt)->GetData() );
			check(BoneName);
			const FAnimNode_ModifyBone* ModifiedBone = PreviewInstance->FindModifiedBone(*BoneName);
			if(ModifiedBone != nullptr)
			{
				if(!bModified)
				{
					PreviewInstance->SetFlags( RF_Transactional );	
					PreviewInstance->Modify();
					bModified = true;
				}

				PreviewInstance->RemoveBoneModification(*BoneName);
			}
		}

		GEditor->EndTransaction();
	}
}

void SSkeletonTree::OnCopySockets() const
{
	FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

	int32 NumSocketsToCopy = TreeSelection.SelectedSockets.Num();
	if ( NumSocketsToCopy > 0 )
	{
		FString SocketsDataString;

		for ( auto ItemIt = TreeSelection.SelectedSockets.CreateConstIterator(); ItemIt; ++ItemIt )
		{
			FDisplayedSocketInfo* DisplayedSocketInfo = (*ItemIt).Get();
			USkeletalMeshSocket* Socket = static_cast<USkeletalMeshSocket*>( DisplayedSocketInfo->GetData() );

			SocketsDataString += SerializeSocketToString( Socket, DisplayedSocketInfo );
		}

		FString CopyString = FString::Printf( TEXT("%s\nNumSockets=%d\n%s"), *SocketCopyPasteHeader, NumSocketsToCopy, *SocketsDataString );

		FPlatformMisc::ClipboardCopy( *CopyString );
	}
}

FString SSkeletonTree::SerializeSocketToString( USkeletalMeshSocket* Socket, const FDisplayedSocketInfo* DisplayedSocketInfo ) const
{
	FString SocketString;

	SocketString += FString::Printf( TEXT( "IsOnSkeleton=%s\n" ), DisplayedSocketInfo->GetParentType() == ESocketParentType::Skeleton ? TEXT( "1" ) : TEXT( "0" ) );

	FStringOutputDevice Buffer;
	const FExportObjectInnerContext Context;
	UExporter::ExportToOutputDevice( &Context, Socket, NULL, Buffer, TEXT( "copy" ), 0, PPF_Copy, false );
	SocketString += Buffer;

	return SocketString;
}

void SSkeletonTree::OnPasteSockets()
{
	FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

	// Pasting sockets should only work if there is just one bone selected
	if ( TreeSelection.IsSingleOfTypeSelected(ESkeletonTreeRowType::Bone) )
	{
		FName DestBoneName = *static_cast<FName*>( TreeSelection.GetSingleSelectedItem()->GetData() );

		FString PasteString;
		FPlatformMisc::ClipboardPaste( PasteString );
		const TCHAR* PastePtr = *PasteString;

		FString PasteLine;
		FParse::Line( &PastePtr, PasteLine );

		if ( PasteLine == SocketCopyPasteHeader )
		{
			const FScopedTransaction Transaction( LOCTEXT( "PasteSockets", "Paste sockets" ) );

			int32 NumSocketsToPaste;
			FParse::Line( &PastePtr, PasteLine );	// Need this to advance PastePtr, for multiple sockets
			FParse::Value( *PasteLine, TEXT( "NumSockets=" ), NumSocketsToPaste );
			FParse::Line( &PastePtr, PasteLine );

			for ( int32 i = 0; i < NumSocketsToPaste; ++i )
			{					
				bool bIsOnSkeleton;
				FParse::Bool( *PasteLine, TEXT( "IsOnSkeleton=" ), bIsOnSkeleton );

				USkeletalMeshSocket* NewSocket;
				FSocketTextObjectFactory TextObjectFactory( &NewSocket );

				if ( bIsOnSkeleton )
				{
					TargetSkeleton->Modify();

					TextObjectFactory.ProcessBuffer( TargetSkeleton, RF_Transactional, PastePtr );
					check(NewSocket);
				}
				else
				{
					USkeletalMesh* Mesh = PersonaPtr.Pin()->GetMesh();

					Mesh->Modify();

					TextObjectFactory.ProcessBuffer( Mesh, RF_Transactional, PastePtr );
					check(NewSocket);
				}

				// Override the bone name to the one we pasted to
				NewSocket->BoneName = DestBoneName;

				// Check the socket name is unique
				NewSocket->SocketName = PersonaPtr.Pin()->GenerateUniqueSocketName( NewSocket->SocketName );

				// Skip ahead in the stream to the next socket (if there is one)
				PastePtr = FCString::Strstr( PastePtr, TEXT( "IsOnSkeleton=" ) );

				if ( bIsOnSkeleton )
				{
					TargetSkeleton->Sockets.Add( NewSocket );
				}
				else
				{
					USkeletalMesh* Mesh = PersonaPtr.Pin()->GetMesh();
					Mesh->GetMeshOnlySocketList().Add( NewSocket );
				}
			}
		}
		CreateFromSkeleton( TargetSkeleton->GetBoneTree() );
	}
}

void SSkeletonTree::OnAddSocket()
{
	// This adds a socket to the currently selected bone in the SKELETON, not the MESH.
	FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

	// Can only add a socket to one bone
	if(TreeSelection.IsSingleOfTypeSelected(ESkeletonTreeRowType::Bone))
	{
		USkeletalMeshSocket* NewSocket;

		const FScopedTransaction Transaction( LOCTEXT( "AddSocket", "Add Socket to Skeleton" ) );
		TargetSkeleton->Modify();

		NewSocket = ConstructObject<USkeletalMeshSocket>( USkeletalMeshSocket::StaticClass(), TargetSkeleton );
		check(NewSocket);

		NewSocket->BoneName = *static_cast<FName*>( TreeSelection.GetSingleSelectedItem()->GetData() );
		FString SocketName = NewSocket->BoneName.ToString() + LOCTEXT("SocketPostfix", "Socket").ToString();
		NewSocket->SocketName = PersonaPtr.Pin()->GenerateUniqueSocketName( *SocketName );

		TargetSkeleton->Sockets.Add( NewSocket );

		FSelectedSocketInfo SocketInfo(NewSocket, true);
		PersonaPtr.Pin()->SetSelectedSocket(SocketInfo, false);

		CreateFromSkeleton( TargetSkeleton->GetBoneTree(), NewSocket );
	}
}

void SSkeletonTree::OnCustomizeSocket()
{
	// This should only be called on a skeleton socket, it copies the 
	// socket to the mesh so the user can edit it separately
	FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

	if(TreeSelection.IsSingleOfTypeSelected(ESkeletonTreeRowType::Socket))
	{
		USkeletalMeshSocket* SocketToCustomize = static_cast<USkeletalMeshSocket*>( TreeSelection.GetSingleSelectedItem()->GetData() );

		if ( PersonaPtr.IsValid() )
		{
			USkeletalMesh* Mesh = PersonaPtr.Pin()->GetMesh();

			if ( Mesh )
			{
				const FScopedTransaction Transaction( LOCTEXT( "CreateMeshSocket", "Create Mesh Socket" ) );
				Mesh->Modify();

				USkeletalMeshSocket* NewSocket = ConstructObject<USkeletalMeshSocket>( USkeletalMeshSocket::StaticClass(), Mesh );
				check(NewSocket);

				NewSocket->BoneName = SocketToCustomize->BoneName;
				NewSocket->SocketName = SocketToCustomize->SocketName;
				NewSocket->RelativeLocation = SocketToCustomize->RelativeLocation;
				NewSocket->RelativeRotation = SocketToCustomize->RelativeRotation;
				NewSocket->RelativeScale = SocketToCustomize->RelativeScale;

				Mesh->GetMeshOnlySocketList().Add( NewSocket );

				CreateFromSkeleton( TargetSkeleton->GetBoneTree() );
			}
		}
	}
}

void SSkeletonTree::OnPromoteSocket()
{
	// This should only be called on a mesh socket, it copies the 
	// socket to the skeleton so all meshes can use it
	FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

	// Can only customize one socket (CreateContextMenu() should prevent this firing!)
	if(TreeSelection.IsSingleOfTypeSelected(ESkeletonTreeRowType::Socket))
	{
		USkeletalMeshSocket* SocketToCustomize = static_cast<USkeletalMeshSocket*>( TreeSelection.GetSingleSelectedItem()->GetData() );

		const FScopedTransaction Transaction( LOCTEXT( "PromoteSocket", "Promote Socket" ) );
		TargetSkeleton->Modify();

		USkeletalMeshSocket* NewSocket = ConstructObject<USkeletalMeshSocket>( USkeletalMeshSocket::StaticClass(), TargetSkeleton );
		check(NewSocket);

		NewSocket->BoneName = SocketToCustomize->BoneName;
		NewSocket->SocketName = SocketToCustomize->SocketName;
		NewSocket->RelativeLocation = SocketToCustomize->RelativeLocation;
		NewSocket->RelativeRotation = SocketToCustomize->RelativeRotation;
		NewSocket->RelativeScale = SocketToCustomize->RelativeScale;

		TargetSkeleton->Sockets.Add( NewSocket );

		CreateFromSkeleton( TargetSkeleton->GetBoneTree() );
	}
}

void SSkeletonTree::FillAttachAssetSubmenu(FMenuBuilder& MenuBuilder, const FDisplayedTreeRowInfoPtr TargetItem)
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	TArray<UClass*> FilterClasses = FComponentAssetBrokerage::GetSupportedAssets(USceneComponent::StaticClass());

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.bRecursiveClasses = true;

	for(int i = 0; i < FilterClasses.Num(); ++i)
	{
		AssetPickerConfig.Filter.ClassNames.Add(FilterClasses[i]->GetFName());
	}


	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SSkeletonTree::OnAssetSelectedFromPicker, TargetItem);

	TSharedRef<SWidget> MenuContent = SNew(SBox)
	.WidthOverride(384)
	.HeightOverride(500)
	[
		ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
	];
	MenuBuilder.AddWidget( MenuContent, FText::GetEmpty(), true);
}

void SSkeletonTree::OnAssetSelectedFromPicker(const FAssetData& AssetData, const FDisplayedTreeRowInfoPtr TargetItem)
{
	FSlateApplication::Get().DismissAllMenus();
	TArray<FAssetData> Assets;
	Assets.Add(AssetData);

	AttachAssetsToSkeletonTree(TargetItem, Assets);
}

void  SSkeletonTree::OnRemoveAllAssets()
{
	FScopedTransaction Transaction( LOCTEXT("AttachedAssetRemoveUndo", "Remove All Attached Assets") );
	TargetSkeleton->Modify();

	DeleteAttachedObjects( TargetSkeleton->PreviewAttachedAssetContainer );

	USkeletalMesh* Mesh = PersonaPtr.Pin()->GetMesh();

	if ( Mesh )
	{
		Mesh->Modify();
		DeleteAttachedObjects( Mesh->PreviewAttachedAssetContainer );
	}

	CreateFromSkeleton( TargetSkeleton->GetBoneTree() );
}

bool SSkeletonTree::CanRemoveAllAssets() const
{
	USkeletalMesh* SkeletalMesh = NULL;
	if ( PersonaPtr.IsValid() )
	{
		SkeletalMesh = PersonaPtr.Pin()->GetMesh();
	}

	const bool bHasPreviewAttachedObjects = TargetSkeleton->PreviewAttachedAssetContainer.Num() > 0;
	const bool bHasMeshPreviewAttachedObjects = ( SkeletalMesh && SkeletalMesh->PreviewAttachedAssetContainer.Num() );

	return bHasPreviewAttachedObjects || bHasMeshPreviewAttachedObjects;
}

void SSkeletonTree::DeleteAttachedObjects( FPreviewAssetAttachContainer& AttachedAssets )
{
	for(auto Iter = AttachedAssets.CreateIterator(); Iter; ++Iter)
	{
		FPreviewAttachedObjectPair& Pair = (*Iter);
		PersonaPtr.Pin()->RemoveAttachedObjectFromPreviewComponent(Pair.GetAttachedObject(), Pair.AttachedTo);
	}

	AttachedAssets.ClearAllAttachedObjects();
}

bool SSkeletonTree::CanRenameSelected() const
{
	FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());
	return TreeSelection.IsSingleOfTypeSelected(ESkeletonTreeRowType::Socket);
}

void SSkeletonTree::OnRenameSocket()
{
	FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

	if(TreeSelection.IsSingleOfTypeSelected(ESkeletonTreeRowType::Socket))
	{
		SkeletonTreeView->RequestScrollIntoView(TreeSelection.GetSingleSelectedItem());
		DeferredRenameRequest = TreeSelection.GetSingleSelectedItem();
	}
}

void SSkeletonTree::OnSelectionChanged(TSharedPtr<FDisplayedTreeRowInfo> Selection, ESelectInfo::Type SelectInfo)
{
	if( Selection.IsValid() )
	{
		//Get all the selected items
		FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

		UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->GetPreviewMeshComponent();
		if( TreeSelection.SelectedItems.Num() > 0 && PreviewComponent )
		{
			// pick the first settable bone from the selection
			for ( auto ItemIt = TreeSelection.SelectedItems.CreateConstIterator(); ItemIt; ++ItemIt )
			{
				FDisplayedTreeRowInfoPtr Item = *(ItemIt);

				// Test SelectInfo so we don't end up in an infinite loop due to delegates calling each other
				if ( SelectInfo != ESelectInfo::Direct && Item->GetType() == ESkeletonTreeRowType::Bone )
				{
					FName BoneName = *static_cast<FName *>( Item->GetData() );

					// Get bone index
					int32 BoneIndex = PreviewComponent->GetBoneIndex( BoneName );
					if( BoneIndex != INDEX_NONE )
					{
						PersonaPtr.Pin()->SetSelectedBone(TargetSkeleton, BoneName, false );
						break;
					}
				}
				// Test SelectInfo so we don't end up in an infinite loop due to delegates calling each other
				else if( SelectInfo != ESelectInfo::Direct && Item->GetType() == ESkeletonTreeRowType::Socket )
				{
					FDisplayedSocketInfo* DisplayedSocketInfo = static_cast<FDisplayedSocketInfo*>( Item.Get() );
					USkeletalMeshSocket* Socket = static_cast<USkeletalMeshSocket*>( Item->GetData() );

					FSelectedSocketInfo SocketInfo( Socket, DisplayedSocketInfo->GetParentType() == ESocketParentType::Skeleton );

 					PersonaPtr.Pin()->SetSelectedSocket( SocketInfo, false );
				}
				else if(Item->GetType() == ESkeletonTreeRowType::AttachedAsset)
				{
					PersonaPtr.Pin()->ClearSelectedBones();
					PersonaPtr.Pin()->ClearSelectedSocket();
				}
			}
			PreviewComponent->PostInitMeshObject(PreviewComponent->MeshObject);
		}
	}
	else
	{
		// Tell Persona if the user ctrl-clicked the selected bone/socket to de-select it
		PersonaPtr.Pin()->ClearSelectedBones();
		PersonaPtr.Pin()->ClearSelectedSocket();
	}
}

FReply SSkeletonTree::OnDropAssetToSkeletonTree(const FDisplayedTreeRowInfoPtr TargetItem, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FAssetDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	if (DragDropOp.IsValid())
	{
		//Do we have some assets to attach?
		if(DragDropOp->AssetData.Num() > 0)
		{
			AttachAssetsToSkeletonTree(TargetItem, DragDropOp->AssetData);
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SSkeletonTree::AttachAssetsToSkeletonTree(const FDisplayedTreeRowInfoPtr TargetItem, const TArray<FAssetData>& AssetData)
{
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->GetPreviewMeshComponent();
	if(PreviewComponent && PreviewComponent->SkeletalMesh && PreviewComponent->SkeletalMesh->Skeleton)
	{
		bool bAllAssetWereLoaded = true;
		TArray<UObject*> DroppedObjects;
		for (int32 AssetIdx = 0; AssetIdx < AssetData.Num(); ++AssetIdx)
		{
			UObject* Object = AssetData[AssetIdx].GetAsset();
			if ( Object != NULL )
			{
				DroppedObjects.Add( Object );
			}
			else
			{
				bAllAssetWereLoaded = false;
			}
		}

		if(bAllAssetWereLoaded)
		{
			FName AttachToName = TargetItem->GetAttachName();

			for(auto Iter = DroppedObjects.CreateIterator(); Iter; ++Iter)
			{
				UObject* Object = (*Iter);

				if ( TargetItem->GetType() == ESkeletonTreeRowType::Socket &&
					static_cast<FDisplayedSocketInfo*>( TargetItem.Get() )->GetParentType() == ESocketParentType::Mesh )
				{
					FScopedTransaction Transaction( LOCTEXT("DragDropAttachMeshUndo", "Attach Assets to Mesh") );

					USkeletalMesh* Mesh = PersonaPtr.Pin()->GetMesh();
					Mesh->Modify();
					PersonaPtr.Pin()->AttachObjectToPreviewComponent( Object, AttachToName, &Mesh->PreviewAttachedAssetContainer );
				}
				else
				{
					FScopedTransaction Transaction( LOCTEXT("DragDropAttachSkeletonUndo", "Attach Assets to Skeleton") );

					TargetSkeleton->Modify();
					PersonaPtr.Pin()->AttachObjectToPreviewComponent( Object, AttachToName, &TargetSkeleton->PreviewAttachedAssetContainer );
				}
			}
			CreateFromSkeleton( TargetSkeleton->GetBoneTree() );
		}
	}
}

void SSkeletonTree::OnItemScrolledIntoView( FDisplayedTreeRowInfoPtr InItem, const TSharedPtr<ITableRow>& InWidget)
{
	if(DeferredRenameRequest.IsValid())
	{
		DeferredRenameRequest->RequestRename();
		DeferredRenameRequest.Reset();
	}
}

void SSkeletonTree::OnTreeDoubleClick( FDisplayedTreeRowInfoPtr InItem )
{
	InItem->OnItemDoubleClicked();
}

void SSkeletonTree::SetTreeItemExpansionRecursive(TSharedPtr< FDisplayedTreeRowInfo > TreeItem, bool bInExpansionState) const
{
	SkeletonTreeView->SetItemExpansion(TreeItem, bInExpansionState);

	// Recursively go through the children.
	for (auto It = TreeItem->Children.CreateIterator(); It; ++It)
	{
		SetTreeItemExpansionRecursive(*It, bInExpansionState);
	}
}

void SSkeletonTree::PostUndo()
{
	// Rebuild the tree view whenever we undo a change to the skeleton
	CreateFromSkeleton( TargetSkeleton->GetBoneTree() );

	PersonaPtr.Pin()->ClearSelectedBones();
	PersonaPtr.Pin()->ClearSelectedSocket();
}

void SSkeletonTree::OnFilterTextChanged( const FText& SearchText )
{
	FilterText = SearchText;

	CreateFromSkeleton( TargetSkeleton->GetBoneTree() );
}

void SSkeletonTree::OnExternalSelectSocket( const FSelectedSocketInfo& SocketInfo )
{
	// This function is called when something else selects a socket (i.e. *NOT* the user clicking on a row in the treeview)
	// For example, this would be called if user clicked a socket hit point in the preview window

	// Firstly, find which row (if any) contains the socket requested
	for ( auto SkeletonRowIt = DisplayMirror.CreateConstIterator(); SkeletonRowIt; ++SkeletonRowIt )
	{
		FDisplayedTreeRowInfoPtr SkeletonRow = *( SkeletonRowIt );

		if ( SkeletonRow->GetType() == ESkeletonTreeRowType::Socket && SkeletonRow->GetData() == SocketInfo.Socket )
		{
			SkeletonTreeView->SetSelection( SkeletonRow );
			SkeletonTreeView->RequestScrollIntoView( SkeletonRow );
		}
	}
}

void SSkeletonTree::OnExternalSelectBone( const FName& BoneName )
{
	// This function is called when something else selects a bone (i.e. *NOT* the user clicking on a row in the treeview)
	// For example, this would be called if user clicked a bone hit point in the preview window

	// Find which row (if any) contains the bone requested
	for ( auto SkeletonRowIt = DisplayMirror.CreateConstIterator(); SkeletonRowIt; ++SkeletonRowIt )
	{
		FDisplayedTreeRowInfoPtr SkeletonRow = *( SkeletonRowIt );

		if ( SkeletonRow->GetType() == ESkeletonTreeRowType::Bone &&
			*static_cast< FName* >( SkeletonRow->GetData() ) == BoneName )
		{
			SkeletonTreeView->SetSelection( SkeletonRow );
			SkeletonTreeView->RequestScrollIntoView( SkeletonRow );
		}
	}
}

void SSkeletonTree::OnExternalDeselectAll()
{
	SkeletonTreeView->ClearSelection();

	if ( PersonaPtr.IsValid() )
	{
		PersonaPtr.Pin()->ClearSelectedBones();
		PersonaPtr.Pin()->ClearSelectedSocket();
	}
}

void SSkeletonTree::NotifyUser( FNotificationInfo& NotificationInfo )
{
	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification( NotificationInfo );
	if ( Notification.IsValid() )
	{
		Notification->SetCompletionState( SNotificationItem::CS_Fail );
	}
}

TSharedRef< SWidget > SSkeletonTree::CreateBoneFilterMenu()
{
	const FSkeletonTreeCommands& Actions = FSkeletonTreeCommands::Get();

	const bool CloseAfterSelection = true;
	FMenuBuilder MenuBuilder( CloseAfterSelection, UICommandList );

	MenuBuilder.BeginSection("Bones", LOCTEXT( "BonesMenuHeading", "Bones" ) );
	{
		MenuBuilder.AddMenuEntry( Actions.ShowAllBones );
		MenuBuilder.AddMenuEntry( Actions.ShowMeshBones );
		MenuBuilder.AddMenuEntry( Actions.ShowWeightedBones );
		MenuBuilder.AddMenuEntry( Actions.HideBones );
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedRef< SWidget > SSkeletonTree::CreateSocketFilterMenu()
{
	const FSkeletonTreeCommands& Actions = FSkeletonTreeCommands::Get();

	const bool CloseAfterSelection = true;
	FMenuBuilder MenuBuilder( CloseAfterSelection, UICommandList );

	MenuBuilder.BeginSection( "Sockets", LOCTEXT( "SocketsMenuHeading", "Sockets" ) );
	MenuBuilder.AddMenuEntry( Actions.ShowActiveSockets );
	MenuBuilder.AddMenuEntry( Actions.ShowMeshSockets );
	MenuBuilder.AddMenuEntry( Actions.ShowSkeletonSockets );
	MenuBuilder.AddMenuEntry( Actions.ShowAllSockets );
	MenuBuilder.AddMenuEntry( Actions.HideSockets );
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SSkeletonTree::SetBoneFilter( EBoneFilter::Type InBoneFilter )
{
	check( InBoneFilter < EBoneFilter::Count );
	BoneFilter = InBoneFilter;

	CreateFromSkeleton( TargetSkeleton->GetBoneTree() );
}

bool SSkeletonTree::IsBoneFilter( EBoneFilter::Type InBoneFilter ) const
{
	return BoneFilter == InBoneFilter;
}

void SSkeletonTree::SetSocketFilter( ESocketFilter::Type InSocketFilter )
{
	check( InSocketFilter < ESocketFilter::Count );
	SocketFilter = InSocketFilter;

	SetPreviewComponentSocketFilter();

	CreateFromSkeleton( TargetSkeleton->GetBoneTree() );
}

void SSkeletonTree::SetPreviewComponentSocketFilter() const
{
	// Set the socket filter in the debug skeletal mesh component so the viewport can share the filter settings
	UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->GetPreviewMeshComponent();

	bool bAllOrActive = ( SocketFilter == ESocketFilter::All || SocketFilter == ESocketFilter::Active );

	if ( PreviewComponent )
	{
		PreviewComponent->bMeshSocketsVisible = bAllOrActive || SocketFilter == ESocketFilter::Mesh;
		PreviewComponent->bSkeletonSocketsVisible = bAllOrActive || SocketFilter == ESocketFilter::Skeleton;
	}
}

bool SSkeletonTree::IsSocketFilter( ESocketFilter::Type InSocketFilter ) const
{
	return SocketFilter == InSocketFilter;
}

bool SSkeletonTree::IsBoneWeighted( int32 MeshBoneIndex, UDebugSkelMeshComponent* PreviewComponent ) const
{
	// MeshBoneIndex must be an index into the mesh's skeleton, *not* the source skeleton!!!

	if ( !PreviewComponent || !PreviewComponent->SkeletalMesh || !PreviewComponent->SkeletalMesh->GetImportedResource()->LODModels.Num() )
	{
		// If there's no mesh, then this bone can't possibly be weighted!
		return false;
	}

	//Get current LOD
	const int32 LODIndex = FMath::Clamp(PreviewComponent->PredictedLODLevel, 0, PreviewComponent->SkeletalMesh->GetImportedResource()->LODModels.Num()-1);
	FStaticLODModel& LODModel = PreviewComponent->SkeletalMesh->GetImportedResource()->LODModels[ LODIndex ];

	//Check whether the bone is vertex weighted
	int32 Index = LODModel.ActiveBoneIndices.Find( MeshBoneIndex );
	
	return Index != INDEX_NONE;
}

FText SSkeletonTree::GetBoneFilterMenuTitle() const
{
	FText BoneFilterMenuText;

	switch ( BoneFilter )
	{
	case EBoneFilter::All:
		BoneFilterMenuText = LOCTEXT( "BoneFilterMenuAll", "All Bones" );
		break;

	case EBoneFilter::Mesh:
		BoneFilterMenuText = LOCTEXT( "BoneFilterMenuMesh", "Mesh Bones" );
		break;

	case EBoneFilter::Weighted:
		BoneFilterMenuText = LOCTEXT( "BoneFilterMenuWeighted", "Weighted Bones" );
		break;

	case EBoneFilter::None:
		BoneFilterMenuText = LOCTEXT( "BoneFilterMenuHidden", "Bones Hidden" );
		break;

	default:
		// Unknown mode
		check( 0 );
		break;
	}

	return BoneFilterMenuText;
}

FText SSkeletonTree::GetSocketFilterMenuTitle() const
{
	FText SocketFilterMenuText;

	switch ( SocketFilter )
	{
	case ESocketFilter::Active:
		SocketFilterMenuText = LOCTEXT( "SocketFilterMenuActive", "Active Sockets" );
		break;

	case ESocketFilter::Mesh:
		SocketFilterMenuText = LOCTEXT( "SocketFilterMenuMesh", "Mesh Sockets" );
		break;

	case ESocketFilter::Skeleton:
		SocketFilterMenuText = LOCTEXT( "SocketFilterMenuSkeleton", "Skeleton Sockets" );
		break;

	case ESocketFilter::All:
		SocketFilterMenuText = LOCTEXT( "SocketFilterMenuAll", "All Sockets" );
		break;

	case ESocketFilter::None:
		SocketFilterMenuText = LOCTEXT( "SocketFilterMenuHidden", "Sockets Hidden" );
		break;

	default:
		// Unknown mode
		check( 0 );
		break;
	}

	return SocketFilterMenuText;
}

void SSkeletonTree::OnPreviewMeshChanged( USkeletalMesh* NewPreviewMesh )
{
	// Simply rebuild the tree
	CreateFromSkeleton( TargetSkeleton->GetBoneTree() );
}

void SSkeletonTree::RenameSocketAttachments(FName& OldSocketName, FName& NewSocketName)
{
	const FScopedTransaction Transaction( LOCTEXT( "RenameSocketAttachments", "Rename Socket Attachments" ) );

	bool bSkeletonModified = false;
	for(int AttachedObjectIndex = 0; AttachedObjectIndex < TargetSkeleton->PreviewAttachedAssetContainer.Num(); ++AttachedObjectIndex)
	{
		FPreviewAttachedObjectPair& Pair = TargetSkeleton->PreviewAttachedAssetContainer[AttachedObjectIndex];
		if(Pair.AttachedTo == OldSocketName)
		{
			// Only modify the skeleton if we actually intend to change something.
			if(!bSkeletonModified)
			{
				TargetSkeleton->Modify();
				bSkeletonModified = true;
			}
			Pair.AttachedTo = NewSocketName;
		}
		PersonaPtr.Pin()->RemoveAttachedObjectFromPreviewComponent(Pair.GetAttachedObject(), OldSocketName);
		PersonaPtr.Pin()->AttachObjectToPreviewComponent(Pair.GetAttachedObject(), Pair.AttachedTo);
	}

	if ( PersonaPtr.IsValid() )
	{
		USkeletalMesh* Mesh = PersonaPtr.Pin()->GetMesh();

		if ( Mesh )
		{
			bool bMeshModified = false;
			for(int AttachedObjectIndex = 0; AttachedObjectIndex < Mesh->PreviewAttachedAssetContainer.Num(); ++AttachedObjectIndex)
			{
				FPreviewAttachedObjectPair& Pair = Mesh->PreviewAttachedAssetContainer[AttachedObjectIndex];
				if(Pair.AttachedTo == OldSocketName)
				{
					// Only modify the mesh if we actually intend to change something. Avoids dirtying
					// meshes when we don't actually update any data on them. (such as adding a new socket)
					if(!bMeshModified)
					{
						Mesh->Modify();
						bMeshModified = true;
					}
					Pair.AttachedTo = NewSocketName;
				}
				PersonaPtr.Pin()->RemoveAttachedObjectFromPreviewComponent(Pair.GetAttachedObject(), OldSocketName);
				PersonaPtr.Pin()->AttachObjectToPreviewComponent(Pair.GetAttachedObject(), Pair.AttachedTo);
			}
		}
	}
}

bool SSkeletonTree::IsAddingSocketsAllowed() const
{
	if ( SocketFilter == ESocketFilter::Skeleton ||
		SocketFilter == ESocketFilter::Active ||
		SocketFilter == ESocketFilter::All )
	{
		return true;
	}

	return false;
}

FReply SSkeletonTree::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if ( UICommandList->ProcessCommandBindings( InKeyEvent ) )
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SSkeletonTree::OnDeleteSelectedRows()
{
	FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

	if(TreeSelection.HasSelectedOfType(ESkeletonTreeRowType::AttachedAsset) || TreeSelection.HasSelectedOfType(ESkeletonTreeRowType::Socket))
	{
		FScopedTransaction Transaction( LOCTEXT( "SkeletonTreeDeleteSelected", "Delete selected sockets/meshes/bones from skeleton tree" ) );

		DeleteAttachedAssets( TreeSelection.SelectedAssets );
		DeleteSockets( TreeSelection.SelectedSockets );

		CreateFromSkeleton( TargetSkeleton->GetBoneTree() );
	}
}

void SSkeletonTree::DeleteAttachedAssets( TArray<TSharedPtr<FDisplayedAttachedAssetInfo>> InDisplayedAttachedAssetInfos )
{
	TargetSkeleton->Modify();

	for(auto Iter = InDisplayedAttachedAssetInfos.CreateIterator(); Iter; ++Iter)
	{
		TSharedPtr<FDisplayedAttachedAssetInfo> AttachedAssetInfo = (*Iter);

		UObject* Asset = AttachedAssetInfo->GetAsset();
		const FName& AttachedTo = AttachedAssetInfo->GetParentName();

		TargetSkeleton->PreviewAttachedAssetContainer.RemoveAttachedObject(Asset, AttachedTo);

		if ( PersonaPtr.IsValid() )
		{
			if(USkeletalMesh* Mesh = PersonaPtr.Pin()->GetMesh())
			{
				Mesh->Modify();

				Mesh->PreviewAttachedAssetContainer.RemoveAttachedObject(Asset, AttachedTo);

				PersonaPtr.Pin()->RemoveAttachedObjectFromPreviewComponent(Asset, AttachedTo);
			}
		}
	}
}

void SSkeletonTree::DeleteSockets( TArray<TSharedPtr<FDisplayedSocketInfo>> InDisplayedSocketInfos )
{
	USkeletalMesh* Mesh = NULL;
	if ( PersonaPtr.IsValid() )
	{
		// Reset the sockets of interest in the Preview Mesh so we don't leave a null pointer dangling
		PersonaPtr.Pin()->ClearSelectedSocket();

		PersonaPtr.Pin()->DeselectAll();
		Mesh = PersonaPtr.Pin()->GetMesh();
	}

	for(auto Iter = InDisplayedSocketInfos.CreateIterator(); Iter; ++Iter)
	{
		TSharedPtr<FDisplayedSocketInfo> DisplayedSocketInfo = (*Iter);
		USkeletalMeshSocket* SocketToDelete = static_cast<USkeletalMeshSocket*>( DisplayedSocketInfo->GetData() );

		FName SocketName = SocketToDelete->SocketName;

		if ( DisplayedSocketInfo->GetParentType() == ESocketParentType::Skeleton )
		{
			TargetSkeleton->Modify();

			TargetSkeleton->Sockets.Remove( SocketToDelete );
		}
		else
		{
			if ( Mesh )
			{
				UObject* Object = Mesh->PreviewAttachedAssetContainer.GetAttachedObjectByAttachName( DisplayedSocketInfo->GetRowItemName() );
				if(Object)
				{
					Mesh->Modify();
					Mesh->PreviewAttachedAssetContainer.RemoveAttachedObject( Object, SocketName );
					PersonaPtr.Pin()->RemoveAttachedObjectFromPreviewComponent( Object, SocketName );
				}

				Mesh->GetMeshOnlySocketList().Remove( SocketToDelete );
			}
		}

		// Remove attached assets
		while(UObject* Object = TargetSkeleton->PreviewAttachedAssetContainer.GetAttachedObjectByAttachName( SocketName ))
		{
			TargetSkeleton->Modify();
			TargetSkeleton->PreviewAttachedAssetContainer.RemoveAttachedObject(Object, SocketName);
			PersonaPtr.Pin()->RemoveAttachedObjectFromPreviewComponent(Object, SocketName);
		}
	}
}

void SSkeletonTree::AddAttachedAssets( const FPreviewAssetAttachContainer& AttachedObjects )
{
	for(auto Iter = AttachedObjects.CreateConstIterator(); Iter; ++Iter)
	{
		const FPreviewAttachedObjectPair& Pair = (*Iter);

		if ( !FilterText.IsEmpty() && !Pair.GetAttachedObject()->GetName().Contains( FilterText.ToString() ) )
		{
			continue;
		}

		TSharedRef<FDisplayedAttachedAssetInfo> DisplayInfo = FDisplayedAttachedAssetInfo::Make(Pair.AttachedTo, Pair.GetAttachedObject(), TargetSkeleton, PersonaPtr, SharedThis( this ) );
		DisplayMirror.Add(DisplayInfo);

		// for now it is a failure to not find where the asset is attached. Its possible that this might have to be changed to unloading the asset
		// if there is a valid reason why the attach parent would not exist
		if ( !AttachToParent( DisplayInfo, Pair.AttachedTo, ( ESkeletonTreeRowType::Bone | ESkeletonTreeRowType::Socket ) ) )
		{
			// Just add it to the list if the parent bone isn't currently displayed
			SkeletonRowList.Add( DisplayInfo );
		}
	}
}

void SSkeletonTree::OnChangeShowingRetargetingOptions(ECheckBoxState NewState)
{
	bShowingRetargetingOptions = NewState == ECheckBoxState::Checked;
	CreateTreeColumns();
}

ECheckBoxState SSkeletonTree::IsShowingRetargetingOptions() const
{
	return bShowingRetargetingOptions ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}


#undef LOCTEXT_NAMESPACE
