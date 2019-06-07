// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CORS_CORS_URL_LOADER_H_
#define SERVICES_NETWORK_CORS_CORS_URL_LOADER_H_

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/cors/preflight_controller.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/cpp/cors/preflight_timing_info.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace cors {

class OriginAccessList;

// Wrapper class that adds cross-origin resource sharing capabilities
// (https://fetch.spec.whatwg.org/#http-cors-protocol), delegating requests as
// well as potential preflight requests to the supplied
// |network_loader_factory|. It is owned by the CorsURLLoaderFactory that
// created it.
class COMPONENT_EXPORT(NETWORK_SERVICE) CorsURLLoader
    : public mojom::URLLoader,
      public mojom::URLLoaderClient {
 public:
  using DeleteCallback = base::OnceCallback<void(mojom::URLLoader* loader)>;

  CorsURLLoader(
      mojom::URLLoaderRequest loader_request,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      DeleteCallback delete_callback,
      const ResourceRequest& resource_request,
      mojom::URLLoaderClientPtr client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojom::URLLoaderFactory* network_loader_factory,
      const base::Optional<url::Origin>& factory_bound_origin_,
      const OriginAccessList* origin_access_list,
      const OriginAccessList* factory_bound_origin_access_list,
      PreflightController* preflight_controller);

  ~CorsURLLoader() override;

  // Starts processing the request. This is expected to be called right after
  // the constructor.
  void Start();

  // mojom::URLLoader overrides:
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override;
  void ProceedWithResponse() override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // mojom::URLLoaderClient overrides:
  void OnReceiveResponse(const ResourceResponseHead& head) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        base::OnceCallback<void()> callback) override;
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const URLLoaderCompletionStatus& status) override;

  // Public for testing.
  //
  // Returns the response tainting value
  // (https://fetch.spec.whatwg.org/#concept-request-response-tainting) for a
  // request and the CORS flag, as specified in
  // https://fetch.spec.whatwg.org/#main-fetch.
  static network::mojom::FetchResponseType CalculateResponseTainting(
      const GURL& url,
      mojom::FetchRequestMode request_mode,
      const base::Optional<url::Origin>& origin,
      bool cors_flag,
      bool tainted_origin,
      const OriginAccessList* origin_access_list);

 private:
  void StartRequest();
  void StartNetworkRequest(
      int net_error,
      base::Optional<CorsErrorStatus> status,
      base::Optional<PreflightTimingInfo> preflight_timing_info);

  // Called when there is a connection error on the upstream pipe used for the
  // actual request.
  void OnUpstreamConnectionError();

  // Handles OnComplete() callback.
  void HandleComplete(const URLLoaderCompletionStatus& status);

  void OnConnectionError();

  void SetCorsFlagIfNeeded();

  static base::Optional<std::string> GetHeaderString(
      const ResourceResponseHead& response,
      const std::string& header_name);

  mojo::Binding<mojom::URLLoader> binding_;

  // We need to save these for redirect.
  const int32_t routing_id_;
  const int32_t request_id_;
  const uint32_t options_;

  DeleteCallback delete_callback_;

  // This raw URLLoaderFactory pointer is shared with the CorsURLLoaderFactory
  // that created and owns this object.
  mojom::URLLoaderFactory* network_loader_factory_;

  // For the actual request.
  mojom::URLLoaderPtr network_loader_;
  mojo::Binding<mojom::URLLoaderClient> network_client_binding_;
  ResourceRequest request_;

  // To be a URLLoader for the client.
  mojom::URLLoaderClientPtr forwarding_client_;

  // The last response URL, that is usually the requested URL, but can be
  // different if redirects happen.
  GURL last_response_url_;

  // https://fetch.spec.whatwg.org/#concept-request-response-tainting
  // As "response tainting" is subset of "response type", we use
  // mojom::FetchResponseType for convenience.
  mojom::FetchResponseType response_tainting_ =
      mojom::FetchResponseType::kBasic;

  // Holds the URL of a redirect if it's currently deferred, waiting for
  // forwarding_client_ to call FollowRedirect.
  std::unique_ptr<GURL> deferred_redirect_url_;

  // Corresponds to the Fetch spec, https://fetch.spec.whatwg.org/.
  bool fetch_cors_flag_ = false;

  net::RedirectInfo redirect_info_;

  // https://fetch.spec.whatwg.org/#concept-request-tainted-origin
  bool tainted_ = false;

  // https://fetch.spec.whatwg.org/#concept-request-redirect-count
  int redirect_count_ = 0;

  // We need to save this for redirect.
  net::MutableNetworkTrafficAnnotationTag traffic_annotation_;

  // Holds timing info if a preflight was made.
  std::vector<PreflightTimingInfo> preflight_timing_info_;

  const base::Optional<url::Origin> factory_bound_origin_;

  // Outlives |this|.
  const OriginAccessList* const origin_access_list_;
  const OriginAccessList* const factory_bound_origin_access_list_;
  PreflightController* preflight_controller_;

  // Used to run asynchronous class instance bound callbacks safely.
  base::WeakPtrFactory<CorsURLLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CorsURLLoader);
};

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_CORS_CORS_URL_LOADER_H_
