// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLResources.h: OpenGL resource RHI definitions.
=============================================================================*/

#pragma once

#include "BoundShaderStateCache.h"
#include "OpenGLShaderResources.h"

extern void OnVertexBufferDeletion( GLuint VertexBufferResource );
extern void OnIndexBufferDeletion( GLuint IndexBufferResource );
extern void OnPixelBufferDeletion( GLuint PixelBufferResource );
extern void OnUniformBufferDeletion( GLuint UniformBufferResource, uint32 AllocatedSize, bool bStreamDraw, uint32 Offset, uint8* Pointer );
extern void OnProgramDeletion( GLint ProgramResource );

extern void CachedBindArrayBuffer( GLuint Buffer );
extern void CachedBindElementArrayBuffer( GLuint Buffer );
extern void CachedBindPixelUnpackBuffer( GLuint Buffer );
extern void CachedBindUniformBuffer( GLuint Buffer );
extern bool IsUniformBufferBound( GLuint Buffer );

namespace OpenGLConsoleVariables
{
	extern int32 bUseMapBuffer;
	extern int32 bPrereadStaging;
	extern int32 bUseVAB;
	extern int32 MaxSubDataSize;
};

#if PLATFORM_WINDOWS
#define RESTRICT_SUBDATA_SIZE 1
#endif

void IncrementBufferMemory(GLenum Type, bool bStructuredBuffer, uint32 NumBytes);
void DecrementBufferMemory(GLenum Type, bool bStructuredBuffer, uint32 NumBytes);

// Extra stats for finer-grained timing
// They shouldn't always be on, as they may impact overall performance
#define OPENGLRHI_DETAILED_STATS 0
#if OPENGLRHI_DETAILED_STATS
	DECLARE_CYCLE_STAT_EXTERN(TEXT("MapBuffer time"),STAT_OpenGLMapBufferTime,STATGROUP_OpenGLRHI, );
	DECLARE_CYCLE_STAT_EXTERN(TEXT("UnmapBuffer time"),STAT_OpenGLUnmapBufferTime,STATGROUP_OpenGLRHI, );
	#define SCOPE_CYCLE_COUNTER_DETAILED(Stat)	SCOPE_CYCLE_COUNTER(Stat)
#else
	#define SCOPE_CYCLE_COUNTER_DETAILED(Stat)
#endif

typedef void (*BufferBindFunction)( GLuint Buffer );

template <typename BaseType, GLenum Type, BufferBindFunction BufBind>
class TOpenGLBuffer : public BaseType
{
	void LoadData( uint32 InOffset, uint32 InSize, const void* InData)
	{
		const uint8* Data = (const uint8*)InData;
		const uint32 BlockSize = OpenGLConsoleVariables::MaxSubDataSize;

		if (BlockSize > 0)
		{
			while ( InSize > 0)
			{
				const uint32 Size = FMath::Min<uint32>( BlockSize, InSize);
			
				glBufferSubData( Type, InOffset, Size, Data);

				InOffset += Size;
				InSize -= Size;
				Data += Size;
			}
		}
		else
		{
			glBufferSubData( Type, InOffset, InSize, InData);
		}

	}
public:

	GLuint Resource;

	TOpenGLBuffer(uint32 InStride,uint32 InSize,uint32 InUsage,
		const void *InData = NULL, bool bStreamedDraw = false, GLuint ResourceToUse = 0, uint32 ResourceSize = 0)
	: BaseType(InStride,InSize,InUsage)
	, Resource(0)
	, bIsLocked(false)
	, bIsLockReadOnly(false)
	, bStreamDraw(bStreamedDraw)
	, bLockBufferWasAllocated(false)
	, LockSize(0)
	, LockOffset(0)
	, LockBuffer(NULL)
	, RealSize(InSize)
	{
		if( (FOpenGL::SupportsVertexAttribBinding() && OpenGLConsoleVariables::bUseVAB) || !( InUsage & BUF_ZeroStride ) )
		{
			VERIFY_GL_SCOPE();
			RealSize = ResourceSize ? ResourceSize : InSize;
			if( ResourceToUse )
			{
				Resource = ResourceToUse;
				check( Type != GL_UNIFORM_BUFFER || !IsUniformBufferBound(Resource) );
				Bind();
				glBufferSubData(Type, 0, InSize, InData);
			}
			else
			{
				if (BaseType::GLSupportsType())
				{
					FOpenGL::GenBuffers(1, &Resource);
					check( Type != GL_UNIFORM_BUFFER || !IsUniformBufferBound(Resource) );
					Bind();
#if !RESTRICT_SUBDATA_SIZE
					if( InData == NULL || RealSize <= InSize )
					{
						glBufferData(Type, RealSize, InData, bStreamDraw ? GL_STREAM_DRAW : (IsDynamic() ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW));
					}
					else
					{
						glBufferData(Type, RealSize, NULL, bStreamDraw ? GL_STREAM_DRAW : (IsDynamic() ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW));
						glBufferSubData(Type, 0, InSize, InData);
					}
#else
					glBufferData(Type, RealSize, NULL, bStreamDraw ? GL_STREAM_DRAW : (IsDynamic() ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW));
					if ( InData != NULL )
					{
						LoadData( 0, FMath::Min<uint32>(InSize,RealSize), InData);
					}
#endif
					IncrementBufferMemory(Type, BaseType::IsStructuredBuffer(), RealSize);
				}
				else
				{
					BaseType::CreateType(Resource, InData, InSize);
				}
			}
		}
	}

	virtual ~TOpenGLBuffer()
	{
		VERIFY_GL_SCOPE();
		if (Resource != 0 && BaseType::OnDelete(Resource,RealSize,bStreamDraw,0))
		{
			glDeleteBuffers(1, &Resource);
			DecrementBufferMemory(Type, BaseType::IsStructuredBuffer(), RealSize);
		}
		if (LockBuffer != NULL)
		{
			if (bLockBufferWasAllocated)
			{
				FMemory::Free(LockBuffer);
			}
			else
			{
				UE_LOG(LogRHI,Warning,TEXT("Destroying TOpenGLBuffer without returning memory to the driver; possibly called RHIMapStagingSurface() but didn't call RHIUnmapStagingSurface()? Resource %u"), Resource);
			}
			LockBuffer = NULL;
		}
	}

	void Bind()
	{
		check( (FOpenGL::SupportsVertexAttribBinding() && OpenGLConsoleVariables::bUseVAB) || ( this->GetUsage() & BUF_ZeroStride ) == 0 );
		BufBind(Resource);
	}

	uint8 *Lock(uint32 InOffset, uint32 InSize, bool bReadOnly, bool bDiscard)
	{
		SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLMapBufferTime);
		check( (FOpenGL::SupportsVertexAttribBinding() && OpenGLConsoleVariables::bUseVAB) || ( this->GetUsage() & BUF_ZeroStride ) == 0 );
		check(InOffset + InSize <= this->GetSize());
		//check( LockBuffer == NULL );	// Only one outstanding lock is allowed at a time!
		check( !bIsLocked );	// Only one outstanding lock is allowed at a time!
		VERIFY_GL_SCOPE();

		Bind();

		bIsLocked = true;
		bIsLockReadOnly = bReadOnly;
		uint8 *Data = NULL;

		// If we're able to discard the current data, do so right away
		if ( bDiscard )
		{
			if (BaseType::GLSupportsType())
			{
				glBufferData( Type, RealSize, NULL, IsDynamic() ? GL_STREAM_DRAW : GL_STATIC_DRAW);
			}
		}

		if ( FOpenGL::SupportsMapBuffer() && BaseType::GLSupportsType() && (OpenGLConsoleVariables::bUseMapBuffer || bReadOnly))
		{
			FOpenGL::EResourceLockMode LockMode = bReadOnly ? FOpenGL::RLM_ReadOnly : FOpenGL::RLM_WriteOnly;
			Data = static_cast<uint8*>( FOpenGL::MapBufferRange( Type, InOffset, InSize, LockMode ) );
//			checkf(Data != NULL, TEXT("FOpenGL::MapBufferRange Failed, glError %d (0x%x)"), glGetError(), glGetError());
			
			LockOffset = InOffset;
			LockSize = InSize;
			LockBuffer = Data;
			bLockBufferWasAllocated = false;
		}
		else
		{
			// Allocate a temp buffer to write into
			LockOffset = InOffset;
			LockSize = InSize;
			LockBuffer = FMemory::Malloc( InSize );
			Data = static_cast<uint8*>( LockBuffer );
			bLockBufferWasAllocated = true;
		}

		check(Data != NULL);
		return Data;
	}

	uint8 *LockWriteOnlyUnsynchronized(uint32 InOffset, uint32 InSize, bool bDiscard)
	{
		SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLMapBufferTime);
		check( (FOpenGL::SupportsVertexAttribBinding() && OpenGLConsoleVariables::bUseVAB) || ( this->GetUsage() & BUF_ZeroStride ) == 0 );
		check(InOffset + InSize <= this->GetSize());
		//check( LockBuffer == NULL );	// Only one outstanding lock is allowed at a time!
		check( !bIsLocked );	// Only one outstanding lock is allowed at a time!
		VERIFY_GL_SCOPE();

		Bind();

		bIsLocked = true;
		bIsLockReadOnly = false;
		uint8 *Data = NULL;

		// If we're able to discard the current data, do so right away
		if ( bDiscard )
		{
			glBufferData( Type, RealSize, NULL, IsDynamic() ? GL_STREAM_DRAW : GL_STATIC_DRAW);
		}

		if ( FOpenGL::SupportsMapBuffer() && OpenGLConsoleVariables::bUseMapBuffer)
		{
			FOpenGL::EResourceLockMode LockMode = bDiscard ? FOpenGL::RLM_WriteOnly : FOpenGL::RLM_WriteOnlyUnsynchronized;
			Data = static_cast<uint8*>( FOpenGL::MapBufferRange( Type, InOffset, InSize, LockMode ) );
			LockOffset = InOffset;
			LockSize = InSize;
			LockBuffer = Data;
			bLockBufferWasAllocated = false;
		}
		else
		{
			// Allocate a temp buffer to write into
			LockOffset = InOffset;
			LockSize = InSize;
			LockBuffer = FMemory::Malloc( InSize );
			Data = static_cast<uint8*>( LockBuffer );
			bLockBufferWasAllocated = true;
		}

		check(Data != NULL);
		return Data;
	}

	void Unlock()
	{
		SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLUnmapBufferTime);
		check( (FOpenGL::SupportsVertexAttribBinding() && OpenGLConsoleVariables::bUseVAB) || ( this->GetUsage() & BUF_ZeroStride ) == 0 );
		VERIFY_GL_SCOPE();
		if (bIsLocked)
		{
			Bind();
			
			if ( FOpenGL::SupportsMapBuffer() && BaseType::GLSupportsType() && (OpenGLConsoleVariables::bUseMapBuffer || bIsLockReadOnly))
			{
				check(!bLockBufferWasAllocated);
				if (Type == GL_ARRAY_BUFFER || Type == GL_ELEMENT_ARRAY_BUFFER)
				{
					FOpenGL::UnmapBufferRange(Type, LockOffset, LockSize);
				}
				else
				{
					FOpenGL::UnmapBuffer(Type);
				}
				LockBuffer = NULL;
			}
			else
			{
				if (BaseType::GLSupportsType())
				{
#if !RESTRICT_SUBDATA_SIZE
					// Check for the typical, optimized case
					if( LockSize == RealSize )
					{
						glBufferData(Type, RealSize, LockBuffer, IsDynamic() ? GL_STREAM_DRAW : GL_STATIC_DRAW);
						check( LockBuffer != NULL );
					}
					else
					{
						// Only updating a subset of the data
						glBufferSubData(Type, LockOffset, LockSize, LockBuffer);
						check( LockBuffer != NULL );
					}
#else
					LoadData( LockOffset, LockSize, LockBuffer);
					check( LockBuffer != NULL);
#endif
				}
				check(bLockBufferWasAllocated);
				FMemory::Free(LockBuffer);
				LockBuffer = NULL;
				bLockBufferWasAllocated = false;
			}
			bIsLocked = false;
		}
	}

	void Update(void *InData, uint32 InOffset, uint32 InSize, bool bDiscard)
	{
		check( (FOpenGL::SupportsVertexAttribBinding() && OpenGLConsoleVariables::bUseVAB) || ( this->GetUsage() & BUF_ZeroStride ) == 0 );
		check(InOffset + InSize <= this->GetSize());
		VERIFY_GL_SCOPE();
		Bind();
#if !RESTRICT_SUBDATA_SIZE
		glBufferSubData(Type, InOffset, InSize, InData);
#else
		LoadData( InOffset, InSize, InData);
#endif
	}

	bool IsDynamic() const { return (this->GetUsage() & BUF_AnyDynamic) != 0; }
	bool IsLocked() const { return bIsLocked; }
	bool IsLockReadOnly() const { return bIsLockReadOnly; }
	void* GetLockedBuffer() const { return LockBuffer; }

private:

	uint32 bIsLocked : 1;
	uint32 bIsLockReadOnly : 1;
	uint32 bStreamDraw : 1;
	uint32 bLockBufferWasAllocated : 1;

	GLuint LockSize;
	GLuint LockOffset;
	void* LockBuffer;

	uint32 RealSize;	// sometimes (for example, for uniform buffer pool) we allocate more in OpenGL than is requested of us.
};

class FOpenGLBasePixelBuffer : public FRefCountedObject
{
public:
	FOpenGLBasePixelBuffer(uint32 InStride,uint32 InSize,uint32 InUsage)
	: Size(InSize)
	, Usage(InUsage)
	{}
	static bool OnDelete(GLuint Resource,uint32 Size,bool bStreamDraw,uint32 Offset)
	{
		OnPixelBufferDeletion(Resource);
		return true;
	}
	uint32 GetSize() const { return Size; }
	uint32 GetUsage() const { return Usage; }

	static FORCEINLINE bool GLSupportsType()
	{
		return FOpenGL::SupportsPixelBuffers();
	}

	static void CreateType(GLuint& Resource, const void* InData, uint32 InSize)
	{
		// @todo-mobile
	}

	static bool IsStructuredBuffer() { return false; }

private:
	uint32 Size;
	uint32 Usage;
};

class FOpenGLBaseVertexBuffer : public FRHIVertexBuffer
{
public:
	FOpenGLBaseVertexBuffer(uint32 InStride,uint32 InSize,uint32 InUsage): FRHIVertexBuffer(InSize,InUsage), ZeroStrideVertexBuffer(0)
	{
		if(!(FOpenGL::SupportsVertexAttribBinding() && OpenGLConsoleVariables::bUseVAB) && InUsage & BUF_ZeroStride )
		{
			ZeroStrideVertexBuffer = FMemory::Malloc( InSize );
		}
	}

	~FOpenGLBaseVertexBuffer( void )
	{
		if( ZeroStrideVertexBuffer )
		{
			FMemory::Free( ZeroStrideVertexBuffer );
		}
	}

	void* GetZeroStrideBuffer( void )
	{
		check( ZeroStrideVertexBuffer );
		return ZeroStrideVertexBuffer;
	}

	static bool OnDelete(GLuint Resource,uint32 Size,bool bStreamDraw,uint32 Offset)
	{
		OnVertexBufferDeletion(Resource);
		return true;
	}

	static FORCEINLINE bool GLSupportsType()
	{
		return true;
	}

	static void CreateType(GLuint& Resource, const void* InData, uint32 InSize)
	{
		// @todo-mobile
	}

	static bool IsStructuredBuffer() { return false; }

private:
	void*	ZeroStrideVertexBuffer;
};

struct FOpenGLEUniformBufferData : public FRefCountedObject
{
	FOpenGLEUniformBufferData(uint32 SizeInBytes)
	{
		uint32 SizeInUint32s = (SizeInBytes + 3) / 4;
		Data.Empty(SizeInUint32s);
		Data.AddUninitialized(SizeInUint32s);
		IncrementBufferMemory(GL_UNIFORM_BUFFER,false,Data.GetAllocatedSize());
	}

	~FOpenGLEUniformBufferData()
	{
		DecrementBufferMemory(GL_UNIFORM_BUFFER,false,Data.GetAllocatedSize());
	}

	TArray<uint32> Data;
};
typedef TRefCountPtr<FOpenGLEUniformBufferData> FOpenGLEUniformBufferDataRef;

extern FOpenGLEUniformBufferDataRef AllocateOpenGLEUniformBufferData(uint32 Size, GLuint& Resource);
extern void FreeOpenGLEUniformBufferData(GLuint Resource);

// Emulated Uniform Buffer
class FOpenGLEUniformBuffer : public FRHIUniformBuffer
{
public:
	const uint32 UniqueID;
	GLuint Resource;
	bool bStreamDraw;
	uint32 RealSize;

	FOpenGLEUniformBuffer(uint32 InStride, uint32 InSize, uint32 InUsage,
		const void *InData, bool bInStreamDraw, GLuint ResourceToUse, uint32 ResourceSize) :
		FRHIUniformBuffer(InSize), UniqueID(UniqueIDCounter), Resource(ResourceToUse), bStreamDraw(bInStreamDraw)
	{
		RealSize = ResourceSize ? ResourceSize : InSize;
		Buffer = AllocateOpenGLEUniformBufferData(InSize, Resource);
		if (InData)
		{
			FMemory::Memcpy(Buffer->Data.GetData(), InData, Buffer->Data.Num() * 4);
		}

		++UniqueIDCounter;
	}

	~FOpenGLEUniformBuffer()
	{
		OnUniformBufferDeletion(Resource, RealSize, bStreamDraw, 0, 0);
	}

	FOpenGLEUniformBufferDataRef Buffer;

protected:
	static uint32 UniqueIDCounter;
};


class FOpenGLBaseUniformBuffer : public FRHIUniformBuffer
{
public:
	FOpenGLBaseUniformBuffer(uint32 InStride,uint32 InSize,uint32 InUsage): FRHIUniformBuffer(InSize) {}
	static bool OnDelete(GLuint Resource,uint32 Size,bool bStreamDraw,uint32 Offset,uint8* Pointer)
	{
		OnUniformBufferDeletion(Resource,Size,bStreamDraw,Offset,Pointer);
		return false;
	}
	uint32 GetUsage() const { return 0; }

	static FORCEINLINE bool GLSupportsType()
	{
		return FOpenGL::SupportsUniformBuffers();
	}

	static void CreateType(GLuint& Resource, const void* InData, uint32 InSize)
	{
		// @todo-mobile
	}

	static bool IsStructuredBuffer() { return false; }
};


class FOpenGLBaseIndexBuffer : public FRHIIndexBuffer
{
public:
	FOpenGLBaseIndexBuffer(uint32 InStride,uint32 InSize,uint32 InUsage): FRHIIndexBuffer(InStride,InSize,InUsage) {}
	static bool OnDelete(GLuint Resource,uint32 Size,bool bStreamDraw,uint32 Offset)
	{
		OnIndexBufferDeletion(Resource);
		return true;
	}

	static FORCEINLINE bool GLSupportsType()
	{
		return true;
	}

	static void CreateType(GLuint& Resource, const void* InData, uint32 InSize)
	{
		// @todo-mobile
	}

	static bool IsStructuredBuffer() { return false; }
};

class FOpenGLBaseStructuredBuffer : public FRHIStructuredBuffer
{
public:
	FOpenGLBaseStructuredBuffer(uint32 InStride,uint32 InSize,uint32 InUsage): FRHIStructuredBuffer(InStride,InSize,InUsage) {}
	static bool OnDelete(GLuint Resource,uint32 Size,bool bStreamDraw,uint32 Offset)
	{
		OnVertexBufferDeletion(Resource);
		return true;
	}

	static FORCEINLINE bool GLSupportsType()
	{
		return FOpenGL::SupportsStructuredBuffers();
	}

	static void CreateType(GLuint& Resource, const void* InData, uint32 InSize)
	{
		// @todo-mobile
	}

	static bool IsStructuredBuffer() { return true; }
};

typedef TOpenGLBuffer<FOpenGLBasePixelBuffer, GL_PIXEL_UNPACK_BUFFER, CachedBindPixelUnpackBuffer> FOpenGLPixelBuffer;
typedef TOpenGLBuffer<FOpenGLBaseVertexBuffer, GL_ARRAY_BUFFER, CachedBindArrayBuffer> FOpenGLVertexBuffer;
typedef TOpenGLBuffer<FOpenGLBaseIndexBuffer,GL_ELEMENT_ARRAY_BUFFER,CachedBindElementArrayBuffer> FOpenGLIndexBuffer;
typedef TOpenGLBuffer<FOpenGLBaseStructuredBuffer,GL_ARRAY_BUFFER,CachedBindArrayBuffer> FOpenGLStructuredBuffer;

#define SUBALLOCATED_CONSTANT_BUFFER 1

#if !SUBALLOCATED_CONSTANT_BUFFER
typedef TOpenGLBuffer<FOpenGLBaseUniformBuffer, GL_UNIFORM_BUFFER, CachedBindUniformBuffer> FOpenGLUniformBuffer;
#else
class FOpenGLUniformBuffer : public FOpenGLBaseUniformBuffer
{
public:
	GLuint Resource;
	bool bStreamDraw;
	uint32 RealSize;
	uint32 Offset;
	uint8* Pointer;

	FOpenGLUniformBuffer(uint32 InStride,uint32 InSize,uint32 InUsage,
		const void *InData = NULL, bool bStreamedDraw = false, GLuint ResourceToUse = 0, uint32 ResourceSize = 0, uint32 InOffset = 0, uint8* InPointer = 0)
	: FOpenGLBaseUniformBuffer(InStride,InSize,InUsage)
	, Resource(0)
	, bStreamDraw(bStreamedDraw)
	, RealSize(InSize)
	, Offset(InOffset)
	, Pointer(InPointer)
	{
		VERIFY_GL_SCOPE();
		RealSize = ResourceSize ? ResourceSize : InSize;
		if( ResourceToUse )
		{
			Resource = ResourceToUse;
			if ( Pointer)
			{
				//Want to just use memcpy, no need to bind, etc
				FMemory::Memcpy( Pointer, InData, InSize);
			}
			else
			{
				CachedBindUniformBuffer(Resource);
				glBufferSubData(GL_UNIFORM_BUFFER, Offset, InSize, InData);
			}
		}
		else
		{
			check( Offset == 0);
			check( Pointer == 0);
			if (GLSupportsType())
			{
				FOpenGL::GenBuffers(1, &Resource);
				CachedBindUniformBuffer(Resource);
				if( InData == NULL || RealSize <= InSize )
				{
					glBufferData(GL_UNIFORM_BUFFER, RealSize, InData, bStreamDraw ? GL_STREAM_DRAW : (IsDynamic() ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW));
				}
				else
				{
					glBufferData(GL_UNIFORM_BUFFER, RealSize, NULL, bStreamDraw ? GL_STREAM_DRAW : (IsDynamic() ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW));
					glBufferSubData(GL_UNIFORM_BUFFER, 0, InSize, InData);
				}
				IncrementBufferMemory(GL_UNIFORM_BUFFER, IsStructuredBuffer(), RealSize);
			}
			else
			{
				CreateType(Resource, InData, InSize);
			}
		}
	}

	~FOpenGLUniformBuffer()
	{
		VERIFY_GL_SCOPE();
		if (Resource != 0 && OnDelete(Resource,RealSize,bStreamDraw,Offset,Pointer))
		{
			glDeleteBuffers(1, &Resource);
			DecrementBufferMemory(GL_UNIFORM_BUFFER, IsStructuredBuffer(), RealSize);
		}
	}

	bool IsDynamic() const { return (this->GetUsage() & BUF_AnyDynamic) != 0; }
};
#endif

#define MAX_STREAMED_BUFFERS_IN_ARRAY 2	// must be > 1!
#define MIN_DRAWS_IN_SINGLE_BUFFER 16

template <typename BaseType, uint32 Stride>
class TOpenGLStreamedBufferArray
{
public:

	TOpenGLStreamedBufferArray( void ) {}
	virtual ~TOpenGLStreamedBufferArray( void ) {}

	void Init( uint32 InitialBufferSize )
	{
		for( int32 BufferIndex = 0; BufferIndex < MAX_STREAMED_BUFFERS_IN_ARRAY; ++BufferIndex )
		{
			Buffer[BufferIndex] = new BaseType(Stride, InitialBufferSize, BUF_Volatile, NULL, true);
		}
		CurrentBufferIndex = 0;
		CurrentOffset = 0;
		LastOffset = 0;
		MinNeededBufferSize = InitialBufferSize / MIN_DRAWS_IN_SINGLE_BUFFER;
	}

	void Cleanup( void )
	{
		for( int32 BufferIndex = 0; BufferIndex < MAX_STREAMED_BUFFERS_IN_ARRAY; ++BufferIndex )
		{
			Buffer[BufferIndex].SafeRelease();
		}
	}

	uint8* Lock( uint32 DataSize )
	{
		check(!Buffer[CurrentBufferIndex]->IsLocked());
		DataSize = Align(DataSize, (1<<8));	// to keep the speed up, let's start data for each next draw at 256-byte aligned offset

		// Keep our dynamic buffers at least MIN_DRAWS_IN_SINGLE_BUFFER times bigger than max single request size
		uint32 NeededBufSize = Align( MIN_DRAWS_IN_SINGLE_BUFFER * DataSize, (1 << 20) );	// 1MB increments
		if (NeededBufSize > MinNeededBufferSize)
		{
			MinNeededBufferSize = NeededBufSize;
		}

		// Check if we need to switch buffer, as the current draw data won't fit in current one
		bool bDiscard = false;
		if (Buffer[CurrentBufferIndex]->GetSize() < CurrentOffset + DataSize)
		{
			// We do.
			++CurrentBufferIndex;
			if (CurrentBufferIndex == MAX_STREAMED_BUFFERS_IN_ARRAY)
			{
				CurrentBufferIndex = 0;
			}
			CurrentOffset = 0;

			// Check if we should extend the next buffer, as max request size has changed
			if (MinNeededBufferSize > Buffer[CurrentBufferIndex]->GetSize())
			{
				Buffer[CurrentBufferIndex].SafeRelease();
				Buffer[CurrentBufferIndex] = new BaseType(Stride, MinNeededBufferSize, BUF_Volatile);
			}

			bDiscard = true;
		}

		LastOffset = CurrentOffset;
		CurrentOffset += DataSize;

		return Buffer[CurrentBufferIndex]->LockWriteOnlyUnsynchronized(LastOffset, DataSize, bDiscard);
	}

	void Unlock( void )
	{
		check(Buffer[CurrentBufferIndex]->IsLocked());
		Buffer[CurrentBufferIndex]->Unlock();
	}

	BaseType* GetPendingBuffer( void ) { return Buffer[CurrentBufferIndex]; }
	uint32 GetPendingOffset( void ) { return LastOffset; }

private:
	TRefCountPtr<BaseType> Buffer[MAX_STREAMED_BUFFERS_IN_ARRAY];
	uint32 CurrentBufferIndex;
	uint32 CurrentOffset;
	uint32 LastOffset;
	uint32 MinNeededBufferSize;
};

typedef TOpenGLStreamedBufferArray<FOpenGLVertexBuffer,0> FOpenGLStreamedVertexBufferArray;
typedef TOpenGLStreamedBufferArray<FOpenGLIndexBuffer,sizeof(uint16)> FOpenGLStreamedIndexBufferArray;

struct FOpenGLVertexElement
{
	GLenum Type;
	GLuint StreamIndex;
	GLuint Offset;
	GLuint Size;
	GLuint Divisor;
	uint8 bNormalized;
	uint8 AttributeIndex;
	uint8 bShouldConvertToFloat;
};

/** Convenience typedef: preallocated array of OpenGL input element descriptions. */
typedef TArray<FOpenGLVertexElement,TFixedAllocator<MaxVertexElementCount> > FOpenGLVertexElements;

/** This represents a vertex declaration that hasn't been combined with a specific shader to create a bound shader. */
class FOpenGLVertexDeclaration : public FRHIVertexDeclaration
{
public:
	/** Elements of the vertex declaration. */
	FOpenGLVertexElements VertexElements;

	/** Initialization constructor. */
	explicit FOpenGLVertexDeclaration(const FOpenGLVertexElements& InElements)
		: VertexElements(InElements)
	{
	}
};


/**
 * Combined shader state and vertex definition for rendering geometry. 
 * Each unique instance consists of a vertex decl, vertex shader, and pixel shader.
 */
class FOpenGLBoundShaderState : public FRHIBoundShaderState
{
public:

	FCachedBoundShaderStateLink CacheLink;

	FOpenGLLinkedProgram* LinkedProgram;
	TRefCountPtr<FOpenGLVertexDeclaration> VertexDeclaration;
	TRefCountPtr<FOpenGLVertexShader> VertexShader;
	TRefCountPtr<FOpenGLPixelShader> PixelShader;
	TRefCountPtr<FOpenGLGeometryShader> GeometryShader;
	TRefCountPtr<FOpenGLHullShader> HullShader;
	TRefCountPtr<FOpenGLDomainShader> DomainShader;

	/** Initialization constructor. */
	FOpenGLBoundShaderState(
		FOpenGLLinkedProgram* InLinkedProgram,
		FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
		FVertexShaderRHIParamRef InVertexShaderRHI,
		FPixelShaderRHIParamRef InPixelShaderRHI,
		FGeometryShaderRHIParamRef InGeometryShaderRHI,
		FHullShaderRHIParamRef InHullShaderRHI,
		FDomainShaderRHIParamRef InDomainShaderRHI
		);

	bool NeedsTextureStage(int32 TextureStageIndex);
	int32 MaxTextureStageUsed();

	virtual ~FOpenGLBoundShaderState();
};


inline GLenum GetOpenGLTargetFromRHITexture(FRHITexture* Texture)
{
	if(!Texture)
	{
		return GL_NONE;
	}
	else if(Texture->GetTexture2D())
	{
		return GL_TEXTURE_2D;
	}
	else if(Texture->GetTexture2DArray())
	{
		return GL_TEXTURE_2D_ARRAY;
	}
	else if(Texture->GetTexture3D())
	{
		return GL_TEXTURE_3D;
	}
	else if(Texture->GetTextureCube())
	{
		return GL_TEXTURE_CUBE_MAP;
	}
	else
	{
		UE_LOG(LogRHI,Fatal,TEXT("Unknown RHI texture type"));
		return GL_NONE;
	}
}


class FOpenGLTextureBase
{
protected:
	class FOpenGLDynamicRHI* OpenGLRHI;

public:
	// Pointer to current sampler state in this unit
	class FOpenGLSamplerState* SamplerState;
	
	/** The OpenGL texture resource. */
	GLuint Resource;

	/** The OpenGL texture target. */
	GLenum Target;

	/** The OpenGL attachment point. This should always be GL_COLOR_ATTACHMENT0 in case of color buffer, but the actual texture may be attached on other color attachments. */
	GLenum Attachment;

	/** Initialization constructor. */
	FOpenGLTextureBase(
		FOpenGLDynamicRHI* InOpenGLRHI,
		GLuint InResource,
		GLenum InTarget,
		GLenum InAttachment
		)
	: OpenGLRHI(InOpenGLRHI)
	, SamplerState(nullptr)
	, Resource(InResource)
	, Target(InTarget)
	, Attachment(InAttachment)
	, MemorySize( 0 )
	, bIsPowerOfTwo(false)
	{}

	int32 GetMemorySize() const
	{
		return MemorySize;
	}

	void SetMemorySize(uint32 InMemorySize)
	{
		MemorySize = InMemorySize;
	}
	
	void SetIsPowerOfTwo(bool bInIsPowerOfTwo)
	{
		bIsPowerOfTwo  = bInIsPowerOfTwo ? 1 : 0;
	}

	bool IsPowerOfTwo() const
	{
		return bIsPowerOfTwo != 0;
	}

#if PLATFORM_MAC // Flithy hack to workaround radr://16011763
	GLuint GetOpenGLFramebuffer(uint32 ArrayIndices, uint32 MipmapLevels);
#endif

private:
	uint32 MemorySize		: 31;
	uint32 bIsPowerOfTwo	: 1;
};

// Temporary Android GL4 WAR
// Use host pointers instead of PBOs for texture uploads
#if PLATFORM_ANDROIDGL4
#define USE_PBO 0
#else
#define USE_PBO 1
#endif

// Textures.
template<typename BaseType>
class TOpenGLTexture : public BaseType, public FOpenGLTextureBase
{
public:

	/** Initialization constructor. */
	TOpenGLTexture(
		class FOpenGLDynamicRHI* InOpenGLRHI,
		GLuint InResource,
		GLenum InTarget,
		GLenum InAttachment,
		uint32 InSizeX,
		uint32 InSizeY,
		uint32 InSizeZ,
		uint32 InNumMips,
		uint32 InNumSamples,
		uint32 InArraySize,
		EPixelFormat InFormat,
		bool bInCubemap,
		bool bInAllocatedStorage,
		uint32 InFlags
		)
	: BaseType(InSizeX,InSizeY,InSizeZ,InNumMips,InNumSamples, InArraySize, InFormat,InFlags)
	, FOpenGLTextureBase(
		InOpenGLRHI,
		InResource,
		InTarget,
		InAttachment
		)
	, BaseLevel(0)
	, bCubemap(bInCubemap)
	{
		PixelBuffers.AddZeroed(this->GetNumMips() * (bCubemap ? 6 : 1) * GetEffectiveSizeZ());
		bAllocatedStorage.Init(bInAllocatedStorage, this->GetNumMips() * (bCubemap ? 6 : 1));
#if !USE_PBO
		TempBuffers.AddZeroed(this->GetNumMips() * (bCubemap ? 6 : 1) * GetEffectiveSizeZ());
#endif
	}

	virtual ~TOpenGLTexture();

	/**
	 * Locks one of the texture's mip-maps.
	 * @return A pointer to the specified texture data.
	 */
	void* Lock(uint32 MipIndex,uint32 ArrayIndex,EResourceLockMode LockMode,uint32& DestStride);

	/** Unlocks a previously locked mip-map. */
	void Unlock(uint32 MipIndex,uint32 ArrayIndex);

	/** Updates the host accessible version of the texture */
	void UpdateHost(uint32 MipIndex,uint32 ArrayIndex);

	/** Get PBO Resource for readback */
	GLuint GetBufferResource(uint32 MipIndex,uint32 ArrayIndex);
	
	// Accessors.
	bool IsDynamic() const { return (this->GetFlags() & TexCreate_Dynamic) != 0; }
	bool IsCubemap() const { return bCubemap != 0; }
	bool IsStaging() const { return (this->GetFlags() & TexCreate_CPUReadback) != 0; }


	/** FRHITexture override.  See FRHITexture::GetNativeResource() */
	virtual void* GetNativeResource() const OVERRIDE
	{ 
		return const_cast<void *>(reinterpret_cast<const void*>(&Resource));
	}

	/**
	 * Accessors to mark whether or not we have allocated storage for each mip/face.
	 * For non-cubemaps FaceIndex should always be zero.
	 */
	bool GetAllocatedStorageForMip(uint32 MipIndex, uint32 FaceIndex) const
	{
		return bAllocatedStorage[MipIndex * (bCubemap ? 6 : 1) + FaceIndex];
	}
	void SetAllocatedStorageForMip(uint32 MipIndex, uint32 FaceIndex)
	{
		bAllocatedStorage[MipIndex * (bCubemap ? 6 : 1) + FaceIndex] = true;
	}

	/**
	 * Clone texture from a source using CopyImageSubData
	 */
	void CloneViaCopyImage( TOpenGLTexture* Src, uint32 NumMips, int32 SrcOffset, int32 DstOffset);
	
	/**
	 * Clone texture from a source going via PBOs
	 */
	void CloneViaPBO( TOpenGLTexture* Src, uint32 NumMips, int32 SrcOffset, int32 DstOffset);

	/*
	 * Resolved the specified face for a read Lock, for non-renderable, CPU readable surfaces this eliminates the readback inside Lock itself.
	 */
	void Resolve(uint32 MipIndex,uint32 ArrayIndex);
private:

	TArray< TRefCountPtr<FOpenGLPixelBuffer> > PixelBuffers;

#if !USE_PBO
	struct FTempBuffer
	{
		void* Data;
		uint32 Size;
		bool bReadOnly;

		FTempBuffer() : Data(0), Size(0), bReadOnly(false)
		{}
	};
	TArray< FTempBuffer> TempBuffers;
#endif

	uint32 GetEffectiveSizeZ( void ) { return this->GetSizeZ() ? this->GetSizeZ() : 1; }

	/** Index of the largest mip-map in the texture */
	uint32 BaseLevel;

	/** Bitfields marking whether we have allocated storage for each mip */
	TBitArray<TInlineAllocator<1> > bAllocatedStorage;

	/** Whether the texture is a cube-map. */
	const uint32 bCubemap : 1;
};

class FOpenGLBaseTexture2D : public FRHITexture2D
{
public:
	FOpenGLBaseTexture2D(uint32 InSizeX,uint32 InSizeY,uint32 InSizeZ,uint32 InNumMips,uint32 InNumSamples, uint32 InArraySize, EPixelFormat InFormat,uint32 InFlags)
	: FRHITexture2D(InSizeX,InSizeY,InNumMips,InNumSamples,InFormat,InFlags)
	, SampleCount(InNumSamples)
	{}
	uint32 GetSizeZ() const { return 0; }
	uint32 GetNumSamples() const { return SampleCount; }
private:
	uint32 SampleCount;
};

class FOpenGLBaseTexture2DArray : public FRHITexture2DArray
{
public:
	FOpenGLBaseTexture2DArray(uint32 InSizeX,uint32 InSizeY,uint32 InSizeZ,uint32 InNumMips,uint32 InNumSamples,uint32 InArraySize, EPixelFormat InFormat,uint32 InFlags)
	: FRHITexture2DArray(InSizeX,InSizeY,InSizeZ,InNumMips,InFormat,InFlags)
	{
		check(InNumSamples == 1);	// OpenGL supports multisampled texture arrays, but they're currently not implemented in OpenGLDrv.
	}
};

class FOpenGLBaseTextureCube : public FRHITextureCube
{
public:
	FOpenGLBaseTextureCube(uint32 InSizeX,uint32 InSizeY,uint32 InSizeZ,uint32 InNumMips,uint32 InNumSamples, uint32 InArraySize, EPixelFormat InFormat,uint32 InFlags)
	: FRHITextureCube(InSizeX,InNumMips,InFormat,InFlags)
	, ArraySize(InArraySize)
	{
		check(InNumSamples == 1);	// OpenGL doesn't currently support multisampled cube textures
	
	}
	uint32 GetSizeX() const { return GetSize(); }
	uint32 GetSizeY() const { return GetSize(); }
	uint32 GetSizeZ() const { return ArraySize > 1 ? ArraySize : 0; }
	
	uint32 GetArraySize() const {return ArraySize;}
private:
	uint32 ArraySize;
};

class FOpenGLBaseTexture3D : public FRHITexture3D
{
public:
	FOpenGLBaseTexture3D(uint32 InSizeX,uint32 InSizeY,uint32 InSizeZ,uint32 InNumMips,uint32 InNumSamples,uint32 InArraySize, EPixelFormat InFormat,uint32 InFlags)
	: FRHITexture3D(InSizeX,InSizeY,InSizeZ,InNumMips,InFormat,InFlags)
	{
		check(InNumSamples == 1);	// Can't have multisampled texture 3D. Not supported anywhere.
	}
};

typedef TOpenGLTexture<FRHITexture>						FOpenGLTexture;
typedef TOpenGLTexture<FOpenGLBaseTexture2D>			FOpenGLTexture2D;
typedef TOpenGLTexture<FOpenGLBaseTexture2DArray>		FOpenGLTexture2DArray;
typedef TOpenGLTexture<FOpenGLBaseTexture3D>			FOpenGLTexture3D;
typedef TOpenGLTexture<FOpenGLBaseTextureCube>			FOpenGLTextureCube;

/** Given a pointer to a RHI texture that was created by the OpenGL RHI, returns a pointer to the FOpenGLTextureBase it encapsulates. */
inline FOpenGLTextureBase* GetOpenGLTextureFromRHITexture(FRHITexture* Texture)
{
	if(!Texture)
	{
		return NULL;
	}
	else if(Texture->GetTexture2D())
	{
		return (FOpenGLTexture2D*)Texture;
	}
	else if(Texture->GetTexture2DArray())
	{
		return (FOpenGLTexture2DArray*)Texture;
	}
	else if(Texture->GetTexture3D())
	{
		return (FOpenGLTexture3D*)Texture;
	}
	else if(Texture->GetTextureCube())
	{
		return (FOpenGLTextureCube*)Texture;
	}
	else
	{
		UE_LOG(LogRHI,Fatal,TEXT("Unknown RHI texture type"));
		return NULL;
	}
}

inline uint32 GetOpenGLTextureSizeXFromRHITexture(FRHITexture* Texture)
{
	if(!Texture)
	{
		return 0;
	}
	else if(Texture->GetTexture2D())
	{
		return ((FOpenGLTexture2D*)Texture)->GetSizeX();
	}
	else if(Texture->GetTexture2DArray())
	{
		return ((FOpenGLTexture2DArray*)Texture)->GetSizeX();
	}
	else if(Texture->GetTexture3D())
	{
		return ((FOpenGLTexture3D*)Texture)->GetSizeX();
	}
	else if(Texture->GetTextureCube())
	{
		return ((FOpenGLTextureCube*)Texture)->GetSize();
	}
	else
	{
		UE_LOG(LogRHI,Fatal,TEXT("Unknown RHI texture type"));
		return 0;
	}
}

inline uint32 GetOpenGLTextureSizeYFromRHITexture(FRHITexture* Texture)
{
	if(!Texture)
	{
		return 0;
	}
	else if(Texture->GetTexture2D())
	{
		return ((FOpenGLTexture2D*)Texture)->GetSizeY();
	}
	else if(Texture->GetTexture2DArray())
	{
		return ((FOpenGLTexture2DArray*)Texture)->GetSizeY();
	}
	else if(Texture->GetTexture3D())
	{
		return ((FOpenGLTexture3D*)Texture)->GetSizeY();
	}
	else if(Texture->GetTextureCube())
	{
		return ((FOpenGLTextureCube*)Texture)->GetSize();
	}
	else
	{
		UE_LOG(LogRHI,Fatal,TEXT("Unknown RHI texture type"));
		return 0;
	}
}

inline uint32 GetOpenGLTextureSizeZFromRHITexture(FRHITexture* Texture)
{
	if(!Texture)
	{
		return 0;
	}
	else if(Texture->GetTexture2D())
	{
		return 0;
	}
	else if(Texture->GetTexture2DArray())
	{
		return ((FOpenGLTexture2DArray*)Texture)->GetSizeZ();
	}
	else if(Texture->GetTexture3D())
	{
		return ((FOpenGLTexture3D*)Texture)->GetSizeZ();
	}
	else if(Texture->GetTextureCube())
	{
		return ((FOpenGLTextureCube*)Texture)->GetSizeZ();
	}
	else
	{
		UE_LOG(LogRHI,Fatal,TEXT("Unknown RHI texture type"));
		return 0;
	}
}

class FOpenGLRenderQuery : public FRHIRenderQuery
{
public:

	/** The query resource. */
	GLuint Resource;

	/** Identifier of the OpenGL context the query is a part of. */
	uint64 ResourceContext;

	/** The cached query result. */
	GLuint64 Result;

	/** true if the query's result is cached. */
	bool bResultIsCached : 1;

	/** true if the context the query is in was released from another thread */
	bool bInvalidResource : 1;

	// todo: memory optimize
	ERenderQueryType QueryType;

	FOpenGLRenderQuery(ERenderQueryType InQueryType);
	FOpenGLRenderQuery(FOpenGLRenderQuery const& OtherQuery);
	virtual ~FOpenGLRenderQuery();
	FOpenGLRenderQuery& operator=(FOpenGLRenderQuery const& OtherQuery);
};

class FOpenGLUnorderedAccessView : public FRHIUnorderedAccessView
{

public:
	FOpenGLUnorderedAccessView():
	  Resource(0),
		Format(0)
	{

	}
	  
	GLuint	Resource;
	GLenum	Format;
};

class FOpenGLTextureUnorderedAccessView : public FOpenGLUnorderedAccessView
{
public:

	FOpenGLTextureUnorderedAccessView(FTextureRHIParamRef InTexture);

	FTextureRHIRef TextureRHI; // to keep the texture alive
};


class FOpenGLVertexBufferUnorderedAccessView : public FOpenGLUnorderedAccessView
{
public:

	FOpenGLVertexBufferUnorderedAccessView();

	FOpenGLVertexBufferUnorderedAccessView(	FOpenGLDynamicRHI* InOpenGLRHI, FVertexBufferRHIParamRef InVertexBuffer, uint8 Format);

	virtual ~FOpenGLVertexBufferUnorderedAccessView();

	FVertexBufferRHIRef VertexBufferRHI; // to keep the vertex buffer alive

	FOpenGLDynamicRHI* OpenGLRHI;
};

class FOpenGLShaderResourceView : public FRHIShaderResourceView
{
	// In OpenGL 3.2, the only view that actually works is a Buffer<type> kind of view from D3D10,
	// and it's mapped to OpenGL's buffer texture.

public:

	/** OpenGL texture the buffer is bound with */
	GLuint Resource;
	GLenum Target;

	int32 LimitMip;

	FOpenGLShaderResourceView( FOpenGLDynamicRHI* InOpenGLRHI, GLuint InResource, GLenum InTarget )
	:	Resource(InResource)
	,	Target(InTarget)
	,	LimitMip(-1)
	,	OpenGLRHI(InOpenGLRHI)
	,	OwnsResource(true)
	{}

	FOpenGLShaderResourceView( FOpenGLDynamicRHI* InOpenGLRHI, GLuint InResource, GLenum InTarget, GLuint Mip, bool InOwnsResource )
	:	Resource(InResource)
	,	Target(InTarget)
	,	LimitMip(Mip)
	,	OpenGLRHI(InOpenGLRHI)
	,	OwnsResource(InOwnsResource)
	{}

	virtual ~FOpenGLShaderResourceView( void );

protected:
	FOpenGLDynamicRHI* OpenGLRHI;
	bool OwnsResource;
};

void OpenGLTextureDeleted( FRHITexture* Texture );
void OpenGLTextureAllocated( FRHITexture* Texture , uint32 Flags);

extern void ReleaseOpenGLFramebuffers(FOpenGLDynamicRHI* Device, FTextureRHIParamRef TextureRHI);

/** A OpenGL event query resource. */
class FOpenGLEventQuery : public FRenderResource
{
public:

	/** Initialization constructor. */
	FOpenGLEventQuery(class FOpenGLDynamicRHI* InOpenGLRHI)
		: OpenGLRHI(InOpenGLRHI)
		, Sync(NULL)
	{
	}

	/** Issues an event for the query to poll. */
	void IssueEvent();

	/** Waits for the event query to finish. */
	void WaitForCompletion();

	// FRenderResource interface.
	virtual void InitDynamicRHI();
	virtual void ReleaseDynamicRHI();

private:
	FOpenGLDynamicRHI* OpenGLRHI;
	UGLsync Sync;
};

class FOpenGLViewport : public FRHIViewport
{
public:

	FOpenGLViewport(class FOpenGLDynamicRHI* InOpenGLRHI,void* InWindowHandle,uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen);
	~FOpenGLViewport();

	void Resize(uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen);

	// Accessors.
	FIntPoint GetSizeXY() const { return FIntPoint(SizeX, SizeY); }
	FOpenGLTexture2D *GetBackBuffer() const { return BackBuffer; }
	bool IsFullscreen( void ) const { return bIsFullscreen; }

	void WaitForFrameEventCompletion()
	{
		FrameSyncEvent.WaitForCompletion();
	}

	void IssueFrameEvent()
	{
		FrameSyncEvent.IssueEvent();
	}

	virtual void* GetNativeWindow(void** AddParam) const OVERRIDE;

	struct FPlatformOpenGLContext* GetGLContext() const { return OpenGLContext; }
	FOpenGLDynamicRHI* GetOpenGLRHI() const { return OpenGLRHI; }
private:

	friend class FOpenGLDynamicRHI;

	FOpenGLDynamicRHI* OpenGLRHI;
	struct FPlatformOpenGLContext* OpenGLContext;
	uint32 SizeX;
	uint32 SizeY;
	bool bIsFullscreen;
	bool bIsValid;
	TRefCountPtr<FOpenGLTexture2D> BackBuffer;
	FOpenGLEventQuery FrameSyncEvent;
};

