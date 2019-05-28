// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/controller_service_worker.h"

#include "base/sequenced_task_runner.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"

namespace blink {

ControllerServiceWorker::ControllerServiceWorker(
    mojom::blink::ControllerServiceWorkerRequest request,
    ServiceWorkerGlobalScope* service_worker_global_scope,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : service_worker_global_scope_(service_worker_global_scope),
      task_runner_(std::move(task_runner)) {
  bindings_.AddBinding(this, std::move(request), task_runner_);
}

ControllerServiceWorker::~ControllerServiceWorker() = default;

void ControllerServiceWorker::DispatchFetchEvent(
    mojom::blink::DispatchFetchEventParamsPtr params,
    mojom::blink::ServiceWorkerFetchResponseCallbackPtr response_callback,
    DispatchFetchEventCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  service_worker_global_scope_->DispatchOrQueueFetchEvent(
      std::move(params), std::move(response_callback), std::move(callback));
}

void ControllerServiceWorker::Clone(
    mojom::blink::ControllerServiceWorkerRequest request) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  bindings_.AddBinding(this, std::move(request), task_runner_);
}

}  // namespace blink
