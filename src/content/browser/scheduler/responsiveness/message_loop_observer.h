// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_MESSAGE_LOOP_OBSERVER_H_
#define CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_MESSAGE_LOOP_OBSERVER_H_

#include "base/macros.h"
#include "base/task/task_observer.h"
#include "content/common/content_export.h"

namespace base {
struct PendingTask;
}  // namespace base

namespace content {
namespace responsiveness {

// This object is not thread safe. It must be constructed and destroyed on the
// same thread. The callbacks will occur synchronously from WillProcessTask()
// and DidProcessTask().
class CONTENT_EXPORT MessageLoopObserver : base::TaskObserver {
 public:
  using WillProcessTaskCallback =
      base::RepeatingCallback<void(const base::PendingTask* task,
                                   bool was_blocked_or_low_priority)>;
  using DidProcessTaskCallback =
      base::RepeatingCallback<void(const base::PendingTask* task)>;

  // The constructor will register the object as an observer of the current
  // MessageLoop. The destructor will unregister the object.
  MessageLoopObserver(WillProcessTaskCallback will_process_task_callback,
                      DidProcessTaskCallback did_process_task_callback);
  ~MessageLoopObserver() override;

 private:
  void WillProcessTask(const base::PendingTask& pending_task,
                       bool was_blocked_or_low_priority) override;
  void DidProcessTask(const base::PendingTask& pending_task) override;

  const WillProcessTaskCallback will_process_task_callback_;
  const DidProcessTaskCallback did_process_task_callback_;

  DISALLOW_COPY_AND_ASSIGN(MessageLoopObserver);
};

}  // namespace responsiveness
}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_MESSAGE_LOOP_OBSERVER_H_
