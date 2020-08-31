// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_INFO_H_
#define CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_INFO_H_

#include "base/strings/string16.h"

namespace updates {

// Contains data used to schedule an update notification.
struct UpdateNotificationInfo {
  UpdateNotificationInfo();
  UpdateNotificationInfo(const UpdateNotificationInfo& other);
  bool operator==(const UpdateNotificationInfo& other) const;
  ~UpdateNotificationInfo();

  // The title of the notification.
  base::string16 title;

  // The body text of the notification.
  base::string16 message;

  // Update state enum value. Align with |UpdateState| in
  // UpdateStatusProvider.java
  int state;

  // Should show the notification right away.
  bool should_show_immediately;
};

}  // namespace updates

#endif  // CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_INFO_H_
