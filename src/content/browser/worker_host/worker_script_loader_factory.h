// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_LOADER_FACTORY_H_
#define CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_LOADER_FACTORY_H_

#include "base/macros.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

class AppCacheHost;
class ServiceWorkerProviderHost;
class ResourceContext;
class WorkerScriptLoader;

// WorkerScriptLoaderFactory creates a WorkerScriptLoader to load the main
// script for a web worker (dedicated worker or shared worker), which follows
// redirects and sets the controller service worker on the web worker if needed.
// It's an error to call CreateLoaderAndStart() more than a total of one time
// across this object or any of its clones.
//
// This is created per one web worker. All functions of this class must be
// called on the IO thread.
class CONTENT_EXPORT WorkerScriptLoaderFactory
    : public network::mojom::URLLoaderFactory {
 public:
  // Returns the resource context, or nullptr during shutdown. Must be called on
  // the IO thread.
  using ResourceContextGetter = base::RepeatingCallback<ResourceContext*(void)>;

  // |loader_factory| is used to load the script if the load is not intercepted
  // by a feature like service worker. Typically it will load the script from
  // the NetworkService. However, it may internally contain non-NetworkService
  // factories used for non-http(s) URLs, e.g., a chrome-extension:// URL.
  WorkerScriptLoaderFactory(
      int process_id,
      base::WeakPtr<ServiceWorkerProviderHost> service_worker_provider_host,
      base::WeakPtr<AppCacheHost> appcache_host,
      const ResourceContextGetter& resource_context_getter,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory);
  ~WorkerScriptLoaderFactory() override;

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& resource_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(network::mojom::URLLoaderFactoryRequest request) override;

  base::WeakPtr<WorkerScriptLoader> GetScriptLoader() { return script_loader_; }

 private:
  const int process_id_;
  base::WeakPtr<ServiceWorkerProviderHost> service_worker_provider_host_;
  base::WeakPtr<AppCacheHost> appcache_host_;
  ResourceContextGetter resource_context_getter_;
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;

  // This is owned by StrongBinding associated with the given URLLoaderRequest,
  // and invalidated after request completion or failure.
  base::WeakPtr<WorkerScriptLoader> script_loader_;

  DISALLOW_COPY_AND_ASSIGN(WorkerScriptLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_LOADER_FACTORY_H_
