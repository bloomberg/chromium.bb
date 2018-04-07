// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/base/virtual_time_domain.h"

#include "base/bind.h"
#include "third_party/blink/renderer/platform/scheduler/base/task_queue_impl.h"
#include "third_party/blink/renderer/platform/scheduler/base/task_queue_manager_impl.h"

namespace blink {
namespace scheduler {

VirtualTimeDomain::VirtualTimeDomain(base::TimeTicks initial_time_ticks)
    : now_ticks_(initial_time_ticks), task_queue_manager_(nullptr) {}

VirtualTimeDomain::~VirtualTimeDomain() = default;

void VirtualTimeDomain::OnRegisterWithTaskQueueManager(
    TaskQueueManagerImpl* task_queue_manager) {
  task_queue_manager_ = task_queue_manager;
  DCHECK(task_queue_manager_);
}

LazyNow VirtualTimeDomain::CreateLazyNow() const {
  base::AutoLock lock(lock_);
  return LazyNow(now_ticks_);
}

base::TimeTicks VirtualTimeDomain::Now() const {
  base::AutoLock lock(lock_);
  return now_ticks_;
}

void VirtualTimeDomain::RequestWakeUpAt(base::TimeTicks now,
                                        base::TimeTicks run_time) {
  // We don't need to do anything here because the caller of AdvanceTo is
  // responsible for calling TaskQueueManagerImpl::MaybeScheduleImmediateWork if
  // needed.
}

void VirtualTimeDomain::CancelWakeUpAt(base::TimeTicks run_time) {
  // We ignore this because RequestWakeUpAt is a NOP.
}

base::Optional<base::TimeDelta> VirtualTimeDomain::DelayTillNextTask(
    LazyNow* lazy_now) {
  return base::nullopt;
}

void VirtualTimeDomain::AsValueIntoInternal(
    base::trace_event::TracedValue* state) const {}

void VirtualTimeDomain::AdvanceNowTo(base::TimeTicks now) {
  base::AutoLock lock(lock_);
  DCHECK_GE(now, now_ticks_);
  now_ticks_ = now;
}

void VirtualTimeDomain::RequestDoWork() {
  task_queue_manager_->MaybeScheduleImmediateWork(FROM_HERE);
}

const char* VirtualTimeDomain::GetName() const {
  return "VirtualTimeDomain";
}

}  // namespace scheduler
}  // namespace blink
