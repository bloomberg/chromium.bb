// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_REAL_TIME_DOMAIN_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_REAL_TIME_DOMAIN_H_

#include <set>

#include "base/macros.h"
#include "platform/PlatformExport.h"
#include "platform/scheduler/base/time_domain.h"

namespace blink {
namespace scheduler {

class PLATFORM_EXPORT RealTimeDomain : public TimeDomain {
 public:
  RealTimeDomain();
  ~RealTimeDomain() override;

  // TimeDomain implementation:
  LazyNow CreateLazyNow() const override;
  base::TimeTicks Now() const override;
  base::Optional<base::TimeDelta> DelayTillNextTask(LazyNow* lazy_now) override;
  const char* GetName() const override;

 protected:
  void OnRegisterWithTaskQueueManager(
      TaskQueueManager* task_queue_manager) override;
  void RequestWakeUpAt(base::TimeTicks now, base::TimeTicks run_time) override;
  void CancelWakeUpAt(base::TimeTicks run_time) override;
  void AsValueIntoInternal(
      base::trace_event::TracedValue* state) const override;

 private:
  TaskQueueManager* task_queue_manager_;  // NOT OWNED

  DISALLOW_COPY_AND_ASSIGN(RealTimeDomain);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_REAL_TIME_DOMAIN_H_
