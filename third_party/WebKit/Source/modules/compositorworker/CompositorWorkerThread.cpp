// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/CompositorWorkerThread.h"

#include <memory>
#include "core/workers/InProcessWorkerObjectProxy.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "modules/compositorworker/CompositorWorkerGlobalScope.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/Assertions.h"

namespace blink {

std::unique_ptr<CompositorWorkerThread> CompositorWorkerThread::Create(
    PassRefPtr<WorkerLoaderProxy> worker_loader_proxy,
    InProcessWorkerObjectProxy& worker_object_proxy,
    double time_origin) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"),
               "CompositorWorkerThread::create");
  DCHECK(IsMainThread());
  return WTF::WrapUnique(new CompositorWorkerThread(
      std::move(worker_loader_proxy), worker_object_proxy, time_origin));
}

CompositorWorkerThread::CompositorWorkerThread(
    PassRefPtr<WorkerLoaderProxy> worker_loader_proxy,
    InProcessWorkerObjectProxy& worker_object_proxy,
    double time_origin)
    : AbstractAnimationWorkletThread(std::move(worker_loader_proxy),
                                     worker_object_proxy),
      worker_object_proxy_(worker_object_proxy),
      time_origin_(time_origin) {}

CompositorWorkerThread::~CompositorWorkerThread() {}

WorkerOrWorkletGlobalScope* CompositorWorkerThread::CreateWorkerGlobalScope(
    std::unique_ptr<WorkerThreadStartupData> startup_data) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"),
               "CompositorWorkerThread::createWorkerGlobalScope");
  return CompositorWorkerGlobalScope::Create(this, std::move(startup_data),
                                             time_origin_);
}

}  // namespace blink
