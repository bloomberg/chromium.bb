/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "modules/serviceworkers/ServiceWorkerThread.h"

#include <memory>

#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/WorkerBackingThread.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScope.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeProxy.h"
#include "modules/serviceworkers/ServiceWorkerInstalledScriptsManager.h"

namespace blink {

ServiceWorkerThread::ServiceWorkerThread(
    ThreadableLoadingContext* loading_context,
    ServiceWorkerGlobalScopeProxy* global_scope_proxy,
    std::unique_ptr<ServiceWorkerInstalledScriptsManager>
        installed_scripts_manager)
    : WorkerThread(loading_context, *global_scope_proxy),
      global_scope_proxy_(global_scope_proxy),
      worker_backing_thread_(
          WorkerBackingThread::Create("ServiceWorker Thread")),
      installed_scripts_manager_(std::move(installed_scripts_manager)) {}

ServiceWorkerThread::~ServiceWorkerThread() {
  global_scope_proxy_->Detach();
}

void ServiceWorkerThread::ClearWorkerBackingThread() {
  worker_backing_thread_ = nullptr;
}

InstalledScriptsManager* ServiceWorkerThread::GetInstalledScriptsManager() {
  return installed_scripts_manager_.get();
}

void ServiceWorkerThread::TerminateForTesting() {
  global_scope_proxy_->TerminateWorkerContext();
  WorkerThread::TerminateForTesting();
}

WorkerOrWorkletGlobalScope* ServiceWorkerThread::CreateWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  return ServiceWorkerGlobalScope::Create(this, std::move(creation_params),
                                          time_origin_);
}

}  // namespace blink
