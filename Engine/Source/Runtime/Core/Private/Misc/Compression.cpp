// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Compression.cpp: Compression classes
=============================================================================*/

#include "CorePrivate.h"
#include "CompressedGrowableBuffer.h"

DEFINE_LOG_CATEGORY_STATIC(LogCompression, Log, All);


#include "ThirdParty/zlib/zlib-1.2.5/Inc/zlib.h"

/**
 * Thread-safe abstract compression routine. Compresses memory from uncompressed buffer and writes it to compressed
 * buffer. Updates CompressedSize with size of compressed data.
 *
 * @param	CompressedBuffer			Buffer compressed data is going to be written to
 * @param	CompressedSize	[in/out]	Size of CompressedBuffer, at exit will be size of compressed data
 * @param	UncompressedBuffer			Buffer containing uncompressed data
 * @param	UncompressedSize			Size of uncompressed data in bytes
 * @return true if compression succeeds, false if it fails because CompressedBuffer was too small or other reasons
 */
DECLARE_CYCLE_STAT(TEXT("Compress Memory ZLIB"),Stat_appCompressMemoryZLIB,STATGROUP_Engine);

static bool appCompressMemoryZLIB( void* CompressedBuffer, int32& CompressedSize, const void* UncompressedBuffer, int32 UncompressedSize )
{
	SCOPE_CYCLE_COUNTER( Stat_appCompressMemoryZLIB );

	// Zlib wants to use unsigned long.
	unsigned long ZCompressedSize	= CompressedSize;
	unsigned long ZUncompressedSize	= UncompressedSize;
	// Compress data
	bool bOperationSucceeded = compress( (uint8*) CompressedBuffer, &ZCompressedSize, (const uint8*) UncompressedBuffer, ZUncompressedSize ) == Z_OK ? true : false;
	// Propagate compressed size from intermediate variable back into out variable.
	CompressedSize = ZCompressedSize;
	return bOperationSucceeded;
}

/**
 * Thread-safe abstract compression routine. Compresses memory from uncompressed buffer and writes it to compressed
 * buffer. Updates CompressedSize with size of compressed data.
 *
 * @param	UncompressedBuffer			Buffer containing uncompressed data
 * @param	UncompressedSize			Size of uncompressed data in bytes
 * @param	CompressedBuffer			Buffer compressed data is going to be read from
 * @param	CompressedSize				Size of CompressedBuffer data in bytes
 * @return true if compression succeeds, false if it fails because CompressedBuffer was too small or other reasons
 */
DECLARE_CYCLE_STAT(TEXT("Uncompress Memory ZLIB"),Stat_appUncompressMemoryZLIB,STATGROUP_Engine);

bool appUncompressMemoryZLIB( void* UncompressedBuffer, int32 UncompressedSize, const void* CompressedBuffer, int32 CompressedSize )
{
	SCOPE_CYCLE_COUNTER( Stat_appUncompressMemoryZLIB );

	// Zlib wants to use unsigned long.
	unsigned long ZCompressedSize	= CompressedSize;
	unsigned long ZUncompressedSize	= UncompressedSize;
	
	// Uncompress data.
	bool bOperationSucceeded = uncompress( (uint8*) UncompressedBuffer, &ZUncompressedSize, (const uint8*) CompressedBuffer, ZCompressedSize ) == Z_OK ? true : false;

	// Sanity check to make sure we uncompressed as much data as we expected to.
	check( UncompressedSize == ZUncompressedSize );
	return bOperationSucceeded;
}

/** Time spent compressing data in seconds. */
double FCompression::CompressorTime		= 0;
/** Number of bytes before compression.		*/
uint64 FCompression::CompressorSrcBytes	= 0;
/** Nubmer of bytes after compression.		*/
uint64 FCompression::CompressorDstBytes	= 0;

/**
 * Thread-safe abstract compression routine. Compresses memory from uncompressed buffer and writes it to compressed
 * buffer. Updates CompressedSize with size of compressed data. Compression controlled by the passed in flags.
 *
 * @param	Flags						Flags to control what method to use and optionally control memory vs speed
 * @param	CompressedBuffer			Buffer compressed data is going to be written to
 * @param	CompressedSize	[in/out]	Size of CompressedBuffer, at exit will be size of compressed data
 * @param	UncompressedBuffer			Buffer containing uncompressed data
 * @param	UncompressedSize			Size of uncompressed data in bytes
 * @return true if compression succeeds, false if it fails because CompressedBuffer was too small or other reasons
 */
bool FCompression::CompressMemory( ECompressionFlags Flags, void* CompressedBuffer, int32& CompressedSize, const void* UncompressedBuffer, int32 UncompressedSize )
{
	double CompressorStartTime = FPlatformTime::Seconds();

	// make sure a valid compression scheme was provided
	check(Flags & COMPRESS_ZLIB);

	bool bCompressSucceeded = false;

	static bool GAlwaysBiasCompressionForSize = false;
	if (FPlatformProperties::HasEditorOnlyData())
	{
		static bool GTestedCmdLine = false;
		if (!GTestedCmdLine && FCommandLine::IsInitialized())
		{
			GTestedCmdLine = true;
			// Override compression settings wrt size.
			GAlwaysBiasCompressionForSize = FParse::Param( FCommandLine::Get(), TEXT("BIASCOMPRESSIONFORSIZE") );
		}
	}

	// Always bias for speed if option is set.
	if( GAlwaysBiasCompressionForSize )
	{
		int32 NewFlags = Flags;
		NewFlags &= ~COMPRESS_BiasSpeed;
		NewFlags |= COMPRESS_BiasMemory;
		Flags = (ECompressionFlags) NewFlags;
	}

	switch(Flags & COMPRESSION_FLAGS_TYPE_MASK)
	{
		case COMPRESS_ZLIB:
			bCompressSucceeded = appCompressMemoryZLIB(CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize);
			break;
		default:
			UE_LOG(LogCompression, Warning, TEXT("appCompressMemory - This compression type not supported"));
			bCompressSucceeded =  false;
	}

	// Keep track of compression time and stats.
	CompressorTime += FPlatformTime::Seconds() - CompressorStartTime;
	if( bCompressSucceeded )
	{
		CompressorSrcBytes += UncompressedSize;
		CompressorDstBytes += CompressedSize;
	}

	return bCompressSucceeded;
}

/**
 * Thread-safe abstract decompression routine. Uncompresses memory from compressed buffer and writes it to uncompressed
 * buffer. UncompressedSize is expected to be the exact size of the data after decompression.
 *
 * @param	Flags						Flags to control what method to use to decompress
 * @param	UncompressedBuffer			Buffer containing uncompressed data
 * @param	UncompressedSize			Size of uncompressed data in bytes
 * @param	CompressedBuffer			Buffer compressed data is going to be read from
 * @param	CompressedSize				Size of CompressedBuffer data in bytes
 * @param	bIsSourcePadded				Whether the source memory is padded with a full cache line at the end
 * @return true if compression succeeds, false if it fails because CompressedBuffer was too small or other reasons
 */
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Uncompressor total time"),STAT_UncompressorTime,STATGROUP_AsyncIO);

bool FCompression::UncompressMemory( ECompressionFlags Flags, void* UncompressedBuffer, int32 UncompressedSize, const void* CompressedBuffer, int32 CompressedSize, bool bIsSourcePadded /*= false*/ )
{
	// Keep track of time spent uncompressing memory.
	STAT(double UncompressorStartTime = FPlatformTime::Seconds();)
	
	// make sure a valid compression scheme was provided
	check(Flags & COMPRESS_ZLIB);

	bool bUncompressSucceeded = false;

	switch(Flags & COMPRESSION_FLAGS_TYPE_MASK)
	{
		case COMPRESS_ZLIB:
			bUncompressSucceeded = appUncompressMemoryZLIB(UncompressedBuffer, UncompressedSize, CompressedBuffer, CompressedSize);
			break;
		default:
			UE_LOG(LogCompression, Warning, TEXT("FCompression::UncompressMemory - This compression type not supported"));
			bUncompressSucceeded = false;
	}
	INC_FLOAT_STAT_BY(STAT_UncompressorTime,(float)(FPlatformTime::Seconds()-UncompressorStartTime));
	
	return bUncompressSucceeded;
}

/*-----------------------------------------------------------------------------
	FCompressedGrowableBuffer.
-----------------------------------------------------------------------------*/

/**
 * Constructor
 *
 * @param	InMaxPendingBufferSize	Max chunk size to compress in uncompressed bytes
 * @param	InCompressionFlags		Compression flags to compress memory with
 */
FCompressedGrowableBuffer::FCompressedGrowableBuffer( int32 InMaxPendingBufferSize, ECompressionFlags InCompressionFlags )
:	MaxPendingBufferSize( InMaxPendingBufferSize )
,	CompressionFlags( InCompressionFlags )
,	CurrentOffset( 0 )
,	NumEntries( 0 )
,	DecompressedBufferBookKeepingInfoIndex( INDEX_NONE )
{
	PendingCompressionBuffer.Empty( MaxPendingBufferSize );
}

/**
 * Locks the buffer for reading. Needs to be called before calls to Access and needs
 * to be matched up with Unlock call.
 */
void FCompressedGrowableBuffer::Lock()
{
	check( DecompressedBuffer.Num() == 0 );
}

/**
 * Unlocks the buffer and frees temporary resources used for accessing.
 */
void FCompressedGrowableBuffer::Unlock()
{
	DecompressedBuffer.Empty();
	DecompressedBufferBookKeepingInfoIndex = INDEX_NONE;
}

/**
 * Appends passed in data to the buffer. The data needs to be less than the max
 * pending buffer size. The code will assert on this assumption.
 *
 * @param	Data	Data to append
 * @param	Size	Size of data in bytes.
 * @return	Offset of data, used for retrieval later on
 */
int32 FCompressedGrowableBuffer::Append( void* Data, int32 Size )
{
	check( DecompressedBuffer.Num() == 0 );
	check( Size <= MaxPendingBufferSize );
	NumEntries++;

	// Data does NOT fit into pending compression buffer. Compress existing data 
	// and purge buffer.
	if( MaxPendingBufferSize - PendingCompressionBuffer.Num() < Size )
	{
		// Allocate temporary buffer to hold compressed data. It is bigger than the uncompressed size as
		// compression is not guaranteed to create smaller data and we don't want to handle that case so 
		// we simply assert if it doesn't fit. For all practical purposes this works out fine and is what
		// other code in the engine does as well.
		int32 CompressedSize = MaxPendingBufferSize * 4 / 3;
		void* TempBuffer = FMemory::Malloc( CompressedSize );

		// Compress the memory. CompressedSize is [in/out]
		verify( FCompression::CompressMemory( CompressionFlags, TempBuffer, CompressedSize, PendingCompressionBuffer.GetData(), PendingCompressionBuffer.Num() ) );

		// Append the compressed data to the compressed buffer and delete temporary data.
		int32 StartIndex = CompressedBuffer.AddUninitialized( CompressedSize );
		FMemory::Memcpy( &CompressedBuffer[StartIndex], TempBuffer, CompressedSize );
		FMemory::Free( TempBuffer );

		// Keep track of book keeping info for later access to data.
		FBufferBookKeeping Info;
		Info.CompressedOffset = StartIndex;
		Info.CompressedSize = CompressedSize;
		Info.UncompressedOffset = CurrentOffset - PendingCompressionBuffer.Num();
		Info.UncompressedSize = PendingCompressionBuffer.Num();
		BookKeepingInfo.Add( Info ); 

		// Resize & empty the pending buffer to the default state.
		PendingCompressionBuffer.Empty( MaxPendingBufferSize );
	}

	// Appends the data to the pending buffer. The pending buffer is compressed
	// as needed above.
	int32 StartIndex = PendingCompressionBuffer.AddUninitialized( Size );
	FMemory::Memcpy( &PendingCompressionBuffer[StartIndex], Data, Size );

	// Return start offset in uncompressed memory.
	int32 StartOffset = CurrentOffset;
	CurrentOffset += Size;
	return StartOffset;
}

/**
 * Accesses the data at passed in offset and returns it. The memory is read-only and
 * memory will be freed in call to unlock. The lifetime of the data is till the next
 * call to Unlock, Append or Access
 *
 * @param	Offset	Offset to return corresponding data for
 */
void* FCompressedGrowableBuffer::Access( int32 Offset )
{
	void* UncompressedData = NULL;

	// Check whether the decompressed data is already cached.
	if( DecompressedBufferBookKeepingInfoIndex != INDEX_NONE )
	{
		const FBufferBookKeeping& Info = BookKeepingInfo[DecompressedBufferBookKeepingInfoIndex];
		// Cache HIT.
		if( (Info.UncompressedOffset <= Offset) && (Info.UncompressedOffset + Info.UncompressedSize > Offset) )
		{
			// Figure out index into uncompressed data and set it. DecompressionBuffer (return value) is going 
			// to be valid till the next call to Access or Unlock.
			int32 InternalOffset = Offset - Info.UncompressedOffset;
			UncompressedData = &DecompressedBuffer[InternalOffset];
		}
		// Cache MISS.
		else
		{
			DecompressedBufferBookKeepingInfoIndex = INDEX_NONE;
		}
	}

	// Traverse book keeping info till we find the matching block.
	if( UncompressedData == NULL )
	{
		for( int32 InfoIndex=0; InfoIndex<BookKeepingInfo.Num(); InfoIndex++ )
		{
			const FBufferBookKeeping& Info = BookKeepingInfo[InfoIndex];
			if( (Info.UncompressedOffset <= Offset) && (Info.UncompressedOffset + Info.UncompressedSize > Offset) )
			{
				// Found the right buffer, now decompress it.
				DecompressedBuffer.Empty( Info.UncompressedSize );
				DecompressedBuffer.AddUninitialized( Info.UncompressedSize );
				verify( FCompression::UncompressMemory( CompressionFlags, DecompressedBuffer.GetData(), Info.UncompressedSize, &CompressedBuffer[Info.CompressedOffset], Info.CompressedSize ) );

				// Figure out index into uncompressed data and set it. DecompressionBuffer (return value) is going 
				// to be valid till the next call to Access or Unlock.
				int32 InternalOffset = Offset - Info.UncompressedOffset;
				UncompressedData = &DecompressedBuffer[InternalOffset];	

				// Keep track of buffer index for the next call to this function.
				DecompressedBufferBookKeepingInfoIndex = InfoIndex;
				break;
			}
		}
	}

	// If we still haven't found the data it might be in the pending compression buffer.
	if( UncompressedData == NULL )
	{
		int32 UncompressedStartOffset = CurrentOffset - PendingCompressionBuffer.Num();
		if( (UncompressedStartOffset <= Offset) && (CurrentOffset > Offset) )
		{
			// Figure out index into uncompressed data and set it. PendingCompressionBuffer (return value) 
			// is going to be valid till the next call to Access, Unlock or Append.
			int32 InternalOffset = Offset - UncompressedStartOffset;
			UncompressedData = &PendingCompressionBuffer[InternalOffset];
		}
	}

	// Return value is only valid till next call to Access, Unlock or Append!
	check( UncompressedData );
	return UncompressedData;
}


