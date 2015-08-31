// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineExternalUIInterface.h"
#include "OnlineSubsystemIOSTypes.h"

class FOnlineSubsystemIOS;

class FOnlineExternalUIIOS : public IOnlineExternalUI
{
PACKAGE_SCOPE:

	FOnlineExternalUIIOS(FOnlineSubsystemIOS* InSubsystem);

public:

	// Begin IOnlineExternalUI interface
	virtual bool ShowLoginUI(const int ControllerIndex, bool bShowOnlineOnly, const FOnLoginUIClosedDelegate& Delegate) override;
	virtual bool ShowFriendsUI(int32 LocalUserNum) override;
	virtual bool ShowInviteUI(int32 LocalUserNum, FName SessionMame = GameSessionName) override;
	virtual bool ShowAchievementsUI(int32 LocalUserNum) override;
	virtual bool ShowLeaderboardUI(const FString& LeaderboardName) override;
	virtual bool ShowWebURL(const FString& WebURL) override;
	virtual bool ShowProfileUI(const FUniqueNetId& Requestor, const FUniqueNetId& Requestee, const FOnProfileUIClosedDelegate& Delegate) override;
	virtual bool ShowAccountUpgradeUI(const FUniqueNetId& UniqueId) override;
	// End IOnlineExternalUI interface
	
private:
	FOnlineSubsystemIOS* Subsystem;
	FDelegateHandle CompleteDelegate;
	FOnLoginUIClosedDelegate CopiedDelegate;

    void OnLoginComplete(int ControllerIndex, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& ErrorString);
};

typedef TSharedPtr<FOnlineExternalUIIOS, ESPMode::ThreadSafe> FOnlineExternalUIIOSPtr;