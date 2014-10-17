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

// Notify that the window is about to be reordered on screen. This ensures a
// paint will occur, even if Cocoa has not yet updated the window visibility.
- (void)onWindowOrderWillChange:(NSWindowOrderingMode)orderingMode;

// Notify that the window has been reordered in (or removed from) the window
// server's screen list. This is a substitute for -[NSWindowDelegate
// windowDidExpose:], which is only sent for nonretained windows (those without
// a backing store).
- (void)onWindowOrderChanged;

@end

#endif  // UI_VIEWS_COCOA_VIEWS_NSWINDOW_DELEGATE_H_
