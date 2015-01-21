// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DirectoryWatcherPrivatePCH.h"

void DirectoryWatchMacCallback( ConstFSEventStreamRef StreamRef, void* WatchRequestPtr, size_t EventCount, void* EventPaths, const FSEventStreamEventFlags EventFlags[], const FSEventStreamEventId EventIDs[] )
{
	FDirectoryWatchRequestMac* WatchRequest = (FDirectoryWatchRequestMac*)WatchRequestPtr;
	check(WatchRequest);
	check(WatchRequest->EventStream == StreamRef);

	WatchRequest->ProcessChanges( EventCount, EventPaths, EventFlags);
}

// ============================================================================================================================

FDirectoryWatchRequestMac::FDirectoryWatchRequestMac()
:	bRunning(false)
,	bEndWatchRequestInvoked(false)
{
}

FDirectoryWatchRequestMac::~FDirectoryWatchRequestMac()
{
	Shutdown();
}

void FDirectoryWatchRequestMac::Shutdown( void )
{
	if( bRunning )
	{
		check(EventStream);

		FSEventStreamStop(EventStream);
		FSEventStreamUnscheduleFromRunLoop(EventStream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
		FSEventStreamInvalidate(EventStream);
		FSEventStreamRelease(EventStream);

		bRunning = false;
	}
}

bool FDirectoryWatchRequestMac::Init(const FString& InDirectory)
{
	if ( InDirectory.Len() == 0 )
	{
		// Verify input
		return false;
	}

	if( bRunning )
	{
		Shutdown();
	}

	bEndWatchRequestInvoked = false;

	// Make sure the path is absolute
	const FString FullPath = FPaths::ConvertRelativePathToFull(InDirectory);

	// Set up streaming and turn it on
	CFStringRef FullPathMac = FPlatformString::TCHARToCFString(*FullPath);
	CFArrayRef PathsToWatch = CFArrayCreate(NULL, (const void**)&FullPathMac, 1, NULL);

	CFAbsoluteTime Latency = 2.0;	// seconds

	FSEventStreamContext Context;
	Context.version = 0;
	Context.info = this;
	Context.retain = NULL;
	Context.release = NULL;
	Context.copyDescription = NULL;

	EventStream = FSEventStreamCreate( NULL,
		&DirectoryWatchMacCallback,
		&Context,
		PathsToWatch,
		kFSEventStreamEventIdSinceNow,
		Latency,
		kFSEventStreamCreateFlagUseCFTypes | kFSEventStreamCreateFlagNoDefer | kFSEventStreamCreateFlagFileEvents
	);

	CFRelease(PathsToWatch);
	CFRelease(FullPathMac);

	if( !EventStream )
	{
		return false;
	}

	FSEventStreamScheduleWithRunLoop( EventStream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode );
	FSEventStreamStart( EventStream );

	bRunning = true;

	return true;
}

FDelegateHandle FDirectoryWatchRequestMac::AddDelegate( const IDirectoryWatcher::FDirectoryChanged& InDelegate )
{
	Delegates.Add(InDelegate);
	return Delegates.Last().GetHandle();
}

bool FDirectoryWatchRequestMac::RemoveDelegate( const IDirectoryWatcher::FDirectoryChanged& InDelegate )
{
	return DEPRECATED_RemoveDelegate(InDelegate);
}

bool FDirectoryWatchRequestMac::DEPRECATED_RemoveDelegate( const IDirectoryWatcher::FDirectoryChanged& InDelegate )
{
	return Delegates.RemoveAll([&](const IDirectoryWatcher::FDirectoryChanged& Delegate) {
		return Delegate.DEPRECATED_Compare(InDelegate);
	}) != 0;
}

bool FDirectoryWatchRequestMac::RemoveDelegate( FDelegateHandle InHandle )
{
	return Delegates.RemoveAll([=](const IDirectoryWatcher::FDirectoryChanged& Delegate) {
		return Delegate.GetHandle() == InHandle;
	}) != 0;
}

bool FDirectoryWatchRequestMac::HasDelegates() const
{
	return Delegates.Num() > 0;
}

void FDirectoryWatchRequestMac::EndWatchRequest()
{
	bEndWatchRequestInvoked = true;
}

void FDirectoryWatchRequestMac::ProcessPendingNotifications()
{
	// Trigger all listening delegates with the files that have changed
	if ( FileChanges.Num() > 0 )
	{
		for (int32 DelegateIdx = 0; DelegateIdx < Delegates.Num(); ++DelegateIdx)
		{
			Delegates[DelegateIdx].Execute(FileChanges);
		}

		FileChanges.Empty();
	}
}

void FDirectoryWatchRequestMac::ProcessChanges( size_t EventCount, void* EventPaths, const FSEventStreamEventFlags EventFlags[] )
{
	if( bEndWatchRequestInvoked )
	{
		// ignore all events
		return;
	}

	TCHAR FilePath[MAX_PATH];

	CFArrayRef EventPathArray = (CFArrayRef)EventPaths;

	for( size_t EventIndex = 0; EventIndex < EventCount; ++EventIndex )
	{
		const FSEventStreamEventFlags Flags = EventFlags[EventIndex];
		if( !(Flags & kFSEventStreamEventFlagItemIsFile) )
		{
			// events about directories and symlinks don't concern us
			continue;
		}

		// Warning: some events have more than one of created, removed nd modified flag.
		// For safety, I think removed should take preference over created, and created over modified.
		FFileChangeData::EFileChangeAction Action;

		const bool bFileAdded = ( Flags & kFSEventStreamEventFlagItemCreated );
		const bool bFileModified = ( Flags & ( kFSEventStreamEventFlagItemRenamed | kFSEventStreamEventFlagItemModified ) );
		const bool bFileRemoved = ( Flags & kFSEventStreamEventFlagItemRemoved );
		bool bFileNeedsChecking = false;

		if( bFileAdded )
		{
			if( bFileRemoved )
			{
				// Event indicates that file was both created and removed. To find out which of those events
				// happened later, we have to check for file's presence.
				bFileNeedsChecking = true;
			}
			Action = FFileChangeData::FCA_Added;
		}
		else if( bFileModified )
		{
			// File modified
			if( bFileRemoved )
			{
				// Event indicates that file was both modified and removed. To find out which of those events
				// happened later, we have to check for file's presence.
				bFileNeedsChecking = true;
			}
			Action = FFileChangeData::FCA_Modified;
		}
		else if( bFileRemoved )
		{
			Action = FFileChangeData::FCA_Removed;
		}
		else
		{
			// events about inode, Finder info, owner change, extended attributes modification don't concern us
			continue;
		}

		FPlatformString::CFStringToTCHAR((CFStringRef)CFArrayGetValueAtIndex(EventPathArray,EventIndex), FilePath);

		if( bFileNeedsChecking && !FPlatformFileManager::Get().GetPlatformFile().FileExists(FilePath) )
		{
			Action = FFileChangeData::FCA_Removed;
		}

		new(FileChanges) FFileChangeData(FString(FilePath), Action);
	}
}
