// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorWorkerThread_h
#define CompositorWorkerThread_h

#include "core/workers/WorkerThread.h"
#include "modules/ModulesExport.h"

namespace blink {

class InProcessWorkerObjectProxy;

// This class is overridden in unit-tests.
class MODULES_EXPORT CompositorWorkerThread final : public WorkerThread {
public:
    static PassOwnPtr<CompositorWorkerThread> create(PassRefPtr<WorkerLoaderProxy>, InProcessWorkerObjectProxy&, double timeOrigin);
    ~CompositorWorkerThread() override;

    InProcessWorkerObjectProxy& workerObjectProxy() const { return m_workerObjectProxy; }
    WorkerBackingThread& workerBackingThread() override;
    bool shouldAttachThreadDebugger() const override { return false; }

    static void resetSharedBackingThreadForTest();

protected:
    CompositorWorkerThread(PassRefPtr<WorkerLoaderProxy>, InProcessWorkerObjectProxy&, double timeOrigin);

    WorkerGlobalScope* createWorkerGlobalScope(PassOwnPtr<WorkerThreadStartupData>) override;

private:
    InProcessWorkerObjectProxy& m_workerObjectProxy;
    double m_timeOrigin;
};

} // namespace blink

#endif // CompositorWorkerThread_h
