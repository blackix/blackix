// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FDirectoryWatchRequestWindows
{
public:
	FDirectoryWatchRequestWindows();
	virtual ~FDirectoryWatchRequestWindows();

	/** Sets up the directory handle and request information */
	bool Init(const FString& InDirectory);

	/** Adds a delegate to get fired when the directory changes */
	FDelegateHandle AddDelegate( const IDirectoryWatcher::FDirectoryChanged& InDelegate );
	/** Removes a delegate to get fired when the directory changes */
	DELEGATE_DEPRECATED("This overload of RemoveDelegate is deprecated, instead pass the result of AddDelegate.")
	bool RemoveDelegate( const IDirectoryWatcher::FDirectoryChanged& InDelegate );
	/** Same as above, but for use within other deprecated calls to prevent multiple deprecation warnings */
	bool DEPRECATED_RemoveDelegate( const IDirectoryWatcher::FDirectoryChanged& InDelegate );
	/** Removes a delegate to get fired when the directory changes */
	bool RemoveDelegate( FDelegateHandle InHandle );
	/** Returns true if this request has any delegates listening to directory changes */
	bool HasDelegates() const;
	/** Returns the file handle for the directory that is being watched */
	HANDLE GetDirectoryHandle() const;
	/** Closes the system resources and prepares the request for deletion */
	void EndWatchRequest();
	/** True if system resources have been closed and the request is ready for deletion */
	bool IsPendingDelete() const { return bPendingDelete; }
	/** Triggers all pending file change notifications */
	void ProcessPendingNotifications();

private:
	/** Non-static handler for an OS notification of a directory change */
	void ProcessChange(uint32 Error, uint32 NumBytes);
	/** Static Handler for an OS notification of a directory change */
	static void CALLBACK ChangeNotification(::DWORD Error, ::DWORD NumBytes, LPOVERLAPPED InOverlapped);

	FString Directory;
	HANDLE DirectoryHandle;
	int32 MaxChanges;
	bool bWatchSubtree;
	uint32 NotifyFilter;
	uint32 BufferLength;
	uint8* Buffer;
	uint8* BackBuffer;
	OVERLAPPED Overlapped;

	bool bPendingDelete;
	bool bEndWatchRequestInvoked;

	TArray<IDirectoryWatcher::FDirectoryChanged> Delegates;
	TArray<FFileChangeData> FileChanges;
};