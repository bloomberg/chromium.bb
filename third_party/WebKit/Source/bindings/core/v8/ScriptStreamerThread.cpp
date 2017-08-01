// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptStreamerThread.h"

#include <memory>
#include "bindings/core/v8/ScriptStreamer.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "platform/WebTaskRunner.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

static ScriptStreamerThread* g_shared_thread = 0;
// Guards s_sharedThread. s_sharedThread is initialized and deleted in the main
// thread, but also used by the streamer thread. Races can occur during
// shutdown.
static Mutex* g_mutex = 0;

void ScriptStreamerThread::Init() {
  DCHECK(!g_shared_thread);
  DCHECK(IsMainThread());
  // This is called in the main thread before any tasks are created, so no
  // locking is needed.
  g_mutex = new Mutex();
  g_shared_thread = new ScriptStreamerThread();
}

ScriptStreamerThread* ScriptStreamerThread::Shared() {
  return g_shared_thread;
}

void ScriptStreamerThread::PostTask(CrossThreadClosure task) {
  DCHECK(IsMainThread());
  MutexLocker locker(mutex_);
  DCHECK(!running_task_);
  running_task_ = true;
  PlatformThread().GetWebTaskRunner()->PostTask(BLINK_FROM_HERE,
                                                std::move(task));
}

void ScriptStreamerThread::TaskDone() {
  MutexLocker locker(mutex_);
  DCHECK(running_task_);
  running_task_ = false;
}

WebThread& ScriptStreamerThread::PlatformThread() {
  if (!IsRunning()) {
    thread_ = Platform::Current()->CreateThread("ScriptStreamerThread");
  }
  return *thread_;
}

void ScriptStreamerThread::RunScriptStreamingTask(
    std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask> task,
    ScriptStreamer* streamer) {
  TRACE_EVENT1(
      "v8,devtools.timeline", "v8.parseOnBackground", "data",
      InspectorParseScriptEvent::Data(streamer->ScriptResourceIdentifier(),
                                      streamer->ScriptURLString()));
  // Running the task can and will block: SourceStream::GetSomeData will get
  // called and it will block and wait for data from the network.
  task->Run();
  streamer->StreamingCompleteOnBackgroundThread();
  MutexLocker locker(*g_mutex);
  ScriptStreamerThread* thread = Shared();
  if (thread)
    thread->TaskDone();
  // If thread is 0, we're shutting down.
}

}  // namespace blink
