// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptStreamerThread_h
#define ScriptStreamerThread_h

#include <memory>

#include "core/CoreExport.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/ThreadingPrimitives.h"
#include "public/platform/WebThread.h"
#include "v8/include/v8.h"

namespace blink {

class ScriptStreamer;

// A singleton thread for running background tasks for script streaming.
class CORE_EXPORT ScriptStreamerThread {
  USING_FAST_MALLOC(ScriptStreamerThread);
  WTF_MAKE_NONCOPYABLE(ScriptStreamerThread);

 public:
  static void Init();
  static ScriptStreamerThread* Shared();

  void PostTask(CrossThreadClosure);

  bool IsRunningTask() const {
    MutexLocker locker(mutex_);
    return running_task_;
  }

  void TaskDone();

  static void RunScriptStreamingTask(
      std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask>,
      ScriptStreamer*);

 private:
  ScriptStreamerThread() : running_task_(false) {}

  bool IsRunning() const { return !!thread_; }

  WebThread& PlatformThread();

  // At the moment, we only use one thread, so we can only stream one script
  // at a time. FIXME: Use a thread pool and stream multiple scripts.
  std::unique_ptr<WebThread> thread_;
  bool running_task_;
  mutable Mutex mutex_;  // Guards m_runningTask.
};

}  // namespace blink

#endif  // ScriptStreamerThread_h
