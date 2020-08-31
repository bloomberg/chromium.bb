// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_SCHEDULING_H_
#define COMPONENTS_FEED_CORE_V2_SCHEDULING_H_

#include <vector>

#include "base/time/time.h"
#include "components/feed/core/v2/enums.h"

namespace base {
class Value;
}
namespace feed {
constexpr base::TimeDelta kSuppressRefreshDuration =
    base::TimeDelta::FromMinutes(30);

// A schedule for making Feed refresh requests.
// |anchor_time| + |refresh_offsets[i]| is the time each fetch should be made.
struct RequestSchedule {
  RequestSchedule();
  ~RequestSchedule();
  RequestSchedule(const RequestSchedule&);
  RequestSchedule& operator=(const RequestSchedule&);

  base::Time anchor_time;
  std::vector<base::TimeDelta> refresh_offsets;
};

base::Value RequestScheduleToValue(const RequestSchedule&);
RequestSchedule RequestScheduleFromValue(const base::Value&);
// Given a schedule, returns the next time a request should be made.
// Updates |schedule| accordingly. If |schedule| has no fetches remaining,
// returns a scheduled time using |Config::default_background_refresh_interval|.
base::Time NextScheduledRequestTime(base::Time now, RequestSchedule* schedule);

// Returns whether we should wait for new content before showing stream content.
bool ShouldWaitForNewContent(bool has_content, base::TimeDelta content_age);
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_SCHEDULING_H_
