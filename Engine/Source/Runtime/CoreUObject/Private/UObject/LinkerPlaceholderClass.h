// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Class.h"
#include "Set.h"

// Forward declarations
class FObjectInitializer;
class UObjectPropertyBase;

/*******************************************************************************
 *FPlaceholderContainerTracker
 ******************************************************************************/
/** 
 * To track placeholder property values, we need to know the root container 
 * instance that is set with the placeholder value (so we can reset it later). 
 * This here is designed to track objects that are actively being preloaded
 * (serialized in); so we have the container on hand, when a UObjectProperty 
 * value is set with a placeholder.
 */
struct FScopedPlaceholderContainerTracker
{
public:
	 FScopedPlaceholderContainerTracker(UObject* PerspectivePlaceholderReferencer);
	~FScopedPlaceholderContainerTracker();

private:
	UObject* PlaceholderReferencerCandidate;
};

/*******************************************************************************
 * ULinkerPlaceholderClass
 ******************************************************************************/

/**  
 * A utility class for the deferred dependency loader, used to stub in temporary
 * class references so we don't have to load blueprint resources for their class.
 * Holds on to references where this is currently being utilized, so we can 
 * easily replace references to it later (once the real class is available).
 */ 
class ULinkerPlaceholderClass : public UClass
{
public:
	DECLARE_CASTED_CLASS_INTRINSIC_NO_CTOR(ULinkerPlaceholderClass, UClass, /*TStaticFlags =*/0, CoreUObject, /*TStaticCastFlags =*/0, NO_API)

	ULinkerPlaceholderClass(const FObjectInitializer& ObjectInitializer);
	virtual ~ULinkerPlaceholderClass();

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	// UObject interface.
 	virtual void PostInitProperties() override;
	// End of UObject interface.

	// UField interface.
 	virtual void Bind() override;
	// End of UField interface.

	/**
	 * Caches off the supplied property so that we can later replace it's use of
	 * this class with another (real) class.
	 * 
	 * @param  ReferencingProperty    A property that uses and stores this class.
	 */
	void AddReferencingProperty(UProperty* ReferencingProperty);

	/**
	 * Attempts to find and store the referencing container object (along with  
	 * the specified property), so that we can replace the reference at a 
	 * later point. Can fail if the container could not be found.
	 * 
	 * @param  ReferencingProperty	The property whose object-value is referencing this.
	 * @param  DataPtr				Not saved off (as it can change), but used to verify that we pick the correct container.
	 * @return True if we successfully found a container object and are now tracking it, otherwise false.
	 */
	bool AddReferencingPropertyValue(const UObjectProperty* ReferencingProperty, void* DataPtr);

	/**
	 * Records a raw pointer, directly to the UClass* script expression (so that
	 * we can switch-out its value in ReplaceTrackedReferences). 
	 *
	 * NOTE: We don't worry about creating some kind of weak ref to the script 
	 *       pointer (or facilitate a way for this tracked reference to be 
	 *       removed). We're not worried about the script ref being deleted 
	 *       before we call ReplaceTrackedReferences (because we expect that we
	 *       do this all within the same frame; before GC can be ran).
	 * 
	 * @param  ExpressionPtr    A direct pointer to the UClass* that is now referencing this placeholder.
	 */
	void AddReferencingScriptExpr(ULinkerPlaceholderClass** ExpressionPtr);

	/**
	 * A query method that let's us check to see if this class is currently 
	 * being referenced by anything (if this returns false, then a referencing 
	 * property could have forgotten to add itself... or, we've replaced all
	 * references).
	 * 
	 * @return True if this has anything stored in its ReferencingProperties container, otherwise false.
	 */
	bool HasReferences() const;

	/**
	 * Query method that retrieves the current number of KNOWN references to 
	 * this placeholder class.
	 * 
	 * @return The number of references that this class is currently tracking.
	 */
	int32 GetRefCount() const;

	/**
	 * Checks to see if 1) this placeholder has had RemoveTrackedReference() 
	 * called on it, and 2) it doesn't have any more references that have since 
	 * been added.
	 * 
	 * @return True if ReplaceTrackedReferences() has been ran, and no KNOWN references have been added.
	 */
	bool HasBeenResolved() const;

	/**
	 * Removes the specified property from this class's internal tracking list 
	 * (which aims to keep track of properties utilizing this class).
	 * 
	 * @param  ReferencingProperty    A property that used to use this class, and now no longer does.
	 */
	void RemovePropertyReference(UProperty* ReferencingProperty);

	/**
	 * Iterates over all referencing properties and attempts to replace their 
	 * references to this class with a new (hopefully proper) class.
	 * 
	 * @param  ReplacementClass    The class that you want all references to this class replaced with.
	 * @return The number of references that were successfully replaced.
	 */
	int32 ReplaceTrackedReferences(UClass* ReplacementClass);

public:
	/** Set by the ULinkerLoad that created this instance, tracks what import this was used in place of. */
	int32 ImportIndex;

private:
	/**
	 * Iterates through ReferencingContainers and replaces any (KNOWN) 
	 * references to this placeholder (with 
	 * 
	 * @param  ReplacementObj    
	 * @return 
	 */
	int32 ResolvePlaceholderPropertyValues(UObject* ReplacementObj);

	/** Links to UProperties that are currently using this class */
	TSet<UProperty*> ReferencingProperties;

	/** Used to catch references that are added after we've already resolved all references */
	bool bResolvedReferences;

	/** Points directly at UClass* refs that we're serialized in as part of script bytecode */
	TSet<UClass**> ReferencingScriptExpressions;

	/** Tracks container objects that have property values set to reference this placeholder (references that need to be replaced later) */
	TMap< TWeakObjectPtr<UObject>, TSet<const UObjectProperty*> > ReferencingContainers;
}; 
