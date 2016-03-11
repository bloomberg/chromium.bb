// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerInspectorProxy_h
#define WorkerInspectorProxy_h

#include "core/CoreExport.h"
#include "core/workers/WorkerThread.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"

namespace blink {

class ExecutionContext;
class KURL;
class WebTraceLocation;
class WorkerGlobalScopeProxy;

// A proxy for talking to the worker inspector on the worker thread.
// All of these methods should be called on the main thread.
class CORE_EXPORT WorkerInspectorProxy final : public NoBaseWillBeGarbageCollectedFinalized<WorkerInspectorProxy> {
    USING_FAST_MALLOC_WILL_BE_REMOVED(WorkerInspectorProxy);
public:
    static PassOwnPtrWillBeRawPtr<WorkerInspectorProxy> create();

    ~WorkerInspectorProxy();
    DECLARE_TRACE();

    class CORE_EXPORT PageInspector {
    public:
        virtual ~PageInspector() { }
        virtual void dispatchMessageFromWorker(WorkerInspectorProxy*, const String&) = 0;
        virtual void workerConsoleAgentEnabled(WorkerInspectorProxy*) = 0;
    };

    WorkerThreadStartMode workerStartMode(ExecutionContext*);
    void workerThreadCreated(ExecutionContext*, WorkerThread*, const KURL&);
    void workerThreadTerminated();
    void dispatchMessageFromWorker(const String&);
    void workerConsoleAgentEnabled();

    void connectToInspector(PageInspector*);
    void disconnectFromInspector(PageInspector*);
    void sendMessageToInspector(const String&);
    void writeTimelineStartedEvent(const String& sessionId, const String& workerId);

    const String& url() { return m_url; }

private:
    WorkerInspectorProxy();

    WorkerThread* m_workerThread;
    RawPtrWillBeMember<ExecutionContext> m_executionContext;
    PageInspector* m_pageInspector;
    String m_url;
};

} // namespace blink

#endif // WorkerInspectorProxy_h
