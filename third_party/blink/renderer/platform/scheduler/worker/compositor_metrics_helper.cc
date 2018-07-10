// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/compositor_metrics_helper.h"

namespace blink {
namespace scheduler {

CompositorMetricsHelper::CompositorMetricsHelper()
    : MetricsHelper(WebThreadType::kCompositorThread) {}

CompositorMetricsHelper::~CompositorMetricsHelper() {}

void CompositorMetricsHelper::RecordTaskMetrics(
    NonMainThreadTaskQueue* queue,
    const base::sequence_manager::TaskQueue::Task& task,
    const base::sequence_manager::TaskQueue::TaskTiming& task_timing) {
  if (ShouldDiscardTask(queue, task, task_timing))
    return;

  MetricsHelper::RecordCommonTaskMetrics(queue, task, task_timing);
}

}  // namespace scheduler
}  // namespace blink
