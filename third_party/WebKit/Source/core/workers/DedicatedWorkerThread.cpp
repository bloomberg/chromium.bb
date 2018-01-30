
/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/workers/DedicatedWorkerThread.h"

#include <memory>
#include "core/workers/DedicatedWorkerGlobalScope.h"
#include "core/workers/DedicatedWorkerObjectProxy.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/WorkerBackingThread.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

std::unique_ptr<DedicatedWorkerThread> DedicatedWorkerThread::Create(
    ThreadableLoadingContext* loading_context,
    DedicatedWorkerObjectProxy& worker_object_proxy) {
  return WTF::WrapUnique(
      new DedicatedWorkerThread(loading_context, worker_object_proxy));
}

DedicatedWorkerThread::DedicatedWorkerThread(
    ThreadableLoadingContext* loading_context,
    DedicatedWorkerObjectProxy& worker_object_proxy)
    : WorkerThread(loading_context, worker_object_proxy),
      worker_backing_thread_(WorkerBackingThread::Create(
          WebThreadCreationParams(GetThreadType()))),
      worker_object_proxy_(worker_object_proxy) {}

DedicatedWorkerThread::~DedicatedWorkerThread() = default;

void DedicatedWorkerThread::ClearWorkerBackingThread() {
  worker_backing_thread_ = nullptr;
}

WorkerOrWorkletGlobalScope* DedicatedWorkerThread::CreateWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  return new DedicatedWorkerGlobalScope(std::move(creation_params), this,
                                        time_origin_);
}

}  // namespace blink
