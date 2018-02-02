// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_WORKER_CONTROLLER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_WORKER_CONTROLLER_H_

#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "platform/PlatformExport.h"

namespace base {
class TickClock;
};

namespace blink {
namespace scheduler {
namespace internal {

class Sequence;

// Interface for TaskQueueManager to schedule work to be run.
class PLATFORM_EXPORT ThreadController {
 public:
  virtual ~ThreadController() = default;

  // Notify the controller that its associated sequence has immediate work
  // to run. Shortly after this is called, the thread associated with this
  // controller will run a task returned by sequence->TakeTask().
  //
  // TODO(altimin): Change this to "the thread associated with this
  // controller will run tasks returned by sequence->TakeTask() until it
  // returns null or sequence->DidRunTask() returns false" once the
  // code is changed to work that way.
  virtual void ScheduleWork() = 0;

  // Notify the controller that its associated sequence will have
  // delayed work to run when |delay| expires. When |delay| expires,
  // the thread associated with this controller will run a task
  // returned by sequence->TakeTask(). This call cancels any previously
  // scheduled delayed work.
  //
  // TODO(altimin): Change this to "the thread associated with this
  // controller will run tasks returned by sequence->TakeTask() until
  // it returns null or sequence->DidRunTask() returns false" once the
  // code is changed to work that way.
  virtual void ScheduleDelayedWork(base::TimeDelta delay) = 0;

  // Notify thread controller that sequence does not have delayed work
  // and previously scheduled callbacks should be cancelled.
  virtual void CancelDelayedWork() = 0;

  // Sets the sequence from which to take tasks after a Schedule*Work() call is
  // made. Must be called before the first call to Schedule*Work().
  virtual void SetSequence(Sequence*) = 0;

  // TODO(altimin): Get rid of the methods below.
  // These methods exist due to current integration of TaskQueueManager
  // with MessageLoop.

  virtual bool RunsTasksInCurrentSequence() = 0;

  virtual base::TickClock* GetClock() = 0;

  virtual void SetDefaultTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner>) = 0;

  virtual void RestoreDefaultTaskRunner() = 0;

  virtual void AddNestingObserver(base::RunLoop::NestingObserver* observer) = 0;

  virtual void RemoveNestingObserver(
      base::RunLoop::NestingObserver* observer) = 0;
};

}  // namespace internal
}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_WORKER_CONTROLLER_H_
