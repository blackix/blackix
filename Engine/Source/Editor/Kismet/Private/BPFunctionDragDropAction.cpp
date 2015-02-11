// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"

#include "BlueprintUtilities.h"
#include "BlueprintEditorUtils.h"
#include "GraphEditorDragDropAction.h"
#include "BPFunctionDragDropAction.h"
#include "K2ActionMenuBuilder.h" // for FK2ActionMenuBuilder::AddSpawnInfoForFunction()

#define LOCTEXT_NAMESPACE "FunctionDragDropAction"

/*******************************************************************************
* Static File Helpers
*******************************************************************************/

/**
 * A default for function node drag-drop operations to use if one wasn't 
 * supplied. Checks to see if a function node is allowed to be placed where it 
 * is currently dragged.
 * 
 * @param  DropActionIn		The action that will be executed when the user drops the dragged item.
 * @param  HoveredGraphIn	A pointer to the graph that the user currently has the item dragged over.
 * @param  ImpededReasonOut	If this returns false, this will be filled out with a reason to present the user with.
 * @param  FunctionIn		The function that the associated node would be calling.
 * @return True is the dragged palette item can be dropped where it currently is, false if not.
 */
static bool CanFunctionBeDropped(TSharedPtr<FEdGraphSchemaAction> /*DropActionIn*/, UEdGraph* HoveredGraphIn, FText& ImpededReasonOut, UFunction const* FunctionIn)
{
	bool bCanBePlaced = true;
	if (HoveredGraphIn == NULL)
	{
		bCanBePlaced = false;
		ImpededReasonOut = LOCTEXT("DropOnlyInGraph", "Nodes can only be placed inside the blueprint graph");
	}
	else if (!HoveredGraphIn->GetSchema()->IsA(UEdGraphSchema_K2::StaticClass()))
	{
		bCanBePlaced = false;
		ImpededReasonOut = LOCTEXT("CannotCreateInThisSchema", "Cannot call functions in this type of graph");
	}
	else if (FunctionIn == NULL)
	{
		bCanBePlaced = false;
		ImpededReasonOut = LOCTEXT("InvalidFuncAction", "Invalid function for placement");
	}
	else if ((HoveredGraphIn->GetSchema()->GetGraphType(HoveredGraphIn) == EGraphType::GT_Function) && FunctionIn->HasMetaData(FBlueprintMetadata::MD_Latent))
	{
		bCanBePlaced = false;
		ImpededReasonOut = LOCTEXT("CannotCreateLatentInGraph", "Cannot call latent functions in function graphs");
	}

	return bCanBePlaced;
}

/**
 * Checks to see if a macro node is allowed to be placed where it is currently 
 * dragged.
 * 
 * @param  DropActionIn		The action that will be executed when the user drops the dragged item.
 * @param  HoveredGraphIn	A pointer to the graph that the user currently has the item dragged over.
 * @param  ImpededReasonOut	If this returns false, this will be filled out with a reason to present the user with.
 * @param  MacroGraph		The macro graph that the node would be executing.
 * @param  bInIsLatentMacro	True if the macro has latent functions in it
 * @return True is the dragged palette item can be dropped where it currently is, false if not.
 */
static bool CanMacroBeDropped(TSharedPtr<FEdGraphSchemaAction> /*DropActionIn*/, UEdGraph* HoveredGraphIn, FText& ImpededReasonOut, UEdGraph* MacroGraph, bool bIsLatentMacro)
{
	bool bCanBePlaced = true;
	if (HoveredGraphIn == NULL)
	{
		bCanBePlaced = false;
		ImpededReasonOut = LOCTEXT("DropOnlyInGraph", "Nodes can only be placed inside the blueprint graph");
	}
	else if (!HoveredGraphIn->GetSchema()->IsA(UEdGraphSchema_K2::StaticClass()))
	{
		bCanBePlaced = false;
		ImpededReasonOut = LOCTEXT("CannotCreateInThisSchema_Macro", "Cannot call macros in this type of graph");
	}
	else if (MacroGraph == HoveredGraphIn)
	{
		bCanBePlaced = false;
		ImpededReasonOut = LOCTEXT("CannotRecurseMacro", "Cannot place a macro instance in its own graph");
	}
	else if( bIsLatentMacro && HoveredGraphIn->GetSchema()->GetGraphType(HoveredGraphIn) == GT_Function)
	{
		bCanBePlaced = false;
		ImpededReasonOut = LOCTEXT("CannotPlaceLatentMacros", "Cannot place a macro instance with latent functions in function graphs!");
	}

	return bCanBePlaced;
}

/*******************************************************************************
* FKismetDragDropAction
*******************************************************************************/

//------------------------------------------------------------------------------
void FKismetDragDropAction::HoverTargetChanged()
{
	UEdGraph* HoveredGraph = GetHoveredGraph();

	FText CannotDropReason = FText::GetEmpty();
	if (ActionWillShowExistingNode())
	{
		FSlateBrush const* ShowsExistingIcon = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.ShowNode"));
		FText DragingText = FText::Format(LOCTEXT("ShowExistingNode", "Show '{0}'"), ActionNode->MenuDescription);
		SetSimpleFeedbackMessage(ShowsExistingIcon, FLinearColor::White, DragingText);
	}
	// it should be obvious that we can't drop on anything but a graph, so no need to point that out
	else if ((HoveredGraph == NULL) || !CanBeDroppedDelegate.IsBound() || CanBeDroppedDelegate.Execute(ActionNode, HoveredGraph, CannotDropReason))
	{
		FGraphSchemaActionDragDropAction::HoverTargetChanged();
	}
	else 
	{
		FSlateBrush const* DropPreventedIcon = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
		SetSimpleFeedbackMessage(DropPreventedIcon, FLinearColor::White, CannotDropReason);
	}
}

//------------------------------------------------------------------------------
FReply FKismetDragDropAction::DroppedOnPanel( const TSharedRef< SWidget >& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph)
{	
	FReply Reply = FReply::Unhandled();

	FText CannotDropReason = FText::GetEmpty();
	if (!CanBeDroppedDelegate.IsBound() || CanBeDroppedDelegate.Execute(ActionNode, GetHoveredGraph(), CannotDropReason))
	{
		Reply = FGraphSchemaActionDragDropAction::DroppedOnPanel(Panel, ScreenPosition, GraphPosition, Graph);
	}

	if (Reply.IsEventHandled())
	{
		AnalyticCallback.ExecuteIfBound();
	}

	return Reply;
}

//------------------------------------------------------------------------------
bool FKismetDragDropAction::ActionWillShowExistingNode() const
{
	bool bWillFocusOnExistingNode = false;

	UEdGraph* HoveredGraph = GetHoveredGraph();
	if (ActionNode.IsValid() && (HoveredGraph != NULL))
	{
		bWillFocusOnExistingNode = (ActionNode->GetTypeId() == FEdGraphSchemaAction_K2TargetNode::StaticGetTypeId()) ||
			(ActionNode->GetTypeId() == FEdGraphSchemaAction_K2Event::StaticGetTypeId()) ||
			(ActionNode->GetTypeId() == FEdGraphSchemaAction_K2InputAction::StaticGetTypeId());

		if (!bWillFocusOnExistingNode && (ActionNode->GetTypeId() == FEdGraphSchemaAction_K2AddEvent::StaticGetTypeId()))
		{
			FEdGraphSchemaAction_K2AddEvent* AddEventAction = (FEdGraphSchemaAction_K2AddEvent*)ActionNode.Get();
			bWillFocusOnExistingNode = AddEventAction->EventHasAlreadyBeenPlaced(FBlueprintEditorUtils::FindBlueprintForGraph(HoveredGraph));
		}
	}

	return bWillFocusOnExistingNode;
}

/*******************************************************************************
* FKismetFunctionDragDropAction
*******************************************************************************/

//------------------------------------------------------------------------------
TSharedRef<FKismetFunctionDragDropAction> FKismetFunctionDragDropAction::New(
	FName                   InFunctionName, 
	UClass*                 InOwningClass, 
	FMemberReference const& InCallOnMember, 
	FNodeCreationAnalytic   AnalyticCallback, 
	FCanBeDroppedDelegate   CanBeDroppedDelegate /* = FCanBeDroppedDelegate() */)
{
	TSharedRef<FKismetFunctionDragDropAction> Operation = MakeShareable(new FKismetFunctionDragDropAction);
	Operation->FunctionName     = InFunctionName;
	Operation->OwningClass      = InOwningClass;
	Operation->CallOnMember     = InCallOnMember;
	Operation->AnalyticCallback = AnalyticCallback;
	Operation->CanBeDroppedDelegate = CanBeDroppedDelegate;

	if (!CanBeDroppedDelegate.IsBound())
	{
		Operation->CanBeDroppedDelegate = FCanBeDroppedDelegate::CreateStatic(&CanFunctionBeDropped, Operation->GetFunctionProperty());
	}

	Operation->Construct();
	return Operation;
}

//------------------------------------------------------------------------------
FKismetFunctionDragDropAction::FKismetFunctionDragDropAction()
	: OwningClass(NULL)
{

}

//------------------------------------------------------------------------------
void FKismetFunctionDragDropAction::HoverTargetChanged()
{
	FGraphActionListBuilderBase::ActionGroup DropActionSet(TSharedPtr<FEdGraphSchemaAction>(NULL));
	GetDropAction(DropActionSet);

	if (DropActionSet.Actions.Num() > 0)
	{
		ActionNode = DropActionSet.Actions[0];
	}
	else 
	{
		ActionNode = NULL;
	}
	
	FKismetDragDropAction::HoverTargetChanged();
}

//------------------------------------------------------------------------------
FReply FKismetFunctionDragDropAction::DroppedOnPanel(TSharedRef<SWidget> const& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph)
{	
	FReply Reply = FReply::Unhandled();

	FGraphActionListBuilderBase::ActionGroup DropActionSet(TSharedPtr<FEdGraphSchemaAction>(NULL));
	GetDropAction(DropActionSet);

	if (DropActionSet.Actions.Num() > 0)
	{
		// we really only expect there to be one action
		TSharedPtr<FEdGraphSchemaAction> FirstDropAction = DropActionSet.Actions[0];

		FText CannotDropReason = FText::GetEmpty();
		if (!CanBeDroppedDelegate.IsBound() || CanBeDroppedDelegate.Execute(FirstDropAction, GetHoveredGraph(), CannotDropReason))
		{
			UFunction const* Function = GetFunctionProperty();
			if ((Function != NULL) && UEdGraphSchema_K2::CanUserKismetCallFunction(Function))
			{
				AnalyticCallback.ExecuteIfBound();

				TArray<UEdGraphPin*> DummyPins;
				DropActionSet.PerformAction(&Graph, DummyPins, GraphPosition);

				Reply = FReply::Handled();
			}
		}
	}

	return Reply;
}

//------------------------------------------------------------------------------
UFunction const* FKismetFunctionDragDropAction::GetFunctionProperty() const
{
	check(OwningClass != NULL);
	check(FunctionName != NAME_None);

	UFunction* Function = FindField<UFunction>(OwningClass, FunctionName);
	return Function;
}

//------------------------------------------------------------------------------
void FKismetFunctionDragDropAction::GetDropAction(FGraphActionListBuilderBase::ActionGroup& DropActionOut) const
{
	if (UEdGraph const* const HoveredGraph = GetHoveredGraph())
	{
		if (UBlueprint* DropOnBlueprint = FBlueprintEditorUtils::FindBlueprintForGraph(HoveredGraph))
		{
			// make temp list builder
			FGraphActionListBuilderBase TempListBuilder;
			TempListBuilder.OwnerOfTemporaries = NewObject<UEdGraph>(DropOnBlueprint);
			TempListBuilder.OwnerOfTemporaries->SetFlags(RF_Transient);

			UEdGraphSchema_K2 const* K2Schema = GetDefault<UEdGraphSchema_K2>();
			check(K2Schema != NULL);

			if (UFunction const* Function = GetFunctionProperty())
			{
				// Use schema function to make 'spawn action'
				FK2ActionMenuBuilder::AddSpawnInfoForFunction(Function, false, FFunctionTargetInfo(), CallOnMember, TEXT(""), K2Schema->AG_LevelReference, TempListBuilder);
				// we expect a single action
				if (ensure(TempListBuilder.GetNumActions() == 1))
				{
					DropActionOut = TempListBuilder.GetAction(0);
				}
			}
		}
	}
}

/*******************************************************************************
* FKismetMacroDragDropAction
*******************************************************************************/

//------------------------------------------------------------------------------
TSharedRef<FKismetMacroDragDropAction> FKismetMacroDragDropAction::New(
	FName                 InMacroName, 
	UBlueprint*           InBlueprint, 
	UEdGraph*             InMacro, 
	FNodeCreationAnalytic AnalyticCallback)
{
	TSharedRef<FKismetMacroDragDropAction> Operation = MakeShareable(new FKismetMacroDragDropAction);
	Operation->MacroName = InMacroName;
	Operation->Macro = InMacro;
	Operation->Blueprint = InBlueprint;
	Operation->AnalyticCallback = AnalyticCallback;

	// Check to see if the macro has any latent functions in it, some graph types do not allow for latent functions
	bool bIsLatentMacro = FBlueprintEditorUtils::CheckIfGraphHasLatentFunctions(Operation->Macro);

	Operation->CanBeDroppedDelegate = FCanBeDroppedDelegate::CreateStatic(&CanMacroBeDropped, InMacro, bIsLatentMacro);

	Operation->Construct();
	return Operation;
}

//------------------------------------------------------------------------------
FKismetMacroDragDropAction::FKismetMacroDragDropAction()
	: Macro(NULL)
{

}

//------------------------------------------------------------------------------
void FKismetMacroDragDropAction::HoverTargetChanged() 
{
	UEdGraph* HoveredGraph = GetHoveredGraph();

	FText CannotDropReason = FText::GetEmpty();
	// it should be obvious that we can't drop on anything but a graph, so no need to point that out
	if ((HoveredGraph == NULL) || !CanBeDroppedDelegate.IsBound() || CanBeDroppedDelegate.Execute(ActionNode, HoveredGraph, CannotDropReason))
	{
		FSlateBrush const* DropPreventedIcon = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.NewNode"));
		SetSimpleFeedbackMessage(DropPreventedIcon, FLinearColor::White, FText::FromName(MacroName));
	}
	else 
	{
		FSlateBrush const* DropPreventedIcon = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
		SetSimpleFeedbackMessage(DropPreventedIcon, FLinearColor::White, CannotDropReason);
	}
}

//------------------------------------------------------------------------------
FReply FKismetMacroDragDropAction::DroppedOnPanel(TSharedRef<SWidget> const& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph)
{	
	check(Macro);
	check(CanBeDroppedDelegate.IsBound());

	FReply Reply = FReply::Unhandled();

	FText CannotDropReason = FText::GetEmpty();
	if (CanBeDroppedDelegate.Execute(TSharedPtr<FEdGraphSchemaAction>(), &Graph, CannotDropReason))
	{
		UK2Node_MacroInstance* MacroTemplate = NewObject<UK2Node_MacroInstance>();
		MacroTemplate->SetMacroGraph(Macro);
		AnalyticCallback.ExecuteIfBound();
		
		FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_MacroInstance>(&Graph, MacroTemplate, GraphPosition);
		Reply = FReply::Handled();
	}
	return Reply;
}

#undef LOCTEXT_NAMESPACE
