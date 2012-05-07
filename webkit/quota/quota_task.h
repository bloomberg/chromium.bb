// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_QUOTA_QUOTA_TASK_H_
#define WEBKIT_QUOTA_QUOTA_TASK_H_
#pragma once

#include <set>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"

namespace base {
class SingleThreadTaskRunner;
class TaskRunner;
}

namespace quota {

class QuotaTaskObserver;
class QuotaThreadTask;

// A base class for quota tasks.
// TODO(kinuko): Revise this using base::Callback.
class QuotaTask {
 public:
  virtual ~QuotaTask();
  void Start();

 protected:
  explicit QuotaTask(QuotaTaskObserver* observer);

  // The task body.
  virtual void Run() = 0;

  // Called upon completion, on the original message loop.
  virtual void Completed() = 0;

  // Called when the task is aborted.
  virtual void Aborted() {}

  void CallCompleted();

  // Call this to delete itself.
  void DeleteSoon();

  QuotaTaskObserver* observer() const { return observer_; }
  base::SingleThreadTaskRunner* original_task_runner() const {
    return original_task_runner_;
  }

 private:
  friend class QuotaTaskObserver;
  friend class QuotaThreadTask;
  void Abort();
  QuotaTaskObserver* observer_;
  scoped_refptr<base::SingleThreadTaskRunner> original_task_runner_;
};

// For tasks that post tasks to the other thread.
class QuotaThreadTask : public QuotaTask,
                        public base::RefCountedThreadSafe<QuotaThreadTask> {
 public:
  QuotaThreadTask(QuotaTaskObserver* observer,
                  base::TaskRunner* target_task_runner);

 protected:
  virtual ~QuotaThreadTask();

  // One of the following Run methods should be overridden for execution
  // on the target thread.

  // A task to invoke the CallCompleted() method on the original thread will
  // be scheduled immediately upon return from RunOnTargetThread().
  virtual void RunOnTargetThread();

  // A task to invoke the CallCompleted() method on the original thread will
  // only be scheduled if RunOnTargetThreadAsync returns true. If false is
  // returned, the derived class should schedule a task to do so upon actual
  // completion.
  virtual bool RunOnTargetThreadAsync();

  virtual void Run() OVERRIDE;
  base::TaskRunner* target_task_runner() const {
    return target_task_runner_;
  }

 private:
  friend class base::RefCountedThreadSafe<QuotaThreadTask>;
  friend class QuotaTaskObserver;
  void CallRunOnTargetThread();

  scoped_refptr<base::TaskRunner> target_task_runner_;
};

class QuotaTaskObserver {
 public:
  virtual ~QuotaTaskObserver();

 protected:
  friend class QuotaTask;
  friend class QuotaThreadTask;

  QuotaTaskObserver();
  void RegisterTask(QuotaTask* task);
  void UnregisterTask(QuotaTask* task);

  typedef std::set<QuotaTask*> TaskSet;
  TaskSet running_quota_tasks_;
};
}

#endif  // WEBKIT_QUOTA_QUOTA_TASK_H_
