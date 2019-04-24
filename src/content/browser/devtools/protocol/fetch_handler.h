// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FETCH_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FETCH_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/fetch.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace content {
class DevToolsAgentHostImpl;
class DevToolsIOContext;
class DevToolsURLLoaderInterceptor;
struct InterceptedRequestInfo;

namespace protocol {

class FetchHandler : public DevToolsDomainHandler, public Fetch::Backend {
 public:
  explicit FetchHandler(DevToolsIOContext* io_context);
  ~FetchHandler() override;

  static std::vector<FetchHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  bool MaybeCreateProxyForInterception(
      RenderFrameHostImpl* rfh,
      bool is_navigation,
      bool is_download,
      network::mojom::URLLoaderFactoryRequest* target_factory_request);

 private:
  // DevToolsDomainHandler
  void Wire(UberDispatcher* dispatcher) override;
  Response Disable() override;

  // Protocol methods.
  Response Enable(Maybe<Array<Fetch::RequestPattern>> patterns,
                  Maybe<bool> handleAuth) override;

  void FailRequest(const String& fetchId,
                   const String& errorReason,
                   std::unique_ptr<FailRequestCallback> callback) override;
  void FulfillRequest(
      const String& fetchId,
      int responseCode,
      std::unique_ptr<Array<Fetch::HeaderEntry>> responseHeaders,
      Maybe<Binary> body,
      Maybe<String> responsePhrase,
      std::unique_ptr<FulfillRequestCallback> callback) override;
  void ContinueRequest(
      const String& fetchId,
      Maybe<String> url,
      Maybe<String> method,
      Maybe<String> postData,
      Maybe<Array<Fetch::HeaderEntry>> headers,
      std::unique_ptr<ContinueRequestCallback> callback) override;
  void ContinueWithAuth(
      const String& fetchId,
      std::unique_ptr<protocol::Fetch::AuthChallengeResponse>
          authChallengeResponse,
      std::unique_ptr<ContinueWithAuthCallback> callback) override;
  void GetResponseBody(
      const String& fetchId,
      std::unique_ptr<GetResponseBodyCallback> callback) override;
  void TakeResponseBodyAsStream(
      const String& fetchId,
      std::unique_ptr<TakeResponseBodyAsStreamCallback> callback) override;

  void OnResponseBodyPipeTaken(
      std::unique_ptr<TakeResponseBodyAsStreamCallback> callback,
      Response response,
      mojo::ScopedDataPipeConsumerHandle pipe,
      const std::string& mime_type);

  void RequestIntercepted(std::unique_ptr<InterceptedRequestInfo> info);

  DevToolsIOContext* const io_context_;
  std::unique_ptr<Fetch::Frontend> frontend_;
  std::unique_ptr<DevToolsURLLoaderInterceptor> interceptor_;
  base::WeakPtrFactory<FetchHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FetchHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FETCH_HANDLER_H_
