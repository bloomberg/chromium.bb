// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_prefetch_handler.h"

#include "base/callback.h"
#include "base/feature_list.h"
#include "content/browser/web_package/signed_exchange_devtools_proxy.h"
#include "content/browser/web_package/signed_exchange_loader.h"
#include "content/browser/web_package/signed_exchange_prefetch_metric_recorder.h"
#include "content/browser/web_package/signed_exchange_reporter.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

SignedExchangePrefetchHandler::SignedExchangePrefetchHandler(
    int frame_tree_node_id,
    const network::ResourceRequest& resource_request,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    mojo::PendingRemote<network::mojom::URLLoader> network_loader,
    mojo::PendingReceiver<network::mojom::URLLoaderClient>
        network_client_receiver,
    scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
    URLLoaderThrottlesGetter loader_throttles_getter,
    network::mojom::URLLoaderClient* forwarding_client,
    scoped_refptr<SignedExchangePrefetchMetricRecorder> metric_recorder,
    const std::string& accept_langs)
    : forwarding_client_(forwarding_client) {
  network::mojom::URLLoaderClientEndpointsPtr endpoints =
      network::mojom::URLLoaderClientEndpoints::New(
          std::move(network_loader), std::move(network_client_receiver));
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      std::move(network_loader_factory);

  // We need the SSLInfo as the prefetched signed exchange may be used as a main
  // frame main resource. Users can inspect the certificate for the main frame
  // using the info bubble in Omnibox.
  const uint32_t url_loader_options =
      network::mojom::kURLLoadOptionSendSSLInfoWithResponse;

  auto reporter = SignedExchangeReporter::MaybeCreate(
      resource_request.url, resource_request.referrer.spec(), *response_head,
      frame_tree_node_id);
  auto devtools_proxy = std::make_unique<SignedExchangeDevToolsProxy>(
      resource_request.url, response_head.Clone(), frame_tree_node_id,
      base::nullopt /* devtools_navigation_token */,
      resource_request.report_raw_headers);
  signed_exchange_loader_ = std::make_unique<SignedExchangeLoader>(
      resource_request, std::move(response_head), std::move(response_body),
      loader_client_receiver_.BindNewPipeAndPassRemote(), std::move(endpoints),
      url_loader_options, false /* should_redirect_to_fallback */,
      std::move(devtools_proxy), std::move(reporter),
      std::move(url_loader_factory), loader_throttles_getter,
      frame_tree_node_id, std::move(metric_recorder), accept_langs);
}

SignedExchangePrefetchHandler::~SignedExchangePrefetchHandler() = default;

mojo::PendingReceiver<network::mojom::URLLoaderClient>
SignedExchangePrefetchHandler::FollowRedirect(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver) {
  DCHECK(signed_exchange_loader_);
  mojo::PendingRemote<network::mojom::URLLoaderClient> client;
  auto pending_receiver = client.InitWithNewPipeAndPassReceiver();
  signed_exchange_loader_->ConnectToClient(std::move(client));
  mojo::MakeSelfOwnedReceiver(std::move(signed_exchange_loader_),
                              std::move(loader_receiver));
  return pending_receiver;
}

base::Optional<net::SHA256HashValue>
SignedExchangePrefetchHandler::ComputeHeaderIntegrity() const {
  if (!signed_exchange_loader_)
    return base::nullopt;
  return signed_exchange_loader_->ComputeHeaderIntegrity();
}

base::Time SignedExchangePrefetchHandler::GetSignatureExpireTime() const {
  if (!signed_exchange_loader_)
    return base::Time();
  return signed_exchange_loader_->GetSignatureExpireTime();
}

void SignedExchangePrefetchHandler::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head) {
  NOTREACHED();
}

void SignedExchangePrefetchHandler::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  forwarding_client_->OnReceiveRedirect(redirect_info, std::move(head));
}

void SignedExchangePrefetchHandler::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    base::OnceCallback<void()> callback) {
  NOTREACHED();
}

void SignedExchangePrefetchHandler::OnReceiveCachedMetadata(
    mojo_base::BigBuffer data) {
  NOTREACHED();
}

void SignedExchangePrefetchHandler::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  NOTREACHED();
}

void SignedExchangePrefetchHandler::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  NOTREACHED();
}

void SignedExchangePrefetchHandler::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  // We only reach here on error, since successful completion of the
  // outer sxg load should trigger redirect and land on ::OnReceiveRedirect.
  DCHECK_NE(net::OK, status.error_code);

  forwarding_client_->OnComplete(status);
}

}  // namespace content
