// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An implementation of WebThread in terms of base::MessageLoop and
// base::Thread

#include "webkit/glue/webthread_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/threading/platform_thread.h"

namespace webkit_glue {

class WebThreadBase::TaskObserverAdapter : public MessageLoop::TaskObserver {
public:
  TaskObserverAdapter(WebThread::TaskObserver* observer)
      : observer_(observer) { }

  // WebThread::TaskObserver does not have a willProcessTask method.
  virtual void WillProcessTask(base::TimeTicks) OVERRIDE { }

  virtual void DidProcessTask(base::TimeTicks) OVERRIDE {
    observer_->didProcessTask();
  }

private:
  WebThread::TaskObserver* observer_;
};

void WebThreadBase::addTaskObserver(TaskObserver* observer) {
  CHECK(IsCurrentThread());
  std::pair<TaskObserverMap::iterator, bool> result = task_observer_map_.insert(
      std::make_pair(observer, static_cast<TaskObserverAdapter*>(NULL)));
  if (result.second)
    result.first->second = new TaskObserverAdapter(observer);
  MessageLoop::current()->AddTaskObserver(result.first->second);
}

void WebThreadBase::removeTaskObserver(TaskObserver* observer) {
  CHECK(IsCurrentThread());
  TaskObserverMap::iterator iter = task_observer_map_.find(observer);
  if (iter == task_observer_map_.end())
    return;
  MessageLoop::current()->RemoveTaskObserver(iter->second);
  delete iter->second;
  task_observer_map_.erase(iter);
}

WebThreadImpl::WebThreadImpl(const char* name)
    : thread_(new base::Thread(name)) {
  thread_->Start();
}

void WebThreadImpl::postTask(Task* task) {
  thread_->message_loop()->PostTask(
      FROM_HERE, base::Bind(&WebKit::WebThread::Task::run, base::Owned(task)));
}

void WebThreadImpl::postDelayedTask(
    Task* task, long long delay_ms) {
  thread_->message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&WebKit::WebThread::Task::run, base::Owned(task)),
      delay_ms);
}

bool WebThreadImpl::IsCurrentThread() const {
  return thread_->thread_id() == base::PlatformThread::CurrentId();
}

WebThreadImpl::~WebThreadImpl() {
  thread_->Stop();
}

WebThreadImplForMessageLoop::WebThreadImplForMessageLoop(
    base::MessageLoopProxy* message_loop)
    : message_loop_(message_loop) {
}

void WebThreadImplForMessageLoop::postTask(Task* task) {
  message_loop_->PostTask(
      FROM_HERE, base::Bind(&WebKit::WebThread::Task::run, base::Owned(task)));
}

void WebThreadImplForMessageLoop::postDelayedTask(
    Task* task, long long delay_ms) {
  message_loop_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&WebKit::WebThread::Task::run, base::Owned(task)),
      delay_ms);
}

bool WebThreadImplForMessageLoop::IsCurrentThread() const {
  return message_loop_->BelongsToCurrentThread();
}

WebThreadImplForMessageLoop::~WebThreadImplForMessageLoop() {
}

}
