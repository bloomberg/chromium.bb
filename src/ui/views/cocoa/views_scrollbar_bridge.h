// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_VIEWS_SCROLLBAR_BRIDGE_DELEGATE_H_
#define UI_VIEWS_COCOA_VIEWS_SCROLLBAR_BRIDGE_DELEGATE_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "ui/views/views_export.h"

// The delegate set to ViewsScrollbarBridge.
class ViewsScrollbarBridgeDelegate {
 public:
  // Invoked by ViewsScrollbarBridge when the system informs the process that
  // the preferred scroller style has changed
  virtual void OnScrollerStyleChanged() = 0;
};

// A bridge to NSScroller managed by NativeCocoaScrollbar. Serves as a helper
// class to bridge NSScroller notifications and functions to CocoaScrollbar.
@interface ViewsScrollbarBridge : NSObject {
 @private
  ViewsScrollbarBridgeDelegate* delegate_;  // Weak. Owns this.
}

// Initializes with the given delegate and registers for notifications on
// scroller style changes.
- (id)initWithDelegate:(ViewsScrollbarBridgeDelegate*)delegate;

// Sets |delegate_| to nullptr.
-(void)clearDelegate;

// Returns the style of scrollers that OSX is using.
+ (NSScrollerStyle)getPreferredScrollerStyle;

@end

#endif