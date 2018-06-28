// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_shared_url_loader_factory.h"

#include "base/logging.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/cross_thread_shared_url_loader_factory_info.h"
#include "services/network/test/test_network_service_client.h"

namespace network {

TestSharedURLLoaderFactory::TestSharedURLLoaderFactory() {
  network::mojom::NetworkServicePtr network_service_ptr;
  network_service_ = network::NetworkService::Create(
      mojo::MakeRequest(&network_service_ptr), /*netlog=*/nullptr);
  network::mojom::NetworkContextParamsPtr context_params =
      network::mojom::NetworkContextParams::New();
  context_params->enable_data_url_support = true;
  network_service_ptr->CreateNetworkContext(
      mojo::MakeRequest(&network_context_), std::move(context_params));

  network::mojom::NetworkServiceClientPtr network_service_client_ptr;
  network_service_client_ = std::make_unique<TestNetworkServiceClient>(
      mojo::MakeRequest(&network_service_client_ptr));
  network_service_ptr->SetClient(std::move(network_service_client_ptr));

  mojom::URLLoaderFactoryParamsPtr params =
      mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  network_context_->CreateURLLoaderFactory(
      mojo::MakeRequest(&url_loader_factory_), std::move(params));
}

TestSharedURLLoaderFactory::~TestSharedURLLoaderFactory() {}

void TestSharedURLLoaderFactory::CreateLoaderAndStart(
    mojom::URLLoaderRequest loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& request,
    mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  url_loader_factory_->CreateLoaderAndStart(
      std::move(loader), routing_id, request_id, options, std::move(request),
      std::move(client), traffic_annotation);
}

void TestSharedURLLoaderFactory::Clone(mojom::URLLoaderFactoryRequest request) {
  NOTREACHED();
}

// SharedURLLoaderFactoryInfo implementation
std::unique_ptr<SharedURLLoaderFactoryInfo>
TestSharedURLLoaderFactory::Clone() {
  return std::make_unique<CrossThreadSharedURLLoaderFactoryInfo>(this);
}

}  // namespace network
