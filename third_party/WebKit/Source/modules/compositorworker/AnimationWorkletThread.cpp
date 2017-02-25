// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AnimationWorkletThread.h"

#include "core/workers/WorkerThreadStartupData.h"
#include "modules/compositorworker/AnimationWorkletGlobalScope.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/PtrUtil.h"

namespace blink {

std::unique_ptr<AnimationWorkletThread> AnimationWorkletThread::create(
    PassRefPtr<WorkerLoaderProxy> workerLoaderProxy,
    WorkerReportingProxy& workerReportingProxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("animation-worklet"),
               "AnimationWorkletThread::create");
  DCHECK(isMainThread());
  return WTF::wrapUnique(new AnimationWorkletThread(
      std::move(workerLoaderProxy), workerReportingProxy));
}

AnimationWorkletThread::AnimationWorkletThread(
    PassRefPtr<WorkerLoaderProxy> workerLoaderProxy,
    WorkerReportingProxy& workerReportingProxy)
    : AbstractAnimationWorkletThread(std::move(workerLoaderProxy),
                                     workerReportingProxy) {}

AnimationWorkletThread::~AnimationWorkletThread() {}

WorkerOrWorkletGlobalScope* AnimationWorkletThread::createWorkerGlobalScope(
    std::unique_ptr<WorkerThreadStartupData> startupData) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("animation-worklet"),
               "AnimationWorkletThread::createWorkerGlobalScope");

  RefPtr<SecurityOrigin> securityOrigin =
      SecurityOrigin::create(startupData->m_scriptURL);
  if (startupData->m_starterOriginPrivilegeData)
    securityOrigin->transferPrivilegesFrom(
        std::move(startupData->m_starterOriginPrivilegeData));

  // TODO(ikilpatrick): The AnimationWorkletGlobalScope will need to store a
  // WorkerClients object for using a CompositorWorkerProxyClient object.
  return AnimationWorkletGlobalScope::create(
      startupData->m_scriptURL, startupData->m_userAgent,
      securityOrigin.release(), this->isolate(), this);
}

}  // namespace blink
