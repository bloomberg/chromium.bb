// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_THREAD_POOL_THREAD_GROUP_NATIVE_MAC_H_
#define BASE_TASK_THREAD_POOL_THREAD_GROUP_NATIVE_MAC_H_

#include <dispatch/dispatch.h>

#include "base/base_export.h"
#include "base/mac/scoped_dispatch_object.h"
#include "base/task/thread_pool/thread_group_native.h"
#include "base/threading/platform_thread.h"

namespace base {
namespace internal {

// A ThreadGroup implementation backed by libdispatch.
//
// libdispatch official documentation:
// https://developer.apple.com/documentation/dispatch
//
// Guides:
// https://apple.github.io/swift-corelibs-libdispatch/tutorial/
// https://developer.apple.com/library/archive/documentation/General/Conceptual/ConcurrencyProgrammingGuide/OperationQueues/OperationQueues.html
class BASE_EXPORT ThreadGroupNativeMac : public ThreadGroupNative {
 public:
  // `io_thread_task_runner` is used to setup FileDescriptorWatcher on worker
  // threads. `io_thread_task_runner` must refer to a Thread with
  // MessgaePumpType::IO
  ThreadGroupNativeMac(
      ThreadPriority priority_hint,
      scoped_refptr<SingleThreadTaskRunner> io_thread_task_runner,
      TrackedRef<TaskTracker> task_tracker,
      TrackedRef<Delegate> delegate,
      ThreadGroup* predecessor_thread_group = nullptr);

  ThreadGroupNativeMac(const ThreadGroupNativeMac&) = delete;
  ThreadGroupNativeMac& operator=(const ThreadGroupNativeMac&) = delete;
  ~ThreadGroupNativeMac() override;

 private:
  // ThreadGroupNative:
  void JoinImpl() override;
  void StartImpl() override;
  void SubmitWork() override;

  const ThreadPriority priority_hint_;

  // Dispatch queue on which work is scheduled. Backed by a shared thread pool
  // managed by libdispatch.
  ScopedDispatchObject<dispatch_queue_t> queue_;

  // Dispatch group to enable synchronization.
  ScopedDispatchObject<dispatch_group_t> group_;

  // Service thread task runner.
  scoped_refptr<SingleThreadTaskRunner> io_thread_task_runner_;
};

using ThreadGroupNativeImpl = ThreadGroupNativeMac;

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_THREAD_POOL_THREAD_GROUP_NATIVE_MAC_H_
