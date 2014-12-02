// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_NATIVE_WIDGET_MAC_NSWINDOW_H_
#define UI_VIEWS_COCOA_NATIVE_WIDGET_MAC_NSWINDOW_H_

#import <Cocoa/Cocoa.h>

#include "ui/views/views_export.h"

// The NSWindow used by BridgedNativeWidget. Provides hooks into AppKit that
// can only be accomplished by overriding methods.
VIEWS_EXPORT
@interface NativeWidgetMacNSWindow : NSWindow
@end

#endif  // UI_VIEWS_COCOA_NATIVE_WIDGET_MAC_NSWINDOW_H_
