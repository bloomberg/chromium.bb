// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "ui/base/ui_base_export.h"

@class NSColor;

// HyperlinkTextView is an NSTextView subclass for unselectable, linkable text.
// This subclass doesn't show the text caret or IBeamCursor, whereas the base
// class NSTextView displays both with full keyboard accessibility enabled.
UI_BASE_EXPORT
@interface HyperlinkTextView : NSTextView {
 @private
  BOOL refusesFirstResponder_;
  BOOL drawsBackgroundUsingSuperview_;
}

@property(nonatomic, assign) BOOL drawsBackgroundUsingSuperview;

// Convenience function that sets the |HyperlinkTextView| contents to the
// specified |message| with a hypertext style |link| inserted at |linkOffset|.
// Uses the supplied |font|, |messageColor|, and |linkColor|.
- (void)setMessageAndLink:(NSString*)message
                 withLink:(NSString*)link
                 atOffset:(NSUInteger)linkOffset
                     font:(NSFont*)font
             messageColor:(NSColor*)messageColor
                linkColor:(NSColor*)linkColor;

// Set the |message| displayed by the HyperlinkTextView, using |font| and
// |messageColor|.
- (void)setMessage:(NSString*)message
          withFont:(NSFont*)font
      messageColor:(NSColor*)messageColor;

// Marks a |range| within the given message as link, associating it with
// a |name| that is passed to the delegate's textView:clickedOnLink:atIndex:.
- (void)addLinkRange:(NSRange)range
            withName:(id)name
           linkColor:(NSColor*)linkColor;

// This is NO (by default) if the view rejects first responder status.
- (void)setRefusesFirstResponder:(BOOL)refusesFirstResponder;

@end
