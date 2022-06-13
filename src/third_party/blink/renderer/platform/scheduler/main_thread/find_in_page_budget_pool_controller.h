// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FIND_IN_PAGE_BUDGET_POOL_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FIND_IN_PAGE_BUDGET_POOL_CONTROLLER_H_

#include "base/task/sequence_manager/task_queue.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/cpu_time_budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"

namespace blink {
namespace scheduler {

using TaskQueue = base::sequence_manager::TaskQueue;
using QueuePriority = base::sequence_manager::TaskQueue::QueuePriority;

class CPUTimeBudgetPool;
class MainThreadSchedulerImpl;

class PLATFORM_EXPORT FindInPageBudgetPoolController {
 public:
  static constexpr auto kFindInPageBudgetNotExhaustedPriority =
      QueuePriority::kVeryHighPriority;
  static constexpr auto kFindInPageBudgetExhaustedPriority =
      QueuePriority::kNormalPriority;

  explicit FindInPageBudgetPoolController(MainThreadSchedulerImpl* scheduler);
  ~FindInPageBudgetPoolController();

  void OnTaskCompleted(MainThreadTaskQueue* queue,
                       TaskQueue::TaskTiming* task_timing);

  QueuePriority CurrentTaskPriority() { return task_priority_; }

 private:
  MainThreadSchedulerImpl* scheduler_;  // Not owned.
  std::unique_ptr<CPUTimeBudgetPool> find_in_page_budget_pool_;
  QueuePriority task_priority_;
  const bool best_effort_budget_experiment_enabled_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FIND_IN_PAGE_BUDGET_POOL_CONTROLLER_H_
