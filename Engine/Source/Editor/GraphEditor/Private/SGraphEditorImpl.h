// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GraphEditorCommon.h"

#include "GraphEditor.h"
#include "GraphEditorModule.h"
#include "GraphEditorActions.h"
#include "ScopedTransaction.h"
#include "SGraphEditorActionMenu.h"
#include "EdGraphUtilities.h"

/////////////////////////////////////////////////////
// SGraphEditorImpl

class SGraphEditorImpl : public SGraphEditor
{
public:
	SLATE_BEGIN_ARGS( SGraphEditorImpl )
		: _AdditionalCommands( TSharedPtr<FUICommandList>() )
		, _IsEditable(true)
		{}


		SLATE_ARGUMENT(TSharedPtr<FUICommandList>, AdditionalCommands)
		SLATE_ATTRIBUTE( bool, IsEditable )
		SLATE_ARGUMENT( TSharedPtr<SWidget>, TitleBar )
		SLATE_ATTRIBUTE( bool, TitleBarEnabledOnly )
		SLATE_ATTRIBUTE( FGraphAppearanceInfo, Appearance )
		SLATE_ARGUMENT( UEdGraph*, GraphToEdit )
		SLATE_ARGUMENT( UEdGraph*, GraphToDiff )
		SLATE_ARGUMENT(SGraphEditor::FGraphEditorEvents, GraphEvents)
		SLATE_ARGUMENT(bool, AutoExpandActionMenu)
		SLATE_EVENT(FSimpleDelegate, OnNavigateHistoryBack)
		SLATE_EVENT(FSimpleDelegate, OnNavigateHistoryForward)

		/** Show overlay elements for the graph state such as the PIE and read-only borders and text */
		SLATE_ATTRIBUTE(bool, ShowGraphStateOverlay)
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );
private:
	TSharedPtr< FUICommandList > Commands;
	mutable TSet<UEdGraphNode*> SelectedNodeCache;

	/** The panel contains the GraphNode widgets, draws the connections, etc */
	SOverlay::FOverlaySlot* GraphPanelSlot;
	TSharedPtr<SGraphPanel> GraphPanel;
	TSharedPtr<SWidget>	TitleBar;

	UEdGraphPin* GraphPinForMenu;

	/** Do we need to refresh next tick? */
	bool bNeedsRefresh;

	/** Info on the appearance */
	TAttribute<FGraphAppearanceInfo> Appearance;

	SGraphEditor::FOnFocused OnFocused;
	SGraphEditor::FOnCreateActionMenu OnCreateActionMenu;

	TAttribute<bool> IsEditable;
	TAttribute<bool> TitleBarEnabledOnly;

	bool bAutoExpandActionMenu;

	/** Whether to show the state (read only / PIE etc) Overlay on the panel */
	TAttribute<bool> ShowGraphStateOverlay;

	//FOnViewChanged	OnViewChanged;
	TArray< TWeakPtr<SGraphEditor> > LockedGraphs;

	/** Function to check whether PIE is active to display "Simulating" text in graph panel*/
	EVisibility PIENotification( ) const;

	/** Function to check whether we should show read-only text in the panel */
	EVisibility ReadOnlyVisibility() const;

	/** Returns dynamic text, meant to passively instruct the user on what to do in the graph */
	FText GetInstructionText() const;
	/** Function to check whether we should show instruction text to the user */
	EVisibility InstructionTextVisibility() const;
	/** Returns a 0.0 to 1.0 value, denoting the instruction text's fade percent */
	float GetInstructionTextFade() const;
	/** A dynamic tint for the instruction text (allows us to nicely fade it in/out) */
	FLinearColor InstructionTextTint() const;
	/** Determines the color of the box containing the instruction text */
	FSlateColor InstructionBorderColor() const;

	/** Notification list to pass messages to editor users  */
	TSharedPtr<SNotificationList> NotificationListPtr;

	/** Callback to navigate backward in the history */
	FSimpleDelegate OnNavigateHistoryBack;
	/** Callback to navigate forward in the history */
	FSimpleDelegate OnNavigateHistoryForward;

	/** Invoked when a node is created by a keymap */
	FOnNodeSpawnedByKeymap OnNodeSpawnedByKeymap;

public:
	virtual ~SGraphEditorImpl();

	void OnClosedActionMenu();

	bool GraphEd_OnGetGraphEnabled() const;

	FActionMenuContent GraphEd_OnGetContextMenuFor(const FGraphContextMenuArguments& SpawnInfo);

	void GraphEd_OnPanelUpdated();

	// SWidget interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	void FocusLockedEditorHere();

	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent ) override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual bool SupportsKeyboardFocus() const override;
	// End of SWidget interface

	// SGraphEditor interface
	virtual const TSet<class UObject*>& GetSelectedNodes() const override;
	virtual void ClearSelectionSet() override;
	virtual void SetNodeSelection(UEdGraphNode* Node, bool bSelect) override;
	virtual void SelectAllNodes() override;
	virtual FVector2D GetPasteLocation() const override;
	virtual bool IsNodeTitleVisible( const UEdGraphNode* Node, bool bRequestRename ) override;
	virtual void JumpToNode( const UEdGraphNode* JumpToMe, bool bRequestRename = false ) override;
	virtual void JumpToPin( const UEdGraphPin* JumpToMe ) override;
	virtual UEdGraphPin* GetGraphPinForMenu() override;
	virtual void ZoomToFit(bool bOnlySelection) override;
	virtual bool GetBoundsForSelectedNodes( class FSlateRect& Rect, float Padding) override;
	virtual void NotifyGraphChanged();
	virtual TSharedPtr<SWidget> GetTitleBar() const override;
	virtual void SetViewLocation(const FVector2D& Location, float ZoomAmount) override;
	virtual void GetViewLocation(FVector2D& Location, float& ZoomAmount) override;
	virtual void LockToGraphEditor(TWeakPtr<SGraphEditor> Other) override;
	virtual void UnlockFromGraphEditor(TWeakPtr<SGraphEditor> Other) override;
	virtual void AddNotification ( FNotificationInfo& Info, bool bSuccess ) override;
	virtual void SetPinVisibility(SGraphEditor::EPinVisibility Visibility) override;
	// End of SGraphEditor interface
protected:
	//
	// COMMAND HANDLING
	// 
	bool CanReconstructNodes() const;
	bool CanBreakNodeLinks() const;
	bool CanBreakPinLinks() const;

	void ReconstructNodes();
	void BreakNodeLinks();
	void BreakPinLinks(bool bSendNodeNotification);

	// SGraphEditor interface
	virtual void OnGraphChanged( const FEdGraphEditAction& InAction ) override;
	// End of SGraphEditorInterface
private:
	FText GetZoomText() const;

	FSlateColor GetZoomTextColorAndOpacity() const;

	bool IsGraphEditable() const;

	bool IsLocked() const;
};

