// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_PHYSX

#pragma warning( push )
#pragma warning( disable : 4946 ) // reinterpret_cast used between related classes

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#endif

#if USING_CODE_ANALYSIS
	#pragma warning( push )
	#pragma warning( disable : ALL_CODE_ANALYSIS_WARNINGS )
#endif	// USING_CODE_ANALYSIS

#pragma pack(push,8)

#include "Px.h"
#include "PxPhysicsAPI.h"
#include "PxRenderBuffer.h"
#include "PxExtensionsAPI.h"
#include "PxVisualDebuggerExt.h"
//#include "PxDefaultCpuDispatcher.h"

// vehicle related header files
//#include "PxVehicleSDK.h"
//#include "PxVehicleUtils.h"

// utils
#include "PxGeometryQuery.h"
#include "PxMeshQuery.h"
#include "PxTriangle.h"

#pragma pack(pop)

#if USING_CODE_ANALYSIS
	#pragma warning( pop )
#endif	// USING_CODE_ANALYSIS

#pragma warning( pop )

#ifdef __clang__
#pragma clang diagnostic pop
#endif

using namespace physx;

/** Utility wrapper for a uint8 TArray for saving into PhysX. */
class FPhysXOutputStream : public PxOutputStream
{
public:
	/** Raw byte data */
	TArray<uint8>			*Data;

	FPhysXOutputStream()
		: Data(NULL)
	{}

	FPhysXOutputStream(TArray<uint8> *InData)
		: Data(InData)
	{}

	virtual PxU32 write(const void* Src, PxU32 Count) OVERRIDE
	{
		check(Data);
		check(Src);
		int32 CurrentNum = (*Data).Num();
		(*Data).AddUninitialized(Count);
		FMemory::Memcpy( &(*Data)[CurrentNum], Src, Count );
		return Count;
	}
};

#endif // WITH_PHYSX
