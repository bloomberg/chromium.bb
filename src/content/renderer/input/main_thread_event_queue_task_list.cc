// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/main_thread_event_queue_task_list.h"

#include <utility>

namespace content {

MainThreadEventQueueTaskList::MainThreadEventQueueTaskList() {}

MainThreadEventQueueTaskList::~MainThreadEventQueueTaskList() {}

MainThreadEventQueueTaskList::EnqueueResult
MainThreadEventQueueTaskList::Enqueue(
    std::unique_ptr<MainThreadEventQueueTask> event) {
  for (auto last_event_iter = queue_.rbegin(); last_event_iter != queue_.rend();
       ++last_event_iter) {
    switch ((*last_event_iter)->FilterNewEvent(event.get())) {
      case MainThreadEventQueueTask::FilterResult::CoalescedEvent:
        return EnqueueResult::kCoalesced;
      case MainThreadEventQueueTask::FilterResult::StopIterating:
        break;
      case MainThreadEventQueueTask::FilterResult::KeepIterating:
        continue;
    }
    break;
  }
  queue_.emplace_back(std::move(event));
  return EnqueueResult::kEnqueued;
}

std::unique_ptr<MainThreadEventQueueTask> MainThreadEventQueueTaskList::Pop() {
  std::unique_ptr<MainThreadEventQueueTask> result;
  if (!queue_.empty()) {
    result.reset(queue_.front().release());
    queue_.pop_front();
  }
  return result;
}

std::unique_ptr<MainThreadEventQueueTask> MainThreadEventQueueTaskList::remove(
    size_t pos) {
  std::unique_ptr<MainThreadEventQueueTask> result;
  if (!queue_.empty()) {
    result.reset(queue_.at(pos).release());
    queue_.erase(queue_.begin() + pos);
  }
  return result;
}

}  // namespace
