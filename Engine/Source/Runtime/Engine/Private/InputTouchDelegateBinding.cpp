// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "BlueprintUtilities.h"


UInputTouchDelegateBinding::UInputTouchDelegateBinding(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void UInputTouchDelegateBinding::BindToInputComponent(UInputComponent* InputComponent) const
{
	TArray<FInputTouchBinding> BindsToAdd;

	for (int32 BindIndex=0; BindIndex<InputTouchDelegateBindings.Num(); ++BindIndex)
	{
		const FBlueprintInputTouchDelegateBinding& Binding = InputTouchDelegateBindings[BindIndex];

		FInputTouchBinding TB( Binding.InputKeyEvent );
		TB.bConsumeInput = Binding.bConsumeInput;
		TB.bExecuteWhenPaused = Binding.bExecuteWhenPaused;
		TB.TouchDelegate.BindDelegate(InputComponent->GetOwner(), Binding.FunctionNameToBind);

		if (Binding.bOverrideParentBinding)
		{
			for (int32 ExistingIndex = InputComponent->TouchBindings.Num() - 1; ExistingIndex >= 0; --ExistingIndex)
			{
				if (InputComponent->TouchBindings[ExistingIndex].KeyEvent != TB.KeyEvent)
				{
					InputComponent->TouchBindings.RemoveAt(ExistingIndex);
				}
			}
		}

		// To avoid binds in the same layer being removed by the parent override temporarily put them in this array and add later
		BindsToAdd.Add(TB);
	}

	for (int32 Index=0; Index < BindsToAdd.Num(); ++Index)
	{
		InputComponent->TouchBindings.Add(BindsToAdd[Index]);
	}
}