// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Compression.h: Compression classes
=============================================================================*/

#pragma once

/**
 * Flags controlling [de]compression
 */
enum ECompressionFlags
{
	/** No compression																*/
	COMPRESS_None					= 0x00,
	/** Compress with ZLIB															*/
	COMPRESS_ZLIB 					= 0x01,
	/** Prefer compression that compresses smaller (ONLY VALID FOR COMPRESSION)		*/
	COMPRESS_BiasMemory 			= 0x10,
	/** Prefer compression that compresses faster (ONLY VALID FOR COMPRESSION)		*/
	COMPRESS_BiasSpeed				= 0x20,
	/** If this flag is present, decompression will not happen on the SPUs.			*/
	COMPRESS_ForcePPUDecompressZLib	= 0x80
};

// Define global current platform default to current platform.
#define COMPRESS_Default			COMPRESS_ZLIB

/** Compression Flag Masks **/
/** mask out compression type flags */
#define COMPRESSION_FLAGS_TYPE_MASK		0x0F
/** mask out compression type */
#define COMPRESSION_FLAGS_OPTIONS_MASK	0xF0


/**
 * Chunk size serialization code splits data into. The loading value CANNOT be changed without resaving all
 * compressed data which is why they are split into two separate defines.
 */
#define LOADING_COMPRESSION_CHUNK_SIZE_PRE_369  32768
#define LOADING_COMPRESSION_CHUNK_SIZE			131072
#define SAVING_COMPRESSION_CHUNK_SIZE			LOADING_COMPRESSION_CHUNK_SIZE

struct FCompression
{
	/** Maximum allowed size of an uncompressed buffer passed to CompressMemory or UncompressMemory. */
	const static uint32 MaxUncompressedSize = 256 * 1024;

	/** Time spent compressing data in seconds. */
	CORE_API static double CompressorTime;
	/** Number of bytes before compression.		*/
	CORE_API static uint64 CompressorSrcBytes;
	/** Number of bytes after compression.		*/
	CORE_API static uint64 CompressorDstBytes;

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
	CORE_API static bool CompressMemory( ECompressionFlags Flags, void* CompressedBuffer, int32& CompressedSize, const void* UncompressedBuffer, int32 UncompressedSize );

	/**
	 * Thread-safe abstract decompression routine. Uncompresses memory from compressed buffer and writes it to uncompressed
	 * buffer. UncompressedSize is expected to be the exact size of the data after decompression.
	 *
	 * @param	Flags						Flags to control what method to use to decompress
	 * @param	UncompressedBuffer			Buffer containing uncompressed data
	 * @param	UncompressedSize			Size of uncompressed data in bytes
	 * @param	CompressedBuffer			Buffer compressed data is going to be read from
	 * @param	CompressedSize				Size of CompressedBuffer data in bytes
	 * @param	bIsSourcePadded		Whether the source memory is padded with a full cache line at the end
	 * @return true if compression succeeds, false if it fails because CompressedBuffer was too small or other reasons
	 */
	CORE_API static bool UncompressMemory( ECompressionFlags Flags, void* UncompressedBuffer, int32 UncompressedSize, const void* CompressedBuffer, int32 CompressedSize, bool bIsSourcePadded = false );
};


