// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/url_loader_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/cookies/cookie_constants.h"
#include "net/url_request/url_request_context.h"
#include "services/network/cookie_manager.h"
#include "services/network/cookie_settings.h"
#include "services/network/cors/cors_url_loader_factory.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/load_info_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/trust_tokens/local_trust_token_operation_delegate_impl.h"
#include "services/network/trust_tokens/trust_token_request_helper_factory.h"
#include "services/network/url_loader.h"
#include "services/network/web_bundle/web_bundle_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace {

// The interval to send load updates.
constexpr auto kUpdateLoadStatesInterval = base::Milliseconds(250);

bool LoadInfoIsMoreInteresting(uint32_t a_load_state,
                               uint64_t a_upload_size,
                               uint32_t b_load_state,
                               uint64_t b_upload_size) {
  // Set |*_uploading_size| to be the size of the corresponding upload body if
  // it's currently being uploaded.

  uint64_t a_uploading_size = 0;
  if (a_load_state == net::LOAD_STATE_SENDING_REQUEST)
    a_uploading_size = a_upload_size;

  uint64_t b_uploading_size = 0;
  if (b_load_state == net::LOAD_STATE_SENDING_REQUEST)
    b_uploading_size = b_upload_size;

  if (a_uploading_size != b_uploading_size)
    return a_uploading_size > b_uploading_size;

  return a_load_state > b_load_state;
}

}  // namespace

constexpr int URLLoaderFactory::kMaxKeepaliveConnections;
constexpr int URLLoaderFactory::kMaxKeepaliveConnectionsPerTopLevelFrame;
constexpr int URLLoaderFactory::kMaxTotalKeepaliveRequestSize;

URLLoaderFactory::URLLoaderFactory(
    NetworkContext* context,
    mojom::URLLoaderFactoryParamsPtr params,
    scoped_refptr<ResourceSchedulerClient> resource_scheduler_client,
    cors::CorsURLLoaderFactory* cors_url_loader_factory)
    : context_(context),
      params_(std::move(params)),
      resource_scheduler_client_(std::move(resource_scheduler_client)),
      header_client_(std::move(params_->header_client)),
      cors_url_loader_factory_(cors_url_loader_factory),
      cookie_observer_(std::move(params_->cookie_observer)),
      url_loader_network_service_observer_(
          std::move(params_->url_loader_network_observer)),
      devtools_observer_(std::move(params_->devtools_observer)) {
  DCHECK(context);
  DCHECK_NE(mojom::kInvalidProcessId, params_->process_id);
  DCHECK(!params_->factory_override);
  // Only non-navigation IsolationInfos should be bound to URLLoaderFactories.
  DCHECK_EQ(net::IsolationInfo::RequestType::kOther,
            params_->isolation_info.request_type());
  DCHECK(!params_->automatically_assign_isolation_info ||
         params_->isolation_info.IsEmpty());

  if (!params_->top_frame_id) {
    params_->top_frame_id = base::UnguessableToken::Create();
  }

  if (context_->network_service()) {
    context_->network_service()->keepalive_statistics_recorder()->Register(
        *params_->top_frame_id);
  }
}

URLLoaderFactory::~URLLoaderFactory() {
  if (context_->network_service()) {
    context_->network_service()->keepalive_statistics_recorder()->Unregister(
        *params_->top_frame_id);
  }
}

void URLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& url_request,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  CreateLoaderAndStartWithSyncClient(
      std::move(receiver), request_id, options, url_request, std::move(client),
      /* sync_client= */ nullptr, traffic_annotation);
}

void URLLoaderFactory::Clone(
    mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) {
  NOTREACHED();
}

void URLLoaderFactory::CreateLoaderAndStartWithSyncClient(
    mojo::PendingReceiver<mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& url_request,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    base::WeakPtr<mojom::URLLoaderClient> sync_client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  // Requests with |trusted_params| when params_->is_trusted is not set should
  // have been rejected at the CorsURLLoader layer.
  DCHECK(!url_request.trusted_params || params_->is_trusted);

  std::string origin_string;
  bool has_origin = url_request.headers.GetHeader("Origin", &origin_string) &&
                    origin_string != "null";
  absl::optional<url::Origin> request_initiator = url_request.request_initiator;
  if (has_origin && request_initiator.has_value()) {
    url::Origin origin = url::Origin::Create(GURL(origin_string));
    bool origin_head_same_as_request_origin =
        request_initiator.value().IsSameOriginWith(origin);
    UMA_HISTOGRAM_BOOLEAN(
        "NetworkService.URLLoaderFactory.OriginHeaderSameAsRequestOrigin",
        origin_head_same_as_request_origin);
  }

  if (url_request.web_bundle_token_params.has_value() &&
      url_request.destination !=
          network::mojom::RequestDestination::kWebBundle) {
    mojo::Remote<mojom::TrustedHeaderClient> trusted_header_client;
    if (header_client_ && (options & mojom::kURLLoadOptionUseHeaderClient)) {
      // CORS preflight request must not come here.
      DCHECK(!(options & mojom::kURLLoadOptionAsCorsPreflight));
      header_client_->OnLoaderCreated(
          request_id, trusted_header_client.BindNewPipeAndPassReceiver());
    }

    // Load a subresource from a WebBundle.
    context_->GetWebBundleManager().StartSubresourceRequest(
        std::move(receiver), url_request, std::move(client),
        params_->process_id, std::move(trusted_header_client));
    return;
  }

  base::WeakPtr<KeepaliveStatisticsRecorder> keepalive_statistics_recorder;
  if (context_->network_service()) {
    keepalive_statistics_recorder = context_->network_service()
                                        ->keepalive_statistics_recorder()
                                        ->AsWeakPtr();
  }

  bool exhausted = false;
  if (!context_->CanCreateLoader(params_->process_id)) {
    exhausted = true;
  }

  int keepalive_request_size = 0;
  if (url_request.keepalive && keepalive_statistics_recorder) {
    const size_t url_size = url_request.url.spec().size();
    size_t headers_size = 0;

    net::HttpRequestHeaders merged_headers = url_request.headers;
    merged_headers.MergeFrom(url_request.cors_exempt_headers);

    for (const auto& pair : merged_headers.GetHeaderVector()) {
      headers_size += (pair.key.size() + pair.value.size());
    }

    keepalive_request_size = url_size + headers_size;

    const auto& top_frame_id = *params_->top_frame_id;
    const auto& recorder = *keepalive_statistics_recorder;

    if (!exhausted) {
      if (recorder.num_inflight_requests() >= kMaxKeepaliveConnections ||
          recorder.NumInflightRequestsPerTopLevelFrame(top_frame_id) >=
              kMaxKeepaliveConnectionsPerTopLevelFrame ||
          recorder.GetTotalRequestSizePerTopLevelFrame(top_frame_id) +
                  keepalive_request_size >
              kMaxTotalKeepaliveRequestSize) {
        exhausted = true;
      }
    }
  }

  if (exhausted) {
    URLLoaderCompletionStatus status;
    status.error_code = net::ERR_INSUFFICIENT_RESOURCES;
    status.exists_in_cache = false;
    status.completion_time = base::TimeTicks::Now();
    mojo::Remote<mojom::URLLoaderClient>(std::move(client))->OnComplete(status);
    return;
  }

  std::unique_ptr<TrustTokenRequestHelperFactory> trust_token_factory;
  if (url_request.trust_token_params) {
    trust_token_factory = std::make_unique<TrustTokenRequestHelperFactory>(
        context_->trust_token_store(),
        context_->network_service()->trust_token_key_commitments(),
        // It's safe to use Unretained because |context_| is guaranteed to
        // outlive the URLLoader that will own this
        // TrustTokenRequestHelperFactory.
        base::BindRepeating(&NetworkContext::client,
                            base::Unretained(context_)),
        // It's safe to use Unretained here because
        // NetworkContext::CookieManager outlives the URLLoaders associated with
        // the NetworkContext.
        base::BindRepeating(
            [](const CookieManager* manager) {
              return !manager->cookie_settings()
                          .are_third_party_cookies_blocked();
            },
            base::Unretained(context_->cookie_manager())));
  }

  mojo::PendingRemote<mojom::CookieAccessObserver> cookie_observer;
  if (url_request.trusted_params &&
      url_request.trusted_params->cookie_observer) {
    cookie_observer =
        std::move(const_cast<mojo::PendingRemote<mojom::CookieAccessObserver>&>(
            url_request.trusted_params->cookie_observer));
  }
  mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
      url_loader_network_observer;
  if (url_request.trusted_params &&
      url_request.trusted_params->url_loader_network_observer) {
    url_loader_network_observer =
        std::move(const_cast<
                  mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>&>(
            url_request.trusted_params->url_loader_network_observer));
  }

  mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer;
  if (url_request.trusted_params &&
      url_request.trusted_params->devtools_observer) {
    devtools_observer =
        std::move(const_cast<mojo::PendingRemote<mojom::DevToolsObserver>&>(
            url_request.trusted_params->devtools_observer));
  }

  mojo::PendingRemote<mojom::AcceptCHFrameObserver> accept_ch_frame_observer;
  if (url_request.trusted_params &&
      url_request.trusted_params->accept_ch_frame_observer) {
    accept_ch_frame_observer = std::move(
        const_cast<mojo::PendingRemote<mojom::AcceptCHFrameObserver>&>(
            url_request.trusted_params->accept_ch_frame_observer));
  }

  auto loader = std::make_unique<URLLoader>(
      context_->url_request_context(), this, context_->client(),
      base::BindOnce(&cors::CorsURLLoaderFactory::DestroyURLLoader,
                     base::Unretained(cors_url_loader_factory_)),
      std::move(receiver), options, url_request, std::move(client),
      std::move(sync_client),
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation),
      params_.get(), cors_url_loader_factory_->coep_reporter(), request_id,
      keepalive_request_size, context_->require_network_isolation_key(),
      resource_scheduler_client_, std::move(keepalive_statistics_recorder),
      header_client_.is_bound() ? header_client_.get() : nullptr,
      context_->origin_policy_manager(), std::move(trust_token_factory),
      context_->cors_origin_access_list(), std::move(cookie_observer),
      std::move(url_loader_network_observer), std::move(devtools_observer),
      std::move(accept_ch_frame_observer));

  cors_url_loader_factory_->OnURLLoaderCreated(std::move(loader));
}

mojom::DevToolsObserver* URLLoaderFactory::GetDevToolsObserver() const {
  if (devtools_observer_)
    return devtools_observer_.get();
  return nullptr;
}

mojom::CookieAccessObserver* URLLoaderFactory::GetCookieAccessObserver() const {
  if (cookie_observer_)
    return cookie_observer_.get();
  return nullptr;
}

mojom::URLLoaderNetworkServiceObserver*
URLLoaderFactory::GetURLLoaderNetworkServiceObserver() const {
  if (url_loader_network_service_observer_)
    return url_loader_network_service_observer_.get();
  if (!context_->network_service())
    return nullptr;
  return context_->network_service()
      ->GetDefaultURLLoaderNetworkServiceObserver();
}

void URLLoaderFactory::AckUpdateLoadInfo() {
  DCHECK(waiting_on_load_state_ack_);
  waiting_on_load_state_ack_ = false;
  MaybeStartUpdateLoadInfoTimer();
}

void URLLoaderFactory::MaybeStartUpdateLoadInfoTimer() {
  if (!params_->provide_loading_state_updates ||
      !GetURLLoaderNetworkServiceObserver() || waiting_on_load_state_ack_ ||
      update_load_info_timer_.IsRunning()) {
    return;
  }
  update_load_info_timer_.Start(FROM_HERE, kUpdateLoadStatesInterval, this,
                                &URLLoaderFactory::UpdateLoadInfo);
}

void URLLoaderFactory::UpdateLoadInfo() {
  DCHECK(!waiting_on_load_state_ack_);

  mojom::LoadInfoPtr most_interesting;
  URLLoader* most_interesting_url_loader = nullptr;

  SCOPED_UMA_HISTOGRAM_TIMER("NetworkService.URLLoaderFactory.UpdateLoadInfo");

  if (base::FeatureList::IsEnabled(features::kOptimizeUpdateLoadInfo)) {
    for (auto& loader : cors_url_loader_factory_->url_loaders()) {
      if (!most_interesting ||
          LoadInfoIsMoreInteresting(
              loader->GetLoadState(), loader->GetUploadProgress().size(),
              most_interesting->load_state, most_interesting->upload_size)) {
        most_interesting = loader->CreateLoadInfo();
        most_interesting_url_loader = loader.get();
      }
    }
  } else {
    for (auto* request : *context_->url_request_context()->url_requests()) {
      auto* loader = URLLoader::ForRequest(*request);
      if (!loader || loader->url_loader_factory() != this)
        continue;
      mojom::LoadInfoPtr load_info = loader->CreateLoadInfo();
      if (!most_interesting ||
          LoadInfoIsMoreInteresting(*load_info, *most_interesting)) {
        most_interesting = std::move(load_info);
        most_interesting_url_loader = loader;
      }
    }
  }

  if (most_interesting_url_loader) {
    most_interesting_url_loader->GetURLLoaderNetworkServiceObserver()
        ->OnLoadingStateUpdate(
            std::move(most_interesting),
            base::BindOnce(&URLLoaderFactory::AckUpdateLoadInfo,
                           base::Unretained(this)));
    waiting_on_load_state_ack_ = true;
  }
}

void URLLoaderFactory::OnBeforeURLRequest() {
  MaybeStartUpdateLoadInfoTimer();
}

}  // namespace network
