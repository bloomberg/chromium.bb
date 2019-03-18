// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/thread_scheduler_impl.h"

#include "third_party/blink/renderer/platform/scheduler/common/scheduler_helper.h"

namespace blink {
namespace scheduler {

ThreadSchedulerImpl::ThreadSchedulerImpl() {}

ThreadSchedulerImpl::~ThreadSchedulerImpl() = default;

scoped_refptr<base::SingleThreadTaskRunner>
ThreadSchedulerImpl::DeprecatedDefaultTaskRunner() {
  return DefaultTaskRunner();
}

}  // namespace scheduler
}  // namespace blink
