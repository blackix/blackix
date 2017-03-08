// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemOculusPrivatePCH.h"
#include "OnlineSessionInterfaceOculus.h"
#include "OnlineIdentityInterface.h"
#include "OnlineFriendsInterfaceOculus.h"
#include "OnlineSubsystemOculus.h"
#include <string>

FOnlineSessionInfoOculus::FOnlineSessionInfoOculus(ovrID RoomId) :
	SessionId(FUniqueNetIdOculus(RoomId))
{
}

/**
* FOnlineSessionOculus
*/

FOnlineSessionOculus::FOnlineSessionOculus(FOnlineSubsystemOculus& InSubsystem) :
	OculusSubsystem(InSubsystem)
{
	OnRoomNotificationUpdateHandle =
		OculusSubsystem.GetNotifDelegate(ovrMessage_Notification_Room_RoomUpdate)
		.AddRaw(this, &FOnlineSessionOculus::OnRoomNotificationUpdate);

	OnRoomNotificationInviteAcceptedHandle =
		OculusSubsystem.GetNotifDelegate(ovrMessage_Notification_Room_InviteAccepted)
		.AddRaw(this, &FOnlineSessionOculus::OnRoomInviteAccepted);

	OnMatchmakingNotificationMatchFoundHandle =
		OculusSubsystem.GetNotifDelegate(ovrMessage_Notification_Matchmaking_MatchFound)
		.AddRaw(this, &FOnlineSessionOculus::OnMatchmakingNotificationMatchFound);
}

FOnlineSessionOculus::~FOnlineSessionOculus()
{
	if (OnRoomNotificationUpdateHandle.IsValid())
	{
		OculusSubsystem.RemoveNotifDelegate(ovrMessage_Notification_Room_RoomUpdate, OnRoomNotificationUpdateHandle);
		OnRoomNotificationUpdateHandle.Reset();
	}

	if (OnRoomNotificationInviteAcceptedHandle.IsValid())
	{
		OculusSubsystem.RemoveNotifDelegate(ovrMessage_Notification_Room_InviteAccepted, OnRoomNotificationInviteAcceptedHandle);
		OnRoomNotificationInviteAcceptedHandle.Reset();
	}

	if (OnMatchmakingNotificationMatchFoundHandle.IsValid())
	{
		OculusSubsystem.RemoveNotifDelegate(ovrMessage_Notification_Matchmaking_MatchFound, OnMatchmakingNotificationMatchFoundHandle);
		OnMatchmakingNotificationMatchFoundHandle.Reset();
	}

	PendingInviteAcceptedSessions.Empty();

	// Make sure the player leaves all the sessions they were in before destroying this
	for (auto It = Sessions.CreateConstIterator(); It; ++It)
	{
		auto Session = It.Value();
		auto RoomId = GetOvrIDFromSession(*Session);
		if (RoomId != 0)
		{
			ovr_Room_Leave(RoomId);
		}
		Session->SessionState = EOnlineSessionState::Destroying;
	}
	Sessions.Empty();
};

ovrID FOnlineSessionOculus::GetOvrIDFromSession(const FNamedOnlineSession& Session)
{
	// check to see if there is a SessionInfo and if so if the SessionInfo is valid
	if (!Session.SessionInfo.IsValid() || !Session.SessionInfo.Get()->IsValid())
	{
		return 0;
	}
	auto OculusId = static_cast<FUniqueNetIdOculus>(Session.SessionInfo->GetSessionId());
	return OculusId.GetID();
}

bool FOnlineSessionOculus::CreateSession(int32 HostingPlayerNum, FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		UE_LOG_ONLINE(Warning, TEXT("Cannot create session '%s': session already exists."), *SessionName.ToString());
		return false;
	}

	IOnlineIdentityPtr Identity = OculusSubsystem.GetIdentityInterface();
	if (!Identity.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("No valid oculus identity interface."));
		return false;
	}

	if (NewSessionSettings.NumPrivateConnections > 0)
	{
		UE_LOG_ONLINE(Warning, TEXT("Oculus does not support private connections"));
		return false;
	}

	// Create a new session and deep copy the game settings
	Session = AddNamedSession(SessionName, NewSessionSettings);

	check(Session);
	Session->SessionState = EOnlineSessionState::Creating;
	Session->NumOpenPrivateConnections = NewSessionSettings.NumPrivateConnections;
	Session->NumOpenPublicConnections = NewSessionSettings.NumPublicConnections;

	Session->HostingPlayerNum = HostingPlayerNum;
	Session->LocalOwnerId = OculusSubsystem.GetIdentityInterface()->GetUniquePlayerId(HostingPlayerNum);

	// Setup the join policy
	ovrRoomJoinPolicy JoinPolicy = ovrRoom_JoinPolicyInvitedUsers;
	if (NewSessionSettings.bShouldAdvertise)
	{
		if (NewSessionSettings.bAllowJoinViaPresenceFriendsOnly)
		{
			// Presence implies invites allowed
			JoinPolicy = ovrRoom_JoinPolicyFriendsOfMembers;
		}
		else if (NewSessionSettings.bAllowInvites && !NewSessionSettings.bAllowJoinViaPresence)
		{
			// Invite Only
			JoinPolicy = ovrRoom_JoinPolicyInvitedUsers;
		}
		else // bAllowJoinViaPresence
		{
			// Otherwise public
			JoinPolicy = ovrRoom_JoinPolicyEveryone;
		}
	}

	// Unique identifier of this build for compatibility
	Session->SessionSettings.BuildUniqueId = GetBuildUniqueId();

	if (NewSessionSettings.Settings.Contains(SETTING_OCULUS_POOL))
	{
		return CreateMatchmakingSession(*Session, JoinPolicy);
	}

	return CreateRoomSession(*Session, JoinPolicy);
}

bool FOnlineSessionOculus::CreateMatchmakingSession(FNamedOnlineSession& Session, ovrRoomJoinPolicy JoinPolicy)
{
	auto PoolSettings = Session.SessionSettings.Settings.Find(SETTING_OCULUS_POOL);
	FString Pool;
	PoolSettings->Data.GetValue(Pool);

	unsigned int MaxUsers = Session.SessionSettings.NumPublicConnections + Session.SessionSettings.NumPrivateConnections;

	ovrMatchmakingOptionsHandle MatchmakingOptions = ovr_MatchmakingOptions_Create();
	ovr_MatchmakingOptions_SetCreateRoomJoinPolicy(MatchmakingOptions, JoinPolicy);
	if (MaxUsers > 0)
	{
		ovr_MatchmakingOptions_SetCreateRoomMaxUsers(MatchmakingOptions, MaxUsers);
	}
	for (auto& KeyValuePair : Session.SessionSettings.Settings)
	{
		std::string Key(TCHAR_TO_UTF8(*KeyValuePair.Key.ToString()));
		std::string Value(TCHAR_TO_UTF8(*KeyValuePair.Value.Data.ToString()));
		ovr_MatchmakingOptions_SetCreateRoomDataStoreString(MatchmakingOptions, Key.c_str(), Value.c_str());
	};

	// bShouldAdvertise controls whether or not this room should be enqueued
	// or should it be enqueued later through UpdateSession
	auto RequestId = (Session.SessionSettings.bShouldAdvertise)
		? ovr_Matchmaking_CreateAndEnqueueRoom2(TCHAR_TO_ANSI(*Pool), MatchmakingOptions)
		: ovr_Matchmaking_CreateRoom2(TCHAR_TO_ANSI(*Pool), MatchmakingOptions);
	OculusSubsystem.AddRequestDelegate(
		RequestId,
		FOculusMessageOnCompleteDelegate::CreateRaw(this, &FOnlineSessionOculus::OnCreateRoomComplete, Session.SessionName));

	ovr_MatchmakingOptions_Destroy(MatchmakingOptions);

	return true;
}

bool FOnlineSessionOculus::CreateRoomSession(FNamedOnlineSession& Session, ovrRoomJoinPolicy JoinPolicy)
{
	ovrRoomOptionsHandle RoomOptions = ovr_RoomOptions_Create();
	for (auto& KeyValuePair : Session.SessionSettings.Settings)
	{
		std::string Key(TCHAR_TO_UTF8(*KeyValuePair.Key.ToString()));
		std::string Value(TCHAR_TO_UTF8(*KeyValuePair.Value.Data.ToString()));
		ovr_RoomOptions_SetDataStoreString(RoomOptions, Key.c_str(), Value.c_str());
	};

	unsigned int MaxUsers = Session.SessionSettings.NumPublicConnections + Session.SessionSettings.NumPrivateConnections;

	OculusSubsystem.AddRequestDelegate(
		ovr_Room_CreateAndJoinPrivate2(JoinPolicy, MaxUsers, RoomOptions),
		FOculusMessageOnCompleteDelegate::CreateRaw(this, &FOnlineSessionOculus::OnCreateRoomComplete, Session.SessionName));
	ovr_RoomOptions_Destroy(RoomOptions);

	return true;
}

void FOnlineSessionOculus::OnCreateRoomComplete(ovrMessageHandle Message, bool bIsError, FName SessionName)
{
	if (bIsError)
	{
		auto Error = ovr_Message_GetError(Message);
		auto ErrorMessage = ovr_Error_GetMessage(Error);
		UE_LOG_ONLINE(Error, TEXT("%s"), *FString(ErrorMessage));
		RemoveNamedSession(SessionName);
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return;
	}

	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session == nullptr)
	{
		UE_LOG_ONLINE(Error, TEXT("Session '%s': not found."), *SessionName.ToString());
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return;
	}

	if (Session->SessionState != EOnlineSessionState::Creating)
	{
		UE_LOG_ONLINE(Error, TEXT("Session '%s': already created."), *SessionName.ToString());
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return;
	}

	ovrRoomHandle Room;
	ovrID RoomId;
	auto MessageType = ovr_Message_GetType(Message);
	if (MessageType == ovrMessageType::ovrMessage_Matchmaking_CreateAndEnqueueRoom2)
	{
		auto EnqueueResultAndRoom = ovr_Message_GetMatchmakingEnqueueResultAndRoom(Message);
		Room = ovr_MatchmakingEnqueueResultAndRoom_GetRoom(EnqueueResultAndRoom);
	}
	else
	{
		Room = ovr_Message_GetRoom(Message);
	}
	RoomId = ovr_Room_GetID(Room);

	auto MaxUsers = ovr_Room_GetMaxUsers(Room);
	Session->SessionInfo = MakeShareable(new FOnlineSessionInfoOculus(RoomId));

	UpdateSessionFromRoom(*Session, Room);

	// Waiting for new players
	Session->SessionState = EOnlineSessionState::Pending;

	TriggerOnCreateSessionCompleteDelegates(SessionName, true);
}

bool FOnlineSessionOculus::CreateSession(const FUniqueNetId& HostingPlayerId, FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
	return CreateSession(0, SessionName, NewSessionSettings);
}

bool FOnlineSessionOculus::StartSession(FName SessionName)
{
	// Grab the session information by name
	auto Session = GetNamedSession(SessionName);
	if (Session == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't start an online game for session (%s) that hasn't been created"), *SessionName.ToString());
		return false;
	}

	// Can't start a match multiple times
	// Sessions can be started if they are pending or the last one has ended
	if (Session->SessionState != EOnlineSessionState::Pending && Session->SessionState != EOnlineSessionState::Ended)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't start an online session (%s) in state %s"),
			*SessionName.ToString(),
			EOnlineSessionState::ToString(Session->SessionState));
		TriggerOnStartSessionCompleteDelegates(SessionName, false);
		return false;
	}

	Session->SessionState = EOnlineSessionState::InProgress;

	TriggerOnStartSessionCompleteDelegates(SessionName, true);
	return true;
}

bool FOnlineSessionOculus::UpdateSession(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, bool bShouldRefreshOnlineData)
{
	// Grab the session information by name
	auto Session = GetNamedSession(SessionName);
	if (Session == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("There is no session (%s) to update"), *SessionName.ToString());
		return false;
	}

	auto LoggedInPlayerId = OculusSubsystem.GetIdentityInterface()->GetUniquePlayerId(0);
	if (!LoggedInPlayerId.IsValid() || *Session->OwningUserId != *LoggedInPlayerId)
	{
		UE_LOG_ONLINE(Warning, TEXT("Need to own session (%s) before updating.  Current Owner: %s"), *SessionName.ToString(), *Session->OwningUserName);
		return false;
	}

	if (Session->SessionSettings.Settings.Contains(SETTING_OCULUS_POOL))
	{
		return UpdateMatchmakingRoom(SessionName, UpdatedSessionSettings);
	}

	return UpdateRoomDataStore(SessionName, UpdatedSessionSettings);
}

bool FOnlineSessionOculus::UpdateMatchmakingRoom(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings)
{
	// Grab the session information by name
	auto Session = GetNamedSession(SessionName);
	if (Session == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("There is no session (%s) to update"), *SessionName.ToString());
		return false;
	}

	// check if bShouldAdvertise has changed.  If so, then enqueue, or cancel as appropriate
	if (Session->SessionSettings.bShouldAdvertise != UpdatedSessionSettings.bShouldAdvertise) {
		// If bShouldAdvertise flipped true then start enqueuing
		// If bShouldAdvertise flipped false then stop enqueuing
		auto RequestId = (UpdatedSessionSettings.bShouldAdvertise)
			? ovr_Matchmaking_EnqueueRoom(GetOvrIDFromSession(*Session), nullptr)
			: ovr_Matchmaking_Cancel2();

		OculusSubsystem.AddRequestDelegate(
			RequestId,
			FOculusMessageOnCompleteDelegate::CreateLambda([this, SessionName, &UpdatedSessionSettings](ovrMessageHandle Message, bool bIsError)
		{
			if (bIsError)
			{
				auto Error = ovr_Message_GetError(Message);
				auto ErrorMessage = ovr_Error_GetMessage(Error);
				UE_LOG_ONLINE(Error, TEXT("%s"), *FString(ErrorMessage));
				TriggerOnUpdateSessionCompleteDelegates(SessionName, false);
				return;
			}

			auto Session = GetNamedSession(SessionName);
			if (Session == nullptr)
			{
				UE_LOG_ONLINE(Error, TEXT("Session (%s) no longer exists"), *SessionName.ToString());
				TriggerOnUpdateSessionCompleteDelegates(SessionName, false);
				return;
			}

			// Update the Session Settings
			Session->SessionSettings.bShouldAdvertise = !Session->SessionSettings.bShouldAdvertise;

			UpdateRoomDataStore(SessionName, UpdatedSessionSettings);
		}));
	}
	else
	{
		return UpdateRoomDataStore(SessionName, UpdatedSessionSettings);
	}
	return true;
}

bool FOnlineSessionOculus::UpdateRoomDataStore(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings)
{
	// Grab the session information by name
	auto Session = GetNamedSession(SessionName);
	if (Session == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("There is no session (%s) to update"), *SessionName.ToString());
		return false;
	}

	TArray<ovrKeyValuePair> DataStore;
	// Add the updated settings that changed
	for (auto& Setting : UpdatedSessionSettings.Settings)
	{
		// Oculus matchmaking pool key cannot be added or changed
		if (Setting.Key == SETTING_OCULUS_POOL)
		{
			continue;
		}
		FOnlineSessionSetting* ExistingSetting = Session->SessionSettings.Settings.Find(Setting.Key);
		if (!ExistingSetting || ExistingSetting->Data != Setting.Value.Data)
		{
			std::string Key(TCHAR_TO_UTF8(*Setting.Key.ToString()));
			// Always convert to a string because that's the only type supported for room data store
			std::string Value(TCHAR_TO_UTF8(*Setting.Value.Data.ToString()));
			DataStore.Add(ovrKeyValuePair_makeString(_strdup(Key.c_str()), _strdup(Value.c_str())));
		}
	}

	// Clear existing keys that use to exist
	for (auto& Setting : Session->SessionSettings.Settings)
	{
		// Oculus matchmaking pool key cannot be added or changed
		if (Setting.Key == SETTING_OCULUS_POOL)
		{
			continue;
		}

		FOnlineSessionSetting* ExistingSetting = UpdatedSessionSettings.Settings.Find(Setting.Key);
		if (!ExistingSetting)
		{
			std::string Key(TCHAR_TO_UTF8(*Setting.Key.ToString()));
			DataStore.Add(ovrKeyValuePair_makeString(_strdup(Key.c_str()), ""));
		}
	}
	auto DataStoreSize = DataStore.Num();

	// If there is a delta, then fire off the request
	if (DataStoreSize > 0)
	{
		// move to a regular array to put in our OVR function
		ovrKeyValuePair* DataStoreArray = new ovrKeyValuePair[DataStoreSize];
		for (int i = 0; i < DataStoreSize; ++i)
		{
			DataStoreArray[i] = MoveTemp(DataStore[i]);
		}
		OculusSubsystem.AddRequestDelegate(
			ovr_Room_UpdateDataStore(GetOvrIDFromSession(*Session), DataStoreArray, DataStoreSize),
			FOculusMessageOnCompleteDelegate::CreateLambda([this, SessionName](ovrMessageHandle Message, bool bIsError)
		{
			if (bIsError)
			{
				auto Error = ovr_Message_GetError(Message);
				auto ErrorMessage = ovr_Error_GetMessage(Error);
				UE_LOG_ONLINE(Error, TEXT("%s"), *FString(ErrorMessage));
				TriggerOnUpdateSessionCompleteDelegates(SessionName, false);
				return;
			}

			auto Session = GetNamedSession(SessionName);
			if (Session == nullptr)
			{
				UE_LOG_ONLINE(Error, TEXT("Session (%s) no longer exists"), *SessionName.ToString());
				TriggerOnUpdateSessionCompleteDelegates(SessionName, false);
				return;
			}

			// Update the Room
			auto Room = ovr_Message_GetRoom(Message);
			UpdateSessionFromRoom(*Session, Room);

			TriggerOnUpdateSessionCompleteDelegates(SessionName, true);
		}));

		// clean up the array
		delete[] DataStoreArray;
		DataStore.Empty();
	}
	else
	{
		TriggerOnUpdateSessionCompleteDelegates(SessionName, true);
	}
	return true;
}

bool FOnlineSessionOculus::EndSession(FName SessionName)
{
	// Grab the session information by name
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't end an online game for session (%s) that hasn't been created"), *SessionName.ToString());
		return false;
	}

	// Can't end a match multiple times
	if (Session->SessionState != EOnlineSessionState::InProgress)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't end an online session (%s) in state %s"),
			*SessionName.ToString(),
			EOnlineSessionState::ToString(Session->SessionState));
		TriggerOnStartSessionCompleteDelegates(SessionName, false);
		return false;
	}

	Session->SessionState = EOnlineSessionState::Ended;

	TriggerOnEndSessionCompleteDelegates(SessionName, true);
	return true;
}

bool FOnlineSessionOculus::DestroySession(FName SessionName, const FOnDestroySessionCompleteDelegate& CompletionDelegate)
{
	// Grab the session information by name
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't leave an online game for session (%s) that doesn't exist"), *SessionName.ToString());
		return false;
	}

	auto RoomId = GetOvrIDFromSession(*Session);

	Session->SessionState = EOnlineSessionState::Destroying;

	OculusSubsystem.AddRequestDelegate(
		ovr_Room_Leave(RoomId),
		FOculusMessageOnCompleteDelegate::CreateLambda([this, SessionName, Session](ovrMessageHandle Message, bool bIsError)
	{
		// Failed to leave the room
		if (bIsError)
		{
			auto Error = ovr_Message_GetError(Message);
			auto ErrorMessage = ovr_Error_GetMessage(Error);
			UE_LOG_ONLINE(Error, TEXT("%s"), *FString(ErrorMessage));
			TriggerOnDestroySessionCompleteDelegates(SessionName, false);
			return;
		}

		RemoveNamedSession(SessionName);
		TriggerOnDestroySessionCompleteDelegates(SessionName, true);
	}));

	return true;
}

bool FOnlineSessionOculus::IsPlayerInSession(FName SessionName, const FUniqueNetId& UniqueId)
{
	/* TODO: #10920536 */
	return false;
}

bool FOnlineSessionOculus::StartMatchmaking(const TArray< TSharedRef<const FUniqueNetId> >& LocalPlayers, FName SessionName, const FOnlineSessionSettings& NewSessionSettings, TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	if (LocalPlayers.Num() > 1)
	{
		UE_LOG_ONLINE(Warning, TEXT("Oculus does not support more than one player for matchmaking"));
		return false;
	}

	FString Pool;
	if (!SearchSettings->QuerySettings.Get(SETTING_OCULUS_POOL, Pool))
	{
		UE_LOG_ONLINE(Warning, TEXT("No oculus pool specified. %s is required in SearchSettings->QuerySettings"), *SETTING_OCULUS_POOL.ToString());
		// Fall back to using the map name as the pool name
		if (!SearchSettings->QuerySettings.Get(SETTING_MAPNAME, Pool))
		{
			return false;
		}
	}

	if (NewSessionSettings.NumPrivateConnections > 0)
	{
		UE_LOG_ONLINE(Warning, TEXT("Oculus does not support private connections"));
		return false;
	}

	if (InProgressMatchmakingSearch.IsValid())
	{
		InProgressMatchmakingSearch.Reset();
	}

	SearchSettings->SearchState = EOnlineAsyncTaskState::InProgress;
	InProgressMatchmakingSearch = SearchSettings;
	InProgressMatchmakingSearchName = SessionName;

	OculusSubsystem.AddRequestDelegate(
		ovr_Matchmaking_Enqueue2(TCHAR_TO_UTF8(*Pool), nullptr),
		FOculusMessageOnCompleteDelegate::CreateLambda([this, SessionName, SearchSettings](ovrMessageHandle Message, bool bIsError)
	{
		if (bIsError)
		{
			SearchSettings->SearchState = EOnlineAsyncTaskState::Failed;
			if (InProgressMatchmakingSearch.IsValid())
			{
				InProgressMatchmakingSearch.Reset();
			}
			TriggerOnMatchmakingCompleteDelegates(SessionName, false);
		}
		// Nothing to trigger here.  
		// If a match is found, TriggerOnMatchmakingCompleteDelegates() from the notification
	}));

	return true;
}

bool FOnlineSessionOculus::CancelMatchmaking(int32 SearchingPlayerNum, FName SessionName)
{
	// If we are not searching for those matchmaking session to begin with, return as if we cancelled them
	if (!(InProgressMatchmakingSearch.IsValid() && SessionName == InProgressMatchmakingSearchName))
	{
		TriggerOnCancelMatchmakingCompleteDelegates(SessionName, true);
		return true;
	}

	OculusSubsystem.AddRequestDelegate(
		ovr_Matchmaking_Cancel2(),
		FOculusMessageOnCompleteDelegate::CreateLambda([this, SessionName](ovrMessageHandle Message, bool bIsError)
	{
		if (bIsError)
		{
			TriggerOnCancelMatchmakingCompleteDelegates(SessionName, false);
			return;
		}
		InProgressMatchmakingSearch->SearchState = EOnlineAsyncTaskState::Failed;
		InProgressMatchmakingSearch = nullptr;
		TriggerOnCancelMatchmakingCompleteDelegates(SessionName, true);
	}));

	return true;
}

bool FOnlineSessionOculus::CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, FName SessionName)
{
	return CancelMatchmaking(0, SessionName);
}

bool FOnlineSessionOculus::FindSessions(int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	if (SearchSettings->MaxSearchResults <= 0)
	{
		UE_LOG_ONLINE(Warning, TEXT("Invalid MaxSearchResults"));
		SearchSettings->SearchState = EOnlineAsyncTaskState::Failed;
		TriggerOnFindSessionsCompleteDelegates(false);
		return false;
	}

	bool bFindOnlyModeratedRooms = false;
	if (SearchSettings->QuerySettings.Get(SEARCH_OCULUS_MODERATED_ROOMS_ONLY, bFindOnlyModeratedRooms) && bFindOnlyModeratedRooms)
	{
		return FindModeratedRoomSessions(SearchSettings);
	}

	FString Pool;
	if (SearchSettings->QuerySettings.Get(SETTING_OCULUS_POOL, Pool))
	{
		return FindMatchmakingSessions(Pool, SearchSettings);
	}

	return false;
}

bool FOnlineSessionOculus::FindSessions(const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	return FindSessions(0, SearchSettings);
}

bool FOnlineSessionOculus::FindModeratedRoomSessions(const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	SearchSettings->SearchState = EOnlineAsyncTaskState::InProgress;
	OculusSubsystem.AddRequestDelegate(
		ovr_Room_GetModeratedRooms(),
		FOculusMessageOnCompleteDelegate::CreateLambda([this, SearchSettings](ovrMessageHandle Message, bool bIsError)
	{
		if (bIsError)
		{
			SearchSettings->SearchState = EOnlineAsyncTaskState::Failed;
			TriggerOnFindSessionsCompleteDelegates(false);
			return;
		}

		auto RoomArray = ovr_Message_GetRoomArray(Message);

		auto SearchResultsSize = ovr_RoomArray_GetSize(RoomArray);
		bool bHasPaging = ovr_RoomArray_HasNextPage(RoomArray);

		if (SearchResultsSize > SearchSettings->MaxSearchResults)
		{
			// Only return up to MaxSearchResults
			SearchResultsSize = SearchSettings->MaxSearchResults;
		}
		else if (bHasPaging)
		{
			// Log warning if there was still more moderated rooms that could be returned
			UE_LOG_ONLINE(Warning, TEXT("Truncated moderated rooms results returned from the server"));
		}

		SearchSettings->SearchResults.Reset(SearchResultsSize);

		for (size_t i = 0; i < SearchResultsSize; i++)
		{
			auto Room = ovr_RoomArray_GetElement(RoomArray, i);
			auto Session = CreateSessionFromRoom(Room);
			auto SearchResult = FOnlineSessionSearchResult();
			SearchResult.Session = Session.Get();
			SearchResult.PingInMs = 0; // Ping is not included in the result, but it shouldn't be considered as unreachable
			SearchSettings->SearchResults.Add(MoveTemp(SearchResult));
		}

		SearchSettings->SearchState = EOnlineAsyncTaskState::Done;
		TriggerOnFindSessionsCompleteDelegates(true);
	}));

	return true;
}

bool FOnlineSessionOculus::FindMatchmakingSessions(const FString Pool, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	SearchSettings->SearchState = EOnlineAsyncTaskState::InProgress;
	OculusSubsystem.AddRequestDelegate(
		ovr_Matchmaking_Browse2(TCHAR_TO_UTF8(*Pool), nullptr),
		FOculusMessageOnCompleteDelegate::CreateLambda([this, SearchSettings](ovrMessageHandle Message, bool bIsError)
	{
		if (bIsError)
		{
			SearchSettings->SearchState = EOnlineAsyncTaskState::Failed;
			TriggerOnFindSessionsCompleteDelegates(false);
			return;
		}

		auto BrowseResult = ovr_Message_GetMatchmakingBrowseResult(Message);

		auto RoomArray = ovr_MatchmakingBrowseResult_GetRooms(BrowseResult);

		auto SearchResultsSize = ovr_MatchmakingRoomArray_GetSize(RoomArray);

		if (SearchResultsSize > SearchSettings->MaxSearchResults)
		{
			// Only return up to MaxSearchResults
			SearchResultsSize = SearchSettings->MaxSearchResults;
		}
		// there is no paging for this array

		SearchSettings->SearchResults.Reset(SearchResultsSize);

		for (size_t i = 0; i < SearchResultsSize; i++)
		{
			auto MatchmakingRoom = ovr_MatchmakingRoomArray_GetElement(RoomArray, i);
			auto Room = ovr_MatchmakingRoom_GetRoom(MatchmakingRoom);
			auto Session = CreateSessionFromRoom(Room);
			auto SearchResult = FOnlineSessionSearchResult();
			SearchResult.Session = Session.Get();
			SearchResult.PingInMs = ovr_MatchmakingRoom_HasPingTime(MatchmakingRoom) ? ovr_MatchmakingRoom_GetPingTime(MatchmakingRoom) : 0;
			SearchSettings->SearchResults.Add(MoveTemp(SearchResult));
		}

		SearchSettings->SearchState = EOnlineAsyncTaskState::Done;
		TriggerOnFindSessionsCompleteDelegates(true);
	}));

	return true;
}

bool FOnlineSessionOculus::FindSessionById(const FUniqueNetId& SearchingUserId, const FUniqueNetId& SessionId, const FUniqueNetId& FriendId, const FOnSingleSessionResultCompleteDelegate& CompletionDelegate)
{
	auto LoggedInPlayerId = OculusSubsystem.GetIdentityInterface()->GetUniquePlayerId(0);
	if (!LoggedInPlayerId.IsValid() || SearchingUserId != *LoggedInPlayerId)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can only search session with logged in player"));
		return false;
	}

	if (FriendId.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("Optional FriendId param not supported.  Use FindFriendSession() instead."));
		return false;
	}

	auto RoomId = static_cast<const FUniqueNetIdOculus&>(SessionId);
	OculusSubsystem.AddRequestDelegate(
		ovr_Room_Get(RoomId.GetID()),
		FOculusMessageOnCompleteDelegate::CreateLambda([this, CompletionDelegate](ovrMessageHandle Message, bool bIsError)
	{
		auto SearchResult = FOnlineSessionSearchResult();

		if (bIsError)
		{
			CompletionDelegate.ExecuteIfBound(0, false, SearchResult);
			return;
		}

		auto Room = ovr_Message_GetRoom(Message);

		if (Room == nullptr)
		{
			CompletionDelegate.ExecuteIfBound(0, false, SearchResult);
			return;
		}

		auto Session = CreateSessionFromRoom(Room);
		SearchResult.Session = Session.Get();

		auto RoomJoinability = ovr_Room_GetJoinability(Room);
		CompletionDelegate.ExecuteIfBound(0, RoomJoinability == ovrRoom_JoinabilityCanJoin, SearchResult);
	}));
	
	return true;
}

bool FOnlineSessionOculus::CancelFindSessions()
{
	/* TODO: #10920536 */
	return false;
}

bool FOnlineSessionOculus::JoinSession(int32 PlayerNum, FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
	FNamedOnlineSession* Session = GetNamedSession(SessionName);

	// Don't join a session if already in one or hosting one
	if (Session)
	{
		UE_LOG_ONLINE(Warning, TEXT("Session (%s) already exists, can't join twice"), *SessionName.ToString());
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::AlreadyInSession);
		return false;
	}

	// Don't join a session without any sessioninfo
	if (!DesiredSession.Session.SessionInfo.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("No valid SessionInfo in the DesiredSession passed in"));
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::SessionDoesNotExist);
		return false;
	}

	// Create a named session from the search result data
	Session = AddNamedSession(SessionName, DesiredSession.Session);
	check(Session);
	Session->SessionState = EOnlineSessionState::Creating;
	Session->HostingPlayerNum = PlayerNum;
	Session->LocalOwnerId = OculusSubsystem.GetIdentityInterface()->GetUniquePlayerId(PlayerNum);

	auto SearchSessionInfo = static_cast<const FOnlineSessionInfoOculus*>(DesiredSession.Session.SessionInfo.Get());
	auto RoomId = (static_cast<FUniqueNetIdOculus>(SearchSessionInfo->GetSessionId())).GetID();

	OculusSubsystem.AddRequestDelegate(
		ovr_Room_Join(RoomId, /* subscribeToUpdates */ true),
		FOculusMessageOnCompleteDelegate::CreateLambda([this, SessionName, Session](ovrMessageHandle Message, bool bIsError)
	{
		auto Room = ovr_Message_GetRoom(Message);

		if (bIsError)
		{
			RemoveNamedSession(SessionName);

			auto RoomJoinability = ovr_Room_GetJoinability(Room);
			EOnJoinSessionCompleteResult::Type FailureReason;
			if (RoomJoinability == ovrRoom_JoinabilityIsFull)
			{
				FailureReason = EOnJoinSessionCompleteResult::SessionIsFull;
			}
			else if (RoomJoinability == ovrRoom_JoinabilityAreIn)
			{
				FailureReason = EOnJoinSessionCompleteResult::AlreadyInSession;
			}
			else
			{
				FailureReason = EOnJoinSessionCompleteResult::UnknownError;
			}
			TriggerOnJoinSessionCompleteDelegates(SessionName, FailureReason);
			return;
		}
		auto LocalRoomId = ovr_Room_GetID(Room);
		UpdateSessionFromRoom(*Session, Room);

		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::Success);
	}));

	return true;
}

bool FOnlineSessionOculus::JoinSession(const FUniqueNetId& PlayerId, FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
	return JoinSession(0, SessionName, DesiredSession);
}

bool FOnlineSessionOculus::FindFriendSession(int32 LocalUserNum, const FUniqueNetId& Friend)
{
	auto OculusId = static_cast<const FUniqueNetIdOculus&>(Friend);

	OculusSubsystem.AddRequestDelegate(
		ovr_Room_GetCurrentForUser(OculusId.GetID()),
		FOculusMessageOnCompleteDelegate::CreateLambda([this, LocalUserNum](ovrMessageHandle Message, bool bIsError)
	{
		auto SearchResult = FOnlineSessionSearchResult();

		if (bIsError)
		{
			TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, false, SearchResult);
			return;
		}

		auto Room = ovr_Message_GetRoom(Message);

		// Friend is not in a room
		if (Room == nullptr)
		{
			TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, false, SearchResult);
			return;
		}

		auto Session = CreateSessionFromRoom(Room);
		SearchResult.Session = Session.Get();

		auto RoomJoinability = ovr_Room_GetJoinability(Room);
		TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, RoomJoinability == ovrRoom_JoinabilityCanJoin, SearchResult);
	}));

	return true;
};

bool FOnlineSessionOculus::FindFriendSession(const FUniqueNetId& LocalUserId, const FUniqueNetId& Friend)
{
	return FindFriendSession(0, Friend);
}

bool FOnlineSessionOculus::SendSessionInviteToFriend(int32 LocalUserNum, FName SessionName, const FUniqueNetId& Friend)
{
	TArray< TSharedRef<const FUniqueNetId> > Friends;
	Friends.Add(MakeShareable(new FUniqueNetIdOculus(Friend)));
	return SendSessionInviteToFriends(LocalUserNum, SessionName, Friends);
};

bool FOnlineSessionOculus::SendSessionInviteToFriend(const FUniqueNetId& LocalUserId, FName SessionName, const FUniqueNetId& Friend)
{
	return SendSessionInviteToFriend(0, SessionName, Friend);
}

bool FOnlineSessionOculus::SendSessionInviteToFriends(int32 LocalUserNum, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends)
{
	FNamedOnlineSession* Session = GetNamedSession(SessionName);

	// Cannot invite friends to session that doesn't exist
	if (!Session)
	{
		UE_LOG_ONLINE(Warning, TEXT("Session (%s) doesn't exist"), *SessionName.ToString());
		return false;
	}

	IOnlineFriendsPtr FriendsInterface = OculusSubsystem.GetFriendsInterface();
	if (!FriendsInterface.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("Cannot get invite tokens for friends"));
		return false;
	}

	auto RoomId = GetOvrIDFromSession(*Session);

	// Fetching through the Friends Interface because we already have paging support there
	FriendsInterface->ReadFriendsList(
		LocalUserNum,
		FOnlineFriendsOculus::FriendsListInviteableUsers,
		FOnReadFriendsListComplete::CreateLambda([RoomId, FriendsInterface, Friends](int32 InLocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorName) {

		if (!bWasSuccessful)
		{
			UE_LOG_ONLINE(Warning, TEXT("Cannot get invite tokens for friends: %s"), *ErrorName);
			return;
		}

		for (auto FriendId : Friends)
		{
			auto Friend = FriendsInterface->GetFriend(
				InLocalUserNum,
				FriendId.Get(),
				ListName
			);

			if (Friend.IsValid())
			{
				auto OculusFriend = static_cast<const FOnlineOculusFriend&>(*Friend);
				ovr_Room_InviteUser(RoomId, OculusFriend.GetInviteToken());
			}
		}
	}));

	return true;
}

bool FOnlineSessionOculus::SendSessionInviteToFriends(const FUniqueNetId& LocalUserId, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends)
{
	return SendSessionInviteToFriends(0, SessionName, Friends);
}

bool FOnlineSessionOculus::PingSearchResults(const FOnlineSessionSearchResult& SearchResult)
{
	/* TODO: #10920536 */
	return false;
}

bool FOnlineSessionOculus::GetResolvedConnectString(FName SessionName, FString& ConnectInfo)
{
	// Find the session
	auto Session = GetNamedSession(SessionName);
	if (Session != nullptr)
	{
		auto OwnerId = static_cast<const FUniqueNetIdOculus*>(Session->OwningUserId.Get());
		ConnectInfo = FString::Printf(TEXT("%llu.oculus"), OwnerId->GetID());
		return true;
	}
	return false;
}

bool FOnlineSessionOculus::GetResolvedConnectString(const FOnlineSessionSearchResult& SearchResult, FName PortType, FString& ConnectInfo)
{
	if (SearchResult.IsValid())
	{
		auto Session = SearchResult.Session;
		auto OwnerId = static_cast<const FUniqueNetIdOculus*>(Session.OwningUserId.Get());
		ConnectInfo = FString::Printf(TEXT("%llu.oculus"), OwnerId->GetID());
		return true;
	}
	return false;
}

FOnlineSessionSettings* FOnlineSessionOculus::GetSessionSettings(FName SessionName)
{
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		return &Session->SessionSettings;
	}
	return nullptr;
}

bool FOnlineSessionOculus::RegisterPlayer(FName SessionName, const FUniqueNetId& PlayerId, bool bWasInvited)
{
	// The actual registration of players is done by OnRoomNotificationUpdate()
	// That way Oculus keeps the source of truth on who's actually in the room and therefore the session
	TArray< TSharedRef<const FUniqueNetId> > Players;
	Players.Add(MakeShareable(new FUniqueNetIdOculus(PlayerId)));
	TriggerOnRegisterPlayersCompleteDelegates(SessionName, Players, true);
	return true;
}

bool FOnlineSessionOculus::RegisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players, bool bWasInvited)
{
	// The actual registration of players is done by OnRoomNotificationUpdate()
	// That way Oculus keeps the source of truth on who's actually in the room and therefore the session
	TriggerOnRegisterPlayersCompleteDelegates(SessionName, Players, true);
	return true;
}

bool FOnlineSessionOculus::UnregisterPlayer(FName SessionName, const FUniqueNetId& PlayerId)
{
	/* TODO: #10920536 */
	return false;
}

bool FOnlineSessionOculus::UnregisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players)
{
	/* TODO: #10920536 */
	return false;
}

int32 FOnlineSessionOculus::GetNumSessions()
{
	return Sessions.Num();
}

void FOnlineSessionOculus::DumpSessionState()
{
	for (auto Session : Sessions)
	{
		DumpNamedSession(Session.Value.Get());
	}
}

void FOnlineSessionOculus::RegisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnRegisterLocalPlayerCompleteDelegate& Delegate)
{
	/* NOT USED */
	Delegate.ExecuteIfBound(PlayerId, EOnJoinSessionCompleteResult::UnknownError);
}

void FOnlineSessionOculus::UnregisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnUnregisterLocalPlayerCompleteDelegate& Delegate)
{
	/* NOT USED */
	Delegate.ExecuteIfBound(PlayerId, false);
}

FNamedOnlineSession* FOnlineSessionOculus::GetNamedSession(FName SessionName)
{
	if (Sessions.Contains(SessionName))
	{
		return Sessions[SessionName].Get();
	}
	return nullptr;
}

void FOnlineSessionOculus::RemoveNamedSession(FName SessionName)
{
	if (Sessions.Contains(SessionName))
	{
		Sessions.Remove(SessionName);
	}
}

EOnlineSessionState::Type FOnlineSessionOculus::GetSessionState(FName SessionName) const
{
	if (Sessions.Contains(SessionName))
	{
		return Sessions[SessionName]->SessionState;
	}
	return EOnlineSessionState::NoSession;
}
bool FOnlineSessionOculus::HasPresenceSession()
{
	for (auto it = Sessions.CreateIterator(); it; ++it)
	{
		if (it.Value()->SessionSettings.bUsesPresence)
		{
			return true;
		}
	}
	return false;
}

FNamedOnlineSession* FOnlineSessionOculus::AddNamedSession(FName SessionName, const FOnlineSessionSettings& SessionSettings)
{
	TSharedPtr<FNamedOnlineSession> Session = MakeShareable(new FNamedOnlineSession(SessionName, SessionSettings));
	Sessions.Add(SessionName, Session);
	return Session.Get();
}

FNamedOnlineSession* FOnlineSessionOculus::AddNamedSession(FName SessionName, const FOnlineSession& Session)
{
	TSharedPtr<FNamedOnlineSession> NamedSession = MakeShareable(new FNamedOnlineSession(SessionName, Session));
	Sessions.Add(SessionName, NamedSession);
	return NamedSession.Get();
}

void FOnlineSessionOculus::OnRoomNotificationUpdate(ovrMessageHandle Message, bool bIsError)
{
	if (bIsError)
	{
		UE_LOG_ONLINE(Warning, TEXT("Error on getting a room notification update"));
		return;
	}
	
	auto Room = ovr_Message_GetRoom(Message);
	auto RoomId = ovr_Room_GetID(Room);

	// Counting on the mapping of SessionName -> Session should be small
	for (auto SessionKV : Sessions)
	{
		if (SessionKV.Value.IsValid())
		{
			auto Session = SessionKV.Value.Get();
			auto SessionRoomId = GetOvrIDFromSession(*Session);
			if (RoomId == SessionRoomId)
			{
				UpdateSessionFromRoom(*Session, Room);
				return;
			}
		}
	}
	UE_LOG_ONLINE(Warning, TEXT("Session was gone before the notif update came back"));

}

void FOnlineSessionOculus::OnRoomInviteAccepted(ovrMessageHandle Message, bool bIsError)
{
	IOnlineIdentityPtr Identity = OculusSubsystem.GetIdentityInterface();
	auto PlayerId = Identity->GetUniquePlayerId(0);

	FOnlineSessionSearchResult SearchResult;
	if (bIsError)
	{
		UE_LOG_ONLINE(Warning, TEXT("Error on accepting room invite"));
		TriggerOnSessionUserInviteAcceptedDelegates(false, 0, PlayerId, SearchResult);
		return;
	}

	auto RoomIdString = ovr_Message_GetString(Message);

	ovrID RoomId;

	if (!ovrID_FromString(&RoomId, RoomIdString))
	{
		UE_LOG_ONLINE(Warning, TEXT("Could not parse the room id"));
		TriggerOnSessionUserInviteAcceptedDelegates(false, 0, PlayerId, SearchResult);
		return;
	}

	// Fetch the room details to create the SessionResult

	ovr_Room_Get(RoomId);

	OculusSubsystem.AddRequestDelegate(
		ovr_Room_Get(RoomId),
		FOculusMessageOnCompleteDelegate::CreateLambda([this, PlayerId](ovrMessageHandle InMessage, bool bInIsError)
	{
		FOnlineSessionSearchResult LocalSearchResult;

		if (bInIsError)
		{
			UE_LOG_ONLINE(Warning, TEXT("Could not get room details"));
			TriggerOnSessionUserInviteAcceptedDelegates(false, 0, PlayerId, LocalSearchResult);
			return;
		}

		auto Room = ovr_Message_GetRoom(InMessage);
		auto Session = CreateSessionFromRoom(Room);
		LocalSearchResult.Session = Session.Get();

		auto RoomJoinability = ovr_Room_GetJoinability(Room);

		// check if there's a delegate bound, if not save this session for later.
		if (!OnSessionUserInviteAcceptedDelegates.IsBound())
		{
			// No delegates are bound, just save this for later
			PendingInviteAcceptedSessions.Add(MakeShareable(&LocalSearchResult));
			return;
		}

		TriggerOnSessionUserInviteAcceptedDelegates(true, 0, PlayerId, LocalSearchResult);
	}));
}

void FOnlineSessionOculus::OnMatchmakingNotificationMatchFound(ovrMessageHandle Message, bool bIsError)
{
	if (!InProgressMatchmakingSearch.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("No matchmaking searches in progress"));
		return;
	}

	if (bIsError)
	{
		InProgressMatchmakingSearch->SearchState = EOnlineAsyncTaskState::Failed;
		InProgressMatchmakingSearch = nullptr;
		TriggerOnMatchmakingCompleteDelegates(InProgressMatchmakingSearchName, false);
		return;
	}

	auto Room = ovr_Message_GetRoom(Message);

	FOnlineSessionSearchResult SearchResult;
	auto Session = CreateSessionFromRoom(Room);
	SearchResult.Session = Session.Get();

	InProgressMatchmakingSearch->SearchResults.Add(SearchResult);

	InProgressMatchmakingSearch->SearchState = EOnlineAsyncTaskState::Done;
	InProgressMatchmakingSearch = nullptr;
	TriggerOnMatchmakingCompleteDelegates(InProgressMatchmakingSearchName, true);
}

TSharedRef<FOnlineSession> FOnlineSessionOculus::CreateSessionFromRoom(ovrRoomHandle Room)
{
	auto RoomId = ovr_Room_GetID(Room);
	auto RoomOwner = ovr_Room_GetOwner(Room);
	auto RoomMaxUsers = ovr_Room_GetMaxUsers(Room);
	auto RoomUsers = ovr_Room_GetUsers(Room);
	auto RoomCurrentUsersSize = ovr_UserArray_GetSize(RoomUsers);
	auto RoomDataStore = ovr_Room_GetDataStore(Room);

	auto SessionSettings = FOnlineSessionSettings();
	SessionSettings.NumPublicConnections = RoomMaxUsers;
	SessionSettings.NumPrivateConnections = 0;

	UpdateSessionSettingsFromDataStore(SessionSettings, RoomDataStore);

	auto Session = new FOnlineSession(SessionSettings);

	Session->OwningUserId = MakeShareable(new FUniqueNetIdOculus(ovr_User_GetID(RoomOwner)));
	Session->OwningUserName = ovr_User_GetOculusID(RoomOwner);

	Session->NumOpenPublicConnections = RoomMaxUsers - RoomCurrentUsersSize;
	Session->NumOpenPrivateConnections = 0;

	Session->SessionInfo = MakeShareable(new FOnlineSessionInfoOculus(RoomId));

	return MakeShareable(Session);
}

void FOnlineSessionOculus::UpdateSessionFromRoom(FNamedOnlineSession& Session, ovrRoomHandle Room)
{
	// update the list of players
	auto UserArray = ovr_Room_GetUsers(Room);
	auto UserArraySize = ovr_UserArray_GetSize(UserArray);

	TArray< TSharedRef<const FUniqueNetId> > Players;
	for (size_t UserIndex = 0; UserIndex < UserArraySize; ++UserIndex)
	{
		auto User = ovr_UserArray_GetElement(UserArray, UserIndex);
		auto UserId = ovr_User_GetID(User);
		Players.Add(MakeShareable(new FUniqueNetIdOculus(UserId)));
	}

	Session.RegisteredPlayers = Players;

	// Update number of open connections
	auto RemainingConnections = Session.SessionSettings.NumPublicConnections - UserArraySize;
	Session.NumOpenPublicConnections = (RemainingConnections > 0) ? RemainingConnections : 0;
	Session.NumOpenPrivateConnections = 0;

	auto RoomOwner = ovr_Room_GetOwner(Room);
	auto RoomOwnerId = ovr_User_GetID(RoomOwner);

	// Update the room owner if there is a change of ownership
	if (!Session.OwningUserId.IsValid() || static_cast<const FUniqueNetIdOculus *>(Session.OwningUserId.Get())->GetID() != RoomOwnerId)
	{
		Session.OwningUserId = MakeShareable(new FUniqueNetIdOculus(ovr_User_GetID(RoomOwner)));
		Session.OwningUserName = ovr_User_GetOculusID(RoomOwner);
		/** Whether or not this local player is hosting the session.  Assuming hosting and owning is the same for Oculus */
		Session.bHosting = (*Session.OwningUserId == *Session.LocalOwnerId);
	}

	// Update the data store
	auto RoomDataStore = ovr_Room_GetDataStore(Room);
	UpdateSessionSettingsFromDataStore(Session.SessionSettings, RoomDataStore);
}

void FOnlineSessionOculus::UpdateSessionSettingsFromDataStore(FOnlineSessionSettings& SessionSettings, ovrDataStoreHandle DataStore)
{
	// Copy everything from the data store to the sessionsettings
	auto DataStoreSize = ovr_DataStore_GetNumKeys(DataStore);
	SessionSettings.Settings.Empty(DataStoreSize); // Clear out the existing SessionSettings
	for (size_t DataStoreIndex = 0; DataStoreIndex < DataStoreSize; DataStoreIndex++)
	{
		auto DataStoreKey = ovr_DataStore_GetKey(DataStore, DataStoreIndex);
		auto DataStoreValue = ovr_DataStore_GetValue(DataStore, DataStoreKey);
		// Preserving the type of the built in settings
		if (DataStoreKey == SETTING_NUMBOTS
			|| DataStoreKey == SETTING_BEACONPORT
			|| DataStoreKey == SETTING_QOS
			|| DataStoreKey == SETTING_NEEDS
			|| DataStoreKey == SETTING_NEEDSSORT) 
		{
			int32 IntDataStoreValue = std::stoi(DataStoreValue);
			SessionSettings.Set(
				FName(DataStoreKey),
				IntDataStoreValue,
				EOnlineDataAdvertisementType::ViaOnlineService
			);
		}
		else
		{
			SessionSettings.Set(
				FName(DataStoreKey),
				FString(DataStoreValue),
				EOnlineDataAdvertisementType::ViaOnlineService
			);
		}
	}
}

void FOnlineSessionOculus::TickPendingInvites(float DeltaTime)
{
	if (PendingInviteAcceptedSessions.Num() > 0 && OnSessionUserInviteAcceptedDelegates.IsBound())
	{
		IOnlineIdentityPtr Identity = OculusSubsystem.GetIdentityInterface();
		auto PlayerId = Identity->GetUniquePlayerId(0);
		for (auto PendingInviteAcceptedSession : PendingInviteAcceptedSessions)
		{
			TriggerOnSessionUserInviteAcceptedDelegates(true, 0, PlayerId, PendingInviteAcceptedSession.Get());
		}
		PendingInviteAcceptedSessions.Empty();
	}
}
