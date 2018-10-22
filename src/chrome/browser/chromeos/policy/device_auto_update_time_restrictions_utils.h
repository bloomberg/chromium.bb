// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_AUTO_UPDATE_TIME_RESTRICTIONS_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_AUTO_UPDATE_TIME_RESTRICTIONS_UTILS_H_

#include <vector>

#include "base/optional.h"

namespace base {
class Clock;
}
namespace policy {

class WeeklyTimeInterval;

// Places the intervals for DeviceAutoUpdateTimeRestrictions according to
// CrosSettings into |intervals_out|. Note that the intervals are specified in
// a timezone-agnostic format by the policy, so this function converts them to
// the local timezone. It is thus not advisable to store the returned intervals
// for extended periods (because the local timezone could change). Returns false
// if there are no intervals set or if the conversion to the local timezone was
// unsuccessful.
bool GetDeviceAutoUpdateTimeRestrictionsIntervalsInLocalTimezone(
    base::Clock* clock,
    std::vector<WeeklyTimeInterval>* intervals_out);

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_AUTO_UPDATE_TIME_RESTRICTIONS_UTILS_H_
