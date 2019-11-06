// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/sec_fetch_site_resource_handler.h"

#include "content/browser/loader/resource_request_info_impl.h"
#include "services/network/sec_fetch_site.h"

namespace content {

SecFetchSiteResourceHandler::SecFetchSiteResourceHandler(
    net::URLRequest* request,
    std::unique_ptr<ResourceHandler> next_handler)
    : LayeredResourceHandler(request, std::move(next_handler)),
      factory_params_(CreateURLLoaderFactoryParams()) {}

SecFetchSiteResourceHandler::~SecFetchSiteResourceHandler() = default;

void SecFetchSiteResourceHandler::OnWillStart(
    const GURL& url,
    std::unique_ptr<ResourceController> controller) {
  SetHeader(nullptr);

  next_handler_->OnWillStart(url, std::move(controller));
}

void SecFetchSiteResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    network::ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  SetHeader(&redirect_info.new_url);

  next_handler_->OnRequestRedirected(redirect_info, response,
                                     std::move(controller));
}

void SecFetchSiteResourceHandler::SetHeader(const GURL* new_url_from_redirect) {
  network::SetSecFetchSiteHeader(request(), new_url_from_redirect,
                                 *factory_params_);
}

network::mojom::URLLoaderFactoryParamsPtr
SecFetchSiteResourceHandler::CreateURLLoaderFactoryParams() {
  auto result = network::mojom::URLLoaderFactoryParams::New();

  // Translate |info->GetChildID()| (an int, -1 designates a browser process)
  // into |result->process_id| (an uint32_t, 0 designates a browser process).
  ResourceRequestInfoImpl* info = GetRequestInfo();
  if (!info || info->GetChildID() == -1) {
    result->process_id = network::mojom::kBrowserProcessId;
  } else {
    result->process_id = info->GetChildID();
    DCHECK_NE(network::mojom::kBrowserProcessId, result->process_id);
  }

  // |request_initiator_site_lock| is not enforced in the legacy,
  // pre-NetworkService path, so below we plug in a lock that is obviously
  // compatible with the request initiator.
  result->request_initiator_site_lock = request()->initiator();

  // Note that some fields of |result| might be left at their default values at
  // this point.  This is okay - we only care about the two fields that
  // the network::SetSecFetchSiteHeader function will inspect.
  return result;
}

}  // namespace content
