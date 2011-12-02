// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef WEBKIT_GLUE_WEBTHREAD_IMPL_H_
#define WEBKIT_GLUE_WEBTHREAD_IMPL_H_

#include "base/threading/thread.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebThread.h"
#include "webkit/glue/webkit_glue_export.h"

namespace webkit_glue {

class WebThreadImpl : public WebKit::WebThread {
 public:
  WEBKIT_GLUE_EXPORT WebThreadImpl(const char* name);
  WEBKIT_GLUE_EXPORT virtual ~WebThreadImpl();

  virtual void postTask(Task* task) OVERRIDE;
  virtual void postDelayedTask(Task* task, long long delay_ms) OVERRIDE;

  MessageLoop* message_loop() const { return thread_->message_loop(); }

 protected:
  scoped_ptr<base::Thread> thread_;
};

class WebThreadImplForMessageLoop : public WebKit::WebThread {
 public:
  WebThreadImplForMessageLoop(base::MessageLoopProxy* message_loop);
  virtual ~WebThreadImplForMessageLoop();

  virtual void postTask(Task* task) OVERRIDE;
  virtual void postDelayedTask(Task* task, long long delay_ms) OVERRIDE;

 protected:
  scoped_refptr<base::MessageLoopProxy> message_loop_;
};

} // namespace webkit_glue

#endif
