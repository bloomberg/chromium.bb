// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_COCOA_TRAY_CONTROLLER_H_
#define UI_MESSAGE_CENTER_COCOA_TRAY_CONTROLLER_H_

#import <AppKit/AppKit.h>

#include "base/basictypes.h"
#include "base/memory/scoped_nsobject.h"
#include "ui/message_center/message_center_export.h"

@class MCTrayViewController;

namespace message_center {
class MessageCenterTray;
}

// The window controller for the message center tray. This merely hosts the
// view from MCTrayViewController.
MESSAGE_CENTER_EXPORT
@interface MCTrayController : NSWindowController {
 @private
  message_center::MessageCenterTray* tray_;  // Weak, indirectly owns this.

  // View controller that provides this window's content.
  scoped_nsobject<MCTrayViewController> viewController_;
}

// Designated initializer.
- (id)initWithMessageCenterTray:(message_center::MessageCenterTray*)tray;

// Opens the message center tray with the window's upper-left corner at |point|.
- (void)showTrayAt:(NSPoint)point;

// Callback from MessageCenterTrayDelegate, used to update the tray content.
- (void)onMessageCenterTrayChanged;

@end

#endif  // UI_MESSAGE_CENTER_COCOA_TRAY_CONTROLLER_H_
