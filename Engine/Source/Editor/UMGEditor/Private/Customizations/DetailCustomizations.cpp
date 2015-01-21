// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"

#include "DetailCustomizations.h"
#include "BlueprintModes/WidgetBlueprintApplicationModes.h"
#include "PropertyEditing.h"
#include "ObjectEditorUtils.h"
#include "WidgetGraphSchema.h"
#include "ScopedTransaction.h"
#include "BlueprintEditorUtils.h"
#include "WidgetGraphSchema.h"
#include "Components/Widget.h"
#include "WidgetBlueprint.h"
#include "K2Node_ComponentBoundEvent.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2_Actions.h"
#include "WidgetBlueprintEditor.h"
#include "Components/PanelSlot.h"
#include "SPropertyBinding.h"

#define LOCTEXT_NAMESPACE "UMG"


class SGraphSchemaActionButton : public SCompoundWidget, public FGCObject
{
public:

	SLATE_BEGIN_ARGS(SGraphSchemaActionButton) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FWidgetBlueprintEditor> InEditor, TSharedPtr<FEdGraphSchemaAction> InClickAction)
	{
		Editor = InEditor;
		Action = InClickAction;

		ChildSlot
		[
			SNew(SButton)
			.ToolTipText(FText::FromString(Action->TooltipDescription))
			.OnClicked(this, &SGraphSchemaActionButton::AddOrViewEventBinding)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(Action->MenuDescription)
			]
		];
	}

private:
	FReply AddOrViewEventBinding()
	{
		UBlueprint* Blueprint = Editor.Pin()->GetBlueprintObj();

		UEdGraph* TargetGraph = nullptr;
		if ( Blueprint->UbergraphPages.Num() > 0 )
		{
			TargetGraph = Blueprint->UbergraphPages[0]; // Just use the first graph
		}

		if ( TargetGraph != nullptr )
		{
			Editor.Pin()->SetCurrentMode(FWidgetBlueprintApplicationModes::GraphMode);

			// Figure out a decent place to stick the node
			const FVector2D NewNodePos = TargetGraph->GetGoodPlaceForNewNode();

			Action->PerformAction(TargetGraph, nullptr, NewNodePos);
		}

		return FReply::Handled();
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Action->AddReferencedObjects(Collector);
	}

private:
	TWeakPtr<FWidgetBlueprintEditor> Editor;

	TSharedPtr<FEdGraphSchemaAction> Action;
};

void FBlueprintWidgetCustomization::CreateEventCustomization( IDetailLayoutBuilder& DetailLayout, UDelegateProperty* Property, UWidget* Widget )
{
	if ( !Property->GetName().EndsWith(TEXT("Event")) )
	{
		return;
	}

	TSharedRef<IPropertyHandle> DelegatePropertyHandle = DetailLayout.GetProperty(Property->GetFName(), CastChecked<UClass>(Property->GetOuter()));

	const bool bHasValidHandle = DelegatePropertyHandle->IsValidHandle();
	if(!bHasValidHandle)
	{
		return;
	}

	IDetailCategoryBuilder& PropertyCategory = DetailLayout.EditCategory(FObjectEditorUtils::GetCategoryFName(Property), FText::GetEmpty(), ECategoryPriority::Uncommon);

	IDetailPropertyRow& PropertyRow = PropertyCategory.AddProperty(DelegatePropertyHandle);
	PropertyRow.OverrideResetToDefault(true, FSimpleDelegate::CreateSP(this, &FBlueprintWidgetCustomization::ResetToDefault_RemoveBinding, DelegatePropertyHandle));

	FString LabelStr = Property->GetName();
	LabelStr.RemoveFromEnd(TEXT("Event"));

	FText Label = FText::FromString(LabelStr);

	const bool bShowChildren = true;
	PropertyRow.CustomWidget(bShowChildren)
		.NameContent()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0,0,5,0)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("GraphEditor.Event_16x"))
			]

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Label)
			]
		]
		.ValueContent()
		.MinDesiredWidth(200)
		.MaxDesiredWidth(250)
		[
			SNew(SPropertyBinding, Editor.Pin().ToSharedRef(), Property, DelegatePropertyHandle)
			.GeneratePureBindings(false)
		];
}

void FBlueprintWidgetCustomization::ResetToDefault_RemoveBinding(TSharedRef<IPropertyHandle> PropertyHandle)
{
	const FScopedTransaction Transaction(LOCTEXT("BindDelegate", "Remove Binding"));

	Blueprint->Modify();

	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	for ( UObject* SelectedObject : OuterObjects )
	{
		FDelegateEditorBinding Binding;
		Binding.ObjectName = SelectedObject->GetName();
		Binding.PropertyName = PropertyHandle->GetProperty()->GetFName();

		Blueprint->Bindings.Remove(Binding);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

void FBlueprintWidgetCustomization::CreateMulticastEventCustomization(IDetailLayoutBuilder& DetailLayout, FName ThisComponentName, UClass* PropertyClass, UMulticastDelegateProperty* DelegateProperty)
{
	const FString AddString = FString(TEXT("Add "));
	const FString ViewString = FString(TEXT("View "));

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	if ( !K2Schema->CanUserKismetAccessVariable(DelegateProperty, PropertyClass, UEdGraphSchema_K2::MustBeDelegate) )
	{
		return;
	}

	FString PropertyTooltip = DelegateProperty->GetToolTipText().ToString();
	if ( PropertyTooltip.IsEmpty() )
	{
		PropertyTooltip = DelegateProperty->GetName();
	}

	// Add on category for delegate property
	const FString EventCategory = FObjectEditorUtils::GetCategory(DelegateProperty);

	UObjectProperty* ComponentProperty = FindField<UObjectProperty>(Blueprint->SkeletonGeneratedClass, ThisComponentName);

	if ( !ComponentProperty )
	{
		return;
	}

	const UK2Node* EventNode = FKismetEditorUtilities::FindBoundEventForComponent(Blueprint, DelegateProperty->GetFName(), ComponentProperty->GetFName());

	TSharedPtr<FEdGraphSchemaAction> ClickAction;

	if ( EventNode )
	{
		TSharedPtr<FEdGraphSchemaAction_K2ViewNode> NewDelegateNode = TSharedPtr<FEdGraphSchemaAction_K2ViewNode>(new FEdGraphSchemaAction_K2ViewNode(EventCategory, FText::FromString(ViewString + DelegateProperty->GetName()), PropertyTooltip, K2Schema->AG_LevelReference));
		NewDelegateNode->NodePtr = EventNode;

		ClickAction = NewDelegateNode;
	}
	else
	{
		TSharedPtr<FEdGraphSchemaAction_K2NewNode> NewDelegateNode = TSharedPtr<FEdGraphSchemaAction_K2NewNode>(new FEdGraphSchemaAction_K2NewNode(EventCategory, FText::FromString(AddString + DelegateProperty->GetName()), PropertyTooltip, K2Schema->AG_LevelReference));

		UK2Node_ComponentBoundEvent* NewComponentEvent = NewObject<UK2Node_ComponentBoundEvent>(Blueprint, UK2Node_ComponentBoundEvent::StaticClass());
		NewComponentEvent->InitializeComponentBoundEventParams(ComponentProperty, DelegateProperty);
		NewDelegateNode->NodeTemplate = NewComponentEvent;
		NewDelegateNode->bGotoNode = true;

		ClickAction = NewDelegateNode;
	}

	TSharedRef<IPropertyHandle> DelegatePropertyHandle = DetailLayout.GetProperty(DelegateProperty->GetFName(), CastChecked<UClass>(DelegateProperty->GetOuter()));

	IDetailCategoryBuilder& PropertyCategory = DetailLayout.EditCategory("Events", LOCTEXT("Events", "Events"), ECategoryPriority::Uncommon);

	FText DelegatePropertyName = FText::FromString(DelegateProperty->GetName());
	PropertyCategory.AddCustomRow(DelegatePropertyName)
	.NameContent()
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0,0,5,0)
		[
			SNew(SImage)
			.Image(FEditorStyle::GetBrush("GraphEditor.Event_16x"))
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(DelegatePropertyName)
		]
	]
	.ValueContent()
	.MinDesiredWidth(200)
	.MaxDesiredWidth(250)
	[
		SNew(SGraphSchemaActionButton, Editor.Pin(), ClickAction)
	];
}

void FBlueprintWidgetCustomization::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	static const FName LayoutCategoryKey(TEXT("Layout"));

	TArray< TWeakObjectPtr<UObject> > OutObjects;
	DetailLayout.GetObjectsBeingCustomized(OutObjects);
	
	if ( OutObjects.Num() == 1 )
	{
		if ( UWidget* Widget = Cast<UWidget>(OutObjects[0].Get()) )
		{
			if ( Widget->Slot )
			{
				UClass* SlotClass = Widget->Slot->GetClass();
				FText LayoutCatName = FText::Format(LOCTEXT("SlotNameFmt", "Slot ({0})"), SlotClass->GetDisplayNameText());

				DetailLayout.EditCategory(LayoutCategoryKey, LayoutCatName, ECategoryPriority::TypeSpecific);
			}
			else
			{
				auto& Category = DetailLayout.EditCategory(LayoutCategoryKey);
				// TODO UMG Hide Category
			}
		}
	}

	PerformBindingCustomization(DetailLayout);
}

void FBlueprintWidgetCustomization::PerformBindingCustomization(IDetailLayoutBuilder& DetailLayout)
{
	TArray< TWeakObjectPtr<UObject> > OutObjects;
	DetailLayout.GetObjectsBeingCustomized(OutObjects);

	if ( OutObjects.Num() == 1 )
	{
		UWidget* Widget = Cast<UWidget>(OutObjects[0].Get());
		UClass* PropertyClass = OutObjects[0].Get()->GetClass();

		for ( TFieldIterator<UProperty> PropertyIt(PropertyClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt )
		{
			UProperty* Property = *PropertyIt;

			if ( UDelegateProperty* DelegateProperty = Cast<UDelegateProperty>(*PropertyIt) )
			{
				if ( DelegateProperty->GetName().EndsWith(TEXT("Event")) )
				{
					CreateEventCustomization(DetailLayout, DelegateProperty, Widget);
				}
			}
			else if ( UMulticastDelegateProperty* MulticastDelegateProperty = Cast<UMulticastDelegateProperty>(Property) )
			{
				CreateMulticastEventCustomization(DetailLayout, OutObjects[0].Get()->GetFName(), PropertyClass, MulticastDelegateProperty);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
