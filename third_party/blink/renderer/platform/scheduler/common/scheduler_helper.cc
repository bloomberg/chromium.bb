// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/scheduler_helper.h"

#include <utility>

#include "base/task/sequence_manager/task_queue.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "third_party/blink/renderer/platform/scheduler/child/task_queue_with_task_type.h"

namespace blink {
namespace scheduler {

using base::sequence_manager::TaskQueue;
using base::sequence_manager::SequenceManager;
using base::sequence_manager::TaskTimeObserver;
using base::sequence_manager::TimeDomain;

SchedulerHelper::SchedulerHelper(
    std::unique_ptr<SequenceManager> sequence_manager)
    : sequence_manager_(std::move(sequence_manager)), observer_(nullptr) {
  sequence_manager_->SetWorkBatchSize(4);
}

void SchedulerHelper::InitDefaultQueues(
    scoped_refptr<TaskQueue> default_task_queue,
    scoped_refptr<TaskQueue> control_task_queue,
    TaskType default_task_type) {
  control_task_queue->SetQueuePriority(TaskQueue::kControlPriority);

  default_task_runner_ =
      TaskQueueWithTaskType::Create(default_task_queue, default_task_type);

  DCHECK(sequence_manager_);
  sequence_manager_->SetDefaultTaskRunner(default_task_runner_);
}

SchedulerHelper::~SchedulerHelper() {
  Shutdown();
}

void SchedulerHelper::Shutdown() {
  CheckOnValidThread();
  if (!sequence_manager_)
    return;
  sequence_manager_->SetObserver(nullptr);
  sequence_manager_.reset();
}

scoped_refptr<base::SingleThreadTaskRunner>
SchedulerHelper::DefaultTaskRunner() {
  return default_task_runner_;
}

void SchedulerHelper::SetWorkBatchSizeForTesting(size_t work_batch_size) {
  CheckOnValidThread();
  DCHECK(sequence_manager_.get());
  sequence_manager_->SetWorkBatchSize(work_batch_size);
}

bool SchedulerHelper::GetAndClearSystemIsQuiescentBit() {
  CheckOnValidThread();
  DCHECK(sequence_manager_.get());
  return sequence_manager_->GetAndClearSystemIsQuiescentBit();
}

void SchedulerHelper::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  CheckOnValidThread();
  if (sequence_manager_)
    sequence_manager_->AddTaskObserver(task_observer);
}

void SchedulerHelper::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  CheckOnValidThread();
  if (sequence_manager_)
    sequence_manager_->RemoveTaskObserver(task_observer);
}

void SchedulerHelper::AddTaskTimeObserver(
    TaskTimeObserver* task_time_observer) {
  if (sequence_manager_)
    sequence_manager_->AddTaskTimeObserver(task_time_observer);
}

void SchedulerHelper::RemoveTaskTimeObserver(
    TaskTimeObserver* task_time_observer) {
  if (sequence_manager_)
    sequence_manager_->RemoveTaskTimeObserver(task_time_observer);
}

void SchedulerHelper::SetObserver(Observer* observer) {
  CheckOnValidThread();
  observer_ = observer;
  DCHECK(sequence_manager_);
  sequence_manager_->SetObserver(this);
}

void SchedulerHelper::SweepCanceledDelayedTasks() {
  CheckOnValidThread();
  DCHECK(sequence_manager_);
  sequence_manager_->SweepCanceledDelayedTasks();
}

TimeDomain* SchedulerHelper::real_time_domain() const {
  CheckOnValidThread();
  DCHECK(sequence_manager_);
  return sequence_manager_->GetRealTimeDomain();
}

void SchedulerHelper::RegisterTimeDomain(TimeDomain* time_domain) {
  CheckOnValidThread();
  DCHECK(sequence_manager_);
  sequence_manager_->RegisterTimeDomain(time_domain);
}

void SchedulerHelper::UnregisterTimeDomain(TimeDomain* time_domain) {
  CheckOnValidThread();
  if (sequence_manager_)
    sequence_manager_->UnregisterTimeDomain(time_domain);
}

void SchedulerHelper::OnBeginNestedRunLoop() {
  if (observer_)
    observer_->OnBeginNestedRunLoop();
}

void SchedulerHelper::OnExitNestedRunLoop() {
  if (observer_)
    observer_->OnExitNestedRunLoop();
}

const base::TickClock* SchedulerHelper::GetClock() const {
  return sequence_manager_->GetTickClock();
}

base::TimeTicks SchedulerHelper::NowTicks() const {
  if (sequence_manager_)
    return sequence_manager_->NowTicks();
  // We may need current time for tracing when shutting down worker thread.
  return base::TimeTicks::Now();
}

double SchedulerHelper::GetSamplingRateForRecordingCPUTime() const {
  if (sequence_manager_) {
    return sequence_manager_->GetMetricRecordingSettings()
        .task_sampling_rate_for_recording_cpu_time;
  }
  return 0;
}

bool SchedulerHelper::HasCPUTimingForEachTask() const {
  if (sequence_manager_) {
    return sequence_manager_->GetMetricRecordingSettings()
        .records_cpu_time_for_each_task;
  }
  return false;
}

}  // namespace scheduler
}  // namespace blink
