// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BitArray.h: Bit array definition.
=============================================================================*/

#pragma once

// Functions for manipulating bit sets.
struct FBitSet
{
	/** Clears the next set bit in the mask and returns its index. */
	static FORCEINLINE uint32 GetAndClearNextBit(uint32& Mask)
	{
		const uint32 LowestBitMask = (Mask) & (-(int32)Mask);
		const uint32 BitIndex = FMath::FloorLog2(LowestBitMask);
		Mask ^= LowestBitMask;
		return BitIndex;
	}
};

// Forward declaration.
template<typename Allocator = FDefaultBitArrayAllocator>
class TBitArray;

template<typename Allocator = FDefaultBitArrayAllocator>
class TConstSetBitIterator;

template<typename Allocator = FDefaultBitArrayAllocator,typename OtherAllocator = FDefaultBitArrayAllocator>
class TConstDualSetBitIterator;

/**
 * Serializer (predefined for no friend injection in gcc 411)
 */
template<typename Allocator>
FArchive& operator<<(FArchive& Ar, TBitArray<Allocator>& BitArray);

/** Used to read/write a bit in the array as a bool. */
class FBitReference
{
public:

	FORCEINLINE FBitReference(uint32& InData,uint32 InMask)
	:	Data(InData)
	,	Mask(InMask)
	{}

	FORCEINLINE operator bool() const
	{
		 return (Data & Mask) != 0;
	}
	FORCEINLINE void operator=(const bool NewValue)
	{
		if(NewValue)
		{
			Data |= Mask;
		}
		else
		{
			Data &= ~Mask;
		}
	}

private:
	uint32& Data;
	uint32 Mask;
};

/** Used to read a bit in the array as a bool. */
class FConstBitReference
{
public:

	FORCEINLINE FConstBitReference(const uint32& InData,uint32 InMask)
	:	Data(InData)
	,	Mask(InMask)
	{}

	FORCEINLINE operator bool() const
	{
		 return (Data & Mask) != 0;
	}

private:
	const uint32& Data;
	uint32 Mask;
};

/** Used to reference a bit in an unspecified bit array. */
class FRelativeBitReference
{
	template<typename>
	friend class TBitArray;
	template<typename>
	friend class TConstSetBitIterator;
	template<typename,typename>
	friend class TConstDualSetBitIterator;
public:

	FORCEINLINE FRelativeBitReference(int32 BitIndex)
	:	DWORDIndex(BitIndex >> NumBitsPerDWORDLogTwo)
	,	Mask(1 << (BitIndex & (NumBitsPerDWORD - 1)))
	{}

protected:
	int32 DWORDIndex;
	uint32 Mask;
};

/**
 * A dynamically sized bit array.
 * An array of Booleans.  They stored in one bit/Boolean.  There are iterators that efficiently iterate over only set bits.
 */
template<typename Allocator /*= FDefaultBitArrayAllocator*/>
class TBitArray : protected Allocator::template ForElementType<uint32>
{
	typedef typename Allocator::template ForElementType<uint32> Super;

public:

	template<typename>
	friend class TConstSetBitIterator;

	template<typename,typename>
	friend class TConstDualSetBitIterator;

	/**
	 * Minimal initialization constructor.
	 * @param Value - The value to initial the bits to.
	 * @param InNumBits - The initial number of bits in the array.
	 */
	explicit TBitArray( const bool Value = false, const int32 InNumBits = 0 )
	:	NumBits(0)
	,	MaxBits(0)
	{
		Init(Value,InNumBits);
	}

	/**
	 * Copy constructor.
	 */
	FORCEINLINE TBitArray(const TBitArray& Copy)
	:	NumBits(0)
	,	MaxBits(0)
	{
		*this = Copy;
	}

	/**
	 * Assignment operator.
	 */
	FORCEINLINE TBitArray& operator=(const TBitArray& Copy)
	{
		// check for self assignment since we don't use swap() mechanic
		if( this == &Copy )
		{
			return *this;
		}

		Empty(Copy.Num());
		NumBits = MaxBits = Copy.NumBits;
		if(NumBits)
		{
			const int32 NumDWORDs = (MaxBits + NumBitsPerDWORD - 1) / NumBitsPerDWORD;
			Realloc(0);
			FMemory::Memcpy(GetData(),Copy.GetData(),NumDWORDs * sizeof(uint32));
		}
		return *this;
	}

#if PLATFORM_COMPILER_HAS_RVALUE_REFERENCES

private:
	template <typename BitArrayType>
	static FORCEINLINE typename TEnableIf<TContainerTraits<BitArrayType>::MoveWillEmptyContainer>::Type MoveOrCopy(BitArrayType& ToArray, BitArrayType& FromArray)
	{
		ToArray.Super::MoveToEmpty((Super&)FromArray);

		ToArray  .NumBits = FromArray.NumBits;
		ToArray  .MaxBits = FromArray.MaxBits;
		FromArray.NumBits = 0;
		FromArray.MaxBits = 0;
	}

	template <typename BitArrayType>
	static FORCEINLINE typename TEnableIf<!TContainerTraits<BitArrayType>::MoveWillEmptyContainer>::Type MoveOrCopy(BitArrayType& ToArray, BitArrayType& FromArray)
	{
		ToArray = FromArray;
	}

public:
	/**
	 * Move constructor.
	 */
	FORCEINLINE TBitArray(TBitArray&& Other)
	{
		MoveOrCopy(*this, Other);
	}

	/**
	 * Move assignment.
	 */
	FORCEINLINE TBitArray& operator=(TBitArray&& Other)
	{
		if (this != &Other)
		{
			MoveOrCopy(*this, Other);
		}

		return *this;
	}

#endif

	/**
	 * Serializer
	 */
	friend FArchive& operator<<(FArchive& Ar, TBitArray& BitArray)
	{
		// serialize number of bits
		Ar << BitArray.NumBits;

		if (Ar.IsLoading())
		{
			// no need for slop when reading
			BitArray.MaxBits = BitArray.NumBits;

			// allocate room for new bits
			BitArray.Realloc(0);
		}

		// calc the number of dwords for all the bits
		const int32 NumDWORDs = (BitArray.NumBits + NumBitsPerDWORD - 1) / NumBitsPerDWORD; 

		// serialize the data as one big chunk
		Ar.Serialize(BitArray.GetData(), NumDWORDs * sizeof(uint32));

		return Ar;
	}

	/**
	 * Adds a bit to the array with the given value.
	 * @return The index of the added bit.
	 */
	int32 Add(const bool Value)
	{
		const int32 Index = NumBits;
		const bool bReallocate = (NumBits + 1) > MaxBits;

		NumBits++;

		if(bReallocate)
		{
			// Allocate memory for the new bits.
			const uint32 MaxDWORDs = this->CalculateSlack(
				(NumBits + NumBitsPerDWORD - 1) / NumBitsPerDWORD,
				(MaxBits + NumBitsPerDWORD - 1) / NumBitsPerDWORD,
				sizeof(uint32)
				);
			MaxBits = MaxDWORDs * NumBitsPerDWORD;
			Realloc(NumBits - 1);
		}

		(*this)[Index] = Value;

		return Index;
	}

	/**
	 * Removes all bits from the array, potentially leaving space allocated for an expected number of bits about to be added.
	 * @param ExpectedNumBits - The expected number of bits about to be added.
	 */
	void Empty(int32 ExpectedNumBits = 0)
	{
		NumBits = 0;

		// If the expected number of bits doesn't match the allocated number of bits, reallocate.
		if(MaxBits != ExpectedNumBits)
		{
			MaxBits = ExpectedNumBits;
			Realloc(0);
		}
	}

	/**
	 * Removes all bits from the array retaining any space already allocated.
	 */
	void Reset()
	{
		NumBits = 0;
	}

	/**
	 * Resets the array's contents.
	 * @param Value - The value to initial the bits to.
	 * @param NumBits - The number of bits in the array.
	 */
	void Init(bool Value,int32 InNumBits)
	{
		Empty(InNumBits);
		if(InNumBits)
		{
			NumBits = InNumBits;
			FMemory::Memset(GetData(),Value ? 0xff : 0,(NumBits + NumBitsPerDWORD - 1) / NumBitsPerDWORD * sizeof(uint32));
		}
	}

	/**
	 * Removes bits from the array.
	 * @param BaseIndex - The index of the first bit to remove.
	 * @param NumBitsToRemove - The number of consecutive bits to remove.
	 */
	void RemoveAt(int32 BaseIndex,int32 NumBitsToRemove = 1)
	{
		check(BaseIndex >= 0 && BaseIndex + NumBitsToRemove <= NumBits);

		// Until otherwise necessary, this is an obviously correct implementation rather than an efficient implementation.
		FIterator WriteIt(*this);
		for(FConstIterator ReadIt(*this);ReadIt;++ReadIt)
		{
			// If this bit isn't being removed, write it back to the array at its potentially new index.
			if(ReadIt.GetIndex() < BaseIndex || ReadIt.GetIndex() >= BaseIndex + NumBitsToRemove)
			{
				if(WriteIt.GetIndex() != ReadIt.GetIndex())
				{
					WriteIt.GetValue() = (bool)ReadIt.GetValue();
				}
				++WriteIt;
			}
		}
		NumBits -= NumBitsToRemove;
	}

	/* Removes bits from the array by swapping them with bits at the end of the array.
	 * This is mainly implemented so that other code using TArray::RemoveSwap will have
	 * matching indices.
 	 * @param BaseIndex - The index of the first bit to remove.
	 * @param NumBitsToRemove - The number of consecutive bits to remove.
	 */
	void RemoveAtSwap( int32 BaseIndex, int32 NumBitsToRemove=1 )
	{
		check(BaseIndex >= 0 && BaseIndex + NumBitsToRemove <= NumBits);
		if( BaseIndex < NumBits - NumBitsToRemove )
		{
			// Copy bits from the end to the region we are removing
			for( int32 Index=0;Index<NumBitsToRemove;Index++ )
			{
#if PLATFORM_MAC
				// Clang compiler doesn't understand the short syntax, so let's be explicit
				int32 FromIndex = NumBits - NumBitsToRemove + Index;
				FConstBitReference From(GetData()[FromIndex / NumBitsPerDWORD],1 << (FromIndex & (NumBitsPerDWORD - 1)));

				int32 ToIndex = BaseIndex + Index;
				FBitReference To(GetData()[ToIndex / NumBitsPerDWORD],1 << (ToIndex & (NumBitsPerDWORD - 1)));

				To = From;
#else
				(*this)[BaseIndex + Index] = (*this)[NumBits - NumBitsToRemove + Index];
#endif
			}
		}
		// Remove the bits from the end of the array.
		RemoveAt(NumBits - NumBitsToRemove, NumBitsToRemove);
	}
	

	/** 
	 * Helper function to return the amount of memory allocated by this container 
	 * @return number of bytes allocated by this container
	 */
	uint32 GetAllocatedSize( void ) const
	{
		return (MaxBits / NumBitsPerDWORD) * sizeof(uint32);
	}

	/** Tracks the container's memory use through an archive. */
	void CountBytes(FArchive& Ar)
	{
		Ar.CountBytes(
			(NumBits / NumBitsPerDWORD) * sizeof(uint32),
			(MaxBits / NumBitsPerDWORD) * sizeof(uint32)
			);
	}

	/**
	 * Finds the first zero bit in the array, sets it to true, and returns the bit index.
	 * If there is none, INDEX_NONE is returned.
	 */
	int32 FindAndSetFirstZeroBit()
	{
		// Iterate over the array until we see a word with a zero bit.
		uint32* RESTRICT DwordArray = GetData();
		const int32 DwordCount = (Num() + NumBitsPerDWORD - 1) / NumBitsPerDWORD;
		int32 DwordIndex = 0;
		while ( DwordIndex < DwordCount && DwordArray[DwordIndex] == 0xffffffff )
		{
			DwordIndex++;
		}

		if ( DwordIndex < DwordCount )
		{
			// Flip the bits, then we only need to find the first one bit -- easy.
			const uint32 Bits = ~(DwordArray[DwordIndex]);
			const uint32 LowestBitMask = (Bits) & (-(int32)Bits);
			const int32 LowestBitIndex = FMath::FloorLog2( LowestBitMask ) + (DwordIndex << NumBitsPerDWORDLogTwo);
			if ( LowestBitIndex < NumBits )
			{
				DwordArray[DwordIndex] |= LowestBitMask;
				return LowestBitIndex;
			}
		}

		return INDEX_NONE;
	}

	// Accessors.
	FORCEINLINE bool IsValidIndex(int32 InIndex) const
	{
		return InIndex >= 0 && InIndex < NumBits;
	}

	FORCEINLINE int32 Num() const { return NumBits; }
	FORCEINLINE FBitReference operator[](int32 Index)
	{
		check(Index>=0 && Index<NumBits);
		return FBitReference(
			GetData()[Index / NumBitsPerDWORD],
			1 << (Index & (NumBitsPerDWORD - 1))
			);
	}
	FORCEINLINE const FConstBitReference operator[](int32 Index) const
	{
		check(Index>=0 && Index<NumBits);
		return FConstBitReference(
			GetData()[Index / NumBitsPerDWORD],
			1 << (Index & (NumBitsPerDWORD - 1))
			);
	}
	FORCEINLINE FBitReference AccessCorrespondingBit(const FRelativeBitReference& RelativeReference)
	{
		checkSlow(RelativeReference.Mask);
		checkSlow(RelativeReference.DWORDIndex >= 0);
		checkSlow(((uint32)RelativeReference.DWORDIndex + 1) * NumBitsPerDWORD - 1 - FMath::CountLeadingZeros(RelativeReference.Mask) < (uint32)NumBits);
		return FBitReference(
			GetData()[RelativeReference.DWORDIndex],
			RelativeReference.Mask
			);
	}
	FORCEINLINE const FConstBitReference AccessCorrespondingBit(const FRelativeBitReference& RelativeReference) const
	{
		checkSlow(RelativeReference.Mask);
		checkSlow(RelativeReference.DWORDIndex >= 0);
		checkSlow(((uint32)RelativeReference.DWORDIndex + 1) * NumBitsPerDWORD - 1 - FMath::CountLeadingZeros(RelativeReference.Mask) < (uint32)NumBits);
		return FConstBitReference(
			GetData()[RelativeReference.DWORDIndex],
			RelativeReference.Mask
			);
	}

	/** BitArray iterator. */
	class FIterator : public FRelativeBitReference
	{
	public:
		FORCEINLINE FIterator(TBitArray<Allocator>& InArray,int32 StartIndex = 0)
		:	FRelativeBitReference(StartIndex)
		,	Array(InArray)
		,	Index(StartIndex)
		{
		}
		FORCEINLINE FIterator& operator++()
		{
			++Index;
			this->Mask <<= 1;
			if(!this->Mask)
			{
				// Advance to the next uint32.
				this->Mask = 1;
				++this->DWORDIndex;
			}
			return *this;
		}
		SAFE_BOOL_OPERATORS(FIterator)
		/** conversion to "bool" returning true if the iterator is valid. */
		FORCEINLINE_EXPLICIT_OPERATOR_BOOL() const
		{ 
			return Index < Array.Num(); 
		}
		/** inverse of the "bool" operator */
		FORCEINLINE bool operator !() const 
		{
			return !(bool)*this;
		}

		FORCEINLINE FBitReference GetValue() const { return FBitReference(Array.GetData()[this->DWORDIndex],this->Mask); }
		FORCEINLINE int32 GetIndex() const { return Index; }
	private:
		TBitArray<Allocator>& Array;
		int32 Index;
	};

	/** Const BitArray iterator. */
	class FConstIterator : public FRelativeBitReference
	{
	public:
		FORCEINLINE FConstIterator(const TBitArray<Allocator>& InArray,int32 StartIndex = 0)
		:	FRelativeBitReference(StartIndex)
		,	Array(InArray)
		,	Index(StartIndex)
		{
		}
		FORCEINLINE FConstIterator& operator++()
		{
			++Index;
			this->Mask <<= 1;
			if(!this->Mask)
			{
				// Advance to the next uint32.
				this->Mask = 1;
				++this->DWORDIndex;
			}
			return *this;
		}

		SAFE_BOOL_OPERATORS(FConstIterator)

		/** conversion to "bool" returning true if the iterator is valid. */
		FORCEINLINE_EXPLICIT_OPERATOR_BOOL() const
		{ 
			return Index < Array.Num(); 
		}
		/** inverse of the "bool" operator */
		FORCEINLINE bool operator !() const 
		{
			return !(bool)*this;
		}

		FORCEINLINE FConstBitReference GetValue() const { return FConstBitReference(Array.GetData()[this->DWORDIndex],this->Mask); }
		FORCEINLINE int32 GetIndex() const { return Index; }
	private:
		const TBitArray<Allocator>& Array;
		int32 Index;
	};

	/** Const reverse iterator. */
	class FConstReverseIterator : public FRelativeBitReference
	{
	public:
		FORCEINLINE FConstReverseIterator(const TBitArray<Allocator>& InArray)
			:	FRelativeBitReference(InArray.Num() - 1)
			,	Array(InArray)
			,	Index(InArray.Num() - 1)
		{
		}
		FORCEINLINE FConstReverseIterator& operator++()
		{
			--Index;
			this->Mask >>= 1;
			if(!this->Mask)
			{
				// Advance to the next uint32.
				this->Mask = (1 << (NumBitsPerDWORD-1));
				--this->DWORDIndex;
			}
			return *this;
		}

		SAFE_BOOL_OPERATORS(FConstReverseIterator)

		/** conversion to "bool" returning true if the iterator is valid. */
		FORCEINLINE_EXPLICIT_OPERATOR_BOOL() const
		{ 
			return Index >= 0; 
		}
		/** inverse of the "bool" operator */
		FORCEINLINE bool operator !() const 
		{
			return !(bool)*this;
		}

		FORCEINLINE FConstBitReference GetValue() const { return FConstBitReference(Array.GetData()[this->DWORDIndex],this->Mask); }
		FORCEINLINE int32 GetIndex() const { return Index; }
	private:
		const TBitArray<Allocator>& Array;
		int32 Index;
	};

	FORCEINLINE const uint32* GetData() const
	{
		return (uint32*)this->GetAllocation();
	}

	FORCEINLINE uint32* GetData()
	{
		return (uint32*)this->GetAllocation();
	}

private:
	int32 NumBits;
	int32 MaxBits;

	void Realloc(int32 PreviousNumBits)
	{
		const int32 PreviousNumDWORDs = (PreviousNumBits + NumBitsPerDWORD - 1) / NumBitsPerDWORD;
		const int32 MaxDWORDs = (MaxBits + NumBitsPerDWORD - 1) / NumBitsPerDWORD;

		this->ResizeAllocation(PreviousNumDWORDs,MaxDWORDs,sizeof(uint32));

		if(MaxDWORDs)
		{
			// Reset the newly allocated slack DWORDs.
			FMemory::Memzero((uint32*)this->GetAllocation() + PreviousNumDWORDs,(MaxDWORDs - PreviousNumDWORDs) * sizeof(uint32));
		}
	}
};

template<typename Allocator>
struct TContainerTraits<TBitArray<Allocator> > : public TContainerTraitsBase<TBitArray<Allocator> >
{
	enum { MoveWillEmptyContainer =
		PLATFORM_COMPILER_HAS_RVALUE_REFERENCES &&
		TAllocatorTraits<Allocator>::SupportsMove };
};

/** An iterator which only iterates over set bits. */
template<typename Allocator>
class TConstSetBitIterator : public FRelativeBitReference
{
public:

	/** Constructor. */
	TConstSetBitIterator(const TBitArray<Allocator>& InArray,int32 StartIndex = 0)
		: FRelativeBitReference(StartIndex)
		, Array                (InArray)
		, UnvisitedBitMask     ((~0) << (StartIndex & (NumBitsPerDWORD - 1)))
		, CurrentBitIndex      (StartIndex)
		, BaseBitIndex         (StartIndex & ~(NumBitsPerDWORD - 1))
	{
		check(StartIndex >= 0 && StartIndex <= Array.Num());
		if (StartIndex != Array.Num())
		{
			FindFirstSetBit();
		}
	}

	/** Advancement operator. */
	FORCEINLINE TConstSetBitIterator& operator++()
	{
		// Mark the current bit as visited.
		UnvisitedBitMask &= ~this->Mask;

		// Find the first set bit that hasn't been visited yet.
		FindFirstSetBit();

		return *this;
	}

	FORCEINLINE friend bool operator==(const TConstSetBitIterator& Lhs, const TConstSetBitIterator& Rhs) 
	{
		// We only need to compare the bit index and the array... all the rest of the state is unobservable.
		return Lhs.CurrentBitIndex == Rhs.CurrentBitIndex && &Lhs.Array == &Rhs.Array;
	}

	FORCEINLINE friend bool operator!=(const TConstSetBitIterator& Lhs, const TConstSetBitIterator& Rhs)
	{ 
		return !(Lhs == Rhs);
	}

	/** conversion to "bool" returning true if the iterator is valid. */
	FORCEINLINE_EXPLICIT_OPERATOR_BOOL() const
	{ 
		return CurrentBitIndex < Array.Num(); 
	}
	/** inverse of the "bool" operator */
	FORCEINLINE bool operator !() const 
	{
		return !(bool)*this;
	}

	/** Index accessor. */
	FORCEINLINE int32 GetIndex() const
	{
		return CurrentBitIndex;
	}

private:

	const TBitArray<Allocator>& Array;

	uint32 UnvisitedBitMask;
	int32 CurrentBitIndex;
	int32 BaseBitIndex;

	/** Find the first set bit starting with the current bit, inclusive. */
	void FindFirstSetBit()
	{
		const uint32* ArrayData      = Array.GetData();
		const int32   LastDWORDIndex = (Array.Num() - 1) / NumBitsPerDWORD;

		// Advance to the next non-zero uint32.
		uint32 RemainingBitMask = ArrayData[this->DWORDIndex] & UnvisitedBitMask;
		while (!RemainingBitMask)
		{
			++this->DWORDIndex;
			BaseBitIndex += NumBitsPerDWORD;
			if (this->DWORDIndex > LastDWORDIndex)
			{
				// We've advanced past the end of the array.
				CurrentBitIndex = Array.Num();
				return;
			}

			RemainingBitMask = ArrayData[this->DWORDIndex];
			UnvisitedBitMask = ~0;
		}

		// This operation has the effect of unsetting the lowest set bit of BitMask
		const uint32 NewRemainingBitMask = RemainingBitMask & (RemainingBitMask - 1);

		// This operation XORs the above mask with the original mask, which has the effect
		// of returning only the bits which differ; specifically, the lowest bit
		this->Mask = NewRemainingBitMask ^ RemainingBitMask;

		// If the Nth bit was the lowest set bit of BitMask, then this gives us N
		CurrentBitIndex = BaseBitIndex + NumBitsPerDWORD - 1 - FMath::CountLeadingZeros(this->Mask);
	}
};

/** An iterator which only iterates over the bits which are set in both of two bit-arrays. */
template<typename Allocator,typename OtherAllocator>
class TConstDualSetBitIterator : public FRelativeBitReference
{
public:

	/** Constructor. */
	FORCEINLINE TConstDualSetBitIterator(
		const TBitArray<Allocator>& InArrayA,
		const TBitArray<OtherAllocator>& InArrayB,
		int32 StartIndex = 0
		)
	:	FRelativeBitReference(StartIndex)
	,	ArrayA(InArrayA)
	,	ArrayB(InArrayB)
	,	UnvisitedBitMask((~0) << (StartIndex & (NumBitsPerDWORD - 1)))
	,	CurrentBitIndex(StartIndex)
	,	BaseBitIndex(StartIndex & ~(NumBitsPerDWORD - 1))
	{
		check(ArrayA.Num() == ArrayB.Num());

		FindFirstSetBit();
	}

	/** Advancement operator. */
	FORCEINLINE TConstDualSetBitIterator& operator++()
	{
		checkSlow(ArrayA.Num() == ArrayB.Num());

		// Mark the current bit as visited.
		UnvisitedBitMask &= ~this->Mask;

		// Find the first set bit that hasn't been visited yet.
		FindFirstSetBit();

		return *this;

	}

	SAFE_BOOL_OPERATORS(TConstDualSetBitIterator<Allocator,OtherAllocator>)

	/** conversion to "bool" returning true if the iterator is valid. */
	FORCEINLINE_EXPLICIT_OPERATOR_BOOL() const
	{ 
		return CurrentBitIndex < ArrayA.Num(); 
	}
	/** inverse of the "bool" operator */
	FORCEINLINE bool operator !() const 
	{
		return !(bool)*this;
	}

	/** Index accessor. */
	FORCEINLINE int32 GetIndex() const
	{
		return CurrentBitIndex;
	}

private:

	const TBitArray<Allocator>& ArrayA;
	const TBitArray<OtherAllocator>& ArrayB;

	uint32 UnvisitedBitMask;
	int32 CurrentBitIndex;
	int32 BaseBitIndex;

	/** Find the first bit that is set in both arrays, starting with the current bit, inclusive. */
	void FindFirstSetBit()
	{
		static const uint32 EmptyArrayData = 0;
		const uint32* ArrayDataA = IfAThenAElseB(ArrayA.GetData(),&EmptyArrayData);
		const uint32* ArrayDataB = IfAThenAElseB(ArrayB.GetData(),&EmptyArrayData);

		// Advance to the next non-zero uint32.
		uint32 RemainingBitMask = ArrayDataA[this->DWORDIndex] & ArrayDataB[this->DWORDIndex] & UnvisitedBitMask;
		while(!RemainingBitMask)
		{
			this->DWORDIndex++;
			BaseBitIndex += NumBitsPerDWORD;
			const int32 LastDWORDIndex = (ArrayA.Num() - 1) / NumBitsPerDWORD;
			if(this->DWORDIndex <= LastDWORDIndex)
			{
				RemainingBitMask = ArrayDataA[this->DWORDIndex] & ArrayDataB[this->DWORDIndex];
				UnvisitedBitMask = ~0;
			}
			else
			{
				// We've advanced past the end of the array.
				CurrentBitIndex = ArrayA.Num();
				return;
			}
		};

		// We can assume that RemainingBitMask!=0 here.
		checkSlow(RemainingBitMask);

		// This operation has the effect of unsetting the lowest set bit of BitMask
		const uint32 NewRemainingBitMask = RemainingBitMask & (RemainingBitMask - 1);

		// This operation XORs the above mask with the original mask, which has the effect
		// of returning only the bits which differ; specifically, the lowest bit
		this->Mask = NewRemainingBitMask ^ RemainingBitMask;

		// If the Nth bit was the lowest set bit of BitMask, then this gives us N
		CurrentBitIndex = BaseBitIndex + NumBitsPerDWORD - 1 - FMath::CountLeadingZeros(this->Mask);
	}
};

/** A specialization of the exchange macro that avoids reallocating when exchanging two bit arrays. */
template<typename Allocator>
FORCEINLINE void Exchange(TBitArray<Allocator>& A,TBitArray<Allocator>& B)
{
	FMemory::Memswap(&A,&B,sizeof(TBitArray<Allocator>));
}

