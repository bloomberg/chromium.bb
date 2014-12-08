// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_BRIDGED_CONTENT_VIEW_H_
#define UI_VIEWS_COCOA_BRIDGED_CONTENT_VIEW_H_

#import <Cocoa/Cocoa.h>

#import "ui/base/cocoa/tracking_area.h"

namespace ui {
class TextInputClient;
}

namespace views {
class View;
}

// The NSView that sits as the root contentView of the NSWindow, whilst it has
// a views::RootView present. Bridges requests from Cocoa to the hosted
// views::View.
@interface BridgedContentView : NSView<NSTextInputClient> {
 @private
  // Weak. The hosted RootView, owned by hostedView_->GetWidget().
  views::View* hostedView_;

  // Weak. If non-null the TextInputClient of the currently focused View in the
  // hierarchy rooted at |hostedView_|. Owned by the focused View.
  ui::TextInputClient* textInputClient_;

  // A tracking area installed to enable mouseMoved events.
  ui::ScopedCrTrackingArea trackingArea_;
}

@property(readonly, nonatomic) views::View* hostedView;
@property(assign, nonatomic) ui::TextInputClient* textInputClient;

// Initialize the NSView -> views::View bridge. |viewToHost| must be non-NULL.
- (id)initWithView:(views::View*)viewToHost;

// Clear the hosted view. For example, if it is about to be destroyed.
- (void)clearView;

@end

#endif  // UI_VIEWS_COCOA_BRIDGED_CONTENT_VIEW_H_
