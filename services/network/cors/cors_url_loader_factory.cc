// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/cors_url_loader_factory.h"

#include "net/base/load_flags.h"
#include "services/network/cors/cors_url_loader.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/resource_scheduler_client.h"
#include "services/network/url_loader_factory.h"

namespace network {

namespace cors {

CORSURLLoaderFactory::CORSURLLoaderFactory(
    NetworkContext* context,
    mojom::URLLoaderFactoryParamsPtr params,
    scoped_refptr<ResourceSchedulerClient> resource_scheduler_client,
    mojom::URLLoaderFactoryRequest request)
    : context_(context),
      network_loader_factory_(std::make_unique<::network::URLLoaderFactory>(
          context,
          std::move(params),
          std::move(resource_scheduler_client),
          this)) {
  DCHECK(context_);
  bindings_.AddBinding(this, std::move(request));
  bindings_.set_connection_error_handler(base::BindRepeating(
      &CORSURLLoaderFactory::DeleteIfNeeded, base::Unretained(this)));
}

CORSURLLoaderFactory::CORSURLLoaderFactory(
    std::unique_ptr<mojom::URLLoaderFactory> network_loader_factory,
    const base::RepeatingCallback<void(int)>& preflight_finalizer)
    : network_loader_factory_(std::move(network_loader_factory)),
      preflight_finalizer_(preflight_finalizer) {}

CORSURLLoaderFactory::~CORSURLLoaderFactory() = default;

void CORSURLLoaderFactory::OnLoaderCreated(
    std::unique_ptr<mojom::URLLoader> loader) {
  loaders_.insert(std::move(loader));
}

void CORSURLLoaderFactory::DestroyURLLoader(mojom::URLLoader* loader) {
  auto it = loaders_.find(loader);
  DCHECK(it != loaders_.end());
  loaders_.erase(it);

  DeleteIfNeeded();
}

void CORSURLLoaderFactory::CreateLoaderAndStart(
    mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& resource_request,
    mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (base::FeatureList::IsEnabled(features::kOutOfBlinkCORS)) {
    auto loader = std::make_unique<CORSURLLoader>(
        std::move(request), routing_id, request_id, options,
        base::BindOnce(&CORSURLLoaderFactory::DestroyURLLoader,
                       base::Unretained(this)),
        resource_request, std::move(client), traffic_annotation,
        network_loader_factory_.get(), preflight_finalizer_);
    auto* raw_loader = loader.get();
    OnLoaderCreated(std::move(loader));
    raw_loader->Start();
  } else {
    network_loader_factory_->CreateLoaderAndStart(
        std::move(request), routing_id, request_id, options, resource_request,
        std::move(client), traffic_annotation);
  }
}

void CORSURLLoaderFactory::Clone(mojom::URLLoaderFactoryRequest request) {
  // The cloned factories stop working when this factory is destructed.
  bindings_.AddBinding(this, std::move(request));
}

void CORSURLLoaderFactory::DeleteIfNeeded() {
  if (!context_)
    return;
  if (bindings_.empty() && loaders_.empty())
    context_->DestroyURLLoaderFactory(this);
}

}  // namespace cors

}  // namespace network
