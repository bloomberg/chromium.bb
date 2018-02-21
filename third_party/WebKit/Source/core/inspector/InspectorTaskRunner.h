// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorTaskRunner_h
#define InspectorTaskRunner_h

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "core/CoreExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/ThreadSafeRefCounted.h"
#include "platform/wtf/ThreadingPrimitives.h"
#include "v8/include/v8.h"

namespace blink {

// This class manages a queue of tasks posted from any threads, and interrupts
// v8::Isolate to run the tasks.
//
// The tasks will be run on the isolate's thread after interruption or via
// posting to it's task runner.
class CORE_EXPORT InspectorTaskRunner final
    : public ThreadSafeRefCounted<InspectorTaskRunner> {
  USING_FAST_MALLOC(InspectorTaskRunner);

 public:
  // Can be created on any thread.
  static scoped_refptr<InspectorTaskRunner> Create(
      scoped_refptr<base::SingleThreadTaskRunner> isolate_task_runner) {
    return base::AdoptRef(new InspectorTaskRunner(isolate_task_runner));
  }

  // Must be called on the isolate's thread.
  void InitIsolate(v8::Isolate*) LOCKS_EXCLUDED(mutex_);
  // Can be disposed from any thread.
  void Dispose() LOCKS_EXCLUDED(mutex_);

  // Can be called from any thread other than isolate's thread.
  // This method appends a task, and both posts to the isolate's task runner
  // and requests interrupt. Whatever comes first - executes the task.
  using Task = WTF::CrossThreadClosure;
  void AppendTask(Task) LOCKS_EXCLUDED(mutex_);

  // Can be called on any thread.
  bool IsRunningTask() LOCKS_EXCLUDED(mutex_);

  // Must be called on the isolate's thread.
  // Returns |true| is the task was run, and |false| if the runner
  // was disposed while waiting for the task.
  bool WaitForAndRunSingleTask() LOCKS_EXCLUDED(mutex_);

  // This can only be used on the isolate's thread.
  class CORE_EXPORT IgnoreInterruptsScope final {
    USING_FAST_MALLOC(IgnoreInterruptsScope);

   public:
    explicit IgnoreInterruptsScope(scoped_refptr<InspectorTaskRunner>);
    ~IgnoreInterruptsScope();

   private:
    bool was_ignoring_;
    scoped_refptr<InspectorTaskRunner> task_runner_;
  };

 private:
  friend ThreadSafeRefCounted<InspectorTaskRunner>;
  explicit InspectorTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> isolate_task_runner);
  ~InspectorTaskRunner();

  // All these methods are run on the isolate's thread.
  enum WaitMode { kWaitForTask, kDontWaitForTask };
  Task TakeNextTask(WaitMode) LOCKS_EXCLUDED(mutex_);
  void PerformSingleTask(Task) LOCKS_EXCLUDED(mutex_);
  void PerformSingleTaskDontWait() LOCKS_EXCLUDED(mutex_);
  static void V8InterruptCallback(v8::Isolate*, void* data);

  Mutex mutex_;
  scoped_refptr<base::SingleThreadTaskRunner> isolate_task_runner_;
  v8::Isolate* isolate_ GUARDED_BY(mutex_) = nullptr;
  bool ignore_interrupts_ = false;
  ThreadCondition condition_;
  Deque<Task> queue_;
  bool disposed_ GUARDED_BY(mutex_) = false;
  bool running_task_ GUARDED_BY(mutex_) = false;
  DISALLOW_COPY_AND_ASSIGN(InspectorTaskRunner);
};

}  // namespace blink

#endif  // !defined(InspectorTaskRunner_h)
