// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8InspectorSession_h
#define V8InspectorSession_h

#include "platform/PlatformExport.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class V8DebuggerAgent;
class V8HeapProfilerAgent;
class V8InspectorSessionClient;
class V8ProfilerAgent;
class V8RuntimeAgent;

class PLATFORM_EXPORT V8InspectorSession {
public:
    static const char backtraceObjectGroup[];
    virtual ~V8InspectorSession() { }

    virtual void setClient(V8InspectorSessionClient*) = 0;

    // API for the embedder to report native activities.
    virtual void schedulePauseOnNextStatement(const String16& breakReason, PassOwnPtr<protocol::DictionaryValue> data) = 0;
    virtual void cancelPauseOnNextStatement() = 0;
    virtual void breakProgram(const String16& breakReason, PassOwnPtr<protocol::DictionaryValue> data) = 0;
    virtual void breakProgramOnException(const String16& breakReason, PassOwnPtr<protocol::DictionaryValue> data) = 0;
    virtual void setSkipAllPauses(bool) = 0;

    // Async call stacks implementation.
    virtual void asyncTaskScheduled(const String16& taskName, void* task, bool recurring) = 0;
    virtual void asyncTaskCanceled(void* task) = 0;
    virtual void asyncTaskStarted(void* task) = 0;
    virtual void asyncTaskFinished(void* task) = 0;
    virtual void allAsyncTasksCanceled() = 0;

    virtual V8DebuggerAgent* debuggerAgent() = 0;
    virtual V8HeapProfilerAgent* heapProfilerAgent() = 0;
    virtual V8ProfilerAgent* profilerAgent() = 0;
    virtual V8RuntimeAgent* runtimeAgent() = 0;
};

} // namespace blink

#endif // V8InspectorSession_h
