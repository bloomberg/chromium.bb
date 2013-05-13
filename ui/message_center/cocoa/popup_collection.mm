// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/cocoa/popup_collection.h"

#import "ui/message_center/cocoa/notification_controller.h"
#import "ui/message_center/cocoa/popup_controller.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_constants.h"
#include "ui/message_center/message_center_observer.h"

@interface MCPopupCollection (Private)
// Returns the primary screen's visible frame rectangle.
- (NSRect)screenFrame;

// Shows a popup, if there is room on-screen, for the given notification.
// Returns YES if the notification was actually displayed.
- (BOOL)addNotification:(const message_center::Notification*)notification;

// Updates the contents of the notification with the given ID.
- (void)updateNotification:(const std::string&)notificationID;

// Removes a popup from the screen and lays out new notifications that can
// now potentially fit on the screen.
- (void)removeNotification:(const std::string&)notificationID;

// Closes all the popups.
- (void)removeAllNotifications;

// Positions popup notifications for the |index|th notification in |popups_|.
// This will put the proper amount of space between popup |index - 1| and
// |index| and then any subsequent notifications.
- (void)layoutNotificationsStartingAt:(NSUInteger)index;
@end

namespace {

class PopupCollectionObserver : public message_center::MessageCenterObserver {
 public:
  PopupCollectionObserver(message_center::MessageCenter* message_center,
                          MCPopupCollection* popup_collection)
      : message_center_(message_center),
        popup_collection_(popup_collection) {
    message_center_->AddObserver(this);
  }

  virtual ~PopupCollectionObserver() {
    message_center_->RemoveObserver(this);
  }

  virtual void OnNotificationAdded(
      const std::string& notification_id) OVERRIDE {
    for (const auto& notification : message_center_->GetPopupNotifications()) {
      if (notification->id() == notification_id) {
        [popup_collection_ addNotification:notification];
        return;
      }
    }
  }

  virtual void OnNotificationRemoved(const std::string& notification_id,
                                     bool user_id) OVERRIDE {
    [popup_collection_ removeNotification:notification_id];
  }

  virtual void OnNotificationUpdated(
      const std::string& notification_id) OVERRIDE {
    [popup_collection_ updateNotification:notification_id];
  }

 private:
  message_center::MessageCenter* message_center_;  // Weak, global.

  MCPopupCollection* popup_collection_;  // Weak, owns this.
};

}  // namespace

@implementation MCPopupCollection

- (id)initWithMessageCenter:(message_center::MessageCenter*)messageCenter {
  if ((self = [super init])) {
    messageCenter_ = messageCenter;
    observer_.reset(new PopupCollectionObserver(messageCenter_, self));
    popups_.reset([[NSMutableArray alloc] init]);
  }
  return self;
}

- (void)dealloc {
  [self removeAllNotifications];
  [super dealloc];
}

// Testing API /////////////////////////////////////////////////////////////////

- (void)setScreenFrame:(NSRect)frame {
  testingScreenFrame_ = frame;
}

// Private /////////////////////////////////////////////////////////////////////

- (NSRect)screenFrame {
  if (!NSIsEmptyRect(testingScreenFrame_))
    return testingScreenFrame_;
  return [[[NSScreen screens] objectAtIndex:0] visibleFrame];
}

- (BOOL)addNotification:(const message_center::Notification*)notification {
  scoped_nsobject<MCPopupController> popup(
      [[MCPopupController alloc] initWithNotification:notification
                                        messageCenter:messageCenter_]);

  NSRect screenFrame = [self screenFrame];
  NSRect popupFrame = [[popup window] frame];

  CGFloat x = NSMaxX(screenFrame) - message_center::kMarginBetweenItems -
      NSWidth(popupFrame);
  CGFloat y = 0;

  MCPopupController* bottomPopup = [popups_ lastObject];
  if (!bottomPopup) {
    y = NSMaxY(screenFrame);
  } else {
    y = NSMinY([[bottomPopup window] frame]);
  }

  y -= message_center::kMarginBetweenItems + NSHeight(popupFrame);

  if (y > NSMinY(screenFrame)) {
    [[popup window] setFrameOrigin:NSMakePoint(x, y)];
    [popup showWindow:self];
    [popups_ addObject:popup];  // Transfer ownership.
    return YES;
  }

  return NO;
}

- (void)updateNotification:(const std::string&)notificationID {
  // Find the controller for this notification ID.
  NSUInteger index = [popups_ indexOfObjectPassingTest:
      ^BOOL(id popup, NSUInteger i, BOOL* stop) {
          return [popup notificationID] == notificationID;
      }];
  // The notification may not be on screen.
  if (index == NSNotFound)
    return;

  // Find the corresponding model object for the ID. This may be a replaced
  // notification, so the controller's current model object may be stale.
  const message_center::Notification* notification = NULL;
  const auto& modelPopups = messageCenter_->GetPopupNotifications();
  for (auto it = modelPopups.begin(); it != modelPopups.end(); ++it) {
    if ((*it)->id() == notificationID) {
      notification = *it;
      break;
    }
  }
  DCHECK(notification);
  if (!notification)
    return;

  MCPopupController* popup = [popups_ objectAtIndex:index];

  CGFloat oldHeight =
      NSHeight([[[popup notificationController] view] frame]);
  CGFloat newHeight = NSHeight(
      [[popup notificationController] updateNotification:notification]);

  // The notification has changed height. This requires updating the popup
  // window and any popups that come below it.
  if (oldHeight != newHeight) {
    NSRect popupFrame = [[popup window] frame];
    popupFrame.size.height += newHeight - oldHeight;
    [[popup window] setFrame:popupFrame display:YES];

    // Start re-layout of notifications at this index, so that it readjusts
    // the Y origin of this notification.
    [self layoutNotificationsStartingAt:index];
  }
}

- (void)removeNotification:(const std::string&)notificationID {
  NSUInteger index = [popups_ indexOfObjectPassingTest:
      ^BOOL(id popup, NSUInteger index, BOOL* stop) {
          return [popup notificationID] == notificationID;
      }];
  // The notification may not be on screen.
  if (index == NSNotFound)
    return;

  [[popups_ objectAtIndex:index] close];
  [popups_ removeObjectAtIndex:index];

  if (index < [popups_ count])
    [self layoutNotificationsStartingAt:index];
}

- (void)removeAllNotifications {
  [popups_ makeObjectsPerformSelector:@selector(close)];
  [popups_ removeAllObjects];
}

- (void)layoutNotificationsStartingAt:(NSUInteger)index {
  NSRect screenFrame = [self screenFrame];
  std::set<std::string> onScreen;

  // Calcluate the bottom edge of the |index - 1|th popup.
  CGFloat maxY = 0;
  if (index == 0)
    maxY = NSMaxY(screenFrame);
  else
    maxY = NSMinY([[[popups_ objectAtIndex:index - 1] window] frame]);

  // Collect the IDs of popups that are currently on screen.
  for (NSUInteger i = 0; i < index; ++i)
    onScreen.insert([[popups_ objectAtIndex:i] notificationID]);

  // Iterate between [index, count) notifications and reposition each. If one
  // does not fit on screen, close it and any other on-screen popups that come
  // after it.
  NSUInteger removeAt = NSNotFound;
  for (NSUInteger i = index; i < [popups_ count]; ++i) {
    MCPopupController* popup = [popups_ objectAtIndex:i];
    NSRect frame = [[popup window] frame];
    frame.origin.y = maxY - message_center::kMarginBetweenItems -
                     NSHeight(frame);

    // If this popup does not fit on screen, stop repositioning and close this
    // and subsequent popups.
    if (NSMinY(frame) < NSMinY(screenFrame)) {
      removeAt = i;
      break;
    }

    [[popup window] setFrame:frame display:YES];
    onScreen.insert([popup notificationID]);

    // Set the new maximum Y to be the bottom of this notification.
    maxY = NSMinY(frame);
  }

  if (removeAt != NSNotFound) {
    // Remove any popups that are on screen but no longer fit.
    while ([popups_ count] >= removeAt) {
      [[popups_ lastObject] close];
      [popups_ removeLastObject];
    }
  } else {
    // Display any new popups that can now fit on screen.
    const auto& allPopups = messageCenter_->GetPopupNotifications();
    for (auto it = allPopups.begin(); it != allPopups.end(); ++it) {
      if (onScreen.find((*it)->id()) == onScreen.end()) {
        // If there's no room left on screen to display notifications, stop
        // trying.
        if (![self addNotification:*it])
          break;
      }
    }
  }
}

@end
