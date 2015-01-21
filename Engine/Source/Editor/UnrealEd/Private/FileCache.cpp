// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "FileCache.h"
#include "Developer/DirectoryWatcher/Public/DirectoryWatcherModule.h"

const FGuid FFileCacheCustomVersion::Key(0x8E7DDCB3, 0x80DA47BB, 0x9FD346A2, 0x93984DF6);
FCustomVersionRegistration GRegisterFileCacheVersion(FFileCacheCustomVersion::Key, FFileCacheCustomVersion::Latest, TEXT("FileCacheVersion"));

namespace
{
	/** Helper template function to remove duplicates from an array, given some predicate */
	template<typename T, typename P>
	void RemoveDuplicates(TArray<T>& Array, P Predicate)
	{
		if (Array.Num() == 0)
			return;

		const int32 NumElements = Array.Num();
		int32 Write = 0;
		for (int32 Read = 0; Read < NumElements; ++Read)
		{
			for (int32 DuplRead = Read + 1; DuplRead < NumElements; ++DuplRead)
			{
				if (Predicate(Array[Read], Array[DuplRead]))
				{
					++Read;
					goto next;
				}
			}

			if (Write != Read)
			{
				Swap(Array[Write++], Array[Read]);
			}
	next:
			;
		}
		const int32 NumDuplicates = NumElements - (Write + 1);
		if (NumDuplicates != 0)
		{
			Array.RemoveAt(Write + 1, NumDuplicates, false);
		}
	}
}

FAsyncDirectoryReader::FAsyncDirectoryReader(const FString& InDirectory)
{
	PendingDirectories.Add(InDirectory);
	State.Emplace();
}

TOptional<FDirectoryState> FAsyncDirectoryReader::GetFinalState()
{
	TOptional<FDirectoryState> OldState;
	Swap(OldState, State);

	return OldState;
}

bool FAsyncDirectoryReader::IsComplete() const
{
	return PendingFiles.Num() == 0 && PendingDirectories.Num() == 0;
}

EProgressResult FAsyncDirectoryReader::Tick(const FWorkLimiter& Limiter)
{
	auto& FileManager = IFileManager::Get();

	for (int32 Index = 0; Index < PendingDirectories.Num(); ++Index)
	{
		ScanDirectory(PendingDirectories[Index]);
		if (Limiter.ShouldLimit())
		{
			// We've spent too long, bail
			PendingDirectories.RemoveAt(0, Index, false);
			return EProgressResult::Pending;
		}
	}
	PendingDirectories.Empty();

	for (int32 Index = 0; Index < PendingFiles.Num(); ++Index)
	{
		const auto& File = PendingFiles[Index];
		State->Files.Emplace(File, FileManager.GetTimeStamp(*File));

		if (Index % 100 == 0 && Limiter.ShouldLimit())
		{
			// We've spent too long, bail
			PendingFiles.RemoveAt(0, Index, false);
			return EProgressResult::Pending;
		}
	}
	PendingFiles.Empty();

	return EProgressResult::Finished;
}

void FAsyncDirectoryReader::ScanDirectory(const FString& InDirectory)
{
	struct FVisitor : public IPlatformFile::FDirectoryVisitor
	{
		TArray<FString>* PendingFiles;
		TArray<FString>* PendingDirectories;

		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			if (bIsDirectory)
			{
				PendingDirectories->Add(FString(FilenameOrDirectory));
			}
			else
			{
				PendingFiles->Add(FString(FilenameOrDirectory));
			}
			return true;
		}
	};

	FVisitor Visitor;
	Visitor.PendingFiles = &PendingFiles;
	Visitor.PendingDirectories = &PendingDirectories;
	IFileManager::Get().IterateDirectory(*InDirectory, Visitor);
}

FFileCache::FFileCache(const FFileCacheConfig& InConfig)
	: Config(InConfig)
	, DirectoryReader(InConfig.Directory)
	, bSavedCacheDirty(false)
	, LastChangeTimeS(0)
{
	// Ensure that the extension strings are of the form ;ext1;ext2;ext3;
	if (!Config.IncludeExtensions.IsEmpty() && Config.IncludeExtensions[0] != ';')
	{
		Config.IncludeExtensions.InsertAt(0, ';');
	}
	if (!Config.ExcludeExtensions.IsEmpty() && Config.ExcludeExtensions[0] != ';')
	{
		Config.ExcludeExtensions.InsertAt(0, ';');
	}

	// Attempt to load an existing cache file
	auto ExistingCache = ReadCache();
	if (ExistingCache.IsSet())
	{
		CachedDirectoryState = MoveTemp(ExistingCache.GetValue());
	}

	FDirectoryWatcherModule& Module = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	if (IDirectoryWatcher* DirectoryWatcher = Module.Get())
	{
		auto Callback = IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FFileCache::OnDirectoryChanged);
		DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(Config.Directory, Callback, WatcherDelegate);
	}
}

FFileCache::~FFileCache()
{
	UnbindWatcher();
	WriteCache();
}

void FFileCache::Destroy()
{
	// Delete the cache file, and clear out everything
	bSavedCacheDirty = false;
	IFileManager::Get().Delete(*Config.CacheFile, false, true, true);

	DirectoryReader = FAsyncDirectoryReader(Config.Directory);
	OutstandingChanges.Empty();
	CachedDirectoryState = FDirectoryState();

	UnbindWatcher();
}

void FFileCache::UnbindWatcher()
{
	if (WatcherDelegate == FDelegateHandle())
	{
		return;
	}

	if (FDirectoryWatcherModule* Module = FModuleManager::GetModulePtr<FDirectoryWatcherModule>(TEXT("DirectoryWatcher")))
	{
		if (IDirectoryWatcher* DirectoryWatcher = Module->Get())
		{
			DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(Config.Directory, WatcherDelegate);
		}
	}

	WatcherDelegate = FDelegateHandle();
}

TOptional<FDirectoryState> FFileCache::ReadCache() const
{
	TOptional<FDirectoryState> Optional;

	FArchive* Ar = IFileManager::Get().CreateFileReader(*Config.CacheFile);
	if (Ar)
	{
		FDirectoryState Result;

		*Ar << Result;
		Ar->Close();
		delete Ar;

		Optional.Emplace(MoveTemp(Result));
	}

	return Optional;
}

void FFileCache::WriteCache()
{
	if (bSavedCacheDirty)
	{
		FArchive* Ar = IFileManager::Get().CreateFileWriter(*Config.CacheFile);
		check(Ar);

		*Ar << CachedDirectoryState;

		Ar->Close();
		delete Ar;

		CachedDirectoryState.Files.Shrink();

		bSavedCacheDirty = false;
	}
}

TArray<FUpdateCacheTransaction> FFileCache::GetOutstandingChanges()
{
	if (FPlatformTime::Seconds() - LastChangeTimeS >= Config.BatchDelayS)
	{
		TArray<FUpdateCacheTransaction> Moved;
		Swap(OutstandingChanges, Moved);
		return Moved;
	}
	else
	{
		return TArray<FUpdateCacheTransaction>();
	}
}

void FFileCache::CompleteTransaction(FUpdateCacheTransaction&& Transaction)
{
	auto* CachedData = CachedDirectoryState.Files.Find(Transaction.Filename);

	switch (Transaction.Action)
	{
	case FFileChangeData::FCA_Modified:
		if (CachedData && CachedData->Timestamp < Transaction.Timestamp)
		{
			// Update the timestamp
			CachedData->Timestamp = Transaction.Timestamp;

			bSavedCacheDirty = true;
		}
		break;
	case FFileChangeData::FCA_Added:
		if (!CachedData)
		{
			// Add the file information to the cache
			CachedDirectoryState.Files.Emplace(Transaction.Filename, Transaction.Timestamp);

			bSavedCacheDirty = true;
		}
		break;
	case FFileChangeData::FCA_Removed:
		if (CachedData)
		{
			// Remove the file information to the cache
			CachedDirectoryState.Files.Remove(Transaction.Filename);

			bSavedCacheDirty = true;
		}
		break;

	default:
		checkf(false, TEXT("Invalid file cached transaction"));
		break;
	}
}

void FFileCache::Tick(const FWorkLimiter& Limiter)
{
	if (DirectoryReader.IsComplete() || DirectoryReader.Tick(Limiter) == EProgressResult::Pending)
	{
		return;
	}

	// We should only ever get here once. The directory reader has finished scanning, and we can now diff the results with what we had saved in the cache file.
	check(DirectoryReader.IsComplete());
	
	TOptional<FDirectoryState> CurrentFileData = DirectoryReader.GetFinalState();
	if (CachedDirectoryState.Files.Num() == 0)
	{
		// If we don't have any cached data yet, just use the file data we just harvested
		CachedDirectoryState = MoveTemp(CurrentFileData.GetValue());
		bSavedCacheDirty = true;
		return;
	}

	// We already have cached data so we need to compare it with the harvested data
	// to detect additions, modifications, and removals
	for (const auto& FilenameAndData : CurrentFileData->Files)
	{
		const FString& Filename = FilenameAndData.Key.Get();
		if (!IsFileApplicable(Filename))
		{
			continue;
		}

		const auto* CachedData = CachedDirectoryState.Files.Find(Filename);
		// Have we encountered this before?
		if (CachedData)
		{
			if (CachedData->Timestamp != FilenameAndData.Value.Timestamp)
			{
				OutstandingChanges.Add(FUpdateCacheTransaction(Filename, FFileChangeData::FCA_Modified, FilenameAndData.Value.Timestamp));
			}
		}
		else
		{
			// No file already - create one. We don't do this if the cache didn't exist before as we have no idea what is new and what isn't
			OutstandingChanges.Add(FUpdateCacheTransaction(Filename, FFileChangeData::FCA_Added, FilenameAndData.Value.Timestamp));
		}
	}

	// Delete anything that doesn't exist on disk anymore
	for (auto It = CachedDirectoryState.Files.CreateIterator(); It; ++It)
	{
		const FImmutableString& Filename = It.Key();
		if (!CurrentFileData->Files.Contains(Filename) && IsFileApplicable(Filename.Get()))
		{
			OutstandingChanges.Add(FUpdateCacheTransaction(Filename, FFileChangeData::FCA_Removed));
		}
	}
}

void FFileCache::OnDirectoryChanged(const TArray<FFileChangeData>& FileChanges)
{
	auto& FileManager = IFileManager::Get();

	OutstandingChanges.Reserve(OutstandingChanges.Num() + FileChanges.Num());

	for (const auto& ThisEntry : FileChanges)
	{
		// If it's a directory or is not applicable, ignore it
		if (FileManager.DirectoryExists(*ThisEntry.Filename) || !IsFileApplicable(ThisEntry.Filename))
		{
			continue;
		}

		FImmutableString RelativeFilename;
		{
			FString Temp = FPaths::ConvertRelativePathToFull(ThisEntry.Filename);
			Temp.GetCharArray().RemoveAt(0, Config.Directory.Len());
			RelativeFilename = Temp;
		}
		
		switch (ThisEntry.Action)
		{
		case FFileChangeData::FCA_Added:
			{
				const int32 NumRemoved = OutstandingChanges.RemoveAll([&](const FUpdateCacheTransaction& X){
					return X.Action == FFileChangeData::FCA_Removed && X.Filename == RelativeFilename;
				});

				const auto Action = NumRemoved == 0 ? FFileChangeData::FCA_Added : FFileChangeData::FCA_Modified;
				OutstandingChanges.Add(FUpdateCacheTransaction(RelativeFilename, Action, FileManager.GetTimeStamp(*ThisEntry.Filename)));
			}
			break;

		case FFileChangeData::FCA_Removed:
			{
				bool bPreviouslyAdded = false;
				const int32 NumRemoved = OutstandingChanges.RemoveAll([&](const FUpdateCacheTransaction& X){
					if (X.Filename == RelativeFilename)
					{
						bPreviouslyAdded = X.Action == FFileChangeData::FCA_Added || bPreviouslyAdded;
						return true;
					}
					return false;
				});

				if (!bPreviouslyAdded)
				{
					OutstandingChanges.Add(FUpdateCacheTransaction(RelativeFilename, FFileChangeData::FCA_Removed));
				}
			}
			
			break;

		case FFileChangeData::FCA_Modified:
			{
				const bool bPreviouslyAdded = OutstandingChanges.ContainsByPredicate([&](const FUpdateCacheTransaction& X){
					return X.Filename == RelativeFilename && X.Action == FFileChangeData::FCA_Added;
				});

				if (!bPreviouslyAdded)
				{
					OutstandingChanges.Add(FUpdateCacheTransaction(RelativeFilename, FFileChangeData::FCA_Modified, FileManager.GetTimeStamp(*ThisEntry.Filename)));
				}
			}
			break;

		default:
			break;
		}
	}

	RemoveDuplicates(OutstandingChanges, [](const FUpdateCacheTransaction& A, const FUpdateCacheTransaction& B){
		return A.Action == B.Action && A.Filename == B.Filename;
	});

	LastChangeTimeS = FPlatformTime::Seconds();
}

bool FFileCache::IsFileApplicable(const FString& Filename)
{
	return (Config.ExcludeExtensions.IsEmpty() || !MatchExtensionString(Config.ExcludeExtensions, Filename)) &&
		   (Config.IncludeExtensions.IsEmpty() ||  MatchExtensionString(Config.IncludeExtensions, Filename));
}

bool FFileCache::MatchExtensionString(const FString& InSource, const FString& InPath)
{
	int32 SlashPosition = InPath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	// in case we are using backslashes on a platform that doesn't use backslashes
	SlashPosition = FMath::Max(SlashPosition, InPath.Find(TEXT("\\"), ESearchCase::CaseSensitive, ESearchDir::FromEnd));

	const int32 DotPosition = InPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);

	if (DotPosition > INDEX_NONE && DotPosition > SlashPosition && DotPosition < InPath.Len() - 1)
	{
		FString Search;
		Search.Reserve(InPath.Len() - DotPosition + 2);;
		Search.InsertAt(0, ';');
		Search += &InPath[DotPosition + 1];
		Search.AppendChar(';');

		return InSource.Find(Search, ESearchCase::IgnoreCase) != INDEX_NONE;
	}

	return false;
}