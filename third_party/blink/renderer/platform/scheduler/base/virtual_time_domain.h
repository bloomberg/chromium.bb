// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_VIRTUAL_TIME_DOMAIN_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_VIRTUAL_TIME_DOMAIN_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/renderer/platform/scheduler/base/time_domain.h"

namespace blink {
namespace scheduler {

class PLATFORM_EXPORT VirtualTimeDomain : public TimeDomain {
 public:
  explicit VirtualTimeDomain(base::TimeTicks initial_time_ticks);
  ~VirtualTimeDomain() override;

  // TimeDomain implementation:
  LazyNow CreateLazyNow() const override;
  base::TimeTicks Now() const override;
  base::Optional<base::TimeDelta> DelayTillNextTask(LazyNow* lazy_now) override;
  const char* GetName() const override;

  // Advances this time domain to |now|. NOTE |now| is supposed to be
  // monotonically increasing.  NOTE it's the responsibility of the caller to
  // call TaskQueueManager::MaybeScheduleImmediateWork if needed.
  void AdvanceNowTo(base::TimeTicks now);

 protected:
  void OnRegisterWithTaskQueueManager(
      TaskQueueManagerImpl* task_queue_manager) override;
  void RequestWakeUpAt(base::TimeTicks now, base::TimeTicks run_time) override;
  void CancelWakeUpAt(base::TimeTicks run_time) override;
  void AsValueIntoInternal(
      base::trace_event::TracedValue* state) const override;

  void RequestDoWork();

 private:
  mutable base::Lock lock_;  // Protects |now_ticks_|
  base::TimeTicks now_ticks_;

  TaskQueueManagerImpl* task_queue_manager_;  // NOT OWNED
  base::Closure do_work_closure_;

  DISALLOW_COPY_AND_ASSIGN(VirtualTimeDomain);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_VIRTUAL_TIME_DOMAIN_H_
