// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/thread_scheduler_impl.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/auto_advancing_virtual_time_domain.h"

namespace blink {
namespace scheduler {

void ThreadSchedulerImpl::ExecuteAfterCurrentTask(
    base::OnceClosure on_completion_task) {
  GetOnTaskCompletionCallbacks().push_back(std::move(on_completion_task));
}

void ThreadSchedulerImpl::Shutdown() {
  GetHelper().ResetTimeDomain();
  virtual_time_domain_.reset();
}

base::TimeTicks ThreadSchedulerImpl::MonotonicallyIncreasingVirtualTime() {
  return GetTickClock()->NowTicks();
}

base::TimeTicks ThreadSchedulerImpl::EnableVirtualTime(
    base::Time initial_time) {
  if (virtual_time_domain_)
    return virtual_time_domain_->InitialTicks();
  if (initial_time.is_null())
    initial_time = base::Time::Now();
  base::TimeTicks initial_ticks = GetTickClock()->NowTicks();
  virtual_time_domain_ = std::make_unique<AutoAdvancingVirtualTimeDomain>(
      initial_time, initial_ticks, &GetHelper());
  GetHelper().SetTimeDomain(virtual_time_domain_.get());

  OnVirtualTimeEnabled();

  DCHECK(!virtual_time_stopped_);
  virtual_time_domain_->SetCanAdvanceVirtualTime(true);

  return initial_ticks;
}

void ThreadSchedulerImpl::DisableVirtualTimeForTesting() {
  if (!IsVirtualTimeEnabled())
    return;
  // Reset virtual time and all tasks queues back to their initial state.
  SetVirtualTimeStopped(false);

  // This can only happen during test tear down, in which case there is no need
  // to notify the pages that virtual time was disabled.
  GetHelper().ResetTimeDomain();

  virtual_time_domain_.reset();

  OnVirtualTimeDisabled();
}

bool ThreadSchedulerImpl::VirtualTimeAllowedToAdvance() const {
  DCHECK(!virtual_time_stopped_ || virtual_time_domain_);
  return !virtual_time_stopped_;
}

void ThreadSchedulerImpl::GrantVirtualTimeBudget(
    base::TimeDelta budget,
    base::OnceClosure budget_exhausted_callback) {
  GetVirtualTimeTaskRunner()->PostDelayedTask(
      FROM_HERE, std::move(budget_exhausted_callback), budget);
  // This can shift time forwards if there's a pending MaybeAdvanceVirtualTime,
  // so it's important this is called second.
  virtual_time_domain_->SetVirtualTimeFence(GetTickClock()->NowTicks() +
                                            budget);
}

void ThreadSchedulerImpl::SetVirtualTimePolicy(VirtualTimePolicy policy) {
  DCHECK(IsVirtualTimeEnabled());
  virtual_time_policy_ = policy;
  ApplyVirtualTimePolicy();
}

void ThreadSchedulerImpl::SetMaxVirtualTimeTaskStarvationCount(
    int max_task_starvation_count) {
  DCHECK(IsVirtualTimeEnabled());
  max_virtual_time_task_starvation_count_ = max_task_starvation_count;
  ApplyVirtualTimePolicy();
}

WebScopedVirtualTimePauser
ThreadSchedulerImpl::CreateWebScopedVirtualTimePauser(
    const WTF::String& name,
    WebScopedVirtualTimePauser::VirtualTaskDuration duration) {
  return WebScopedVirtualTimePauser(this, duration, name);
}

bool ThreadSchedulerImpl::IsVirtualTimeEnabled() const {
  return !!virtual_time_domain_;
}

base::TimeTicks ThreadSchedulerImpl::IncrementVirtualTimePauseCount() {
  virtual_time_pause_count_++;
  if (IsVirtualTimeEnabled())
    ApplyVirtualTimePolicy();
  return GetTickClock()->NowTicks();
}

void ThreadSchedulerImpl::DecrementVirtualTimePauseCount() {
  virtual_time_pause_count_--;
  DCHECK_GE(virtual_time_pause_count_, 0);
  if (IsVirtualTimeEnabled())
    ApplyVirtualTimePolicy();
}

void ThreadSchedulerImpl::MaybeAdvanceVirtualTime(
    base::TimeTicks new_virtual_time) {
  if (virtual_time_domain_)
    virtual_time_domain_->MaybeAdvanceVirtualTime(new_virtual_time);
}

AutoAdvancingVirtualTimeDomain* ThreadSchedulerImpl::GetVirtualTimeDomain() {
  return virtual_time_domain_.get();
}

ThreadSchedulerImpl::ThreadSchedulerImpl() = default;
ThreadSchedulerImpl::~ThreadSchedulerImpl() = default;

void ThreadSchedulerImpl::DispatchOnTaskCompletionCallbacks() {
  for (auto& closure : GetOnTaskCompletionCallbacks()) {
    std::move(closure).Run();
  }
  GetOnTaskCompletionCallbacks().clear();
}

namespace {
const char* VirtualTimePolicyToString(
    VirtualTimeController::VirtualTimePolicy virtual_time_policy) {
  switch (virtual_time_policy) {
    case VirtualTimeController::VirtualTimePolicy::kAdvance:
      return "ADVANCE";
    case VirtualTimeController::VirtualTimePolicy::kPause:
      return "PAUSE";
    case VirtualTimeController::VirtualTimePolicy::kDeterministicLoading:
      return "DETERMINISTIC_LOADING";
  }
}
}  // namespace

void ThreadSchedulerImpl::WriteVirtualTimeInfoIntoTrace(
    perfetto::TracedDictionary& dict) const {
  dict.Add("virtual_time_stopped", virtual_time_stopped_);
  dict.Add("virtual_time_pause_count", virtual_time_pause_count_);
  dict.Add("virtual_time_policy",
           VirtualTimePolicyToString(virtual_time_policy_));
  dict.Add("virtual_time", !!virtual_time_domain_);
}

void ThreadSchedulerImpl::SetVirtualTimeStopped(bool virtual_time_stopped) {
  DCHECK(virtual_time_domain_);
  if (virtual_time_stopped_ == virtual_time_stopped)
    return;
  virtual_time_stopped_ = virtual_time_stopped;
  virtual_time_domain_->SetCanAdvanceVirtualTime(!virtual_time_stopped);

  if (virtual_time_stopped)
    OnVirtualTimePaused();
  else
    OnVirtualTimeResumed();
}

void ThreadSchedulerImpl::ApplyVirtualTimePolicy() {
  DCHECK(virtual_time_domain_);
  switch (virtual_time_policy_) {
    case VirtualTimePolicy::kAdvance:
      virtual_time_domain_->SetMaxVirtualTimeTaskStarvationCount(
          GetHelper().IsInNestedRunloop()
              ? 0
              : max_virtual_time_task_starvation_count_);
      virtual_time_domain_->SetVirtualTimeFence(base::TimeTicks());
      SetVirtualTimeStopped(false);
      break;
    case VirtualTimePolicy::kPause:
      virtual_time_domain_->SetMaxVirtualTimeTaskStarvationCount(0);
      virtual_time_domain_->SetVirtualTimeFence(GetTickClock()->NowTicks());
      SetVirtualTimeStopped(true);
      break;
    case VirtualTimePolicy::kDeterministicLoading:
      virtual_time_domain_->SetMaxVirtualTimeTaskStarvationCount(
          GetHelper().IsInNestedRunloop()
              ? 0
              : max_virtual_time_task_starvation_count_);

      // We pause virtual time while the run loop is nested because that implies
      // something modal is happening such as the DevTools debugger pausing the
      // system. We also pause while the renderer is waiting for various
      // asynchronous things e.g. resource load or navigation.
      SetVirtualTimeStopped(virtual_time_pause_count_ != 0 ||
                            GetHelper().IsInNestedRunloop());
      break;
  }
}

void ThreadSchedulerImpl::OnBeginNestedRunLoop() {
  if (IsVirtualTimeEnabled())
    ApplyVirtualTimePolicy();
}

void ThreadSchedulerImpl::OnExitNestedRunLoop() {
  if (IsVirtualTimeEnabled())
    ApplyVirtualTimePolicy();
}

}  // namespace scheduler
}  // namespace blink
