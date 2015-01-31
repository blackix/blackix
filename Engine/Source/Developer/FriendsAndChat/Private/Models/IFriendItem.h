// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Class containing the friend information - used to build the list view.
 */
class IFriendItem
{
public:

	/**
	 * Get the on-line user associated with this account.
	 *
	 * @return The online user.
	 * @see GetOnlineFriend
	 */
	virtual const TSharedPtr< FOnlineUser > GetOnlineUser() const = 0;

	/**
	 * Get the cached on-line Friend.
	 *
	 * @return The online friend.
	 * @see GetOnlineUser, SetOnlineFriend
	 */
	virtual const TSharedPtr< FOnlineFriend > GetOnlineFriend() const = 0;

	/**
	 * Get the cached user name.
	 * @return The user name.
	 */
	virtual const FString GetName() const = 0;

	/**
	 * Get the user location.
	 * @return The user location.
	 */
	virtual const FText GetFriendLocation() const = 0;

	/**
	 * Get the client the user is logged in on
	 * @return The client id
	 */
	virtual const FString GetClientId() const = 0;

	/**
	 * Get if the user is online.
	 * @return The user online state.
	 */
	virtual const bool IsOnline() const = 0;

	/**
	 * Get the online status of the user
	 * @return online presence status
	 */
	virtual EOnlinePresenceState::Type GetOnlineStatus() const = 0;

	/**
	 * Get if the user is online and his game is joinable
	 * @return The user joinable game state.
	 */
	virtual bool IsGameJoinable() const = 0;

	/**
	 * Get game session id that this friend is currently in
	 * @return The id of the game session
	 */
	virtual FString GetGameSessionId() const = 0;

	/**
	 * Get the Unique ID.
	 * @return The Unique Net ID.
	 */
	virtual const TSharedRef< FUniqueNetId > GetUniqueID() const = 0;

	/**
	 * Is this friend in the default list.
	 * @return The List Type.
	 */
	virtual const EFriendsDisplayLists::Type GetListType() const = 0;

	/**
	 * Set new online friend.
	 *
	 * @param InOnlineFriend The new online friend.
	 * @see GetOnlineFriend
	 */
	virtual void SetOnlineFriend( TSharedPtr< FOnlineFriend > InOnlineFriend ) = 0;

	/**
	 * Set new online user.
	 *
	 * @param InOnlineUser The new online user.
	 */
	virtual void SetOnlineUser( TSharedPtr< FOnlineUser > InOnlineFriend ) = 0;

	/**
	 * Clear updated flag.
	 */
	virtual void ClearUpdated() = 0;

	/**
	 * Check if we have been updated.
	 *
	 * @return true if updated.
	 */
	virtual bool IsUpdated() = 0;

	/** Set if pending invitation response. */
	virtual void SetPendingInvite() = 0;

	/** Set if pending invitation response. */
	virtual void SetPendingAccept() = 0;

	/** Set if pending delete. */
	virtual void SetPendingDelete() = 0;

	/** Get if pending delete. */
	virtual bool IsPendingDelete() const = 0;

	/** Get if pending invitation response. */
	virtual bool IsPendingAccepted() const = 0;

	/** Get if is from a game request. */
	virtual bool IsGameRequest() const = 0;

	/**
	 * Get the invitation status.
	 *
	 * @return The invitation status.
	 */
	virtual EInviteStatus::Type GetInviteStatus() = 0;

public:

	/** Virtual destructor. */
	virtual ~IFriendItem() { }
};
