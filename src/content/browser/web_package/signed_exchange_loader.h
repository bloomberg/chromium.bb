// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_LOADER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_LOADER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "content/browser/web_package/signed_exchange_error.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "url/origin.h"

namespace net {
struct SHA256HashValue;
class SourceStream;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

class SignedExchangeDevToolsProxy;
class SignedExchangeHandler;
class SignedExchangeHandlerFactory;
class SignedExchangePrefetchMetricRecorder;
class SignedExchangeReporter;
class SignedExchangeValidityPinger;
class SourceStreamToDataPipe;
class URLLoaderThrottle;

// SignedExchangeLoader handles an origin-signed HTTP exchange response. It is
// created when a SignedExchangeRequestHandler recieves an origin-signed HTTP
// exchange response, and is owned by the handler until the StartLoaderCallback
// of SignedExchangeRequestHandler::StartResponse is called. After that, it is
// owned by the URLLoader mojo endpoint.
class CONTENT_EXPORT SignedExchangeLoader final
    : public network::mojom::URLLoaderClient,
      public network::mojom::URLLoader {
 public:
  using URLLoaderThrottlesGetter = base::RepeatingCallback<
      std::vector<std::unique_ptr<content::URLLoaderThrottle>>()>;

  // If |should_redirect_on_failure| is true, verification failure causes a
  // redirect to the fallback URL.
  SignedExchangeLoader(
      const network::ResourceRequest& outer_request,
      const network::ResourceResponseHead& outer_response_head,
      mojo::ScopedDataPipeConsumerHandle outer_response_body,
      network::mojom::URLLoaderClientPtr forwarding_client,
      network::mojom::URLLoaderClientEndpointsPtr endpoints,
      uint32_t url_loader_options,
      bool should_redirect_on_failure,
      std::unique_ptr<SignedExchangeDevToolsProxy> devtools_proxy,
      std::unique_ptr<SignedExchangeReporter> reporter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      URLLoaderThrottlesGetter url_loader_throttles_getter,
      base::RepeatingCallback<int(void)> frame_tree_node_id_getter,
      scoped_refptr<SignedExchangePrefetchMetricRecorder> metric_recorder,
      const std::string& accept_langs);
  ~SignedExchangeLoader() override;


  // network::mojom::URLLoaderClient implementation
  // Only OnStartLoadingResponseBody() and OnComplete() are called.
  void OnReceiveResponse(
      const network::ResourceResponseHead& response_head) override;
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const network::ResourceResponseHead& response_head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // network::mojom::URLLoader implementation
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override;
  void ProceedWithResponse() override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  void ConnectToClient(network::mojom::URLLoaderClientPtr client);

  const base::Optional<GURL>& fallback_url() const { return fallback_url_; }

  const base::Optional<GURL>& inner_request_url() const {
    return inner_request_url_;
  }

  // Returns the header integrity value of the loaded signed exchange if
  // available. This is available after OnReceiveRedirect() of
  // |forwarding_client| is called. Otherwise returns nullopt.
  base::Optional<net::SHA256HashValue> ComputeHeaderIntegrity() const;

  // Returns the signature expire time of the loaded signed exchange if
  // available. This is available after OnReceiveRedirect() of
  // |forwarding_client| is called. Otherwise returns a null Time.
  base::Time GetSignatureExpireTime() const;

  // Set nullptr to reset the mocking.
  static void SetSignedExchangeHandlerFactoryForTest(
      SignedExchangeHandlerFactory* factory);

 private:
  // Called from |signed_exchange_handler_| when it finds an origin-signed HTTP
  // exchange.
  void OnHTTPExchangeFound(
      SignedExchangeLoadResult result,
      net::Error error,
      const GURL& request_url,
      const network::ResourceResponseHead& resource_response,
      std::unique_ptr<net::SourceStream> payload_stream);

  void StartReadingBody();
  void FinishReadingBody(int result);
  void NotifyClientOnCompleteIfReady();
  void ReportLoadResult(SignedExchangeLoadResult result);

  const network::ResourceRequest outer_request_;

  // The outer response of signed HTTP exchange which was received from network.
  const network::ResourceResponseHead outer_response_head_;

  // This client is alive until OnHTTPExchangeFound() is called.
  network::mojom::URLLoaderClientPtr forwarding_client_;

  // This |url_loader_| is the pointer of the network URL loader.
  network::mojom::URLLoaderPtr url_loader_;
  // This binding connects |this| with the network URL loader.
  mojo::Binding<network::mojom::URLLoaderClient> url_loader_client_binding_;

  // This is pending until connected by ConnectToClient().
  network::mojom::URLLoaderClientPtr client_;

  // This URLLoaderClientRequest is used by ConnectToClient() to connect
  // |client_|.
  network::mojom::URLLoaderClientRequest pending_client_request_;

  std::unique_ptr<SignedExchangeHandler> signed_exchange_handler_;
  std::unique_ptr<SourceStreamToDataPipe> body_data_pipe_adapter_;

  // Kept around until ProceedWithResponse is called.
  mojo::ScopedDataPipeConsumerHandle pending_body_consumer_;

  const uint32_t url_loader_options_;
  const bool should_redirect_on_failure_;
  std::unique_ptr<SignedExchangeDevToolsProxy> devtools_proxy_;
  std::unique_ptr<SignedExchangeReporter> reporter_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  URLLoaderThrottlesGetter url_loader_throttles_getter_;
  base::RepeatingCallback<int(void)> frame_tree_node_id_getter_;
  scoped_refptr<SignedExchangePrefetchMetricRecorder> metric_recorder_;

  base::Optional<net::SSLInfo> ssl_info_;

  std::string content_type_;

  base::Optional<GURL> fallback_url_;
  base::Optional<GURL> inner_request_url_;

  struct OuterResponseLengthInfo {
    int64_t encoded_data_length;
    int64_t decoded_body_length;
  };
  // Set when URLLoaderClient::OnComplete() is called.
  base::Optional<OuterResponseLengthInfo> outer_response_length_info_;

  // Set when |body_data_pipe_adapter_| finishes loading the decoded body.
  base::Optional<int> decoded_body_read_result_;
  const std::string accept_langs_;

  std::unique_ptr<SignedExchangeValidityPinger> validity_pinger_;

  base::WeakPtrFactory<SignedExchangeLoader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SignedExchangeLoader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_LOADER_H_
