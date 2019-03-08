// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/compositor_thread.h"

#include "base/task/sequence_manager/sequence_manager.h"
#include "third_party/blink/renderer/platform/scheduler/worker/compositor_thread_scheduler.h"

namespace blink {
namespace scheduler {

CompositorThread::CompositorThread(const ThreadCreationParams& params)
    : WorkerThread(params) {}

CompositorThread::~CompositorThread() = default;

std::unique_ptr<NonMainThreadSchedulerImpl>
CompositorThread::CreateNonMainThreadScheduler() {
  return std::make_unique<CompositorThreadScheduler>(
      base::sequence_manager::CreateSequenceManagerOnCurrentThread(
          base::sequence_manager::SequenceManager::Settings{
              base::MessageLoop::TYPE_DEFAULT,
              /*randomised_sampling_enabled=*/true}));
}

}  // namespace scheduler
}  // namespace blink
