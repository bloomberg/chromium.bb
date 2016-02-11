// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerInspectorProxy_h
#define WorkerInspectorProxy_h

#include "core/CoreExport.h"
#include "core/workers/WorkerThread.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebTaskRunner.h"
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

    class PageInspector : public NoBaseWillBeGarbageCollectedFinalized<PageInspector> {
    public:
        virtual ~PageInspector() { }
        virtual void dispatchMessageFromWorker(const String&) = 0;
        virtual void workerConsoleAgentEnabled(WorkerGlobalScopeProxy*) = 0;
        DEFINE_INLINE_VIRTUAL_TRACE() { }
    };

    WorkerThreadStartMode workerStartMode(ExecutionContext*);
    void workerThreadCreated(ExecutionContext*, WorkerThread*, const KURL&);
    void workerThreadTerminated();

    void connectToInspector(PageInspector*);
    void disconnectFromInspector();
    void sendMessageToInspector(const String&);
    void writeTimelineStartedEvent(const String& sessionId, const String& workerId);

    PageInspector* pageInspector() const { return m_pageInspector; }

    void setWorkerGlobalScopeProxy(WorkerGlobalScopeProxy* proxy) { m_workerGlobalScopeProxy = proxy; }
    WorkerGlobalScopeProxy* workerGlobalScopeProxy() const { return m_workerGlobalScopeProxy; }

private:
    WorkerInspectorProxy();

    void addDebuggerTaskForWorker(const WebTraceLocation&, PassOwnPtr<WebTaskRunner::Task>);

    WorkerThread* m_workerThread;
    RawPtrWillBeMember<ExecutionContext> m_executionContext;
    RawPtrWillBeMember<WorkerInspectorProxy::PageInspector> m_pageInspector;
    WorkerGlobalScopeProxy* m_workerGlobalScopeProxy;
};

} // namespace blink

#endif // WorkerInspectorProxy_h
