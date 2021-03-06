// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"

/*-----------------------------------------------------------------------------
	UAssetObjectProperty.
-----------------------------------------------------------------------------*/

FString UAssetObjectProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, uint32 CPPExportFlags/*=0*/ ) const
{
	return FString::Printf( TEXT("TAssetPtr<class %s%s>"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName() );
}
FString UAssetObjectProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	ExtendedTypeText = FString::Printf(TEXT("TAssetPtr<%s%s>"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
	return TEXT("ASSETOBJECT");
}

FName UAssetObjectProperty::GetID() const
{
	return NAME_AssetObjectProperty;
}

// this is always shallow, can't see that we would want it any other way
bool UAssetObjectProperty::Identical( const void* A, const void* B, uint32 PortFlags ) const
{
	FAssetPtr ObjectA = A ? *((FAssetPtr*)A) : FAssetPtr();
	FAssetPtr ObjectB = B ? *((FAssetPtr*)B) : FAssetPtr();

	return ObjectA.GetUniqueID() == ObjectB.GetUniqueID();
}

void UAssetObjectProperty::SerializeItem( FArchive& Ar, void* Value, int32 MaxReadBytes, void const* Defaults ) const
{
	// We never serialize our reference while the garbage collector is harvesting references
	// to objects, because we don't want asset pointers to keep objects from being garbage collected
	if( !Ar.IsObjectReferenceCollector() || Ar.IsModifyingWeakAndStrongReferences() )
	{
		FAssetPtr OldValue = *(FAssetPtr*)Value;
		Ar << *(FAssetPtr*)Value;

		if (Ar.IsLoading() || Ar.IsModifyingWeakAndStrongReferences()) 
		{
			if (OldValue.GetUniqueID() != ((FAssetPtr*)Value)->GetUniqueID())
			{
				CheckValidObject(Value);
			}
		}
	}
}

void UAssetObjectProperty::ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	FAssetPtr& AssetPtr = *(FAssetPtr*)PropertyValue;

	FStringAssetReference ID;
	UObject *Object = AssetPtr.Get();

	if (Object)
	{
		// Use object in case name has changed.
		ID = FStringAssetReference(Object);
	}
	else
	{
		ID = AssetPtr.GetUniqueID();
	}

	if (!ID.ToString().IsEmpty())
	{
		ValueStr += ID.ToString();
	}
	else
	{
		ValueStr += TEXT("None");
	}
}

const TCHAR* UAssetObjectProperty::ImportText_Internal( const TCHAR* InBuffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const
{
	FAssetPtr& AssetPtr = *(FAssetPtr*)Data;

	FString NewPath;
	const TCHAR* Buffer = InBuffer;
	const TCHAR* NewBuffer = UPropertyHelpers::ReadToken( Buffer, NewPath, 1 );
	if( !NewBuffer )
	{
		return NULL;
	}
	Buffer = NewBuffer;
	if( NewPath==TEXT("None") )
	{
		AssetPtr = NULL;
	}
	else
	{
		if( *Buffer == TCHAR('\'') )
		{
			NewBuffer = UPropertyHelpers::ReadToken( Buffer, NewPath, 1 );
			if( !NewBuffer )
			{
				return NULL;
			}
			Buffer = NewBuffer;
			if( *Buffer++ != TCHAR('\'') )
			{
				return NULL;
			}
		}
		FStringAssetReference ID(NewPath);
		AssetPtr = ID;
	}

	return Buffer;
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UAssetObjectProperty, UObjectPropertyBase,
	{
	}
);

