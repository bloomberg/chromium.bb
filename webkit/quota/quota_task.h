// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_QUOTA_QUOTA_TASK_H_
#define WEBKIT_QUOTA_QUOTA_TASK_H_
#pragma once

#include <set>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"

namespace base {
class MessageLoopProxy;
}

namespace quota {

class QuotaTaskObserver;

// A base class for quota tasks.
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
  scoped_refptr<base::MessageLoopProxy> original_message_loop() const {
    return original_message_loop_;
  }

 private:
  friend class QuotaTaskObserver;
  void Abort();
  QuotaTaskObserver* observer_;
  scoped_refptr<base::MessageLoopProxy> original_message_loop_;
};

// For tasks that post tasks to the other thread.
class QuotaThreadTask : public QuotaTask,
                        public base::RefCountedThreadSafe<QuotaThreadTask> {
 public:
  QuotaThreadTask(QuotaTaskObserver* observer,
                  scoped_refptr<base::MessageLoopProxy> target_message_loop);

 protected:
  virtual ~QuotaThreadTask();

  // One of the following Run methods should be overriden for execution
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
  scoped_refptr<base::MessageLoopProxy> target_message_loop() const {
    return target_message_loop_;
  }

 private:
  friend class base::RefCountedThreadSafe<QuotaThreadTask>;
  friend class QuotaTaskObserver;
  void CallRunOnTargetThread();

  scoped_refptr<base::MessageLoopProxy> target_message_loop_;
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
