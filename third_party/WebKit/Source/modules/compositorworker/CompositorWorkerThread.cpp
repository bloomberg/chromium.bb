// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/compositorworker/CompositorWorkerThread.h"

#include "bindings/core/v8/V8Initializer.h"
#include "core/workers/WorkerObjectProxy.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "modules/compositorworker/CompositorWorkerGlobalScope.h"
#include "public/platform/Platform.h"

namespace blink {

PassRefPtr<CompositorWorkerThread> CompositorWorkerThread::create(PassRefPtr<WorkerLoaderProxy> workerLoaderProxy, WorkerObjectProxy& workerObjectProxy, double timeOrigin)
{
    ASSERT(isMainThread());
    return adoptRef(new CompositorWorkerThread(workerLoaderProxy, workerObjectProxy, timeOrigin));
}

CompositorWorkerThread::CompositorWorkerThread(PassRefPtr<WorkerLoaderProxy> workerLoaderProxy, WorkerObjectProxy& workerObjectProxy, double timeOrigin)
    : WorkerThread(workerLoaderProxy, workerObjectProxy)
    , m_workerObjectProxy(workerObjectProxy)
    , m_timeOrigin(timeOrigin)
{
}

CompositorWorkerThread::~CompositorWorkerThread()
{
}

PassRefPtrWillBeRawPtr<WorkerGlobalScope> CompositorWorkerThread::createWorkerGlobalScope(PassOwnPtr<WorkerThreadStartupData> startupData)
{
    return CompositorWorkerGlobalScope::create(this, startupData, m_timeOrigin);
}

WebThreadSupportingGC& CompositorWorkerThread::backingThread()
{
    if (!m_thread)
        m_thread = WebThreadSupportingGC::create("CompositorWorker Thread");
    return *m_thread.get();
}

} // namespace blink
