// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectBase.cpp: Unreal UObject base class
=============================================================================*/

#include "CoreUObjectPrivate.h"

DEFINE_LOG_CATEGORY_STATIC(LogUObjectBase, Log, All);
DEFINE_STAT(STAT_UObjectsStatGroupTester);

/** Whether uobject system is initialized.												*/
namespace Internal
{
	bool GObjInitialized = false;
};


/** Objects to automatically register once the object system is ready.					*/
struct FPendingRegistrantInfo
{
	const TCHAR*	Name;
	const TCHAR*	PackageName;
	FPendingRegistrantInfo(const TCHAR* InName,const TCHAR* InPackageName)
		:	Name(InName)
		,	PackageName(InPackageName)
	{}
	static TMap<UObjectBase*, FPendingRegistrantInfo>& GetMap()
	{
		static TMap<UObjectBase*, FPendingRegistrantInfo> PendingRegistrantInfo;
		return PendingRegistrantInfo;
	}
};



/** Objects to automatically register once the object system is ready.					*/
struct FPendingRegistrant
{
	UObjectBase*	Object;
	FPendingRegistrant*	NextAutoRegister;
	FPendingRegistrant(UObjectBase* InObject)
	:	Object(InObject)
	,	NextAutoRegister(NULL)
	{}
};
static FPendingRegistrant* GFirstPendingRegistrant = NULL;
static FPendingRegistrant* GLastPendingRegistrant = NULL;

#if EXTERNAL_OBJECT_NAMES

/**
 * Annotation for FNames
 */
struct FNameAnnotation
{
	/**
	 * default constructor
	 * Default constructor must be the default item
	 */
	FNameAnnotation() :
		Name(NAME_None)
	{
	}
	/**
	 * Determine if this Name is the default...which is NAME_None
	 * @return true is this is NAME_None
	 */
	FORCEINLINE bool IsDefault()
	{
		return Name == NAME_None;
	}

	/**
	 * Constructor 
	 * @param InName name to assign
	 */
	FNameAnnotation(FName InName) :
		Name(InName)
	{
	}

	/**
	 * Name for this object
	 */
	FName				Name; 

};

template <> struct TIsPODType<FNameAnnotation> { enum { Value = true }; };


/**
 * Annotation to relate names to uobjects
 *
 */
static FUObjectAnnotationDense<FNameAnnotation,false> NameAnnotation;

#endif

/**
 * Constructor used for bootstrapping
 * @param	InClass			possibly NULL, this gives the class of the new object, if known at this time
 * @param	InFlags			RF_Flags to assign
 */
UObjectBase::UObjectBase( EObjectFlags InFlags )
:	ObjectFlags			(InFlags)
,	InternalIndex		(INDEX_NONE)
,	Class				(NULL)
,	Outer				(NULL)
{}

/**
 * Constructor used by StaticAllocateObject
 * @param	InClass				non NULL, this gives the class of the new object, if known at this time
 * @param	InFlags				RF_Flags to assign
 * @param	InOuter				outer for this object
 * @param	InName				name of the new object
 * @param	InObjectArchetype	archetype to assign
 */
UObjectBase::UObjectBase( UClass* InClass, EObjectFlags InFlags, UObject *InOuter, FName InName )
:	ObjectFlags			(InFlags)
,	InternalIndex		(INDEX_NONE)
,	Class				(InClass)
,	Outer				(InOuter)
{
	check(Class);
	// Add to global table.
	AddObject(InName);
}


/**
 * Final destructor, removes the object from the object array, and indirectly, from any annotations
 **/
UObjectBase::~UObjectBase()
{
	// If not initialized, skip out.
	if( UObjectInitialized() && Class && !GIsCriticalError )
	{
		// Validate it.
		check(IsValidLowLevel());
		LowLevelRename(NAME_None);
		GUObjectArray.FreeUObjectIndex(this);
	}
}


const FName UObjectBase::GetFName() const
{
#if EXTERNAL_OBJECT_NAMES
	return NameAnnotation.GetAnnotation(InternalIndex).Name;
#else
	return Name;
#endif
}


#if STATS

void UObjectBase::CreateStatID() const
{
	FString LongName;
	UObjectBase const* Target = this;
	do 
	{
		LongName = Target->GetFName().GetPlainNameString() + (LongName.IsEmpty() ? TEXT("") : TEXT(".")) + LongName;
		Target = Target->GetOuter();
	} while(Target);
	if (GetClass())
	{
		LongName = GetClass()->GetFName().GetPlainNameString() / LongName;
	}

	const FName StatName = FName( *LongName );
	FStartupMessages::Get().AddMetadata( StatName, *LongName, 
		STAT_GROUP_TO_FStatGroup( STATGROUP_UObjects )::GetGroupName(), 
		STAT_GROUP_TO_FStatGroup( STATGROUP_UObjects )::GetGroupCategory(), 
		STAT_GROUP_TO_FStatGroup( STATGROUP_UObjects )::GetDescription(),
		true, EStatDataType::ST_int64, true );

	StatID = IStatGroupEnableManager::Get().GetHighPerformanceEnableForStat(StatName, 
		STAT_GROUP_TO_FStatGroup(STATGROUP_UObjects)::GetGroupName(), 
		STAT_GROUP_TO_FStatGroup(STATGROUP_UObjects)::GetGroupCategory(), 
		STAT_GROUP_TO_FStatGroup(STATGROUP_UObjects)::DefaultEnable,
		true, EStatDataType::ST_int64, *LongName, true);
}
#endif


/**
 * Convert a boot-strap registered class into a real one, add to uobject array, etc
 *
 * @param UClassStaticClass Now that it is known, fill in UClass::StaticClass() as the class
 */
void UObjectBase::DeferredRegister(UClass *UClassStaticClass,const TCHAR* PackageName,const TCHAR* InName)
{
	check(Internal::GObjInitialized);
	// Set object properties.
	Outer        = CreatePackage(NULL,PackageName);
	check(Outer);

	check(UClassStaticClass);
	check(!Class);
	Class = UClassStaticClass;

	// Add to the global object table.
	AddObject(FName(InName));

	// Make sure that objects disregarded for GC are part of root set.
	check(!GUObjectArray.IsDisregardForGC(this) || (GetFlags() & RF_RootSet) );
}

/**
 * Add a newly created object to the name hash tables and the object array
 *
 * @param Name name to assign to this uobject
 */
void UObjectBase::AddObject(FName InName)
{
#if EXTERNAL_OBJECT_NAMES
	NameAnnotation.AddAnnotation(InternalIndex,InName);
#else
	Name = InName;
#endif
	GUObjectArray.AllocateUObjectIndex(this);
	check(InName != NAME_None && InternalIndex >= 0);
	HashObject(this);
	check(IsValidLowLevel());
}

/**
 * Just change the FName and Outer and rehash into name hash tables. For use by higher level rename functions.
 *
 * @param NewName	new name for this object
 * @param NewOuter	new outer for this object, if NULL, outer will be unchanged
 */
void UObjectBase::LowLevelRename(FName NewName,UObject *NewOuter)
{
	STAT(StatID = TStatId();) // reset the stat id since this thing now has a different name
	UnhashObject(this);
	check(InternalIndex >= 0);
#if EXTERNAL_OBJECT_NAMES
	NameAnnotation.AddAnnotation(InternalIndex,NewName);
#else
	Name = NewName;
#endif
	if (NewOuter)
	{
		Outer = NewOuter;
	}
	HashObject(this);
}

void UObjectBase::SetClass(UClass* NewClass)
{
	STAT(StatID = TStatId();) // reset the stat id since this thing now has a different name

	UnhashObject(this);
#if USE_UBER_GRAPH_PERSISTENT_FRAME
	Class->DestroyPersistentUberGraphFrame((UObject*)this);
#endif
	Class = NewClass;
#if USE_UBER_GRAPH_PERSISTENT_FRAME
	Class->CreatePersistentUberGraphFrame((UObject*)this);
#endif
	HashObject(this);
}


/**
 * Checks to see if the object appears to be valid
 * @return true if this appears to be a valid object
 */
bool UObjectBase::IsValidLowLevel() const
{
	if( !this )
	{
		UE_LOG(LogUObjectBase, Warning, TEXT("NULL object") );
		return false;
	}
	if( !Class )
	{
		UE_LOG(LogUObjectBase, Warning, TEXT("Object is not registered") );
		return false;
	}
	return GUObjectArray.IsValid(this);
}

bool UObjectBase::IsValidLowLevelFast(bool bRecursive /*= true*/) const
{
	// As DEFULT_ALIGNMENT is defined to 0 now, I changed that to the original numerical value here
	const int32 AlignmentCheck = MIN_ALIGNMENT - 1;

	// Check 'this' pointer before trying to access any of the Object's members
	if (!this || (UPTRINT)this < 0x100)
	{
		UE_LOG(LogUObjectBase, Error, TEXT("\'this\' pointer is invalid."));
		return false;
	}	
	if ((UPTRINT)this & AlignmentCheck)
	{
		UE_LOG(LogUObjectBase, Error, TEXT("\'this\' pointer is misaligned."));
		return false;
	}
	if (*(void**)this == NULL)
	{
		UE_LOG(LogUObjectBase, Error, TEXT("Virtual functions table is invalid."));
		return false;
	}

	// These should all be 0.
	const UPTRINT CheckZero = (ObjectFlags & ~RF_AllFlags) | ((UPTRINT)Class & AlignmentCheck) | ((UPTRINT)Outer & AlignmentCheck);
	if (!!CheckZero)
	{
		UE_LOG(LogUObjectBase, Error, TEXT("Object flags are invalid or either Class or Outer is misaligned"));
		return false;
	}
	// Avoid infinite recursion so call IsValidLowLevelFast on the class object with bRecirsive = false.
	if (bRecursive && !Class->IsValidLowLevelFast(false))
	{
		UE_LOG(LogUObjectBase, Error, TEXT("Class object failed IsValidLowLevelFast test."));
		return false;
	}
	// These should all be non-NULL (except CDO-alignment check which should be 0)
	if (Class == NULL || Class->ClassDefaultObject == NULL || ((UPTRINT)Class->ClassDefaultObject & AlignmentCheck) != 0)
	{
		UE_LOG(LogUObjectBase, Error, TEXT("Class pointer is invalid or CDO is invalid."));
		return false;
	}
	// Lightweight versions of index checks.
	if (!GUObjectArray.IsValidIndex(this) || !Name.IsValidIndexFast())
	{
		UE_LOG(LogUObjectBase, Error, TEXT("Object array index or name index is invalid."));
		return false;
	}
	return true;
}

void UObjectBase::EmitBaseReferences(UClass *RootClass)
{
	static const FName ClassPropertyName(TEXT("Class"));
	static const FName OuterPropertyName(TEXT("Outer"));
	RootClass->EmitObjectReference(STRUCT_OFFSET(UObjectBase, Class), ClassPropertyName);
	RootClass->EmitObjectReference(STRUCT_OFFSET(UObjectBase, Outer), OuterPropertyName, GCRT_PersistentObject);
}

/** Enqueue the registration for this object. */
void UObjectBase::Register(const TCHAR* PackageName,const TCHAR* InName)
{
	FPendingRegistrant* PendingRegistration = new FPendingRegistrant(this);
	FPendingRegistrantInfo::GetMap().Add(this, FPendingRegistrantInfo(InName, PackageName));
	if(GLastPendingRegistrant)
	{
		GLastPendingRegistrant->NextAutoRegister = PendingRegistration;
	}
	else
	{
		check(!GFirstPendingRegistrant);
		GFirstPendingRegistrant = PendingRegistration;
	}
	GLastPendingRegistrant = PendingRegistration;
}


/**
 * Dequeues registrants from the list of pending registrations into an array.
 * The contents of the array is preserved, and the new elements are appended.
 */
static void DequeuePendingAutoRegistrants(TArray<FPendingRegistrant>& OutPendingRegistrants)
{
	// We process registrations in the order they were enqueued, since each registrant ensures
	// its dependencies are enqueued before it enqueues itself.
	FPendingRegistrant* NextPendingRegistrant = GFirstPendingRegistrant;
	GFirstPendingRegistrant = NULL;
	GLastPendingRegistrant = NULL;
	while(NextPendingRegistrant)
	{
		FPendingRegistrant* PendingRegistrant = NextPendingRegistrant;
		OutPendingRegistrants.Add(*PendingRegistrant);
		NextPendingRegistrant = PendingRegistrant->NextAutoRegister;
		delete PendingRegistrant;
	};
}

/**
 * Process the auto register objects adding them to the UObject array
 */
static void UObjectProcessRegistrants()
{
	check(UObjectInitialized());
	// Make list of all objects to be registered.
	TArray<FPendingRegistrant> PendingRegistrants;
	DequeuePendingAutoRegistrants(PendingRegistrants);

	for(int32 RegistrantIndex = 0;RegistrantIndex < PendingRegistrants.Num();++RegistrantIndex)
	{
		const FPendingRegistrant& PendingRegistrant = PendingRegistrants[RegistrantIndex];

		UObjectForceRegistration(PendingRegistrant.Object);

		check(PendingRegistrant.Object->GetClass()); // should have been set by DeferredRegister

		// Register may have resulted in new pending registrants being enqueued, so dequeue those.
		DequeuePendingAutoRegistrants(PendingRegistrants);
	}
}

void UObjectForceRegistration(UObjectBase* Object)
{
	FPendingRegistrantInfo* Info = FPendingRegistrantInfo::GetMap().Find(Object);
	if (Info)
	{
		const TCHAR* PackageName = Info->PackageName;
		const TCHAR* Name = Info->Name;
		FPendingRegistrantInfo::GetMap().Remove(Object);  // delete this first so that it doesn't try to do it twice
		Object->DeferredRegister(UClass::StaticClass(),PackageName,Name);
	}
}

/**
 * Struct containing the function pointer and package name of a UStruct to be registered with UObject system
 */
struct FPendingStructRegistrant
{	
	class UScriptStruct *(*RegisterFn)();
	const TCHAR* PackageName;

	FPendingStructRegistrant() {}
	FPendingStructRegistrant(class UScriptStruct *(*Fn)(), const TCHAR* InPackageName)
		: RegisterFn(Fn)
		, PackageName(InPackageName)
	{
	}
	FORCEINLINE bool operator==(const FPendingStructRegistrant& Other) const
	{
		return RegisterFn == Other.RegisterFn;
	}
};

static TArray<FPendingStructRegistrant>& GetDeferredCompiledInStructRegistration()
{
	static TArray<FPendingStructRegistrant> DeferredCompiledInRegistration;
	return DeferredCompiledInRegistration;
}

void UObjectCompiledInDeferStruct(class UScriptStruct *(*InRegister)(), const TCHAR* PackageName)
{
	// we do reregister StaticStruct in hot reload
	FPendingStructRegistrant Registrant(InRegister, PackageName);
	checkSlow(!GetDeferredCompiledInStructRegistration().Contains(Registrant));
	GetDeferredCompiledInStructRegistration().Add(Registrant);
}

#if WITH_HOT_RELOAD
struct FStructOrEnumCompiledInfo : FFieldCompiledInInfo
{
	FStructOrEnumCompiledInfo(SIZE_T InClassSize, uint32 InCrc)
		: FFieldCompiledInInfo(InClassSize, InCrc)
	{
	}

	virtual UClass* Register() const
	{
		return nullptr;
	}
};

/** Registered struct info (including size and reflection info) */
static TMap<FName, FStructOrEnumCompiledInfo*>& GetStructOrEnumGeneratedCodeInfo()
{
	static TMap<FName, FStructOrEnumCompiledInfo*> StructOrEnumCompiledInfoMap;
	return StructOrEnumCompiledInfoMap;
}
#endif

class UScriptStruct *GetStaticStruct(class UScriptStruct *(*InRegister)(), UObject* StructOuter, const TCHAR* StructName, SIZE_T Size, uint32 Crc)
{
#if WITH_HOT_RELOAD
	// Try to get the existing info for this struct
	const FName StructFName(StructName);
	FStructOrEnumCompiledInfo* Info = nullptr;
	auto ExistingInfo = GetStructOrEnumGeneratedCodeInfo().Find(StructFName);
	if (!ExistingInfo)
	{
		// New struct
		Info = new FStructOrEnumCompiledInfo(Size, Crc);
		Info->bHasChanged = true;
		GetStructOrEnumGeneratedCodeInfo().Add(StructFName, Info);
	}
	else
	{
		// Hot-relaoded struct, check if it has changes
		Info = *ExistingInfo;
		Info->bHasChanged = (*ExistingInfo)->Size != Size || (*ExistingInfo)->Crc != Crc;
		Info->Size = Size;
		Info->Crc = Crc;
	}

	if (GIsHotReload)
	{		
		if (!Info->bHasChanged)
		{
			// New struct added during hot-reload
			UScriptStruct* ReturnStruct = FindObject<UScriptStruct>(StructOuter, StructName);
			if (ReturnStruct)
			{
				UE_LOG(LogClass, Log, TEXT( "%s HotReload."), StructName);
				return ReturnStruct;
			}
			UE_LOG(LogClass, Log, TEXT("Could not find existing script struct %s for HotReload. Assuming new"), StructName);
		}
		else
		{
			// Existing struct, make sure we destroy the old one
			UScriptStruct* ExistingStruct = FindObject<UScriptStruct>(StructOuter, StructName);
			if (ExistingStruct)
			{
				// Make sure the old struct is not used by anything
				ExistingStruct->ClearFlags(RF_RootSet | RF_Standalone | RF_Public);
				const FName OldStructRename = MakeUniqueObjectName(GetTransientPackage(), ExistingStruct->GetClass(), *FString::Printf(TEXT("HOTRELOADED_%s"), StructName));
				ExistingStruct->Rename(*OldStructRename.ToString(), GetTransientPackage());
			}
		}
	}
#endif
	return (*InRegister)();
}

/**
 * Struct containing the function pointer and package name of a UEnum to be registered with UObject system
 */
struct FPendingEnumRegistrant
{
	class UEnum *(*RegisterFn)();
	const TCHAR* PackageName;

	FPendingEnumRegistrant() {}
	FPendingEnumRegistrant(class UEnum *(*Fn)(), const TCHAR* InPackageName)
		: RegisterFn(Fn)
		, PackageName(InPackageName)
	{
	}
	FORCEINLINE bool operator==(const FPendingEnumRegistrant& Other) const
	{
		return RegisterFn == Other.RegisterFn;
	}
};

// Same thing as GetDeferredCompiledInStructRegistration but for UEnums declared in header files without UClasses.
static TArray<FPendingEnumRegistrant>& GetDeferredCompiledInEnumRegistration()
{
	static TArray<FPendingEnumRegistrant> DeferredCompiledInRegistration;
	return DeferredCompiledInRegistration;
}

void UObjectCompiledInDeferEnum(class UEnum *(*InRegister)(), const TCHAR* PackageName)
{
	// we do reregister StaticStruct in hot reload
	FPendingEnumRegistrant Registrant(InRegister, PackageName);
	checkSlow(!GetDeferredCompiledInEnumRegistration().Contains(Registrant));
	GetDeferredCompiledInEnumRegistration().Add(Registrant);
}

class UEnum *GetStaticEnum(class UEnum *(*InRegister)(), UObject* EnumOuter, const TCHAR* EnumName)
{
#if WITH_HOT_RELOAD
	if (GIsHotReload)
	{
		UEnum* ReturnEnum = FindObjectChecked<UEnum>(EnumOuter, EnumName);
		if (ReturnEnum)
		{
			UE_LOG(LogClass, Log, TEXT( "%s HotReload."), EnumName);
			return ReturnEnum;
		}
		UE_LOG(LogClass, Log, TEXT("Could not find existing enum %s for HotReload. Assuming new"), EnumName);
	}
#endif
	return (*InRegister)();
}

static TArray<class UClass *(*)()>& GetDeferredCompiledInRegistration()
{
	static TArray<class UClass *(*)()> DeferredCompiledInRegistration;
	return DeferredCompiledInRegistration;
}

/** Classes loaded with a module, deferred until we register them all in one go */
static TArray<FFieldCompiledInInfo*>& GetDeferredClassRegistration()
{
	static TArray<FFieldCompiledInInfo*> DeferredClassRegistration;
	return DeferredClassRegistration;
}

#if WITH_HOT_RELOAD
/** Map of deferred class registration info (including size and reflection info) */
static TMap<FName, FFieldCompiledInInfo*>& GetDeferRegisterClassMap()
{
	static TMap<FName, FFieldCompiledInInfo*> DeferRegisterClassMap;
	return DeferRegisterClassMap;
}

/** Classes that changed during hot-reload and need to be re-instanced */
static TArray<FFieldCompiledInInfo*>& GetHotReloadClasses()
{
	static TArray<FFieldCompiledInInfo*> HotReloadClasses;
	return HotReloadClasses;
}
#endif

/** Removes prefix from the native class name */
FString RemoveClassPrefix(const TCHAR* ClassName)
{
	const TCHAR* DeprecatedPrefix = TEXT("DEPRECATED_");
	FString NameWithoutPrefix(ClassName);
	NameWithoutPrefix = NameWithoutPrefix.Mid(1);
	if (NameWithoutPrefix.StartsWith(DeprecatedPrefix))
	{
		NameWithoutPrefix = NameWithoutPrefix.Mid(FCString::Strlen(DeprecatedPrefix));
	}
	return NameWithoutPrefix;
}

/**
 * Helper class for updating VTables of hot reloaded classes.
 */
class FUClassVTableHelper
{
public:
	/**
	 * Adds UClasses VTable address if it's not cached yet.
	 *
	 * @param Class A UClass for which to add the VTable address.
	 */
	void TouchUClassVTable(UClass* Class)
	{
		auto* Found = VTableMap.Find(Class);

		if (Found != nullptr)
		{
			return;
		}

		VTableMap.Add(Class, GetUClassVTable(Class));
	}

	/**
	 * Changes old VTable address to the new one after hot reload.
	 *
	 * @param OldClass Old class.
	 * @param NewClass New class.
	 */
	void UpdateVTables(UClass* OldClass, UClass* NewClass)
	{
		UE_LOG(LogClass, Verbose, TEXT("Attempting to change VTable for class %s."), *OldClass->GetName());

		// We could do this later, but might as well get it before we start corrupting the object
		void* OldVTable = VTableMap[OldClass];
		void* NewVTable = GetUClassVTable(NewClass);

		if (NewVTable != OldVTable)
		{
			int32 Count = 0;
			for (FRawObjectIterator It; It; ++It)
			{
				UObject* Target = *It;
				if (OldVTable == *(void**)Target)
				{
					*(void**)Target = NewVTable;
					Count++;
				}
			}
			UE_LOG(LogClass, Verbose, TEXT("Updated the vtable for %d live objects. %016llx -> %016llx"), Count, PTRINT(OldVTable), PTRINT(NewVTable));

			VTableMap[OldClass] = NewVTable;
			VTableMap.Add(NewClass, NewVTable);
		}
		else
		{
			UE_LOG(LogClass, Error, TEXT("VTable for class %s did not change?"), *NewClass->GetName());
		}
	}

	/**
	 * Gets the singleton.
	 */
	static FUClassVTableHelper& Get()
	{
		static FUClassVTableHelper Helper;
		return Helper;
	}

private:
	/**
	 * Forbid public construction.
	 */
	FUClassVTableHelper() {}

	/**
	 * Gets UClasses VTable address.
	 *
	 * Note that this function creates an object of UClass type.
	 *
	 * @param Class A UClass for which to get VTable address.
	 *
	 * @returns VTable address.
	 */
	static void* GetUClassVTable(UClass* Class)
	{
		auto* ClassWithin = Class->ClassWithin;
		Class->ClassWithin = UPackage::StaticClass(); // We are just avoiding error checks with this... we don't care about this temp object other than to get the vtable.
		UObject* TempObjectForVTable = StaticConstructObject(Class, GetTransientPackage(), NAME_None, RF_NeedLoad | RF_ClassDefaultObject);

		if (!TempObjectForVTable->IsRooted())
		{
			TempObjectForVTable->MarkPendingKill();
		}
		else
		{
			UE_LOG(LogClass, Warning, TEXT("Hot Reload: Was not expecting temporary object '%s' for class '%s' to become rooted during construction. This object cannot be marked pending kill."), *TempObjectForVTable->GetFName().ToString(), *Class->GetName());
		}

		Class->ClassWithin = ClassWithin;

		return *(void**)TempObjectForVTable;
	}

	// A map of classes VTables.
	TMap<UClass*, void*> VTableMap;
};

void UClassCompiledInDefer(FFieldCompiledInInfo* ClassInfo, const TCHAR* Name, SIZE_T ClassSize, uint32 Crc)
{
	const FName CPPClassName(Name);
#if WITH_HOT_RELOAD
	// Check for existing classes
	auto ExistingClassInfo = GetDeferRegisterClassMap().Find(CPPClassName);
	ClassInfo->bHasChanged = !ExistingClassInfo || (*ExistingClassInfo)->Size != ClassInfo->Size || (*ExistingClassInfo)->Crc != ClassInfo->Crc;
	if (ExistingClassInfo)
	{
		// Class exists, this can only happen during hot-reload
		check(GIsHotReload);

		// Get the native name
		FString NameWithoutPrefix(RemoveClassPrefix(Name));
		auto ExistingClass = FindObjectChecked<UClass>(ANY_PACKAGE, *NameWithoutPrefix);

		if (ClassInfo->bHasChanged)
		{
			// Check in cache and create if doesn't exist UCLASSes VTable address.
			FUClassVTableHelper::Get().TouchUClassVTable(ExistingClass);

			// Rename the old class and move it to transient package
			ExistingClass->ClearFlags(RF_RootSet | RF_Standalone | RF_Public);
			ExistingClass->GetDefaultObject()->ClearFlags(RF_RootSet | RF_Standalone | RF_Public);
			const FName OldClassRename = MakeUniqueObjectName(GetTransientPackage(), ExistingClass->GetClass(), *FString::Printf(TEXT("HOTRELOADED_%s"), *NameWithoutPrefix));
			ExistingClass->Rename(*OldClassRename.ToString(), GetTransientPackage());
			ExistingClass->SetFlags(RF_Transient);
			ExistingClass->AddToRoot();

			// Make sure enums de-register their names BEFORE we create the new class, otherwise there will be name conflicts
			TArray<UObject*> ClassSubobjects;
			GetObjectsWithOuter(ExistingClass, ClassSubobjects);
			for (auto ClassSubobject : ClassSubobjects)
			{
				if (auto Enum = dynamic_cast<UEnum*>(ClassSubobject))
				{
					Enum->RemoveNamesFromMasterList();
				}
			}
		}
		ClassInfo->OldClass = ExistingClass;
		GetHotReloadClasses().Add(ClassInfo);

		*ExistingClassInfo = ClassInfo;
	}
	else
	{
		GetDeferRegisterClassMap().Add(CPPClassName, ClassInfo);
	}
#endif
	// We will either create a new class or update the static class pointer of the existing one
	GetDeferredClassRegistration().Add(ClassInfo);
}

void UObjectCompiledInDefer(class UClass *(*InRegister)(), const TCHAR* Name)
{
#if WITH_HOT_RELOAD
	// Either add all classes if not hot-reloading, or those which have changed
	if (!GIsHotReload || GetDeferRegisterClassMap().FindChecked(Name)->bHasChanged)
#endif
	{
		checkSlow(!GetDeferredCompiledInRegistration().Contains(InRegister));
		GetDeferredCompiledInRegistration().Add(InRegister);
	}
}

/** Register all loaded classes */
void UClassRegisterAllCompiledInClasses()
{
	for (auto Class : GetDeferredClassRegistration())
	{
		Class->Register();
	}
	GetDeferredClassRegistration().Empty();
}

#if WITH_HOT_RELOAD
/** Re-instance all existing classes that have changed during hot-reload */
void UClassReplaceHotReloadClasses()
{
	struct FClassTuple
	{
		UClass* OldClass;
		UClass* NewClass;

		FClassTuple(UClass* OldClass, UClass* NewClass)
			: OldClass(OldClass), NewClass(NewClass) { }
	};
	TArray<FClassTuple> VTablesToUpdate;

	for (auto Class : GetHotReloadClasses())
	{
		check(Class->OldClass);

		UClass* RegisteredClass = nullptr;
		if (Class->bHasChanged)
		{
			RegisteredClass = Class->Register();
			VTablesToUpdate.Add(FClassTuple(Class->OldClass, RegisteredClass));
		}

		if (FCoreUObjectDelegates::ReplaceHotReloadClassDelegate.IsBound())
		{
			FCoreUObjectDelegates::ReplaceHotReloadClassDelegate.Execute(Class->OldClass, RegisteredClass);
		}
	}

	for (auto Tuple : VTablesToUpdate)
	{
		FUClassVTableHelper::Get().UpdateVTables(Tuple.OldClass, Tuple.NewClass);
	}

	GetHotReloadClasses().Empty();
}
#endif

/**
 * Load any outstanding compiled in default properties
 */
static void UObjectLoadAllCompiledInDefaultProperties()
{
	const bool bHaveRegistrants = GetDeferredCompiledInRegistration().Num() != 0;
	if( bHaveRegistrants )
	{
		TArray<class UClass *(*)()> PendingRegistrants;
		TArray<UClass*> NewClasses;
		PendingRegistrants = GetDeferredCompiledInRegistration();
		GetDeferredCompiledInRegistration().Empty();
		for(int32 RegistrantIndex = 0;RegistrantIndex < PendingRegistrants.Num();++RegistrantIndex)
		{
			NewClasses.Add(PendingRegistrants[RegistrantIndex]());
		}
		for (int32 Index = 0; Index < NewClasses.Num(); Index++)
		{
			UClass* Class = NewClasses[Index];
			Class->GetDefaultObject();
		}
		FFeedbackContext& ErrorsFC = UClass::GetDefaultPropertiesFeedbackContext();
		if (ErrorsFC.Errors.Num() || ErrorsFC.Warnings.Num())
		{
			TArray<FString> All;
			All = ErrorsFC.Errors;
			All += ErrorsFC.Warnings;

			ErrorsFC.Errors.Empty();
			ErrorsFC.Warnings.Empty();

			FString AllInOne;
			UE_LOG(LogUObjectBase, Warning, TEXT("-------------- Default Property warnings and errors:"));
			for (int32 Index = 0; Index < All.Num(); Index++)
			{
				UE_LOG(LogUObjectBase, Warning, TEXT("%s"), *All[Index]);
				AllInOne += All[Index];
				AllInOne += TEXT("\n");
			}
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format( NSLOCTEXT("Core", "DefaultPropertyWarningAndErrors", "Default Property warnings and errors:\n{0}"), FText::FromString( AllInOne ) ) );
		}
	}
}

/**
 * Call StaticStruct for each struct...this sets up the internal singleton, and important works correctly with hot reload
 */
static void UObjectLoadAllCompiledInStructs()
{
	// Load Enums first
	const bool bHaveEnumRegistrants = GetDeferredCompiledInEnumRegistration().Num() != 0;
	if( bHaveEnumRegistrants )
	{
		TArray<FPendingEnumRegistrant> PendingRegistrants;
		PendingRegistrants = GetDeferredCompiledInEnumRegistration();
		GetDeferredCompiledInEnumRegistration().Empty();
		for (auto& EnumRegistrant : PendingRegistrants)
		{
			// Make sure the package exists in case it does not contain any UObjects
			CreatePackage(NULL, EnumRegistrant.PackageName);
			EnumRegistrant.RegisterFn();
		}
	}

	// Load Structs
	const bool bHaveRegistrants = GetDeferredCompiledInStructRegistration().Num() != 0;
	if( bHaveRegistrants )
	{
		TArray<FPendingStructRegistrant> PendingRegistrants;
		PendingRegistrants = GetDeferredCompiledInStructRegistration();
		GetDeferredCompiledInStructRegistration().Empty();
		for (auto& StructRegistrant : PendingRegistrants)
		{
			// Make sure the package exists in case it does not contain any UObjects or UEnums
			CreatePackage(NULL, StructRegistrant.PackageName);
			StructRegistrant.RegisterFn();
		}
	}
}

bool AnyNewlyLoadedUObjects()
{
	return GFirstPendingRegistrant != NULL || GetDeferredCompiledInRegistration().Num() || GetDeferredCompiledInStructRegistration().Num();
}


void ProcessNewlyLoadedUObjects()
{
	UClassRegisterAllCompiledInClasses();

	while( AnyNewlyLoadedUObjects() )
	{
		UObjectProcessRegistrants();
		UObjectLoadAllCompiledInStructs();
		UObjectLoadAllCompiledInDefaultProperties();		
	}
#if WITH_HOT_RELOAD
	UClassReplaceHotReloadClasses();
#endif
}


/**
 * Final phase of UObject initialization. all auto register objects are added to the main data structures.
 */
void UObjectBaseInit()
{
	// Zero initialize and later on get value from .ini so it is overridable per game/ platform...
	int32 MaxObjectsNotConsideredByGC	= 0;  
	int32 SizeOfPermanentObjectPool	= 0;

	// To properly set MaxObjectsNotConsideredByGC look for "Log: XXX objects as part of root set at end of initial load."
	// in your log file. This is being logged from LaunchEnglineLoop after objects have been added to the root set. 

	// Disregard for GC relies on seekfree loading for interaction with linkers. We also don't want to use it in the Editor, for which
	// FPlatformProperties::RequiresCookedData() will be false. Please note that GIsEditor and FApp::IsGame() are not valid at this point.
	if( FPlatformProperties::RequiresCookedData() )
	{
		GConfig->GetInt( TEXT("Core.System"), TEXT("MaxObjectsNotConsideredByGC"), MaxObjectsNotConsideredByGC, GEngineIni );

		// Not used on PC as in-place creation inside bigger pool interacts with the exit purge and deleting UObject directly.
		GConfig->GetInt( TEXT("Core.System"), TEXT("SizeOfPermanentObjectPool"), SizeOfPermanentObjectPool, GEngineIni );
	}

	// Log what we're doing to track down what really happens as log in LaunchEngineLoop doesn't report those settings in pristine form.
	UE_LOG(LogInit, Log, TEXT("Presizing for %i objects not considered by GC, pre-allocating %i bytes."), MaxObjectsNotConsideredByGC, SizeOfPermanentObjectPool );

	GUObjectAllocator.AllocatePermanentObjectPool(SizeOfPermanentObjectPool);
	GUObjectArray.AllocatePermanentObjectPool(MaxObjectsNotConsideredByGC);

	// Note initialized.
	Internal::GObjInitialized = true;

	UObjectProcessRegistrants();
}

/**
 * Final phase of UObject shutdown
 */
void UObjectBaseShutdown()
{
	GUObjectArray.ShutdownUObjectArray();
	Internal::GObjInitialized = false;
}

/**
 * Helper function that can be used inside the debuggers watch window. E.g. "DebugFName(Class)". 
 *
 * @param	Object	Object to look up the name for 
 * @return			Associated name
 */
const TCHAR* DebugFName(UObject* Object)
{
	// Hardcoded static array. This function is only used inside the debugger so it should be fine to return it.
	static TCHAR TempName[256];
	FName Name = Object->GetFName();
	FCString::Strcpy(TempName,Object ? *FName::SafeString(Name.GetDisplayIndex(), Name.GetNumber()) : TEXT("NULL"));
	return TempName;
}

/**
 * Helper function that can be used inside the debuggers watch window. E.g. "DebugFName(Object)". 
 *
 * @param	Object	Object to look up the name for 
 * @return			Fully qualified path name
 */
const TCHAR* DebugPathName(UObject* Object)
{
	if( Object )
	{
		// Hardcoded static array. This function is only used inside the debugger so it should be fine to return it.
		static TCHAR PathName[1024];
		PathName[0] = 0;

		// Keep track of how many outers we have as we need to print them in inverse order.
		UObject*	TempObject = Object;
		int32			OuterCount = 0;
		while( TempObject )
		{
			TempObject = TempObject->GetOuter();
			OuterCount++;
		}

		// Iterate over each outer + self in reverse oder and append name.
		for( int32 OuterIndex=OuterCount-1; OuterIndex>=0; OuterIndex-- )
		{
			// Move to outer name.
			TempObject = Object;
			for( int32 i=0; i<OuterIndex; i++ )
			{
				TempObject = TempObject->GetOuter();
			}

			// Dot separate entries.
			if( OuterIndex != OuterCount -1 )
			{
				FCString::Strcat( PathName, TEXT(".") );
			}
			// And app end the name.
			FCString::Strcat( PathName, DebugFName( TempObject ) );
		}

		return PathName;
	}
	else
	{
		return TEXT("None");
	}
}

/**
 * Helper function that can be used inside the debuggers watch window. E.g. "DebugFName(Object)". 
 *
 * @param	Object	Object to look up the name for 
 * @return			Fully qualified path name prepended by class name
 */
const TCHAR* DebugFullName(UObject* Object)
{
	if( Object )
	{
		// Hardcoded static array. This function is only used inside the debugger so it should be fine to return it.
		static TCHAR FullName[1024];
		FullName[0]=0;

		// Class Full.Path.Name
		FCString::Strcat( FullName, DebugFName(Object->GetClass()) );
		FCString::Strcat( FullName, TEXT(" "));
		FCString::Strcat( FullName, DebugPathName(Object) );

		return FullName;
	}
	else
	{
		return TEXT("None");
	}
}