// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
#include <unistd.h>
#endif

namespace blink {

ThreadCreationParams::ThreadCreationParams(WebThreadType thread_type)
    : thread_type(thread_type),
      name(GetNameForThreadType(thread_type)),
      frame_or_worker_scheduler(nullptr) {}

ThreadCreationParams& ThreadCreationParams::SetThreadNameForTest(
    const char* thread_name) {
  name = thread_name;
  return *this;
}

ThreadCreationParams& ThreadCreationParams::SetFrameOrWorkerScheduler(
    FrameOrWorkerScheduler* scheduler) {
  frame_or_worker_scheduler = scheduler;
  return *this;
}

#if defined(OS_WIN)
static_assert(sizeof(blink::PlatformThreadId) >= sizeof(DWORD),
              "size of platform thread id is too small");
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
static_assert(sizeof(blink::PlatformThreadId) >= sizeof(pid_t),
              "size of platform thread id is too small");
#else
#error Unexpected platform
#endif

}  // namespace blink
