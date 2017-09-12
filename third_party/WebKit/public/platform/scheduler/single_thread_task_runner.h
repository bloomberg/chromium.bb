// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_SINGLE_THREAD_TASK_RUNNER_H_
#define THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_SINGLE_THREAD_TASK_RUNNER_H_

#include "base/single_thread_task_runner.h"

// Helper header for passing references to task queues across blink/content
// boundary. It's explicitly mandated to have a strong reference to
// a SingleThreadTaskRunner inside Blink.
//
// Please use WebTaskRunner inside Blink wherever is possible and consult
// scheduler-dev@chromium.org if you want to use this.

namespace blink {

using SingleThreadTaskRunnerRefPtr =
    scoped_refptr<base::SingleThreadTaskRunner>;

}  // namespace blink

#endif
