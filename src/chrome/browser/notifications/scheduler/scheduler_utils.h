// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_SCHEDULER_UTILS_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_SCHEDULER_UTILS_H_

#include "base/time/time.h"

namespace notifications {

// Retrieves the time stamp of a certain hour at a certain day from today.
// |hour| must be in the range of [0, 23].
// |today| is a timestamp to define today, usually caller can directly pass in
// the current system time.
// |day_delta| is the different between the output date and today.
// Returns false if the conversion is failed.
bool ToLocalHour(int hour,
                 const base::Time& today,
                 int day_delta,
                 base::Time* out);

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_SCHEDULER_UTILS_H_
