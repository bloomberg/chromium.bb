/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ServiceWorkerGlobalScopeClientImpl_h
#define ServiceWorkerGlobalScopeClientImpl_h

#include <memory>
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerClientsInfo.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerSkipWaitingCallbacks.h"

namespace blink {

struct WebPaymentAppResponse;
class WebServiceWorkerContextClient;
class WebServiceWorkerResponse;
class WebURL;

class ServiceWorkerGlobalScopeClientImpl final
    : public GarbageCollectedFinalized<ServiceWorkerGlobalScopeClientImpl>,
      public ServiceWorkerGlobalScopeClient {
  USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerGlobalScopeClientImpl);

 public:
  static ServiceWorkerGlobalScopeClient* Create(WebServiceWorkerContextClient&);
  ~ServiceWorkerGlobalScopeClientImpl() override;

  void GetClient(const WebString&,
                 std::unique_ptr<WebServiceWorkerClientCallbacks>) override;
  void GetClients(const WebServiceWorkerClientQueryOptions&,
                  std::unique_ptr<WebServiceWorkerClientsCallbacks>) override;
  void OpenWindowForClients(
      const WebURL&,
      std::unique_ptr<WebServiceWorkerClientCallbacks>) override;
  void OpenWindowForPaymentHandler(
      const WebURL&,
      std::unique_ptr<WebServiceWorkerClientCallbacks>) override;
  void SetCachedMetadata(const WebURL&, const char*, size_t) override;
  void ClearCachedMetadata(const WebURL&) override;

  WebURL Scope() const override;

  void DidHandleActivateEvent(int event_id,
                              WebServiceWorkerEventResult,
                              double event_dispatch_time) override;
  void DidHandleBackgroundFetchAbortEvent(int event_id,
                                          WebServiceWorkerEventResult,
                                          double event_dispatch_time) override;
  void DidHandleBackgroundFetchClickEvent(int event_id,
                                          WebServiceWorkerEventResult,
                                          double event_dispatch_time) override;
  void DidHandleBackgroundFetchFailEvent(int event_id,
                                         WebServiceWorkerEventResult,
                                         double event_dispatch_time) override;
  void DidHandleBackgroundFetchedEvent(int event_id,
                                       WebServiceWorkerEventResult,
                                       double event_dispatch_time) override;
  void DidHandleExtendableMessageEvent(int event_id,
                                       WebServiceWorkerEventResult,
                                       double event_dispatch_time) override;
  void RespondToFetchEventWithNoResponse(int fetch_event_id,
                                         double event_dispatch_time) override;
  void RespondToFetchEvent(int fetch_event_id,
                           const WebServiceWorkerResponse&,
                           double event_dispatch_time) override;
  void RespondToFetchEventWithResponseStream(
      int fetch_event_id,
      const WebServiceWorkerResponse&,
      WebServiceWorkerStreamHandle*,
      double event_dispatch_time) override;
  void RespondToPaymentRequestEvent(int event_id,
                                    const WebPaymentAppResponse&,
                                    double event_dispatch_time) override;
  void DidHandleFetchEvent(int fetch_event_id,
                           WebServiceWorkerEventResult,
                           double event_dispatch_time) override;
  void DidHandleInstallEvent(int install_event_id,
                             WebServiceWorkerEventResult,
                             double event_dispatch_time) override;
  void DidHandleNotificationClickEvent(int event_id,
                                       WebServiceWorkerEventResult,
                                       double event_dispatch_time) override;
  void DidHandleNotificationCloseEvent(int event_id,
                                       WebServiceWorkerEventResult,
                                       double event_dispatch_time) override;
  void DidHandlePushEvent(int push_event_id,
                          WebServiceWorkerEventResult,
                          double event_dispatch_time) override;
  void DidHandleSyncEvent(int sync_event_id,
                          WebServiceWorkerEventResult,
                          double event_dispatch_time) override;
  void DidHandlePaymentRequestEvent(int payment_request_event_id,
                                    WebServiceWorkerEventResult,
                                    double event_dispatch_time) override;
  void PostMessageToClient(const WebString& client_uuid,
                           const WebString& message,
                           WebMessagePortChannelArray) override;
  void SkipWaiting(
      std::unique_ptr<WebServiceWorkerSkipWaitingCallbacks>) override;
  void Claim(std::unique_ptr<WebServiceWorkerClientsClaimCallbacks>) override;
  void Focus(const WebString& client_uuid,
             std::unique_ptr<WebServiceWorkerClientCallbacks>) override;
  void Navigate(const WebString& client_uuid,
                const WebURL&,
                std::unique_ptr<WebServiceWorkerClientCallbacks>) override;
  void RegisterForeignFetchScopes(int install_event_id,
                                  const WebVector<WebURL>& sub_scopes,
                                  const WebVector<WebSecurityOrigin>&) override;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    ServiceWorkerGlobalScopeClient::Trace(visitor);
  }

 private:
  explicit ServiceWorkerGlobalScopeClientImpl(WebServiceWorkerContextClient&);

  WebServiceWorkerContextClient& client_;
};

}  // namespace blink

#endif  // ServiceWorkerGlobalScopeClientImpl_h
