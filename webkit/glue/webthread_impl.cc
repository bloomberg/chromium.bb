// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An implementation of WebThread in terms of base::MessageLoop and
// base::Thread

#include "webkit/glue/webthread_impl.h"

#include "base/task.h"
#include "base/message_loop.h"

namespace webkit_glue {

class TaskAdapter : public Task {
public:
  TaskAdapter(WebKit::WebThread::Task* task) : task_(task) { }
  virtual void Run() {
    task_->run();
  }
private:
  scoped_ptr<WebKit::WebThread::Task> task_;
};

WebThreadImpl::WebThreadImpl(const char* name)
    : thread_(new base::Thread(name)) {
  thread_->Start();
}

void WebThreadImpl::postTask(Task* task) {
  thread_->message_loop()->PostTask(FROM_HERE,
                                    new TaskAdapter(task));
}
#ifdef WEBTHREAD_HAS_LONGLONG_CHANGE
void WebThreadImpl::postDelayedTask(
    Task* task, long long delay_ms) {
#else
void WebThreadImpl::postDelayedTask(
    Task* task, int64 delay_ms) {
#endif
  thread_->message_loop()->PostDelayedTask(
      FROM_HERE, new TaskAdapter(task), delay_ms);
}

WebThreadImpl::~WebThreadImpl() {
  thread_->Stop();
}

}
