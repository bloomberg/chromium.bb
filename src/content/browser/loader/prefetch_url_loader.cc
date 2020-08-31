// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/prefetch_url_loader.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache_adapter.h"
#include "content/browser/web_package/signed_exchange_prefetch_handler.h"
#include "content/browser/web_package/signed_exchange_prefetch_metric_recorder.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/public/common/content_features.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

constexpr char kSignedExchangeEnabledAcceptHeaderForPrefetch[] =
    "application/signed-exchange;v=b3;q=0.9,*/*;q=0.8";

}  // namespace

PrefetchURLLoader::PrefetchURLLoader(
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    int frame_tree_node_id,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
    URLLoaderThrottlesGetter url_loader_throttles_getter,
    BrowserContext* browser_context,
    scoped_refptr<SignedExchangePrefetchMetricRecorder>
        signed_exchange_prefetch_metric_recorder,
    scoped_refptr<PrefetchedSignedExchangeCache>
        prefetched_signed_exchange_cache,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
    const std::string& accept_langs,
    RecursivePrefetchTokenGenerator recursive_prefetch_token_generator)
    : frame_tree_node_id_(frame_tree_node_id),
      resource_request_(resource_request),
      network_loader_factory_(std::move(network_loader_factory)),
      forwarding_client_(std::move(client)),
      url_loader_throttles_getter_(url_loader_throttles_getter),
      signed_exchange_prefetch_metric_recorder_(
          std::move(signed_exchange_prefetch_metric_recorder)),
      accept_langs_(accept_langs),
      recursive_prefetch_token_generator_(
          std::move(recursive_prefetch_token_generator)),
      is_signed_exchange_handling_enabled_(
          signed_exchange_utils::IsSignedExchangeHandlingEnabled(
              browser_context)) {
  DCHECK(network_loader_factory_);

  if (is_signed_exchange_handling_enabled_) {
    // Set the SignedExchange accept header.
    // (https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#internet-media-type-applicationsigned-exchange).
    resource_request_.headers.SetHeader(
        net::HttpRequestHeaders::kAccept,
        kSignedExchangeEnabledAcceptHeaderForPrefetch);
    if (prefetched_signed_exchange_cache &&
        resource_request.is_signed_exchange_prefetch_cache_enabled) {
      prefetched_signed_exchange_cache_adapter_ =
          std::make_unique<PrefetchedSignedExchangeCacheAdapter>(
              std::move(prefetched_signed_exchange_cache),
              BrowserContext::GetBlobStorageContext(browser_context),
              resource_request.url, this);
    }
  }

  network_loader_factory_->CreateLoaderAndStart(
      loader_.BindNewPipeAndPassReceiver(), routing_id, request_id, options,
      resource_request_, client_receiver_.BindNewPipeAndPassRemote(),
      traffic_annotation);
  client_receiver_.set_disconnect_handler(base::BindOnce(
      &PrefetchURLLoader::OnNetworkConnectionError, base::Unretained(this)));
}

PrefetchURLLoader::~PrefetchURLLoader() = default;

void PrefetchURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const base::Optional<GURL>& new_url) {
  DCHECK(modified_headers.IsEmpty())
      << "Redirect with modified headers was not supported yet. "
         "crbug.com/845683";
  DCHECK(!new_url) << "Redirect with modified URL was not "
                      "supported yet. crbug.com/845683";
  if (signed_exchange_prefetch_handler_) {
    // Rebind |client_receiver_| and |loader_|.
    client_receiver_.Bind(signed_exchange_prefetch_handler_->FollowRedirect(
        loader_.BindNewPipeAndPassReceiver()));
    return;
  }

  DCHECK(loader_);
  loader_->FollowRedirect(
      removed_headers, net::HttpRequestHeaders() /* modified_headers */,
      net::HttpRequestHeaders() /* modified_cors_exempt_headers */,
      base::nullopt);
}

void PrefetchURLLoader::SetPriority(net::RequestPriority priority,
                                    int intra_priority_value) {
  if (loader_)
    loader_->SetPriority(priority, intra_priority_value);
}

void PrefetchURLLoader::PauseReadingBodyFromNet() {
  // TODO(kinuko): Propagate or handle the case where |loader_| is
  // detached (for SignedExchanges), see OnReceiveResponse.
  if (loader_)
    loader_->PauseReadingBodyFromNet();
}

void PrefetchURLLoader::ResumeReadingBodyFromNet() {
  // TODO(kinuko): Propagate or handle the case where |loader_| is
  // detached (for SignedExchanges), see OnReceiveResponse.
  if (loader_)
    loader_->ResumeReadingBodyFromNet();
}

void PrefetchURLLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response) {
  if (is_signed_exchange_handling_enabled_ &&
      signed_exchange_utils::ShouldHandleAsSignedHTTPExchange(
          resource_request_.url, *response)) {
    DCHECK(!signed_exchange_prefetch_handler_);
    if (prefetched_signed_exchange_cache_adapter_) {
      prefetched_signed_exchange_cache_adapter_->OnReceiveOuterResponse(
          response.Clone());
    }
    // Note that after this point this doesn't directly get upcalls from the
    // network. (Until |this| calls the handler's FollowRedirect.)
    signed_exchange_prefetch_handler_ =
        std::make_unique<SignedExchangePrefetchHandler>(
            frame_tree_node_id_, resource_request_, std::move(response),
            mojo::ScopedDataPipeConsumerHandle(), loader_.Unbind(),
            client_receiver_.Unbind(), network_loader_factory_,
            url_loader_throttles_getter_, this,
            signed_exchange_prefetch_metric_recorder_, accept_langs_);
    return;
  }

  // If the response is marked as a restricted cross-origin prefetch, we
  // populate the response's |recursive_prefetch_token| member with a unique
  // token. The renderer will propagate this token to recursive prefetches
  // coming from this response, in the form of preload headers. This token is
  // later used by the PrefetchURLLoaderService to recover the correct
  // NetworkIsolationKey to use when fetching the request. In the Signed
  // Exchange case, we do this after redirects from the outer response, because
  // we redirect back here for the inner response.
  if (resource_request_.load_flags & net::LOAD_RESTRICTED_PREFETCH) {
    DCHECK(!recursive_prefetch_token_generator_.is_null());
    base::UnguessableToken recursive_prefetch_token =
        std::move(recursive_prefetch_token_generator_).Run(resource_request_);
    response->recursive_prefetch_token = recursive_prefetch_token;
  }

  if (prefetched_signed_exchange_cache_adapter_ &&
      signed_exchange_prefetch_handler_) {
    prefetched_signed_exchange_cache_adapter_->OnReceiveInnerResponse(
        response.Clone());
  }

  forwarding_client_->OnReceiveResponse(std::move(response));
}

void PrefetchURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  if (prefetched_signed_exchange_cache_adapter_ &&
      signed_exchange_prefetch_handler_) {
    prefetched_signed_exchange_cache_adapter_->OnReceiveRedirect(
        redirect_info.new_url,
        signed_exchange_prefetch_handler_->ComputeHeaderIntegrity(),
        signed_exchange_prefetch_handler_->GetSignatureExpireTime());
  }

  resource_request_.url = redirect_info.new_url;
  resource_request_.site_for_cookies = redirect_info.new_site_for_cookies;
  resource_request_.referrer = GURL(redirect_info.new_referrer);
  resource_request_.referrer_policy = redirect_info.new_referrer_policy;
  forwarding_client_->OnReceiveRedirect(redirect_info, std::move(head));
}

void PrefetchURLLoader::OnUploadProgress(int64_t current_position,
                                         int64_t total_size,
                                         base::OnceCallback<void()> callback) {
  forwarding_client_->OnUploadProgress(current_position, total_size,
                                       std::move(callback));
}

void PrefetchURLLoader::OnReceiveCachedMetadata(mojo_base::BigBuffer data) {
  // Just drop this; we don't need to forward this to the renderer
  // for prefetch.
}

void PrefetchURLLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void PrefetchURLLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  if (prefetched_signed_exchange_cache_adapter_ &&
      signed_exchange_prefetch_handler_) {
    prefetched_signed_exchange_cache_adapter_->OnStartLoadingResponseBody(
        std::move(body));
    return;
  }

  // Just drain the original response's body here.
  DCHECK(!pipe_drainer_);
  pipe_drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(body));

  SendEmptyBody();
}

void PrefetchURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (prefetched_signed_exchange_cache_adapter_ &&
      signed_exchange_prefetch_handler_) {
    prefetched_signed_exchange_cache_adapter_->OnComplete(status);
    return;
  }

  SendOnComplete(status);
}

bool PrefetchURLLoader::SendEmptyBody() {
  // Send an empty response's body.
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  if (CreateDataPipe(nullptr, &producer, &consumer) != MOJO_RESULT_OK) {
    // No more resources available for creating a data pipe. Close the
    // connection, which will in turn make this loader destroyed.
    forwarding_client_->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    forwarding_client_.reset();
    client_receiver_.reset();
    return false;
  }
  forwarding_client_->OnStartLoadingResponseBody(std::move(consumer));
  return true;
}

void PrefetchURLLoader::SendOnComplete(
    const network::URLLoaderCompletionStatus& completion_status) {
  forwarding_client_->OnComplete(completion_status);
}

void PrefetchURLLoader::OnNetworkConnectionError() {
  // The network loader has an error; we should let the client know it's closed
  // by dropping this, which will in turn make this loader destroyed.
  forwarding_client_.reset();
}

}  // namespace content
