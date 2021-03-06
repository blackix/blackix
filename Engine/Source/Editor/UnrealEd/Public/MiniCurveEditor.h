// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#ifndef __MiniCurveEditor_h__
#define __MiniCurveEditor_h__
#include "Toolkits/AssetEditorManager.h"

class UNREALED_API SMiniCurveEditor :  public SCompoundWidget,public IAssetEditorInstance
{
public:
	SLATE_BEGIN_ARGS( SMiniCurveEditor )
		: _CurveOwner(NULL)
		{}

	SLATE_ARGUMENT( FCurveOwnerInterface*, CurveOwner )
	SLATE_ARGUMENT( TWeakPtr<SWindow>, ParentWindow )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SMiniCurveEditor();

	// IAssetEditorInstance interface
	virtual FName GetEditorName() const OVERRIDE;
	virtual void FocusWindow(UObject* ObjectToFocusOn) OVERRIDE;
	virtual bool CloseWindow() OVERRIDE;
private:

	float ViewMinInput;
	float ViewMaxInput;

	TSharedPtr<class SCurveEditor> TrackWidget;

	float GetViewMinInput() const { return ViewMinInput; }
	float GetViewMaxInput() const { return ViewMaxInput; }
	/** Return length of timeline */
	float GetTimelineLength() const;

	void SetInputViewRange(float InViewMinInput, float InViewMaxInput);


protected:
	TWeakPtr<SWindow> WidgetWindow;
};

#endif // MiniCurveEditor