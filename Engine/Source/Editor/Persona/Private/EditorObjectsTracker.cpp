// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "PersonaPrivatePCH.h"
#include "EditorObjectsTracker.h"

void FEditorObjectTracker::AddReferencedObjects( FReferenceCollector& Collector )
{
	for (TMap<UClass*, UObject*>::TIterator It(EditorObjMap); It; ++It)
	{
		UObject *Obj = It.Value();
		if(ensure(Obj))
		{
			Collector.AddReferencedObject(Obj);
		}
	}	
}

UObject* FEditorObjectTracker::GetEditorObjectForClass( UClass* EdClass )
{
	UObject *Obj = (EditorObjMap.Contains(EdClass) ? *EditorObjMap.Find(EdClass) : NULL);
	if(Obj == NULL)
	{
		FString ObjName = EdClass->GetName();
		ObjName += "_EdObj";
		Obj = StaticConstructObject(EdClass, GetTransientPackage(), FName(*ObjName), RF_Public|RF_Standalone|RF_Transient);
		EditorObjMap.Add(EdClass,Obj);
	}
	return Obj;
}