// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FDirectoryWatcherLinux : public IDirectoryWatcher
{
public:
	FDirectoryWatcherLinux();
	virtual ~FDirectoryWatcherLinux();

	virtual bool RegisterDirectoryChangedCallback(const FString& Directory, const FDirectoryChanged& InDelegate) override;
	virtual bool UnregisterDirectoryChangedCallback(const FString& Directory, const FDirectoryChanged& InDelegate) override;
	virtual bool RegisterDirectoryChangedCallback_Handle(const FString& Directory, const FDirectoryChanged& InDelegate, FDelegateHandle& OutHandle) override;
	virtual bool UnregisterDirectoryChangedCallback_Handle(const FString& Directory, FDelegateHandle InHandle) override;
	virtual void Tick (float DeltaSeconds) override;

	/** Map of directory paths to requests */
	TMap<FString, FDirectoryWatchRequestLinux*> RequestMap;
	TArray<FDirectoryWatchRequestLinux*> RequestsPendingDelete;

	/** A count of FDirectoryWatchRequestLinux created to ensure they are cleaned up on shutdown */
	int32 NumRequests;
};

typedef FDirectoryWatcherLinux FDirectoryWatcher;
