// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_SCOPED_THREAD_PROXY_H_
#define REMOTING_BASE_SCOPED_THREAD_PROXY_H_

#include "base/bind.h"
#include "base/message_loop_proxy.h"

namespace remoting {

// ScopedThreadProxy is proxy for message loops that cancels all
// pending tasks when it is destroyed. It can be used to post tasks
// for a non-refcounted object. Must be deleted on the thread it
// belongs to.
//
// The main difference from WeakPtr<> and ScopedRunnableMethodFactory<>
// is that this class can be used safely to post tasks from different
// threads.
// It is similar to WeakHandle<> used in sync: the main difference is
// that WeakHandle<> creates closures itself, while ScopedThreadProxy
// accepts base::Closure instances which caller needs to create using
// base::Bind().
//
// TODO(sergeyu): Potentially we could use WeakHandle<> instead of
// this class. Consider migrating to WeakHandle<> when it is moved to
// src/base and support for delayed tasks is implemented.
//
// Usage:
//   class MyClass {
//    public:
//     MyClass()
//         : thread_proxy_(base::MessageLoopProxy::current()) {}
//
//     // Always called on the thread on which this object was created.
//     void NonThreadSafeMethod() {}
//
//     // Can be called on any thread.
//     void ThreadSafeMethod() {
//       thread_proxy_.PostTask(FROM_HERE, base::Bind(
//           &MyClass::NonThreadSafeMethod, base::Unretained(this)));
//     }
//
//    private:
//     ScopedThreadProxy thread_proxy_;
//   };
class ScopedThreadProxy {
 public:
  ScopedThreadProxy(base::MessageLoopProxy* message_loop);
  ~ScopedThreadProxy();

  void PostTask(const tracked_objects::Location& from_here,
                const base::Closure& closure);
  void PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& closure,
                       int64 delay_ms);

  // Cancels all tasks posted via this proxy. Must be called on the
  // thread this object belongs to.
  void Detach();

 private:
  class Core;

  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(ScopedThreadProxy);
};

}  // namespace remoting

#endif  // REMOTING_BASE_SCOPED_THREAD_PROXY_H_
