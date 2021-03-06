// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RawIndexBuffer.cpp: Raw index buffer implementation.
=============================================================================*/

#include "EnginePrivate.h"

#if WITH_EDITOR
#include "MeshUtilities.h"
template<typename IndexDataType, typename Allocator>
void CacheOptimizeIndexBuffer(TArray<IndexDataType,Allocator>& Indices)
{
	TArray<IndexDataType> TempIndices(Indices);
	IMeshUtilities& MeshUtilities = FModuleManager::LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	MeshUtilities.CacheOptimizeIndexBuffer(TempIndices);
	Indices = TempIndices;
}
#endif // #if WITH_EDITOR

/*-----------------------------------------------------------------------------
FRawIndexBuffer
-----------------------------------------------------------------------------*/

/**
* Orders a triangle list for better vertex cache coherency.
*/
void FRawIndexBuffer::CacheOptimize()
{
#if WITH_EDITOR
	CacheOptimizeIndexBuffer(Indices);
#endif
}

void FRawIndexBuffer::InitRHI()
{
	uint32 Size = Indices.Num() * sizeof(uint16);
	if( Size > 0 )
	{
		// Create the index buffer.
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16),Size,NULL,BUF_Static);

		// Initialize the buffer.
		void* Buffer = RHILockIndexBuffer(IndexBufferRHI,0,Size,RLM_WriteOnly);
		FMemory::Memcpy(Buffer,Indices.GetTypedData(),Size);
		RHIUnlockIndexBuffer(IndexBufferRHI);
	}
}

FArchive& operator<<(FArchive& Ar,FRawIndexBuffer& I)
{
	I.Indices.BulkSerialize( Ar );
	return Ar;
}

/*-----------------------------------------------------------------------------
	FRawIndexBuffer16or32
-----------------------------------------------------------------------------*/

// on platforms that only support 16-bit indices, the FRawIndexBuffer16or32 class is just typedef'd to the 16-bit version
#if !DISALLOW_32BIT_INDICES

/**
* Orders a triangle list for better vertex cache coherency.
*/
void FRawIndexBuffer16or32::CacheOptimize()
{
#if WITH_EDITOR
	CacheOptimizeIndexBuffer(Indices);
#endif
}

void FRawIndexBuffer16or32::InitRHI()
{
	uint32 Size = Indices.Num() * sizeof(uint32);
	if( Size > 0 )
	{
		// Create the index buffer.
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint32),Size,NULL,BUF_Static);

		// Initialize the buffer.
		void* Buffer = RHILockIndexBuffer(IndexBufferRHI,0,Size,RLM_WriteOnly);
		FMemory::Memcpy(Buffer,Indices.GetTypedData(),Size);
		RHIUnlockIndexBuffer(IndexBufferRHI);
	}

	// BSP sample debugging requires CPU access to index buffers
#if !ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
	// Undo/redo can destroy and recreate the render resources for UModels without rebuilding the
	// buffers, so the indices need to be saved when in the editor.
	if (!GIsEditor && !IsRunningCommandlet())
	{
		Indices.Empty();
	}
#endif
}

FArchive& operator<<(FArchive& Ar,FRawIndexBuffer16or32& I)
{
	I.Indices.BulkSerialize( Ar );
	return Ar;
}

#endif

/*-----------------------------------------------------------------------------
	FRawStaticIndexBuffer
-----------------------------------------------------------------------------*/

FRawStaticIndexBuffer::FRawStaticIndexBuffer(bool InNeedsCPUAccess)
	: IndexStorage(InNeedsCPUAccess)
	, b32Bit(false)
{
}

void FRawStaticIndexBuffer::SetIndices(const TArray<uint32>& InIndices, EIndexBufferStride::Type DesiredStride)
{
	int32 NumIndices = InIndices.Num();
	bool bShouldUse32Bit = false;

	// Figure out if we should store the indices as 16 or 32 bit.
	if (DesiredStride == EIndexBufferStride::Force32Bit)
	{
		bShouldUse32Bit = true;
	}
	else if (DesiredStride == EIndexBufferStride::AutoDetect)
	{
		int32 i = 0;
		while (!bShouldUse32Bit && i < NumIndices)
		{
			bShouldUse32Bit = InIndices[i] > MAX_uint16;
			i++;
		}
	}

	// Allocate storage for the indices.
	int32 IndexStride = bShouldUse32Bit ? sizeof(uint32) : sizeof(uint16);
	IndexStorage.Empty(IndexStride * NumIndices);
	IndexStorage.AddUninitialized(IndexStride * NumIndices);

	// Store them!
	if (bShouldUse32Bit)
	{
		// If the indices are 32 bit we can just do a memcpy.
		check(IndexStorage.Num() == InIndices.Num() * InIndices.GetTypeSize());
		FMemory::Memcpy(IndexStorage.GetData(),InIndices.GetData(),IndexStorage.Num());
		b32Bit = true;
	}
	else
	{
		// Copy element by element demoting 32-bit integers to 16-bit.
		check(IndexStorage.Num() == InIndices.Num() * sizeof(uint16));
		uint16* DestIndices16Bit = (uint16*)IndexStorage.GetData();
		for (int32 i = 0; i < NumIndices; ++i)
		{
			DestIndices16Bit[i] = InIndices[i];
		}
		b32Bit = false;
	}
}

void FRawStaticIndexBuffer::GetCopy(TArray<uint32>& OutIndices) const
{
	int32 NumIndices = b32Bit ? (IndexStorage.Num() / 4) : (IndexStorage.Num() / 2);
	OutIndices.Empty(NumIndices);
	OutIndices.AddUninitialized(NumIndices);

	if (b32Bit)
	{
		// If the indices are 32 bit we can just do a memcpy.
		check(IndexStorage.Num() == OutIndices.Num() * OutIndices.GetTypeSize());
		FMemory::Memcpy(OutIndices.GetData(),IndexStorage.GetData(),IndexStorage.Num());
	}
	else
	{
		// Copy element by element promoting 16-bit integers to 32-bit.
		check(IndexStorage.Num() == OutIndices.Num() * sizeof(uint16));
		const uint16* SrcIndices16Bit = (const uint16*)IndexStorage.GetData();
		for (int32 i = 0; i < NumIndices; ++i)
		{
			OutIndices[i] = SrcIndices16Bit[i];
		}
	}
}

FIndexArrayView FRawStaticIndexBuffer::GetArrayView() const
{
	int32 NumIndices = b32Bit ? (IndexStorage.Num() / 4) : (IndexStorage.Num() / 2);
	return FIndexArrayView(IndexStorage.GetData(),NumIndices,b32Bit);
}

void FRawStaticIndexBuffer::InitRHI()
{
	uint32 IndexStride = b32Bit ? sizeof(uint32) : sizeof(uint16);
	uint32 SizeInBytes = IndexStorage.Num();

	if (SizeInBytes > 0)
	{
		// Create the index buffer.
		IndexBufferRHI = RHICreateIndexBuffer(IndexStride,SizeInBytes,&IndexStorage,BUF_Static);
	}    
}

void FRawStaticIndexBuffer::Serialize(FArchive& Ar, bool bNeedsCPUAccess)
{
	IndexStorage.SetAllowCPUAccess(bNeedsCPUAccess);

	if (Ar.UE4Ver() < VER_UE4_SUPPORT_32BIT_STATIC_MESH_INDICES)
	{
		TResourceArray<uint16,INDEXBUFFER_ALIGNMENT> LegacyIndices;

		b32Bit = false;
		LegacyIndices.BulkSerialize(Ar);
		int32 NumIndices = LegacyIndices.Num();
		int32 IndexStride = sizeof(uint16);
		IndexStorage.Empty(NumIndices * IndexStride);
		IndexStorage.AddUninitialized(NumIndices * IndexStride);
		FMemory::Memcpy(IndexStorage.GetData(),LegacyIndices.GetData(),IndexStorage.Num());
	}
	else
	{
		Ar << b32Bit;
		IndexStorage.BulkSerialize(Ar);
	}
}

/*-----------------------------------------------------------------------------
FRawStaticIndexBuffer16or32
-----------------------------------------------------------------------------*/

/**
* Orders a triangle list for better vertex cache coherency.
*/
template <typename INDEX_TYPE>
void FRawStaticIndexBuffer16or32<INDEX_TYPE>::CacheOptimize()
{
#if WITH_EDITOR
	CacheOptimizeIndexBuffer(Indices);
#endif
}

