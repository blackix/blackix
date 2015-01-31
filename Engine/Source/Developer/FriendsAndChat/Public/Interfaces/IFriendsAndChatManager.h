// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Interface for the Friends and chat manager.
 */
class IFriendsAndChatManager
{
public:

	/**
	 * Create a friends list window.
	 * @param InStyle The style to use to create the widgets
	 */
	virtual void CreateFriendsListWindow(const struct FFriendsAndChatStyle* InStyle) = 0;

	/**
	 * Create a chat window.
	 * @param InStyle The style to use to create the widgets
	 */
	virtual void CreateChatWindow(const struct FFriendsAndChatStyle* InStyle) = 0;

	/**
	 * Set the FriendsAndChatUserSettings.
	 *
	 * @param UserSettings - The Friends and chat user settings
	 */
	virtual void SetUserSettings(const FFriendsAndChatSettings& UserSettings) = 0;

	/**
	 * Set the analytics provider for capturing friends/chat events
	 *
	 * @param AnalyticsProvider the provider to use
	 */
	virtual void SetAnalyticsProvider(const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider) = 0;

	/**
	 * Create the a friends list widget without a container.
	 * @param InStyle The style to use to create the widgets.
	 * @return The Friends List widget.
	 */
	virtual TSharedPtr< SWidget > GenerateFriendsListWidget( const struct FFriendsAndChatStyle* InStyle ) = 0;

	/**
	 * Generate a chat widget.
	 * @param InStyle The style to use to create the widgets.
	 * @param The chat view model.
	 * @return The chat widget.
	 */
	virtual TSharedPtr< SWidget > GenerateChatWidget(const FFriendsAndChatStyle* InStyle, TSharedRef<IChatViewModel> ViewModel) = 0;

	/**
	 * Get the chat system view model for manipulating the chat widget.
	 * @return The chat view model.
	 */
	virtual TSharedPtr<IChatViewModel> GetChatViewModel() = 0;

	/**
	 * Insert a network chat message.
	 * @param InMessage The chat message.
	 */
	virtual void InsertNetworkChatMessage(const FString& InMessage) = 0;

	/**
	 * Join a global chat room
	 */
	virtual void JoinPublicChatRoom(const FString& RoomName) = 0;

	/** Log out and kill the friends list window. */
	virtual void Logout() = 0;

	/** Log in and start checking for Friends. */
	virtual void Login() = 0;

	/** Is the chat manager logged in. */
	virtual bool IsLoggedIn() = 0;

	/** 
	 * Set the application view model to query and perform actions on.
	 * @param ApplicationViewModel The view model.
	 */
	virtual void SetApplicationViewModel(TSharedPtr<IFriendsApplicationViewModel> ApplicationViewModel) = 0;

	DECLARE_EVENT_OneParam(IFriendsAndChatManager, FOnFriendsNotificationEvent, const bool /*Show or Clear */)
	virtual FOnFriendsNotificationEvent& OnFriendsNotification() = 0;

	DECLARE_EVENT_OneParam(IFriendsAndChatManager, FOnFriendsNotificationActionEvent, TSharedRef<FFriendsAndChatMessage> /*Chat notification*/)
	virtual FOnFriendsNotificationActionEvent& OnFriendsActionNotification() = 0;

	DECLARE_EVENT_OneParam(IFriendsAndChatManager, FOnFriendsUserSettingsUpdatedEvent, /*struct*/ FFriendsAndChatSettings& /* New Options */)
	virtual FOnFriendsUserSettingsUpdatedEvent& OnFriendsUserSettingsUpdated() = 0;

	DECLARE_EVENT_TwoParams(IFriendsAndChatManager, FOnFriendsJoinGameEvent, const FUniqueNetId& /*FriendId*/, const FString& /*SessionId*/)
	virtual FOnFriendsJoinGameEvent& OnFriendsJoinGame() = 0;

	DECLARE_EVENT(IFriendsAndChatManager, FChatMessageReceivedEvent);
	virtual FChatMessageReceivedEvent& OnChatMessageRecieved() = 0;

	DECLARE_DELEGATE_RetVal(bool, FAllowFriendsJoinGame);
	virtual FAllowFriendsJoinGame& AllowFriendsJoinGame() = 0;

public:

	/** Virtual destructor. */
	virtual ~IFriendsAndChatManager() { }
};
