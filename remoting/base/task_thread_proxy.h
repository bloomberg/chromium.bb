// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_TASK_THREAD_PROXY_H_
#define REMOTING_BASE_TASK_THREAD_PROXY_H_

#include "base/message_loop.h"

namespace remoting {

// This is a refcounted class that is used to switch to the appropriate thread
// before running a task on a target object. It should be used whenever you
// need to post to an object, but:
//  (1) You don't know when the object might be deleted, and
//  (2) You cannot subclass the target from RefCountedThreadSafe.
//
// Example usage:
// Instead of:
//   MyClass* obj;
//   obj->Method(param);
// Use:
//   proxy->Call(base::Bind(&MyClass::Method,
//                          base::Unretained(obj),
//                          param);
class TaskThreadProxy : public base::RefCountedThreadSafe<TaskThreadProxy> {
 public:
  TaskThreadProxy(MessageLoop* loop);

  // Detach should be called when the target of the posted task is being
  // destroyed.
  void Detach();

  void Call(const base::Closure& closure);

 private:
  friend class base::RefCountedThreadSafe<TaskThreadProxy>;

  virtual ~TaskThreadProxy();

  void CallClosure(const base::Closure& closure);

  MessageLoop* message_loop_;

  DISALLOW_COPY_AND_ASSIGN(TaskThreadProxy);
};

}  // namespace remoting

#endif  // REMOTING_BASE_TASK_THREAD_PROXY_H_
