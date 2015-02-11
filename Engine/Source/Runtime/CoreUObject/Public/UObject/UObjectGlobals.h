// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnObjGlobals.h: Unreal object system globals.
=============================================================================*/

#ifndef __UNOBJGLOBALS_H__
#define __UNOBJGLOBALS_H__

#include "Script.h"

COREUOBJECT_API DECLARE_LOG_CATEGORY_EXTERN(LogUObjectGlobals, Log, All);

DECLARE_CYCLE_STAT_EXTERN(TEXT("ConstructObject"),STAT_ConstructObject,STATGROUP_Object, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("AllocateObject"),STAT_AllocateObject,STATGROUP_Object, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("PostConstructInitializeProperties"),STAT_PostConstructInitializeProperties,STATGROUP_Object, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("LoadConfig"),STAT_LoadConfig,STATGROUP_Object, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("LoadObject"),STAT_LoadObject,STATGROUP_Object, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("InitProperties"),STAT_InitProperties,STATGROUP_Object, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("NameTable Entries"),STAT_NameTableEntries,STATGROUP_Object, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("NameTable ANSI Entries"),STAT_NameTableAnsiEntries,STATGROUP_Object, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("NameTable Wide Entries"),STAT_NameTableWideEntries,STATGROUP_Object, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("NameTable Memory Size"),STAT_NameTableMemorySize,STATGROUP_Object, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("~UObject"),STAT_DestroyObject,STATGROUP_Object, );

DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("FindObject"),STAT_FindObject,STATGROUP_ObjectVerbose, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("FindObjectFast"),STAT_FindObjectFast,STATGROUP_ObjectVerbose, );

/**
 * Network stats counters
 */

DECLARE_CYCLE_STAT_EXTERN(TEXT("NetSerializeFast Array"),STAT_NetSerializeFast_Array,STATGROUP_ServerCPU, COREUOBJECT_API);



#define	INVALID_OBJECT	(UObject*)-1

/** The type of a native function callable by script */
typedef void (UObject::*Native)( FFrame& TheStack, RESULT_DECL );



// Private system wide variables.

/** set while in SavePackage() to detect certain operations that are illegal while saving */
extern COREUOBJECT_API bool					GIsSavingPackage;
/** Imports for EndLoad optimization.									*/
extern int32						GImportCount;
/** Forced exports for EndLoad optimization.							*/
extern int32						GForcedExportCount;
/** Objects that might need preloading.									*/
extern TArray<UObject*>			GObjLoaded;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
/** Used to verify that the Super::Serialize chain is intact.			*/
extern TArray<UObject*,TInlineAllocator<16> >		DebugSerialize;
#endif



/*-----------------------------------------------------------------------------
	FObjectDuplicationParameters.
-----------------------------------------------------------------------------*/

/**
 * This struct is used for passing parameter values to the StaticDuplicateObject() method.  Only the constructor parameters are required to
 * be valid - all other members are optional.
 */
struct FObjectDuplicationParameters
{
	/**
	 * The object to be duplicated
	 */
	UObject*		SourceObject;

	/**
	 * The object to use as the Outer for the duplicate of SourceObject.
	 */
	UObject*		DestOuter;

	/**
	 * The name to use for the duplicate of SourceObject
	 */
	FName			DestName;

	/**
	 * a bitmask of EObjectFlags to propagate to the duplicate of SourceObject (and its subobjects).
	 */
	EObjectFlags	FlagMask;

	/**
	 * a bitmask of EObjectFlags to set on each duplicate object created.  Different from FlagMask in that only the bits
	 * from FlagMask which are also set on the source object will be set on the duplicate, while the flags in this value
	 * will always be set.
	 */
	EObjectFlags	ApplyFlags;

	/**
	 * Any PortFlags to be applied when serializing.
	 */
	uint32			PortFlags;

	/**
	 * optional class to specify for the destination object.
	 * @note: MUST BE SERIALIZATION COMPATIBLE WITH SOURCE OBJECT, AND DOES NOT WORK WELL FOR OBJECT WHICH HAVE COMPLEX COMPONENT HIERARCHIES!!!
	 */
	UClass*			DestClass;

	/**
	 * Objects to use for prefilling the dup-source => dup-target map used by StaticDuplicateObject.  Can be used to allow individual duplication of several objects that share
	 * a common Outer in cases where you don't want to duplicate the shared Outer but need references between the objects to be replaced anyway.
	 *
	 * Objects in this map will NOT be duplicated
	 * Key should be the source object; value should be the object which will be used as its duplicate.
	 */
	TMap<UObject*,UObject*>	DuplicationSeed;

	/**
	 * If non-NULL, this will be filled with the list of objects created during the call to StaticDuplicateObject.
	 *
	 * Key will be the source object; value will be the duplicated object
	 */
	TMap<UObject*,UObject*>* CreatedObjects;

	/**
	 * Constructor
	 */
	COREUOBJECT_API FObjectDuplicationParameters( UObject* InSourceObject, UObject* InDestOuter );
};

COREUOBJECT_API TArray<const TCHAR*> ParsePropertyFlags(uint64 Flags);

COREUOBJECT_API UPackage* GetTransientPackage();

COREUOBJECT_API bool ResolveName( UObject*& Outer, FString& Name, bool Create, bool Throw );
COREUOBJECT_API void SafeLoadError( UObject* Outer, uint32 LoadFlags, const TCHAR* Error, const TCHAR* Fmt, ... );

/**
 * Fast version of StaticFindObject that relies on the passed in FName being the object name
 * without any group/ package qualifiers.
 *
 * @param	Class			The to be found object's class
 * @param	InOuter			The to be found object's outer
 * @param	InName			The to be found object's class
 * @param	ExactClass		Whether to require an exact match with the passed in class
 * @param	AnyPackage		Whether to look in any package
 * @param	ExclusiveFlags	Ignores objects that contain any of the specified exclusive flags
 * @return	Returns a pointer to the found object or NULL if none could be found
 */
COREUOBJECT_API UObject* StaticFindObjectFast( UClass* Class, UObject* InOuter, FName InName, bool ExactClass=false, bool AnyPackage=false, EObjectFlags ExclusiveFlags=RF_NoFlags );
COREUOBJECT_API UObject* StaticFindObject( UClass* Class, UObject* InOuter, const TCHAR* Name, bool ExactClass=false );
COREUOBJECT_API UObject* StaticFindObjectChecked( UClass* Class, UObject* InOuter, const TCHAR* Name, bool ExactClass=false );
COREUOBJECT_API UObject* StaticFindObjectSafe( UClass* Class, UObject* InOuter, const TCHAR* Name, bool ExactClass=false );


/**
 * Parse an object from a text representation
 *
 * @param Stream		String containing text to parse
 * @param Class			Tag to search for object representation within string
 * @param Class			The class of the object to be loaded.
 * @param DestRes		returned uobject pointer
 * @param InParent		Outer to search
 *
 * @return true if the object parsed successfully
 */
COREUOBJECT_API bool ParseObject( const TCHAR* Stream, const TCHAR* Match, UClass* Class, UObject*& DestRes, UObject* InParent, bool* bInvalidObject=NULL );

/**
 * Find or load an object by string name with optional outer and filename specifications.
 * These are optional because the InName can contain all of the necessary information.
 *
 * @param ObjectClass	The class (or a superclass) of the object to be loaded.
 * @param InOuter		An optional object to narrow where to find/load the object from
 * @param InName		String name of the object. If it's not fully qualified, InOuter and/or Filename will be needed
 * @param Filename		An optional file to load from (or find in the file's package object)
 * @param LoadFlags		Flags controlling how to handle loading from disk
 * @param Sandbox		A list of packages to restrict the search for the object
 * @param bAllowObjectReconciliation	Whether to allow the object to be found via FindObject in the case of seek free loading
 *
 * @return The object that was loaded or found. NULL for a failure.
 */
COREUOBJECT_API UObject* StaticLoadObject( UClass* Class, UObject* InOuter, const TCHAR* Name, const TCHAR* Filename = NULL, uint32 LoadFlags = LOAD_None, UPackageMap* Sandbox = NULL, bool bAllowObjectReconciliation = true );
COREUOBJECT_API UClass* StaticLoadClass(UClass* BaseClass, UObject* InOuter, const TCHAR* Name, const TCHAR* Filename = NULL, uint32 LoadFlags = LOAD_None, UPackageMap* Sandbox = NULL);
/**
 * Create a new instance of an object.  The returned object will be fully initialized.  If InFlags contains RF_NeedsLoad (indicating that the object still needs to load its object data from disk), components
 * are not instanced (this will instead occur in PostLoad()).  The different between StaticConstructObject and StaticAllocateObject is that StaticConstructObject will also call the class constructor on the object
 * and instance any components.
 * 
 * @param	Class		the class of the object to create
 * @param	InOuter		the object to create this object within (the Outer property for the new object will be set to the value specified here).
 * @param	Name		the name to give the new object. If no value (NAME_None) is specified, the object will be given a unique name in the form of ClassName_#.
 * @param	SetFlags	the ObjectFlags to assign to the new object. some flags can affect the behavior of constructing the object.
 * @param	Template	if specified, the property values from this object will be copied to the new object, and the new object's ObjectArchetype value will be set to this object.
 *						If NULL, the class default object is used instead.
 * @param	bInCopyTransientsFromClassDefaults - if true, copy transient from the class defaults instead of the pass in archetype ptr (often these are the same)
 * @param	InstanceGraph
 *						contains the mappings of instanced objects and components to their templates
 *
 * @return	a pointer to a fully initialized object of the specified class.
 */
COREUOBJECT_API UObject* StaticConstructObject( UClass* Class, UObject* InOuter=(UObject*)GetTransientPackage(), FName Name=NAME_None, EObjectFlags SetFlags=RF_NoFlags, UObject* Template=NULL, bool bCopyTransientsFromClassDefaults=false, struct FObjectInstancingGraph* InstanceGraph=NULL );
/**
 * Creates a copy of SourceObject using the Outer and Name specified, as well as copies of all objects contained by SourceObject.  
 * Any objects referenced by SourceOuter or RootObject and contained by SourceOuter are also copied, maintaining their name relative to SourceOuter.  Any
 * references to objects that are duplicated are automatically replaced with the copy of the object.
 *
 * @param	SourceObject	the object to duplicate
 * @param	RootObject		should always be the same value as SourceObject (unused)
 * @param	DestOuter		the object to use as the Outer for the copy of SourceObject
 * @param	DestName		the name to use for the copy of SourceObject
 * @param	FlagMask		a bitmask of EObjectFlags that should be propagated to the object copies.  The resulting object copies will only have the object flags
 *							specified copied from their source object.
 * @param	DestClass		optional class to specify for the destination object. MUST BE SERIALIZATION COMPATIBLE WITH SOURCE OBJECT!!!
 * @param	bMigrateArchetypes unused
 *
 * @return	the duplicate of SourceObject.
 *
 * @note: this version is deprecated in favor of StaticDuplicateObjectEx
 */
enum EDuplicateForPie
{
	SDO_No_DuplicateForPie,
	SDO_DuplicateForPie,
};
COREUOBJECT_API UObject* StaticDuplicateObject(UObject const* SourceObject,UObject* DestOuter,const TCHAR* DestName,EObjectFlags FlagMask = RF_AllFlags,UClass* DestClass=NULL, EDuplicateForPie DuplicateForPIE = SDO_No_DuplicateForPie);
COREUOBJECT_API UObject* StaticDuplicateObjectEx( struct FObjectDuplicationParameters& Parameters );

/**
 * Performs UObject system pre-initialization. Depracated, do not use.
 */
COREUOBJECT_API void PreInitUObject();
/** 
 *   Iterate over all objects considered part of the root to setup GC optimizations
 */
COREUOBJECT_API void MarkObjectsToDisregardForGC();
COREUOBJECT_API bool StaticExec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
COREUOBJECT_API void StaticTick( float DeltaTime, bool bUseFullTimeLimit = true, float AsyncLoadingTime = 0.005f );

/**
 * Loads a package and all contained objects that match context flags.
 *
 * @param	InOuter		Package to load new package into (usually NULL or ULevel->GetOuter())
 * @param	Filename	Long package name to load
 * @param	LoadFlags	Flags controlling loading behavior
 * @return	Loaded package if successful, NULL otherwise
 */
COREUOBJECT_API UPackage* LoadPackage( UPackage* InOuter, const TCHAR* InLongPackageName, uint32 LoadFlags );

/**
 * Delegate called on completion of async package loading
 * @param	PackageName			Package name we were trying to load
 * @param	LoadedPackage		Loaded package if successful, NULL otherwise	
 */
DECLARE_DELEGATE_TwoParams(FLoadPackageAsyncDelegate, const FName& /*PackageName*/, UPackage* /*LoadedPackage*/)
struct FAsyncPackage;
/**
 * Asynchronously load a package and all contained objects that match context flags. Non- blocking.
 *
 * @param	PackageName			Name of package to load
 * @param	CompletionDelegate	Delegate called on completion of loading
 * @param	PackageGuid			GUID of the package to load, or NULL for "don't care"
 * @param	PackageType			A type name associated with this package for later use
 * @param	PackageToLoadFrom	If non-null, this is another package name. We load from this package name, into a (probably new) package named PackageName
 * @return	Handle for this async loading request
 */
COREUOBJECT_API FAsyncPackage& LoadPackageAsync( const FString& PackageName, FLoadPackageAsyncDelegate CompletionDelegate, const FGuid* RequiredGuid = NULL, FName PackageType = NAME_None, const TCHAR* PackageToLoadFrom = NULL );

/**
 * Asynchronously load a package and all contained objects that match context flags. Non- blocking.
 *
 * @param	PackageName			Name of package to load
 * @param	PackageGuid			GUID of the package to load, or NULL for "don't care"
 * @param	PackageType			A type name associated with this package for later use
 * @param	PackageToLoadFrom	If non-null, this is another package name. We load from this package name, into a (probably new) package named PackageName
 * @return	Handle for this async loading request
 */
COREUOBJECT_API FAsyncPackage& LoadPackageAsync( const FString& PackageName, const FGuid* RequiredGuid = NULL, FName PackageType = NAME_None, const TCHAR* PackageToLoadFrom = NULL );

/**
 * Returns the async load percentage for a package in flight with the passed in name or -1 if there isn't one.
 *
 * @param	PackageName			Name of package to query load percentage for
 * @return	Async load percentage if package is currently being loaded, -1 otherwise
 */
COREUOBJECT_API float GetAsyncLoadPercentage( const FName& PackageName );

/** 
 * Deletes all unreferenced objects, keeping objects that have any of the passed in KeepFlags set
 *
 * @param	KeepFlags			objects with those flags will be kept regardless of being referenced or not
 * @param	bPerformFullPurge	if true, perform a full purge after the mark pass
 */
COREUOBJECT_API void CollectGarbage( EObjectFlags KeepFlags, bool bPerformFullPurge = true );
COREUOBJECT_API void SerializeRootSet( FArchive& Ar, EObjectFlags KeepFlags );

/**
 * Returns whether an incremental purge is still pending/ in progress.
 *
 * @return	true if incremental purge needs to be kicked off or is currently in progress, false othwerise.
 */
COREUOBJECT_API bool IsIncrementalPurgePending();

/**
 * Incrementally purge garbage by deleting all unreferenced objects after routing Destroy.
 *
 * Calling code needs to be EXTREMELY careful when and how to call this function as 
 * RF_Unreachable cannot change on any objects unless any pending purge has completed!
 *
 * @param	bUseTimeLimit	whether the time limit parameter should be used
 * @param	TimeLimit		soft time limit for this function call
 */
COREUOBJECT_API void IncrementalPurgeGarbage( bool bUseTimeLimit, float TimeLimit = 0.002 );

/**
 * Create a unique name by combining a base name and an arbitrary number string.
 * The object name returned is guaranteed not to exist.
 *
 * @param	Parent		the outer for the object that needs to be named
 * @param	Class		the class for the object
 * @param	BaseName	optional base name to use when generating the unique object name; if not specified, the class's name is used
 *
 * @return	name is the form BaseName_##, where ## is the number of objects of this
 *			type that have been created since the last time the class was garbage collected.
 */
COREUOBJECT_API FName MakeUniqueObjectName( UObject* Outer, UClass* Class, FName BaseName=NAME_None );

/**
 * Given an actor label string, generates an FName that can be used as an object name for that label.
 * The generated name isn't guaranteed to be unique.  If the object's current name is already satisfactory, then
 * that name will be returned.
 *
 * @param	ActorLabel	The label string to convert to an FName
 * @param	CurrentObjectName	The object's current name, or NAME_None if it has no name yet
 *
 * @return	The generated actor object name
 */
COREUOBJECT_API FName MakeObjectNameFromActorLabel( const FString& InActorLabel, const FName CurrentObjectName );

/**
 * Returns whether an object is referenced, not counting the one
 * reference at Obj.
 *
 * @param	Obj			Object to check
 * @param	KeepFlags	Objects with these flags will be considered as being referenced
 * @param	bCheckSubObjects	Treat subobjects as if they are the same as passed in object
 * @param	FoundReferences		If non-NULL fill in with list of objects that hold references
 * @return true if object is referenced, false otherwise
 */
COREUOBJECT_API bool IsReferenced( UObject*& Res, EObjectFlags KeepFlags, bool bCheckSubObjects = false, FReferencerInformationList* FoundReferences = NULL );
/**
 * Blocks till all pending package/ linker requests are fulfilled.
 *
 * @param	ExcludeType		Do not flush packages associated with this specific type name
 */
COREUOBJECT_API void FlushAsyncLoading( FName ExcludeType = NAME_None );
/**
 * Returns whether we are currently async loading a package.
 *
 * @return true if we are async loading a package, false otherwise
 */
COREUOBJECT_API bool IsAsyncLoading();

/**
 * @return number of active async load package requests
 */
COREUOBJECT_API int32 GetNumAsyncPackages();

/**
 * Returns whether we are currently loading a package (sync or async)
 *
 * @return true if we are loading a package, false otherwise
 */
COREUOBJECT_API bool IsLoading();

/**
 * State of the async package after the last tick.
 */
namespace EAsyncPackageState
{
	enum Type
	{
		/** Package tick has timed out. */
		TimeOut = 0,
		/** Package has pending import packages that need to be streamed in. */
		PendingImports,
		/** Package has finished loading. */
		Complete
	};
}
/**
 * Serializes a bit of data each frame with a soft time limit. The function is designed to be able
 * to fully load a package in a single pass given sufficient time.
 *
 * @param	bUseTimeLimit	Whether to use a time limit
 * @param	bUseFullTimeLimit	If true, use the entire time limit even if blocked on I/O
 * @param	TimeLimit		Soft limit of time this function is allowed to consume
 * @param	ExcludeType		Do not process packages associated with this specific type name
 * @return The minimum state of any of the queued packages.
 */
COREUOBJECT_API EAsyncPackageState::Type ProcessAsyncLoading( bool bUseTimeLimit, bool bUseFullTimeLimit, float TimeLimit, FName ExcludeType = NAME_None );
COREUOBJECT_API void BeginLoad();
COREUOBJECT_API void EndLoad();

/**
 * Find an existing package by name
 * @param InOuter		The Outer object to search inside
 * @param PackageName	The name of the package to find
 *
 * @return The package if it exists
 */
COREUOBJECT_API UPackage* FindPackage(UObject* InOuter, const TCHAR* PackageName);

/**
 * Find an existing package by name or create it if it doesn't exist
 * @param InOuter		The Outer object to search inside
 * @return The existing package or a newly created one
 *
 */
COREUOBJECT_API UPackage* CreatePackage( UObject* InOuter, const TCHAR* PackageName );

void GlobalSetProperty( const TCHAR* Value, UClass* Class, UProperty* Property, bool bNotifyObjectOfChange );

/**
 * Save a copy of this object into the transaction buffer if we are currently recording into
 * one (undo/redo). If bMarkDirty is true, will also mark the package as needing to be saved.
 *
 * @param	bMarkDirty	If true, marks the package dirty if we are currently recording into a
 *						transaction buffer
 * @param	Object		object to save.
 *
 * @return	true if a copy of the object was saved and the package potentially marked dirty; false
 *			if we are not recording into a transaction buffer, the package is a PIE/script package,
 *			or the object is not transactional (implies the package was not marked dirty)
 */
COREUOBJECT_API bool SaveToTransactionBuffer(UObject* Object, bool bMarkDirty);


/**
 * Check for StaticAllocateObject error; only for use with the editor, make or other commandlets.
 * 
 * @param	Class		the class of the object to create
 * @param	InOuter		the object to create this object within (the Outer property for the new object will be set to the value specified here).
 * @param	Name		the name to give the new object. If no value (NAME_None) is specified, the object will be given a unique name in the form of ClassName_#.
 * @param	SetFlags	the ObjectFlags to assign to the new object. some flags can affect the behavior of constructing the object.
 * @return	true if NULL should be returned; there was a problem reported 
 */
bool StaticAllocateObjectErrorTests( UClass* Class, UObject* InOuter, FName Name, EObjectFlags SetFlags);

/**
 * Create a new instance of an object or replace an existing object.  If both an Outer and Name are specified, and there is an object already in memory with the same Class, Outer, and Name, the
 * existing object will be destructed, and the new object will be created in its place.
 * 
 * @param	Class		the class of the object to create
 * @param	InOuter		the object to create this object within (the Outer property for the new object will be set to the value specified here).
 * @param	Name		the name to give the new object. If no value (NAME_None) is specified, the object will be given a unique name in the form of ClassName_#.
 * @param	SetFlags	the ObjectFlags to assign to the new object. some flags can affect the behavior of constructing the object.
 * @param bCanReuseSubobjects	if set to true, SAO will not attempt to destroy a subobject if it already exists in memory.
 * @param bOutReusedSubobject	flag indicating if the object is a subobject that has already been created (in which case further initialization is not necessary).
 * @return	a pointer to a fully initialized object of the specified class.
 */
COREUOBJECT_API UObject* StaticAllocateObject( UClass* Class, UObject* InOuter, FName Name, EObjectFlags SetFlags, bool bCanReuseSubobjects = false, bool* bOutReusedSubobject = NULL);

/** Base class for TSubobjectPtr template. Holds the actual pointer and utility methods. */
class COREUOBJECT_API FSubobjectPtr
{
protected:

	enum EInvalidPtr
	{
		InvalidPtrValue = 3,
	};

	/** Subobject pointer. */
	UObject* Object;

	/** Constructor used by TSubobjectPtr. */
	explicit FSubobjectPtr(UObject* InObject)
		: Object(InObject)
	{
	}

	/** Sets the object pointer. Does runtime checks to see if the assignment is allowed.
	 * 
	 * @param InObject New subobject pointer.
	 */
	void Set(UObject* InObject);

public:
	/** Resets the internal pointer to NULL. */
	FORCEINLINE void Reset()
	{
		Set(NULL);
	}
	/** Gets the pointer to the subobject. */
	FORCEINLINE UObject* Get() const
	{
		return Object;
	}
	/** Checks if the subobject != NULL. */
	FORCEINLINE bool IsValid() const
	{
		return !!Object && Object != (UObject*)InvalidPtrValue;
	}
	/** Convenience operator. Does the same thing as IsValid(). */
	FORCEINLINE operator bool() const
	{
		return IsValid();
	}
	/** Compare against NULL */
	FORCEINLINE bool operator==(TYPE_OF_NULL Other) const
	{
		return !IsValid();
	}
	FORCEINLINE bool operator!=(TYPE_OF_NULL Other) const
	{
		return IsValid();
	}

	FORCEINLINE static bool IsInitialized(const UObject* Ptr)
	{
		return (Ptr != (UObject*)InvalidPtrValue);
	}
};

template<> struct TIsPODType<FSubobjectPtr> { enum { Value = true }; };
template<> struct TIsZeroConstructType<FSubobjectPtr> { enum { Value = true }; };
template<> struct TIsWeakPointerType<FSubobjectPtr> { enum { Value = false }; };

/**
 * TSubobjectPtr - Sub-object smart pointer, soon to be deprecated and should no longer be used.
 */
template <class SubobjectType>
class TSubobjectPtrDeprecated : public FSubobjectPtr
{
public:
	/** Internal constructors. */
	TSubobjectPtrDeprecated(SubobjectType* InObject)
		: FSubobjectPtr(InObject)
	{}
	TSubobjectPtrDeprecated& operator=(const TSubobjectPtrDeprecated& Other)
	{ 
		Set(Other.Object);
		return *this; 
	}

	/** Default constructor. */
	TSubobjectPtrDeprecated()
		: FSubobjectPtr((UObject*)FSubobjectPtr::InvalidPtrValue)
	{
		static_assert(sizeof(TSubobjectPtrDeprecated) == sizeof(UObject*), "TSuobjectPtr should equal pointer size.");
	}
	/** Copy constructor */
	template <class DerivedSubobjectType>
	TSubobjectPtrDeprecated(TSubobjectPtrDeprecated<DerivedSubobjectType>& Other)
		: FSubobjectPtr(Other.Object)
	{
		static_assert((CanConvertPointerFromTo<DerivedSubobjectType, SubobjectType>::Result), "Subobject pointers must be compatible.");
	}
	/** Gets the sub-object pointer. */
	FORCEINLINE SubobjectType* Get() const
	{
		return (SubobjectType*)Object;
	}
	/** Gets the sub-object pointer. */
	FORCEINLINE SubobjectType* operator->() const
	{
		return (SubobjectType*)Object;
	}
	/** Gets the sub-object pointer. */
	FORCEINLINE operator SubobjectType*() const
	{
		return (SubobjectType*)Object;
	}
};

template<class T> struct TIsPODType< TSubobjectPtrDeprecated<T> > { enum { Value = true }; };
template<class T> struct TIsZeroConstructType< TSubobjectPtrDeprecated<T> > { enum { Value = true }; };
template<class T> struct TIsWeakPointerType< TSubobjectPtrDeprecated<T> > { enum { Value = false }; };

#define TSubobjectPtr \
	EMIT_DEPRECATED_WARNING_MESSAGE("TSubobjectPtr is deprecated and should no longer be used. Please use pointers instead.") \
	TSubobjectPtrDeprecated
	

/**
 * Internal class to finalize UObject creation (initialize properties) after the real C++ constructor is called.
 **/
class COREUOBJECT_API FObjectInitializer
{
public:
	/**
	 * Default Constructor, used when you are using the C++ "new" syntax. UObject::UObject will set the object pointer
	 **/
	FObjectInitializer();

	/**
	 * Constructor
	 * @param	InObj object to initialize, from static allocate object, after construction
	 * @param	InObjectArchetype object to initialize properties from
	 * @param	bInCopyTransientsFromClassDefaults - if true, copy transient from the class defaults instead of the pass in archetype ptr (often these are the same)
	 * @param	bInShouldIntializeProps false is a special case for changing base classes in UCCMake
	 * @param	InInstanceGraph passed instance graph
	 */
	FObjectInitializer(UObject* InObj, UObject* InObjectArchetype, bool bInCopyTransientsFromClassDefaults, bool bInShouldIntializeProps, struct FObjectInstancingGraph* InInstanceGraph = NULL);

	~FObjectInitializer();

	/** 
	 * Return the archetype that this object will copy properties from later
	**/
	FORCEINLINE UObject* GetArchetype() const
	{
		return ObjectArchetype;
	}

	/**
	* Return the object that is being constructed
	**/
	FORCEINLINE UObject* GetObj() const
	{
		return Obj;
	}

	/**
	* Return the class of the object that is being constructed
	**/
	UClass* GetClass() const;

	/**
	 * Create a component or subobject
	 * @param	TReturnType					class of return type, all overrides must be of this type
	 * @param	Outer						outer to construct the subobject in
	 * @param	SubobjectName				name of the new component
	 * @param bTransient		true if the component is being assigned to a transient property
	 */
	template<class TReturnType>
	TReturnType* CreateDefaultSubobject(UObject* Outer, FName SubobjectName, bool bTransient = false) const
	{
		UClass* ReturnType = TReturnType::StaticClass();
		return static_cast<TReturnType*>(CreateDefaultSubobject(Outer, SubobjectName, ReturnType, ReturnType, /*bIsRequired =*/ true, /*bIsAbstract =*/ false, bTransient));
	}

	/**
	 * Create optional component or subobject. Optional subobjects may not get created
	 * when a derived class specified DoNotCreateDefaultSubobject with the subobject's name.
	 * @param	TReturnType					class of return type, all overrides must be of this type
	 * @param	Outer						outer to construct the subobject in
	 * @param	SubobjectName				name of the new component
	 * @param bTransient		true if the component is being assigned to a transient property
	 */
	template<class TReturnType>
	TReturnType* CreateOptionalDefaultSubobject(UObject* Outer, FName SubobjectName, bool bTransient = false) const
	{
		UClass* ReturnType = TReturnType::StaticClass();
		return static_cast<TReturnType*>(CreateDefaultSubobject(Outer, SubobjectName, ReturnType, ReturnType, /*bIsRequired =*/ false, /*bIsAbstract =*/ false, bTransient));
	}

	/**
	 * Create optional component or subobject. Optional subobjects may not get created
	 * when a derived class specified DoNotCreateDefaultSubobject with the subobject's name.
	 * @param	TReturnType					class of return type, all overrides must be of this type
	 * @param	Outer						outer to construct the subobject in
	 * @param	SubobjectName				name of the new component
	 * @param bTransient		true if the component is being assigned to a transient property
	 */
	template<class TReturnType>
	TReturnType* CreateAbstractDefaultSubobject(UObject* Outer, FName SubobjectName, bool bTransient = false) const
	{
		UClass* ReturnType = TReturnType::StaticClass();
		return static_cast<TReturnType*>(CreateDefaultSubobject(Outer, SubobjectName, ReturnType, ReturnType, /*bIsRequired =*/ true, /*bIsAbstract =*/ true, bTransient));
	}

	/** 
	* Create a component or subobject 
	* @param TReturnType class of return type, all overrides must be of this type 
	* @param TClassToConstructByDefault class to construct by default
	* @param Outer outer to construct the subobject in 
	* @param SubobjectName name of the new component 
	* @param bTransient		true if the component is being assigned to a transient property
	*/ 
	template<class TReturnType, class TClassToConstructByDefault> 
	TReturnType* CreateDefaultSubobject(UObject* Outer, FName SubobjectName, bool bTransient = false) const 
	{ 
		return static_cast<TReturnType*>(CreateDefaultSubobject(Outer, SubobjectName, TReturnType::StaticClass(), TClassToConstructByDefault::StaticClass(), /*bIsRequired =*/ true, /*bIsAbstract =*/ false, bTransient));
	}

	/**
	 * Create a component or subobject only to be used with the editor.
	 * @param	TReturnType					class of return type, all overrides must be of this type
	 * @param	Outer						outer to construct the subobject in
	 * @param	SubobjectName				name of the new component
	 * @param	bTransient					true if the component is being assigned to a transient property
	 */
	template<class TReturnType>
	TReturnType* CreateEditorOnlyDefaultSubobject(UObject* Outer, FName SubobjectName, bool bTransient = false) const
	{
		UClass* ReturnType = TReturnType::StaticClass();
		return static_cast<TReturnType*>(CreateEditorOnlyDefaultSubobject(Outer, SubobjectName, ReturnType, bTransient));
	}

	/**
	* Create a component or subobject only to be used with the editor.
	* @param	TReturnType					class of return type, all overrides must be of this type
	* @param	Outer						outer to construct the subobject in
	* @param	ReturnType					type of the new component 
	* @param	SubobjectName				name of the new component
	* @param	bTransient					true if the component is being assigned to a transient property
	*/
	UObject* CreateEditorOnlyDefaultSubobject(UObject* Outer, FName SubobjectName, UClass* ReturnType, bool bTransient = false) const;

	/**
	 * Create a component or subobject
	 * @param	TReturnType					class of return type, all overrides must be of this type
	 * @param	TClassToConstructByDefault	if the derived class has not overriden, create a component of this type (default is TReturnType)
	 * @param	Outer						outer to construct the subobject in
	 * @param	SubobjectName				name of the new component
	 * @param bIsRequired			true if the component is required and will always be created even if DoNotCreateDefaultSubobject was sepcified.
	 * @param bIsTransient		true if the component is being assigned to a transient property
	 */
	UObject* CreateDefaultSubobject(UObject* Outer, FName SubobjectFName, UClass* ReturnType, UClass* ClassToCreateByDefault, bool bIsRequired, bool bAbstract, bool bIsTransient) const;

	/**
	 * Sets the class of a subobject for a base class
	 * @param	SubobjectName	name of the new component or subobject
	 */
	template<class T>
	FObjectInitializer const& SetDefaultSubobjectClass(FName SubobjectName) const
	{
		AssertIfSubobjectSetupIsNotAllowed(*SubobjectName.GetPlainNameString());
		ComponentOverrides.Add(SubobjectName, T::StaticClass(), *this);
		return *this;
	}
	/**
	 * Sets the class of a subobject for a base class
	 * @param	SubobjectName	name of the new component or subobject
	 */
	template<class T>
	FORCEINLINE FObjectInitializer const& SetDefaultSubobjectClass(TCHAR const*SubobjectName) const
	{
		AssertIfSubobjectSetupIsNotAllowed(SubobjectName);
		ComponentOverrides.Add(SubobjectName, T::StaticClass(), *this);
		return *this;
	}

	/**
	 * Indicates that a base class should not create a component
	 * @param	SubobjectName	name of the new component or subobject to not create
	 */
	FObjectInitializer const& DoNotCreateDefaultSubobject(FName SubobjectName) const
	{
		AssertIfSubobjectSetupIsNotAllowed(*SubobjectName.GetPlainNameString());
		ComponentOverrides.Add(SubobjectName, NULL, *this);
		return *this;
	}

	/**
	 * Indicates that a base class should not create a component
	 * @param	ComponentName	name of the new component or subobject to not create
	 */
	FORCEINLINE FObjectInitializer const& DoNotCreateDefaultSubobject(TCHAR const*SubobjectName) const
	{
		AssertIfSubobjectSetupIsNotAllowed(SubobjectName);
		ComponentOverrides.Add(SubobjectName, NULL, *this);
		return *this;
	}

	/** 
	 * Internal use only, checks if the override is legal and if not deal with error messages
	**/
	bool IslegalOverride(FName InComponentName, class UClass *DerivedComponentClass, class UClass *BaseComponentClass) const;

	/**
	 * Asserts with the specified message if code is executed inside UObject constructor
	 **/
	static void AssertIfInConstructor(UObject* Outer, const TCHAR* ErrorMessage);

	FORCEINLINE void FinalizeSubobjectClassInitialization()
	{
		bSubobjectClassInitializationAllowed = false;
	}

	/** Gets ObjectInitializer for the currently constructed object. Can only be used inside of a constructor of UObject-derived class. */
	static FObjectInitializer& Get();

private:

	friend class UObject; 
	friend class FScriptIntegrationObjectHelper;

	template<class T>
	friend void InternalConstructor(const class FObjectInitializer& X);

	/**
	 * Binary initialize object properties to zero or defaults.
	 *
	 * @param	Obj					object to initialize data for
	 * @param	DefaultsClass		the class to use for initializing the data
	 * @param	DefaultData			the buffer containing the source data for the initialization
	 * @param	bCopyTransientsFromClassDefaults if true, copy the transients from the DefaultsClass defaults, otherwise copy the transients from DefaultData
	 */
	static void InitProperties(UObject* Obj, UClass* DefaultsClass, UObject* DefaultData, bool bCopyTransientsFromClassDefaults);

	bool IsInstancingAllowed() const;

	/**
	 * Calls InitProperties for any default subobjects created through this ObjectInitializer.
	 * @param bAllowInstancing	Indicates whether the object's components may be copied from their templates.
	 * @return true if there are any subobjects which require instancing.
	*/
	bool InitSubobjectProperties(bool bAllowInstancing) const;

	/**
	 * Create copies of the object's components from their templates.
	 * @param Class						Class of the object we are initializing
	 * @param bNeedInstancing			Indicates whether the object's components need to be instanced
	 * @param bNeedSubobjectInstancing	Indicates whether subobjects of the object's components need to be instanced
	 */
	void InstanceSubobjects(UClass* Class, bool bNeedInstancing, bool bNeedSubobjectInstancing) const;

	/** 
	 * Initializes a non-native property, according to the initialization rules. If the property is non-native
	 * and does not have a zero contructor, it is inialized with the default value.
	 * @param	Property			Property to be initialized
	 * @param	Data				Default data
	 * @return	Returns true if that property was a non-native one, otherwise false
	 */
	static bool InitNonNativeProperty(UProperty* Property, UObject* Data);
private:

	/**  Littel helper struct to manage overrides from dervied classes **/
	struct FOverrides
	{
		/**  Add an override, make sure it is legal **/
		void Add(FName InComponentName, UClass *InComponentClass, FObjectInitializer const& ObjectInitializer)
		{
			int32 Index = Find(InComponentName);
			if (Index == INDEX_NONE)
			{
				new (Overrides) FOverride(InComponentName, InComponentClass);
			}
			else if (InComponentClass && Overrides[Index].ComponentClass)
			{
				ObjectInitializer.IslegalOverride(InComponentName, Overrides[Index].ComponentClass, InComponentClass); // if a base class is asking for an override, the existing override (which we are going to use) had better be derived
			}
		}
		/**  Retrieve an override, or TClassToConstructByDefault::StaticClass or NULL if this was removed by a derived class **/
		UClass* Get(FName InComponentName, UClass* ReturnType, UClass* ClassToConstructByDefault, FObjectInitializer const& ObjectInitializer)
		{
			int32 Index = Find(InComponentName);
			UClass *BaseComponentClass = ClassToConstructByDefault;
			if (Index == INDEX_NONE)
			{
				return BaseComponentClass; // no override so just do what the base class wanted
			}
			else if (Overrides[Index].ComponentClass)
			{
				if (ObjectInitializer.IslegalOverride(InComponentName, Overrides[Index].ComponentClass, ReturnType)) // if THE base class is asking for a T, the existing override (which we are going to use) had better be derived
				{
					return Overrides[Index].ComponentClass; // the override is of an acceptable class, so use it
				}
				// else return NULL; this is a unacceptable override
			}
			return NULL;  // the override is of NULL, which means "don't create this component"
		}
private:
		/**  Search for an override **/
		int32 Find(FName InComponentName)
		{
			for (int32 Index = 0 ; Index < Overrides.Num(); Index++)
			{
				if (Overrides[Index].ComponentName == InComponentName)
				{
					return Index;
				}
			}
			return INDEX_NONE;
		}
		/**  Element of the override array **/
		struct FOverride
		{
			FName	ComponentName;
			UClass *ComponentClass;
			FOverride(FName InComponentName, UClass *InComponentClass)
				: ComponentName(InComponentName)
				, ComponentClass(InComponentClass)
			{
			}
		};
		/**  The override array **/
		TArray<FOverride, TInlineAllocator<8> > Overrides;
	};
	/**  Littel helper struct to manage overrides from dervied classes **/
	struct FSubobjectsToInit
	{
		/**  Add a subobject **/
		void Add(UObject* Subobject, UObject* Template)
		{
			for (int32 Index = 0; Index < SubobjectInits.Num(); Index++)
			{
				check(SubobjectInits[Index].Subobject != Subobject);
			}
			new (SubobjectInits) FSubobjectInit(Subobject, Template);
		}
		/**  Element of the SubobjectInits array **/
		struct FSubobjectInit
		{
			UObject* Subobject;
			UObject* Template;
			FSubobjectInit(UObject* InSubobject, UObject* InTemplate)
				: Subobject(InSubobject)
				, Template(InTemplate)
			{
			}
		};
		/**  The SubobjectInits array **/
		TArray<FSubobjectInit, TInlineAllocator<8> > SubobjectInits;
	};

	/** Asserts if SetDefaultSubobjectClass or DoNotCreateOptionalDefaultSuobject are called inside of the constructor body */
	void AssertIfSubobjectSetupIsNotAllowed(const TCHAR* SubobjectName) const;

	/**  object to initialize, from static allocate object, after construction **/
	UObject* Obj;
	/**  object to copy properties from **/
	UObject* ObjectArchetype;
	/**  if true, copy the transients from the DefaultsClass defaults, otherwise copy the transients from DefaultData **/
	bool bCopyTransientsFromClassDefaults;
	/**  If true, intialize the properties **/
	bool bShouldIntializePropsFromArchetype;
	/**  Only true until ObjectInitializer has not reached the base UObject class */
	bool bSubobjectClassInitializationAllowed;
	/**  Instance graph **/
	struct FObjectInstancingGraph* InstanceGraph;
	/**  List of component classes to override from derived classes **/
	mutable FOverrides ComponentOverrides;
	/**  List of component classes to intialize after the C++ constructors **/
	mutable FSubobjectsToInit ComponentInits;
	/**  Previously constructed object in the callstack */
	UObject* LastConstructedObject;
};

#define FPostConstructInitializeProperties \
	FObjectInitializer \
	EMIT_DEPRECATED_WARNING_MESSAGE("FPostConstructInitializeProperties is deprecated and was renamed to FObjectInitializer. Please use that type instead.")

/**
* Helper class for script integrations to access some UObject innards. Needed for script-generated UObject classes
*/
class FScriptIntegrationObjectHelper
{
public:
	/**
	* Binary initialize object properties to zero or defaults.
	*
	* @param	ObjectInitializer	FObjectInitializer helper
	* @param	Obj					object to initialize data for
	* @param	DefaultsClass		the class to use for initializing the data
	* @param	DefaultData			the buffer containing the source data for the initialization
	*/
	inline static void InitProperties(const FObjectInitializer& ObjectInitializer, UObject* Obj, UClass* DefaultsClass, UObject* DefaultData)
	{
		FObjectInitializer::InitProperties(Obj, DefaultsClass, DefaultData, ObjectInitializer.bCopyTransientsFromClassDefaults);
	}

	/**
	* Calls InitProperties for any default subobjects created through this ObjectInitializer.
	* @param bAllowInstancing	Indicates whether the object's components may be copied from their templates.
	* @return true if there are any subobjects which require instancing.
	*/
	inline static bool InitSubobjectProperties(const FObjectInitializer& ObjectInitializer)
	{
		return ObjectInitializer.InitSubobjectProperties(ObjectInitializer.IsInstancingAllowed());
	}

	/**
	* Create copies of the object's components from their templates.
	* @param ObjectInitializer			FObjectInitializer helper
	* @param Class						Class of the object we are initializing
	* @param bNeedInstancing			Indicates whether the object's components need to be instanced
	* @param bNeedSubobjectInstancing	Indicates whether subobjects of the object's components need to be instanced
	*/
	inline static void InstanceSubobjects(const FObjectInitializer& ObjectInitializer, UClass* Class, bool bNeedInstancing, bool bNeedSubobjectInstancing)
	{
		ObjectInitializer.InstanceSubobjects(Class, bNeedInstancing, bNeedSubobjectInstancing);
	}
};

/**
 * Helper class for deferred execution of 
*/

/**
 * Construct an object of a particular class.
 * 
 * @param	Class		the class of object to construct
 * @param	Outer		the outer for the new object.  If not specified, object will be created in the transient package.
 * @param	Name		the name for the new object.  If not specified, the object will be given a transient name via
 *						MakeUniqueObjectName
 * @param	SetFlags	the object flags to apply to the new object
 * @param	Template	the object to use for initializing the new object.  If not specified, the class's default object will
 *						be used
 * @param	bInCopyTransientsFromClassDefaults - if true, copy transient from the class defaults instead of the pass in archetype ptr (often these are the same)
 * @param	InstanceGraph
 *						contains the mappings of instanced objects and components to their templates
 *
 * @return	a pointer of type T to a new object of the specified class
 */
template< class T >
T* ConstructObject(UClass* Class, UObject* Outer = (UObject*)GetTransientPackage(), FName Name=NAME_None, EObjectFlags SetFlags=RF_NoFlags, UObject* Template=NULL, bool bCopyTransientsFromClassDefaults=false, struct FObjectInstancingGraph* InstanceGraph=NULL );

/**
 * Convenience template for constructing a gameplay object
 *
 * @param	Outer		the outer for the new object.  If not specified, object will be created in the transient package.
 * @param	Class		the class of object to construct
 */
template< class T >
T* NewObject(UObject* Outer=(UObject*)GetTransientPackage(),UClass* Class=T::StaticClass() )
{
	FObjectInitializer::AssertIfInConstructor(Outer, TEXT("NewObject can't be used to create default subobjects (inside of UObject derived class constructor) as it produces inconsistent object names. Use ObjectInitializer.CreateDefaultSuobject<> instead."));
	return ConstructObject<T>(Class,Outer);
}

/**
 * Convenience template for constructing a named object.
 *
 * @param	Outer	The outer for the new object.
 * @param	Name	The name of the new object.
 * @param	Flags	The object flags for the new object.
 */
template< class TClass >
TClass* NewNamedObject(UObject* Outer, FName Name, EObjectFlags Flags = RF_NoFlags, UObject* Template=NULL)
{
	return ConstructObject<TClass>(TClass::StaticClass(), Outer, Name, Flags, Template);
}


/**
 * Convenience template for duplicating an object
 *
 * @param SourceObject the object being copied
 * @param Outer the outer to use for the object
 * @param Name the optional name of the object
 *
 * @return the copied object or null if it failed for some reason
 */
template< class T >
T* DuplicateObject(T const* SourceObject,UObject* Outer,const TCHAR* Name = TEXT("None"))
{
	if (SourceObject != NULL)
	{
		if (Outer == NULL || Outer == INVALID_OBJECT)
		{
			Outer = (UObject*)GetTransientPackage();
		}
		return (T*)StaticDuplicateObject(SourceObject,Outer,Name);
	}
	return NULL;
}

/*----------------------------------------------------------------------------
	Core templates.
----------------------------------------------------------------------------*/

// Parse an object name in the input stream.
template< class T > 
inline bool ParseObject( const TCHAR* Stream, const TCHAR* Match, T*& Obj, UObject* Outer, bool* bInvalidObject=NULL )
{
	return ParseObject( Stream, Match, T::StaticClass(), (UObject*&)Obj, Outer, bInvalidObject );
}

// Find an optional object, relies on the name being unqualified
template< class T > 
inline T* FindObjectFast( UObject* Outer, FName Name, bool ExactClass=false, bool AnyPackage=false, EObjectFlags ExclusiveFlags=RF_NoFlags )
{
	return (T*)StaticFindObjectFast( T::StaticClass(), Outer, Name, ExactClass, AnyPackage, ExclusiveFlags );
}

// Find an optional object.
template< class T > 
inline T* FindObject( UObject* Outer, const TCHAR* Name, bool ExactClass=false )
{
	return (T*)StaticFindObject( T::StaticClass(), Outer, Name, ExactClass );
}

// Find an object, no failure allowed.
template< class T > 
inline T* FindObjectChecked( UObject* Outer, const TCHAR* Name, bool ExactClass=false )
{
	return (T*)StaticFindObjectChecked( T::StaticClass(), Outer, Name, ExactClass );
}

// Find an object without asserting on GIsSavingPackage or GIsGarbageCollecting
template< class T > 
inline T* FindObjectSafe( UObject* Outer, const TCHAR* Name, bool ExactClass=false )
{
	return (T*)StaticFindObjectSafe( T::StaticClass(), Outer, Name, ExactClass );
}


// Load an object.
template< class T > 
inline T* LoadObject( UObject* Outer, const TCHAR* Name, const TCHAR* Filename=NULL, uint32 LoadFlags=LOAD_None, UPackageMap* Sandbox=NULL )
{
	return (T*)StaticLoadObject( T::StaticClass(), Outer, Name, Filename, LoadFlags, Sandbox );
}

// Load a class object.
template< class T > 
inline UClass* LoadClass( UObject* Outer, const TCHAR* Name, const TCHAR* Filename, uint32 LoadFlags, UPackageMap* Sandbox )
{
	return StaticLoadClass( T::StaticClass(), Outer, Name, Filename, LoadFlags, Sandbox );
}

// Get default object of a class.
template< class T > 
inline const T* GetDefault()
{
	return (const T*)T::StaticClass()->GetDefaultObject();
}

// Get default object of a class.
template< class T > 
inline const T* GetDefault(UClass *Class);

// Get the default object of a class (mutable).
template< class T >
inline T* GetMutableDefault()
{
	return (T*)T::StaticClass()->GetDefaultObject();
}

// Get default object of a class (mutable).
template< class T > 
inline T* GetMutableDefault(UClass *Class);

/**
 * Determines whether the specified array contains objects of the specified class.
 *
 * @param	ObjectArray		the array to search - must be an array of pointers to instances of a UObject-derived class
 * @param	ClassToCheck	the object class to search for
 * @param	bExactClass		true to consider only those objects that have the class specified, or false to consider objects
 *							of classes derived from the specified SearhClass as well
 * @param	out_Objects		if specified, any objects that match the SearchClass will be added to this array
 */
template <class T>
bool ContainsObjectOfClass( const TArray<T*>& ObjectArray, UClass* ClassToCheck, bool bExactClass=false, TArray<T*>* out_Objects=NULL )
{
	bool bResult = false;
	for ( int32 ArrayIndex = 0; ArrayIndex < ObjectArray.Num(); ArrayIndex++ )
	{
		if ( ObjectArray[ArrayIndex] != NULL )
		{
			bool bMatchesSearchCriteria = bExactClass
				? ObjectArray[ArrayIndex]->GetClass() == ClassToCheck
				: ObjectArray[ArrayIndex]->IsA(ClassToCheck);

			if ( bMatchesSearchCriteria )
			{
				bResult = true;
				if ( out_Objects != NULL )
				{
					out_Objects->Add(ObjectArray[ArrayIndex]);
				}
				else
				{
					// if we don't need a list of objects that match the search criteria, we can stop as soon as we find at least one object of that class
					break;
				}
			}
		}
	}

	return bResult;
}

/**
 * Utility struct for restoring object flags for all objects.
 */
class FScopedObjectFlagMarker
{
	/**
	 * Map that tracks the ObjectFlags set on all objects; we use a map rather than iterating over all objects twice because FObjectIterator
	 * won't return objects that have RF_Unreachable set, and we may want to actually unset that flag.
	 */
	TMap<UObject*,EObjectFlags> StoredObjectFlags;
	
	/**
	 * Stores the object flags for all objects in the tracking array.
	 */
	void SaveObjectFlags();

	/**
	 * Restores the object flags for all objects from the tracking array.
	 */
	void RestoreObjectFlags();

public:
	/** Constructor */
	FScopedObjectFlagMarker()
	{
		SaveObjectFlags();
	}

	/** Destructor */
	~FScopedObjectFlagMarker()
	{
		RestoreObjectFlags();
	}
};


/**
  * Iterator for arrays of UObject pointers
  * @param TObjectClass		type of pointers contained in array
*/
template<class TObjectClass>
class TObjectArrayIterator
{
	/* sample code
	TArray<APawn *> TestPawns;
	...
	// test iterator, all items
	for ( TObjectArrayIterator<APawn> It(TestPawns); It; ++It )
	{
		UE_LOG(LogUObjectGlobals, Log, TEXT("Item %s"),*It->GetFullName());
	}
	*/
public:
	/**
		* Constructor, iterates all non-null, non pending kill objects, optionally of a particular class or base class
		* @param	InArray			the array to iterate on
		* @param	InClass			if non-null, will only iterate on items IsA this class
		* @param	InbExactClass	if true, will only iterate on exact matches
		*/
	FORCEINLINE TObjectArrayIterator( TArray<TObjectClass*>& InArray, UClass* InClassToCheck = NULL, bool InbExactClass = false) :	
		Array(InArray),
		Index(-1),
		ClassToCheck(InClassToCheck),
		bExactClass(InbExactClass)
	{
		Advance();
	}
	/**
		* Iterator advance
		*/
	FORCEINLINE void operator++()
	{
		Advance();
	}
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
	/**
		* Dereferences the iterator 
		* @return	the UObject at the iterator
	*/
	FORCEINLINE TObjectClass& operator*() const
	{
		checkSlow(GetObject());
		return *GetObject();
	}
	/**
		* Dereferences the iterator 
		* @return	the UObject at the iterator
	*/
	FORCEINLINE TObjectClass* operator->() const
	{
		checkSlow(GetObject());
		return GetObject();
	}

	/** 
	  * Removes the current element from the array, slower, but preserves the order. 
	  * Iterator is decremented for you so a loop will check all items.
	*/
	FORCEINLINE void RemoveCurrent()
	{
		Array.RemoveAt(Index--);
	}
	/** 
	  * Removes the current element from the array, faster, but does not preserves the array order. 
	  * Iterator is decremented for you so a loop will check all items.
	*/
	FORCEINLINE void RemoveCurrentSwap()
	{
		Array.RemoveSwap(Index--);
	}

protected:
	/**
		* Dereferences the iterator with an ordinary name for clarity in derived classes
		* @return	the UObject at the iterator
	*/
	FORCEINLINE TObjectClass* GetObject() const 
	{ 
		return Array(Index);
	}
	/**
		* Iterator advance with ordinary name for clarity in subclasses
		* @return	true if the iterator points to a valid object, false if iteration is complete
	*/
	FORCEINLINE bool Advance()
	{
		while(++Index < Array.Num())
		{
			TObjectClass* At = GetObject();
			if (
				IsValid(At) && 
				(!ClassToCheck ||
					(bExactClass
						? At->GetClass() == ClassToCheck
						: At->IsA(ClassToCheck)))
				)
			{
				return true;
			}
		}
		return false;
	}
private:
	/** The array that we are iterating on */
	TArray<TObjectClass*>&	Array;
	/** Index of the current element in the object array */
	int32						Index;
	/** Class using as a criteria */
	UClass*					ClassToCheck;
	/** Flag to require exact class matches */
	bool					bExactClass;
};

/**
 * FReferenceCollector.
 * Helper class used by the garbage collector to collect object references.
 */
class COREUOBJECT_API FReferenceCollector
{
public:
	/**
	 * Adds object reference.
	 *
	 * @param Object Referenced object.
	 * @param ReferencingObject Referencing object (if available).
	 * @param ReferencingProperty Referencing property (if available).
	 */
	template<class UObjectType>
	void AddReferencedObject(UObjectType*& Object, const UObject* ReferencingObject = NULL, const UObject* ReferencingProperty = NULL)
	{
		HandleObjectReference(*(UObject**)&Object, ReferencingObject, ReferencingProperty);
	}

	/**
	* Adds references to an array of objects.
	*
	* @param ObjectArray Referenced objects array.
	* @param ReferencingObject Referencing object (if available).
	* @param ReferencingProperty Referencing property (if available).
	*/
	template<class UObjectType>
	void AddReferencedObjects(TArray<UObjectType>& ObjectArray, const UObject* ReferencingObject = NULL, const UObject* ReferencingProperty = NULL)
	{
		for (auto& Object : ObjectArray)
		{
			HandleObjectReference(*(UObject**)&Object, ReferencingObject, ReferencingProperty);
		}
	}

	/**
	* Adds references to a set of objects.
	*
	* @param ObjectSet Referenced objects set.
	* @param ReferencingObject Referencing object (if available).
	* @param ReferencingProperty Referencing property (if available).
	*/
	template<class UObjectType>
	void AddReferencedObjects(TSet<UObjectType>& ObjectSet, const UObject* ReferencingObject = NULL, const UObject* ReferencingProperty = NULL)
	{
		for (auto& Object : ObjectSet)
		{
			HandleObjectReference(*(UObject**)&Object, ReferencingObject, ReferencingProperty);
		}
	}

private:

	/** Compile time check if a type can be automatically converted to another one */
	template<class From, class To>
	class CanConverFromTo
	{
		static uint8 Test(...);
		static uint16 Test(To);
	public:
		enum Type
		{
			Result = sizeof(Test(From())) - 1
		};
	};

	/**
	* Functions used by AddReferencedObject (TMap version). Adds references to UObjects, ignores value types
	*/
	template<class UObjectType>
	void AddReferencedObjectOrIgnoreValue(UObjectType& Object, const UObject* ReferencingObject, const UObject* ReferencingProperty)
	{
	}
	template<class UObjectType>
	void AddReferencedObjectOrIgnoreValue(UObjectType*& Object, const UObject* ReferencingObject, const UObject* ReferencingProperty)
	{
		HandleObjectReference(*(UObject**)&Object, ReferencingObject, ReferencingProperty);
	}

public:

	/**
	* Adds references to a map of objects.
	*
	* @param ObjectArray Referenced objects map.
	* @param ReferencingObject Referencing object (if available).
	* @param ReferencingProperty Referencing property (if available).
	*/
	template <typename TKeyType, typename TValueType>
	void AddReferencedObjects(TMap<TKeyType, TValueType>& Map, const UObject* ReferencingObject = NULL, const UObject* ReferencingProperty = NULL)
	{
		static_assert(CanConverFromTo<TKeyType, UObjectBase*>::Result || CanConverFromTo<TValueType, UObjectBase*>::Result, "At least one of TMap template types must be derived from UObject");
		for (auto& It : Map)
		{
			if (CanConverFromTo<TKeyType, UObjectBase*>::Result)
			{
				AddReferencedObjectOrIgnoreValue(It.Key, ReferencingObject, ReferencingProperty);
			}
			if (CanConverFromTo<TValueType, UObjectBase*>::Result)
			{
				AddReferencedObjectOrIgnoreValue(It.Value, ReferencingObject, ReferencingProperty);
			}
		}
	}

	/**
	 * If true archetype references should not be added to this collector.
	 */
	virtual bool IsIgnoringArchetypeRef() const = 0;
	/**
	 * If true transient objects should not be added to this collector.
	 */
	virtual bool IsIgnoringTransient() const = 0;
	/**
	 * Allows reference elimination by this collector.
	 */
	virtual void AllowEliminatingReferences(bool bAllow) {}
	/**
	* Sets the property that is currently being serialized
	*/
	virtual void SetSerializedProperty(class UProperty* Inproperty) {}
	/**
	* Gets the property that is currently being serialized
	*/
	virtual class UProperty* GetSerializedProperty() const { return nullptr; }
protected:
	/**
	 * Handle object reference. Called by AddReferencedObject.
	 *
	 * @param Object Referenced object.
	 * @param ReferencingObject Referencing object (if available).
	 * @param ReferencingProperty Referencing property (if available).
	 */
	virtual void HandleObjectReference(UObject*& Object, const UObject* ReferencingObject, const UObject* ReferencingProperty) = 0;
};

/**
 * FReferenceFinder.
 * Helper class used to collect object references.
 */
class COREUOBJECT_API FReferenceFinder : public FReferenceCollector
{
public:

	/**
	 * Constructor
	 *
	 * @param InObjectArray Array to add object references to
	 * @param	InOuter					value for LimitOuter
	 * @param	bInRequireDirectOuter	value for bRequireDirectOuter
	 * @param	bShouldIgnoreArchetype	whether to disable serialization of ObjectArchetype references
	 * @param	bInSerializeRecursively	only applicable when LimitOuter != NULL && bRequireDirectOuter==true;
	 *									serializes each object encountered looking for subobjects of referenced
	 *									objects that have LimitOuter for their Outer (i.e. nested subobjects/components)
	 * @param	bShouldIgnoreTransient	true to skip serialization of transient properties
	 */
	FReferenceFinder(TArray<UObject*>& InObjectArray, UObject* InOuter = NULL, bool bInRequireDirectOuter = true, bool bInShouldIgnoreArchetype = false, bool bInSerializeRecursively = false, bool bInShouldIgnoreTransient = false);

	/**
	 * Finds all objects referenced by Object.
	 *
	 * @param Object Object which references are to be found.
	 * @param ReferencingObject object that's referencing the current object.
	 * @param ReferencingProperty property the current object is being referenced through.
	 */
	virtual void FindReferences(UObject* Object, UObject* ReferencingObject = nullptr, UObject* ReferencingProperty = nullptr);	

	// FReferenceCollector interface.
	virtual void HandleObjectReference(UObject*& Object, const UObject* ReferencingObject, const UObject* InReferencingProperty) override;
	virtual bool IsIgnoringArchetypeRef() const override { return bShouldIgnoreArchetype; }
	virtual bool IsIgnoringTransient() const override { return bShouldIgnoreTransient; }
	virtual void SetSerializedProperty(class UProperty* Inproperty) override
	{
		SerializedProperty = Inproperty;
	}
	virtual class UProperty* GetSerializedProperty() const override
	{
		return SerializedProperty;
	}
protected:

	/** Stored reference to array of objects we add object references to. */
	TArray<UObject*>&		ObjectArray;
	/** List of objects that have been recursively serialized. */
	TSet<const UObject*>	SerializedObjects;
	/** Only objects within this outer will be considered, NULL value indicates that outers are disregarded. */
	UObject*		LimitOuter;
	/** Property that is referencing the current object */
	class UProperty* SerializedProperty;
	/** Determines whether nested objects contained within LimitOuter are considered. */
	bool			bRequireDirectOuter;
	/** Determines whether archetype references are considered. */
	bool			bShouldIgnoreArchetype;
	/** Determines whether we should recursively look for references of the referenced objects. */
	bool			bSerializeRecursively;
	/** Determines whether transient references are considered. */
	bool			bShouldIgnoreTransient;
};


/** Delegate types for source control package saving checks and adding package to default changelist */
DECLARE_DELEGATE_RetVal_TwoParams( bool, FCheckForAutoAddDelegate, UPackage*, const FString& );
DECLARE_DELEGATE_OneParam( FAddPackageToDefaultChangelistDelegate, const TCHAR* );

/**
 * Global CoreUObject delegates
 */
struct COREUOBJECT_API FCoreUObjectDelegates
{
	// Callback for object property modifications
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnObjectPropertyChanged, UObject*, struct FPropertyChangedEvent&);

	// Called when a property is changed
	static FOnObjectPropertyChanged OnObjectPropertyChanged;

	// Callback for PreEditChange
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPreObjectPropertyChanged, UObject*, const class FEditPropertyChain&);

	// Called before a property is changed
	static FOnPreObjectPropertyChanged OnPreObjectPropertyChanged;

	/** Delegate type for making auto backup of package */
	DECLARE_DELEGATE_RetVal_OneParam(bool, FAutoPackageBackupDelegate, const UPackage&);

#if WITH_EDITOR
	// Callback for all object modifications
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnObjectModified, UObject*);

	// Called when any object is modified at all 
	static FOnObjectModified OnObjectModified;

	// Set of objects modified this frame, to prevent multiple triggerings of the OnObjectModified delegate.
	static TSet<UObject*> ObjectsModifiedThisFrame;

	// Broadcast OnObjectModified if the broadcast hasn't ocurred for this object in this frame
	static void BroadcastOnObjectModified(UObject* Object)
	{
		if (OnObjectModified.IsBound() && !ObjectsModifiedThisFrame.Contains(Object))
		{
			ObjectsModifiedThisFrame.Add(Object);
			OnObjectModified.Broadcast(Object);
		}
	}

	// Callback for when an asset is loaded (Editor)
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAssetLoaded, UObject*);

	// Called when an asset is loaded
	static FOnAssetLoaded OnAssetLoaded;

	// Callback for when an asset is saved (Editor)
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnObjectSaved, UObject*);

	// Called when an asset is saved
	static FOnObjectSaved OnObjectSaved;

#endif	//WITH_EDITOR

	// Delegate type for redirector followed events ( Params: const FString& PackageName, UObject* Redirector )
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnRedirectorFollowed, const FString&, UObject*);

	// Sent when a UObjectRedirector was followed to find the destination object
	static FOnRedirectorFollowed RedirectorFollowed;

	/** Delegate used by SavePackage() to create the package backup */
	static FAutoPackageBackupDelegate AutoPackageBackupDelegate;

	/** Delegate type for saving check */
	DECLARE_DELEGATE_RetVal_ThreeParams(bool, FIsPackageOKToSaveDelegate, UPackage*, const FString&, FOutputDevice*);

	/** Delegate used by SavePackage() to check whether a package should be saved */
	static FIsPackageOKToSaveDelegate IsPackageOKToSaveDelegate;

	/** Delegate for replacing hot-reloaded classes that changed after hot-reload */
	DECLARE_DELEGATE_TwoParams(FReplaceHotReloadClassDelegate, UClass*, UClass*);
	static FReplaceHotReloadClassDelegate ReplaceHotReloadClassDelegate;

	// Sent at the very beginning of LoadMap
	static FSimpleMulticastDelegate PreLoadMap;

	// Sent at the _successful_ end of LoadMap
	static FSimpleMulticastDelegate PostLoadMap;

	// Sent at the _successful_ end of LoadMap
	static FSimpleMulticastDelegate PostDemoPlay;

	// Called before garbage collection
	static FSimpleMulticastDelegate PreGarbageCollect;

	// Called after garbage collection
	static FSimpleMulticastDelegate PostGarbageCollect;

	/** delegate type for querying whether a loaded object should replace an already existing one */
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnLoadObjectsOnTop, const FString&);

	/** Queries whether an object should be loaded on top ( replace ) an already existing one */
	static FOnLoadObjectsOnTop ShouldLoadOnTop;

	/** called when loading a string asset reference */
	DECLARE_DELEGATE_OneParam(FStringAssetReferenceLoaded, FString const& /*LoadedAssetLongPathname*/);
	static FStringAssetReferenceLoaded StringAssetReferenceLoaded;

	/** called when path to world root is changed */
	DECLARE_MULTICAST_DELEGATE_OneParam(FPackageCreatedForLoad, class UPackage*);
	static FPackageCreatedForLoad PackageCreatedForLoad;

	/** called when saving a string asset reference, can replace the value with something else */
	DECLARE_DELEGATE_RetVal_OneParam(FString, FStringAssetReferenceSaving, FString const& /*SavingAssetLongPathname*/);
	static FStringAssetReferenceSaving StringAssetReferenceSaving;
};

/** Allows release builds to override not verifying GC assumptions. Useful for profiling as it's hitchy. */
extern COREUOBJECT_API bool GShouldVerifyGCAssumptions;

/** A struct used as stub for deleted ones. */
COREUOBJECT_API UScriptStruct* GetFallbackStruct();

#endif	// __UNOBJGLOBALS_H__
