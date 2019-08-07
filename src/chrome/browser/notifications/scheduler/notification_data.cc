// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/notification_data.h"

namespace notifications {

NotificationData::NotificationData() = default;

NotificationData::NotificationData(const NotificationData& other) = default;

bool NotificationData::operator==(const NotificationData& other) const {
  return id == other.id && title == other.title && message == other.message &&
         icon_uuid == other.icon_uuid && url == other.url;
}

NotificationData::~NotificationData() = default;

}  // namespace notifications
