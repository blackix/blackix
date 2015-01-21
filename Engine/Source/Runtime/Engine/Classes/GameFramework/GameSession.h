// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Multiplayer game session.
 */

#pragma once
#include "GameFramework/Info.h"
#include "GameSession.generated.h"

class UWorld;
class APlayerController;

UCLASS(config=Game, notplaceable)
class ENGINE_API AGameSession : public AInfo
{
	GENERATED_UCLASS_BODY()

	/** Maximum number of spectators allowed by this server. */
	UPROPERTY(globalconfig)
	int32 MaxSpectators;

	/** Maximum number of players allowed by this server. */
	UPROPERTY(globalconfig)
	int32 MaxPlayers;

	/** Maximum number of splitscreen players to allow from one connection */
	UPROPERTY(globalconfig)
	uint8 MaxSplitscreensPerConnection;

    /** Is voice enabled always or via a push to talk keybinding */
	UPROPERTY(globalconfig)
	bool bRequiresPushToTalk;

	/** SessionName local copy from PlayerState class.  should really be define in this class, but need to address replication issues */
	UPROPERTY()
	FName SessionName;

	/** Initialize options based on passed in options string */
	virtual void InitOptions( const FString& Options );

	/** @return A new unique player ID */
	int32 GetNextPlayerID();

	//=================================================================================
	// LOGIN

	/** 
	 * Allow an online service to process a login if specified on the commandline with -auth_login/-auth_password
	 * @return true if login is in progress, false otherwise
	 */
	virtual bool ProcessAutoLogin();

    /** Delegate triggered on auto login completion */
	virtual void OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);

	/** 
	 * Called from GameMode.PreLogin() and Login().
	 * @param	Options	The URL options (e.g. name/spectator) the player has passed
	 * @return	Non-empty Error String if player not approved
	 */
	virtual FString ApproveLogin(const FString& Options);

	/**
	 * Register a player with the online service session
	 * @param NewPlayer player to register
	 * @param UniqueId uniqueId they sent over on Login
	 * @param bWasFromInvite was this from an invite
	 */
	virtual void RegisterPlayer(APlayerController* NewPlayer, const TSharedPtr<FUniqueNetId>& UniqueId, bool bWasFromInvite);

	/**
	 * Called by GameMode::PostLogin to give session code chance to do work after PostLogin
	 * @param NewPlayer player logging in
	 */
	virtual void PostLogin(APlayerController* NewPlayer);

	/** @return true if there is no room on the server for an additional player */
	virtual bool AtCapacity(bool bSpectator);

	//=================================================================================
	// LOGOUT

	/** Called when a PlayerController logs out of game. */
	virtual void NotifyLogout(APlayerController* PC);

	/** Unregister a player from the online service session	 */
	virtual void UnregisterPlayer(APlayerController* ExitingPlayer);

	/**
	 * Add a player to the admin list of this session
	 *
	 * @param AdminPlayer player to add to the list
	 */
	virtual void AddAdmin(APlayerController* AdminPlayer);

	/**
	 * Remove a player from the admin list of this session
	 *
	 * @param AdminPlayer player to remove from the list
	 */
	virtual void RemoveAdmin(APlayerController* AdminPlayer);

	/** 
	 * Forcibly remove player from the server
	 *
	 * @param KickedPlayer player to kick
	 * @param KickReason text reason to display to player
	 *
	 * @return true if player was able to be kicked, false otherwise
	 */
	virtual bool KickPlayer(APlayerController* KickedPlayer, const FText& KickReason);

	/**
	 * Forcibly remove player from the server and ban them permanently
	 *
	 * @param BannedPlayer player to ban
	 * @param KickReason text reason to display to player
	 *
	 * @return true if player was able to be banned, false otherwise
	 */
	virtual bool BanPlayer(APlayerController* BannedPlayer, const FText& BanReason);

	/** Gracefully tell all clients then local players to return to lobby */
	virtual void ReturnToMainMenuHost();

	/** 
	 * called after a seamless level transition has been completed on the *new* GameMode
	 * used to reinitialize players already in the game as they won't have *Login() called on them
	 */
	virtual void PostSeamlessTravel();

	//=================================================================================
	// SESSION INFORMATION

	/** Restart the session	 */
	virtual void Restart() {}

	/** Allow a dedicated server a chance to register itself with an online service */
	virtual void RegisterServer();

	/**
	 * Update session join parameters
	 *
	 * @param SessionName name of session to update
	 * @param bPublicSearchable can the game be found via matchmaking
	 * @param bAllowInvites can you invite friends
	 * @param bJoinViaPresence anyone who can see you can join the game
	 * @param bJoinViaPresenceFriendsOnly can only friends actively join your game 
	 */
	virtual void UpdateSessionJoinability(FName SessionName, bool bPublicSearchable, bool bAllowInvites, bool bJoinViaPresence, bool bJoinViaPresenceFriendsOnly);

	/**
	 * Travel to a session URL (as client) for a given session
	 *
	 * @param ControllerId controller initiating the session travel
	 * @param SessionName name of session to travel to
	 *
	 * @return true if successful, false otherwise
	 */
	virtual bool TravelToSession(int32 ControllerId, FName SessionName);

    /**
     * Does the session require push to talk
     * @return true if a push to talk keybinding is required or if voice is always enabled
     */
	virtual bool RequiresPushToTalk() const { return bRequiresPushToTalk; }

	/** Dump session info to log for debugging.	  */
	virtual void DumpSessionState();

	//=================================================================================
	// MATCH INTERFACE

	/** @RETURNS true if GameSession handled the request, in case it wants to stall for some reason. Otherwise, game mode will start immediately */
	virtual bool HandleStartMatchRequest();

	/** Handle when the match enters waiting to start */
	virtual void HandleMatchIsWaitingToStart();

	/** Handle when the match has started */
	virtual void HandleMatchHasStarted();

	/** Handle when the match has completed */
	virtual void HandleMatchHasEnded();

	/** Called from GameMode.RestartGame(). */
	virtual bool CanRestartGame();

private:
	// Hidden functions that don't make sense to use on this class.
	HIDE_ACTOR_TRANSFORM_FUNCTIONS();

	FDelegateHandle OnLoginCompleteDelegateHandle;
};

/** 
 * Returns the player controller associated with this net id
 * @param PlayerNetId the id to search for
 * @return the player controller if found, otherwise NULL
 */
ENGINE_API APlayerController* GetPlayerControllerFromNetId(UWorld* World, const FUniqueNetId& PlayerNetId);




