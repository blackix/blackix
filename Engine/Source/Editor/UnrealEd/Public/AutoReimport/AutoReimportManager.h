// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AutoReimportManager.generated.h"

/** Struct representing a path on disk, and its virtual mount point */
struct FPathAndMountPoint
{
	FPathAndMountPoint() {}
	FPathAndMountPoint(FString InPath, FString InMountPoint) : Path(MoveTemp(InPath)), MountPoint(MoveTemp(InMountPoint)) {}

	/** The directory on disk. Absolute. */
	FString Path;

	/** The mount point, if any to which this directory relates */
	FString MountPoint;
};
	
/* Deals with auto reimporting of objects when the objects file on disk is modified*/
UCLASS(config=Editor, transient)
class UNREALED_API UAutoReimportManager : public UObject
{
	GENERATED_UCLASS_BODY()
public:

	~UAutoReimportManager();

	/** Initialize during engine startup */
	void Initialize();

	/** Get a list of absolute directories that we are monitoring */
	TArray<FPathAndMountPoint> GetMonitoredDirectories() const;

private:

	/** UObject Interface */
	virtual void BeginDestroy() override;

	/** Private implementation of the reimport manager */
	TSharedPtr<class FAutoReimportManager> Implementation;
};
