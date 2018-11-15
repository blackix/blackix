#include "OculusMR_State.h"
#include "OculusMRFunctionLibrary.h"
#include "OVR_Plugin_MixedReality.h"

UOculusMR_State::UOculusMR_State(const FObjectInitializer& ObjectInitializer)
	: TrackedCamera()
	, TrackingReferenceComponent(nullptr)
	, CurrentCapturingCamera(ovrpCameraDevice_None)
	, ChangeCameraStateRequested(false)
	, BindToTrackedCameraIndexRequested(false)
{}