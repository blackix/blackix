// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AutoReimportUtilities.h"
#include "FileCache.h"
#include "MessageLog.h"

/** Class responsible for watching a specific content directory for changes */
class FContentDirectoryMonitor
{
public:

	/**
	 * Constructor.
	 * @param InDirectory			Content directory path to monitor. Assumed to be absolute.
	 * @param InSupportedExtensions A string containing semi-colon separated extensions to monitor.
	 * @param InMountedContentPath	(optional) Mounted content path (eg /Engine/, /Game/) to which InDirectory maps.
	 */
	FContentDirectoryMonitor(const FString& InDirectory, const FString& InSupportedExtensions, const FString& InMountedContentPath = FString());

	/** Tick this monitor's cache to give it a chance to finish scanning for files */
	void Tick(const FTimeLimit& TimeLimit);

	/** Start processing any outstanding changes this monitor is aware of */
	void StartProcessing();

	/** Extract the files we need to import from our outstanding changes (happens first)*/ 
	void ProcessAdditions(TArray<UPackage*>& OutPackagesToSave, const FTimeLimit& TimeLimit, const TMap<FString, TArray<UFactory*>>& InFactoriesByExtension, class FReimportFeedbackContext& Context);

	/** Process the outstanding changes that we have cached */
	void ProcessModifications(const IAssetRegistry& Registry, const FTimeLimit& TimeLimit, class FReimportFeedbackContext& Context);

	/** Extract the assets we need to delete from our outstanding changes (happens last) */ 
	void ExtractAssetsToDelete(const IAssetRegistry& Registry, TArray<FAssetData>& OutAssetsToDelete);

	/** Destroy this monitor including its cache */
	void Destroy();

	/** Get the directory that this monitor applies to */
	const FString& GetDirectory() const { return Cache.GetDirectory(); }

public:

	/** Get the number of outstanding changes that we potentially have to process (when not already processing) */
	int32 GetNumUnprocessedChanges() const { return Cache.GetNumOutstandingChanges(); }

	/** Get the total amount of work this monitor has to perform in the current processing operation */
	int32 GetTotalWork() const { return TotalWork; }

	/** Get the total amount of work this monitor has performed in the current processing operation */
	int32 GetWorkProgress() const { return WorkProgress; }

private:

	/** The file cache that monitors and reflects the content directory. */
	FFileCache Cache;

	/** The mounted content path for this monitor (eg /Game/) */
	FString MountedContentPath;

	/** A list of file system changes that are due to be processed */
	TArray<FUpdateCacheTransaction> AddedFiles, ModifiedFiles, DeletedFiles;

	/** The number of changes we've processed out of the OutstandingChanges array */
	int32 TotalWork, WorkProgress;

	/** The last time we attempted to save the cache file */
	double LastSaveTime;

	/** The interval between potential re-saves of the cache file */
	static const int32 ResaveIntervalS = 60;
};