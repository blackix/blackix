// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "Core.h"

#include "MacWindow.h"
#include "MacTextInputMethodSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogMacTextInputMethodSystem, Log, All);

@implementation FSlateTextView

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self)
	{
        // Initialization code here.
		IMMContext = nil;
		markedRange = {NSNotFound, 0};
		reallyHandledEvent = false;
    }
    return self;
}

- (void)drawRect:(NSRect)dirtyRect
{
	[super drawRect:dirtyRect];
	
    // Drawing code here.
}

- (bool)imkKeyDown:(NSEvent *)theEvent
{
	if (IMMContext.IsValid())
	{
		reallyHandledEvent = true;
		return [[self inputContext] handleEvent:theEvent] && reallyHandledEvent;
	}
	else
	{
		return false;
	}
}

/** Forward mouse events up to the window rather than through the responder chain - thus avoiding
 *	the hidden titlebar controls. Normal windows just use the responder chain as usual.
 */
- (BOOL)acceptsFirstMouse:(NSEvent *)Event
{
	return YES;
}

- (void)mouseDown:(NSEvent *)theEvent
{
	if (IMMContext.IsValid())
	{
		[[self inputContext] handleEvent:theEvent];
	}
	
	FSlateCocoaWindow* SlateCocoaWindow = [[self window] isKindOfClass:[FSlateCocoaWindow class]] ? (FSlateCocoaWindow*)[self window] : nil;
	if (SlateCocoaWindow)
	{
		[SlateCocoaWindow mouseDown:theEvent];
	}
}

- (void)mouseDragged:(NSEvent *)theEvent
{
	if (IMMContext.IsValid())
	{
		[[self inputContext] handleEvent:theEvent];
	}
}

- (void)mouseUp:(NSEvent *)theEvent
{
	if (IMMContext.IsValid())
	{
		[[self inputContext] handleEvent:theEvent];
	}
	
	FSlateCocoaWindow* SlateCocoaWindow = [[self window] isKindOfClass:[FSlateCocoaWindow class]] ? (FSlateCocoaWindow*)[self window] : nil;
	if (SlateCocoaWindow)
	{
		[SlateCocoaWindow mouseUp:theEvent];
	}
}

- (void)rightMouseDown:(NSEvent*)Event
{
	FSlateCocoaWindow* SlateCocoaWindow = [[self window] isKindOfClass:[FSlateCocoaWindow class]] ? (FSlateCocoaWindow*)[self window] : nil;
	if (SlateCocoaWindow)
	{
		[SlateCocoaWindow rightMouseDown:Event];
	}
	else
	{
		[super rightMouseDown:Event];
	}
}

- (void)otherMouseDown:(NSEvent*)Event
{
	FSlateCocoaWindow* SlateCocoaWindow = [[self window] isKindOfClass:[FSlateCocoaWindow class]] ? (FSlateCocoaWindow*)[self window] : nil;
	if (SlateCocoaWindow)
	{
		[SlateCocoaWindow otherMouseDown:Event];
	}
	else
	{
		[super otherMouseDown:Event];
	}
}

- (void)rightMouseUp:(NSEvent*)Event
{
	FSlateCocoaWindow* SlateCocoaWindow = [[self window] isKindOfClass:[FSlateCocoaWindow class]] ? (FSlateCocoaWindow*)[self window] : nil;
	if (SlateCocoaWindow)
	{
		[SlateCocoaWindow rightMouseUp:Event];
	}
	else
	{
		[super rightMouseUp:Event];
	}
}

- (void)otherMouseUp:(NSEvent*)Event
{
	FSlateCocoaWindow* SlateCocoaWindow = [[self window] isKindOfClass:[FSlateCocoaWindow class]] ? (FSlateCocoaWindow*)[self window] : nil;
	if (SlateCocoaWindow)
	{
		[SlateCocoaWindow otherMouseUp:Event];
	}
	else
	{
		[super otherMouseUp:Event];
	}
}

- (void)activateInputMethod:(const TSharedRef<ITextInputMethodContext>&)InContext
{
	if (IMMContext.IsValid())
	{
		[self unmarkText];
		[[self inputContext] deactivate];
		[[self inputContext] discardMarkedText];
	}
	IMMContext = InContext;
	[[self inputContext] activate];
}

- (void)deactivateInputMethod
{
	[self unmarkText];
	IMMContext = NULL;
	[[self inputContext] deactivate];
	[[self inputContext] discardMarkedText];
}

//@protocol NSTextInputClient
//@required
/* The receiver inserts aString replacing the content specified by replacementRange. aString can be either an NSString or NSAttributedString instance.
 */
- (void)insertText:(id)aString replacementRange:(NSRange)replacementRange
{
	if (IMMContext.IsValid() && [self hasMarkedText])
	{
		uint32 SelectionLocation, SelectionLength;
		if (replacementRange.location == NSNotFound)
		{
			if (markedRange.location != NSNotFound)
			{
				SelectionLocation = markedRange.location;
				SelectionLength = markedRange.length;
			}
			else
			{
				ITextInputMethodContext::ECaretPosition CaretPosition;
				IMMContext->GetSelectionRange(SelectionLocation, SelectionLength, CaretPosition);
			}
		}
		else
		{
			SelectionLocation = replacementRange.location;
			SelectionLength = replacementRange.length;
		}
		
		NSString* TheString;
		if ([aString isKindOfClass:[NSAttributedString class]])
		{
			TheString = [(NSAttributedString*)aString string];
		}
		else
		{
			TheString = (NSString*)aString;
		}
		
		FString TheFString(TheString);
		IMMContext->SetTextInRange(SelectionLocation, SelectionLength, TheFString);
		IMMContext->SetSelectionRange(SelectionLocation+TheFString.Len(), 0, ITextInputMethodContext::ECaretPosition::Ending);
		
		[self unmarkText];
		[[self inputContext] invalidateCharacterCoordinates]; // recentering
	}
	else
	{
		reallyHandledEvent = false;
	}
}

/* The receiver invokes the action specified by aSelector.
 */
- (void)doCommandBySelector:(SEL)aSelector
{
	reallyHandledEvent = false;
}

/* The receiver inserts aString replacing the content specified by replacementRange. aString can be either an NSString or NSAttributedString instance. selectedRange specifies the selection inside the string being inserted; hence, the location is relative to the beginning of aString. When aString is an NSString, the receiver is expected to render the marked text with distinguishing appearance (i.e. NSTextView renders with -markedTextAttributes).
 */
- (void)setMarkedText:(id)aString selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange
{
	if (IMMContext.IsValid())
	{
		uint32 SelectionLocation, SelectionLength;
		if (replacementRange.location == NSNotFound)
		{
			if (markedRange.location != NSNotFound)
			{
				SelectionLocation = markedRange.location;
				SelectionLength = markedRange.length;
			}
			else
			{
				ITextInputMethodContext::ECaretPosition CaretPosition;
				IMMContext->GetSelectionRange(SelectionLocation, SelectionLength, CaretPosition);
			}
		}
		else
		{
			SelectionLocation = replacementRange.location;
			SelectionLength = replacementRange.length;
		}
		
		if ([aString length] == 0)
		{
			IMMContext->SetTextInRange(SelectionLocation, SelectionLength, FString());
			[self unmarkText];
		}
		else
		{
			if(markedRange.location == NSNotFound)
			{
				IMMContext->BeginComposition();
			}
			markedRange = NSMakeRange(SelectionLocation, [aString length]);
			
			__block NSRange CompositionRange = markedRange;
			
			NSString* TheString;
			if ([aString isKindOfClass:[NSAttributedString class]])
			{
				// While the whole string is being composed NSUnderlineStyleAttributeName is 1 to show a single line below the whole string.
				// When using the pop-up glyph selection window in some IME's the NSAttributedString is broken up into separate glyph ranges, each with its own set of attributes.
				// Each range specifies NSMarkedClauseSegment, incrementing the NSNumber value from 0 as well as NSUnderlineStyleAttributeName, which makes the underlining show the different ranges.
				// The subrange being edited by the pop-up glyph selection window will set NSUnderlineStyleAttributeName to a value >1, while all other ranges will be set NSUnderlineStyleAttributeName to 1.
				NSAttributedString* AttributedString = (NSAttributedString*)aString;
				[AttributedString enumerateAttribute:NSUnderlineStyleAttributeName inRange:NSMakeRange(0, [aString length]) options:0 usingBlock:^(id Value, NSRange Range, BOOL* bStop) {
					if(Value && [Value isKindOfClass:[NSNumber class]])
					{
						NSNumber* NumberValue = (NSNumber*)Value;
						const int UnderlineValue = [NumberValue intValue];
						if(UnderlineValue > 1)
						{
							// Found the active range, stop enumeration.
							*bStop = YES;
							CompositionRange.location += Range.location;
							CompositionRange.length = Range.length;
						}
					}
				}];
				
				TheString = [AttributedString string];
			}
			else
			{
				TheString = (NSString*)aString;
			}
			
			FString TheFString(TheString);
			IMMContext->SetTextInRange(SelectionLocation, SelectionLength, TheFString);
			IMMContext->UpdateCompositionRange(CompositionRange.location, CompositionRange.length);
			IMMContext->SetSelectionRange(markedRange.location + selectedRange.location, 0, ITextInputMethodContext::ECaretPosition::Ending);
		}
		[[self inputContext] invalidateCharacterCoordinates]; // recentering
	}
	else
	{
		reallyHandledEvent = false;
	}
}

/* The receiver unmarks the marked text. If no marked text, the invocation of this method has no effect.
 */
- (void)unmarkText
{
	if(markedRange.location != NSNotFound)
	{
		markedRange = {NSNotFound, 0};
		if (IMMContext.IsValid())
		{
			IMMContext->UpdateCompositionRange(0, 0);
			IMMContext->EndComposition();
		}
	}
}

/* Returns the selection range. The valid location is from 0 to the document length.
 */
- (NSRange)selectedRange
{
	if (IMMContext.IsValid())
	{
		uint32 SelectionLocation, SelectionLength;
		ITextInputMethodContext::ECaretPosition CaretPosition;
		IMMContext->GetSelectionRange(SelectionLocation, SelectionLength, CaretPosition);
		
		return {SelectionLocation, SelectionLength};
	}
	else
	{
		return {NSNotFound, 0};
	}
}

/* Returns the marked range. Returns {NSNotFound, 0} if no marked range.
 */
- (NSRange)markedRange
{
	if (IMMContext.IsValid())
	{
		return markedRange;
	}
	else
	{
		return {NSNotFound, 0};
	}
}

/* Returns whether or not the receiver has marked text.
 */
- (BOOL)hasMarkedText
{
	return IMMContext.IsValid() && (markedRange.location != NSNotFound);
}

/* Returns attributed string specified by aRange. It may return nil. If non-nil return value and actualRange is non-NULL, it contains the actual range for the return value. The range can be adjusted from various reasons (i.e. adjust to grapheme cluster boundary, performance optimization, etc).
 */
- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)aRange actualRange:(NSRangePointer)actualRange
{
	NSAttributedString* AttributedString = nil;
	if (IMMContext.IsValid())
	{
		FString String;
		IMMContext->GetTextInRange(aRange.location, aRange.length, String);
		CFStringRef CFString = FPlatformString::TCHARToCFString(*String);
		if(CFString)
		{
			AttributedString = [[[NSAttributedString alloc] initWithString:(NSString*)CFString] autorelease];
			CFRelease(CFString);
			if(actualRange)
			{
				*actualRange = aRange;
			}
		}
	}
	return AttributedString;
}

/* Returns an array of attribute names recognized by the receiver.
 */
- (NSArray*)validAttributesForMarkedText
{
    // We only allow these attributes to be set on our marked text (plus standard attributes)
    // NSMarkedClauseSegmentAttributeName is important for CJK input, among other uses
    // NSGlyphInfoAttributeName allows alternate forms of characters
    return [NSArray arrayWithObjects:NSMarkedClauseSegmentAttributeName, NSGlyphInfoAttributeName, nil];
}

/* Returns the first logical rectangular area for aRange. The return value is in the screen coordinate. The size value can be negative if the text flows to the left. If non-NULL, actuallRange contains the character range corresponding to the returned area.
 */
- (NSRect)firstRectForCharacterRange:(NSRange)aRange actualRange:(NSRangePointer)actualRange
{
	if (IMMContext.IsValid())
	{
		FVector2D Position, Size;
		IMMContext->GetTextBounds(aRange.location, aRange.length, Position, Size);
		
		if(actualRange)
		{
			*actualRange = aRange;
		}
		
		NSScreen* PrimaryScreen = [[self window] screen];
		const float PrimaryScreenHeight = [PrimaryScreen visibleFrame].size.height;
		Position.Y = -(Position.Y - PrimaryScreenHeight + 1);
		
		NSRect GlyphBox = {Position.X,Position.Y,Size.X,Size.Y};
		return GlyphBox;
	}
	else
	{
		NSRect GlyphBox = {0,0,0,0};
		return GlyphBox;
	}
}

/* Returns the index for character that is nearest to aPoint. aPoint is in the screen coordinate system.
 */
- (NSUInteger)characterIndexForPoint:(NSPoint)aPoint
{
	if (IMMContext.IsValid())
	{
		FVector2D Pos(aPoint.x, aPoint.y);
		int32 Index = IMMContext->GetCharacterIndexFromPoint(Pos);
		NSUInteger Idx = Index == INDEX_NONE ? NSNotFound : (NSUInteger)Index;
		return Idx;
	}
	else
	{
		return NSNotFound;
	}
}

//@optional

/* Returns the window level of the receiver. An NSTextInputClient can implement this interface to specify its window level if it is higher than NSFloatingWindowLevel.
 */
- (NSInteger)windowLevel
{
	return [[self window] level];
}

@end

namespace
{
	class FTextInputMethodChangeNotifier : public ITextInputMethodChangeNotifier
	{
	public:
		FTextInputMethodChangeNotifier(const TSharedRef<ITextInputMethodContext>& InContext)
		:	Context(InContext)
		{
			ContextWindow = Context.Pin()->GetWindow();
		}
		
		virtual ~FTextInputMethodChangeNotifier() {}
		
		void SetContextWindow(TSharedPtr<FGenericWindow> Window);
		TSharedPtr<FGenericWindow> GetContextWindow(void) const;
		
		virtual void NotifyLayoutChanged(const ELayoutChangeType ChangeType) override;
		virtual void NotifySelectionChanged() override;
		virtual void NotifyTextChanged(const uint32 BeginIndex, const uint32 OldLength, const uint32 NewLength) override;
		virtual void CancelComposition() override;
		
	private:
		TWeakPtr<ITextInputMethodContext> Context;
		TSharedPtr<FGenericWindow> ContextWindow;
	};
	
	void FTextInputMethodChangeNotifier::SetContextWindow(TSharedPtr<FGenericWindow> Window)
	{
		ContextWindow = Window;
	}
	
	TSharedPtr<FGenericWindow> FTextInputMethodChangeNotifier::GetContextWindow(void) const
	{
		return ContextWindow;
	}
	
	void FTextInputMethodChangeNotifier::NotifyLayoutChanged(const ELayoutChangeType ChangeType)
	{
		if(ContextWindow.IsValid())
		{
			FSlateCocoaWindow* CocoaWindow = (FSlateCocoaWindow*)ContextWindow->GetOSWindowHandle();
			if(CocoaWindow && [CocoaWindow openGLView])
			{
				FSlateTextView* TextView = (FSlateTextView*)[CocoaWindow openGLView];
				[[TextView inputContext] invalidateCharacterCoordinates];
			}
		}
	}
	
	void FTextInputMethodChangeNotifier::NotifySelectionChanged()
	{
		if(ContextWindow.IsValid())
		{
			FSlateCocoaWindow* CocoaWindow = (FSlateCocoaWindow*)ContextWindow->GetOSWindowHandle();
			if(CocoaWindow && [CocoaWindow openGLView])
			{
				FSlateTextView* TextView = (FSlateTextView*)[CocoaWindow openGLView];
				[[TextView inputContext] invalidateCharacterCoordinates];
			}
		}
	}
	
	void FTextInputMethodChangeNotifier::NotifyTextChanged(const uint32 BeginIndex, const uint32 OldLength, const uint32 NewLength)
	{
		if(ContextWindow.IsValid())
		{
			FSlateCocoaWindow* CocoaWindow = (FSlateCocoaWindow*)ContextWindow->GetOSWindowHandle();
			if(CocoaWindow && [CocoaWindow openGLView])
			{
				FSlateTextView* TextView = (FSlateTextView*)[CocoaWindow openGLView];
				[[TextView inputContext] invalidateCharacterCoordinates];
			}
		}
	}
	
	void FTextInputMethodChangeNotifier::CancelComposition()
	{
		if(ContextWindow.IsValid())
		{
			FSlateCocoaWindow* CocoaWindow = (FSlateCocoaWindow*)ContextWindow->GetOSWindowHandle();
			if(CocoaWindow && [CocoaWindow openGLView])
			{
				FSlateTextView* TextView = (FSlateTextView*)[CocoaWindow openGLView];
				[[TextView inputContext] discardMarkedText];
				[TextView unmarkText];
			}
		}
	}
}

bool FMacTextInputMethodSystem::Initialize()
{
	// Nothing to do here for Cocoa
	return true;
}

void FMacTextInputMethodSystem::Terminate()
{
	// Nothing to do here for Cocoa
}

// ITextInputMethodSystem Interface Begin
TSharedPtr<ITextInputMethodChangeNotifier> FMacTextInputMethodSystem::RegisterContext(const TSharedRef<ITextInputMethodContext>& Context)
{
	TSharedRef<ITextInputMethodChangeNotifier> Notifier = MakeShareable( new FTextInputMethodChangeNotifier(Context) );
	TWeakPtr<ITextInputMethodChangeNotifier>& NotifierRef = ContextMap.Add(Context);
	NotifierRef = Notifier;
	return Notifier;
}

void FMacTextInputMethodSystem::UnregisterContext(const TSharedRef<ITextInputMethodContext>& Context)
{
	TWeakPtr<ITextInputMethodChangeNotifier>& NotifierRef = ContextMap[Context];
	if(!NotifierRef.IsValid())
	{
		UE_LOG(LogMacTextInputMethodSystem, Error, TEXT("Unregistering a context failed when its registration couldn't be found."));
		return;
	}
	
	TSharedPtr<ITextInputMethodChangeNotifier> Notifier(NotifierRef.Pin());
	FTextInputMethodChangeNotifier* MacNotifier = (FTextInputMethodChangeNotifier*)Notifier.Get();
	TSharedPtr<FGenericWindow> GenericWindow = MacNotifier->GetContextWindow();
	if(GenericWindow.IsValid())
	{
		DeactivateContext(Context);
	}
	
	ContextMap.Remove(Context);
}

void FMacTextInputMethodSystem::ActivateContext(const TSharedRef<ITextInputMethodContext>& Context)
{
	TWeakPtr<ITextInputMethodChangeNotifier>& NotifierRef = ContextMap[Context];
	if(!NotifierRef.IsValid())
	{
		UE_LOG(LogMacTextInputMethodSystem, Error, TEXT("Activating a context failed when its registration couldn't be found."));
		return;
	}
	
	TSharedPtr<ITextInputMethodChangeNotifier> Notifier(NotifierRef.Pin());
	FTextInputMethodChangeNotifier* MacNotifier = (FTextInputMethodChangeNotifier*)Notifier.Get();
	TSharedPtr<FGenericWindow> GenericWindow = Context->GetWindow();
	if(GenericWindow.IsValid())
	{
		MacNotifier->SetContextWindow(GenericWindow);
		FSlateCocoaWindow* CocoaWindow = (FSlateCocoaWindow*)GenericWindow->GetOSWindowHandle();
		if(CocoaWindow && [CocoaWindow openGLView])
		{
			NSView* GLView = [CocoaWindow openGLView];
			if([GLView isKindOfClass:[FSlateTextView class]])
			{
				FSlateTextView* TextView = (FSlateTextView*)GLView;
				[TextView activateInputMethod:Context];
				return;
			}
		}
	}
	UE_LOG(LogMacTextInputMethodSystem, Error, TEXT("Activating a context failed when its window couldn't be found."));
}

void FMacTextInputMethodSystem::DeactivateContext(const TSharedRef<ITextInputMethodContext>& Context)
{
	TWeakPtr<ITextInputMethodChangeNotifier>& NotifierRef = ContextMap[Context];
	if(!NotifierRef.IsValid())
	{
		UE_LOG(LogMacTextInputMethodSystem, Error, TEXT("Deactivating a context failed when its registration couldn't be found."));
		return;
	}
	
	TSharedPtr<ITextInputMethodChangeNotifier> Notifier(NotifierRef.Pin());
	FTextInputMethodChangeNotifier* MacNotifier = (FTextInputMethodChangeNotifier*)Notifier.Get();
	TSharedPtr<FGenericWindow> GenericWindow = MacNotifier->GetContextWindow();
	if(GenericWindow.IsValid())
	{
		FSlateCocoaWindow* CocoaWindow = (FSlateCocoaWindow*)GenericWindow->GetOSWindowHandle();
		if(CocoaWindow && [CocoaWindow openGLView])
		{
			NSView* GLView = [CocoaWindow openGLView];
			if([GLView isKindOfClass:[FSlateTextView class]])
			{
				FSlateTextView* TextView = (FSlateTextView*)GLView;
				[TextView deactivateInputMethod];
				return;
			}
		}
	}
	UE_LOG(LogMacTextInputMethodSystem, Error, TEXT("Deactivating a context failed when its window couldn't be found."));
}
// ITextInputMethodSystem Interface End
