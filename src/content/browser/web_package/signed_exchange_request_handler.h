// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_REQUEST_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/public/common/resource_type.h"
#include "url/origin.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

class URLLoaderThrottle;
class SignedExchangeLoader;
class SignedExchangePrefetchMetricRecorder;

class SignedExchangeRequestHandler final : public NavigationLoaderInterceptor {
 public:
  using URLLoaderThrottlesGetter = base::RepeatingCallback<
      std::vector<std::unique_ptr<content::URLLoaderThrottle>>()>;

  static bool IsSupportedMimeType(const std::string& mime_type);

  SignedExchangeRequestHandler(
      uint32_t url_loader_options,
      int frame_tree_node_id,
      const base::UnguessableToken& devtools_navigation_token,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      URLLoaderThrottlesGetter url_loader_throttles_getter,
      scoped_refptr<SignedExchangePrefetchMetricRecorder> metric_recorder,
      std::string accept_langs);
  ~SignedExchangeRequestHandler() override;

  // NavigationLoaderInterceptor implementation
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      ResourceContext* resource_context,
      LoaderCallback callback,
      FallbackCallback fallback_callback) override;
  bool MaybeCreateLoaderForResponse(
      const network::ResourceRequest& request,
      const network::ResourceResponseHead& response,
      network::mojom::URLLoaderPtr* loader,
      network::mojom::URLLoaderClientRequest* client_request,
      ThrottlingURLLoader* url_loader,
      bool* skip_other_interceptors) override;

 private:
  void StartResponse(const network::ResourceRequest& resource_request,
                     network::mojom::URLLoaderRequest request,
                     network::mojom::URLLoaderClientPtr client);

  // Valid after MaybeCreateLoaderForResponse intercepts the request and until
  // the loader is re-bound to the new client for the redirected request in
  // StartResponse.
  std::unique_ptr<SignedExchangeLoader> signed_exchange_loader_;

  const uint32_t url_loader_options_;
  const int frame_tree_node_id_;
  const base::UnguessableToken devtools_navigation_token_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  URLLoaderThrottlesGetter url_loader_throttles_getter_;
  scoped_refptr<SignedExchangePrefetchMetricRecorder> metric_recorder_;
  const std::string accept_langs_;

  base::WeakPtrFactory<SignedExchangeRequestHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SignedExchangeRequestHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_REQUEST_HANDLER_H_
