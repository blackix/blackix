// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once

//////////////////////////////////////////////////////////////////////////
// SMatineeRecorder

class MATINEE_API SMatineeRecorder : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SMatineeRecorder ) : 
	  _MatineeWindow( NULL )
	{
	}

	SLATE_ARGUMENT( TWeakPtr<IMatineeBase>, MatineeWindow )

	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);
	virtual ~SMatineeRecorder();

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) OVERRIDE;
	void RefreshViewport();

	FLevelEditorViewportClient* GetViewport() const { return LevelViewportClient.Get(); }

private:
	bool IsVisible() const;

	/** Gets the current image that should be displayed for the Record/Stop button based on the status of the InterpEditor. */
	const FSlateBrush* GetRecordImageDelegate() const;

	/** When selecting an item in the drop down list it will call this function to relay the information to InterpEditor. */
	void SelectCameraMode( TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo );

	/** Function call when pressing the Record/Stop button. Will toggle the InterpEditor to record/stop. */
	FReply ToggleRecord();

private:
	/** Instance of the InterpEditor that this instance is using. */
	TWeakPtr<IMatineeBase> ParentMatineeWindow;

	/** Starts and stops recording */
	TSharedPtr<SButton> RecordButton;

	/** The camera modes drop down list. */
	TSharedPtr< STextComboBox > CameraModeComboBox;

	/** Level viewport client */
	TSharedPtr<FLevelEditorViewportClient> LevelViewportClient;

	/** The options for the drop down list. */
	TArray< TSharedPtr<FString> > CameraModeOptions;

	/** Slate viewport for rendering and I/O */
	TSharedPtr<class FSceneViewport> Viewport;

	/** Viewport widget*/
	TSharedPtr<SViewport> ViewportWidget;
};
