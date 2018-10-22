// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_DRIVE_NOTIFICATION_OBSERVER_H_
#define COMPONENTS_DRIVE_DRIVE_NOTIFICATION_OBSERVER_H_

#include <set>
#include <string>

namespace drive {

// Interface for classes which need to know when to check Google Drive for
// updates.
class DriveNotificationObserver {
 public:
  // Called when a notification from Google Drive is received. |ids| is the set
  // of objects that raised the notification, either a team drive id or empty
  // string to represent the users default corpus.
  virtual void OnNotificationReceived(const std::set<std::string>& ids) = 0;

  // Called when there has not been a recent notification from Google Drive and
  // the timer used for polling Google Drive fired.
  virtual void OnNotificationTimerFired() = 0;

  // Called when XMPP-based push notification is enabled or disabled.
  virtual void OnPushNotificationEnabled(bool enabled) {}

 protected:
  virtual ~DriveNotificationObserver() = default;
};

}  // namespace drive

#endif  // COMPONENTS_DRIVE_DRIVE_NOTIFICATION_OBSERVER_H_
