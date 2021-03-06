// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimMontageFactory.cpp: Factory for AnimMontages
=============================================================================*/

#include "UnrealEd.h"

#include "AssetData.h"
#include "ContentBrowserModule.h"

#define LOCTEXT_NAMESPACE "AnimMontageFactory"

UAnimMontageFactory::UAnimMontageFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	bCreateNew = true;
	SupportedClass = UAnimMontage::StaticClass();
}

bool UAnimMontageFactory::ConfigureProperties()
{
	// Null the skeleton so we can check for selection later
	TargetSkeleton = NULL;
	SourceAnimation = NULL;

	// Load the content browser module to display an asset picker
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	FAssetPickerConfig AssetPickerConfig;
	
	/** The asset picker will only show skeletons */
	AssetPickerConfig.Filter.ClassNames.Add(USkeleton::StaticClass()->GetFName());
	AssetPickerConfig.Filter.bRecursiveClasses = true;

	/** The delegate that fires when an asset was selected */
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateUObject(this, &UAnimMontageFactory::OnTargetSkeletonSelected);

	/** The default view mode should be a list view */
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;

	/** The default scale for thumbnails. [0-1] range */
	AssetPickerConfig.ThumbnailScale = 0.25f;

	PickerWindow = SNew(SWindow)
	.Title(LOCTEXT("CreateAnimMontageOptions", "Pick Skeleton"))
	.ClientSize(FVector2D(500, 600))
	.SupportsMinimize(false) .SupportsMaximize(false)
	[
		SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush("Menu.Background") )
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		]
	];

	GEditor->EditorAddModalWindow(PickerWindow.ToSharedRef());
	PickerWindow.Reset();

	return TargetSkeleton != NULL;
}

UObject* UAnimMontageFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	if (TargetSkeleton || SourceAnimation)
	{
		UAnimMontage* AnimMontage = ConstructObject<UAnimMontage>(Class,InParent,Name,Flags);

		if(SourceAnimation)
		{
			USkeleton * SourceSkeleton = SourceAnimation->GetSkeleton();
			//Make sure we haven't asked to create an AnimComposite with mismatching skeletons
			check(TargetSkeleton == NULL || TargetSkeleton == SourceSkeleton);
			TargetSkeleton = SourceSkeleton;

			FAnimSegment NewSegment;
			NewSegment.AnimReference = SourceAnimation;
			NewSegment.AnimStartTime = 0.f;
			NewSegment.AnimEndTime = SourceAnimation->SequenceLength;
			NewSegment.AnimPlayRate = 1.f;
			NewSegment.LoopingCount = 1;
			NewSegment.StartPos = 0.f;

			FSlotAnimationTrack NewTrack;
			NewTrack.AnimTrack.AnimSegments.Add(NewSegment);

			NewTrack.SlotName = FName("");
			AnimMontage->SlotAnimTracks.Add( NewTrack );
			AnimMontage->SetSequenceLength(SourceAnimation->SequenceLength);
		}

		AnimMontage->SetSkeleton(TargetSkeleton);

		return AnimMontage;
	}

	return NULL;
}

void UAnimMontageFactory::OnTargetSkeletonSelected(const FAssetData& SelectedAsset)
{
	TargetSkeleton = Cast<USkeleton>(SelectedAsset.GetAsset());
	PickerWindow->RequestDestroyWindow();
}

#undef LOCTEXT_NAMESPACE
