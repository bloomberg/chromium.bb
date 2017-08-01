// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorTaskRunner_h
#define InspectorTaskRunner_h

#include "core/CoreExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/ThreadingPrimitives.h"
#include "v8/include/v8.h"

namespace blink {

class CORE_EXPORT InspectorTaskRunner final {
  WTF_MAKE_NONCOPYABLE(InspectorTaskRunner);
  USING_FAST_MALLOC(InspectorTaskRunner);

 public:
  InspectorTaskRunner();
  ~InspectorTaskRunner();

  using Task = WTF::CrossThreadClosure;
  void AppendTask(Task);

  enum WaitMode { kWaitForTask, kDontWaitForTask };
  Task TakeNextTask(WaitMode);

  void InterruptAndRunAllTasksDontWait(v8::Isolate*);
  void RunAllTasksDontWait();

  void Kill();

  class CORE_EXPORT IgnoreInterruptsScope final {
    USING_FAST_MALLOC(IgnoreInterruptsScope);

   public:
    explicit IgnoreInterruptsScope(InspectorTaskRunner*);
    ~IgnoreInterruptsScope();

   private:
    bool was_ignoring_;
    InspectorTaskRunner* task_runner_;
  };

 private:
  static void V8InterruptCallback(v8::Isolate*, void* data);

  bool ignore_interrupts_;
  Mutex mutex_;
  ThreadCondition condition_;
  Deque<Task> queue_;
  bool killed_;
};

}  // namespace blink

#endif  // !defined(InspectorTaskRunner_h)
