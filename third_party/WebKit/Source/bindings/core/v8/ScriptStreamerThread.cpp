// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptStreamerThread.h"

#include "bindings/core/v8/ScriptStreamer.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "platform/WebTaskRunner.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

static ScriptStreamerThread* s_sharedThread = 0;
// Guards s_sharedThread. s_sharedThread is initialized and deleted in the main
// thread, but also used by the streamer thread. Races can occur during
// shutdown.
static Mutex* s_mutex = 0;

void ScriptStreamerThread::init() {
  ASSERT(!s_sharedThread);
  ASSERT(isMainThread());
  // This is called in the main thread before any tasks are created, so no
  // locking is needed.
  s_mutex = new Mutex();
  s_sharedThread = new ScriptStreamerThread();
}

ScriptStreamerThread* ScriptStreamerThread::shared() {
  return s_sharedThread;
}

void ScriptStreamerThread::postTask(std::unique_ptr<CrossThreadClosure> task) {
  ASSERT(isMainThread());
  MutexLocker locker(m_mutex);
  ASSERT(!m_runningTask);
  m_runningTask = true;
  platformThread().getWebTaskRunner()->postTask(BLINK_FROM_HERE,
                                                std::move(task));
}

void ScriptStreamerThread::taskDone() {
  MutexLocker locker(m_mutex);
  ASSERT(m_runningTask);
  m_runningTask = false;
}

WebThread& ScriptStreamerThread::platformThread() {
  if (!isRunning()) {
    m_thread = WTF::wrapUnique(
        Platform::current()->createThread("ScriptStreamerThread"));
  }
  return *m_thread;
}

void ScriptStreamerThread::runScriptStreamingTask(
    std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask> task,
    ScriptStreamer* streamer) {
  TRACE_EVENT1(
      "v8,devtools.timeline", "v8.parseOnBackground", "data",
      InspectorParseScriptEvent::data(streamer->scriptResourceIdentifier(),
                                      streamer->scriptURLString()));
  // Running the task can and will block: SourceStream::GetSomeData will get
  // called and it will block and wait for data from the network.
  task->Run();
  streamer->streamingCompleteOnBackgroundThread();
  MutexLocker locker(*s_mutex);
  ScriptStreamerThread* thread = shared();
  if (thread)
    thread->taskDone();
  // If thread is 0, we're shutting down.
}

}  // namespace blink
