// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneFadeSection.h"
#include "MovieSceneFadeTrack.h"
#include "MovieSceneFadeTrackInstance.h"


/* FMovieSceneFadeTrackInstance structors
 *****************************************************************************/

FMovieSceneFadeTrackInstance::FMovieSceneFadeTrackInstance(UMovieSceneFadeTrack& InFadeTrack)
	: FadeTrack(&InFadeTrack)
{ }


/* IMovieSceneTrackInstance interface
 *****************************************************************************/

void FMovieSceneFadeTrackInstance::Update(float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player)
{
	FLinearColor FadeColor = FLinearColor::Black;
	bool bFadeAudio = false;

	float FloatValue = 0.0f;

	if (FadeTrack->Eval(Position, LastPosition, FloatValue))
	{
		const UMovieSceneSection* NearestSection = MovieSceneHelpers::FindNearestSectionAtTime(FadeTrack->GetAllSections(), Position);
		const UMovieSceneFadeSection* FadeSection = CastChecked<const UMovieSceneFadeSection>(NearestSection);
		if (FadeSection)
		{
			FadeColor = FadeSection->FadeColor;
			bFadeAudio = FadeSection->bFadeAudio;
		}

		// Set editor preview/fade
		EMovieSceneViewportParams ViewportParams;
		ViewportParams.SetWhichViewportParam = (EMovieSceneViewportParams::SetViewportParam)(EMovieSceneViewportParams::SVP_FadeAmount | EMovieSceneViewportParams::SVP_FadeColor);
		ViewportParams.FadeAmount = FloatValue;
		ViewportParams.FadeColor = FadeColor;

		TMap<FViewportClient*, EMovieSceneViewportParams> ViewportParamsMap;
		Player.GetViewportSettings(ViewportParamsMap);
		for (auto ViewportParamsPair : ViewportParamsMap)
		{
			ViewportParamsMap[ViewportParamsPair.Key] = ViewportParams;
		}
		Player.SetViewportSettings(ViewportParamsMap);

		// Set runtime fade
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
			{
				UWorld* World = Context.World();
				if (World != nullptr)
				{
					APlayerController* PlayerController = World->GetGameInstance()->GetFirstLocalPlayerController();
					if (PlayerController != nullptr && PlayerController->PlayerCameraManager && !PlayerController->PlayerCameraManager->IsPendingKill())
					{
						PlayerController->PlayerCameraManager->SetManualCameraFade(FloatValue, FadeColor, bFadeAudio);
					}
				}
			}
		}
	}
}
