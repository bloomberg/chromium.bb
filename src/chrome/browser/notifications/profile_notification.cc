// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/profile_notification.h"

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "components/account_id/account_id.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"

// static
std::string ProfileNotification::GetProfileNotificationId(
    const std::string& delegate_id,
    ProfileID profile_id) {
  return base::StringPrintf("notification-ui-manager#%p#%s",
                            profile_id,  // Each profile has its unique instance
                                         // including incognito profile.
                            delegate_id.c_str());
}

ProfileNotification::ProfileNotification(
    Profile* profile,
    const message_center::Notification& notification,
    NotificationHandler::Type type)
    : profile_(profile),
      profile_id_(NotificationUIManager::GetProfileID(profile)),
      notification_(
          // Uses Notification's copy constructor to assign the message center
          // id, which should be unique for every profile + Notification pair.
          GetProfileNotificationId(
              notification.id(),
              NotificationUIManager::GetProfileID(profile)),
          notification),
      original_id_(notification.id()),
      type_(type) {
#if defined(OS_CHROMEOS)
  if (profile_) {
    notification_.set_profile_id(
        multi_user_util::GetAccountIdFromProfile(profile).GetUserEmail());
  }
#else
  // This ScopedKeepAlive prevents the browser process from shutting down when
  // the last browser window is closed and there are open notifications. It's
  // not used on Chrome OS as closing the last browser window never shuts down
  // the process.
  keep_alive_ = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::NOTIFICATION, KeepAliveRestartOption::DISABLED);
#endif
}

ProfileNotification::~ProfileNotification() {}
