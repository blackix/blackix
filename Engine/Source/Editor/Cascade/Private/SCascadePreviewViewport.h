// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SEditorViewport.h"

class FCascade;
class SCascadePreviewViewport;


/*-----------------------------------------------------------------------------
   SCascadeViewport
-----------------------------------------------------------------------------*/

class SCascadePreviewViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SCascadePreviewViewport)
		{}

		SLATE_ARGUMENT(TWeakPtr<FCascade>, Cascade)
	SLATE_END_ARGS()

	/** Destructor */
	~SCascadePreviewViewport();

	/** SCompoundWidget interface */
	void Construct(const FArguments& InArgs);

	/** Refreshes the viewport */
	void RefreshViewport();

	/** Returns true if the viewport is visible */
	bool IsVisible() const;
	EVisibility GetWidgetVisibility() const;

	/** Accessors */
	TSharedPtr<FSceneViewport> GetViewport() const;
	TSharedPtr<FCascadeEdPreviewViewportClient> GetViewportClient() const;
	TSharedPtr<SViewport> GetViewportWidget() const;
	
protected:
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() OVERRIDE;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() OVERRIDE;
	virtual void OnFocusViewportToSelection() OVERRIDE;
public:
	/** The parent tab where this viewport resides */
	TWeakPtr<SDockTab> ParentTab;

private:
	/** Pointer back to the ParticleSystem editor tool that owns us */
	TWeakPtr<FCascade> CascadePtr;
	
	/** Level viewport client */
	TSharedPtr<FCascadeEdPreviewViewportClient> ViewportClient;
};
