// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "BlueprintEditorPrivatePCH.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "Editor/UnrealEd/Public/Kismet2/KismetDebugUtilities.h"
#include "BlueprintUtilities.h"
#include "AnimGraphDefinitions.h"
#include "KismetDebugCommands.h"

//////////////////////////////////////////////////////////////////////////
// FDebuggingActionCallbacks

void FDebuggingActionCallbacks::ClearWatches(UBlueprint* Blueprint)
{
	FKismetDebugUtilities::ClearPinWatches(Blueprint);
}

void FDebuggingActionCallbacks::ClearWatch(UEdGraphPin* WatchedPin)
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(WatchedPin->GetOwningNode());
	if (Blueprint != NULL)
	{
		FKismetDebugUtilities::RemovePinWatch(Blueprint, WatchedPin);
	}
}

void FDebuggingActionCallbacks::ClearBreakpoints(UBlueprint* OwnerBlueprint)
{
	FKismetDebugUtilities::ClearBreakpoints(OwnerBlueprint);
}

void FDebuggingActionCallbacks::ClearBreakpoint(UBreakpoint* Breakpoint, UBlueprint* OwnerBlueprint)
{
	FKismetDebugUtilities::StartDeletingBreakpoint(Breakpoint, OwnerBlueprint);
}

void FDebuggingActionCallbacks::SetBreakpointEnabled(UBreakpoint* Breakpoint, bool bEnabled)
{
	FKismetDebugUtilities::SetBreakpointEnabled(Breakpoint, bEnabled);
}

void FDebuggingActionCallbacks::SetEnabledOnAllBreakpoints(UBlueprint* OwnerBlueprint, bool bShouldBeEnabled)
{
	for (TArray<UBreakpoint*>::TIterator BreakpointIt(OwnerBlueprint->Breakpoints); BreakpointIt; ++BreakpointIt)
	{
		UBreakpoint* BP = *BreakpointIt;
		FKismetDebugUtilities::SetBreakpointEnabled(BP, bShouldBeEnabled);
	}
}
