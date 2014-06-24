// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_VIEWS_NSWINDOW_DELEGATE_H_
#define UI_VIEWS_COCOA_VIEWS_NSWINDOW_DELEGATE_H_

#import <Cocoa/Cocoa.h>

namespace views {
class NativeWidgetMac;
class BridgedNativeWidget;
}

// The delegate set on the NSWindow when a views::BridgedNativeWidget is
// initialized.
@interface ViewsNSWindowDelegate : NSObject<NSWindowDelegate> {
 @private
  views::BridgedNativeWidget* parent_;  // Weak. Owns this.
}

// The NativeWidgetMac that created the window this is attached to. Returns
// NULL if not created by NativeWidgetMac.
@property(nonatomic, readonly) views::NativeWidgetMac* nativeWidgetMac;

// Initialize with the given |parent|.
- (id)initWithBridgedNativeWidget:(views::BridgedNativeWidget*)parent;

@end

#endif  // UI_VIEWS_COCOA_VIEWS_NSWINDOW_DELEGATE_H_
