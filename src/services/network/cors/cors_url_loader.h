// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CORS_CORS_URL_LOADER_H_
#define SERVICES_NETWORK_CORS_CORS_URL_LOADER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/log/net_log_with_source.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/cors/preflight_controller.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_completion_status.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

class URLLoaderFactory;

namespace cors {

class OriginAccessList;

// Wrapper class that adds cross-origin resource sharing capabilities
// (https://fetch.spec.whatwg.org/#http-cors-protocol), delegating requests as
// well as potential preflight requests to the supplied
// `network_loader_factory`. It is owned by the CorsURLLoaderFactory that
// created it.
class COMPONENT_EXPORT(NETWORK_SERVICE) CorsURLLoader
    : public mojom::URLLoader,
      public mojom::URLLoaderClient {
 public:
  using DeleteCallback = base::OnceCallback<void(CorsURLLoader* loader)>;

  // Raw pointer arguments must outlive the returned instance.
  CorsURLLoader(
      mojo::PendingReceiver<mojom::URLLoader> loader_receiver,
      int32_t process_id,
      int32_t request_id,
      uint32_t options,
      DeleteCallback delete_callback,
      const ResourceRequest& resource_request,
      bool ignore_isolated_world_origin,
      bool skip_cors_enabled_scheme_check,
      mojo::PendingRemote<mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojom::URLLoaderFactory* network_loader_factory,
      URLLoaderFactory* sync_network_loader_factory,
      const OriginAccessList* origin_access_list,
      PreflightController* preflight_controller,
      const base::flat_set<std::string>* allowed_exempt_headers,
      bool allow_any_cors_exempt_header,
      NonWildcardRequestHeadersSupport non_wildcard_request_headers_support,
      const net::IsolationInfo& isolation_info,
      mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer,
      const mojom::ClientSecurityState* factory_client_security_state);

  CorsURLLoader(const CorsURLLoader&) = delete;
  CorsURLLoader& operator=(const CorsURLLoader&) = delete;

  ~CorsURLLoader() override;

  // Starts processing the request. This is expected to be called right after
  // the constructor.
  void Start();

  // mojom::URLLoader overrides:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // mojom::URLLoaderClient overrides:
  void OnReceiveEarlyHints(mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(mojom::URLResponseHeadPtr head) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        base::OnceCallback<void()> callback) override;
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const URLLoaderCompletionStatus& status) override;

  static network::mojom::FetchResponseType CalculateResponseTaintingForTesting(
      const GURL& url,
      mojom::RequestMode request_mode,
      const absl::optional<url::Origin>& origin,
      const absl::optional<url::Origin>& isolated_world_origin,
      bool cors_flag,
      bool tainted_origin,
      const OriginAccessList& origin_access_list);

  static absl::optional<CorsErrorStatus> CheckRedirectLocationForTesting(
      const GURL& url,
      mojom::RequestMode request_mode,
      const absl::optional<url::Origin>& origin,
      bool cors_flag,
      bool tainted);

 private:
  void StartRequest();

  // Helper for `OnPreflightRequestComplete()`.
  absl::optional<URLLoaderCompletionStatus> ConvertPreflightResult(
      int net_error,
      absl::optional<CorsErrorStatus> status);

  void OnPreflightRequestComplete(int net_error,
                                  absl::optional<CorsErrorStatus> status,
                                  bool has_authorization_covered_by_wildcard);
  void StartNetworkRequest();

  // Called when there is a connection error on the upstream pipe used for the
  // actual request.
  void OnUpstreamConnectionError();

  // Reports the error represented by `status` to DevTools, if possible.
  // If `is_warning` is true, then the error is only reported as a warning.
  void ReportCorsErrorToDevTools(const CorsErrorStatus& status,
                                 bool is_warning = false);

  // Handles OnComplete() callback.
  void HandleComplete(const URLLoaderCompletionStatus& status);

  void OnMojoDisconnect();

  void SetCorsFlagIfNeeded();

  // Returns true if request's origin has special access to the destination URL
  // (via `origin_access_list_`).
  bool HasSpecialAccessToDestination() const;

  bool PassesTimingAllowOriginCheck(
      const mojom::URLResponseHead& response) const;

  // Returns the client security state that applies to this request.
  // May return nullptr.
  const mojom::ClientSecurityState* GetClientSecurityState() const;

  // Returns a clone of the value returned by `GetClientSecurityState()`.
  mojom::ClientSecurityStatePtr CloneClientSecurityState() const;

  // Returns whether preflight errors due exclusively to Private Network Access
  // checks should be ignored.
  //
  // This is used to soft-launch Private Network Access preflights: we send
  // preflights but do not require them to succeed.
  //
  // TODO(https://crbug.com/1268378): Remove this once it never returns true.
  bool ShouldIgnorePrivateNetworkAccessErrors() const;

  static absl::optional<std::string> GetHeaderString(
      const mojom::URLResponseHead& response,
      const std::string& header_name);

  mojo::Receiver<mojom::URLLoader> receiver_;

  // We need to save these for redirect, and DevTools.
  const int32_t process_id_;
  const int32_t request_id_;
  const uint32_t options_;

  DeleteCallback delete_callback_;

  // This raw URLLoaderFactory pointer is shared with the CorsURLLoaderFactory
  // that created and owns this object.
  raw_ptr<mojom::URLLoaderFactory> network_loader_factory_;
  // This has the same lifetime as `network_loader_factory_`, and should be used
  // when non-null to create optimized URLLoaders which can call URLLoaderClient
  // methods synchronously.
  raw_ptr<URLLoaderFactory> sync_network_loader_factory_;

  // For the actual request.
  mojo::Remote<mojom::URLLoader> network_loader_;
  // `sync_client_receiver_factory_` should be invalidated if this is ever
  // reset.
  mojo::Receiver<mojom::URLLoaderClient> network_client_receiver_{this};
  ResourceRequest request_;

  // To be a URLLoader for the client.
  mojo::Remote<mojom::URLLoaderClient> forwarding_client_;

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

  // https://fetch.spec.whatwg.org/#timing-allow-failed
  bool timing_allow_failed_flag_ = false;

  // We need to save this for redirect.
  net::MutableNetworkTrafficAnnotationTag traffic_annotation_;

  // Outlives `this`.
  const raw_ptr<const OriginAccessList> origin_access_list_;
  raw_ptr<PreflightController> preflight_controller_;
  raw_ptr<const base::flat_set<std::string>> allowed_exempt_headers_;

  // Flag to specify if the CORS-enabled scheme check should be applied.
  const bool skip_cors_enabled_scheme_check_;

  const bool allow_any_cors_exempt_header_;

  const NonWildcardRequestHeadersSupport non_wildcard_request_headers_support_;

  net::IsolationInfo isolation_info_;

  // The client security state set on the factory that created this loader.
  //
  // If non-null, this is used over any request-specific client security state
  // passed in `request_.trusted_params`.
  //
  // NOTE: A default value is set here in order to avoid any risk of undefined
  // behavior, but it should never be used since the constructor always
  // initializes this member explicitly.
  raw_ptr<const mojom::ClientSecurityState> factory_client_security_state_ =
      nullptr;

  bool has_authorization_covered_by_wildcard_ = false;

  // If set to true, then any and all errors raised by subsequent preflight
  // requests are ignored.
  //
  // This is used to soft-launch Private Network Access preflights. In some
  // cases, the only reason we send a preflight is because of Private Network
  // Access. Errors that arise then would never have been noticed if we had not
  // sent the preflight, so we ignore them all.
  //
  // INVARIANT: if this is true, then
  // `ShouldIgnorePrivateNetworkAccessErrors()` is also true.
  //
  // TODO(https://crbug.com/1268378): Remove this along with
  // `should_ignore_private_network_access_errors_`.
  bool should_ignore_preflight_errors_ = false;

  mojo::Remote<mojom::DevToolsObserver> devtools_observer_;

  net::NetLogWithSource net_log_;

  // Used to provide weak pointers of this class for synchronously calling
  // URLLoaderClient methods. This should be reset any time
  // `network_client_receiver_` is reset.
  base::WeakPtrFactory<CorsURLLoader> sync_client_receiver_factory_{this};

  // Used to run asynchronous class instance bound callbacks safely.
  base::WeakPtrFactory<CorsURLLoader> weak_factory_{this};
};

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_CORS_CORS_URL_LOADER_H_
