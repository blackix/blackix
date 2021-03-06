// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CodeAudioEffects.h: Unreal CoreAudio audio effects interface object.
=============================================================================*/

#ifndef _INC_COREAUDIOEFFECTS
#define _INC_COREAUDIOEFFECTS

#define REVERB_ENABLED 1
#define EQ_ENABLED 1
#define RADIO_ENABLED 1

/** 
 * CoreAudio effects manager
 */
class FCoreAudioEffectsManager : public FAudioEffectsManager
{
public:
	FCoreAudioEffectsManager( FAudioDevice* InDevice );
	~FCoreAudioEffectsManager( void );

	/** 
	 * Calls the platform specific code to set the parameters that define reverb
	 */
	virtual void SetReverbEffectParameters( const FAudioReverbEffect& ReverbEffectParameters );

	/** 
	 * Calls the platform specific code to set the parameters that define EQ
	 */
	virtual void SetEQEffectParameters( const FAudioEQEffect& EQEffectParameters );

	/** 
	 * Calls the platform specific code to set the parameters that define a radio effect.
	 * 
	 * @param	RadioEffectParameters	The new parameters for the radio distortion effect. 
	 */
	virtual void SetRadioEffectParameters( const FAudioRadioEffect& RadioEffectParameters );

private:

	bool bRadioAvailable;

	/** Console variables to tweak the radio effect output */
	static TConsoleVariableData<float>*	Radio_ChebyshevPowerMultiplier;
	static TConsoleVariableData<float>*	Radio_ChebyshevPower;
	static TConsoleVariableData<float>*	Radio_ChebyshevCubedMultiplier;
	static TConsoleVariableData<float>*	Radio_ChebyshevMultiplier;

	friend class FCoreAudioDevice;
	friend class FCoreAudioSoundSource;
};

#endif
