/*
* Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef InspectorInstrumentation_h
#define InspectorInstrumentation_h

#include "core/CoreExport.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InstrumentingSessions.h"
#include "core/page/ChromeClient.h"

namespace blink {

class WorkerGlobalScope;

namespace InspectorInstrumentation {

class CORE_EXPORT AsyncTask {
    STACK_ALLOCATED();
public:
    AsyncTask(ExecutionContext*, void* task);
    AsyncTask(ExecutionContext*, void* task, bool enabled);
    ~AsyncTask();

private:
    Member<InstrumentingSessions> m_instrumentingSessions;
    void* m_task;
};

class CORE_EXPORT NativeBreakpoint {
    STACK_ALLOCATED();
public:
    NativeBreakpoint(ExecutionContext*, const String& name, bool sync);
    NativeBreakpoint(ExecutionContext*, EventTarget*, Event*);
    ~NativeBreakpoint();

private:
    Member<InstrumentingSessions> m_instrumentingSessions;
    bool m_sync;
};

class CORE_EXPORT StyleRecalc {
    STACK_ALLOCATED();
public:
    StyleRecalc(Document*);
    ~StyleRecalc();
private:
    Member<InstrumentingSessions> m_instrumentingSessions;
};

class CORE_EXPORT JavaScriptDialog {
    STACK_ALLOCATED();
public:
    JavaScriptDialog(LocalFrame*, const String& message, ChromeClient::DialogType);
    void setResult(bool);
    ~JavaScriptDialog();
private:
    Member<InstrumentingSessions> m_instrumentingSessions;
    bool m_result;
};

class CORE_EXPORT FrontendCounter {
    STATIC_ONLY(FrontendCounter);
private:
    friend void frontendCreated();
    friend void frontendDeleted();
    friend bool hasFrontends();
    static int s_frontendCounter;
};

inline void frontendCreated() { atomicIncrement(&FrontendCounter::s_frontendCounter); }
inline void frontendDeleted() { atomicDecrement(&FrontendCounter::s_frontendCounter); }
inline bool hasFrontends() { return acquireLoad(&FrontendCounter::s_frontendCounter); }

CORE_EXPORT void registerInstrumentingSessions(InstrumentingSessions*);
CORE_EXPORT void unregisterInstrumentingSessions(InstrumentingSessions*);

CORE_EXPORT extern const char kInspectorEmulateNetworkConditionsClientId[];

// Called from generated instrumentation code.
CORE_EXPORT InstrumentingSessions* instrumentingSessionsFor(WorkerGlobalScope*);
CORE_EXPORT InstrumentingSessions* instrumentingSessionsForNonDocumentContext(ExecutionContext*);

inline InstrumentingSessions* instrumentingSessionsFor(LocalFrame* frame)
{
    return frame ? frame->instrumentingSessions() : nullptr;
}

inline InstrumentingSessions* instrumentingSessionsFor(Document& document)
{
    LocalFrame* frame = document.frame();
    if (!frame && document.templateDocumentHost())
        frame = document.templateDocumentHost()->frame();
    return instrumentingSessionsFor(frame);
}

inline InstrumentingSessions* instrumentingSessionsFor(Document* document)
{
    return document ? instrumentingSessionsFor(*document) : nullptr;
}

inline InstrumentingSessions* instrumentingSessionsFor(ExecutionContext* context)
{
    if (!context)
        return nullptr;
    return context->isDocument() ? instrumentingSessionsFor(*toDocument(context)) : instrumentingSessionsForNonDocumentContext(context);
}

inline InstrumentingSessions* instrumentingSessionsFor(Node* node)
{
    return node ? instrumentingSessionsFor(node->document()) : nullptr;
}

inline InstrumentingSessions* instrumentingSessionsFor(EventTarget* eventTarget)
{
    return eventTarget ? instrumentingSessionsFor(eventTarget->getExecutionContext()) : nullptr;
}

} // namespace InspectorInstrumentation
} // namespace blink

#include "core/InspectorInstrumentationInl.h"

#include "core/inspector/InspectorInstrumentationCustomInl.h"

#include "core/InspectorOverridesInl.h"

#endif // !defined(InspectorInstrumentation_h)
