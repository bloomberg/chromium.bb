// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_FOR_WORKER_H_
#define CONTENT_RENDERER_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_FOR_WORKER_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider_type.mojom.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"

namespace content {

struct NavigationResponseOverrideParameters;

// The WebServiceWorkerNetworkProvider implementation used for shared
// workers.
//
// This class is only used for the main script request from the shadow page.
// Remove it when the shadow page is removed (https://crbug.com/538751).
class ServiceWorkerNetworkProviderForWorker final
    : public blink::WebServiceWorkerNetworkProvider {
 public:
  // Creates a new instance.
  // - |info|: provider info from the browser
  // - |script_loader_factory_info|: the factory for loading the worker's
  //   scripts
  // - |controller_info|: info about controller service worker
  // - |fallback_loader_factory|: the factory to use when a service worker falls
  //   back to network (unlike the default factory of this renderer, it skips
  //   AppCache)
  // - |is_secure_context|: whether this context is secure
  // - |response_override|: the main script response
  static std::unique_ptr<ServiceWorkerNetworkProviderForWorker> Create(
      blink::mojom::ServiceWorkerProviderInfoForWorkerPtr info,
      network::mojom::URLLoaderFactoryAssociatedPtrInfo
          script_loader_factory_info,
      blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
      scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory,
      bool is_secure_context,
      std::unique_ptr<NavigationResponseOverrideParameters> response_override);

  ServiceWorkerNetworkProviderForWorker(
      bool is_secure_context,
      std::unique_ptr<NavigationResponseOverrideParameters> response_override);
  ~ServiceWorkerNetworkProviderForWorker() override;

  // Implements WebServiceWorkerNetworkProvider.
  void WillSendRequest(blink::WebURLRequest& request) override;
  std::unique_ptr<blink::WebURLLoader> CreateURLLoader(
      const blink::WebURLRequest& request,
      std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
          task_runner_handle) override;
  blink::mojom::ControllerServiceWorkerMode IsControlledByServiceWorker()
      override;
  int64_t ControllerServiceWorkerID() override;
  void DispatchNetworkQuiet() override;

  ServiceWorkerProviderContext* context() { return context_.get(); }

 private:
  const bool is_secure_context_;
  std::unique_ptr<NavigationResponseOverrideParameters> response_override_;

  // |context_| is null if |this| is an invalid instance, in which case there is
  // no connection to the browser process.
  scoped_refptr<ServiceWorkerProviderContext> context_;

  // The URL loader factory for loading the worker's scripts.
  network::mojom::URLLoaderFactoryAssociatedPtr script_loader_factory_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_FOR_WORKER_H_
