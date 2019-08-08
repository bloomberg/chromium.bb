// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_FOR_SERVICE_WORKER_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_FOR_SERVICE_WORKER_H_

#include <memory>

#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/web_url_request.h"

namespace content {

// The WebServiceWorkerNetworkProvider implementation for service worker
// execution contexts.
//
// This class is only used for the main script request from the shadow page.
// Remove it when the shadow page is removed (https://crbug.com/538751).
class ServiceWorkerNetworkProviderForServiceWorker final
    : public blink::WebServiceWorkerNetworkProvider {
 public:
  explicit ServiceWorkerNetworkProviderForServiceWorker(
      network::mojom::URLLoaderFactoryPtrInfo script_loader_factory_info);
  ~ServiceWorkerNetworkProviderForServiceWorker() override;

  // blink::WebServiceWorkerNetworkProvider:
  void WillSendRequest(blink::WebURLRequest& request) override;
  std::unique_ptr<blink::WebURLLoader> CreateURLLoader(
      const blink::WebURLRequest& request,
      std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
          task_runner_handle) override;
  blink::mojom::ControllerServiceWorkerMode IsControlledByServiceWorker()
      override;
  int64_t ControllerServiceWorkerID() override;
  void DispatchNetworkQuiet() override;

  network::mojom::URLLoaderFactory* script_loader_factory() {
    return script_loader_factory_.get();
  }

 private:
  // The URL loader factory for loading the service worker's scripts.
  network::mojom::URLLoaderFactoryPtr script_loader_factory_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_FOR_SERVICE_WORKER_H_
