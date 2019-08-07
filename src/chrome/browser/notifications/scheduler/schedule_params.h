// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_SCHEDULE_PARAMS_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_SCHEDULE_PARAMS_H_

namespace notifications {

// Specifies when to show the scheduled notification, and throttling details.
struct ScheduleParams {
  enum class Priority {
    // Notification may be delivered if picked by display decision layer. Most
    // notification types should use this priority.
    kLow,
    // Notification may be delivered if picked by display decision layer. Has
    // higher priority to pick as the next notification to deliver. Should not
    // be used by feature frequently send notifications.
    kHigh,
    // No notification throttling logic is applied, every notification scheduled
    // will be delivered.
    kNoThrottle,
  };

  ScheduleParams();
  bool operator==(const ScheduleParams& other) const;
  ~ScheduleParams();

  Priority priority;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_SCHEDULE_PARAMS_H_
