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

std::unique_ptr<AnimationWorkletThread> AnimationWorkletThread::Create(
    PassRefPtr<WorkerLoaderProxy> worker_loader_proxy,
    WorkerReportingProxy& worker_reporting_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("animation-worklet"),
               "AnimationWorkletThread::create");
  DCHECK(IsMainThread());
  return WTF::WrapUnique(new AnimationWorkletThread(
      std::move(worker_loader_proxy), worker_reporting_proxy));
}

AnimationWorkletThread::AnimationWorkletThread(
    PassRefPtr<WorkerLoaderProxy> worker_loader_proxy,
    WorkerReportingProxy& worker_reporting_proxy)
    : AbstractAnimationWorkletThread(std::move(worker_loader_proxy),
                                     worker_reporting_proxy) {}

AnimationWorkletThread::~AnimationWorkletThread() {}

WorkerOrWorkletGlobalScope* AnimationWorkletThread::CreateWorkerGlobalScope(
    std::unique_ptr<WorkerThreadStartupData> startup_data) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("animation-worklet"),
               "AnimationWorkletThread::createWorkerGlobalScope");

  RefPtr<SecurityOrigin> security_origin =
      SecurityOrigin::Create(startup_data->script_url_);
  if (startup_data->starter_origin_privilege_data_)
    security_origin->TransferPrivilegesFrom(
        std::move(startup_data->starter_origin_privilege_data_));

  // TODO(ikilpatrick): The AnimationWorkletGlobalScope will need to store a
  // WorkerClients object for using a CompositorWorkerProxyClient object.
  return AnimationWorkletGlobalScope::Create(
      startup_data->script_url_, startup_data->user_agent_,
      security_origin.Release(), this->GetIsolate(), this);
}

}  // namespace blink
