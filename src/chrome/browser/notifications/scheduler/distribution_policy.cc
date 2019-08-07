// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/distribution_policy.h"

#include "base/logging.h"

namespace notifications {
namespace {

// Evenly distributes notifications to show in morning and evening. Morning
// will have one more to show if the total quota is odd.
class EvenDistributionHigherMorning : public DistributionPolicy {
 public:
  EvenDistributionHigherMorning() = default;
  ~EvenDistributionHigherMorning() override = default;

 private:
  // DistributionPolicy implementation.
  int MaxToShow(SchedulerTaskTime task_start_time, int quota) override {
    DCHECK_GE(quota, 0);
    switch (task_start_time) {
      case SchedulerTaskTime::kUnknown:
        NOTREACHED();
        return quota;
      case SchedulerTaskTime::kMorning:
        return quota / 2 + quota % 2;
      case SchedulerTaskTime::kEvening:
        // The task running in the evening should flush all the remaining
        // notifications.
        return quota;
    }
  }

  DISALLOW_COPY_AND_ASSIGN(EvenDistributionHigherMorning);
};

}  // namespace

// static
std::unique_ptr<DistributionPolicy> DistributionPolicy::Create() {
  return std::make_unique<EvenDistributionHigherMorning>();
}

}  // namespace notifications
