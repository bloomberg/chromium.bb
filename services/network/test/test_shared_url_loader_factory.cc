// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_shared_url_loader_factory.h"

#include "base/notreached.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"

namespace network {

TestSharedURLLoaderFactory::TestSharedURLLoaderFactory(
    NetworkService* network_service,
    bool is_trusted) {
  url_request_context_ = std::make_unique<net::TestURLRequestContext>();
  mojo::Remote<mojom::NetworkContext> network_context;
  network_context_ = std::make_unique<NetworkContext>(
      network_service, network_context.BindNewPipeAndPassReceiver(),
      url_request_context_.get(),
      /*cors_exempt_header_list=*/std::vector<std::string>());
  mojom::URLLoaderFactoryParamsPtr params =
      mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  params->is_trusted = is_trusted;
  network_context_->CreateURLLoaderFactory(
      url_loader_factory_.BindNewPipeAndPassReceiver(), std::move(params));
}

TestSharedURLLoaderFactory::~TestSharedURLLoaderFactory() {}

void TestSharedURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<mojom::URLLoader> loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& request,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  num_created_loaders_++;
  url_loader_factory_->CreateLoaderAndStart(
      std::move(loader), routing_id, request_id, options, std::move(request),
      std::move(client), traffic_annotation);
}

void TestSharedURLLoaderFactory::Clone(
    mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) {
  NOTIMPLEMENTED();
}

// PendingSharedURLLoaderFactory implementation
std::unique_ptr<PendingSharedURLLoaderFactory>
TestSharedURLLoaderFactory::Clone() {
  return std::make_unique<CrossThreadPendingSharedURLLoaderFactory>(this);
}

}  // namespace network
