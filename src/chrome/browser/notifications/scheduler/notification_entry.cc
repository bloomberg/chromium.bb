// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/notification_entry.h"

#include <utility>

namespace notifications {

NotificationEntry::NotificationEntry()
    : NotificationEntry(SchedulerClientType::kUnknown, std::string()) {}

NotificationEntry::NotificationEntry(SchedulerClientType type,
                                     const std::string& guid)
    : type(type), guid(guid), create_time(base::Time::Now()) {}

NotificationEntry::NotificationEntry(const NotificationEntry& other) = default;

bool NotificationEntry::operator==(const NotificationEntry& other) const {
  return type == other.type && guid == other.guid &&
         create_time == other.create_time &&
         notification_data == other.notification_data &&
         schedule_params == other.schedule_params;
}

NotificationEntry::~NotificationEntry() = default;

}  // namespace notifications
