// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DialogueSoundWaveProxy.generated.h"

UCLASS()
class UDialogueSoundWaveProxy : public USoundBase
{
	GENERATED_UCLASS_BODY()

	friend class UDialogueWave;

	USoundWave* SoundWave;
	TArray<struct FSubtitleCue> Subtitles;

public:
	/** Returns whether the sound base is set up in a playable manner */
	virtual bool IsPlayable() const;

	/** Returns a pointer to the attenuation settings that are to be applied for this node */
	virtual const FAttenuationSettings* GetAttenuationSettingsToApply() const;

	/** 
	 * Returns the farthest distance at which the sound could be heard
	 */
	virtual float GetMaxAudibleDistance();

	/** 
	 * Returns the length of the sound
	 */
	virtual float GetDuration();

	virtual float GetVolumeMultiplier();
	virtual float GetPitchMultiplier();

	/** 
	 * Parses the Sound to generate the WaveInstances to play
	 */
	virtual void Parse( class FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& AudioComponent, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances );
};