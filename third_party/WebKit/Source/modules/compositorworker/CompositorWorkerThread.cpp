// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/CompositorWorkerThread.h"

#include <memory>
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/InProcessWorkerObjectProxy.h"
#include "modules/compositorworker/CompositorWorkerGlobalScope.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/Assertions.h"

namespace blink {

std::unique_ptr<CompositorWorkerThread> CompositorWorkerThread::Create(
    ThreadableLoadingContext* loading_context,
    InProcessWorkerObjectProxy& worker_object_proxy,
    double time_origin) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"),
               "CompositorWorkerThread::create");
  DCHECK(IsMainThread());
  return WTF::WrapUnique(new CompositorWorkerThread(
      loading_context, worker_object_proxy, time_origin));
}

CompositorWorkerThread::CompositorWorkerThread(
    ThreadableLoadingContext* loading_context,
    InProcessWorkerObjectProxy& worker_object_proxy,
    double time_origin)
    : AbstractAnimationWorkletThread(loading_context, worker_object_proxy),
      worker_object_proxy_(worker_object_proxy),
      time_origin_(time_origin) {}

CompositorWorkerThread::~CompositorWorkerThread() {}

WorkerOrWorkletGlobalScope* CompositorWorkerThread::CreateWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"),
               "CompositorWorkerThread::createWorkerGlobalScope");
  return CompositorWorkerGlobalScope::Create(this, std::move(creation_params),
                                             time_origin_);
}

}  // namespace blink
