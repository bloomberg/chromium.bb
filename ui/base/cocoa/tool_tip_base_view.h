// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_TOOL_TIP_BASE_VIEW_H_
#define UI_BASE_COCOA_TOOL_TIP_BASE_VIEW_H_

#import <AppKit/AppKit.h>

#import "ui/base/cocoa/base_view.h"

// An NSiew that allows tooltip text to be set at the current mouse location. It
// can take effect immediately, but won't appear unless the tooltip delay has
// elapsed.
UI_BASE_EXPORT
@interface ToolTipBaseView : BaseView {
 @private
  // These are part of the magic tooltip code from WebKit's WebHTMLView:
  id _trackingRectOwner;  // (not retained)
  void* _trackingRectUserData;
  NSTrackingRectTag _lastToolTipTag;
  base::scoped_nsobject<NSString> _toolTip;
}

// Set the current tooltip. It is the responsibility of the caller to set a nil
// tooltip when the mouse cursor leaves the appropriate region.
- (void)setToolTipAtMousePoint:(NSString*)string;

@end

#endif  // UI_BASE_COCOA_TOOL_TIP_BASE_VIEW_H_
