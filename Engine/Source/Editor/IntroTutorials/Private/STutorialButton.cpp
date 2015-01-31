// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "IntroTutorialsPrivatePCH.h"
#include "STutorialButton.h"
#include "STutorialLoading.h"
#include "EditorTutorialSettings.h"
#include "TutorialStateSettings.h"
#include "TutorialMetaData.h"
#include "EngineBuildSettings.h"
#include "AssetRegistryModule.h"
#include "LevelEditor.h"
#include "SDockTab.h"

#define LOCTEXT_NAMESPACE "STutorialButton"

namespace TutorialButtonConstants
{
	const float MaxPulseOffset = 32.0f;
	const float PulseAnimationLength = 2.0f;
}

void STutorialButton::Construct(const FArguments& InArgs)
{
	Context = InArgs._Context;
	ContextWindow = InArgs._ContextWindow;

	bTestAlerts = FParse::Param(FCommandLine::Get(), TEXT("TestTutorialAlerts"));

	bPendingClickAction = false;
	bTutorialAvailable = false;
	bTutorialCompleted = false;
	bTutorialDismissed = false;
	bDeferTutorialOpen = true;
	AlertStartTime = 0.0f;

	PulseAnimation.AddCurve(0.0f, TutorialButtonConstants::PulseAnimationLength, ECurveEaseFunction::Linear);
	PulseAnimation.Play();

	IIntroTutorials& IntroTutorials = FModuleManager::LoadModuleChecked<IIntroTutorials>(TEXT("IntroTutorials"));
	LoadingWidget = IntroTutorials.CreateTutorialsLoadingWidget(ContextWindow);
	/*
	//LoadingWidget->SetVisibility(EVisibility::Visible);

	if (GEngine)
	{
		UGameViewportClient* GVC = GEngine->Viewport;
		if (GVC)
		{
			GVC->AddViewportWidgetContent(LoadingWidgetRef, 100);
		}
	}
	*/

	ChildSlot
	[
		SNew(SButton)
		.AddMetaData<FTagMetaData>(*FString::Printf(TEXT("%s.TutorialLaunchButton"), *Context.ToString()))
		.ButtonStyle(FEditorStyle::Get(), "TutorialLaunch.Button")
		.ToolTipText(this, &STutorialButton::GetButtonToolTip)
		.OnClicked(this, &STutorialButton::HandleButtonClicked)
		.ContentPadding(0.0f)
		[
			SNew(SBox)
			.WidthOverride(16)
			.HeightOverride(16)
		]
	];

	//ChildSlot.AttachWidget(LoadingWidgetRef);
}

void STutorialButton::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (bDeferTutorialOpen)
	{
		RefreshStatus();

		if (bTutorialAvailable && CachedAttractTutorial != nullptr && !bTutorialDismissed && !bTutorialCompleted)
		{
			// kick off the attract tutorial if the user hasn't dismissed it and hasn't completed it
			FIntroTutorials& IntroTutorials = FModuleManager::GetModuleChecked<FIntroTutorials>(TEXT("IntroTutorials"));
			const bool bRestart = true;
			IntroTutorials.LaunchTutorial(CachedAttractTutorial, bRestart, ContextWindow);
		}

		if(ShouldShowAlert())
		{
			AlertStartTime = FPlatformTime::Seconds();
		}

		if(CachedLaunchTutorial != nullptr)
		{
			TutorialTitle = CachedLaunchTutorial->Title;
		}
	}
	bDeferTutorialOpen = false;

	//The user has clicked to the button, but if the asset registry isn't done loading, we don't know whether to open the browser or do a tutorial immediately.
	if (bPendingClickAction)
	{
		bPendingClickAction = HandleButtonClicked_AssetRegistryChecker();
	}
}

static void GetAnimationValues(float InAnimationProgress, float& OutAlphaFactor0, float& OutPulseFactor0, float& OutAlphaFactor1, float& OutPulseFactor1)
{
	InAnimationProgress = FMath::Fmod(InAnimationProgress * 2.0f, 1.0f);

	OutAlphaFactor0 = FMath::Square(1.0f - InAnimationProgress);
	OutPulseFactor0 = 1.0f - FMath::Square(1.0f - InAnimationProgress);

	float OffsetProgress = FMath::Fmod(InAnimationProgress + 0.25f, 1.0f);
	OutAlphaFactor1 = FMath::Square(1.0f - OffsetProgress);
	OutPulseFactor1 = 1.0f - FMath::Square(1.0f - OffsetProgress);
}

int32 STutorialButton::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled) + 1000;

	if (ShouldShowAlert())
	{
		float AlphaFactor0 = 0.0f;
		float AlphaFactor1 = 0.0f;
		float PulseFactor0 = 0.0f;
		float PulseFactor1 = 0.0f;
		GetAnimationValues(PulseAnimation.GetLerpLooping(), AlphaFactor0, PulseFactor0, AlphaFactor1, PulseFactor1);

		const FSlateBrush* PulseBrush = FEditorStyle::Get().GetBrush(TEXT("TutorialLaunch.Circle"));
		const FLinearColor PulseColor = FEditorStyle::Get().GetColor(TEXT("TutorialLaunch.Circle.Color"));

		// We should be clipped by the window size, not our containing widget, as we want to draw outside the widget
		const FVector2D WindowSize = OutDrawElements.GetWindow()->GetSizeInScreen();
		FSlateRect WindowClippingRect(0.0f, 0.0f, WindowSize.X, WindowSize.Y);

		{
			FVector2D PulseOffset = FVector2D(PulseFactor0 * TutorialButtonConstants::MaxPulseOffset, PulseFactor0 * TutorialButtonConstants::MaxPulseOffset);

			FVector2D BorderPosition = (AllottedGeometry.AbsolutePosition - ((FVector2D(PulseBrush->Margin.Left, PulseBrush->Margin.Top) * PulseBrush->ImageSize * AllottedGeometry.Scale) + PulseOffset));
			FVector2D BorderSize = ((AllottedGeometry.Size * AllottedGeometry.Scale) + (PulseOffset * 2.0f) + (FVector2D(PulseBrush->Margin.Right * 2.0f, PulseBrush->Margin.Bottom * 2.0f) * PulseBrush->ImageSize * AllottedGeometry.Scale));

			FPaintGeometry BorderGeometry(BorderPosition, BorderSize, AllottedGeometry.Scale);

			// draw highlight border
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId++, BorderGeometry, PulseBrush, WindowClippingRect, ESlateDrawEffect::None, FLinearColor(PulseColor.R, PulseColor.G, PulseColor.B, AlphaFactor0));
		}

		{
			FVector2D PulseOffset = FVector2D(PulseFactor1 * TutorialButtonConstants::MaxPulseOffset, PulseFactor1 * TutorialButtonConstants::MaxPulseOffset);

			FVector2D BorderPosition = (AllottedGeometry.AbsolutePosition - ((FVector2D(PulseBrush->Margin.Left, PulseBrush->Margin.Top) * PulseBrush->ImageSize * AllottedGeometry.Scale) + PulseOffset));
			FVector2D BorderSize = ((AllottedGeometry.Size * AllottedGeometry.Scale) + (PulseOffset * 2.0f) + (FVector2D(PulseBrush->Margin.Right * 2.0f, PulseBrush->Margin.Bottom * 2.0f) * PulseBrush->ImageSize * AllottedGeometry.Scale));

			FPaintGeometry BorderGeometry(BorderPosition, BorderSize, AllottedGeometry.Scale);

			// draw highlight border
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId++, BorderGeometry, PulseBrush, WindowClippingRect, ESlateDrawEffect::None, FLinearColor(PulseColor.R, PulseColor.G, PulseColor.B, AlphaFactor1));
		}
	}

	return LayerId;
}

FReply STutorialButton::HandleButtonClicked()
{
	if (bPendingClickAction)
	{
		//There's already a click pending
		return FReply::Handled();
	}

	RefreshStatus();

	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> EventAttributes;
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Context"), Context.ToString()));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("TimeSinceAlertStarted"), (AlertStartTime != 0.0f && ShouldShowAlert()) ? (FPlatformTime::Seconds() - AlertStartTime) : -1.0f));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("LaunchedBrowser"), ShouldLaunchBrowser()));

		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Rocket.Tutorials.ClickedContextButton"), EventAttributes);
	}

	FIntroTutorials& IntroTutorials = FModuleManager::GetModuleChecked<FIntroTutorials>(TEXT("IntroTutorials"));
	IntroTutorials.AttachWidget(LoadingWidget);
	bPendingClickAction = HandleButtonClicked_AssetRegistryChecker();
	return FReply::Handled();
}

bool STutorialButton::HandleButtonClicked_AssetRegistryChecker()
{
	//Force tutorials to load into the asset registry before we proceed any further.
	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	bool IsStillLoading = AssetRegistry.Get().IsLoadingAssets();
	if (IsStillLoading)
	{
		return true;		//Keep doing this on tick
	}

	//Sometimes, this gives a false positive because the tutorial we want to launch wasn't loaded into the asset registry when we checked. Opening and closing the tab works around that by letting the browser recheck.
	if (ShouldLaunchBrowser())
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		LevelEditorModule.GetLevelEditorTabManager()->InvokeTab(FTabId("TutorialsBrowser"))->RequestCloseTab();
		RefreshStatus();
	}

	//Now we know the asset registry is loaded, the tutorial broswer is updated, and we are ready to complete the click and stop this active timer
	FIntroTutorials& IntroTutorials = FModuleManager::GetModuleChecked<FIntroTutorials>(TEXT("IntroTutorials"));
	IntroTutorials.DetachWidget();
	if (ShouldLaunchBrowser())
	{
		IntroTutorials.SummonTutorialBrowser();
	}
	else if (CachedLaunchTutorial != nullptr)
	{
		//If we don't want to launch the browser, and we have a tutorial in mind, launch the tutorial now.
		auto Delegate = FSimpleDelegate::CreateSP(this, &STutorialButton::HandleTutorialExited);

		const bool bRestart = true;
		IntroTutorials.LaunchTutorial(CachedLaunchTutorial, bRestart, ContextWindow, Delegate, Delegate);

		const bool bDismissAcrossSessions = true;
		GetMutableDefault<UTutorialStateSettings>()->DismissTutorial(CachedLaunchTutorial, bDismissAcrossSessions);
		GetMutableDefault<UTutorialStateSettings>()->SaveProgress();
		bTutorialDismissed = true;
	}
	return false;		//Stop doing this
}

FReply STutorialButton::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		const bool bInShouldCloseWindowAfterMenuSelection = true;
		FMenuBuilder MenuBuilder(bInShouldCloseWindowAfterMenuSelection, nullptr);

		if(ShouldShowAlert())
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("DismissReminder", "Dismiss Alert"),
				LOCTEXT("DismissReminderTooltip", "Don't show me this alert again"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &STutorialButton::DismissAlert))
				);
		}

		if(bTutorialAvailable)
		{
			MenuBuilder.AddMenuEntry(
				FText::Format(LOCTEXT("LaunchTutorialPattern", "Start Tutorial: {0}"), TutorialTitle),
				FText::Format(LOCTEXT("TutorialLaunchToolTip", "Click to begin the '{0}' tutorial"), TutorialTitle),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &STutorialButton::LaunchTutorial))
				);
		}

		MenuBuilder.AddMenuEntry(
			LOCTEXT("LaunchBrowser", "Show Available Tutorials"),
			LOCTEXT("LaunchBrowserTooltip", "Display the tutorials browser"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &STutorialButton::LaunchBrowser))
			);


		FSlateApplication::Get().PushMenu(SharedThis(this), MenuBuilder.MakeWidget(), FSlateApplication::Get().GetCursorPos(), FPopupTransitionEffect::ContextMenu);
	}
	return FReply::Handled();
}

void STutorialButton::DismissAlert()
{
	RefreshStatus();

	if( FEngineAnalytics::IsAvailable() )
	{
		TArray<FAnalyticsEventAttribute> EventAttributes;
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Context"), Context.ToString()));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("TimeSinceAlertStarted"), (AlertStartTime != 0.0f && ShouldShowAlert()) ? (FPlatformTime::Seconds() - AlertStartTime) : -1.0f));

		FEngineAnalytics::GetProvider().RecordEvent( TEXT("Rocket.Tutorials.DismissedTutorialAlert"), EventAttributes );
	}

	const bool bDismissAcrossSessions = true;
	if (CachedAttractTutorial != nullptr)
	{
		GetMutableDefault<UTutorialStateSettings>()->DismissTutorial(CachedAttractTutorial, bDismissAcrossSessions);
	}
	if( CachedLaunchTutorial != nullptr)
	{
		GetMutableDefault<UTutorialStateSettings>()->DismissTutorial(CachedLaunchTutorial, bDismissAcrossSessions);
	}
	GetMutableDefault<UTutorialStateSettings>()->SaveProgress();
	bTutorialDismissed = true;

	FIntroTutorials& IntroTutorials = FModuleManager::GetModuleChecked<FIntroTutorials>(TEXT("IntroTutorials"));
	IntroTutorials.CloseAllTutorialContent();
}

void STutorialButton::LaunchTutorial()
{
	HandleButtonClicked();
}

void STutorialButton::LaunchBrowser()
{
	RefreshStatus();

	FIntroTutorials& IntroTutorials = FModuleManager::GetModuleChecked<FIntroTutorials>(TEXT("IntroTutorials"));
	IntroTutorials.SummonTutorialBrowser();
}

bool STutorialButton::ShouldLaunchBrowser() const
{
	return (!bTutorialAvailable || (bTutorialAvailable && bTutorialCompleted));
}

bool STutorialButton::ShouldShowAlert() const
{
	return ((bTestAlerts || !FEngineBuildSettings::IsInternalBuild()) && bTutorialAvailable && !(bTutorialCompleted || bTutorialDismissed));
}

FText STutorialButton::GetButtonToolTip() const
{
	if(ShouldLaunchBrowser())
	{
		return LOCTEXT("TutorialLaunchBrowserToolTip", "Show Available Tutorials");
	}
	else if(bTutorialAvailable)
	{
		return FText::Format(LOCTEXT("TutorialLaunchToolTipPattern", "Click to begin the '{0}' tutorial, or right click for more options"), TutorialTitle);
	}
	
	return LOCTEXT("TutorialToolTip", "Take Tutorial");
}

void STutorialButton::RefreshStatus()
{
	CachedAttractTutorial = nullptr;
	CachedLaunchTutorial = nullptr;
	CachedBrowserFilter = TEXT("");
	GetDefault<UEditorTutorialSettings>()->FindTutorialInfoForContext(Context, CachedAttractTutorial, CachedLaunchTutorial, CachedBrowserFilter);

	bTutorialAvailable = (CachedLaunchTutorial != nullptr);
	bTutorialCompleted = (CachedLaunchTutorial != nullptr) && GetDefault<UTutorialStateSettings>()->HaveCompletedTutorial(CachedLaunchTutorial);
	bTutorialDismissed = ((CachedAttractTutorial != nullptr) && GetDefault<UTutorialStateSettings>()->IsTutorialDismissed(CachedAttractTutorial)) ||
						 ((CachedLaunchTutorial != nullptr) && GetDefault<UTutorialStateSettings>()->IsTutorialDismissed(CachedLaunchTutorial));
}

void STutorialButton::HandleTutorialExited()
{
	RefreshStatus();
}

#undef LOCTEXT_NAMESPACE