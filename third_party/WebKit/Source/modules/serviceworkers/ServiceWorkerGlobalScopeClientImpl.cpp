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

#include "modules/serviceworkers/ServiceWorkerGlobalScopeClientImpl.h"

#include <memory>
#include <utility>
#include "modules/fetch/Response.h"
#include "public/platform/WebURL.h"
#include "public/platform/modules/payments/WebPaymentAppResponse.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponse.h"
#include "public/web/modules/serviceworker/WebServiceWorkerContextClient.h"

namespace blink {

ServiceWorkerGlobalScopeClient* ServiceWorkerGlobalScopeClientImpl::Create(
    WebServiceWorkerContextClient& client) {
  return new ServiceWorkerGlobalScopeClientImpl(client);
}

ServiceWorkerGlobalScopeClientImpl::~ServiceWorkerGlobalScopeClientImpl() {}

void ServiceWorkerGlobalScopeClientImpl::GetClient(
    const WebString& id,
    std::unique_ptr<WebServiceWorkerClientCallbacks> callbacks) {
  client_.GetClient(id, std::move(callbacks));
}

void ServiceWorkerGlobalScopeClientImpl::GetClients(
    const WebServiceWorkerClientQueryOptions& options,
    std::unique_ptr<WebServiceWorkerClientsCallbacks> callbacks) {
  client_.GetClients(options, std::move(callbacks));
}

void ServiceWorkerGlobalScopeClientImpl::OpenWindowForClients(
    const WebURL& url,
    std::unique_ptr<WebServiceWorkerClientCallbacks> callbacks) {
  client_.OpenNewTab(url, std::move(callbacks));
}

void ServiceWorkerGlobalScopeClientImpl::OpenWindowForPaymentHandler(
    const WebURL& url,
    std::unique_ptr<WebServiceWorkerClientCallbacks> callbacks) {
  client_.OpenNewPopup(url, std::move(callbacks));
}

void ServiceWorkerGlobalScopeClientImpl::SetCachedMetadata(const WebURL& url,
                                                           const char* data,
                                                           size_t size) {
  client_.SetCachedMetadata(url, data, size);
}

void ServiceWorkerGlobalScopeClientImpl::ClearCachedMetadata(
    const WebURL& url) {
  client_.ClearCachedMetadata(url);
}

WebURL ServiceWorkerGlobalScopeClientImpl::Scope() const {
  return client_.Scope();
}

void ServiceWorkerGlobalScopeClientImpl::DidHandleActivateEvent(
    int event_id,
    WebServiceWorkerEventResult result,
    double event_dispatch_time) {
  client_.DidHandleActivateEvent(event_id, result, event_dispatch_time);
}

void ServiceWorkerGlobalScopeClientImpl::DidHandleBackgroundFetchAbortEvent(
    int event_id,
    WebServiceWorkerEventResult result,
    double event_dispatch_time) {
  client_.DidHandleBackgroundFetchAbortEvent(event_id, result,
                                             event_dispatch_time);
}

void ServiceWorkerGlobalScopeClientImpl::DidHandleBackgroundFetchClickEvent(
    int event_id,
    WebServiceWorkerEventResult result,
    double event_dispatch_time) {
  client_.DidHandleBackgroundFetchClickEvent(event_id, result,
                                             event_dispatch_time);
}

void ServiceWorkerGlobalScopeClientImpl::DidHandleBackgroundFetchFailEvent(
    int event_id,
    WebServiceWorkerEventResult result,
    double event_dispatch_time) {
  client_.DidHandleBackgroundFetchFailEvent(event_id, result,
                                            event_dispatch_time);
}

void ServiceWorkerGlobalScopeClientImpl::DidHandleBackgroundFetchedEvent(
    int event_id,
    WebServiceWorkerEventResult result,
    double event_dispatch_time) {
  client_.DidHandleBackgroundFetchedEvent(event_id, result,
                                          event_dispatch_time);
}

void ServiceWorkerGlobalScopeClientImpl::DidHandleExtendableMessageEvent(
    int event_id,
    WebServiceWorkerEventResult result,
    double event_dispatch_time) {
  client_.DidHandleExtendableMessageEvent(event_id, result,
                                          event_dispatch_time);
}

void ServiceWorkerGlobalScopeClientImpl::RespondToFetchEventWithNoResponse(
    int fetch_event_id,
    double event_dispatch_time) {
  client_.RespondToFetchEventWithNoResponse(fetch_event_id,
                                            event_dispatch_time);
}

void ServiceWorkerGlobalScopeClientImpl::RespondToFetchEvent(
    int fetch_event_id,
    const WebServiceWorkerResponse& response,
    double event_dispatch_time) {
  client_.RespondToFetchEvent(fetch_event_id, response, event_dispatch_time);
}

void ServiceWorkerGlobalScopeClientImpl::RespondToFetchEventWithResponseStream(
    int fetch_event_id,
    const WebServiceWorkerResponse& response,
    WebServiceWorkerStreamHandle* stream_handle,
    double event_dispatch_time) {
  client_.RespondToFetchEventWithResponseStream(
      fetch_event_id, response, stream_handle, event_dispatch_time);
}

void ServiceWorkerGlobalScopeClientImpl::RespondToPaymentRequestEvent(
    int event_id,
    const WebPaymentAppResponse& response,
    double event_dispatch_time) {
  client_.RespondToPaymentRequestEvent(event_id, response, event_dispatch_time);
}

void ServiceWorkerGlobalScopeClientImpl::DidHandleFetchEvent(
    int fetch_event_id,
    WebServiceWorkerEventResult result,
    double event_dispatch_time) {
  client_.DidHandleFetchEvent(fetch_event_id, result, event_dispatch_time);
}

void ServiceWorkerGlobalScopeClientImpl::DidHandleInstallEvent(
    int install_event_id,
    WebServiceWorkerEventResult result,
    double event_dispatch_time) {
  client_.DidHandleInstallEvent(install_event_id, result, event_dispatch_time);
}

void ServiceWorkerGlobalScopeClientImpl::DidHandleNotificationClickEvent(
    int event_id,
    WebServiceWorkerEventResult result,
    double event_dispatch_time) {
  client_.DidHandleNotificationClickEvent(event_id, result,
                                          event_dispatch_time);
}

void ServiceWorkerGlobalScopeClientImpl::DidHandleNotificationCloseEvent(
    int event_id,
    WebServiceWorkerEventResult result,
    double event_dispatch_time) {
  client_.DidHandleNotificationCloseEvent(event_id, result,
                                          event_dispatch_time);
}

void ServiceWorkerGlobalScopeClientImpl::DidHandlePushEvent(
    int push_event_id,
    WebServiceWorkerEventResult result,
    double event_dispatch_time) {
  client_.DidHandlePushEvent(push_event_id, result, event_dispatch_time);
}

void ServiceWorkerGlobalScopeClientImpl::DidHandleSyncEvent(
    int sync_event_id,
    WebServiceWorkerEventResult result,
    double event_dispatch_time) {
  client_.DidHandleSyncEvent(sync_event_id, result, event_dispatch_time);
}

void ServiceWorkerGlobalScopeClientImpl::DidHandlePaymentRequestEvent(
    int payment_request_event_id,
    WebServiceWorkerEventResult result,
    double event_dispatch_time) {
  client_.DidHandlePaymentRequestEvent(payment_request_event_id, result,
                                       event_dispatch_time);
}

void ServiceWorkerGlobalScopeClientImpl::PostMessageToClient(
    const WebString& client_uuid,
    const WebString& message,
    WebMessagePortChannelArray web_channels) {
  client_.PostMessageToClient(client_uuid, message, std::move(web_channels));
}

void ServiceWorkerGlobalScopeClientImpl::SkipWaiting(
    std::unique_ptr<WebServiceWorkerSkipWaitingCallbacks> callbacks) {
  client_.SkipWaiting(std::move(callbacks));
}

void ServiceWorkerGlobalScopeClientImpl::Claim(
    std::unique_ptr<WebServiceWorkerClientsClaimCallbacks> callbacks) {
  client_.Claim(std::move(callbacks));
}

void ServiceWorkerGlobalScopeClientImpl::Focus(
    const WebString& client_uuid,
    std::unique_ptr<WebServiceWorkerClientCallbacks> callback) {
  client_.Focus(client_uuid, std::move(callback));
}

void ServiceWorkerGlobalScopeClientImpl::Navigate(
    const WebString& client_uuid,
    const WebURL& url,
    std::unique_ptr<WebServiceWorkerClientCallbacks> callback) {
  client_.Navigate(client_uuid, url, std::move(callback));
}

void ServiceWorkerGlobalScopeClientImpl::RegisterForeignFetchScopes(
    int install_event_id,
    const WebVector<WebURL>& sub_scopes,
    const WebVector<WebSecurityOrigin>& origins) {
  client_.RegisterForeignFetchScopes(install_event_id, sub_scopes, origins);
}

ServiceWorkerGlobalScopeClientImpl::ServiceWorkerGlobalScopeClientImpl(
    WebServiceWorkerContextClient& client)
    : client_(client) {}

}  // namespace blink
