// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "HeadMountedDisplayPrivate.h"
#include "Layout/SlateRect.h"

class FHeadMountedDisplayModule : public IHeadMountedDisplayModule
{
	virtual TSharedPtr< class IHeadMountedDisplay > CreateHeadMountedDisplay()
	{
		TSharedPtr<IHeadMountedDisplay> DummyVal = NULL;
		return DummyVal;
	}
};

IMPLEMENT_MODULE( FHeadMountedDisplayModule, HeadMountedDisplay );

IHeadMountedDisplay::IHeadMountedDisplay()
{
	PreFullScreenRect.Left = PreFullScreenRect.Right = PreFullScreenRect.Top = PreFullScreenRect.Bottom = -1.f;
}

void IHeadMountedDisplay::PushPreFullScreenRect(const FSlateRect& InPreFullScreenRect)
{
	PreFullScreenRect.Left	= InPreFullScreenRect.Left;
	PreFullScreenRect.Top	= InPreFullScreenRect.Top;
	PreFullScreenRect.Right	= InPreFullScreenRect.Right;
	PreFullScreenRect.Bottom = InPreFullScreenRect.Bottom;
}

void IHeadMountedDisplay::PopPreFullScreenRect(FSlateRect& OutPreFullScreenRect)
{
	OutPreFullScreenRect = FSlateRect(PreFullScreenRect.Left, PreFullScreenRect.Top, PreFullScreenRect.Right, PreFullScreenRect.Bottom);
	PreFullScreenRect.Left = PreFullScreenRect.Right = PreFullScreenRect.Top = PreFullScreenRect.Bottom = -1.f;
}