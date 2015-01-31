// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "SFriendsAndChatCombo.h"

#define LOCTEXT_NAMESPACE "SFriendsAndChatCombo"

class SFriendsAndChatComboButton : public SComboButton
{

private:

	/** Helper class to override standard button's behavior, to show it in "pressed" style depending on dropdown state. */
	class SCustomDropdownButton : public SButton
	{
	public:

		/** Show as pressed if dropdown is opened. */
		virtual bool IsPressed() const override
		{
			return IsMenuOpenDelegate.IsBound() && IsMenuOpenDelegate.Execute();
		}

		/** Delegate set by parent SFriendsAndChatCombo, checks open state. */
		FIsMenuOpen IsMenuOpenDelegate;
	};

public:

	SLATE_USER_ARGS(SFriendsAndChatComboButton)
		: _bShowIcon(false)
		, _IconBrush(nullptr)
		, _bSetButtonTextToSelectedItem(false)
		, _bAutoCloseWhenClicked(true)
		, _ButtonSize(150, 36)
		, _Placement(MenuPlacement_ComboBox)
	{}

		/** Text to display on main button. */
		SLATE_TEXT_ATTRIBUTE(ButtonText)

		/** Whether the optional icon is shown */
		SLATE_ATTRIBUTE(bool, bShowIcon)

		/** Optional icon brush */
		SLATE_ATTRIBUTE(const FSlateBrush*, IconBrush)

		SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)

		/** If true, text displayed on the main button will be set automatically after user selects a dropdown item */
		SLATE_ARGUMENT(bool, bSetButtonTextToSelectedItem)

		/** List of items to display in dropdown list. */
		SLATE_ATTRIBUTE(SFriendsAndChatCombo::FItemsArray, DropdownItems)

		/** Should the dropdown list be closed automatically when user clicks an item. */
		SLATE_ARGUMENT(bool, bAutoCloseWhenClicked)

		/** Size of the button content. Needs to be supplied manually, because dropdown must also be scaled manually. */
		SLATE_ARGUMENT(FVector2D, ButtonSize)

		/** Popup menu placement. */
		SLATE_ATTRIBUTE(EMenuPlacement, Placement)

		/** Called when user clicks an item from the dropdown. */
		SLATE_EVENT(FOnDropdownItemClicked, OnDropdownItemClicked)

		/** Called when dropdown is opened (main button is clicked). */
		SLATE_EVENT(FOnComboBoxOpened, OnDropdownOpened)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		FriendStyle = *InArgs._FriendStyle;
		bAutoCloseWhenClicked = InArgs._bAutoCloseWhenClicked;
		ButtonSize = InArgs._ButtonSize;
		DropdownItems = InArgs._DropdownItems;
		OnItemClickedDelegate = InArgs._OnDropdownItemClicked;
		ButtonText = InArgs._ButtonText;
		bShowIcon = InArgs._bShowIcon;
		IconBrush = InArgs._IconBrush;
		bSetButtonTextToSelectedItem = InArgs._bSetButtonTextToSelectedItem;

		const FComboButtonStyle& ComboButtonStyle = FriendStyle.FriendListComboButtonStyle;

		MenuBorderBrush = &ComboButtonStyle.MenuBorderBrush;
		MenuBorderPadding = ComboButtonStyle.MenuBorderPadding;

		OnComboBoxOpened = InArgs._OnDropdownOpened;
		OnGetMenuContent = FOnGetContent::CreateSP(this, &SFriendsAndChatComboButton::GetMenuContent);

		// Purposefully not calling SComboButton's constructor as we want to override more of the visuals

		SMenuAnchor::Construct(SMenuAnchor::FArguments()
			.Placement(InArgs._Placement)
			.Method(EPopupMethod::UseCurrentWindow)
			[
				SAssignNew(DropdownButton, SCustomDropdownButton)
				.ButtonStyle(&ComboButtonStyle.ButtonStyle)
				.ClickMethod(EButtonClickMethod::MouseDown)
				.OnClicked(this, &SFriendsAndChatComboButton::OnButtonClicked)
				.ContentPadding(0)
				.ForegroundColor(FLinearColor::White)
				[
					SNew(SBox)
					.WidthOverride(ButtonSize.X)
					.HeightOverride(ButtonSize.Y)
					.Padding(FMargin(8, 0, 0, 0))
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.Padding(FMargin(0, 0, 8, 0))
						.AutoWidth()
						[
							SNew(SImage)
							.Visibility(this, &SFriendsAndChatComboButton::GetIconVisibility)
							.Image(this, &SFriendsAndChatComboButton::GetIconBrush)
						]
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.Padding(FMargin(0, 2, 22, 0))
						[
							SNew(STextBlock)
							.Visibility(this, &SFriendsAndChatComboButton::GetTextVisibility)
							.Text(this, &SFriendsAndChatComboButton::GetButtonText)
							.Font(FriendStyle.FriendsFontStyleBold)
							.ShadowOffset(FVector2D(0, 1))
							.ShadowColorAndOpacity(FColor(0, 84, 80))
						]
					]
				]
			]
		);

		if (DropdownButton.IsValid())
		{
			DropdownButton->IsMenuOpenDelegate = FIsMenuOpen::CreateSP(this, &SMenuAnchor::IsOpen);
		}
	}

protected:

	/** Overridden to pass button click to SComboBox */
	virtual FReply OnButtonClicked() override
	{
		return SComboButton::OnButtonClicked();
	}

	/** Returns pointer to dropdown button */
	TSharedPtr<SButton> GetDropdownButton()
	{
		return DropdownButton;
	}

private:

	/** Unlike generic ComboBox, SFriendsAndChatCombo has well defined content, created right here. */
	TSharedRef<SWidget> GetMenuContent()
	{
		TSharedPtr<SVerticalBox> EntriesWidget = SNew(SVerticalBox);

		const SFriendsAndChatCombo::FItemsArray& DropdownItemsRef = DropdownItems.Get();
		DropdownItemButtons.Empty();
		DropdownItemButtons.AddZeroed(DropdownItemsRef.Num());

		for (int32 Idx = 0; Idx < DropdownItemsRef.Num(); ++Idx)
		{
			EntriesWidget->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0, 0, 0, 1))
			[
				SAssignNew(DropdownItemButtons[Idx], SButton)
				.ButtonStyle(&FriendStyle.FriendListItemButtonStyle)
				.ContentPadding(FMargin(8, 2, 8, 2))
				.IsEnabled(DropdownItemsRef[Idx].bIsEnabled)
				.OnClicked(this, &SFriendsAndChatComboButton::OnDropdownButtonClicked, Idx)
				[
					SNew(STextBlock)
					.Text(DropdownItemsRef[Idx].EntryText)
					.Font(FriendStyle.FriendsFontStyleSmallBold)
					.ColorAndOpacity(this, &SFriendsAndChatComboButton::GetTextColor, Idx)
					.ShadowOffset(FVector2D(0, 1))
					.ShadowColorAndOpacity(FColor(35, 14, 12))
				]
			];
		}

		TSharedPtr<SHorizontalBox> BackgroundBox = SNew(SHorizontalBox);

		if (Placement.Get() == MenuPlacement_ComboBoxRight)
		{

			BackgroundBox->AddSlot()
			[
				SNew(SImage)
				.Image(&FriendStyle.FriendComboBackgroundRightFlippedBrush)
			];

			BackgroundBox->AddSlot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(ButtonSize.X - 10)
				[
					SNew(SImage)
					.Image(&FriendStyle.FriendComboBackgroundLeftFlippedBrush)
				]
			];
		}
		else
		{
			BackgroundBox->AddSlot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(ButtonSize.X - 30)
				[
					SNew(SImage)
					.Image(&FriendStyle.FriendComboBackgroundLeftBrush)
				]
			];

			BackgroundBox->AddSlot()
			[
				SNew(SImage)
				.Image(&FriendStyle.FriendComboBackgroundRightBrush)
			];
		}

		return
		SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			BackgroundBox.ToSharedRef()
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBox)
			.Padding(FMargin(8, 8, 8, 8))
			[
				EntriesWidget.ToSharedRef()
			]
		];
	}

	/** Returns color of text on dropdown button Idx. */
	FSlateColor GetTextColor(int32 Idx) const
	{
		if (DropdownItemButtons.IsValidIndex(Idx) && DropdownItemButtons[Idx].IsValid() && DropdownItemButtons[Idx]->IsHovered())
		{
			return FSlateColor(FColor::White);
		}
		return FSlateColor(FColor(255, 246, 235));
	}

	/** Called when user clicks item from the dropdown. Calls OnItemClickedDelegate and potentially closes the menu. */
	FReply OnDropdownButtonClicked(int32 Idx)
	{
		if (bSetButtonTextToSelectedItem && DropdownItems.IsSet() && DropdownItems.Get().IsValidIndex(Idx))
		{
			ButtonText = DropdownItems.Get()[Idx].EntryText;
			IconBrush = DropdownItems.Get()[Idx].EntryIcon;
		}
		if (OnItemClickedDelegate.IsBound() && DropdownItems.IsSet() && DropdownItems.Get().IsValidIndex(Idx))
		{
			OnItemClickedDelegate.Execute(DropdownItems.Get()[Idx].ButtonTag);
		}
		if (bAutoCloseWhenClicked)
		{
			SetIsOpen(false);
		}

		return FReply::Handled();
	}

	/** Returns text to display on the main button */
	FText GetButtonText() const
	{
		return ButtonText.Get();
	}

	EVisibility GetIconVisibility() const
	{
		return bShowIcon.Get() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility GetTextVisibility() const
	{
		return GetButtonText().IsEmptyOrWhitespace() ? EVisibility::Collapsed : EVisibility::Visible;
	}

	const FSlateBrush* GetIconBrush() const
	{
		return IconBrush.Get();
	}

	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;

	/** String value displayed on the main button */
	TAttribute<FText> ButtonText;

	/** Whether the optional icon is shown */
	TAttribute<bool> bShowIcon;

	/** Optional icon brush */
	TAttribute<const FSlateBrush*> IconBrush;

	/** Delegate to call when user clicks item from the dropdown list. */
	FOnDropdownItemClicked OnItemClickedDelegate;

	/** Cached list of items to generate menu content with. */
	TAttribute<SFriendsAndChatCombo::FItemsArray> DropdownItems;

	/** Cached actual dropdown button */
	TSharedPtr<SCustomDropdownButton> DropdownButton;

	/** Cached list of buttons corresponding to items from DropdownItems. */
	TArray< TSharedPtr<SButton> > DropdownItemButtons;

	/** If true, text displayed on the main button will be set automatically after user selects a dropdown item */
	bool bSetButtonTextToSelectedItem;

	/** Should the dropdown list be closed automatically when user clicks an item. */
	bool bAutoCloseWhenClicked;

	/** Size of the button content. */
	FVector2D ButtonSize;
};

class SFriendsAndChatComboImpl : public SFriendsAndChatCombo
{
public:
	virtual void Construct(const FArguments& InArgs)
	{
		SUserWidget::Construct(SUserWidget::FArguments()
			[
				SAssignNew(Anchor, SFriendsAndChatComboButton)
				.ButtonText(InArgs._ButtonText)
				.bShowIcon(InArgs._bShowIcon)
				.IconBrush(InArgs._IconBrush)
				.FriendStyle(InArgs._FriendStyle)
				.bSetButtonTextToSelectedItem(InArgs._bSetButtonTextToSelectedItem)
				.DropdownItems(InArgs._DropdownItems)
				.bAutoCloseWhenClicked(InArgs._bAutoCloseWhenClicked)
				.ButtonSize(InArgs._ButtonSize)
				.Placement(InArgs._Placement)
				.OnDropdownItemClicked(InArgs._OnDropdownItemClicked)
				.OnDropdownOpened(InArgs._OnDropdownOpened)
			]);
	}

	virtual bool IsOpen() const
	{
		return Anchor.IsValid() && Anchor->IsOpen();
	}

private:
	TSharedPtr<SMenuAnchor> Anchor;
};

TSharedRef<SFriendsAndChatCombo> SFriendsAndChatCombo::New()
{
	return MakeShareable(new SFriendsAndChatComboImpl());
}

#undef LOCTEXT_NAMESPACE
