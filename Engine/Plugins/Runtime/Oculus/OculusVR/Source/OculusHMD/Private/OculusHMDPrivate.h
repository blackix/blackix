// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IOculusHMDModule.h"
#include "OculusFunctionLibrary.h"
#include "StereoRendering.h"
#include "RunnableThread.h"
#include "RHI.h"
#include <functional>

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#define OCULUS_HMD_SUPPORTED_PLATFORMS_D3D11 PLATFORM_WINDOWS
#define OCULUS_HMD_SUPPORTED_PLATFORMS_D3D12 PLATFORM_WINDOWS
#define OCULUS_HMD_SUPPORTED_PLATFORMS_OPENGL (PLATFORM_WINDOWS || PLATFORM_ANDROID)
#define OCULUS_HMD_SUPPORTED_PLATFORMS_VULKAN (PLATFORM_WINDOWS || PLATFORM_ANDROID)
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS


//-------------------------------------------------------------------------------------------------
// OVRPlugin
//-------------------------------------------------------------------------------------------------

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#endif

#include "OVR_Plugin.h"

#if PLATFORM_WINDOWS
#include "HideWindowsPlatformTypes.h"
#endif

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS

//-------------------------------------------------------------------------------------------------
// Utility functions
//-------------------------------------------------------------------------------------------------

namespace OculusHMD
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	struct FPose
	{
		FQuat Orientation;
		FVector Position;

		FPose() {}
		FPose(const FQuat& InOrientation, const FVector& InPosition) : Orientation(InOrientation), Position(InPosition) {}
	};

	/** Converts ovrpQuatf to FQuat */
	FORCEINLINE FQuat ToFQuat(const ovrpQuatf& InQuat)
	{
		return FQuat(-InQuat.z, InQuat.x, InQuat.y, -InQuat.w);
	}

	/** Converts FQuat to ovrpQuatf */
	FORCEINLINE ovrpQuatf ToOvrpQuatf(const FQuat& InQuat)
	{
		return ovrpQuatf { InQuat.Y, InQuat.Z, -InQuat.X, -InQuat.W };
	}

	/** Converts vector from Oculus to Unreal */
	FORCEINLINE FVector ToFVector(const ovrpVector3f& InVec)
	{
		return FVector(-InVec.z, InVec.x, InVec.y);
	}

	/** Converts vector from Unreal to Oculus. */
	FORCEINLINE ovrpVector3f ToOvrpVector3f(const FVector& InVec)
	{
		return ovrpVector3f { InVec.Y, InVec.Z, -InVec.X };
	}

	FORCEINLINE FMatrix ToFMatrix(const ovrpMatrix4f& vtm)
	{
		// Rows and columns are swapped between ovrpMatrix4f and FMatrix
		return FMatrix(
			FPlane(vtm.M[0][0], vtm.M[1][0], vtm.M[2][0], vtm.M[3][0]),
			FPlane(vtm.M[0][1], vtm.M[1][1], vtm.M[2][1], vtm.M[3][1]),
			FPlane(vtm.M[0][2], vtm.M[1][2], vtm.M[2][2], vtm.M[3][2]),
			FPlane(vtm.M[0][3], vtm.M[1][3], vtm.M[2][3], vtm.M[3][3]));
	}

	FORCEINLINE ovrpRecti ToOvrpRecti(const FIntRect& rect)
	{
		return ovrpRecti { { rect.Min.X, rect.Min.Y }, { rect.Size().X, rect.Size().Y } };
	}

	FORCEINLINE int32 ViewIndexFromStereoPass(const EStereoscopicPass StereoPassType) {
		switch (StereoPassType)
		{
		case eSSP_LEFT_EYE:
		case eSSP_FULL:
			return 0;

		case eSSP_RIGHT_EYE:
			return 1;

		case eSSP_MONOSCOPIC_EYE:
			return 2;

		default:
			check(0);
			return -1;
		}
	}

	/** Helper that converts ovrTrackedDeviceType to ETrackedDeviceType */
	FORCEINLINE ETrackedDeviceType ToETrackedDeviceType(ovrpNode Source)
	{
		ETrackedDeviceType Destination = ETrackedDeviceType::All; // Best attempt at initialization

		switch (Source)
		{
		case ovrpNode_Head:
			Destination = ETrackedDeviceType::HMD;
			break;
		case ovrpNode_HandLeft:
			Destination = ETrackedDeviceType::LTouch;
			break;
		case ovrpNode_HandRight:
			Destination = ETrackedDeviceType::RTouch;
			break;
		default:
			break;
		}
		return Destination;
	}

	/** Helper that converts ETrackedDeviceType to ovrTrackedDeviceType */
	FORCEINLINE ovrpNode ToOvrpNode(ETrackedDeviceType Source)
	{
		ovrpNode Destination = ovrpNode_None; // Best attempt at initialization

		switch (Source)
		{
		case ETrackedDeviceType::HMD:
			Destination = ovrpNode_Head;
			break;
		case ETrackedDeviceType::LTouch:
			Destination = ovrpNode_HandLeft;
			break;
		case ETrackedDeviceType::RTouch:
			Destination = ovrpNode_HandRight;
			break;
		default:
			break;
		}
		return Destination;
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS


	/** Check currently executing from Game thread */
	FORCEINLINE bool InGameThread()
	{
		if (GIsGameThreadIdInitialized)
		{
			return FPlatformTLS::GetCurrentThreadId() == GGameThreadId;
		}
		else
		{
			return true;
		}
	}

	FORCEINLINE void CheckInGameThread()
	{
#if DO_CHECK
		check(InGameThread());
#endif
	}


	/** Check currently executing from Render thread */
	FORCEINLINE bool InRenderThread()
	{
		if (GRenderingThread && !GIsRenderingThreadSuspended)
		{
			return FPlatformTLS::GetCurrentThreadId() == GRenderingThread->GetThreadID();
		}
		else
		{
			return InGameThread();
		}
	}

	FORCEINLINE void CheckInRenderThread()
	{
#if DO_CHECK
		check(InRenderThread());
#endif
	}


	/** Check currently executing from RHI thread */
	FORCEINLINE bool InRHIThread()
	{
		if (GRenderingThread && !GIsRenderingThreadSuspended)
		{
			if (GRHIThread)
			{
				return FPlatformTLS::GetCurrentThreadId() == GRHIThread->GetThreadID();
			}
			else
			{
				return FPlatformTLS::GetCurrentThreadId() == GRenderingThread->GetThreadID();
			}
		}
		else
		{
			return InGameThread();
		}
	}

	FORCEINLINE void CheckInRHIThread()
	{
#if DO_CHECK
		check(InRHIThread());
#endif
	}


	/** Called from Game thread to execute a function on the Render thread. */
	void ExecuteOnRenderThread(const std::function<void()>& Function);
	void ExecuteOnRenderThread_DoNotWait(const std::function<void()>& Function);
	void ExecuteOnRenderThread(const std::function<void(FRHICommandListImmediate&)>& Function);
	void ExecuteOnRenderThread_DoNotWait(const std::function<void(FRHICommandListImmediate&)>& Function);

	/** Called from Render thread to execute a function on the RHI thread. */
	void ExecuteOnRHIThread(const std::function<void()>& Function);
	void ExecuteOnRHIThread_DoNotWait(const std::function<void()>& Function);
	void ExecuteOnRHIThread(const std::function<void(FRHICommandList&)>& Function);
	void ExecuteOnRHIThread_DoNotWait(const std::function<void(FRHICommandList&)>& Function);

#if OCULUS_HMD_SUPPORTED_PLATFORMS
	/** Tests if Oculus service is running */
	bool IsOculusServiceRunning();

	/** Tests if Oculus service is running and HMD is connected */
	bool IsOculusHMDConnected();
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS

} // namespace OculusHMD


//-------------------------------------------------------------------------------------------------
// Logging and Stats
//-------------------------------------------------------------------------------------------------

#if OCULUS_HMD_SUPPORTED_PLATFORMS
DEFINE_LOG_CATEGORY_STATIC(LogHMD, Log, All);
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
