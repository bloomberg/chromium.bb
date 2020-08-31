// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/focus_tracker.h"


@implementation FocusTracker

- (instancetype)initWithWindow:(NSWindow*)window {
  if ((self = [super init])) {
    NSResponder* current_focus = [window firstResponder];

    // Special case NSTextViews, because they are removed from the
    // view hierarchy when their text field does not have focus.  If
    // an NSTextView is the current first responder, save a pointer to
    // its NSTextField delegate instead.
    if ([current_focus isKindOfClass:[NSTextView class]]) {
      id delegate = [(NSTextView*)current_focus delegate];
      if ([delegate isKindOfClass:[NSTextField class]])
        current_focus = delegate;
      else
        current_focus = nil;
    }

    if ([current_focus isKindOfClass:[NSView class]]) {
      NSView* current_focus_view = (NSView*)current_focus;
      _focusedView.reset([current_focus_view retain]);
    }
  }

  return self;
}

- (BOOL)restoreFocusInWindow:(NSWindow*)window {
  if (!_focusedView.get())
    return NO;

  if ([_focusedView window] && [_focusedView window] == window)
    return [window makeFirstResponder:_focusedView.get()];

  return NO;
}

@end
