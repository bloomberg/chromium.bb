// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/bridged_content_view.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "grit/ui_strings.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/gfx/canvas_paint_mac.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

@interface BridgedContentView ()

// Translates the location of |theEvent| to toolkit-views coordinates and passes
// the event to NativeWidgetMac for handling.
- (void)handleMouseEvent:(NSEvent*)theEvent;

// Execute a command on the currently focused TextInputClient.
// |commandId| should be a resource ID from ui_strings.grd.
- (void)doCommandByID:(int)commandId;

@end

@implementation BridgedContentView

@synthesize hostedView = hostedView_;
@synthesize textInputClient = textInputClient_;

- (id)initWithView:(views::View*)viewToHost {
  DCHECK(viewToHost);
  gfx::Rect bounds = viewToHost->bounds();
  // To keep things simple, assume the origin is (0, 0) until there exists a use
  // case for something other than that.
  DCHECK(bounds.origin().IsOrigin());
  NSRect initialFrame = NSMakeRect(0, 0, bounds.width(), bounds.height());
  if ((self = [super initWithFrame:initialFrame]))
    hostedView_ = viewToHost;

  return self;
}

- (void)clearView {
  hostedView_ = NULL;
}

// BridgedContentView private implementation.

- (void)handleMouseEvent:(NSEvent*)theEvent {
  if (!hostedView_)
    return;

  ui::MouseEvent event(theEvent);
  hostedView_->GetWidget()->OnMouseEvent(&event);
}

- (void)doCommandByID:(int)commandId {
  if (textInputClient_ && textInputClient_->IsEditingCommandEnabled(commandId))
    textInputClient_->ExecuteEditingCommand(commandId);
}

// NSView implementation.

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (void)setFrameSize:(NSSize)newSize {
  [super setFrameSize:newSize];
  if (!hostedView_)
    return;

  hostedView_->SetSize(gfx::Size(newSize.width, newSize.height));
}

- (void)drawRect:(NSRect)dirtyRect {
  if (!hostedView_)
    return;

  gfx::CanvasSkiaPaint canvas(dirtyRect, false /* opaque */);
  hostedView_->Paint(&canvas, views::CullSet());
}

// NSResponder implementation.

- (void)keyDown:(NSEvent*)theEvent {
  if (textInputClient_)
    [self interpretKeyEvents:@[ theEvent ]];
  else
    [super keyDown:theEvent];
}

- (void)mouseDown:(NSEvent*)theEvent {
  [self handleMouseEvent:theEvent];
}

- (void)rightMouseDown:(NSEvent*)theEvent {
  [self handleMouseEvent:theEvent];
}

- (void)otherMouseDown:(NSEvent*)theEvent {
  [self handleMouseEvent:theEvent];
}

- (void)mouseUp:(NSEvent*)theEvent {
  [self handleMouseEvent:theEvent];
}

- (void)rightMouseUp:(NSEvent*)theEvent {
  [self handleMouseEvent:theEvent];
}

- (void)otherMouseUp:(NSEvent*)theEvent {
  [self handleMouseEvent:theEvent];
}

- (void)mouseDragged:(NSEvent*)theEvent {
  [self handleMouseEvent:theEvent];
}

- (void)rightMouseDragged:(NSEvent*)theEvent {
  [self handleMouseEvent:theEvent];
}

- (void)otherMouseDragged:(NSEvent*)theEvent {
  [self handleMouseEvent:theEvent];
}

- (void)mouseMoved:(NSEvent*)theEvent {
  // Note: mouseEntered: and mouseExited: are not handled separately.
  // |hostedView_| is responsible for converting the move events into entered
  // and exited events for the view heirarchy.
  // TODO(tapted): Install/uninstall tracking areas when required so that the
  // NSView will receive these events outside of tests.
  [self handleMouseEvent:theEvent];
}

- (void)scrollWheel:(NSEvent*)theEvent {
  [self handleMouseEvent:theEvent];
}

- (void)deleteBackward:(id)sender {
  [self doCommandByID:IDS_DELETE_BACKWARD];
}

- (void)deleteForward:(id)sender {
  [self doCommandByID:IDS_DELETE_FORWARD];
}

- (void)moveLeft:(id)sender {
  [self doCommandByID:IDS_MOVE_LEFT];
}

- (void)moveRight:(id)sender {
  [self doCommandByID:IDS_MOVE_RIGHT];
}

- (void)insertText:(id)text {
  if (textInputClient_)
    textInputClient_->InsertText(base::SysNSStringToUTF16(text));
}

// Support for Services in context menus.
// Currently we only support reading and writing plain strings.
- (id)validRequestorForSendType:(NSString*)sendType
                     returnType:(NSString*)returnType {
  BOOL canWrite = [sendType isEqualToString:NSStringPboardType] &&
                  [self selectedRange].length > 0;
  BOOL canRead = [returnType isEqualToString:NSStringPboardType];
  // Valid if (sendType, returnType) is either (string, nil), (nil, string),
  // or (string, string).
  BOOL valid = textInputClient_ && ((canWrite && (canRead || !returnType)) ||
                                    (canRead && (canWrite || !sendType)));
  return valid ? self : [super validRequestorForSendType:sendType
                                              returnType:returnType];
}

// NSServicesRequests informal protocol.

- (BOOL)writeSelectionToPasteboard:(NSPasteboard*)pboard types:(NSArray*)types {
  DCHECK([types containsObject:NSStringPboardType]);
  if (!textInputClient_)
    return NO;

  gfx::Range selectionRange;
  if (!textInputClient_->GetSelectionRange(&selectionRange))
    return NO;

  base::string16 text;
  textInputClient_->GetTextFromRange(selectionRange, &text);
  return [pboard writeObjects:@[ base::SysUTF16ToNSString(text) ]];
}

- (BOOL)readSelectionFromPasteboard:(NSPasteboard*)pboard {
  NSArray* objects =
      [pboard readObjectsForClasses:@[ [NSString class] ] options:0];
  DCHECK([objects count] == 1);
  [self insertText:[objects lastObject]];
  return YES;
}

// NSTextInputClient protocol implementation.

- (NSAttributedString*)
    attributedSubstringForProposedRange:(NSRange)range
                            actualRange:(NSRangePointer)actualRange {
  base::string16 substring;
  if (textInputClient_) {
    gfx::Range textRange;
    textInputClient_->GetTextRange(&textRange);
    gfx::Range subrange = textRange.Intersect(gfx::Range(range));
    textInputClient_->GetTextFromRange(subrange, &substring);
    if (actualRange)
      *actualRange = subrange.ToNSRange();
  }
  return [[[NSAttributedString alloc]
      initWithString:base::SysUTF16ToNSString(substring)] autorelease];
}

- (NSUInteger)characterIndexForPoint:(NSPoint)aPoint {
  NOTIMPLEMENTED();
  return 0;
}

- (void)doCommandBySelector:(SEL)selector {
  if ([self respondsToSelector:selector])
    [self performSelector:selector withObject:nil];
  else
    [[self nextResponder] doCommandBySelector:selector];
}

- (NSRect)firstRectForCharacterRange:(NSRange)range
                         actualRange:(NSRangePointer)actualRange {
  NOTIMPLEMENTED();
  return NSZeroRect;
}

- (BOOL)hasMarkedText {
  return textInputClient_ && textInputClient_->HasCompositionText();
}

- (void)insertText:(id)text replacementRange:(NSRange)replacementRange {
  if (!textInputClient_)
    return;

  if ([text isKindOfClass:[NSAttributedString class]])
    text = [text string];
  textInputClient_->DeleteRange(gfx::Range(replacementRange));
  textInputClient_->InsertText(base::SysNSStringToUTF16(text));
}

- (NSRange)markedRange {
  if (!textInputClient_)
    return NSMakeRange(NSNotFound, 0);

  gfx::Range range;
  textInputClient_->GetCompositionTextRange(&range);
  return range.ToNSRange();
}

- (NSRange)selectedRange {
  if (!textInputClient_)
    return NSMakeRange(NSNotFound, 0);

  gfx::Range range;
  textInputClient_->GetSelectionRange(&range);
  return range.ToNSRange();
}

- (void)setMarkedText:(id)text
        selectedRange:(NSRange)selectedRange
     replacementRange:(NSRange)replacementRange {
  if (!textInputClient_)
    return;

  if ([text isKindOfClass:[NSAttributedString class]])
    text = [text string];
  ui::CompositionText composition;
  composition.text = base::SysNSStringToUTF16(text);
  composition.selection = gfx::Range(selectedRange);
  textInputClient_->SetCompositionText(composition);
}

- (void)unmarkText {
  if (textInputClient_)
    textInputClient_->ConfirmCompositionText();
}

- (NSArray*)validAttributesForMarkedText {
  return @[];
}

// NSAccessibility informal protocol implementation.

- (id)accessibilityAttributeValue:(NSString*)attribute {
  if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
    return @[ hostedView_->GetNativeViewAccessible() ];
  }

  return [super accessibilityAttributeValue:attribute];
}

- (id)accessibilityHitTest:(NSPoint)point {
  return [hostedView_->GetNativeViewAccessible() accessibilityHitTest:point];
}

@end
