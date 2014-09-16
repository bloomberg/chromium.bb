// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptStreamerThread_h
#define ScriptStreamerThread_h

#include "platform/TaskSynchronizer.h"
#include "public/platform/WebThread.h"
#include "wtf/OwnPtr.h"

#include <v8.h>

namespace blink {

class ScriptStreamer;

// A singleton thread for running background tasks for script streaming.
class ScriptStreamerThread {
    WTF_MAKE_NONCOPYABLE(ScriptStreamerThread);
public:
    static void init();
    static void shutdown();
    static ScriptStreamerThread* shared();

    void postTask(WebThread::Task*);

    bool isRunningTask() const
    {
        return m_runningTask;
    }

    static void taskDone(void*)
    {
        ASSERT(shared()->m_runningTask);
        shared()->m_runningTask = false;
    }

private:
    ScriptStreamerThread()
        : m_runningTask(false) { }

    bool isRunning()
    {
        return !!m_thread;
    }

    void markAsCompleted(TaskSynchronizer* taskSynchronizer)
    {
        taskSynchronizer->taskCompleted();
    }

    blink::WebThread& platformThread();

    // At the moment, we only use one thread, so we can only stream one script
    // at a time. FIXME: Use a thread pool and stream multiple scripts.
    WTF::OwnPtr<blink::WebThread> m_thread;
    bool m_runningTask;
};

class ScriptStreamingTask : public WebThread::Task {
    WTF_MAKE_NONCOPYABLE(ScriptStreamingTask);
public:
    ScriptStreamingTask(v8::ScriptCompiler::ScriptStreamingTask*, ScriptStreamer*);
    virtual void run() OVERRIDE;

private:
    WTF::OwnPtr<v8::ScriptCompiler::ScriptStreamingTask> m_v8Task;
    ScriptStreamer* m_streamer;
};


} // namespace blink

#endif // ScriptStreamerThread_h
