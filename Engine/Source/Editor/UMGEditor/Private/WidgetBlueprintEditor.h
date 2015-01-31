// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/Kismet/Public/BlueprintEditor.h"
#include "ISequencer.h"
#include "PreviewScene.h"
#include "WidgetReference.h"

class ISequencer;
class UWidgetAnimation;
class UUserWidget;

/**
 * widget blueprint editor (extends Blueprint editor)
 */
class FWidgetBlueprintEditor : public FBlueprintEditor
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnSelectedWidgetsChanging)
	DECLARE_MULTICAST_DELEGATE(FOnSelectedWidgetsChanged)

	/** Called after the widget preview has been updated */
	DECLARE_MULTICAST_DELEGATE(FOnWidgetPreviewUpdated)
public:
	FWidgetBlueprintEditor();

	virtual ~FWidgetBlueprintEditor();

	void InitWidgetBlueprintEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode);

	/** FBlueprintEditor interface */
	virtual void Tick(float DeltaTime) override;
	virtual void PostUndo(bool bSuccessful) override;
	virtual void PostRedo(bool bSuccessful) override;

	/** FGCObjectInterface */
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

	/** @return The widget blueprint currently being edited in this editor */
	class UWidgetBlueprint* GetWidgetBlueprintObj() const;

	/** @return The preview widget. */
	UUserWidget* GetPreview() const;

	/** @return The preview scene that owns the preview widget. */
	FPreviewScene* GetPreviewScene();

	/**  */
	bool IsSimulating() const;

	/**  */
	void SetIsSimulating(bool bSimulating);

	/** Causes the preview to be destroyed and a new one to be created next tick */
	void InvalidatePreview();

	/** Immediately rebuilds the preview widget. */
	void RefreshPreview();

	/** Creates a widget reference using the template. */
	FWidgetReference GetReferenceFromTemplate(UWidget* TemplateWidget);

	/** Creates a widget reference using the preview.  Which is used to lookup the stable template pointer. */
	FWidgetReference GetReferenceFromPreview(UWidget* PreviewWidget);

	/** @return The sequencer used to create widget animations */
	TSharedPtr<ISequencer>& GetSequencer();

	/** Changes the currently viewed animation in Sequencer to the new one*/
	void ChangeViewedAnimation( UWidgetAnimation& InAnimationToView );

	/** Updates the current animation if it is invalid */
	const UWidgetAnimation* RefreshCurrentAnimation();

	/** Sets the currently selected set of widgets */
	void SelectWidgets(const TSet<FWidgetReference>& Widgets, bool bAppendOrToggle);

	/** Sets the currently selected set of objects */
	void SelectObjects(const TSet<UObject*>& Objects);

	/** Removes removed widgets from the selection set. */
	void CleanSelection();

	/** @return The selected set of widgets */
	const TSet<FWidgetReference>& GetSelectedWidgets() const;

	/** @return The selected set of Objects */
	const TSet< TWeakObjectPtr<UObject> >& GetSelectedObjects() const;

	/** @return Notification for when the preview widget has been updated */
	FOnWidgetPreviewUpdated& GetOnWidgetPreviewUpdated() { return OnWidgetPreviewUpdated; }

	TSharedPtr<class FWidgetBlueprintEditorToolbar> GetWidgetToolbarBuilder() { return WidgetToolbar; }

	/** Migrate a property change from the preview GUI to the template GUI. */
	void MigrateFromChain(FEditPropertyChain* PropertyThatChanged, bool bIsModify);

	/** Event called when an undo/redo transaction occurs */
	DECLARE_EVENT(FWidgetBlueprintEditor, FOnWidgetBlueprintTransaction)
	FOnWidgetBlueprintTransaction& GetOnWidgetBlueprintTransaction() { return OnWidgetBlueprintTransaction; }

	/** Creates a sequencer widget */
	TSharedRef<SWidget> CreateSequencerWidget();

	/**
	 * The widget we're now hovering over in any particular context, allows multiple views to 
	 * synchronize feedback on where that widget is in their representation.
	 */
	void SetHoveredWidget(FWidgetReference& InHoveredWidget);

	void ClearHoveredWidget();

	FWidgetReference GetHoveredWidget() const;

	float GetHoveredWidgetTime() const;

	void AddPostDesignerLayoutAction(TFunction<void()> Action);

	TArray< TFunction<void()> >& GetQueuedDesignerActions();

public:
	/** Fires whenever the selected set of widgets changing */
	FOnSelectedWidgetsChanged OnSelectedWidgetsChanging;

	/** Fires whenever the selected set of widgets changes */
	FOnSelectedWidgetsChanged OnSelectedWidgetsChanged;

	/** Command list for handling widget actions in the WidgetBlueprintEditor */
	TSharedPtr< FUICommandList > DesignerCommandList;

	/** Paste Metadata */
	FVector2D PasteDropLocation;

protected:
	// Begin FBlueprintEditor
	virtual void RegisterApplicationModes(const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode, bool bNewlyCreated = false) override;
	virtual FGraphAppearanceInfo GetGraphAppearance() const override;
	// End FBlueprintEditor

private:
	bool CanDeleteSelectedWidgets();
	void DeleteSelectedWidgets();

	bool CanCopySelectedWidgets();
	void CopySelectedWidgets();

	bool CanPasteWidgets();
	void PasteWidgets();

	bool CanCutSelectedWidgets();
	void CutSelectedWidgets();

private:
	/** Called whenever the blueprint is structurally changed. */
	virtual void OnBlueprintChangedImpl(UBlueprint* InBlueprint, bool bIsJustBeingCompiled = false ) override;

	/** Called when objects need to be swapped out for new versions, like after a blueprint recompile. */
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap);

	/** Destroy the current preview GUI object */
	void DestroyPreview();

	/** Tick the current preview GUI object */
	void UpdatePreview(UBlueprint* InBlueprint, bool bInForceFullUpdate);

private:
	/** The preview scene that owns the preview GUI */
	FPreviewScene PreviewScene;

	/** Sequencer for creating and previewing widget animations */
	TSharedPtr<ISequencer> Sequencer;

	/** Overlay used to display UI on top of sequencer */
	TWeakPtr<SOverlay> SequencerOverlay;

	/** Manager for handling bindings to sequence animations */
	TSharedPtr<class FUMGSequencerObjectBindingManager> SequencerObjectBindingManager;

	/** The Blueprint associated with the current preview */
	UWidgetBlueprint* PreviewBlueprint;

	/** The currently selected preview widgets in the preview GUI */
	TSet< FWidgetReference > SelectedWidgets;

	/** The currently selected objects in the designer */
	TSet< TWeakObjectPtr<UObject> > SelectedObjects;

	/** The preview GUI object */
	mutable TWeakObjectPtr<UUserWidget> PreviewWidgetPtr;

	/** Notification for when the preview widget has been updated */
	FOnWidgetPreviewUpdated OnWidgetPreviewUpdated;

	/** Delegate called when a undo/redo transaction happens */
	FOnWidgetBlueprintTransaction OnWidgetBlueprintTransaction;

	/** The toolbar builder associated with this editor */
	TSharedPtr<class FWidgetBlueprintEditorToolbar> WidgetToolbar;

	/** The widget references out in the ether that may need to be updated after being issued. */
	TArray< TWeakPtr<FWidgetHandle> > WidgetHandlePool;

	/** The wall clock time the user has been hovering over a single widget */
	float HoverTime;

	/** The current widget being hovered */
	FWidgetReference HoveredWidget;

	/** The preview becomes invalid and needs to be rebuilt on the next tick. */
	bool bPreviewInvalidated;

	/**  */
	bool bIsSimulateEnabled;

	/**  */
	bool bIsRealTime;

	TArray< TFunction<void()> > QueuedDesignerActions;
};
