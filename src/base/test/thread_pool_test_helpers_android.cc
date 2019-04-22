// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_pool.h"
#include "jni/ThreadPoolTestHelpers_jni.h"

namespace base {

// ThreadPoolTestHelpers is a friend of ThreadPool which grants access to
// SetExecutionFenceEnabled.
class ThreadPoolTestHelpers {
 public:
  // Enables/disables an execution fence that prevents tasks from running.
  static void SetThreadPoolExecutionFenceEnabledForTesting(
      bool execution_fence_enabled);
};

// static
void ThreadPoolTestHelpers::SetThreadPoolExecutionFenceEnabledForTesting(
    bool execution_fence_enabled) {
  ThreadPool::GetInstance()->SetCanRun(!execution_fence_enabled);
}

}  // namespace base

void JNI_ThreadPoolTestHelpers_EnableThreadPoolExecutionForTesting(
    JNIEnv* env) {
  base::ThreadPoolTestHelpers::SetThreadPoolExecutionFenceEnabledForTesting(
      false);
}

void JNI_ThreadPoolTestHelpers_DisableThreadPoolExecutionForTesting(
    JNIEnv* env) {
  base::ThreadPoolTestHelpers::SetThreadPoolExecutionFenceEnabledForTesting(
      true);
}
