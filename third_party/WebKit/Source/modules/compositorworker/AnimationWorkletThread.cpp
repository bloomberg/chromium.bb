// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AnimationWorkletThread.h"

#include "core/workers/WorkerThreadStartupData.h"
#include "platform/TraceEvent.h"
#include "wtf/PtrUtil.h"

namespace blink {

std::unique_ptr<AnimationWorkletThread> AnimationWorkletThread::create(PassRefPtr<WorkerLoaderProxy> workerLoaderProxy, WorkerReportingProxy& workerReportingProxy)
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("animation-worklet"), "AnimationWorkletThread::create");
    DCHECK(isMainThread());
    return wrapUnique(new AnimationWorkletThread(workerLoaderProxy, workerReportingProxy));
}

AnimationWorkletThread::AnimationWorkletThread(PassRefPtr<WorkerLoaderProxy> workerLoaderProxy, WorkerReportingProxy& workerReportingProxy)
    : AbstractAnimationWorkletThread(workerLoaderProxy, workerReportingProxy)
{
}

AnimationWorkletThread::~AnimationWorkletThread()
{
}

WorkerOrWorkletGlobalScope* AnimationWorkletThread::createWorkerGlobalScope(std::unique_ptr<WorkerThreadStartupData> startupData)
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("animation-worklet"), "AnimationWorkletThread::createWorkerGlobalScope");
    // TODO(ikilpatrick): implement.
    return nullptr;
}

} // namespace blink
