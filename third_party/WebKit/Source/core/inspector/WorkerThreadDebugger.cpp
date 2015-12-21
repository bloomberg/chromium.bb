/*
 * Copyright (c) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/inspector/WorkerThreadDebugger.h"

#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/inspector/DebuggerScript.h"
#include "core/inspector/WorkerDebuggerAgent.h"
#include "core/workers/WorkerThread.h"
#include <v8.h>

namespace blink {

static const int workerContextGroupId = 1;

WorkerThreadDebugger::WorkerThreadDebugger(WorkerThread* workerThread)
    : m_isolate(v8::Isolate::GetCurrent())
    , m_debugger(V8Debugger::create(v8::Isolate::GetCurrent(), this))
    , m_workerThread(workerThread)
    , m_paused(false)
{
}

WorkerThreadDebugger::~WorkerThreadDebugger()
{
}

void WorkerThreadDebugger::setContextDebugData(v8::Local<v8::Context> context)
{
    V8Debugger::setContextDebugData(context, "worker", workerContextGroupId);
}

int WorkerThreadDebugger::contextGroupId()
{
    return workerContextGroupId;
}

v8::Local<v8::Object> WorkerThreadDebugger::compileDebuggerScript()
{
    return blink::compileDebuggerScript(m_isolate);
}

void WorkerThreadDebugger::runMessageLoopOnPause(int contextGroupId)
{
    ASSERT(contextGroupId == workerContextGroupId);
    m_paused = true;
    WorkerThread::TaskQueueResult result;
    m_workerThread->willRunDebuggerTasks();
    do {
        result = m_workerThread->runDebuggerTask();
    // Keep waiting until execution is resumed.
    } while (result == WorkerThread::TaskReceived && m_paused);
    m_workerThread->didRunDebuggerTasks();
}

void WorkerThreadDebugger::quitMessageLoopOnPause()
{
    m_paused = false;
}

} // namespace blink
