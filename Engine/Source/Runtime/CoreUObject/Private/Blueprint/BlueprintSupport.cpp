// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "CoreUObjectPrivate.h"
#include "UObject/LinkerPlaceholderClass.h"
#include "Linker.h"
#include "PropertyTag.h"


/** 
 * Defined in BlueprintSupport.cpp
 * Duplicates all fields of a class in depth-first order. It makes sure that everything contained
 * in a class is duplicated before the class itself, as well as all function parameters before the
 * function itself.
 *
 * @param	StructToDuplicate			Instance of the struct that is about to be duplicated
 * @param	Writer						duplicate writer instance to write the duplicated data to
 */
void FBlueprintSupport::DuplicateAllFields(UStruct* StructToDuplicate, FDuplicateDataWriter& Writer)
{
	// This is a very simple fake topological-sort to make sure everything contained in the class
	// is processed before the class itself is, and each function parameter is processed before the function
	if (StructToDuplicate)
	{
		// Make sure each field gets allocated into the array
		for (TFieldIterator<UField> FieldIt(StructToDuplicate, EFieldIteratorFlags::ExcludeSuper); FieldIt; ++FieldIt)
		{
			UField* Field = *FieldIt;

			// Make sure functions also do their parameters and children first
			if (UFunction* Function = dynamic_cast<UFunction*>(Field))
			{
				for (TFieldIterator<UField> FunctionFieldIt(Function, EFieldIteratorFlags::ExcludeSuper); FunctionFieldIt; ++FunctionFieldIt)
				{
					UField* InnerField = *FunctionFieldIt;
					Writer.GetDuplicatedObject(InnerField);
				}
			}

			Writer.GetDuplicatedObject(Field);
		}
	}
}

bool FBlueprintSupport::UseDeferredDependencyLoading()
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	static const FBoolConfigValueHelper DeferDependencyLoads(TEXT("Kismet"), TEXT("bDeferDependencyLoads"), GEngineIni);
	return DeferDependencyLoads;
#else
	return false;
#endif
}


bool FBlueprintSupport::IsResolvingDeferredDependenciesDisabled()
{
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	static const FBoolConfigValueHelper NoDeferredDependencyResolves(TEXT("Kismet"), TEXT("bForceDisableDeferredDependencyResolving"), GEngineIni);
	return !UseDeferredDependencyLoading() || NoDeferredDependencyResolves;
#else 
	return false;
#endif
}

bool FBlueprintSupport::IsDeferredCDOSerializationDisabled()
{
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	static const FBoolConfigValueHelper NoDeferredCDOLoading(TEXT("Kismet"), TEXT("bForceDisableDeferredCDOLoading"), GEngineIni);
	return !UseDeferredDependencyLoading() || NoDeferredCDOLoading;
#else 
	return false;
#endif
}

/*******************************************************************************
 * FScopedClassDependencyGather
 ******************************************************************************/

#if WITH_EDITOR
UClass* FScopedClassDependencyGather::BatchMasterClass = NULL;
TArray<UClass*> FScopedClassDependencyGather::BatchClassDependencies;

FScopedClassDependencyGather::FScopedClassDependencyGather(UClass* ClassToGather)
	: bMasterClass(false)
{
	// Do NOT track duplication dependencies, as these are intermediate products that we don't care about
	if( !GIsDuplicatingClassForReinstancing )
	{
		if( BatchMasterClass == NULL )
		{
			// If there is no current dependency master, register this class as the master, and reset the array
			BatchMasterClass = ClassToGather;
			BatchClassDependencies.Empty();
			bMasterClass = true;
		}
		else
		{
			// This class was instantiated while another class was gathering dependencies, so record it as a dependency
			BatchClassDependencies.AddUnique(ClassToGather);
		}
	}
}

FScopedClassDependencyGather::~FScopedClassDependencyGather()
{
	// If this gatherer was the initial gatherer for the current scope, process 
	// dependencies (unless compiling on load is explicitly disabled)
	if( bMasterClass && !GForceDisableBlueprintCompileOnLoad )
	{
		for( auto DependencyIter = BatchClassDependencies.CreateIterator(); DependencyIter; ++DependencyIter )
		{
			UClass* Dependency = *DependencyIter;
			if( Dependency->ClassGeneratedBy != BatchMasterClass->ClassGeneratedBy )
			{
				Dependency->ConditionalRecompileClass(&GObjLoaded);
			}
		}

		// Finally, recompile the master class to make sure it gets updated too
		BatchMasterClass->ConditionalRecompileClass(&GObjLoaded);
		
		BatchMasterClass = NULL;
	}
}

TArray<UClass*> const& FScopedClassDependencyGather::GetCachedDependencies()
{
	return BatchClassDependencies;
}
#endif //WITH_EDITOR



/*******************************************************************************
 * ULinkerLoad
 ******************************************************************************/

// rather than littering the code with USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
// checks, let's just define DEFERRED_DEPENDENCY_CHECK for the file
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	#define DEFERRED_DEPENDENCY_CHECK(CheckExpr) check(CheckExpr)
#else  // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	#define DEFERRED_DEPENDENCY_CHECK(CheckExpr)
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
 
struct FPreloadMembersHelper
{
	static void PreloadMembers(UObject* InObject)
	{
		// Collect a list of all things this element owns
		TArray<UObject*> BPMemberReferences;
		FReferenceFinder ComponentCollector(BPMemberReferences, InObject, false, true, true, true);
		ComponentCollector.FindReferences(InObject);

		// Iterate over the list, and preload everything so it is valid for refreshing
		for (TArray<UObject*>::TIterator it(BPMemberReferences); it; ++it)
		{
			UObject* CurrentObject = *it;
			if (!CurrentObject->HasAnyFlags(RF_LoadCompleted))
			{
				CurrentObject->SetFlags(RF_NeedLoad);
				if (auto Linker = CurrentObject->GetLinker())
				{
					Linker->Preload(CurrentObject);
					PreloadMembers(CurrentObject);
				}
			}
		}
	}

	static void PreloadObject(UObject* InObject)
	{
		if (InObject && !InObject->HasAnyFlags(RF_LoadCompleted))
		{
			InObject->SetFlags(RF_NeedLoad);
			if (ULinkerLoad* Linker = InObject->GetLinker())
			{
				Linker->Preload(InObject);
			}
		}
	}
};

/**
 * Regenerates/Refreshes a blueprint class
 *
 * @param	LoadClass		Instance of the class currently being loaded and which is the parent for the blueprint
 * @param	ExportObject	Current object being exported
 * @return	Returns true if regeneration was successful, otherwise false
 */
bool ULinkerLoad::RegenerateBlueprintClass(UClass* LoadClass, UObject* ExportObject)
{
	// determine if somewhere further down the callstack, we're already in this
	// function for this class
	bool const bAlreadyRegenerating = LoadClass->ClassGeneratedBy->HasAnyFlags(RF_BeingRegenerated);
	// Flag the class source object, so we know we're already in the process of compiling this class
	LoadClass->ClassGeneratedBy->SetFlags(RF_BeingRegenerated);

	// Cache off the current CDO, and specify the CDO for the load class 
	// manually... do this before we Preload() any children members so that if 
	// one of those preloads subsequently ends up back here for this class, 
	// then the ExportObject is carried along and used in the eventual RegenerateClass() call
	UObject* CurrentCDO = ExportObject;
	check(!bAlreadyRegenerating || (LoadClass->ClassDefaultObject == ExportObject));
	LoadClass->ClassDefaultObject = CurrentCDO;

	// Finish loading the class here, so we have all the appropriate data to copy over to the new CDO
	TArray<UObject*> AllChildMembers;
	GetObjectsWithOuter(LoadClass, /*out*/ AllChildMembers);
	for (int32 Index = 0; Index < AllChildMembers.Num(); ++Index)
	{
		UObject* Member = AllChildMembers[Index];
		Preload(Member);
	}

	// if this was subsequently regenerated from one of the above preloads, then 
	// we don't have to finish this off, it was already done
	bool const bWasSubsequentlyRegenerated = !LoadClass->ClassGeneratedBy->HasAnyFlags(RF_BeingRegenerated);
	// @TODO: find some other condition to block this if we've already  
	//        regenerated the class (not just if we've regenerated the class 
	//        from an above Preload(Member))... UBlueprint::RegenerateClass() 
	//        has an internal conditional to block getting into it again, but we
	//        can't check UBlueprint members from this module
	if (!bWasSubsequentlyRegenerated)
	{
		Preload(LoadClass);

		LoadClass->StaticLink(true);
		Preload(CurrentCDO);

		// Load the class config values
		if( LoadClass->HasAnyClassFlags( CLASS_Config ))
		{
			CurrentCDO->LoadConfig( LoadClass );
		}

		// Make sure that we regenerate any parent classes first before attempting to build a child
		TArray<UClass*> ClassChainOrdered;
		{
			// Just ordering the class hierarchy from root to leafs:
			UClass* ClassChain = LoadClass->GetSuperClass();
			while (ClassChain && ClassChain->ClassGeneratedBy)
			{
				// O(n) insert, but n is tiny because this is a class hierarchy...
				ClassChainOrdered.Insert(ClassChain, 0);
				ClassChain = ClassChain->GetSuperClass();
			}
		}
		for (auto Class : ClassChainOrdered)
		{
			UObject* BlueprintObject = Class->ClassGeneratedBy;
			if (BlueprintObject && BlueprintObject->HasAnyFlags(RF_BeingRegenerated))
			{
				// Always load the parent blueprint here in case there is a circular dependency. This will
				// ensure that the blueprint is fully serialized before attempting to regenerate the class.
				FPreloadMembersHelper::PreloadObject(BlueprintObject);
			
				FPreloadMembersHelper::PreloadMembers(BlueprintObject);
				// recurse into this function for this parent class; 
				// 'ClassDefaultObject' should be the class's original ExportObject
				RegenerateBlueprintClass(Class, Class->ClassDefaultObject);
			}
		}

		{
			UObject* BlueprintObject = LoadClass->ClassGeneratedBy;
			// Preload the blueprint to make sure it has all the data the class needs for regeneration
			FPreloadMembersHelper::PreloadObject(BlueprintObject);

			UClass* RegeneratedClass = BlueprintObject->RegenerateClass(LoadClass, CurrentCDO, GObjLoaded);
			if (RegeneratedClass)
			{
				BlueprintObject->ClearFlags(RF_BeingRegenerated);
				// Fix up the linker so that the RegeneratedClass is used
				LoadClass->ClearFlags(RF_NeedLoad | RF_NeedPostLoad);
			}
		}
	}

	bool const bSuccessfulRegeneration = !LoadClass->ClassGeneratedBy->HasAnyFlags(RF_BeingRegenerated);
	// if this wasn't already flagged as regenerating when we first entered this 
	// function, the clear it ourselves.
	if (!bAlreadyRegenerating)
	{
		LoadClass->ClassGeneratedBy->ClearFlags(RF_BeingRegenerated);
	}

	return bSuccessfulRegeneration;
}

bool ULinkerLoad::DeferPotentialCircularImport(const int32 Index)
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (!FBlueprintSupport::UseDeferredDependencyLoading())
	{
		return false;
	}

	//--------------------------------------
	// Phase 1: Stub in Dependencies
	//--------------------------------------

	FObjectImport& Import = ImportMap[Index];
	if (Import.XObject != nullptr)
	{
		bool const bIsPlaceHolderClass = Import.XObject->IsA<ULinkerPlaceholderClass>();
		return bIsPlaceHolderClass;
	}

	if ((LoadFlags & LOAD_DeferDependencyLoads) && !IsImportNative(Index))
	{
		if (UObject* ClassPackage = FindObject<UPackage>(/*Outer =*/nullptr, *Import.ClassPackage.ToString()))
		{
			if (const UClass* ImportClass = FindObject<UClass>(ClassPackage, *Import.ClassName.ToString()))
			{
				bool const bIsBlueprintClass = ImportClass->IsChildOf<UClass>();
				// @TODO: if we could see if the related package is created 
				//        (without loading it further), AND it already has this 
				//        import loaded, then we can probably return that rather 
				//        than allocating a placeholder
				if (bIsBlueprintClass)
				{
					UPackage* PlaceholderOuter = LinkerRoot;
					UClass*   PlaceholderType  = ULinkerPlaceholderClass::StaticClass();

					FName PlaceholderName(*FString::Printf(TEXT("PLACEHOLDER-CLASS_%s"), *Import.ObjectName.ToString()));
					PlaceholderName = MakeUniqueObjectName(PlaceholderOuter, PlaceholderType, PlaceholderName);

					ULinkerPlaceholderClass* Placeholder = ConstructObject<ULinkerPlaceholderClass>(PlaceholderType, PlaceholderOuter, PlaceholderName, RF_Public | RF_Transient);
					// store the import index in the placeholder, so we can 
					// easily look it up in the import map, given the 
					// placeholder (needed, to find the corresponding import for 
					// ResolvingDeferredPlaceholder)
					Placeholder->ImportIndex = Index;
					// make sure the class is fully formed (has its 
					// ClassAddReferencedObjects/ClassConstructor members set)
					Placeholder->Bind();
					Placeholder->StaticLink(/*bRelinkExistingProperties =*/true);

					Import.XObject = Placeholder;
				}
			}
		}
	}
	return (Import.XObject != nullptr);
#else // !USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	return false;
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
}

/**
 * This utility struct helps track blueprint structs/linkers that are currently 
 * in the  middle of a call to ResolveDeferredDependencies(). This can be used  
 * to know if a dependency's resolve needs to be finished (to avoid unwanted 
 * placeholder references ending up in script-code).
 */
struct FUnresolvedStructTracker
{
public:
	/** Marks the specified struct (and its linker) as "resolving" for the lifetime of this instance */
	FUnresolvedStructTracker(UStruct* LoadStruct)
		: TrackedStruct(LoadStruct)
	{
		DEFERRED_DEPENDENCY_CHECK((LoadStruct != nullptr) && (LoadStruct->GetLinker() != nullptr));
		UnresolvedStructs.Add(LoadStruct);
	}

	/** Removes the wrapped struct from the "resolving" set (it has been fully "resolved") */
	~FUnresolvedStructTracker()
	{
		// even if another FUnresolvedStructTracker added this struct earlier,  
		// we want the most nested one removing it from the set (because this 
		// means the struct is fully resolved, even if we're still in the middle  
		// of a ResolveDeferredDependencies() call further up the stack)
		UnresolvedStructs.Remove(TrackedStruct);
	}

	/**
	 * Checks to see if the specified import object is a blueprint class/struct 
	 * that is currently in the midst of resolving (and hasn't completed that  
	 * resolve elsewhere in some nested call).
	 * 
	 * @param  ImportObject    The object you wish to check.
	 * @return True if the specified object is a class/struct that hasn't been fully resolved yet (otherwise false).
	 */
	static bool IsImportStructUnresolved(UObject* ImportObject)
	{
		return UnresolvedStructs.Contains(ImportObject);
	}

	/**
	 * Checks to see if the specified linker is associated with any of the 
	 * unresolved structs that this is currently tracking.
	 *
	 * NOTE: This could return false, even if the linker is in a 
	 *       ResolveDeferredDependencies() call futher up the callstack... in 
	 *       that scenario, the associated struct was fully resolved by a 
	 *       subsequent call to the same function (for the same linker/struct).
	 * 
	 * @param  Linker	The linker you want to check.
	 * @return True if the specified linker is in the midst of an unfinished ResolveDeferredDependencies() call (otherwise false).
	 */
	static bool IsAssociatedStructUnresolved(const ULinkerLoad* Linker)
	{
		for (UObject* UnresolvedObj : UnresolvedStructs)
		{
			// each unresolved struct should have a linker set on it, because 
			// they would have had to go through Preload()
			if (UnresolvedObj->GetLinker() == Linker)
			{
				return true;
			}
		}
		return false;
	}

private:
	/** The struct that is currently being "resolved" */
	UStruct* TrackedStruct;

	/** 
	 * A set of blueprint structs & classes that are currently being "resolved"  
	 * by ResolveDeferredDependencies() (using UObject* instead of UStruct, so
	 * we don't have to cast import objects before checking for their presence).
	 */
	static TSet<UObject*> UnresolvedStructs;
};
/** A global set that tracks structs currently being ran through (and unfinished by) ULinkerLoad::ResolveDeferredDependencies() */
TSet<UObject*> FUnresolvedStructTracker::UnresolvedStructs;

void ULinkerLoad::ResolveDeferredDependencies(UStruct* LoadStruct)
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	//--------------------------------------
	// Phase 2: Resolve Dependency Stubs
	//--------------------------------------
	TGuardValue<uint32> LoadFlagsGuard(LoadFlags, (LoadFlags & ~LOAD_DeferDependencyLoads));

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	static int32 RecursiveDepth = 0;
	int32 const MeasuredDepth = RecursiveDepth;
	TGuardValue<int32> RecursiveDepthGuard(RecursiveDepth, RecursiveDepth + 1);

	DEFERRED_DEPENDENCY_CHECK( (LoadStruct != nullptr) && (LoadStruct->GetLinker() == this) );
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

	// scoped block to manage the lifetime of ScopedResolveTracker, so that 
	// this resolve is only tracked for the duration of resolving all its 
	// placeholder classes, all member struct's placholders, and its parent's
	{
		FUnresolvedStructTracker ScopedResolveTracker(LoadStruct);

		// this function (for this linker) could be reentrant (see where we 
		// recursively call ResolveDeferredDependencies() for super-classes below);  
		// if that's the case, then we want to finish resolving the pending class 
		// before we continue on
		if (ResolvingDeferredPlaceholder != nullptr)
		{
			int32 ResolvedRefCount = ResolveDependencyPlaceholder(ResolvingDeferredPlaceholder, Cast<UClass>(LoadStruct));
			// @TODO: can we reliably count on this resolving some dependencies?... 
			//        if so, check verify that!
			ResolvingDeferredPlaceholder = nullptr;
		}

		// because this loop could recurse (and end up finishing all of this for
		// us), we check HasUnresolvedDependencies() so we can early out  
		// from this loop in that situation (the loop has been finished elsewhere)
		for (int32 ImportIndex = 0; ImportIndex < ImportMap.Num() && HasUnresolvedDependencies(); ++ImportIndex)
		{
			FObjectImport& Import = ImportMap[ImportIndex];

			if (ULinkerPlaceholderClass* PlaceholderClass = Cast<ULinkerPlaceholderClass>(Import.XObject))
			{
				DEFERRED_DEPENDENCY_CHECK(PlaceholderClass->ImportIndex == ImportIndex);

				// NOTE: we don't check that this resolve successfully replaced any
				//       references (by the return value), because this resolve 
				//       could have been re-entered and completed by a nested call
				//       to the same function (for the same placeholder)
				ResolveDependencyPlaceholder(PlaceholderClass, Cast<UClass>(LoadStruct));
			}
			else if (UScriptStruct* StructObj = Cast<UScriptStruct>(Import.XObject))
			{
				// in case this is a user defined struct, we have to resolve any 
				// deferred dependencies in the struct 
				if (Import.SourceLinker != nullptr)
				{
					Import.SourceLinker->ResolveDeferredDependencies(StructObj);
				}
			}
		}

		if (UStruct* SuperStruct = LoadStruct->GetSuperStruct())
		{
			ULinkerLoad* SuperLinker = SuperStruct->GetLinker();
			// NOTE: there is no harm in calling this when the super is not 
			//       "actively resolving deferred dependencies"... this condition  
			//       just saves on wasted ops, looping over the super's ImportMap
			if ((SuperLinker != nullptr) && SuperLinker->HasUnresolvedDependencies())
			{
				// a resolve could have already been started up the stack, and in turn  
				// started loading a different package that resulted in another (this) 
				// resolve beginning... in that scenario, the original resolve could be 
				// for this class's super and we want to make sure that finishes before
				// we regenerate this class (else the generated script code could end up 
				// with unwanted placeholder references; ones that would have been
				// resolved by the super's linker)
				SuperLinker->ResolveDeferredDependencies(SuperStruct);
			}
		}

	// close the scope on ScopedResolveTracker (so LoadClass doesn't appear to 
	// be resolving through the rest of this function)
	}

	// @TODO: don't know if we need this, but could be good to have (as class 
	//        regeneration is about to force load a lot of this), BUT! this 
	//        doesn't work for map packages (because this would load the level's
	//        ALevelScriptActor instance BEFORE the class has been regenerated)
	//LoadAllObjects();

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	for (TObjectIterator<ULinkerPlaceholderClass> PlaceholderIt; PlaceholderIt && !FBlueprintSupport::IsResolvingDeferredDependenciesDisabled(); ++PlaceholderIt)
	{
		ULinkerPlaceholderClass* PlaceholderClass = *PlaceholderIt;
		if (PlaceholderClass->GetOuter() == LinkerRoot)
		{
			// there shouldn't be any deferred dependencies (belonging to this 
			// linker) that need to be resolved by this point
			DEFERRED_DEPENDENCY_CHECK(!PlaceholderClass->HasReferences() && PlaceholderClass->IsPendingKill());
		}
	}
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
}

bool ULinkerLoad::HasUnresolvedDependencies() const
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	// checking (ResolvingDeferredPlaceholder != nullptr) is not sufficient, 
	// because the linker could be in the midst of a nested resolve (for a 
	// struct, or super... see ULinkerLoad::ResolveDeferredDependencies)
	bool bIsClassExportUnresolved = FUnresolvedStructTracker::IsAssociatedStructUnresolved(this);

	// (ResolvingDeferredPlaceholder != nullptr) should imply 
	// bIsClassExportUnresolved is true (but not the other way around)
	DEFERRED_DEPENDENCY_CHECK((ResolvingDeferredPlaceholder == nullptr) || bIsClassExportUnresolved);
	
	return bIsClassExportUnresolved;

#else  // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	return false;
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
}

int32 ULinkerLoad::ResolveDependencyPlaceholder(UClass* PlaceholderIn, UClass* ReferencingClass)
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	if (FBlueprintSupport::IsResolvingDeferredDependenciesDisabled())
	{
		return 0;
	}
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

	TGuardValue<uint32>  LoadFlagsGuard(LoadFlags, (LoadFlags & ~LOAD_DeferDependencyLoads));
 	TGuardValue<UClass*> ResolvingClassGuard(ResolvingDeferredPlaceholder, PlaceholderIn);

	DEFERRED_DEPENDENCY_CHECK(Cast<ULinkerPlaceholderClass>(PlaceholderIn) != nullptr);
	DEFERRED_DEPENDENCY_CHECK(PlaceholderIn->GetOuter() == LinkerRoot);

	ULinkerPlaceholderClass* PlaceholderClass = (ULinkerPlaceholderClass*)PlaceholderIn;
	
	int32 const ImportIndex = PlaceholderClass->ImportIndex;
	FObjectImport& Import = ImportMap[ImportIndex];

	DEFERRED_DEPENDENCY_CHECK( (Import.XObject == PlaceholderClass) || (Import.XObject == nullptr) );
	
	// clear the placeholder from the import, so that a call to CreateImport()
	// properly fills it in
	Import.XObject = nullptr;
	// NOTE: this is a possible point of recursion... CreateImport() could 
	//       continue to load a package already started up the stack and you 
	//       could end up in another ResolveDependencyPlaceholder() for some  
	//       other placeholder before this one has completely finished resolving
	UClass* RealClassObj = CastChecked<UClass>(CreateImport(ImportIndex), ECastCheckedType::NullAllowed);

	int32 ReplacementCount = 0;
	if (ReferencingClass != nullptr)
	{
		for (FImplementedInterface& Interface : ReferencingClass->Interfaces)
		{
			if (Interface.Class == PlaceholderClass)
			{
				++ReplacementCount;
				Interface.Class = RealClassObj;
			}
		}
	}

	// make sure that we know what utilized this placeholder class... right now
	// we only expect UObjectProperties/UClassProperties/UInterfaceProperties/
	// FImplementedInterfaces to, but something else could have requested the 
	// class without logging itself with the placeholder... if the placeholder
	// doesn't have any known references (and it hasn't already been resolved in
	// some recursive call), then there is something out there still using this
	// placeholder class
	DEFERRED_DEPENDENCY_CHECK( (ReplacementCount > 0) || PlaceholderClass->HasReferences() || PlaceholderClass->HasBeenResolved() );

	ReplacementCount += PlaceholderClass->ReplaceTrackedReferences(RealClassObj);
	PlaceholderClass->MarkPendingKill(); // @TODO: ensure these are properly GC'd

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	UObject* PlaceholderObj = PlaceholderClass;
	// @TODO: not an actual method, but would be nice to circumvent the need for bIsAsyncLoadRef below
	//FAsyncObjectsReferencer::Get().RemoveObject(PlaceholderObj);

	// there should not be any references left to this placeholder class 
	// (if there is, then we didn't log that referencer with the placeholder)
	FReferencerInformationList UnresolvedReferences;
	bool const bIsReferenced = IsReferenced(PlaceholderObj, RF_NoFlags, /*bCheckSubObjects =*/false, &UnresolvedReferences);

	// when we're running with async loading there may be an acceptable 
	// reference left in FAsyncObjectsReferencer (which reports its refs  
	// through FGCObject::GGCObjectReferencer)... since garbage collection can  
	// be ran during async loading, FAsyncObjectsReferencer is in charge of  
	// holding onto objects that are spawned during the process (to ensure 
	// they're not thrown away prematurely)
	bool const bIsAsyncLoadRef = (UnresolvedReferences.ExternalReferences.Num() == 1) &&
		PlaceholderObj->HasAnyFlags(RF_AsyncLoading) && (UnresolvedReferences.ExternalReferences[0].Referencer == FGCObject::GGCObjectReferencer);

	DEFERRED_DEPENDENCY_CHECK(!bIsReferenced || bIsAsyncLoadRef);
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS


	return ReplacementCount;
#else  // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	return 0;
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
}

void ULinkerLoad::FinalizeBlueprint(UClass* LoadClass)
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (!FBlueprintSupport::UseDeferredDependencyLoading())
	{
		return;
	}

	//--------------------------------------
	// Phase 3: Finalize (serialize CDO & regenerate class)
	//--------------------------------------
	TGuardValue<uint32> LoadFlagsGuard(LoadFlags, LoadFlags & ~LOAD_DeferDependencyLoads);

	// we can get in a state where a sub-class is getting finalized here, before
	// its super-class has been "finalized" (like when the super's 
	// ResolveDeferredDependencies() ends up Preloading a sub-class), so we
	// want to make sure that its properly finalized before this sub-class is
	// (so we can have a properly formed sub-class)
	if (UClass* SuperClass = LoadClass->GetSuperClass())
	{
		ULinkerLoad* SuperLinker = SuperClass->GetLinker();
		if ((SuperLinker != nullptr) && SuperLinker->IsBlueprintFinalizationPending())
		{
			SuperLinker->FinalizeBlueprint(SuperClass);
		}
	}

	// at this point, we're sure that LoadClass doesn't contain any class 
	// placeholders (because ResolveDeferredDependencies() was ran on it); 
	// however, once we get to regenerating/compiling the blueprint, the graph 
	// (nodes, pins, etc.) could bring in new dependencies that weren't part of 
	// the class... this would normally be all fine and well, but in complicated 
	// dependency chains those graph-dependencies could already be in the middle 
	// of resolving themselves somewhere up the stack... if we just continue  
	// along and let the blueprint compile, then it could end up with  
	// placeholder refs in its script code (which it bad); we need to make sure
	// that all dependencies don't have any placeholder classes left in them
	// 
	// we don't want this to be part of ResolveDeferredDependencies() 
	// because we don't want this to count as a linker's "class resolution 
	// phase"; at this point, it is ok if other blueprints compile with refs to  
	// this LoadClass since it doesn't have any placeholders left in it (we also 
	// don't want this linker externally claiming that it has resolving left to 
	// do, otherwise other linkers could want to finish this off when they don't
	// have to)... we do however need it here in FinalizeBlueprint(), because
	// we need it ran for any super-classes before we regen
	for (int32 ImportIndex = 0; ImportIndex < ImportMap.Num() && IsBlueprintFinalizationPending(); ++ImportIndex)
	{
		FObjectImport& Import = ImportMap[ImportIndex];
		if (Import.XObject == nullptr)
		{
			// first, make sure every import object is available... just because 
			// it isn't present in the map already, doesn't mean it isn't in the 
			// middle of a resolve (the CreateImport() brings in an export 
			// object from another package, which could be resolving itself)... 
			// 
			// don't fret, all these imports were bound to get created sooner or 
			// later (like when the blueprint was regenerated)
			//
			// NOTE: this is a possible root point for recursion... accessing a 
			//       separate package could continue its loading process which
			//       in turn, could end us back in this function before we ever  
			//       returned from this
			CreateImport(ImportIndex);
		}

		// see if this import is currently being resolved (presumably somewhere 
		// up the callstack)... if it is, we need to ensure that this dependency 
		// is fully resolved before we get to regenerating the blueprint (else,
		// we could end up with placeholder classes in our script-code)
		if (FUnresolvedStructTracker::IsImportStructUnresolved(Import.XObject))
		{
			// because it is tracked by FUnresolvedStructTracker, it must be a struct
			DEFERRED_DEPENDENCY_CHECK(Cast<UStruct>(Import.XObject) != nullptr);
			if (Import.SourceLinker)
			{
				Import.SourceLinker->ResolveDeferredDependencies((UStruct*)Import.XObject);
			}
		}
	}

	// the above loop could have caused some recursion... if it ended up 
	// finalizing a sub-class (of LoadClass), then that would have finalized
	// this as well; so, before we continue, make sure that this didn't already 
	// get fully executed in some nested call
	if (IsBlueprintFinalizationPending())
	{
		DEFERRED_DEPENDENCY_CHECK(DeferredExportIndex != INDEX_NONE);
		FObjectExport& CDOExport = ExportMap[DeferredExportIndex];

		UObject* CDO = CDOExport.Object;
		DEFERRED_DEPENDENCY_CHECK(CDO != nullptr);

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
		if (!FBlueprintSupport::IsDeferredCDOSerializationDisabled())
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
		{
			// have to prematurely set the CDO's linker so we can force a Preload()/
			// Serialization of the CDO before we regenerate the Blueprint class
			{
				const EObjectFlags OldFlags = CDO->GetFlags();
				CDO->ClearFlags(RF_NeedLoad | RF_NeedPostLoad);
				CDO->SetLinker(this, DeferredExportIndex, /*bShouldDetatchExisting =*/false);
				CDO->SetFlags(OldFlags);
			}
			// should load the CDO (ensuring that it has been serialized in by the 
			// time we get to class regeneration)
			//
			// NOTE: this is point where circular dependencies could reveal 
			//       themselves, as the CDO could depend on a class not listed in 
			//       the package's imports
			//
			// NOTE: we don't guard against reentrant behavior... if the CDO has
			//       already been "finalized", then its RF_NeedLoad flag would 
			//       have been cleared
			Preload(CDO);

			DEFERRED_DEPENDENCY_CHECK(CDO->HasAnyFlags(RF_LoadCompleted));
		}

		UClass* BlueprintClass = Cast<UClass>(IndexToObject(CDOExport.ClassIndex));
		DEFERRED_DEPENDENCY_CHECK(BlueprintClass == LoadClass);
		DEFERRED_DEPENDENCY_CHECK(BlueprintClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint));

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
		// at this point there should not be any instances of the Blueprint (else,
		// we'd have to reinstance and that is too expensive an operation to 
		// have at load time)
		TArray<UObject*> ClassInstances;
		GetObjectsOfClass(BlueprintClass, ClassInstances, /*bIncludeDerivedClasses =*/true);
		DEFERRED_DEPENDENCY_CHECK(ClassInstances.Num() == 0);
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

		// clear this so IsBlueprintFinalizationPending() doesn't report true
		DeferredExportIndex = INDEX_NONE;

		// just in case we choose to enable the deferred dependency loading for 
		// cooked builds... we want to keep from regenerating in that scenario
		if (!LoadClass->bCooked)
		{
			UObject* OldCDO = BlueprintClass->ClassDefaultObject;
			if (RegenerateBlueprintClass(BlueprintClass, CDO))
			{
				// emulate class CDO serialization (RegenerateBlueprintClass() could 
				// have a side-effect where it overwrites the class's CDO; so we 
				// want to make sure that we don't overwrite that new CDO with a 
				// stale one)
				if (OldCDO == BlueprintClass->ClassDefaultObject)
				{
					BlueprintClass->ClassDefaultObject = CDOExport.Object;
				}
			}
		}
	}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
}

bool ULinkerLoad::IsBlueprintFinalizationPending() const
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	return (DeferredExportIndex != INDEX_NONE);
#else  // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	return false;
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
}

// don't want other files ending up with this internal define
#undef DEFERRED_DEPENDENCY_CHECK

/*******************************************************************************
 * UObject
 ******************************************************************************/

/** 
 * Returns whether this object is contained in or part of a blueprint object
 */
bool UObject::IsInBlueprint() const
{
	// Exclude blueprint classes as they may be regenerated at any time
	// Need to exclude classes, CDOs, and their subobjects
	const UObject* TestObject = this;
 	while (TestObject)
 	{
 		const UClass *ClassObject = dynamic_cast<const UClass*>(TestObject);
		if (ClassObject 
			&& ClassObject->HasAnyClassFlags(CLASS_CompiledFromBlueprint) 
			&& ClassObject->ClassGeneratedBy)
 		{
 			return true;
 		}
		else if (TestObject->HasAnyFlags(RF_ClassDefaultObject) 
			&& TestObject->GetClass() 
			&& TestObject->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint) 
			&& TestObject->GetClass()->ClassGeneratedBy)
 		{
 			return true;
 		}
 		TestObject = TestObject->GetOuter();
 	}

	return false;
}

/** 
 *  Destroy properties that won't be destroyed by the native destructor
 */
void UObject::DestroyNonNativeProperties()
{
	// Destroy properties that won't be destroyed by the native destructor
#if USE_UBER_GRAPH_PERSISTENT_FRAME
	GetClass()->DestroyPersistentUberGraphFrame(this);
#endif
	{
		for (UProperty* P = GetClass()->DestructorLink; P; P = P->DestructorLinkNext)
		{
			P->DestroyValue_InContainer(this);
		}
	}
}

/*******************************************************************************
 * FObjectInitializer
 ******************************************************************************/

/** 
 * Initializes a non-native property, according to the initialization rules. If the property is non-native
 * and does not have a zero constructor, it is initialized with the default value.
 * @param	Property			Property to be initialized
 * @param	Data				Default data
 * @return	Returns true if that property was a non-native one, otherwise false
 */
bool FObjectInitializer::InitNonNativeProperty(UProperty* Property, UObject* Data)
{
	if (!Property->GetOwnerClass()->HasAnyClassFlags(CLASS_Native | CLASS_Intrinsic)) // if this property belongs to a native class, it was already initialized by the class constructor
	{
		if (!Property->HasAnyPropertyFlags(CPF_ZeroConstructor)) // this stuff is already zero
		{
			Property->InitializeValue_InContainer(Data);
		}
		return true;
	}
	else
	{
		// we have reached a native base class, none of the rest of the properties will need initialization
		return false;
	}
}
