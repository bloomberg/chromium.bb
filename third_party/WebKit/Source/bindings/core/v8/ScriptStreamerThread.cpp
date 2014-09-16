// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/ScriptStreamerThread.h"

#include "bindings/core/v8/ScriptStreamer.h"
#include "platform/Task.h"
#include "public/platform/Platform.h"
#include "wtf/MainThread.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

static ScriptStreamerThread* s_sharedThread = 0;

void ScriptStreamerThread::init()
{
    ASSERT(!s_sharedThread);
    ASSERT(isMainThread());
    s_sharedThread = new ScriptStreamerThread();
}

void ScriptStreamerThread::shutdown()
{
    ASSERT(s_sharedThread);
    delete s_sharedThread;
    s_sharedThread = 0;
}

ScriptStreamerThread* ScriptStreamerThread::shared()
{
    return s_sharedThread;
}

void ScriptStreamerThread::postTask(WebThread::Task* task)
{
    ASSERT(isMainThread());
    ASSERT(!m_runningTask);
    m_runningTask = true;
    platformThread().postTask(task);
}

blink::WebThread& ScriptStreamerThread::platformThread()
{
    if (!isRunning())
        m_thread = adoptPtr(blink::Platform::current()->createThread("ScriptStreamerThread"));
    return *m_thread;
}

ScriptStreamingTask::ScriptStreamingTask(v8::ScriptCompiler::ScriptStreamingTask* task, ScriptStreamer* streamer)
    : m_v8Task(adoptPtr(task)), m_streamer(streamer) { }

void ScriptStreamingTask::run()
{
    // Running the task can and will block: SourceStream::GetSomeData will get
    // called and it will block and wait for data from the network.
    m_v8Task->Run();
    // Post a task to the main thread to signal that V8 has completed the
    // streaming.
    callOnMainThread(&ScriptStreamerThread::taskDone, 0);
    callOnMainThread(WTF::bind(&ScriptStreamer::streamingComplete, m_streamer));
}

} // namespace blink
