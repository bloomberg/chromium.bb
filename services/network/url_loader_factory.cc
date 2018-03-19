// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/url_loader_factory.h"

#include "base/logging.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/resource_scheduler_client.h"
#include "services/network/url_loader.h"

namespace network {

constexpr int URLLoaderFactory::kMaxKeepaliveConnections;
constexpr int URLLoaderFactory::kMaxKeepaliveConnectionsPerProcess;
constexpr int URLLoaderFactory::kMaxKeepaliveConnectionsPerProcessForFetchAPI;

URLLoaderFactory::URLLoaderFactory(
    NetworkContext* context,
    uint32_t process_id,
    scoped_refptr<ResourceSchedulerClient> resource_scheduler_client)
    : context_(context),
      process_id_(process_id),
      resource_scheduler_client_(std::move(resource_scheduler_client)) {
  if (context_->network_service()) {
    context->network_service()->keepalive_statistics_recorder()->Register(
        process_id_);
  }
}

URLLoaderFactory::~URLLoaderFactory() {
  if (context_->network_service()) {
    context_->network_service()->keepalive_statistics_recorder()->Unregister(
        process_id_);
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
  DCHECK(!url_request.download_to_file);
  bool report_raw_headers = false;
  if (url_request.report_raw_headers) {
    const NetworkService* service = context_->network_service();
    report_raw_headers = service && service->HasRawHeadersAccess(process_id_);
    if (!report_raw_headers)
      DLOG(ERROR) << "Denying raw headers request by process " << process_id_;
  }

  mojom::NetworkServiceClient* network_service_client = nullptr;
  base::WeakPtr<KeepaliveStatisticsRecorder> keepalive_statistics_recorder;
  if (context_->network_service()) {
    network_service_client = context_->network_service()->client();
    keepalive_statistics_recorder = context_->network_service()
                                        ->keepalive_statistics_recorder()
                                        ->AsWeakPtr();
  }

  if (url_request.keepalive && keepalive_statistics_recorder) {
    // This logic comes from
    // content::ResourceDispatcherHostImpl::BeginRequestInternal.
    bool exhausted = false;
    // This is needed because we want to know whether the request is initiated
    // by fetch() or not. We hope that we can unify these restrictions and
    // remove the reference to fetch_request_context_type in the future.
    constexpr uint32_t kInitiatedByFetchAPI = 8;
    const bool is_initiated_by_fetch_api =
        url_request.fetch_request_context_type == kInitiatedByFetchAPI;
    const auto& recorder = *keepalive_statistics_recorder;
    if (recorder.num_inflight_requests() >= kMaxKeepaliveConnections)
      exhausted = true;
    if (recorder.NumInflightRequestsPerProcess(process_id_) >=
        kMaxKeepaliveConnectionsPerProcess) {
      exhausted = true;
    }
    if (is_initiated_by_fetch_api &&
        recorder.NumInflightRequestsPerProcess(process_id_) >=
            kMaxKeepaliveConnectionsPerProcessForFetchAPI) {
      exhausted = true;
    }
    if (exhausted) {
      if (client) {
        URLLoaderCompletionStatus status;
        status.error_code = net::ERR_INSUFFICIENT_RESOURCES;
        status.exists_in_cache = false;
        status.completion_time = base::TimeTicks::Now();
        client->OnComplete(status);
      }
      return;
    }
  }

  new URLLoader(
      context_->url_request_context_getter(), network_service_client,
      std::move(request), options, url_request, report_raw_headers,
      std::move(client),
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation),
      process_id_, request_id, resource_scheduler_client_,
      std::move(keepalive_statistics_recorder));
}

void URLLoaderFactory::Clone(mojom::URLLoaderFactoryRequest request) {
  context_->CreateURLLoaderFactory(std::move(request), process_id_,
                                   resource_scheduler_client_);
}

}  // namespace network
