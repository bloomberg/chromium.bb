// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/compositorworker/CompositorWorkerMessagingProxy.h"

#include "core/workers/WorkerThreadStartupData.h"
#include "modules/compositorworker/CompositorWorkerThread.h"

namespace blink {

CompositorWorkerMessagingProxy::CompositorWorkerMessagingProxy(Worker* worker, PassOwnPtrWillBeRawPtr<WorkerClients> workerClients)
    : WorkerMessagingProxy(worker, workerClients)
{
}

CompositorWorkerMessagingProxy::~CompositorWorkerMessagingProxy()
{
}

PassRefPtr<WorkerThread> CompositorWorkerMessagingProxy::createWorkerThread(double originTime, PassOwnPtrWillBeRawPtr<WorkerThreadStartupData> startupData)
{
    return CompositorWorkerThread::create(loaderProxy(), workerObjectProxy(), originTime, startupData);
}

} // namespace blink
