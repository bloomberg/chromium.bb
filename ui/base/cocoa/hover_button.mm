// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/hover_button.h"

@implementation HoverButton

@synthesize hoverState = hoverState_;
@synthesize trackingEnabled = trackingEnabled_;

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    [self commonInit];
  }
  return self;
}

- (void)awakeFromNib {
  [self commonInit];
}

- (void)commonInit {
  self.hoverState = kHoverStateNone;
  self.trackingEnabled = YES;
}

- (void)dealloc {
  self.trackingEnabled = NO;
  [super dealloc];
}

- (void)setTrackingEnabled:(BOOL)trackingEnabled {
  if (trackingEnabled == trackingEnabled_)
    return;
  trackingEnabled_ = trackingEnabled;
  [self updateTrackingAreas];
}

- (void)setEnabled:(BOOL)enabled {
  if (enabled == self.enabled)
    return;
  super.enabled = enabled;
  [self updateTrackingAreas];
}

- (void)mouseEntered:(NSEvent*)theEvent {
  if (trackingArea_.get())
    self.hoverState = kHoverStateMouseOver;
}

- (void)mouseMoved:(NSEvent*)theEvent {
  [self checkImageState];
}

- (void)mouseExited:(NSEvent*)theEvent {
  if (trackingArea_.get())
    self.hoverState = kHoverStateNone;
}

- (void)mouseDown:(NSEvent*)theEvent {
  if (!self.enabled)
    return;
  mouseDown_ = YES;
  self.hoverState = kHoverStateMouseDown;

  // The hover button needs to hold onto itself here for a bit.  Otherwise,
  // it can be freed while in the tracking loop below.
  // http://crbug.com/28220
  base::scoped_nsobject<HoverButton> myself([self retain]);

  // Begin tracking the mouse.
  if ([theEvent type] == NSLeftMouseDown) {
    NSWindow* window = [self window];
    NSEvent* nextEvent = nil;

    // For the tracking loop ignore key events so that they don't pile up in
    // the queue and get processed after the user releases the mouse.
    const NSEventMask eventMask = (NSLeftMouseDraggedMask | NSLeftMouseUpMask |
                                   NSKeyDownMask | NSKeyUpMask);

    while ((nextEvent = [window nextEventMatchingMask:eventMask])) {
      if ([nextEvent type] == NSLeftMouseUp)
        break;
      // Update the image state, which will change if the user moves the mouse
      // into or out of the button.
      [self checkImageState];
    }
  }

  // If the mouse is still over the button, it means the user clicked the
  // button.
  if (self.hoverState == kHoverStateMouseDown) {
    [self sendAction:self.action to:self.target];
  }

  // Clean up.
  mouseDown_ = NO;
  [self checkImageState];
}

- (void)setAccessibilityTitle:(NSString*)accessibilityTitle {
  NSCell* cell = [self cell];
  [cell accessibilitySetOverrideValue:accessibilityTitle
                         forAttribute:NSAccessibilityTitleAttribute];
}

- (void)updateTrackingAreas {
  if (trackingEnabled_ && self.enabled) {
    if (trackingArea_.get())
      return;
    trackingArea_.reset(
        [[CrTrackingArea alloc] initWithRect:NSZeroRect
                                     options:NSTrackingMouseEnteredAndExited |
                                             NSTrackingMouseMoved |
                                             NSTrackingActiveAlways |
                                             NSTrackingInVisibleRect
                                       owner:self
                                    userInfo:nil]);
    [self addTrackingArea:trackingArea_.get()];

    // If you have a separate window that overlaps the close button, and you
    // move the mouse directly over the close button without entering another
    // part of the tab strip, we don't get any mouseEntered event since the
    // tracking area was disabled when we entered.
    // Done with a delay of 0 because sometimes an event appears to be missed
    // between the activation of the tracking area and the call to
    // checkImageState resulting in the button state being incorrect.
    [self performSelector:@selector(checkImageState)
               withObject:nil
               afterDelay:0];
  } else {
    if (trackingArea_.get()) {
      self.hoverState = kHoverStateNone;
      [self removeTrackingArea:trackingArea_.get()];
      trackingArea_.reset(nil);
    }
  }
  [super updateTrackingAreas];
  [self checkImageState];
}

- (void)checkImageState {
  if (!trackingArea_.get())
    return;

  // Update the button's state if the button has moved.
  NSPoint mouseLoc = [[self window] mouseLocationOutsideOfEventStream];
  mouseLoc = [self convertPoint:mouseLoc fromView:nil];
  BOOL mouseInBounds = NSPointInRect(mouseLoc, [self bounds]);
  if (mouseDown_ && mouseInBounds) {
    self.hoverState = kHoverStateMouseDown;
  } else {
    self.hoverState = mouseInBounds ? kHoverStateMouseOver : kHoverStateNone;
  }
}

- (void)setHoverState:(HoverState)hoverState {
  if (hoverState == hoverState_)
    return;
  hoverState_ = hoverState;
  self.needsDisplay = YES;
}

@end
