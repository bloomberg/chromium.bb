
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
#include "core/workers/InProcessWorkerObjectProxy.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

std::unique_ptr<DedicatedWorkerThread> DedicatedWorkerThread::Create(
    PassRefPtr<WorkerLoaderProxy> worker_loader_proxy,
    InProcessWorkerObjectProxy& worker_object_proxy,
    double time_origin) {
  return WTF::WrapUnique(new DedicatedWorkerThread(
      std::move(worker_loader_proxy), worker_object_proxy, time_origin));
}

DedicatedWorkerThread::DedicatedWorkerThread(
    PassRefPtr<WorkerLoaderProxy> worker_loader_proxy,
    InProcessWorkerObjectProxy& worker_object_proxy,
    double time_origin)
    : WorkerThread(std::move(worker_loader_proxy), worker_object_proxy),
      worker_backing_thread_(
          WorkerBackingThread::Create("DedicatedWorker Thread")),
      worker_object_proxy_(worker_object_proxy),
      time_origin_(time_origin) {}

DedicatedWorkerThread::~DedicatedWorkerThread() {}

WorkerOrWorkletGlobalScope* DedicatedWorkerThread::CreateWorkerGlobalScope(
    std::unique_ptr<WorkerThreadStartupData> startup_data) {
  return DedicatedWorkerGlobalScope::Create(this, std::move(startup_data),
                                            time_origin_);
}

void DedicatedWorkerThread::ClearWorkerBackingThread() {
  worker_backing_thread_ = nullptr;
}

}  // namespace blink
