// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/compositor_thread_event_queue.h"

namespace ui {

CompositorThreadEventQueue::CompositorThreadEventQueue() {}

CompositorThreadEventQueue::~CompositorThreadEventQueue() {}

// TODO(chongz): Support coalescing events across interleaved boundaries.
// https://crbug.com/661601
void CompositorThreadEventQueue::Queue(std::unique_ptr<EventWithCallback> event,
                                       base::TimeTicks timestamp_now) {
  if (!queue_.empty() && queue_.back()->CanCoalesceWith(*event)) {
    queue_.back()->CoalesceWith(event.get(), timestamp_now);
    return;
  }

  queue_.emplace_back(std::move(event));
}

std::unique_ptr<EventWithCallback> CompositorThreadEventQueue::Pop() {
  std::unique_ptr<EventWithCallback> result;
  if (!queue_.empty()) {
    result.reset(queue_.front().release());
    queue_.pop_front();
  }
  return result;
}

}  // namespace ui
