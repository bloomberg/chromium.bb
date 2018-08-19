// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_NATIVE_WIDGET_MAC_NSWINDOW_H_
#define UI_VIEWS_COCOA_NATIVE_WIDGET_MAC_NSWINDOW_H_

#import <Cocoa/Cocoa.h>

#import "ui/base/cocoa/command_dispatcher.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/util_mac.h"

@protocol WindowTouchBarDelegate;

// Weak lets Chrome launch even if a future macOS doesn't have the below classes

WEAK_IMPORT_ATTRIBUTE
@interface NSNextStepFrame : NSView
@end

@class NSThemeFrame;

VIEWS_EXPORT
@interface NativeWidgetMacNSWindowBorderlessFrame : NSNextStepFrame
@end

VIEWS_EXPORT
@interface NativeWidgetMacNSWindowTitledFrame : NSThemeFrame
@end

// The NSWindow used by BridgedNativeWidget. Provides hooks into AppKit that
// can only be accomplished by overriding methods.
VIEWS_EXPORT
@interface NativeWidgetMacNSWindow : NSWindow<CommandDispatchingWindow>

// Set a CommandDispatcherDelegate, i.e. to implement key event handling.
- (void)setCommandDispatcherDelegate:(id<CommandDispatcherDelegate>)delegate;

// Selector passed to [NSApp beginSheet:]. Forwards to [self delegate], if set.
- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(NSInteger)returnCode
        contextInfo:(void*)contextInfo;

// Set a WindowTouchBarDelegate to allow creation of a custom TouchBar when
// AppKit follows the responder chain and reaches the NSWindow when trying to
// create one.
- (void)setWindowTouchBarDelegate:(id<WindowTouchBarDelegate>)delegate;

// Override -[NSWindow toggleFullScreen:] to be a no-op for testing.
- (void)disableToggleFullScreenForTesting;

// Methods to query properties for tests.
// TODO(ccameron): It may be more appropriate to put testing methods into a
// separate subclass.
@property(readonly, nonatomic) int invalidateShadowCountForTesting;
@property(readonly, nonatomic) int orderWindowCountForTesting;
@property(readonly, nonatomic) int toggleFullScreenCountForTesting;
@property(assign, nonatomic) bool* deallocFlagForTesting;
@end

#endif  // UI_VIEWS_COCOA_NATIVE_WIDGET_MAC_NSWINDOW_H_
