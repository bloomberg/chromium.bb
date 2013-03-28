// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_COCOA_POPUP_CONTROLLER_H_
#define UI_MESSAGE_CENTER_COCOA_POPUP_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#import "base/memory/scoped_nsobject.h"

namespace message_center {
class Notification;
class NotificationChangeObserver;
}

@class MCNotificationController;

// A window controller that hosts a notification as a popup balloon on the
// user's desktop. Unlike most window controllers, this does not own itself and
// its lifetime must be managed manually.
@interface MCPopupController : NSWindowController {
 @private
  scoped_nsobject<MCNotificationController> notificationController_;
}

// Designated initializer.
- (id)initWithNotification:(const message_center::Notification*)notification
    changeObserver:(message_center::NotificationChangeObserver*)observer;

// Accessor for the notification.
- (const message_center::Notification*)notification;

@end

#endif  // UI_MESSAGE_CENTER_COCOA_POPUP_CONTROLLER_H_
