// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/cors_url_loader_factory.h"

#include "base/bind.h"
#include "base/logging.h"
#include "net/base/load_flags.h"
#include "services/network/cors/cors_url_loader.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/resource_scheduler_client.h"
#include "services/network/url_loader_factory.h"

namespace network {

namespace cors {

CorsURLLoaderFactory::CorsURLLoaderFactory(
    NetworkContext* context,
    mojom::URLLoaderFactoryParamsPtr params,
    scoped_refptr<ResourceSchedulerClient> resource_scheduler_client,
    mojom::URLLoaderFactoryRequest request,
    const OriginAccessList* origin_access_list,
    std::unique_ptr<mojom::URLLoaderFactory> network_loader_factory_for_testing)
    : context_(context),
      disable_web_security_(params->disable_web_security),
      process_id_(params->process_id),
      origin_access_list_(origin_access_list) {
  DCHECK(context_);
  DCHECK(origin_access_list_);
  factory_bound_origin_access_list_ = std::make_unique<OriginAccessList>();
  if (params->factory_bound_allow_patterns.size()) {
    DCHECK(params->request_initiator_site_lock);
    factory_bound_origin_access_list_->SetAllowListForOrigin(
        *params->request_initiator_site_lock,
        params->factory_bound_allow_patterns);
  }
  network_loader_factory_ =
      network_loader_factory_for_testing
          ? std::move(network_loader_factory_for_testing)
          : std::make_unique<network::URLLoaderFactory>(
                context, std::move(params),
                std::move(resource_scheduler_client), this);

  bindings_.AddBinding(this, std::move(request));
  bindings_.set_connection_error_handler(base::BindRepeating(
      &CorsURLLoaderFactory::DeleteIfNeeded, base::Unretained(this)));
  preflight_controller_ = context_->cors_preflight_controller();
}

CorsURLLoaderFactory::CorsURLLoaderFactory(
    bool disable_web_security,
    std::unique_ptr<mojom::URLLoaderFactory> network_loader_factory,
    const base::RepeatingCallback<void(int)>& preflight_finalizer,
    const OriginAccessList* origin_access_list,
    uint32_t process_id)
    : disable_web_security_(disable_web_security),
      process_id_(process_id),
      network_loader_factory_(std::move(network_loader_factory)),
      preflight_finalizer_(preflight_finalizer),
      origin_access_list_(origin_access_list) {
  DCHECK(origin_access_list_);
  factory_bound_origin_access_list_ = std::make_unique<OriginAccessList>();
  // Ideally this should be per-profile, but per-factory would be enough for
  // this code path that is eventually removed.
  owned_preflight_controller_ = std::make_unique<PreflightController>();
  preflight_controller_ = owned_preflight_controller_.get();
}

CorsURLLoaderFactory::~CorsURLLoaderFactory() = default;

void CorsURLLoaderFactory::OnLoaderCreated(
    std::unique_ptr<mojom::URLLoader> loader) {
  if (context_)
    context_->LoaderCreated(process_id_);
  loaders_.insert(std::move(loader));
}

void CorsURLLoaderFactory::DestroyURLLoader(mojom::URLLoader* loader) {
  if (context_)
    context_->LoaderDestroyed(process_id_);
  auto it = loaders_.find(loader);
  DCHECK(it != loaders_.end());
  loaders_.erase(it);

  DeleteIfNeeded();
}

void CorsURLLoaderFactory::CreateLoaderAndStart(
    mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& resource_request,
    mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (!IsSane(context_, resource_request)) {
    client->OnComplete(URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
    return;
  }

  if (base::FeatureList::IsEnabled(features::kOutOfBlinkCors) &&
      !disable_web_security_) {
    auto loader = std::make_unique<CorsURLLoader>(
        std::move(request), routing_id, request_id, options,
        base::BindOnce(&CorsURLLoaderFactory::DestroyURLLoader,
                       base::Unretained(this)),
        resource_request, std::move(client), traffic_annotation,
        network_loader_factory_.get(), preflight_finalizer_,
        origin_access_list_, factory_bound_origin_access_list_.get(),
        preflight_controller_);
    auto* raw_loader = loader.get();
    OnLoaderCreated(std::move(loader));
    raw_loader->Start();
  } else {
    network_loader_factory_->CreateLoaderAndStart(
        std::move(request), routing_id, request_id, options, resource_request,
        std::move(client), traffic_annotation);
  }
}

void CorsURLLoaderFactory::Clone(mojom::URLLoaderFactoryRequest request) {
  // The cloned factories stop working when this factory is destructed.
  bindings_.AddBinding(this, std::move(request));
}

void CorsURLLoaderFactory::ClearBindings() {
  bindings_.CloseAllBindings();
}

void CorsURLLoaderFactory::DeleteIfNeeded() {
  if (!context_)
    return;
  if (bindings_.empty() && loaders_.empty())
    context_->DestroyURLLoaderFactory(this);
}

bool CorsURLLoaderFactory::IsSane(const NetworkContext* context,
                                  const ResourceRequest& request) {
  // CORS needs a proper origin (including a unique opaque origin). If the
  // request doesn't have one, CORS cannot work.
  if (!request.request_initiator &&
      request.fetch_request_mode != mojom::FetchRequestMode::kNavigate &&
      request.fetch_request_mode != mojom::FetchRequestMode::kNoCors) {
    LOG(WARNING) << "|fetch_request_mode| is " << request.fetch_request_mode
                 << ", but |request_initiator| is not set.";
    return false;
  }

  const auto load_flags_pattern = net::LOAD_DO_NOT_SAVE_COOKIES |
                                  net::LOAD_DO_NOT_SEND_COOKIES |
                                  net::LOAD_DO_NOT_SEND_AUTH_DATA;
  // The credentials mode and load_flags should match.
  if (request.fetch_credentials_mode == mojom::FetchCredentialsMode::kOmit &&
      (request.load_flags & load_flags_pattern) != load_flags_pattern) {
    LOG(WARNING) << "|fetch_credentials_mode| and |load_flags| contradict each "
                    "other.";
    return false;
  }

  if (context) {
    net::HttpRequestHeaders::Iterator header_iterator(
        request.cors_exempt_headers);
    const auto& allowed_exempt_headers = context->cors_exempt_header_list();
    while (header_iterator.GetNext()) {
      if (allowed_exempt_headers.find(header_iterator.name()) !=
          allowed_exempt_headers.end()) {
        continue;
      }
      LOG(WARNING) << "|cors_exempt_headers| contains unexpected key: "
                   << header_iterator.name();
      return false;
    }
  }

  // TODO(yhirano): If the request mode is "no-cors", the redirect mode should
  // be "follow".
  return true;
}

}  // namespace cors

}  // namespace network
