// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTaskRunner_h
#define WebTaskRunner_h

#include <memory>
#include "base/callback.h"
#include "platform/wtf/Compiler.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/Time.h"
#include "platform/wtf/WeakPtr.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebTraceLocation.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

using SingleThreadTaskRunner = base::SingleThreadTaskRunner;

// TaskHandle is associated to a task posted by
// WebTaskRunner::postCancellableTask or
// WebTaskRunner::postCancellableDelayedTask and cancels the associated task on
// TaskHandle::cancel() call or on TaskHandle destruction.
class BLINK_PLATFORM_EXPORT TaskHandle {
 public:
  // Returns true if the task will run later. Returns false if the task is
  // cancelled or the task is run already.
  // This function is not thread safe. Call this on the thread that has posted
  // the task.
  bool IsActive() const;

  // Cancels the task invocation. Do nothing if the task is cancelled or run
  // already.
  // This function is not thread safe. Call this on the thread that has posted
  // the task.
  void Cancel();

  TaskHandle();
  ~TaskHandle();

  TaskHandle(TaskHandle&&);
  TaskHandle& operator=(TaskHandle&&);

  class Runner;

 private:
  friend class WebTaskRunner;

  explicit TaskHandle(RefPtr<Runner>);
  RefPtr<Runner> runner_;
};

// The blink representation of a chromium SingleThreadTaskRunner.
class BLINK_PLATFORM_EXPORT WebTaskRunner
    : public ThreadSafeRefCounted<WebTaskRunner> {
 public:
  // Returns true if tasks posted to this TaskRunner are sequenced
  // with this call.
  virtual bool RunsTasksInCurrentSequence() = 0;

  // ---

  // Headless Chrome virtualises time for determinism and performance (fast
  // forwarding of timers). To make this work some parts of blink (e.g. Timers)
  // need to use virtual time, however by default new code should use the normal
  // non-virtual time APIs.

  // Returns a double which is the number of seconds since epoch (Jan 1, 1970).
  // This may represent either the real time, or a virtual time depending on
  // whether or not the WebTaskRunner is associated with a virtual time domain
  // or a real time domain.
  virtual double VirtualTimeSeconds() const = 0;

  // Returns a microsecond resolution platform dependant time source.
  // This may represent either the real time, or a virtual time depending on
  // whether or not the WebTaskRunner is associated with a virtual time domain
  // or a real time domain.
  virtual double MonotonicallyIncreasingVirtualTimeSeconds() const = 0;

  // Returns the underlying task runner object.
  virtual SingleThreadTaskRunner* ToSingleThreadTaskRunner() = 0;

  // Helpers for posting bound functions as tasks.

  // For cross-thread posting. Can be called from any thread.
  void PostTask(const WebTraceLocation&, std::unique_ptr<CrossThreadClosure>);
  void PostDelayedTask(const WebTraceLocation&,
                       std::unique_ptr<CrossThreadClosure>,
                       TimeDelta delay);

  // For same-thread posting. Must be called from the associated WebThread.
  void PostTask(const WebTraceLocation&, std::unique_ptr<WTF::Closure>);
  void PostDelayedTask(const WebTraceLocation&,
                       std::unique_ptr<WTF::Closure>,
                       TimeDelta delay);

  // For same-thread cancellable task posting. Returns a TaskHandle object for
  // cancellation.
  WARN_UNUSED_RESULT TaskHandle
  PostCancellableTask(const WebTraceLocation&, std::unique_ptr<WTF::Closure>);
  WARN_UNUSED_RESULT TaskHandle
  PostDelayedCancellableTask(const WebTraceLocation&,
                             std::unique_ptr<WTF::Closure>,
                             TimeDelta delay);

 protected:
  friend ThreadSafeRefCounted<WebTaskRunner>;
  WebTaskRunner() = default;
  virtual ~WebTaskRunner();

 private:
  DISALLOW_COPY_AND_ASSIGN(WebTaskRunner);
};

}  // namespace blink

#endif  // WebTaskRunner_h
