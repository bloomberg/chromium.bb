// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerInspectorProxy_h
#define WorkerInspectorProxy_h

#include "wtf/Forward.h"

namespace blink {

class ExecutionContext;
class KURL;
class WorkerThread;

// A proxy for talking to the worker inspector on the worker thread.
// All of these methods should be called on the main thread.
class WorkerInspectorProxy FINAL {
public:
    static PassOwnPtr<WorkerInspectorProxy> create();

    ~WorkerInspectorProxy();

    class PageInspector {
    public:
        virtual ~PageInspector() { }
        virtual void dispatchMessageFromWorker(const String&) = 0;
    };

    void workerThreadCreated(ExecutionContext*, WorkerThread*, const KURL&);
    void workerThreadTerminated();

    void connectToInspector(PageInspector*);
    void disconnectFromInspector();
    void sendMessageToInspector(const String&);
    void writeTimelineStartedEvent(const String& sessionId);

    PageInspector* pageInspector() const { return m_pageInspector; };

private:
    WorkerInspectorProxy();

    WorkerThread* m_workerThread;
    ExecutionContext* m_executionContext;
    WorkerInspectorProxy::PageInspector* m_pageInspector;
};

} // namespace blink

#endif // WorkerInspectorProxy_h
