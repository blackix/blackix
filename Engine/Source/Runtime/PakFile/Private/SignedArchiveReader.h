// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LockFreeList.h"

/**
 * Chunk buffer.
 * Buffers are locked and released only on the worker thread.
 */
struct FChunkBuffer
{
	/** Chunk data */
	uint8* Data;
	/** Number of locks on this buffer */
	int32 LockCount;
	/** Index of the chunk */
	int32 ChunkIndex;
	/** Last time this buffer has been accessed */
	double LastAccessTime;

	FChunkBuffer()
		: Data(NULL)
		, LockCount(0)
		, ChunkIndex(INDEX_NONE)
		, LastAccessTime(0.0)
	{
		Data = (uint8*)FMemory::Malloc(FPakInfo::MaxChunkDataSize);
	}

	~FChunkBuffer()
	{
		FMemory::Free(Data);
		Data = NULL;
	}
};

/**
 * Request to load a chunk. This is how the archive reader and the worker thread
 * communicate. Requests can be locked by both threads.
 */
struct FChunkRequest
{
	/** Chunk index */
	int32 Index;
	/** Chunk offset */
	int64 Offset;
	/** Chunk size */
	int64 Size;
	/** Buffer where the data is cached */
	FChunkBuffer* Buffer;
	/** Flag to indicate if the chunk has been verified */
	FThreadSafeCounter IsTrusted;
	/** Reference count */
	FThreadSafeCounter RefCount;

	/**
	 * Constructor
	 */
	FChunkRequest()
		: Index(INDEX_NONE)
		, Offset(0)
		, Size(0)
		, Buffer(NULL)
		, IsTrusted(0)
		, RefCount(0)
	{}

	/**
	 * Waits until this chunk has been verified
	 */
	FORCEINLINE void WaitUntilReady() const
	{
		while (IsTrusted.GetValue() == 0)
		{
			FPlatformProcess::Sleep(0.0f);
		}
	}
	/**
	 * Checks if this chunk has been verified.
	 */
	FORCEINLINE bool IsReady() const
	{
		return IsTrusted.GetValue() != 0;
	}
};

/**
 * Thread that loads and verifies signed chunks.
 * One per pak file but can serve multiple FSignedArchiveReaders from multiple threads!
 * Can process multiple chunks using a limited number of buffers.
 */
class FChunkCacheWorker : public FRunnable
{	
	enum
	{
		/** Buffer size */
		MaxCachedChunks = 8		
	};

	/** Thread to run the worker FRunnable on */
	FRunnableThread* Thread;
	/** Archive reader */
	FArchive* Reader;
	/** Cached and verified chunks. */
	FChunkBuffer CachedChunks[MaxCachedChunks];
	/** Queue of chunks to cache */
	TArray<FChunkRequest*> RequestQueue;	
	/** Lock for manipulating the queue */
	FCriticalSection QueueLock;
	/** Event used to signal there's work to be done */
	FEvent* QueuedRequestsEvent;
	/** List of active chunk requests */
	TArray<FChunkRequest*> ActiveRequests;
	/** Stops this thread */
	FThreadSafeCounter StopTaskCounter;
	/** Available chunk requests */
	TLockFreePointerList<FChunkRequest> FreeChunkRequests;
	/** Public decryption key */
	FEncryptionKey DecryptionKey;

	/** 
	 * Process requested chunks 
	 *
	 * @return Number of chunks processed this loop.
	 */
	int32 ProcessQueue();
	/** 
	 * Verifies chunk signature 
	 */
	bool CheckSignature(const FChunkRequest& ChunkInfo);
	/** 
	 * Decrypts a signature 
	 */
	void Decrypt(uint8* DecryptedData, const int256* Data, const int64 DataLength);
	/** 
	 * Tries to get a pre-cached chunk buffer 
	 */
	FChunkBuffer* GetCachedChunkBuffer(int32 ChunkIndex);
	/** 
	 * Tries to get the least recent free buffer 
	 */
	FChunkBuffer* GetFreeBuffer();
	/** 
	 * Decrements a ref count on a buffer for the specified chunk 
	 */
	void ReleaseBuffer(int32 ChunkIndex);

public:

	FChunkCacheWorker(FArchive* InReader);
	virtual ~FChunkCacheWorker();

	// Begin FRunnable interface.
	virtual bool Init();
	virtual uint32 Run();
	virtual void Stop();
	// End FRunnable interface

	/** 
	 * Requests a chunk to be loaded and verified
	 * 
	 * @param ChunkIndex Index of a chunk to load
	 * @param StartOffset Offset to the beginning of the chunk
	 * @param ChunkSize Chunk size
	 * @return Handle to the request.
	 */
	FChunkRequest& RequestChunk(int32 ChunkIndex, int64 StartOffset, int64 ChunkSize);
	/**
	 * Releases the requested chunk buffer
	 */
	void ReleaseChunk(FChunkRequest& Chunk);
};

/////////////////////////////////////////////////////////////////////////////////////////

/**
 * FSignedArchiveReader - reads data from pre-cached and verified chunks.
 */
class FSignedArchiveReader : public FArchive
{
	enum
	{
		/** Number of chunks to pre-cache.*/
		PrecacheLength = 0
	};

	struct FReadInfo
	{
		FChunkRequest* Request;
		FChunkBuffer* PreCachedChunk;
		int64 SourceOffset;
		int64 DestOffset;
		int64 Size;
	};

	/** Size of the signature */
	const int64 SignatureSize;
	/** Number of chunks in the archive */
	int32 ChunkCount;	
	/** Reader archive */
	FArchive* PakReader;
	/** Size of the archive on disk */
	int64 SizeOnDisk;
	/** Size of actual data (excluding signatures) */
	int64 PakSize;
	/** Current offet into data */
	int64 PakOffset;
	/** Worker thread - reads chunks from disk and verifies their signatures */
	FChunkCacheWorker* SignatureChecker;
	/** Last pre-cached buffer */
	FChunkBuffer LastCachedChunk;

	/** 
	 * Calculate index of a chunk that contains the specified offset 
	 */
	FORCEINLINE int32 CalculateChunkIndex(int64 ReadOffset) const
	{
		return (int32)(ReadOffset / FPakInfo::MaxChunkDataSize);
	}	

	/** 
	 * Calculate offset of a chunk in the archive 
	 */
	FORCEINLINE int64 CalculateChunkOffsetFromIndex(int64 BufferIndex) const
	{
		return BufferIndex * FPakInfo::MaxChunkDataSize + BufferIndex * SignatureSize;
	}

	/** 
	 * Calculate offset of a chunk in the archive and the offset to read from the archive 
	 *
	 * @param ReadOffset Read request offset
	 * @param OutDataOffset Actuall offset to read from in the archive
	 * @return Offset where the chunk begins in the archive (signature offset)
	 */
	FORCEINLINE int64 CalculateChunkOffset(int64 ReadOffset, int64& OutDataOffset) const
	{
		const int32 ChunkIndex = CalculateChunkIndex(ReadOffset);
		OutDataOffset = ChunkIndex * SignatureSize + ReadOffset;
		return CalculateChunkOffsetFromIndex(ChunkIndex);
	}
	
	/** 
	 * Calculates chunk size based on its index (most chunks have the same size, except the last one 
	 */
	int64 CalculateChunkSize(int64 ChunkIndex) const;

	/** 
	 * Queues chunks on the worker thread 
	 */
	void PrecacheChunks(TArray<FReadInfo>& Chunks, int64 Length);

public:

	FSignedArchiveReader(FArchive* InPakReader, FChunkCacheWorker* InSignatureChecker);
	virtual ~FSignedArchiveReader();

	// Begin FArchive interface
	virtual void Serialize(void* Data, int64 Length) OVERRIDE;
	virtual int64 Tell() OVERRIDE
	{
		return PakOffset;
	}
	virtual int64 TotalSize() OVERRIDE
	{
		return PakSize;
	}
	virtual void Seek(int64 InPos) OVERRIDE
	{
		PakOffset = InPos;
	}
	// End FArchive interface
};
