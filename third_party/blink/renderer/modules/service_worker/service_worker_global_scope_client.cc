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

#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope_client.h"

#include <memory>
#include <utility>
#include "third_party/blink/public/platform/modules/payments/web_payment_handler_response.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_error.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_response.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_client.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// TODO(leonhsl): Remove this function, pass ServiceWorkerClientInfoPtr through
// to replace WebServiceWorkerClientInfo.
blink::WebServiceWorkerClientInfo ToWebServiceWorkerClientInfo(
    mojom::blink::ServiceWorkerClientInfoPtr client_info) {
  DCHECK(!client_info->client_uuid.IsEmpty());

  blink::WebServiceWorkerClientInfo web_client_info;

  web_client_info.uuid = client_info->client_uuid;
  web_client_info.page_visibility_state = client_info->page_visibility_state;
  web_client_info.is_focused = client_info->is_focused;
  web_client_info.url = client_info->url;
  web_client_info.frame_type = client_info->frame_type;
  web_client_info.client_type = client_info->client_type;

  return web_client_info;
}

void DidGetClient(
    std::unique_ptr<blink::WebServiceWorkerClientCallbacks> callbacks,
    mojom::blink::ServiceWorkerClientInfoPtr client) {
  std::unique_ptr<blink::WebServiceWorkerClientInfo> web_client;
  if (client) {
    web_client = std::make_unique<blink::WebServiceWorkerClientInfo>(
        ToWebServiceWorkerClientInfo(std::move(client)));
  }
  callbacks->OnSuccess(std::move(web_client));
}

void DidGetClients(
    std::unique_ptr<blink::WebServiceWorkerClientsCallbacks> callbacks,
    Vector<mojom::blink::ServiceWorkerClientInfoPtr> clients) {
  blink::WebServiceWorkerClientsInfo info;
  blink::WebVector<blink::WebServiceWorkerClientInfo> web_clients(
      clients.size());
  for (size_t i = 0; i < clients.size(); ++i)
    web_clients[i] = ToWebServiceWorkerClientInfo(std::move(clients[i]));
  info.clients.Swap(web_clients);
  callbacks->OnSuccess(info);
}

void DidOpenWindow(
    std::unique_ptr<blink::WebServiceWorkerClientCallbacks> callbacks,
    bool success,
    mojom::blink::ServiceWorkerClientInfoPtr client,
    const String& error_msg) {
  if (!success) {
    DCHECK(!client);
    DCHECK(!error_msg.IsNull());
    callbacks->OnError(blink::WebServiceWorkerError(
        mojom::blink::ServiceWorkerErrorType::kNavigation, error_msg));
    return;
  }

  std::unique_ptr<blink::WebServiceWorkerClientInfo> web_client;
  if (client) {
    web_client = std::make_unique<blink::WebServiceWorkerClientInfo>(
        ToWebServiceWorkerClientInfo(std::move(client)));
  }
  callbacks->OnSuccess(std::move(web_client));
}

void DidFocusClient(
    std::unique_ptr<blink::WebServiceWorkerClientCallbacks> callbacks,
    mojom::blink::ServiceWorkerClientInfoPtr client) {
  if (!client) {
    callbacks->OnError(blink::WebServiceWorkerError(
        mojom::blink::ServiceWorkerErrorType::kNotFound,
        "The client was not found."));
    return;
  }
  auto web_client = std::make_unique<blink::WebServiceWorkerClientInfo>(
      ToWebServiceWorkerClientInfo(std::move(client)));
  callbacks->OnSuccess(std::move(web_client));
}

void DidNavigateClient(
    std::unique_ptr<blink::WebServiceWorkerClientCallbacks> callbacks,
    bool success,
    mojom::blink::ServiceWorkerClientInfoPtr client,
    const String& error_msg) {
  if (!success) {
    DCHECK(!client);
    DCHECK(!error_msg.IsNull());
    callbacks->OnError(blink::WebServiceWorkerError(
        mojom::blink::ServiceWorkerErrorType::kNavigation, error_msg));
    return;
  }

  std::unique_ptr<blink::WebServiceWorkerClientInfo> web_client;
  if (client) {
    web_client = std::make_unique<blink::WebServiceWorkerClientInfo>(
        ToWebServiceWorkerClientInfo(std::move(client)));
  }
  callbacks->OnSuccess(std::move(web_client));
}

}  // namespace

ServiceWorkerGlobalScopeClient::ServiceWorkerGlobalScopeClient(
    WebServiceWorkerContextClient& client)
    : client_(client) {}

void ServiceWorkerGlobalScopeClient::GetClient(
    const String& id,
    std::unique_ptr<WebServiceWorkerClientCallbacks> callbacks) {
  DCHECK(callbacks);
  service_worker_host_->GetClient(
      id, WTF::Bind(&DidGetClient, std::move(callbacks)));
}

void ServiceWorkerGlobalScopeClient::GetClients(
    mojom::blink::ServiceWorkerClientQueryOptionsPtr options,
    std::unique_ptr<WebServiceWorkerClientsCallbacks> callbacks) {
  DCHECK(callbacks);
  service_worker_host_->GetClients(
      std::move(options), WTF::Bind(&DidGetClients, std::move(callbacks)));
}

void ServiceWorkerGlobalScopeClient::OpenWindowForClients(
    const KURL& url,
    std::unique_ptr<WebServiceWorkerClientCallbacks> callbacks) {
  DCHECK(callbacks);
  service_worker_host_->OpenNewTab(
      url, WTF::Bind(&DidOpenWindow, std::move(callbacks)));
}

void ServiceWorkerGlobalScopeClient::OpenWindowForPaymentHandler(
    const KURL& url,
    std::unique_ptr<WebServiceWorkerClientCallbacks> callbacks) {
  DCHECK(callbacks);
  service_worker_host_->OpenPaymentHandlerWindow(
      url, WTF::Bind(&DidOpenWindow, std::move(callbacks)));
}

void ServiceWorkerGlobalScopeClient::SetCachedMetadata(const KURL& url,
                                                       const char* data,
                                                       size_t size) {
  Vector<uint8_t> meta_data;
  meta_data.Append(data, size);
  service_worker_host_->SetCachedMetadata(url, meta_data);
}

void ServiceWorkerGlobalScopeClient::ClearCachedMetadata(const KURL& url) {
  service_worker_host_->ClearCachedMetadata(url);
}

void ServiceWorkerGlobalScopeClient::PostMessageToClient(
    const String& client_uuid,
    BlinkTransferableMessage message) {
  service_worker_host_->PostMessageToClient(client_uuid, std::move(message));
}

void ServiceWorkerGlobalScopeClient::SkipWaiting(SkipWaitingCallback callback) {
  service_worker_host_->SkipWaiting(std::move(callback));
}

void ServiceWorkerGlobalScopeClient::Claim(ClaimCallback callback) {
  service_worker_host_->ClaimClients(std::move(callback));
}

void ServiceWorkerGlobalScopeClient::Focus(
    const String& client_uuid,
    std::unique_ptr<WebServiceWorkerClientCallbacks> callbacks) {
  DCHECK(callbacks);
  service_worker_host_->FocusClient(
      client_uuid, WTF::Bind(&DidFocusClient, std::move(callbacks)));
}

void ServiceWorkerGlobalScopeClient::Navigate(
    const String& client_uuid,
    const KURL& url,
    std::unique_ptr<WebServiceWorkerClientCallbacks> callbacks) {
  DCHECK(callbacks);
  service_worker_host_->NavigateClient(
      client_uuid, url, WTF::Bind(&DidNavigateClient, std::move(callbacks)));
}

void ServiceWorkerGlobalScopeClient::DidHandleActivateEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  client_.DidHandleActivateEvent(event_id, status, event_dispatch_time);
}

void ServiceWorkerGlobalScopeClient::DidHandleBackgroundFetchAbortEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  client_.DidHandleBackgroundFetchAbortEvent(event_id, status,
                                             event_dispatch_time);
}

void ServiceWorkerGlobalScopeClient::DidHandleBackgroundFetchClickEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  client_.DidHandleBackgroundFetchClickEvent(event_id, status,
                                             event_dispatch_time);
}

void ServiceWorkerGlobalScopeClient::DidHandleBackgroundFetchFailEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  client_.DidHandleBackgroundFetchFailEvent(event_id, status,
                                            event_dispatch_time);
}

void ServiceWorkerGlobalScopeClient::DidHandleBackgroundFetchSuccessEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  client_.DidHandleBackgroundFetchSuccessEvent(event_id, status,
                                               event_dispatch_time);
}

void ServiceWorkerGlobalScopeClient::DidHandleCookieChangeEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  client_.DidHandleCookieChangeEvent(event_id, status, event_dispatch_time);
}

void ServiceWorkerGlobalScopeClient::DidHandleExtendableMessageEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  client_.DidHandleExtendableMessageEvent(event_id, status,
                                          event_dispatch_time);
}

void ServiceWorkerGlobalScopeClient::RespondToFetchEventWithNoResponse(
    int fetch_event_id,
    base::TimeTicks event_dispatch_time) {
  client_.RespondToFetchEventWithNoResponse(fetch_event_id,
                                            event_dispatch_time);
}

void ServiceWorkerGlobalScopeClient::RespondToFetchEvent(
    int fetch_event_id,
    const WebServiceWorkerResponse& response,
    base::TimeTicks event_dispatch_time) {
  client_.RespondToFetchEvent(fetch_event_id, response, event_dispatch_time);
}

void ServiceWorkerGlobalScopeClient::RespondToFetchEventWithResponseStream(
    int fetch_event_id,
    const WebServiceWorkerResponse& response,
    WebServiceWorkerStreamHandle* stream_handle,
    base::TimeTicks event_dispatch_time) {
  client_.RespondToFetchEventWithResponseStream(
      fetch_event_id, response, stream_handle, event_dispatch_time);
}

void ServiceWorkerGlobalScopeClient::RespondToAbortPaymentEvent(
    int event_id,
    bool abort_payment,
    base::TimeTicks event_dispatch_time) {
  client_.RespondToAbortPaymentEvent(event_id, abort_payment,
                                     event_dispatch_time);
}

void ServiceWorkerGlobalScopeClient::RespondToCanMakePaymentEvent(
    int event_id,
    bool response,
    base::TimeTicks event_dispatch_time) {
  client_.RespondToCanMakePaymentEvent(event_id, response, event_dispatch_time);
}

void ServiceWorkerGlobalScopeClient::RespondToPaymentRequestEvent(
    int event_id,
    const WebPaymentHandlerResponse& response,
    base::TimeTicks event_dispatch_time) {
  client_.RespondToPaymentRequestEvent(event_id, response, event_dispatch_time);
}

void ServiceWorkerGlobalScopeClient::DidHandleFetchEvent(
    int fetch_event_id,
    mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  client_.DidHandleFetchEvent(fetch_event_id, status, event_dispatch_time);
}

void ServiceWorkerGlobalScopeClient::DidHandleInstallEvent(
    int install_event_id,
    mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  client_.DidHandleInstallEvent(install_event_id, status, event_dispatch_time);
}

void ServiceWorkerGlobalScopeClient::DidHandleNotificationClickEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  client_.DidHandleNotificationClickEvent(event_id, status,
                                          event_dispatch_time);
}

void ServiceWorkerGlobalScopeClient::DidHandleNotificationCloseEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  client_.DidHandleNotificationCloseEvent(event_id, status,
                                          event_dispatch_time);
}

void ServiceWorkerGlobalScopeClient::DidHandlePushEvent(
    int push_event_id,
    mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  client_.DidHandlePushEvent(push_event_id, status, event_dispatch_time);
}

void ServiceWorkerGlobalScopeClient::DidHandleSyncEvent(
    int sync_event_id,
    mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  client_.DidHandleSyncEvent(sync_event_id, status, event_dispatch_time);
}

void ServiceWorkerGlobalScopeClient::DidHandleAbortPaymentEvent(
    int abort_payment_event_id,
    mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  client_.DidHandleAbortPaymentEvent(abort_payment_event_id, status,
                                     event_dispatch_time);
}

void ServiceWorkerGlobalScopeClient::DidHandleCanMakePaymentEvent(
    int payment_request_event_id,
    mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  client_.DidHandleCanMakePaymentEvent(payment_request_event_id, status,
                                       event_dispatch_time);
}

void ServiceWorkerGlobalScopeClient::DidHandlePaymentRequestEvent(
    int payment_request_event_id,
    mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  client_.DidHandlePaymentRequestEvent(payment_request_event_id, status,
                                       event_dispatch_time);
}

void ServiceWorkerGlobalScopeClient::BindServiceWorkerHost(
    mojom::blink::ServiceWorkerHostAssociatedPtrInfo service_worker_host) {
  DCHECK(service_worker_host.is_valid());
  DCHECK(!service_worker_host_);
  service_worker_host_.Bind(std::move(service_worker_host));
}

void ServiceWorkerGlobalScopeClient::WillDestroyWorkerContext() {
  service_worker_host_.reset();
}

const char ServiceWorkerGlobalScopeClient::kSupplementName[] =
    "ServiceWorkerGlobalScopeClient";

ServiceWorkerGlobalScopeClient* ServiceWorkerGlobalScopeClient::From(
    ExecutionContext* context) {
  // TODO(horo): Replace CHECK() to DCHECK() when crbug.com/749930 is fixed.
  CHECK(context);
  WorkerClients* worker_clients = ToWorkerGlobalScope(context)->Clients();
  CHECK(worker_clients);
  ServiceWorkerGlobalScopeClient* client =
      Supplement<WorkerClients>::From<ServiceWorkerGlobalScopeClient>(
          worker_clients);
  CHECK(client);
  return client;
}

void ServiceWorkerGlobalScopeClient::Trace(blink::Visitor* visitor) {
  Supplement<WorkerClients>::Trace(visitor);
}

void ProvideServiceWorkerGlobalScopeClientToWorker(
    WorkerClients* clients,
    ServiceWorkerGlobalScopeClient* client) {
  clients->ProvideSupplement(client);
}

}  // namespace blink
