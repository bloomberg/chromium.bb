// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/base/virtual_time_domain.h"

#include "base/bind.h"
#include "third_party/blink/renderer/platform/scheduler/base/sequence_manager_impl.h"
#include "third_party/blink/renderer/platform/scheduler/base/task_queue_impl_forward.h"

namespace base {
namespace sequence_manager {

VirtualTimeDomain::VirtualTimeDomain(TimeTicks initial_time_ticks)
    : now_ticks_(initial_time_ticks) {}

VirtualTimeDomain::~VirtualTimeDomain() = default;

LazyNow VirtualTimeDomain::CreateLazyNow() const {
  AutoLock lock(lock_);
  return LazyNow(now_ticks_);
}

TimeTicks VirtualTimeDomain::Now() const {
  AutoLock lock(lock_);
  return now_ticks_;
}

void VirtualTimeDomain::SetNextDelayedDoWork(LazyNow* lazy_now,
                                             TimeTicks run_time) {
  // Caller of AdvanceTo is responsible for telling SequenceManager to schedule
  // immediate work if needed.
}

Optional<TimeDelta> VirtualTimeDomain::DelayTillNextTask(LazyNow* lazy_now) {
  return nullopt;
}

void VirtualTimeDomain::AdvanceNowTo(TimeTicks now) {
  AutoLock lock(lock_);
  DCHECK_GE(now, now_ticks_);
  now_ticks_ = now;
}

const char* VirtualTimeDomain::GetName() const {
  return "VirtualTimeDomain";
}

}  // namespace sequence_manager
}  // namespace base
