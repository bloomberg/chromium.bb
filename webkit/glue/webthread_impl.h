// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef WEBKIT_GLUE_WEBTHREAD_IMPL_H_
#define WEBKIT_GLUE_WEBTHREAD_IMPL_H_

#include "base/threading/thread.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebThread.h"

namespace webkit_glue {

class WebThreadImpl : public WebKit::WebThread {
 public:
  WebThreadImpl(const char* name);
  virtual ~WebThreadImpl();

  virtual void postTask(Task* task);
#ifdef WEBTHREAD_HAS_LONGLONG_CHANGE
  virtual void postDelayedTask(Task* task, long long delay_ms);
#else
  virtual void postDelayedTask(Task* task, int64 delay_ms);
#endif

 protected:
  scoped_ptr<base::Thread> thread_;
};

} // namespace webkit_glue

#endif
