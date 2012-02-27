// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DOM_STORAGE_DOM_STORAGE_TASK_RUNNER_
#define WEBKIT_DOM_STORAGE_DOM_STORAGE_TASK_RUNNER_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time.h"

namespace base {
class MessageLoopProxy;
}

namespace dom_storage {

// Tasks must run serially with respect to one another, but may
// execute on different OS threads. The base class is implemented
// in terms of a MessageLoopProxy for use in testing.
class DomStorageTaskRunner
    : public base::RefCountedThreadSafe<DomStorageTaskRunner> {
 public:
  explicit DomStorageTaskRunner(base::MessageLoopProxy* message_loop);
  virtual ~DomStorageTaskRunner();

  // Schedules a task to be run immediately.
  virtual bool PostTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task);

  // Schedules a task to be run after a delay.
  virtual bool PostDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay);

 protected:
  const scoped_refptr<base::MessageLoopProxy> message_loop_;
};

// A derived class that utlizes the SequenceWorkerPool under a
// dom_storage specific SequenceToken. The MessageLoopProxy
// is used to delay scheduling on the worker pool.
class DomStorageWorkerPoolTaskRunner : public DomStorageTaskRunner {
 public:
  DomStorageWorkerPoolTaskRunner(
      base::SequencedWorkerPool* sequenced_worker_pool,
      base::MessageLoopProxy* delayed_task_loop);
  virtual ~DomStorageWorkerPoolTaskRunner();

  // Schedules a sequenced worker task to be run immediately.
  virtual bool PostTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task) OVERRIDE;

  // Schedules a sequenced worker task to be run after a delay.
  virtual bool PostDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay) OVERRIDE;

 private:
  const scoped_refptr<base::SequencedWorkerPool> sequenced_worker_pool_;
  base::SequencedWorkerPool::SequenceToken sequence_token_;
};

// A derived class used in unit tests that causes us to ignore the
// delay in PostDelayedTask so we don't need to block in unit tests
// for the timeout to expire.
class MockDomStorageTaskRunner : public DomStorageTaskRunner {
 public:
  explicit MockDomStorageTaskRunner(base::MessageLoopProxy* message_loop);
  virtual ~MockDomStorageTaskRunner() { }

  virtual bool PostDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay) OVERRIDE;
};

}  // namespace dom_storage

#endif  // WEBKIT_DOM_STORAGE_DOM_STORAGE_TASK_RUNNER_
