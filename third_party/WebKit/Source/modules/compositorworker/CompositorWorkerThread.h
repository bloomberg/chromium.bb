// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorWorkerThread_h
#define CompositorWorkerThread_h

#include "core/workers/WorkerThread.h"

namespace blink {

class WorkerObjectProxy;

class CompositorWorkerThread final : public WorkerThread {
public:
    static PassRefPtr<CompositorWorkerThread> create(PassRefPtr<WorkerLoaderProxy>, WorkerObjectProxy&, double timeOrigin);
    virtual ~CompositorWorkerThread();

    WorkerObjectProxy& workerObjectProxy() const { return m_workerObjectProxy; }

private:
    CompositorWorkerThread(PassRefPtr<WorkerLoaderProxy>, WorkerObjectProxy&, double timeOrigin);

    // WorkerThread:
    PassRefPtrWillBeRawPtr<WorkerGlobalScope> createWorkerGlobalScope(PassOwnPtr<WorkerThreadStartupData>) override;
    WebThreadSupportingGC& backingThread() override;

    WorkerObjectProxy& m_workerObjectProxy;
    double m_timeOrigin;
    OwnPtr<WebThreadSupportingGC> m_thread;
};

} // namespace blink

#endif // CompositorWorkerThread_h
