/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef WebServiceWorkerContextClient_h
#define WebServiceWorkerContextClient_h

#include <memory>

#include "public/platform/WebURL.h"
#include "public/platform/WebWorkerFetchContext.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerClientsClaimCallbacks.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerClientsInfo.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerSkipWaitingCallbacks.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerStreamHandle.h"
#include "public/platform/modules/serviceworker/service_worker_event_status.mojom-shared.h"
#include "public/web/WebDevToolsAgentClient.h"
#include "v8/include/v8.h"

namespace blink {

class MessagePortChannel;
struct WebPaymentHandlerResponse;
struct WebServiceWorkerClientQueryOptions;
class WebServiceWorkerContextProxy;
class WebServiceWorkerNetworkProvider;
class WebServiceWorkerProvider;
class WebServiceWorkerResponse;
class WebString;

// WebServiceWorkerContextClient is a "client" of a service worker execution
// context. This interface is implemented by the embedder and allows the
// embedder to communicate with the service worker execution context.  It is
// created on the main thread and then passed on to the worker thread by a newly
// created ServiceWorkerGlobalScope.
//
// Unless otherwise noted, all methods of this class are called on the worker
// thread.
class WebServiceWorkerContextClient {
 public:
  virtual ~WebServiceWorkerContextClient() {}

  // Returns the scope of the service worker's registration. Currently
  // only used by InstallEvent#registerForeignFetch().
  // TODO(falken): Figure out why registerForeignFetch() can't just use
  // self.registration.scope.
  virtual WebURL Scope() const { return WebURL(); }

  // For Clients#get(id). Requests the embedder to return the specified Client.
  virtual void GetClient(const WebString& client_id,
                         std::unique_ptr<WebServiceWorkerClientCallbacks>) = 0;

  // For Clients#matchAll(options). Requests the embedder to return all matching
  // Clients.
  virtual void GetClients(
      const WebServiceWorkerClientQueryOptions&,
      std::unique_ptr<WebServiceWorkerClientsCallbacks>) = 0;

  // For Clients#openWindow(url). Requests the embedder to open a tab.
  virtual void OpenNewTab(const WebURL&,
                          std::unique_ptr<WebServiceWorkerClientCallbacks>) = 0;

  // Similar to OpenNewTab above. For PaymentRequestEvent#openWindow().
  virtual void OpenNewPopup(
      const WebURL&,
      std::unique_ptr<WebServiceWorkerClientCallbacks>) = 0;

  // A suggestion to cache this metadata in association with this URL.
  virtual void SetCachedMetadata(const WebURL& url,
                                 const char* data,
                                 size_t size) {}

  // A suggestion to clear the cached metadata in association with this URL.
  virtual void ClearCachedMetadata(const WebURL& url) {}

  // ServiceWorker has prepared everything for script loading and is now ready
  // for DevTools inspection.
  virtual void WorkerReadyForInspection() {}

  // Starting the worker failed. This could happen when loading the worker
  // script failed, or the worker was asked to terminate before startup
  // completed. Called on the main thread.
  virtual void WorkerContextFailedToStart() {}

  // The worker script successfully loaded. Called on the main thread when the
  // script is served from ResourceLoader or on the worker thread when the
  // script is served via WebServiceWorkerInstalledScriptsManager.
  virtual void WorkerScriptLoaded() {}

  // Called when a WorkerGlobalScope was created for the worker thread. This
  // also gives a proxy to the embedder to talk to the newly created
  // WorkerGlobalScope. The proxy is owned by WorkerGlobalScope and should not
  // be destroyed by the caller. No proxy methods should be called after
  // willDestroyWorkerContext() is called.
  virtual void WorkerContextStarted(WebServiceWorkerContextProxy*) {}

  // Called when initial script evaluation finished. |success| is true if the
  // evaluation completed with no uncaught exception.
  virtual void DidEvaluateWorkerScript(bool success) {}

  // Called when the worker context is initialized. This is probably called
  // after WorkerContextStarted(). (WorkerThread::InitializeOnWorkerThread()
  // calls WorkerContextStarted() via
  // WorkerReportingProxy::DidCreateWorkerGlobalScope(),
  // and then initializes the worker context if "needed" and calls
  // DidInitializeWorkerContext(), but it's not clear when the context would
  // already be initialized.)
  // TODO(nhiroki): Can you clarify this code and comment?
  virtual void DidInitializeWorkerContext(v8::Local<v8::Context> context) {}

  // WorkerGlobalScope is about to be destroyed. The client should clear
  // the WebServiceWorkerGlobalScopeProxy when this is called.
  virtual void WillDestroyWorkerContext(v8::Local<v8::Context> context) {}

  // WorkerGlobalScope was destroyed and the worker is ready to be terminated.
  virtual void WorkerContextDestroyed() {}

  // Called when some API to be recorded in UseCounter is called on the worker
  // global scope.
  virtual void CountFeature(uint32_t feature) {}

  // Called when the WorkerGlobalScope had an error or an exception.
  virtual void ReportException(const WebString& error_message,
                               int line_number,
                               int column_number,
                               const WebString& source_url) {}

  // Called when a console message was written.
  virtual void ReportConsoleMessage(int source,
                                    int level,
                                    const WebString& message,
                                    int line_number,
                                    const WebString& source_url) {}

  // Inspector related messages.
  virtual void SendDevToolsMessage(int session_id,
                                   int call_id,
                                   const WebString& message,
                                   const WebString& state) {}

  // Message loop for debugging.
  virtual WebDevToolsAgentClient::WebKitClientMessageLoop*
  CreateDevToolsMessageLoop() {
    return nullptr;
  }

  // Called after an 'activate' event completed.
  virtual void DidHandleActivateEvent(int event_id,
                                      mojom::ServiceWorkerEventStatus,
                                      double event_dispatch_time) {}

  // Called after Background Fetch events (dispatched via
  // WebServiceWorkerContextProxy) are handled by the service worker.
  virtual void DidHandleBackgroundFetchAbortEvent(
      int event_id,
      mojom::ServiceWorkerEventStatus,
      double event_dispatch_time) {}
  virtual void DidHandleBackgroundFetchClickEvent(
      int event_id,
      mojom::ServiceWorkerEventStatus,
      double event_dispatch_time) {}
  virtual void DidHandleBackgroundFetchFailEvent(
      int event_id,
      mojom::ServiceWorkerEventStatus,
      double event_dispatch_time) {}
  virtual void DidHandleBackgroundFetchedEvent(int event_id,
                                               mojom::ServiceWorkerEventStatus,
                                               double event_dispatch_time) {}

  // Called after ExtendableMessageEvent was handled by the service worker.
  virtual void DidHandleExtendableMessageEvent(int event_id,
                                               mojom::ServiceWorkerEventStatus,
                                               double event_dispatch_time) {}

  // RespondToFetchEvent* will be called after the service worker returns a
  // response to a FetchEvent, and DidHandleFetchEvent will be called after the
  // end of FetchEvent's lifecycle. |fetch_event_id| is the id that was passed
  // to DispatchFetchEvent.

  // Used when respondWith() is not called. Tells the browser to fall back to
  // native fetch.
  virtual void RespondToFetchEventWithNoResponse(int fetch_event_id,
                                                 double event_dispatch_time) {}
  // Responds to the fetch event with |response|.
  virtual void RespondToFetchEvent(int fetch_event_id,
                                   const WebServiceWorkerResponse& response,
                                   double event_dispatch_time) {}
  // Responds to the fetch event with |response|, where body is
  // |body_as_stream|.
  virtual void RespondToFetchEventWithResponseStream(
      int fetch_event_id,
      const WebServiceWorkerResponse& response,
      WebServiceWorkerStreamHandle* body_as_stream,
      double event_dispatch_time) {}
  virtual void DidHandleFetchEvent(int fetch_event_id,
                                   mojom::ServiceWorkerEventStatus,
                                   double event_dispatch_time) {}

  // Called after InstallEvent (dispatched via WebServiceWorkerContextProxy) is
  // handled by the service worker.
  virtual void DidHandleInstallEvent(int install_event_id,
                                     mojom::ServiceWorkerEventStatus,
                                     double event_dispatch_time) {}

  // Called after NotificationClickEvent (dispatched via
  // WebServiceWorkerContextProxy) is handled by the service worker.
  virtual void DidHandleNotificationClickEvent(int event_id,
                                               mojom::ServiceWorkerEventStatus,
                                               double event_dispatch_time) {}

  // Called after NotificationCloseEvent (dispatched via
  // WebServiceWorkerContextProxy) is handled by the service worker.
  virtual void DidHandleNotificationCloseEvent(int event_id,
                                               mojom::ServiceWorkerEventStatus,
                                               double event_dispatch_time) {}

  // Called after PushEvent (dispatched via WebServiceWorkerContextProxy) is
  // handled by the service worker.
  virtual void DidHandlePushEvent(int push_event_id,
                                  mojom::ServiceWorkerEventStatus,
                                  double event_dispatch_time) {}

  // Called after SyncEvent (dispatched via WebServiceWorkerContextProxy) is
  // handled by the service worker.
  virtual void DidHandleSyncEvent(int sync_event_id,
                                  mojom::ServiceWorkerEventStatus,
                                  double event_dispatch_time) {}

  // RespondToAbortPaymentEvent will be called after the service worker
  // returns a response to a AbortPaymentEvent, and DidHandleAbortPaymentEvent
  // will be called after the end of AbortPaymentEvent's lifecycle.
  // |event_id| is the id that was passed to DispatchAbortPaymentEvent.
  virtual void RespondToAbortPaymentEvent(int event_id,
                                          bool abort_payment,
                                          double event_dispatch_time) {}
  // Called after AbortPaymentEvent (dispatched
  // via WebServiceWorkerContextProxy) is handled by the service worker.
  virtual void DidHandleAbortPaymentEvent(int abort_payment_event_id,
                                          mojom::ServiceWorkerEventStatus,
                                          double event_dispatch_time) {}

  // RespondToCanMakePaymentEvent will be called after the service worker
  // returns a response to a CanMakePaymentEvent, and
  // DidHandleCanMakePaymentEvent will be called after the end of
  // CanMakePaymentEvent's lifecycle. |event_id| is the id that was passed
  // to DispatchCanMakePaymentEvent.
  virtual void RespondToCanMakePaymentEvent(int event_id,
                                            bool can_make_payment,
                                            double event_dispatch_time) {}
  // Called after CanMakePaymentEvent (dispatched
  // via WebServiceWorkerContextProxy) is handled by the service worker.
  virtual void DidHandleCanMakePaymentEvent(int payment_request_event_id,
                                            mojom::ServiceWorkerEventStatus,
                                            double event_dispatch_time) {}

  // RespondToPaymentRequestEvent will be called after the service worker
  // returns a response to a PaymentRequestEvent, and
  // DidHandlePaymentRequestEvent will be called after the end of
  // PaymentRequestEvent's lifecycle. |event_id| is the id that was passed
  // to DispatchPaymentRequestEvent.
  virtual void RespondToPaymentRequestEvent(
      int event_id,
      const WebPaymentHandlerResponse& response,
      double event_dispatch_time) {}
  // Called after PaymentRequestEvent (dispatched via
  // WebServiceWorkerContextProxy) is handled by the service worker.
  virtual void DidHandlePaymentRequestEvent(int payment_request_event_id,
                                            mojom::ServiceWorkerEventStatus,
                                            double event_dispatch_time) {}

  // Called on the main thread.
  virtual std::unique_ptr<WebServiceWorkerNetworkProvider>
  CreateServiceWorkerNetworkProvider() = 0;

  // Creates a WebWorkerFetchContext for a service worker. This is called on the
  // main thread. This is used only when off-main-thread-fetch is enabled.
  virtual std::unique_ptr<blink::WebWorkerFetchContext>
  CreateServiceWorkerFetchContext() {
    return nullptr;
  }

  // Called on the main thread.
  virtual std::unique_ptr<WebServiceWorkerProvider>
  CreateServiceWorkerProvider() = 0;

  // Callee receives ownership of the passed vector.
  // TODO(mek): Blob refs should be passed to maintain ref counts.
  // crbug.com/351753
  virtual void PostMessageToClient(const WebString& uuid,
                                   const WebString&,
                                   WebVector<MessagePortChannel>) = 0;

  // For WindowClient#focus(). Requests the embedder to focus a window.
  virtual void Focus(const WebString& uuid,
                     std::unique_ptr<WebServiceWorkerClientCallbacks>) = 0;

  // For WindowClient#navigate(). Requests the embedder to navigate to a URL.
  virtual void Navigate(const WebString& uuid,
                        const WebURL&,
                        std::unique_ptr<WebServiceWorkerClientCallbacks>) = 0;

  // For ServiceWorkerGlobalScope#skipWaiting().
  virtual void SkipWaiting(
      std::unique_ptr<WebServiceWorkerSkipWaitingCallbacks>) = 0;

  // For Clients#claim().
  virtual void Claim(
      std::unique_ptr<WebServiceWorkerClientsClaimCallbacks>) = 0;

  // Called when the worker wants to register subscopes to handle via foreign
  // fetch. Will only be called while an install event is in progress.
  virtual void RegisterForeignFetchScopes(
      int install_event_id,
      const WebVector<WebURL>& sub_scopes,
      const WebVector<WebSecurityOrigin>& origins) = 0;
};

}  // namespace blink

#endif  // WebServiceWorkerContextClient_h
