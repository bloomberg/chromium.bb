// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/url_loader_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "services/network/cors/cors_url_loader_factory.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/network_usage_accumulator.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/resource_scheduler_client.h"
#include "services/network/url_loader.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

constexpr int URLLoaderFactory::kMaxKeepaliveConnections;
constexpr int URLLoaderFactory::kMaxKeepaliveConnectionsPerProcess;
constexpr int URLLoaderFactory::kMaxKeepaliveConnectionsPerProcessForFetchAPI;

URLLoaderFactory::URLLoaderFactory(
    NetworkContext* context,
    mojom::URLLoaderFactoryParamsPtr params,
    scoped_refptr<ResourceSchedulerClient> resource_scheduler_client,
    cors::CorsURLLoaderFactory* cors_url_loader_factory)
    : context_(context),
      params_(std::move(params)),
      resource_scheduler_client_(std::move(resource_scheduler_client)),
      header_client_(std::move(params_->header_client)),
      cors_url_loader_factory_(cors_url_loader_factory) {
  DCHECK(context);
  DCHECK_NE(mojom::kInvalidProcessId, params_->process_id);

  if (context_->network_service()) {
    context_->network_service()->keepalive_statistics_recorder()->Register(
        params_->process_id);
  }
}

URLLoaderFactory::~URLLoaderFactory() {
  if (context_->network_service()) {
    context_->network_service()->keepalive_statistics_recorder()->Unregister(
        params_->process_id);
    // Reset bytes transferred for the process if this is the last
    // |URLLoaderFactory|.
    if (!context_->network_service()
             ->keepalive_statistics_recorder()
             ->HasRecordForProcess(params_->process_id)) {
      context_->network_service()
          ->network_usage_accumulator()
          ->ClearBytesTransferredForProcess(params_->process_id);
    }
  }
}

void URLLoaderFactory::CreateLoaderAndStart(
    mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& url_request,
    mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  std::string origin_string;
  bool has_origin = url_request.headers.GetHeader("Origin", &origin_string) &&
                    origin_string != "null";
  base::Optional<url::Origin> request_initiator = url_request.request_initiator;
  if (has_origin && request_initiator.has_value()) {
    url::Origin origin = url::Origin::Create(GURL(origin_string));
    bool origin_head_same_as_request_origin =
        request_initiator.value().IsSameOriginWith(origin);
    UMA_HISTOGRAM_BOOLEAN(
        "NetworkService.URLLoaderFactory.OriginHeaderSameAsRequestOrigin",
        origin_head_same_as_request_origin);
  }

  mojom::NetworkServiceClient* network_service_client = nullptr;
  base::WeakPtr<KeepaliveStatisticsRecorder> keepalive_statistics_recorder;
  base::WeakPtr<NetworkUsageAccumulator> network_usage_accumulator;
  if (context_->network_service()) {
    network_service_client = context_->network_service()->client();
    keepalive_statistics_recorder = context_->network_service()
                                        ->keepalive_statistics_recorder()
                                        ->AsWeakPtr();
    network_usage_accumulator =
        context_->network_service()->network_usage_accumulator()->AsWeakPtr();
  }

  bool exhausted = false;
  if (url_request.keepalive && keepalive_statistics_recorder) {
    // This logic comes from
    // content::ResourceDispatcherHostImpl::BeginRequestInternal.
    // This is needed because we want to know whether the request is initiated
    // by fetch() or not. We hope that we can unify these restrictions and
    // remove the reference to fetch_request_context_type in the future.
    constexpr uint32_t kInitiatedByFetchAPI = 8;
    const bool is_initiated_by_fetch_api =
        url_request.fetch_request_context_type == kInitiatedByFetchAPI;
    const auto& recorder = *keepalive_statistics_recorder;
    if (recorder.num_inflight_requests() >= kMaxKeepaliveConnections)
      exhausted = true;
    if (recorder.NumInflightRequestsPerProcess(params_->process_id) >=
        kMaxKeepaliveConnectionsPerProcess) {
      exhausted = true;
    }
    if (is_initiated_by_fetch_api &&
        recorder.NumInflightRequestsPerProcess(params_->process_id) >=
            kMaxKeepaliveConnectionsPerProcessForFetchAPI) {
      exhausted = true;
    }
  }

  if (!context_->CanCreateLoader(params_->process_id))
    exhausted = true;

  if (exhausted) {
    URLLoaderCompletionStatus status;
    status.error_code = net::ERR_INSUFFICIENT_RESOURCES;
    status.exists_in_cache = false;
    status.completion_time = base::TimeTicks::Now();
    client->OnComplete(status);
    return;
  }

  auto loader = std::make_unique<URLLoader>(
      context_->url_request_context(), network_service_client,
      base::BindOnce(&cors::CorsURLLoaderFactory::DestroyURLLoader,
                     base::Unretained(cors_url_loader_factory_)),
      std::move(request), options, url_request, std::move(client),
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation),
      params_.get(), request_id, resource_scheduler_client_,
      std::move(keepalive_statistics_recorder),
      std::move(network_usage_accumulator), header_client_.get());
  cors_url_loader_factory_->OnLoaderCreated(std::move(loader));
}

void URLLoaderFactory::Clone(mojom::URLLoaderFactoryRequest request) {
  NOTREACHED();
}

}  // namespace network
