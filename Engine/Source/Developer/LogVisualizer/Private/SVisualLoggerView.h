// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SVisualLoggerView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVisualLoggerView) 
		: _ViewRange(TRange<float>(0.0f, 5.0f))
		, _ScrubPosition(1.0f)
	{}
	/** The current view range (seconds) */
	SLATE_ATTRIBUTE(TRange<float>, ViewRange)
		/** The current scrub position in (seconds) */
		SLATE_ATTRIBUTE(float, ScrubPosition)
		SLATE_EVENT(FOnFiltersSearchChanged, OnFiltersSearchChanged)
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, const TSharedRef<FUICommandList>& InCommandList, TSharedPtr<IVisualLoggerInterface> VisualLoggerInterface);
	float GetAnimationOutlinerFillPercentage() const { 
		SSplitter::FSlot const& LeftSplitterSlot = SearchSplitter->SlotAt(0);
		SSplitter::FSlot const& RightSplitterSlot = SearchSplitter->SlotAt(1);

		return LeftSplitterSlot.SizeValue.Get()/ RightSplitterSlot.SizeValue.Get();
	}
	void SetAnimationOutlinerFillPercentage(float FillPercentage);

	TSharedRef<SWidget> MakeSectionOverlay(TSharedRef<class FVisualLoggerTimeSliderController> TimeSliderController, const TAttribute< TRange<float> >& ViewRange, const TAttribute<float>& ScrubPosition, bool bTopOverlay);
	void SetSearchString(FText SearchString);

	void OnNewLogEntry(const FVisualLogDevice::FVisualLogEntryItem& Entry);
	void OnFiltersChanged();
	void OnSearchChanged(const FText& Filter);
	void OnFiltersSearchChanged(const FText& Filter);
	void OnSearchSplitterResized();
	void OnObjectSelectionChanged(TSharedPtr<class STimeline> TimeLine);

	void GetTimelines(TArray<TSharedPtr<class STimeline> >&, bool bOnlySelectedOnes = false);
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

protected:
	TSharedPtr<struct IVisualLoggerInterface> VisualLoggerInterface;
	TSharedPtr<class STimelinesContainer> TimelinesContainer;
	TSharedPtr<class SSplitter> SearchSplitter;
	TSharedPtr<class SScrollBox> ScrollBox;
	TSharedPtr<class SSearchBox> SearchBox;

	FVisualLoggerEvents	VisualLoggerEvents;
	float AnimationOutlinerFillPercentage;
};
