// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/prefetch_url_loader.h"

#include "base/feature_list.h"
#include "content/browser/web_package/signed_exchange_prefetch_handler.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/public/common/content_features.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content {

PrefetchURLLoader::PrefetchURLLoader(
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    base::RepeatingCallback<int(void)> frame_tree_node_id_getter,
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
    URLLoaderThrottlesGetter url_loader_throttles_getter,
    ResourceContext* resource_context,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter)
    : frame_tree_node_id_getter_(frame_tree_node_id_getter),
      url_(resource_request.url),
      report_raw_headers_(resource_request.report_raw_headers),
      load_flags_(resource_request.load_flags),
      throttling_profile_id_(resource_request.throttling_profile_id),
      network_loader_factory_(std::move(network_loader_factory)),
      client_binding_(this),
      forwarding_client_(std::move(client)),
      url_loader_throttles_getter_(url_loader_throttles_getter),
      resource_context_(resource_context),
      request_context_getter_(std::move(request_context_getter)) {
  DCHECK(network_loader_factory_);

  if (resource_request.request_initiator)
    request_initiator_ = *resource_request.request_initiator;

  network::mojom::URLLoaderClientPtr network_client;
  client_binding_.Bind(mojo::MakeRequest(&network_client));
  client_binding_.set_connection_error_handler(base::BindOnce(
      &PrefetchURLLoader::OnNetworkConnectionError, base::Unretained(this)));
  network_loader_factory_->CreateLoaderAndStart(
      mojo::MakeRequest(&loader_), routing_id, request_id, options,
      resource_request, std::move(network_client), traffic_annotation);
}

PrefetchURLLoader::~PrefetchURLLoader() = default;

void PrefetchURLLoader::FollowRedirect(
    const base::Optional<std::vector<std::string>>&
        to_be_removed_request_headers,
    const base::Optional<net::HttpRequestHeaders>& modified_request_headers) {
  DCHECK(!modified_request_headers.has_value()) << "Redirect with modified "
                                                   "headers was not supported "
                                                   "yet. crbug.com/845683";
  if (signed_exchange_prefetch_handler_) {
    // Rebind |client_binding_| and |loader_|.
    client_binding_.Bind(signed_exchange_prefetch_handler_->FollowRedirect(
        mojo::MakeRequest(&loader_)));
    return;
  }

  loader_->FollowRedirect(base::nullopt, base::nullopt);
}

void PrefetchURLLoader::ProceedWithResponse() {
  loader_->ProceedWithResponse();
}

void PrefetchURLLoader::SetPriority(net::RequestPriority priority,
                                    int intra_priority_value) {
  loader_->SetPriority(priority, intra_priority_value);
}

void PrefetchURLLoader::PauseReadingBodyFromNet() {
  loader_->PauseReadingBodyFromNet();
}

void PrefetchURLLoader::ResumeReadingBodyFromNet() {
  loader_->ResumeReadingBodyFromNet();
}

void PrefetchURLLoader::OnReceiveResponse(
    const network::ResourceResponseHead& response) {
  if (signed_exchange_utils::ShouldHandleAsSignedHTTPExchange(url_, response)) {
    DCHECK(!signed_exchange_prefetch_handler_);

    // Note that after this point this doesn't directly get upcalls from the
    // network. (Until |this| calls the handler's FollowRedirect.)
    signed_exchange_prefetch_handler_ =
        std::make_unique<SignedExchangePrefetchHandler>(
            frame_tree_node_id_getter_, report_raw_headers_, load_flags_,
            throttling_profile_id_, response, std::move(loader_),
            client_binding_.Unbind(), network_loader_factory_,
            request_initiator_, url_, url_loader_throttles_getter_,
            resource_context_, request_context_getter_, this);
    return;
  }
  forwarding_client_->OnReceiveResponse(response);
}

void PrefetchURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& head) {
  forwarding_client_->OnReceiveRedirect(redirect_info, head);
}

void PrefetchURLLoader::OnUploadProgress(int64_t current_position,
                                         int64_t total_size,
                                         base::OnceCallback<void()> callback) {
  forwarding_client_->OnUploadProgress(current_position, total_size,
                                       std::move(callback));
}

void PrefetchURLLoader::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  // Just drop this; we don't need to forward this to the renderer
  // for prefetch.
}

void PrefetchURLLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void PrefetchURLLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  // Just drain this here; we don't need to forward the body data to
  // the renderer for prefetch.
  DCHECK(!pipe_drainer_);
  pipe_drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(body));
}

void PrefetchURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  forwarding_client_->OnComplete(status);
}

void PrefetchURLLoader::OnNetworkConnectionError() {
  // The network loader has an error; we should let the client know it's closed
  // by dropping this, which will in turn make this loader destroyed.
  forwarding_client_.reset();
}

}  // namespace content
