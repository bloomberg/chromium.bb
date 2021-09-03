// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/notifications/stub_notification_center_mac.h"

#include "base/check.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"

@implementation StubNotificationCenter {
  base::scoped_nsobject<NSMutableArray> _banners;
  id<NSUserNotificationCenterDelegate> _delegate;
}

- (instancetype)init {
  if ((self = [super init])) {
    _banners.reset([[NSMutableArray alloc] init]);
    _delegate = nil;
  }
  return self;
}

// The default implementation adds some extra checks on what constructors can
// be used. isKindOfClass bypasses all of that.
- (BOOL)isKindOfClass:(Class)cls {
  if ([cls isEqual:NSClassFromString(@"_NSConcreteUserNotificationCenter")]) {
    return YES;
  }
  return [super isKindOfClass:cls];
}

- (void)deliverNotification:(NSUserNotification*)notification {
  [_banners addObject:notification];
}

- (NSArray*)deliveredNotifications {
  return [[_banners copy] autorelease];
}

- (void)removeDeliveredNotification:(NSUserNotification*)notification {
  NSString* notificationId =
      (notification.userInfo)[notification_constants::kNotificationId];
  NSString* profileId =
      (notification.userInfo)[notification_constants::kNotificationProfileId];
  BOOL incognito =
      [(notification.userInfo)[notification_constants::kNotificationIncognito]
          boolValue];
  DCHECK(profileId);
  DCHECK(notificationId);
  for (NSUserNotification* toast in _banners.get()) {
    NSString* toastId =
        (toast.userInfo)[notification_constants::kNotificationId];
    NSString* toastProfileId =
        (toast.userInfo)[notification_constants::kNotificationProfileId];
    BOOL toastIncognito =
        [(toast.userInfo)[notification_constants::kNotificationIncognito]
            boolValue];
    if ([notificationId isEqualToString:toastId] &&
        [profileId isEqualToString:toastProfileId] &&
        incognito == toastIncognito) {
      [_banners removeObject:toast];
      break;
    }
  }
}

- (void)removeAllDeliveredNotifications {
  [_banners removeAllObjects];
}

- (void)setDelegate:(id<NSUserNotificationCenterDelegate> _Nullable)delegate {
  _delegate = delegate;
}

- (id<NSUserNotificationCenterDelegate> _Nullable)delegate {
  return _delegate;
}

@end
