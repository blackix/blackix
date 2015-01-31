// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "ChatDisplayOptionsViewModel.h"
#include "ChatViewModel.h"
#include "ChatItemViewModel.h"

class FChatDisplayOptionsViewModelImpl
	: public FChatDisplayOptionsViewModel
{
public:

// Begin IChatViewModel interface
	
	virtual void SetFocus() override
	{
		OnChatListSetFocus().Broadcast();
	}

	virtual void SetEntryBarVisibility(EVisibility Visibility)
	{
		ChatEntryVisibility = Visibility;
	}

	virtual EVisibility GetEntryBarVisibility() const
	{
		return ChatEntryVisibility;
	}

	virtual void SetFontOverrideColor(FSlateColor InOverrideColor) override
	{
		OverrideColor = InOverrideColor;
	}

	virtual void SetOverrideColorActive(bool bSet) override
	{
		bUseOverrideColor = bSet;
	}

	virtual bool GetOverrideColorSet() override
	{
		return bUseOverrideColor;
	}

	virtual FSlateColor GetFontOverrideColor() const override
	{
		return OverrideColor;
	}

	virtual void SetInGameUI(bool bIsInGame) override
	{
		bInGame = bIsInGame;
		ChatViewModel->SetInGame(bInGame);
	}

// End IChatViewModel interface

// Begin ChatDisplayOptionsViewModel interface

	virtual void SetCaptureFocus(bool bInCaptureFocus) override
	{
		bCaptureFocus = bInCaptureFocus;
	}

	virtual void SetChannelUserClicked(const TSharedRef<FChatItemViewModel> ChatItemSelected) override
	{
		ChatViewModel->SetChannelUserClicked(ChatItemSelected);
	}

	virtual void SetTimeDisplayTransparency(const float TimeTransparency)
	{
		TimeDisplayTransaprency = TimeTransparency;
	}

	virtual const bool ShouldCaptureFocus() const override
	{
		return bCaptureFocus;
	}

	virtual const bool IsChatHidden() override
	{
		return ChatViewModel->GetFilteredChatList().Num() == 0 || (bInGame && GetOverrideColorSet());
	}

	virtual TSharedPtr<class FChatViewModel> GetChatViewModel() const override
	{
		return ChatViewModel;
	}

	virtual const float GetTimeTransparency() const override
	{
		return TimeDisplayTransaprency;
	}

	virtual const EVisibility GetTextEntryVisibility() override
	{
		if(GetEntryBarVisibility() == EVisibility::Visible)
		{
			return ChatViewModel->HasActionPending() ? EVisibility::Collapsed : EVisibility::Visible;
		}
		return EVisibility::Collapsed;
	}

	virtual const EVisibility GetConfirmationVisibility() override
	{
		if(GetEntryBarVisibility() == EVisibility::Visible)
		{
			return ChatViewModel->HasActionPending() ? EVisibility::Visible : EVisibility::Collapsed;
		}
		return EVisibility::Collapsed;
	}

	virtual bool SendMessage(const FText NewMessage) override
	{
		bool bSuccess = false;
		if(!NewMessage.IsEmpty())
		{
			if(ChatViewModel->GetChatChannel() == EChatMessageType::Party)
			{
				OnNetworkMessageSentEvent().Broadcast(NewMessage.ToString());
				bSuccess = true;
				FFriendsAndChatManager::Get()->GetAnalytics().RecordChannelChat(TEXT("Party"));
			}
			else
			{
				bSuccess = ChatViewModel->SendMessage(NewMessage);
			}
		}
		else if(bInGame && GetEntryBarVisibility() == EVisibility::Visible)
		{
			SetEntryBarVisibility(EVisibility::Collapsed);
		}

		// Callback to let some UI know to stay active
		OnChatMessageCommitted().Broadcast();
		return bSuccess;
	}

	virtual void EnumerateChatChannelOptionsList(TArray<EChatMessageType::Type>& OUTChannelType) override
	{
		OUTChannelType.Add(EChatMessageType::Global);
		if (OnNetworkMessageSentEvent().IsBound() && FFriendsAndChatManager::Get()->IsInGameSession())
		{
			OUTChannelType.Add(EChatMessageType::Party);
		}
	}

// End ChatDisplayOptionsViewModel Interface

	DECLARE_DERIVED_EVENT(FChatDisplayOptionsViewModelImpl , IChatViewModel::FChatListUpdated, FChatListUpdated);
	virtual FChatListUpdated& OnChatListUpdated() override
	{
		return ChatListUpdatedEvent;
	}

	DECLARE_DERIVED_EVENT(FChatDisplayOptionsViewModelImpl, IChatViewModel::FOnFriendsChatMessageCommitted, FOnFriendsChatMessageCommitted)
	virtual FOnFriendsChatMessageCommitted& OnChatMessageCommitted() override
	{
		return ChatMessageCommittedEvent;
	}

	DECLARE_DERIVED_EVENT(FChatDisplayOptionsViewModelImpl, IChatViewModel::FOnFriendsSendNetworkMessageEvent, FOnFriendsSendNetworkMessageEvent)
	virtual FOnFriendsSendNetworkMessageEvent& OnNetworkMessageSentEvent() override
	{
		return FriendsSendNetworkMessageEvent;
	}

	DECLARE_DERIVED_EVENT(FChatDisplayOptionsViewModelImpl , FChatDisplayOptionsViewModel::FChatListSetFocus, FChatListSetFocus);
	virtual FChatListSetFocus& OnChatListSetFocus() override
	{
		return ChatListSetFocusEvent;
	}

private:
	void Initialize()
	{
		ChatViewModel->OnChatListUpdated().AddSP(this, &FChatDisplayOptionsViewModelImpl::HandleChatLisUpdated);
	}

	void HandleChatLisUpdated()
	{
		OnChatListUpdated().Broadcast();
	}

	FChatDisplayOptionsViewModelImpl(const TSharedRef<FChatViewModel>& ChatViewModel)
		: ChatViewModel(ChatViewModel)
		, TimeDisplayTransaprency(0.f)
		, bUseOverrideColor(false)
		, bInGame(false)
		, bAllowGlobalChat(true)
		, bCaptureFocus(false)
		, bAllowJoinGame(false)
	{
	}

private:

	TSharedPtr<FChatViewModel> ChatViewModel;
	float TimeDisplayTransaprency;
	bool bUseOverrideColor;
	bool bInGame;
	bool bAllowGlobalChat;
	bool bCaptureFocus;
	bool bAllowJoinGame;

	FChatListUpdated ChatListUpdatedEvent;
	FOnFriendsChatMessageCommitted ChatMessageCommittedEvent;
	FOnFriendsSendNetworkMessageEvent FriendsSendNetworkMessageEvent;
	FChatListSetFocus ChatListSetFocusEvent;
	EVisibility ChatEntryVisibility;
	FSlateColor OverrideColor;

private:
	friend FChatDisplayOptionsViewModelFactory;
};

TSharedRef< FChatDisplayOptionsViewModel > FChatDisplayOptionsViewModelFactory::Create(const TSharedRef<FChatViewModel>& ChatViewModel)
{
	TSharedRef< FChatDisplayOptionsViewModelImpl > ViewModel(new FChatDisplayOptionsViewModelImpl(ChatViewModel));
	ViewModel->Initialize();
	return ViewModel;
}