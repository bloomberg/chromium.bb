// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef WEBKIT_GLUE_WEBTHREAD_IMPL_H_
#define WEBKIT_GLUE_WEBTHREAD_IMPL_H_

#include <map>

#include "base/threading/thread.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebThread.h"
#include "webkit/glue/webkit_glue_export.h"

namespace webkit_glue {

class WebThreadBase : public WebKit::WebThread {
 public:
  virtual void addTaskObserver(TaskObserver* observer);
  virtual void removeTaskObserver(TaskObserver* observer);

 protected:
  virtual bool IsCurrentThread() const = 0;

 private:
  class TaskObserverAdapter;
  typedef std::map<TaskObserver*, TaskObserverAdapter*> TaskObserverMap;
  TaskObserverMap task_observer_map_;
};

class WebThreadImpl : public WebThreadBase {
 public:
  WEBKIT_GLUE_EXPORT explicit WebThreadImpl(const char* name);
  WEBKIT_GLUE_EXPORT virtual ~WebThreadImpl();

  virtual void postTask(Task* task) OVERRIDE;
  virtual void postDelayedTask(Task* task, long long delay_ms) OVERRIDE;

  MessageLoop* message_loop() const { return thread_->message_loop(); }

 protected:
  virtual bool IsCurrentThread() const;

 private:
  scoped_ptr<base::Thread> thread_;
};

class WebThreadImplForMessageLoop : public WebThreadBase {
 public:
  explicit WebThreadImplForMessageLoop(base::MessageLoopProxy* message_loop);
  virtual ~WebThreadImplForMessageLoop();

  virtual void postTask(Task* task) OVERRIDE;
  virtual void postDelayedTask(Task* task, long long delay_ms) OVERRIDE;

 protected:
  virtual bool IsCurrentThread() const;

 private:
  scoped_refptr<base::MessageLoopProxy> message_loop_;
};

} // namespace webkit_glue

#endif
