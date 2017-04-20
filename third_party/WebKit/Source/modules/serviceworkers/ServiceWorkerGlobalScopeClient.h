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

#ifndef ServiceWorkerGlobalScopeClient_h
#define ServiceWorkerGlobalScopeClient_h

#include <memory>
#include "core/dom/MessagePort.h"
#include "core/workers/WorkerClients.h"
#include "modules/ModulesExport.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Noncopyable.h"
#include "public/platform/WebMessagePortChannel.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerClientsClaimCallbacks.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerClientsInfo.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerEventResult.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerSkipWaitingCallbacks.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerStreamHandle.h"

namespace blink {

struct WebPaymentAppResponse;
struct WebServiceWorkerClientQueryOptions;
class ExecutionContext;
class WebServiceWorkerResponse;
class WebURL;
class WorkerClients;

// See WebServiceWorkerContextClient for documentation for the methods in this
// class.
class MODULES_EXPORT ServiceWorkerGlobalScopeClient
    : public Supplement<WorkerClients> {
  WTF_MAKE_NONCOPYABLE(ServiceWorkerGlobalScopeClient);
  DISALLOW_NEW();

 public:
  virtual ~ServiceWorkerGlobalScopeClient() {}

  // Called from ServiceWorkerClients.
  virtual void GetClient(const WebString&,
                         std::unique_ptr<WebServiceWorkerClientCallbacks>) = 0;
  virtual void GetClients(
      const WebServiceWorkerClientQueryOptions&,
      std::unique_ptr<WebServiceWorkerClientsCallbacks>) = 0;
  virtual void OpenWindow(const WebURL&,
                          std::unique_ptr<WebServiceWorkerClientCallbacks>) = 0;
  virtual void SetCachedMetadata(const WebURL&, const char*, size_t) = 0;
  virtual void ClearCachedMetadata(const WebURL&) = 0;

  virtual WebURL Scope() const = 0;

  virtual void DidHandleActivateEvent(int event_id,
                                      WebServiceWorkerEventResult,
                                      double event_dispatch_time) = 0;
  virtual void DidHandleBackgroundFetchAbortEvent(
      int event_id,
      WebServiceWorkerEventResult,
      double event_dispatch_time) = 0;
  virtual void DidHandleBackgroundFetchClickEvent(
      int event_id,
      WebServiceWorkerEventResult,
      double event_dispatch_time) = 0;
  virtual void DidHandleBackgroundFetchFailEvent(
      int event_id,
      WebServiceWorkerEventResult,
      double event_dispatch_time) = 0;
  virtual void DidHandleBackgroundFetchedEvent(int event_id,
                                               WebServiceWorkerEventResult,
                                               double event_dispatch_time) = 0;
  virtual void DidHandleExtendableMessageEvent(int event_id,
                                               WebServiceWorkerEventResult,
                                               double event_dispatch_time) = 0;
  virtual void RespondToFetchEventWithNoResponse(
      int fetch_event_id,
      double event_dispatch_time) = 0;
  virtual void RespondToFetchEvent(int fetch_event_id,
                                   const WebServiceWorkerResponse&,
                                   double event_dispatch_time) = 0;
  virtual void RespondToFetchEventWithResponseStream(
      int fetch_event_id,
      const WebServiceWorkerResponse&,
      WebServiceWorkerStreamHandle*,
      double event_dispatch_time) = 0;
  virtual void RespondToPaymentRequestEvent(int event_id,
                                            const WebPaymentAppResponse&,
                                            double event_dispatch_time) = 0;
  virtual void DidHandleFetchEvent(int fetch_event_id,
                                   WebServiceWorkerEventResult,
                                   double event_dispatch_time) = 0;
  virtual void DidHandleInstallEvent(int install_event_id,
                                     WebServiceWorkerEventResult,
                                     double event_dispatch_time) = 0;
  virtual void DidHandleNotificationClickEvent(int event_id,
                                               WebServiceWorkerEventResult,
                                               double event_dispatch_time) = 0;
  virtual void DidHandleNotificationCloseEvent(int event_id,
                                               WebServiceWorkerEventResult,
                                               double event_dispatch_time) = 0;
  virtual void DidHandlePushEvent(int push_event_id,
                                  WebServiceWorkerEventResult,
                                  double event_dispatch_time) = 0;
  virtual void DidHandleSyncEvent(int sync_event_id,
                                  WebServiceWorkerEventResult,
                                  double event_dispatch_time) = 0;
  virtual void DidHandlePaymentRequestEvent(int payment_request_event_id,
                                            WebServiceWorkerEventResult,
                                            double event_dispatch_time) = 0;
  virtual void PostMessageToClient(const WebString& client_uuid,
                                   const WebString& message,
                                   WebMessagePortChannelArray) = 0;
  virtual void SkipWaiting(
      std::unique_ptr<WebServiceWorkerSkipWaitingCallbacks>) = 0;
  virtual void Claim(
      std::unique_ptr<WebServiceWorkerClientsClaimCallbacks>) = 0;
  virtual void Focus(const WebString& client_uuid,
                     std::unique_ptr<WebServiceWorkerClientCallbacks>) = 0;
  virtual void Navigate(const WebString& client_uuid,
                        const WebURL&,
                        std::unique_ptr<WebServiceWorkerClientCallbacks>) = 0;
  virtual void RegisterForeignFetchScopes(
      const WebVector<WebURL>& sub_scopes,
      const WebVector<WebSecurityOrigin>&) = 0;

  static const char* SupplementName();
  static ServiceWorkerGlobalScopeClient* From(ExecutionContext*);

 protected:
  ServiceWorkerGlobalScopeClient() {}
};

MODULES_EXPORT void ProvideServiceWorkerGlobalScopeClientToWorker(
    WorkerClients*,
    ServiceWorkerGlobalScopeClient*);

}  // namespace blink

#endif  // ServiceWorkerGlobalScopeClient_h
