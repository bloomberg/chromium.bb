// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/task_thread_proxy.h"

#include "base/bind.h"

namespace remoting {

TaskThreadProxy::TaskThreadProxy(MessageLoop* loop)
    : message_loop_(loop) {
}

TaskThreadProxy::~TaskThreadProxy() {
}

void TaskThreadProxy::Detach() {
  message_loop_ = NULL;
}

void TaskThreadProxy::Call(const base::Closure& closure) {
  if (message_loop_) {
    message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &TaskThreadProxy::CallClosure, closure));
  }
}

void TaskThreadProxy::CallClosure(const base::Closure& closure) {
  if (message_loop_)
    closure.Run();
}

}  // namespace remoting
