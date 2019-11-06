// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_SEC_FETCH_SITE_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_SEC_FETCH_SITE_RESOURCE_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "content/browser/loader/layered_resource_handler.h"
#include "content/browser/loader/resource_controller.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace net {
struct RedirectInfo;
class URLRequest;
}  // namespace net

namespace content {

class SecFetchSiteResourceHandler : public LayeredResourceHandler {
 public:
  SecFetchSiteResourceHandler(net::URLRequest* request,
                              std::unique_ptr<ResourceHandler> next_handler);

 private:
  ~SecFetchSiteResourceHandler() override;
  void OnWillStart(const GURL& url,
                   std::unique_ptr<ResourceController> controller) override;
  void OnRequestRedirected(
      const net::RedirectInfo& redirect_info,
      network::ResourceResponse* response,
      std::unique_ptr<ResourceController> controller) override;

  // Creates URLLoaderFactoryParams with just enough information filled in to
  // satisfy the needs of network::SetSecFetchSiteHeader function.
  network::mojom::URLLoaderFactoryParamsPtr CreateURLLoaderFactoryParams();

  // Sets the Sec-Fetch-Site header (based on the method argument and also on
  // the state of the |url_request()| and on |factory_params_|).
  void SetHeader(const GURL* new_url_from_redirect);

  network::mojom::URLLoaderFactoryParamsPtr factory_params_;

  DISALLOW_COPY_AND_ASSIGN(SecFetchSiteResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_SEC_FETCH_SITE_RESOURCE_HANDLER_H_
