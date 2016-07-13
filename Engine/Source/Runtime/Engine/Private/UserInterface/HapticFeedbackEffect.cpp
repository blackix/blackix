// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
//
#include "EnginePrivate.h"
#include "EngineGlobals.h"
#include "AudioDevice.h"
#include "GameFramework/HapticFeedbackEffect.h"

UHapticFeedbackEffect::UHapticFeedbackEffect(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

void UHapticFeedbackEffect::GetValues(const float EvalTime, FHapticFeedbackValues& Values) const
{
	Values.Amplitude = HapticDetails.Amplitude.GetRichCurveConst()->Eval(EvalTime);
	Values.Frequency = HapticDetails.Frequency.GetRichCurveConst()->Eval(EvalTime);
}

float UHapticFeedbackEffect::GetDuration() const
{
	float AmplitudeMinTime, AmplitudeMaxTime;
	float FrequencyMinTime, FrequencyMaxTime;

	HapticDetails.Amplitude.GetRichCurveConst()->GetTimeRange(AmplitudeMinTime, AmplitudeMaxTime);
	HapticDetails.Frequency.GetRichCurveConst()->GetTimeRange(FrequencyMinTime, FrequencyMaxTime);

	return FMath::Max(AmplitudeMaxTime, FrequencyMaxTime);
}

bool FActiveHapticFeedbackEffect::Update(const float DeltaTime, FHapticFeedbackValues& Values)
{
	if (HapticEffect == nullptr)
	{
		return false;
	}

	const float Duration = HapticEffect->GetDuration();
	PlayTime += DeltaTime;

	if ((PlayTime > Duration) || (Duration == 0.f))
	{
		return false;
	}

	HapticEffect->GetValues(PlayTime, Values);
	Values.Amplitude *= Scale;
	return true;
}
FActiveHapticFeedbackSoundWave::FActiveHapticFeedbackSoundWave(USoundWave* InSoundWave, float InScale, bool InLoop)
	: PlayTime(0.f),
	SoundWave(InSoundWave),
	TargetFrequency(320),
	Loop(InLoop)
{
	Scale = FMath::Clamp(InScale, 0.f, 10.f);
	PrepareSoundWaveBuffer();
}

FActiveHapticFeedbackSoundWave::~FActiveHapticFeedbackSoundWave()
{
	if (HapticBuffer.RawData)
	{
		FMemory::Free(HapticBuffer.RawData);
	}
}

void FActiveHapticFeedbackSoundWave::Update()
{
	if (Loop && HapticBuffer.SamplesSent == HapticBuffer.BufferLength)
	{
		HapticBuffer.SamplesSent = 0;
		HapticBuffer.CurrentPtr = HapticBuffer.RawData;
		HapticBuffer.bFinishedPlaying = false;
	}
}


void FActiveHapticFeedbackSoundWave::PrepareSoundWaveBuffer()
{
	GEngine->GetMainAudioDevice()->Precache(SoundWave, true, false);
//	SoundWave->InitAudioResource(GEngine->GetAudioDevice()->GetRuntimeFormat(SoundWave));
	uint8* PCMData = SoundWave->RawPCMData;
	int32 SampleRate = SoundWave->SampleRate;

	int TargetBufferSize = (SoundWave->RawPCMDataSize * TargetFrequency) / ( SampleRate * 2 ); //2 because we're only using half of the 16bit source PCM buffer
	HapticBuffer.BufferLength = TargetBufferSize;
	HapticBuffer.RawData = (uint8*)FMemory::Malloc(TargetBufferSize * sizeof(*HapticBuffer.RawData));
	HapticBuffer.CurrentPtr = HapticBuffer.RawData;
	HapticBuffer.Frequency = TargetFrequency;

	int previousTargetIndex = -1;
	int currentMin = 0;
	for (int i = 1; i < SoundWave->RawPCMDataSize; i+=2)
	{
		int targetIndex = i * TargetFrequency / (SampleRate * 2);
		int val = PCMData[i];
		if (val & 0x80)
		{
			val = ~val;
		}
		currentMin = FMath::Min(currentMin, val);

		if (targetIndex != previousTargetIndex)
		{
			
			HapticBuffer.RawData[previousTargetIndex] = val*2*Scale;
			previousTargetIndex = targetIndex;
			currentMin = 0;
		}
	}
	
}


