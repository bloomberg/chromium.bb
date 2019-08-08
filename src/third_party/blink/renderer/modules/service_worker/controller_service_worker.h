// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_CONTROLLER_SERVICE_WORKER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_CONTROLLER_SERVICE_WORKER_H_

#include <utility>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom-blink.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace base {
class SequencedTaskRunner;
}

namespace blink {

class ServiceWorkerGlobalScope;

// An instance of this class is created on the service worker thread
// for its owner ServiceWorkerGlobalScope.
// This implements mojom::blink::ControllerServiceWorker and its Mojo endpoint
// is connected by each controllee and also by the ServiceWorkerProviderHost
// in the browser process.
// Subresource requests made by the controllees are sent to this class as
// Fetch events via the Mojo endpoints.
//
// TODO(kinuko): Implement self-killing timer, that does something similar to
// what ServiceWorkerVersion::StopWorkerIfIdle did in the browser process in
// the non-S13n codepath.
class ControllerServiceWorker : public mojom::blink::ControllerServiceWorker {
 public:
  ControllerServiceWorker(mojom::blink::ControllerServiceWorkerRequest request,
                          ServiceWorkerGlobalScope* service_worker_global_scope,
                          scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~ControllerServiceWorker() override;

  // mojom::blink::ControllerServiceWorker:
  void DispatchFetchEvent(
      mojom::blink::DispatchFetchEventParamsPtr params,
      mojom::blink::ServiceWorkerFetchResponseCallbackPtr response_callback,
      DispatchFetchEventCallback callback) override;
  void Clone(mojom::blink::ControllerServiceWorkerRequest request) override;

 private:
  // Connected by the ServiceWorkerProviderHost in the browser process
  // and by the controllees.
  mojo::BindingSet<mojom::blink::ControllerServiceWorker> bindings_;

  // |service_worker_global_scope_| is guaranteed to outlive |this| because
  // |service_worker_global_scope_| owns |this|, we just use WeakPersistent
  // rather than a raw pointer here because a non-garbage-collected class should
  // not store a raw pointer to an on-heap object.
  WeakPersistent<ServiceWorkerGlobalScope> service_worker_global_scope_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ControllerServiceWorker);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_CONTROLLER_SERVICE_WORKER_H_
