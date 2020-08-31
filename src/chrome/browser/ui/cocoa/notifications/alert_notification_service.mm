// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/notifications/alert_notification_service.h"

#include <unistd.h>

#import "base/mac/scoped_nsobject.h"
#include "base/strings/string_number_conversions.h"
#import "chrome/browser/ui/cocoa/notifications/notification_builder_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"
#import "chrome/browser/ui/cocoa/notifications/xpc_transaction_handler.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"
#include "third_party/crashpad/crashpad/client/simple_string_dictionary.h"

@class NSUserNotificationCenter;

namespace {

crashpad::SimpleStringDictionary* GetCrashpadAnnotations() {
  static crashpad::SimpleStringDictionary* annotations = []() {
    auto* annotations = new crashpad::SimpleStringDictionary();
    annotations->SetKeyValue("ptype", "AlertNotificationService.xpc");
    annotations->SetKeyValue("pid", base::NumberToString(getpid()).c_str());
    return annotations;
  }();
  return annotations;
}

}  // namespace

@implementation AlertNotificationService {
  XPCTransactionHandler* _transactionHandler;

  // Ensures that the XPC service has been configured for crash reporting.
  // Other messages should not be sent to a new instance of the service
  // before -setMachExceptionPort: is called.
  // Because XPC callouts occur on a concurrent dispatch queue, this must be
  // accessed in a @synchronized(self) block.
  BOOL _didSetExceptionPort;
}

- (instancetype)initWithTransactionHandler:(XPCTransactionHandler*)handler {
  if ((self = [super init])) {
    _transactionHandler = handler;
  }
  return self;
}

- (void)setMachExceptionPort:(CrXPCMachPort*)port {
  base::mac::ScopedMachSendRight sendRight([port takeRight]);
  if (!sendRight.is_valid()) {
    NOTREACHED();
    return;
  }

  @synchronized(self) {
    if (_didSetExceptionPort) {
      return;
    }

    crashpad::CrashpadClient client;
    _didSetExceptionPort = client.SetHandlerMachPort(std::move(sendRight));
    DCHECK(_didSetExceptionPort);

    crashpad::CrashpadInfo::GetCrashpadInfo()->set_simple_annotations(
        GetCrashpadAnnotations());
  }
}

- (void)deliverNotification:(NSDictionary*)notificationData {
  DCHECK(_didSetExceptionPort);

  base::scoped_nsobject<NotificationBuilder> builder(
      [[NotificationBuilder alloc] initWithDictionary:notificationData]);

  NSUserNotification* toast = [builder buildUserNotification];
  [[NSUserNotificationCenter defaultUserNotificationCenter]
      deliverNotification:toast];
  [_transactionHandler openTransactionIfNeeded];
}

- (void)closeNotificationWithId:(NSString*)notificationId
                  withProfileId:(NSString*)profileId {
  DCHECK(_didSetExceptionPort);

  NSUserNotificationCenter* notificationCenter =
      [NSUserNotificationCenter defaultUserNotificationCenter];
  for (NSUserNotification* candidate in
       [notificationCenter deliveredNotifications]) {
    NSString* candidateId = [candidate.userInfo
        objectForKey:notification_constants::kNotificationId];

    NSString* candidateProfileId = [candidate.userInfo
        objectForKey:notification_constants::kNotificationProfileId];

    if ([candidateId isEqualToString:notificationId] &&
        [profileId isEqualToString:candidateProfileId]) {
      [notificationCenter removeDeliveredNotification:candidate];
      [_transactionHandler closeTransactionIfNeeded];
      break;
    }
  }
}

- (void)closeAllNotifications {
  DCHECK(_didSetExceptionPort);

  [[NSUserNotificationCenter defaultUserNotificationCenter]
      removeAllDeliveredNotifications];
  [_transactionHandler closeTransactionIfNeeded];
}

- (void)getDisplayedAlertsForProfileId:(NSString*)profileId
                          andIncognito:(BOOL)incognito
                             withReply:(void (^)(NSArray*))reply {
  NSUserNotificationCenter* notificationCenter =
      [NSUserNotificationCenter defaultUserNotificationCenter];
  NSMutableArray* notificationIds = [NSMutableArray
      arrayWithCapacity:[[notificationCenter deliveredNotifications] count]];
  for (NSUserNotification* toast in
       [notificationCenter deliveredNotifications]) {
    NSString* candidateProfileId = [toast.userInfo
        objectForKey:notification_constants::kNotificationProfileId];
    BOOL incognitoNotification = [[toast.userInfo
        objectForKey:notification_constants::kNotificationIncognito] boolValue];
    if ([candidateProfileId isEqualToString:profileId] &&
        incognito == incognitoNotification) {
      [notificationIds
          addObject:[toast.userInfo
                        objectForKey:notification_constants::kNotificationId]];
    }
  }
  reply(notificationIds);
}

@end
