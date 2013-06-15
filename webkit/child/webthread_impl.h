// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef WEBKIT_CHILD_WEBTHREAD_IMPL_H_
#define WEBKIT_CHILD_WEBTHREAD_IMPL_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "third_party/WebKit/public/platform/WebThread.h"
#include "webkit/child/webkit_child_export.h"

namespace webkit_glue {

class WebThreadBase : public WebKit::WebThread {
 public:
  virtual ~WebThreadBase();

  virtual void addTaskObserver(TaskObserver* observer);
  virtual void removeTaskObserver(TaskObserver* observer);

  virtual bool isCurrentThread() const = 0;

 protected:
  WebThreadBase();

 private:
  class TaskObserverAdapter;

  typedef std::map<TaskObserver*, TaskObserverAdapter*> TaskObserverMap;
  TaskObserverMap task_observer_map_;
};

class WebThreadImpl : public WebThreadBase {
 public:
  WEBKIT_CHILD_EXPORT explicit WebThreadImpl(const char* name);
  WEBKIT_CHILD_EXPORT virtual ~WebThreadImpl();

  virtual void postTask(Task* task);
  virtual void postDelayedTask(Task* task, long long delay_ms);

  virtual void enterRunLoop();
  virtual void exitRunLoop();

  base::MessageLoop* message_loop() const { return thread_->message_loop(); }

 private:
  virtual bool isCurrentThread() const OVERRIDE;
  scoped_ptr<base::Thread> thread_;
};

class WebThreadImplForMessageLoop : public WebThreadBase {
 public:
  WEBKIT_CHILD_EXPORT explicit WebThreadImplForMessageLoop(
      base::MessageLoopProxy* message_loop);
  WEBKIT_CHILD_EXPORT virtual ~WebThreadImplForMessageLoop();

  virtual void postTask(Task* task);
  virtual void postDelayedTask(Task* task, long long delay_ms);

  virtual void enterRunLoop();
  virtual void exitRunLoop();

 private:
  virtual bool isCurrentThread() const OVERRIDE;
  scoped_refptr<base::MessageLoopProxy> message_loop_;
};

} // namespace webkit_glue

#endif  // WEBKIT_CHILD_WEBTHREAD_IMPL_H_
