// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class IHapticDevice
{
public:
	/**
	 * Sets the frequency / amplitude of the haptic channel of a controller.
	 *
	 * @param ControllerId	Id of the controller to set haptic feedback parameters on
	 * @param Hand			Which hand, if supported, to add the haptic feedback to (corresponds to EControllerHand)
	 * @param Values		The values to haptic parameter values (e.g. frequency and amplitude) set for the device
	 */
	virtual void SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values) = 0;

	/**
	* Sets the frequency and amplitude of haptic feedback channels for a given controller id.
	* Some devices / platforms may support just haptics, or just force feedback.
	*
	* @param ControllerId	ID of the controller to issue haptic feedback for
	* @param HandId			Which hand id (e.g. left or right) to issue the feedback for.  These usually correspond to EControllerHands
	* @param Buffer			Haptics buffer to play
	*/
	virtual void SetHapticFeedbackBuffer(int32 ControllerId, int32 Hand, FHapticFeedbackBuffer& Buffer) {};

	/**
	 * Determines the valid range of frequencies this haptic device supports, to limit input ranges from UHapticFeedbackEffects
	 */
	virtual void GetHapticFrequencyRange(float& MinFrequency, float& MaxFrequency) const = 0;

	/**
	 * Returns the scaling factor to map the amplitude of UHapticFeedbackEvents from [0.0, 1.0] to the actual range handled by the device
	 */
	virtual float GetHapticAmplitudeScale() const = 0;
};
