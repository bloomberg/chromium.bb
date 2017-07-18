// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AnimationWorkletThread.h"

#include "core/workers/GlobalScopeCreationParams.h"
#include "modules/compositorworker/AnimationWorkletGlobalScope.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

std::unique_ptr<AnimationWorkletThread> AnimationWorkletThread::Create(
    ThreadableLoadingContext* loading_context,
    WorkerReportingProxy& worker_reporting_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("animation-worklet"),
               "AnimationWorkletThread::create");
  DCHECK(IsMainThread());
  return WTF::WrapUnique(
      new AnimationWorkletThread(loading_context, worker_reporting_proxy));
}

AnimationWorkletThread::AnimationWorkletThread(
    ThreadableLoadingContext* loading_context,
    WorkerReportingProxy& worker_reporting_proxy)
    : AbstractAnimationWorkletThread(loading_context, worker_reporting_proxy) {}

AnimationWorkletThread::~AnimationWorkletThread() {}

WorkerOrWorkletGlobalScope* AnimationWorkletThread::CreateWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("animation-worklet"),
               "AnimationWorkletThread::createWorkerGlobalScope");

  RefPtr<SecurityOrigin> security_origin =
      SecurityOrigin::Create(creation_params->script_url);
  if (creation_params->starter_origin_privilege_data) {
    security_origin->TransferPrivilegesFrom(
        std::move(creation_params->starter_origin_privilege_data));
  }

  return AnimationWorkletGlobalScope::Create(
      creation_params->script_url, creation_params->user_agent,
      std::move(security_origin), this->GetIsolate(), this,
      creation_params->worker_clients);
}

}  // namespace blink
