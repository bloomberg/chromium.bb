// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/budget_pool.h"

#include <cstdint>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "platform/WebFrameScheduler.h"
#include "platform/scheduler/base/real_time_domain.h"
#include "platform/scheduler/child/scheduler_tqm_delegate.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/renderer/task_queue_throttler.h"
#include "platform/scheduler/renderer/throttled_time_domain.h"
#include "platform/scheduler/renderer/web_frame_scheduler_impl.h"

namespace blink {
namespace scheduler {

namespace {

std::string PointerToId(void* pointer) {
  return base::StringPrintf(
      "0x%" PRIx64,
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer)));
}

}  // namespace

BudgetPool::~BudgetPool() {}

CPUTimeBudgetPool::CPUTimeBudgetPool(
    const char* name,
    BudgetPoolController* budget_pool_controller,
    base::TimeTicks now)
    : name_(name),
      budget_pool_controller_(budget_pool_controller),
      last_checkpoint_(now),
      cpu_percentage_(1),
      is_enabled_(true) {}

CPUTimeBudgetPool::~CPUTimeBudgetPool() {}

void CPUTimeBudgetPool::SetMaxBudgetLevel(
    base::TimeTicks now,
    base::Optional<base::TimeDelta> max_budget_level) {
  Advance(now);
  max_budget_level_ = max_budget_level;
  EnforceBudgetLevelRestrictions();
}

void CPUTimeBudgetPool::SetMaxThrottlingDelay(
    base::TimeTicks now,
    base::Optional<base::TimeDelta> max_throttling_delay) {
  Advance(now);
  max_throttling_delay_ = max_throttling_delay;
  EnforceBudgetLevelRestrictions();
}

void CPUTimeBudgetPool::SetMinBudgetLevelToRun(
    base::TimeTicks now,
    base::TimeDelta min_budget_level_to_run) {
  Advance(now);
  min_budget_level_to_run_ = min_budget_level_to_run;
}

void CPUTimeBudgetPool::SetTimeBudgetRecoveryRate(base::TimeTicks now,
                                                  double cpu_percentage) {
  Advance(now);
  cpu_percentage_ = cpu_percentage;
  EnforceBudgetLevelRestrictions();
}

void CPUTimeBudgetPool::AddQueue(base::TimeTicks now, TaskQueue* queue) {
  budget_pool_controller_->AddQueueToBudgetPool(queue, this);
  associated_task_queues_.insert(queue);

  if (!is_enabled_ || !budget_pool_controller_->IsThrottled(queue))
    return;

  budget_pool_controller_->BlockQueue(now, queue);
}

void CPUTimeBudgetPool::RemoveQueue(base::TimeTicks now, TaskQueue* queue) {
  budget_pool_controller_->RemoveQueueFromBudgetPool(queue, this);
  associated_task_queues_.erase(queue);

  if (!is_enabled_ || !budget_pool_controller_->IsThrottled(queue))
    return;

  budget_pool_controller_->UnblockQueue(now, queue);
}

void CPUTimeBudgetPool::EnableThrottling(LazyNow* lazy_now) {
  if (is_enabled_)
    return;
  is_enabled_ = true;

  TRACE_EVENT0("renderer.scheduler", "CPUTimeBudgetPool_EnableThrottling");

  BlockThrottledQueues(lazy_now->Now());
}

void CPUTimeBudgetPool::DisableThrottling(LazyNow* lazy_now) {
  if (!is_enabled_)
    return;
  is_enabled_ = false;

  TRACE_EVENT0("renderer.scheduler", "CPUTimeBudgetPool_DisableThrottling");

  for (TaskQueue* queue : associated_task_queues_) {
    if (!budget_pool_controller_->IsThrottled(queue))
      continue;

    budget_pool_controller_->UnblockQueue(lazy_now->Now(), queue);
  }

  // TODO(altimin): We need to disable TimeBudgetQueues here or they will
  // regenerate extra time budget when they are disabled.
}

bool CPUTimeBudgetPool::IsThrottlingEnabled() const {
  return is_enabled_;
}

void CPUTimeBudgetPool::GrantAdditionalBudget(base::TimeTicks now,
                                              base::TimeDelta budget_level) {
  Advance(now);
  current_budget_level_ += budget_level;
  EnforceBudgetLevelRestrictions();
}

void CPUTimeBudgetPool::SetReportingCallback(
    base::Callback<void(base::TimeDelta)> reporting_callback) {
  reporting_callback_ = reporting_callback;
}

void CPUTimeBudgetPool::Close() {
  DCHECK_EQ(0u, associated_task_queues_.size());

  budget_pool_controller_->UnregisterBudgetPool(this);
}

bool CPUTimeBudgetPool::HasEnoughBudgetToRun(base::TimeTicks now) {
  return now >= GetNextAllowedRunTime();
}

base::TimeTicks CPUTimeBudgetPool::GetNextAllowedRunTime() {
  if (!is_enabled_ || current_budget_level_.InMicroseconds() >= 0) {
    return last_checkpoint_;
  } else {
    // Subtract because current_budget is negative.
    return last_checkpoint_ +
           (-current_budget_level_ + min_budget_level_to_run_) /
               cpu_percentage_;
  }
}

void CPUTimeBudgetPool::RecordTaskRunTime(base::TimeTicks start_time,
                                          base::TimeTicks end_time) {
  DCHECK_LE(start_time, end_time);
  Advance(end_time);
  if (is_enabled_) {
    base::TimeDelta old_budget_level = current_budget_level_;
    current_budget_level_ -= (end_time - start_time);
    EnforceBudgetLevelRestrictions();

    if (!reporting_callback_.is_null() && old_budget_level.InSecondsF() > 0 &&
        current_budget_level_.InSecondsF() < 0) {
      reporting_callback_.Run(-current_budget_level_ / cpu_percentage_);
    }
  }
}

const char* CPUTimeBudgetPool::Name() const {
  return name_;
}

void CPUTimeBudgetPool::AsValueInto(base::trace_event::TracedValue* state,
                                    base::TimeTicks now) const {
  state->BeginDictionary(name_);

  state->SetString("name", name_);
  state->SetDouble("time_budget", cpu_percentage_);
  state->SetDouble("time_budget_level_in_seconds",
                   current_budget_level_.InSecondsF());
  state->SetDouble("last_checkpoint_seconds_ago",
                   (now - last_checkpoint_).InSecondsF());
  state->SetBoolean("is_enabled", is_enabled_);
  state->SetDouble("min_budget_level_to_run_in_seconds",
                   min_budget_level_to_run_.InSecondsF());

  if (max_throttling_delay_) {
    state->SetDouble("max_throttling_delay_in_seconds",
                     max_throttling_delay_.value().InSecondsF());
  }
  if (max_budget_level_) {
    state->SetDouble("max_budget_level_in_seconds",
                     max_budget_level_.value().InSecondsF());
  }

  state->BeginArray("task_queues");
  for (TaskQueue* queue : associated_task_queues_) {
    state->AppendString(PointerToId(queue));
  }
  state->EndArray();

  state->EndDictionary();
}

void CPUTimeBudgetPool::Advance(base::TimeTicks now) {
  if (now > last_checkpoint_) {
    if (is_enabled_) {
      current_budget_level_ += cpu_percentage_ * (now - last_checkpoint_);
      EnforceBudgetLevelRestrictions();
    }
    last_checkpoint_ = now;
  }
}

void CPUTimeBudgetPool::BlockThrottledQueues(base::TimeTicks now) {
  for (TaskQueue* queue : associated_task_queues_)
    budget_pool_controller_->BlockQueue(now, queue);
}

void CPUTimeBudgetPool::EnforceBudgetLevelRestrictions() {
  if (max_budget_level_) {
    current_budget_level_ =
        std::min(current_budget_level_, max_budget_level_.value());
  }
  if (max_throttling_delay_) {
    // Current budget level may be negative.
    current_budget_level_ =
        std::max(current_budget_level_,
                 -max_throttling_delay_.value() * cpu_percentage_);
  }
}

}  // namespace scheduler
}  // namespace blink
