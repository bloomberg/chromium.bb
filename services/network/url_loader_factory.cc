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
  new URLLoader(
      context_->url_request_context_getter(), network_service_client,
      std::move(request), options, url_request, report_raw_headers,
      std::move(client),
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation),
      process_id_, resource_scheduler_client_,
      std::move(keepalive_statistics_recorder));
}

void URLLoaderFactory::Clone(mojom::URLLoaderFactoryRequest request) {
  context_->CreateURLLoaderFactory(std::move(request), process_id_,
                                   resource_scheduler_client_);
}

}  // namespace network
