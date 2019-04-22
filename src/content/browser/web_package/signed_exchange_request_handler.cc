// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_request_handler.h"

#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "content/browser/web_package/signed_exchange_devtools_proxy.h"
#include "content/browser/web_package/signed_exchange_loader.h"
#include "content/browser/web_package/signed_exchange_prefetch_metric_recorder.h"
#include "content/browser/web_package/signed_exchange_reporter.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/common/throttling_url_loader.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"

namespace content {

// static
bool SignedExchangeRequestHandler::IsSupportedMimeType(
    const std::string& mime_type) {
  return mime_type == "application/signed-exchange";
}

SignedExchangeRequestHandler::SignedExchangeRequestHandler(
    uint32_t url_loader_options,
    int frame_tree_node_id,
    const base::UnguessableToken& devtools_navigation_token,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    URLLoaderThrottlesGetter url_loader_throttles_getter,
    scoped_refptr<SignedExchangePrefetchMetricRecorder> metric_recorder,
    std::string accept_langs)
    : url_loader_options_(url_loader_options),
      frame_tree_node_id_(frame_tree_node_id),
      devtools_navigation_token_(devtools_navigation_token),
      url_loader_factory_(url_loader_factory),
      url_loader_throttles_getter_(std::move(url_loader_throttles_getter)),
      metric_recorder_(std::move(metric_recorder)),
      accept_langs_(std::move(accept_langs)),
      weak_factory_(this) {
}

SignedExchangeRequestHandler::~SignedExchangeRequestHandler() = default;

void SignedExchangeRequestHandler::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    ResourceContext* resource_context,
    LoaderCallback callback,
    FallbackCallback fallback_callback) {
  if (!signed_exchange_loader_) {
    std::move(callback).Run({});
    return;
  }

  if (signed_exchange_loader_->fallback_url()) {
    DCHECK(tentative_resource_request.url.EqualsIgnoringRef(
        *signed_exchange_loader_->fallback_url()));
    signed_exchange_loader_ = nullptr;
    std::move(fallback_callback)
        .Run(false /* reset_subresource_loader_params */);
    return;
  }

  DCHECK(tentative_resource_request.url.EqualsIgnoringRef(
      *signed_exchange_loader_->inner_request_url()));
  std::move(callback).Run(
      base::BindOnce(&SignedExchangeRequestHandler::StartResponse,
                     weak_factory_.GetWeakPtr()));
}

bool SignedExchangeRequestHandler::MaybeCreateLoaderForResponse(
    const network::ResourceRequest& request,
    const network::ResourceResponseHead& response,
    network::mojom::URLLoaderPtr* loader,
    network::mojom::URLLoaderClientRequest* client_request,
    ThrottlingURLLoader* url_loader,
    bool* skip_other_interceptors) {
  DCHECK(!signed_exchange_loader_);
  if (!signed_exchange_utils::ShouldHandleAsSignedHTTPExchange(request.url,
                                                               response)) {
    return false;
  }

  network::mojom::URLLoaderClientPtr client;
  *client_request = mojo::MakeRequest(&client);

  base::RepeatingCallback<int(void)> frame_tree_node_id_getter =
      base::BindRepeating([](int id) { return id; }, frame_tree_node_id_);

  // This lets the SignedExchangeLoader directly returns an artificial redirect
  // to the downstream client without going through ThrottlingURLLoader, which
  // means some checks like SafeBrowsing may not see the redirect. Given that
  // the redirected request will be checked when it's restarted we suppose
  // this is fine.
  signed_exchange_loader_ = std::make_unique<SignedExchangeLoader>(
      request, response, std::move(client), url_loader->Unbind(),
      url_loader_options_, true /* should_redirect_to_fallback */,
      std::make_unique<SignedExchangeDevToolsProxy>(
          request.url, response, frame_tree_node_id_getter,
          devtools_navigation_token_, request.report_raw_headers),
      SignedExchangeReporter::MaybeCreate(request.url, request.referrer.spec(),
                                          response, frame_tree_node_id_getter),
      url_loader_factory_, url_loader_throttles_getter_,
      frame_tree_node_id_getter, metric_recorder_, accept_langs_);

  *skip_other_interceptors = true;
  return true;
}

void SignedExchangeRequestHandler::StartResponse(
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderRequest request,
    network::mojom::URLLoaderClientPtr client) {
  signed_exchange_loader_->ConnectToClient(std::move(client));
  mojo::MakeStrongBinding(std::move(signed_exchange_loader_),
                          std::move(request));
}

}  // namespace content
