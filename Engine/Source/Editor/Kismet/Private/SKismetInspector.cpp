// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "BlueprintEditorPrivatePCH.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GraphEditor.h"
#include "BlueprintUtilities.h"
#include "AnimGraphDefinitions.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "SKismetInspector.h"
#include "SKismetLinearExpression.h"

#include "Editor/PropertyEditor/Public/PropertyEditing.h"

#include "SMyBlueprint.h"
#include "BlueprintDetailsCustomization.h"
#include "UserDefinedEnumEditor.h"
#include "UserDefinedStructureEditor.h"
#include "FormatTextDetails.h"
#include "Engine/SCS_Node.h"
#include "Components/ChildActorComponent.h"

#define LOCTEXT_NAMESPACE "KismetInspector"

//////////////////////////////////////////////////////////////////////////
// FKismetSelectionInfo

struct FKismetSelectionInfo
{
public:
	TArray<UActorComponent*> EditableComponentTemplates;
	TArray<UObject*> ObjectsForPropertyEditing;
};

//////////////////////////////////////////////////////////////////////////
// SKismetInspector

TSharedRef<SWidget> SKismetInspector::MakeContextualEditingWidget(struct FKismetSelectionInfo& SelectionInfo, const FShowDetailsOptions& Options)
{
	TSharedRef< SVerticalBox > ContextualEditingWidget = SNew( SVerticalBox );

	if(bShowTitleArea)
	{
		if (SelectedObjects.Num() == 0)
		{
			// Warning about nothing being selected
			ContextualEditingWidget->AddSlot()
			.AutoHeight()
			.HAlign( HAlign_Center )
			.Padding( 2.0f, 14.0f, 2.0f, 2.0f )
			[
				SNew( STextBlock )
				.Text( LOCTEXT("NoNodesSelected", "Select a node to edit details.") )
			];
		}
		else
		{
			// Title of things being edited
			ContextualEditingWidget->AddSlot()
			.AutoHeight()
			.Padding( 2.0f, 0.0f, 2.0f, 0.0f )
			[
				SNew(STextBlock)
				.Text(this, &SKismetInspector::GetContextualEditingWidgetTitle)
			];
		}
	}

	// Show the property editor
	PropertyView->HideFilterArea(Options.bHideFilterArea);
	PropertyView->SetObjects(SelectionInfo.ObjectsForPropertyEditing, Options.bForceRefresh);
	if (SelectionInfo.ObjectsForPropertyEditing.Num())
	{
		ContextualEditingWidget->AddSlot()
		.FillHeight( 0.9f )
		.VAlign( VAlign_Top )
		[
			SNew( SBox )
			.Visibility(this, &SKismetInspector::GetPropertyViewVisibility)
			[
				PropertyView.ToSharedRef()
			]
		];

		if (bShowPublicView)
		{
			ContextualEditingWidget->AddSlot()
			.AutoHeight()
			.VAlign( VAlign_Top )
			[
				SNew(SCheckBox)
				.ToolTipText(LOCTEXT("TogglePublicView", "Toggle Public View"))
				.IsChecked(this, &SKismetInspector::GetPublicViewCheckboxState)
				.OnCheckStateChanged( this, &SKismetInspector::SetPublicViewCheckboxState)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PublicViewCheckboxLabel", "Public View"))
				]
			];
		}
	}

	return ContextualEditingWidget;
}

void SKismetInspector::SetOwnerTab(TSharedRef<SDockTab> Tab)
{
	OwnerTab = Tab;
}

TSharedPtr<SDockTab> SKismetInspector::GetOwnerTab() const
{
	return OwnerTab.Pin();
}

FText SKismetInspector::GetContextualEditingWidgetTitle() const
{
	FText Title = PropertyViewTitle;
	if (Title.IsEmpty())
	{
		if (SelectedObjects.Num() == 1 && SelectedObjects[0].IsValid())
		{
			UObject* Object = SelectedObjects[0].Get();

			if (UEdGraphNode* Node = Cast<UEdGraphNode>(Object))
			{
				Title = Node->GetNodeTitle(ENodeTitleType::ListView);
			}
			else if (USCS_Node* SCSNode = Cast<USCS_Node>(Object))
			{
				if (SCSNode->ComponentTemplate != NULL)
				{
					if (SCSNode->VariableName != NAME_None)
					{
						Title = FText::Format(LOCTEXT("TemplateForFmt", "Template for {0}"), FText::FromName(SCSNode->VariableName));
					}
					else 
					{
						Title = FText::Format(LOCTEXT("Name_TemplateFmt", "{0} Template"), FText::FromString(SCSNode->ComponentTemplate->GetClass()->GetName()));
					}
				}
			}
			else if (UK2Node* K2Node = Cast<UK2Node>(Object))
			{
				// Edit the component template
				if (UActorComponent* Template = K2Node->GetTemplateFromNode())
				{
					Title = FText::Format(LOCTEXT("Name_TemplateFmt", "{0} Template"), FText::FromString(Template->GetClass()->GetName()));
				}
			}

			if (Title.IsEmpty())
			{
				Title = FText::FromString(UKismetSystemLibrary::GetDisplayName(Object));
			}
		}
		else if (SelectedObjects.Num() > 1)
		{
			UClass* BaseClass = NULL;

			for (auto ObjectWkPtrIt = SelectedObjects.CreateConstIterator(); ObjectWkPtrIt; ++ObjectWkPtrIt)
			{
				TWeakObjectPtr<UObject> ObjectWkPtr = *ObjectWkPtrIt;
				if (ObjectWkPtr.IsValid())
				{
					UObject* Object = ObjectWkPtr.Get();
					UClass* ObjClass = Object->GetClass();

					if (UEdGraphNode* Node = Cast<UEdGraphNode>(Object))
					{
						// Hide any specifics of node types; they're all ed graph nodes
						ObjClass = UEdGraphNode::StaticClass();
					}

					// Keep track of the class of objects selected
					if (BaseClass == NULL)
					{
						BaseClass = ObjClass;
					}
					while (!ObjClass->IsChildOf(BaseClass))
					{
						BaseClass = BaseClass->GetSuperClass();
					}
				}
			}

			if (BaseClass)
			{
				Title = FText::Format(LOCTEXT("MultipleObjectsSelectedFmt", "{0} {1} selected"), FText::AsNumber(SelectedObjects.Num()), FText::FromString(BaseClass->GetName() + TEXT("s")));
			}
		}
	}
	return Title;
}

void SKismetInspector::Construct(const FArguments& InArgs)
{
	bShowInspectorPropertyView = true;
	PublicViewState = ECheckBoxState::Unchecked;
	bComponenetDetailsCustomizationEnabled = false;

	BlueprintEditorPtr = InArgs._Kismet2;
	bShowPublicView = InArgs._ShowPublicViewControl;
	bShowTitleArea = InArgs._ShowTitleArea;
	TSharedPtr<FBlueprintEditor> Kismet2 = BlueprintEditorPtr.Pin();

	// Create a property view
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FNotifyHook* NotifyHook = NULL;
	if(InArgs._SetNotifyHook)
	{
		NotifyHook = Kismet2.Get();
	}

	FDetailsViewArgs::ENameAreaSettings NameAreaSettings = InArgs._HideNameArea ? FDetailsViewArgs::HideNameArea : FDetailsViewArgs::ObjectsUseNameArea;
	FDetailsViewArgs DetailsViewArgs( /*bUpdateFromSelection=*/ false, /*bLockable=*/ false, /*bAllowSearch=*/ true, NameAreaSettings, /*bHideSelectionTip=*/ true, /*InNotifyHook=*/ NotifyHook, /*InSearchInitialKeyFocus=*/ false, /*InViewIdentifier=*/ InArgs._ViewIdentifier );

	PropertyView = EditModule.CreateDetailView( DetailsViewArgs );
		
	//@TODO: .IsEnabled( FSlateApplication::Get().GetNormalExecutionAttribute() );
	PropertyView->SetIsPropertyVisibleDelegate( FIsPropertyVisible::CreateSP(this, &SKismetInspector::IsPropertyVisible) );
	PropertyView->SetIsPropertyEditingEnabledDelegate(InArgs._IsPropertyEditingEnabledDelegate);
	UserOnFinishedChangingProperties = InArgs._OnFinishedChangingProperties;

	TWeakPtr<SMyBlueprint> MyBlueprint = Kismet2.IsValid() ? Kismet2->GetMyBlueprintWidget() : InArgs._MyBlueprintWidget;
	
	if( MyBlueprint.IsValid() )
	{
		FOnGetDetailCustomizationInstance LayoutDelegateDetails = FOnGetDetailCustomizationInstance::CreateStatic(&FBlueprintDelegateActionDetails::MakeInstance, MyBlueprint);
		PropertyView->RegisterInstancedCustomPropertyLayout(UMulticastDelegateProperty::StaticClass(), LayoutDelegateDetails);

		// Register function and variable details customization
		FOnGetDetailCustomizationInstance LayoutGraphDetails = FOnGetDetailCustomizationInstance::CreateStatic(&FBlueprintGraphActionDetails::MakeInstance, MyBlueprint);
		PropertyView->RegisterInstancedCustomPropertyLayout(UEdGraph::StaticClass(), LayoutGraphDetails);
		PropertyView->RegisterInstancedCustomPropertyLayout(UK2Node_EditablePinBase::StaticClass(), LayoutGraphDetails);
		PropertyView->RegisterInstancedCustomPropertyLayout(UK2Node_CallFunction::StaticClass(), LayoutGraphDetails);

		FOnGetDetailCustomizationInstance LayoutVariableDetails = FOnGetDetailCustomizationInstance::CreateStatic(&FBlueprintVarActionDetails::MakeInstance, MyBlueprint);
		PropertyView->RegisterInstancedCustomPropertyLayout(UProperty::StaticClass(), LayoutVariableDetails);
		PropertyView->RegisterInstancedCustomPropertyLayout(UK2Node_VariableGet::StaticClass(), LayoutVariableDetails);
		PropertyView->RegisterInstancedCustomPropertyLayout(UK2Node_VariableSet::StaticClass(), LayoutVariableDetails);
	}

	if (Kismet2.IsValid() && Kismet2->IsEditingSingleBlueprint())
	{
		FOnGetDetailCustomizationInstance LayoutOptionDetails = FOnGetDetailCustomizationInstance::CreateStatic(&FBlueprintGlobalOptionsDetails::MakeInstance, BlueprintEditorPtr);
		PropertyView->RegisterInstancedCustomPropertyLayout(UBlueprint::StaticClass(), LayoutOptionDetails);

		FOnGetDetailCustomizationInstance LayoutFormatTextDetails = FOnGetDetailCustomizationInstance::CreateStatic(&FFormatTextDetails::MakeInstance);
		PropertyView->RegisterInstancedCustomPropertyLayout(UK2Node_FormatText::StaticClass(), LayoutFormatTextDetails);

		FOnGetDetailCustomizationInstance LayoutDocumentationDetails = FOnGetDetailCustomizationInstance::CreateStatic(&FBlueprintDocumentationDetails::MakeInstance, BlueprintEditorPtr);
		PropertyView->RegisterInstancedCustomPropertyLayout(UEdGraphNode_Documentation::StaticClass(), LayoutDocumentationDetails);

		FOnGetDetailCustomizationInstance GraphNodeDetails = FOnGetDetailCustomizationInstance::CreateStatic(&FBlueprintGraphNodeDetails::MakeInstance, BlueprintEditorPtr);
		PropertyView->RegisterInstancedCustomPropertyLayout(UEdGraphNode::StaticClass(), GraphNodeDetails);

		PropertyView->RegisterInstancedCustomPropertyLayout(UChildActorComponent::StaticClass(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FChildActorComponentDetails::MakeInstance, BlueprintEditorPtr));
	}

	// Create the border that all of the content will get stuffed into
	ChildSlot
	[
		SNew(SVerticalBox)
		.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("BlueprintInspector")))
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew( ContextualEditingBorderWidget, SBorder )
			.Padding(0)
			.BorderImage( FEditorStyle::GetBrush("NoBorder") )
		]
	];

	// Update based on the current (empty) selection set
	TArray<UObject*> InitialSelectedObjects;
	FKismetSelectionInfo SelectionInfo;
	UpdateFromObjects(InitialSelectedObjects, SelectionInfo, SKismetInspector::FShowDetailsOptions(FText::GetEmpty(), true));
}

void SKismetInspector::EnableComponentDetailsCustomization(bool bEnable)
{
	// An "empty" instanced customization that's intended to override any registered global details customization for
	// the AActor class type. This will be applied -only- when the CDO is selected to the Details view in Components mode.
	class FActorDetailsOverrideCustomization : public IDetailCustomization
	{
	public:
		/** IDetailCustomization interface */
		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override {}

		static TSharedRef<class IDetailCustomization> MakeInstance()
		{
			return MakeShareable(new FActorDetailsOverrideCustomization());
		}
	};

	bComponenetDetailsCustomizationEnabled = bEnable;

	if (bEnable)
	{
		FOnGetDetailCustomizationInstance ActorOverrideDetails = FOnGetDetailCustomizationInstance::CreateStatic(&FActorDetailsOverrideCustomization::MakeInstance);
		PropertyView->RegisterInstancedCustomPropertyLayout(AActor::StaticClass(), ActorOverrideDetails);

		FOnGetDetailCustomizationInstance LayoutComponentDetails = FOnGetDetailCustomizationInstance::CreateStatic(&FBlueprintComponentDetails::MakeInstance, BlueprintEditorPtr);
		PropertyView->RegisterInstancedCustomPropertyLayout(UActorComponent::StaticClass(), LayoutComponentDetails);
	}
	else
	{
		PropertyView->UnregisterInstancedCustomPropertyLayout(AActor::StaticClass());
		PropertyView->UnregisterInstancedCustomPropertyLayout(UActorComponent::StaticClass());
	}
}

/** Update the inspector window to show information on the supplied object */
void SKismetInspector::ShowDetailsForSingleObject(UObject* Object, const FShowDetailsOptions& Options)
{
	TArray<UObject*> PropertyObjects;

	if (Object != NULL)
	{
		PropertyObjects.Add(Object);
	}

	ShowDetailsForObjects(PropertyObjects, Options);
}

void SKismetInspector::ShowDetailsForObjects(const TArray<UObject*>& PropertyObjects, const FShowDetailsOptions& Options)
{
	static bool bIsReentrant = false;
	if ( !bIsReentrant )
	{
		bIsReentrant = true;
		// When the selection is changed, we may be potentially actively editing a property,
		// if this occurs we need need to immediately clear keyboard focus
		if ( FSlateApplication::Get().HasFocusedDescendants(AsShared()) )
		{
			FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Mouse);
		}
		bIsReentrant = false;
	}

	FKismetSelectionInfo SelectionInfo;
	UpdateFromObjects(PropertyObjects, SelectionInfo, Options);
}

void SKismetInspector::AddPropertiesRecursive(UProperty* Property)
{
	if (Property != NULL)
	{
		// Add this property
		SelectedObjectProperties.Add(Property);

		// If this is a struct or an array of structs, recursively add the child properties
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property);
		UStructProperty* StructProperty = Cast<UStructProperty>(Property);
		if(	StructProperty != NULL && 
			StructProperty->Struct != NULL)
		{
			for (TFieldIterator<UProperty> StructPropIt(StructProperty->Struct); StructPropIt; ++StructPropIt)
			{
				UProperty* InsideStructProperty = *StructPropIt;
				AddPropertiesRecursive(InsideStructProperty);
			}
		}
		else if( ArrayProperty && ArrayProperty->Inner->IsA<UStructProperty>() )
		{
			AddPropertiesRecursive(ArrayProperty->Inner);
		}
	}
}

void SKismetInspector::UpdateFromObjects(const TArray<UObject*>& PropertyObjects, struct FKismetSelectionInfo& SelectionInfo, const FShowDetailsOptions& Options)
{
	// If we're using the unified blueprint editor, there's not an explicit point where
	// we ender a kind of component editing mode, so instead, just look at what we're selecting.
	// If we select a component, then enable the customization.
	if ( GetDefault<UEditorExperimentalSettings>()->bUnifiedBlueprintEditor )
	{
		bool bEnableComponentCustomization = false;

		TSharedPtr<FBlueprintEditor> BlueprintEditor = BlueprintEditorPtr.Pin();
		if ( BlueprintEditor.IsValid() )
		{
			if ( BlueprintEditor->CanAccessComponentsMode() )
			{
				for ( UObject* PropertyObject : PropertyObjects )
				{
					if ( PropertyObject->IsA<UActorComponent>() )
					{
						bEnableComponentCustomization = true;
						break;
					}
				}
			}
		}

		EnableComponentDetailsCustomization(bEnableComponentCustomization);
	}

	PropertyView->OnFinishedChangingProperties().Clear();
	PropertyView->OnFinishedChangingProperties().Add( UserOnFinishedChangingProperties );

	if (!Options.bForceRefresh)
	{
		// Early out if the PropertyObjects and the SelectedObjects are the same
		bool bEquivalentSets = (PropertyObjects.Num() == SelectedObjects.Num());
		if (bEquivalentSets)
		{
			// Verify the elements of the sets are equivalent
			for (int32 i = 0; i < PropertyObjects.Num(); i++)
			{
				if (PropertyObjects[i] != SelectedObjects[i].Get())
				{
					bEquivalentSets = false;
					break;
				}
			}
		}

		if (bEquivalentSets)
		{
			return;
		}
	}

	// Proceed to update
	SelectedObjects.Empty();

	for (auto ObjectIt = PropertyObjects.CreateConstIterator(); ObjectIt; ++ObjectIt)
	{
		if (UObject* Object = *ObjectIt)
		{
			if (!Object->IsValidLowLevel())
			{
				ensureMsg(false, TEXT("Object in KismetInspector is invalid, see TTP 281915"));
				continue;
			}

			SelectedObjects.Add(Object);

			if (USCS_Node* SCSNode = Cast<USCS_Node>(Object))
			{
				// Edit the component template
				UActorComponent* NodeComponent = SCSNode->ComponentTemplate;
				if (NodeComponent != NULL)
				{
					SelectionInfo.ObjectsForPropertyEditing.Add(NodeComponent);
					SelectionInfo.EditableComponentTemplates.Add(NodeComponent);
				}
			}
			else if (UK2Node* K2Node = Cast<UK2Node>(Object))
			{
				// Edit the component template if it exists
				if (UActorComponent* Template = K2Node->GetTemplateFromNode())
				{
					SelectionInfo.ObjectsForPropertyEditing.Add(Template);
					SelectionInfo.EditableComponentTemplates.Add(Template);
				}

				// See if we should edit properties of the node
				if (K2Node->ShouldShowNodeProperties())
				{
					SelectionInfo.ObjectsForPropertyEditing.Add(Object);
				}
			}
			else if (UActorComponent* ActorComponent = Cast<UActorComponent>(Object))
			{
				AActor* Owner = ActorComponent->GetOwner();
				if(Owner != NULL && Owner->HasAnyFlags(RF_ClassDefaultObject))
				{
					// We're editing a component that's owned by a CDO, so set the CDO to the property editor (so that propagation works) and then filter to just the component property that we want to edit
					SelectionInfo.ObjectsForPropertyEditing.AddUnique(Owner);
					SelectionInfo.EditableComponentTemplates.Add(ActorComponent);
				}
				else
				{
					// We're editing a component that exists outside of a CDO, so just edit the component instance directly
					SelectionInfo.ObjectsForPropertyEditing.AddUnique(ActorComponent);
				}
			}
			else
			{
				// Editing any UObject*
				SelectionInfo.ObjectsForPropertyEditing.AddUnique(Object);
			}
		}
	}

	// By default, no property filtering
	SelectedObjectProperties.Empty();

	// Add to the property filter list for any editable component templates
	if(SelectionInfo.EditableComponentTemplates.Num())
	{
		for(auto CompIt = SelectionInfo.EditableComponentTemplates.CreateIterator(); CompIt; ++CompIt)
		{
			UActorComponent* EditableComponentTemplate = *CompIt;
			check(EditableComponentTemplate != NULL);

			// Add all properties belonging to the component template class
			for(TFieldIterator<UProperty> PropIt(EditableComponentTemplate->GetClass()); PropIt; ++PropIt)
			{
				UProperty* Property = *PropIt;
				check(Property != NULL);

				AddPropertiesRecursive(Property);
			}

			// Attempt to locate a matching property for the current component template
			for(auto ObjIt = SelectionInfo.ObjectsForPropertyEditing.CreateIterator(); ObjIt; ++ObjIt)
			{
				UObject* Object = *ObjIt;
				check(Object != NULL);

				if(Object != EditableComponentTemplate)
				{
					for(TFieldIterator<UObjectProperty> ObjPropIt(Object->GetClass()); ObjPropIt; ++ObjPropIt)
					{
						UObjectProperty* ObjectProperty = *ObjPropIt;
						check(ObjectProperty != NULL);

						// If the property value matches the current component template, add it as a selected property for filtering
						if(EditableComponentTemplate == ObjectProperty->GetObjectPropertyValue_InContainer(Object))
						{
							SelectedObjectProperties.Add(ObjectProperty);
						}
					}
				}
			}
		}
	}

	PropertyViewTitle = Options.ForcedTitle;
	bShowComponents = Options.bShowComponents;

	// Update our context-sensitive editing widget
	ContextualEditingBorderWidget->SetContent( MakeContextualEditingWidget(SelectionInfo, Options) );
}

bool SKismetInspector::IsPropertyVisible( const FPropertyAndParent& PropertyAndParent ) const
{
	const UProperty& Property = PropertyAndParent.Property;


	// If we are in 'instance preview' - hide anything marked 'disabled edit on instance'
	if ((ECheckBoxState::Checked == PublicViewState) && Property.HasAnyPropertyFlags(CPF_DisableEditOnInstance))
	{
		return false;
	}

	bool bEditOnTemplateDisabled = Property.HasAnyPropertyFlags(CPF_DisableEditOnTemplate);

	if(const UClass* OwningClass = Cast<UClass>(Property.GetOuter()))
	{
		const UBlueprint* BP = BlueprintEditorPtr.IsValid() ? BlueprintEditorPtr.Pin()->GetBlueprintObj() : NULL;
		const bool VariableAddedInCurentBlueprint = (OwningClass->ClassGeneratedBy == BP);

		// If we did not add this var, hide it!
		if(!VariableAddedInCurentBlueprint)
		{
			if (bEditOnTemplateDisabled || Property.GetBoolMetaData(FBlueprintMetadata::MD_Private))
			{
				return false;
			}
		}
	}

	// figure out if this Blueprint variable is an Actor variable
	const UArrayProperty* ArrayProperty = Cast<const UArrayProperty>(&Property);
	const UProperty* TestProperty = ArrayProperty ? ArrayProperty->Inner : &Property;
	const UObjectPropertyBase* ObjectProperty = Cast<const UObjectPropertyBase>(TestProperty);
	bool bIsActorProperty = (ObjectProperty != NULL && ObjectProperty->PropertyClass->IsChildOf(AActor::StaticClass()));

	if (bEditOnTemplateDisabled && bIsActorProperty)
	{
		// Actor variables can't have default values (because Blueprint templates are library elements that can 
		// bridge multiple levels and different levels might not have the actor that the default is referencing).
		return false;
	}

	bool bIsComponent = (ObjectProperty != nullptr && ObjectProperty->PropertyClass->IsChildOf(UActorComponent::StaticClass()));
	if (!bShowComponents && bIsComponent)
	{
		// Don't show sub components properties, thats what selecting components in the component tree is for.
		return false;
	}

	// Filter down to selected properties only if set.
	// If the current property is selected then it is visible or if its parent is selected and the current property did not fail any of the above tests it should be visible.
	if( SelectedObjectProperties.Find( &Property ) || ( PropertyAndParent.ParentProperty && SelectedObjectProperties.Find( PropertyAndParent.ParentProperty ) ) )
	{
		return true;
	}


	return !SelectedObjectProperties.Num();
}

void SKismetInspector::SetPropertyWindowContents(TArray<UObject*> Objects)
{
	if (FSlateApplication::IsInitialized())
	{
		check(PropertyView.IsValid());
		PropertyView->SetObjects(Objects);
	}
}

EVisibility SKismetInspector::GetPropertyViewVisibility() const
{
	return bShowInspectorPropertyView? EVisibility::Visible : EVisibility::Collapsed;
}

ECheckBoxState SKismetInspector::GetPublicViewCheckboxState() const
{
	return PublicViewState;
}

void SKismetInspector::SetPublicViewCheckboxState( ECheckBoxState InIsChecked )
{
	PublicViewState = InIsChecked;

	//reset the details view
	TArray<UObject*> Objs;
	for(auto It(SelectedObjects.CreateIterator());It;++It)
	{
		Objs.Add(It->Get());
	}
	SelectedObjects.Empty();
	
	if(Objs.Num() > 1)
	{
		ShowDetailsForObjects(Objs);
	}
	else if(Objs.Num() == 1)
	{
		ShowDetailsForSingleObject(Objs[0], FShowDetailsOptions(PropertyViewTitle));
	}
	
	BlueprintEditorPtr.Pin()->StartEditingDefaults();
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
