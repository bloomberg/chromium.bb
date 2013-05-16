// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_COCOA_TRAY_VIEW_CONTROLLER_H_
#define UI_MESSAGE_CENTER_COCOA_TRAY_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include <map>
#include <string>

#import "base/memory/scoped_nsobject.h"
#include "ui/message_center/message_center_export.h"

@class MCNotificationController;

namespace message_center {
class MessageCenter;
}

// The view controller responsible for the content of the message center tray
// UI. This hosts a scroll view of all the notifications, as well as buttons
// to enter quiet mode and the settings panel.
MESSAGE_CENTER_EXPORT
@interface MCTrayViewController : NSViewController {
 @private
  // Controller of the notifications, where action messages are forwarded. Weak.
  message_center::MessageCenter* messageCenter_;

  // The scroll view that contains all the notifications in its documentView.
  scoped_nsobject<NSScrollView> scrollView_;

  // Array of MCNotificationController objects, which the array owns.
  scoped_nsobject<NSMutableArray> notifications_;

  // Map of notification IDs to weak pointers of the view controllers in
  // |notifications_|.
  std::map<std::string, MCNotificationController*> notificationsMap_;
}

// Designated initializer.
- (id)initWithMessageCenter:(message_center::MessageCenter*)messageCenter;

// Callback for when the MessageCenter model changes.
- (void)onMessageCenterTrayChanged;

// Action for the quiet mode button.
- (void)toggleQuietMode:(id)sender;

// Action for the clear all button.
- (void)clearAllNotifications:(id)sender;

// Action for the settings button.
- (void)showSettings:(id)sender;

@end

// Testing API /////////////////////////////////////////////////////////////////

@interface MCTrayViewController (TestingAPI)
- (NSScrollView*)scrollView;
@end

#endif  // UI_MESSAGE_CENTER_COCOA_TRAY_VIEW_CONTROLLER_H_
