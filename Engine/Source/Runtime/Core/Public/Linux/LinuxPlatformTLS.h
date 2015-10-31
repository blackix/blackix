// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	LinuxPlatformTLS.h: Linux platform TLS (Thread local storage and thread ID) functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformTLS.h"
#include "Linux/LinuxSystemIncludes.h"

#if defined(_GNU_SOURCE)
	#include <sys/syscall.h>	// SYS_gettid
#endif // _GNU_SOURCE

/**
 * Linux implementation of the TLS OS functions
 */
struct CORE_API FLinuxTLS : public FGenericPlatformTLS
{
	/**
	 * Returns the currently executing thread's id
	 */
	static FORCEINLINE uint32 GetCurrentThreadId(void)
	{
		// note: cannot use pthread_self() without updating the rest of API to opaque (or at least 64-bit) thread handles
#if defined(_GNU_SOURCE)

    #if IS_MONOLITHIC
        // syscall() is relatively heavy and shows up in the profiler, given that IsInGameThread() is used quite often. Cache thread id in TLS.
        static __thread uint32 ThreadIdTLS = 0;
        if (ThreadIdTLS == 0)
        {
    #else
        uint32 ThreadIdTLS;
        {
    #endif // IS_MONOLITHIC
            pid_t ThreadId = static_cast<pid_t>(syscall(SYS_gettid));
            static_assert(sizeof(pid_t) <= sizeof(uint32), "pid_t is larger than uint32, reconsider implementation of GetCurrentThreadId()");
            ThreadIdTLS = static_cast<uint32>(ThreadId);
            checkf(ThreadIdTLS != 0, TEXT("ThreadId is 0 - reconsider implementation of GetCurrentThreadId() (syscall changed?)"));
        }
        return ThreadIdTLS;

#else
        // better than nothing...
        static_assert(sizeof(uint32) == sizeof(pthread_t), "pthread_t cannot be converted to uint32 one to one - different number of bits. Review FLinuxTLS::GetCurrentThreadId() implementation.");
        return static_cast< uint32 >(pthread_self());
#endif
	}

	/**
	 * Allocates a thread local store slot
	 */
	static FORCEINLINE uint32 AllocTlsSlot(void)
	{
		// allocate a per-thread mem slot
		pthread_key_t Key = 0;
		if (pthread_key_create(&Key, NULL) != 0)
		{
			Key = 0xFFFFFFFF;  // matches the Windows TlsAlloc() retval //@todo android: should probably check for this below, or assert out instead
		}
		return Key;
	}

	/**
	 * Sets a value in the specified TLS slot
	 *
	 * @param SlotIndex the TLS index to store it in
	 * @param Value the value to store in the slot
	 */
	static FORCEINLINE void SetTlsValue(uint32 SlotIndex,void* Value)
	{
		pthread_setspecific((pthread_key_t)SlotIndex, Value);
	}

	/**
	 * Reads the value stored at the specified TLS slot
	 *
	 * @return the value stored in the slot
	 */
	static FORCEINLINE void* GetTlsValue(uint32 SlotIndex)
	{
		return pthread_getspecific((pthread_key_t)SlotIndex);
	}

	/**
	 * Frees a previously allocated TLS slot
	 *
	 * @param SlotIndex the TLS index to store it in
	 */
	static FORCEINLINE void FreeTlsSlot(uint32 SlotIndex)
	{
		pthread_key_delete((pthread_key_t)SlotIndex);
	}
};

typedef FLinuxTLS FPlatformTLS;
