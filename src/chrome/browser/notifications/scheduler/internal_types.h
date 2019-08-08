// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_TYPES_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_TYPES_H_

namespace notifications {

// Enum to describe the time to process scheduled notification data.
enum class SchedulerTaskTime {
  // The system is started from normal user launch or other background
  // tasks.
  kUnknown = 0,

  // Background task runs in the morning.
  kMorning = 1,

  // Background task runs in the evening.
  kEvening = 2,
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_TYPES_H_
