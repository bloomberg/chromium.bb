// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_AUTO_THREAD_TASK_RUNNER_H_
#define REMOTING_BASE_AUTO_THREAD_TASK_RUNNER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/single_thread_task_runner.h"

class MessageLoop;

namespace remoting {

// A wrapper around |SingleThreadTaskRunner| that provides automatic lifetime
// management, by invoking a caller-supplied callback when no more references
// remain, that can stop the wrapped task runner. The caller may also provide a
// reference to a parent |AutoThreadTaskRunner| to express dependencies between
// child and parent thread lifetimes.
class AutoThreadTaskRunner : public base::SingleThreadTaskRunner {
 public:
  // Constructs an instance of |AutoThreadTaskRunner| wrapping |task_runner|.
  // |stop_callback| is called (on arbitraty thread) when the last reference to
  // the object is dropped.
  explicit AutoThreadTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  AutoThreadTaskRunner(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                       const base::Closure& stop_callback);

  // TODO(alexeypa): Remove the |parent| reference once Thread class supports
  // stopping the thread's message loop and joining the thread separately.
  // See http://crbug.com/145856.
  //
  // Background: currently the only legitimate way of stopping a thread is to
  // call Thread::Stop() from the same thread that started it. Thread::Stop()
  // will stop the thread message loop by posting a private task and join
  // the thread. Thread::Stop() verifies that it is called from the right thread
  // and that the message loop has been stopped by the private task mentioned
  // above.
  //
  // Due to NPAPI/PPAPI limitations tasks cannot be posted to the main message
  // loop when the host plugin is being shut down. This presents a challenge
  // since Thread::Stop() cannot be called from an arbitrary thread. To work
  // around this we keep a reference to the parent task runner (typically
  // the one that started the thread) to keep it alive while the worker thread
  // is in use. Thread::Stop() is called to stop the worker thread when
  // the parent task runner exits.
  AutoThreadTaskRunner(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                       scoped_refptr<AutoThreadTaskRunner> parent);
  AutoThreadTaskRunner(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                       scoped_refptr<AutoThreadTaskRunner> parent,
                       const base::Closure& stop_callback);

  // SingleThreadTaskRunner implementation
  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               const base::Closure& task,
                               base::TimeDelta delay) OVERRIDE;
  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay) OVERRIDE;
  virtual bool RunsTasksOnCurrentThread() const OVERRIDE;

 private:
  friend class base::DeleteHelper<AutoThreadTaskRunner>;

  virtual ~AutoThreadTaskRunner();

  // An reference to the parent message loop to keep it alive while
  // |task_runner_| is running.
  scoped_refptr<AutoThreadTaskRunner> parent_;

  // This callback quits |task_runner_|. It can be run on any thread.
  base::Closure stop_callback_;

  // The wrapped task runner.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(AutoThreadTaskRunner);
};

}  // namespace remoting

#endif  // REMOTING_BASE_AUTO_THREAD_TASK_RUNNER_H_
