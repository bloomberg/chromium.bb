// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHILD_WORKER_TASK_RUNNER_H_
#define WEBKIT_CHILD_WORKER_TASK_RUNNER_H_

#include <map>

#include "base/atomic_sequence_num.h"
#include "base/callback_forward.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_local.h"
#include "third_party/WebKit/public/platform/WebWorkerRunLoop.h"
#include "webkit/child/webkit_child_export.h"

namespace webkit_glue {

class WEBKIT_CHILD_EXPORT WorkerTaskRunner {
 public:
  WorkerTaskRunner();

  bool PostTask(int id, const base::Closure& task);
  int PostTaskToAllThreads(const base::Closure& task);
  int CurrentWorkerId();
  static WorkerTaskRunner* Instance();

  class WEBKIT_CHILD_EXPORT Observer {
   public:
    virtual ~Observer() {}
    virtual void OnWorkerRunLoopStopped() = 0;
  };
  // Add/Remove an observer that will get notified when the current worker run
  // loop is stopping. This observer will not get notified when other threads
  // are stopping.  It's only valid to call these on a worker thread.
  void AddStopObserver(Observer* observer);
  void RemoveStopObserver(Observer* observer);

 private:
  friend class WebKitPlatformSupportChildImpl;
  friend class WorkerTaskRunnerTest;

  typedef std::map<int, WebKit::WebWorkerRunLoop> IDToLoopMap;

  ~WorkerTaskRunner();
  void OnWorkerRunLoopStarted(const WebKit::WebWorkerRunLoop& loop);
  void OnWorkerRunLoopStopped(const WebKit::WebWorkerRunLoop& loop);

  struct ThreadLocalState;
  base::ThreadLocalPointer<ThreadLocalState> current_tls_;

  base::AtomicSequenceNumber id_sequence_;
  IDToLoopMap loop_map_;
  base::Lock loop_map_lock_;
};

}  // namespace webkit_glue

#endif // WEBKIT_CHILD_WORKER_TASK_RUNNER_H_
