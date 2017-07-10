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

#include "public/platform/WebMessagePortChannel.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebWorkerFetchContext.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerClientsClaimCallbacks.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerClientsInfo.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerEventResult.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerSkipWaitingCallbacks.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerStreamHandle.h"
#include "public/web/WebDevToolsAgentClient.h"
#include "v8/include/v8.h"

namespace blink {

struct WebPaymentAppResponse;
struct WebServiceWorkerClientQueryOptions;
class WebServiceWorkerContextProxy;
class WebServiceWorkerNetworkProvider;
class WebServiceWorkerProvider;
class WebServiceWorkerResponse;
class WebString;

// WebServiceWorkerContextClient is a "client" of a service worker execution
// context. This interface is implemented by the embedder and allows the
// embedder to communicate with the service worker execution context.  It is
// supposed to be created on the main thread and then passed on to the worker
// thread by a newly created WorkerGlobalScope. Unless otherwise noted, all
// methods of this class are called on the worker thread.
class WebServiceWorkerContextClient {
 public:
  virtual ~WebServiceWorkerContextClient() {}

  // ServiceWorker specific method. Called when script accesses the
  // the |scope| attribute of the ServiceWorkerGlobalScope. Immutable per spec.
  virtual WebURL Scope() const { return WebURL(); }

  // ServiceWorker has prepared everything for script loading and is now ready
  // for inspection.
  virtual void WorkerReadyForInspection() {}

  // The worker script is successfully loaded and a new thread is about to
  // be started. Called on the main thread.
  virtual void WorkerScriptLoaded() {}

  virtual bool HasAssociatedRegistration() { return false; }

  // A new WorkerGlobalScope is created and started to run on the
  // worker thread.
  // This also gives back a proxy to the client to talk to the
  // newly created WorkerGlobalScope. The proxy is held by WorkerGlobalScope
  // and should not be held by the caller. No proxy methods should be called
  // after willDestroyWorkerContext() is called.
  virtual void WorkerContextStarted(WebServiceWorkerContextProxy*) {}

  // WorkerGlobalScope is about to be destroyed. The client should clear
  // the WebServiceWorkerGlobalScopeProxy when this is called.
  virtual void WillDestroyWorkerContext(v8::Local<v8::Context> context) {}

  // WorkerGlobalScope is destroyed and the worker is ready to be terminated.
  virtual void WorkerContextDestroyed() {}

  // Starting worker context is failed. This could happen when loading
  // worker script fails, or is asked to terminated before the context starts.
  // This is called on the main thread.
  virtual void WorkerContextFailedToStart() {}

  // Called when the worker script is evaluated. |success| is true if the
  // evaluation completed with no uncaught exception.
  virtual void DidEvaluateWorkerScript(bool success) {}

  // Called when the worker context is initialized.
  virtual void DidInitializeWorkerContext(v8::Local<v8::Context> context) {}

  // Called when some API to be recorded in UseCounter is called on the worker
  // global scope.
  virtual void CountFeature(uint32_t feature) {}

  // Called when the WorkerGlobalScope had an error or an exception.
  virtual void ReportException(const WebString& error_message,
                               int line_number,
                               int column_number,
                               const WebString& source_url) {}

  // Called when the console message is reported.
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

  // ServiceWorker specific method.
  virtual void DidHandleActivateEvent(int event_id,
                                      WebServiceWorkerEventResult result,
                                      double event_dispatch_time) {}

  // ServiceWorker specific method. Called after Background Fetch events
  // (dispatched via WebServiceWorkerContextProxy) is handled by the
  // ServiceWorker's script context.
  virtual void DidHandleBackgroundFetchAbortEvent(
      int event_id,
      WebServiceWorkerEventResult result,
      double event_dispatch_time) {}
  virtual void DidHandleBackgroundFetchClickEvent(
      int event_id,
      WebServiceWorkerEventResult result,
      double event_dispatch_time) {}
  virtual void DidHandleBackgroundFetchFailEvent(
      int event_id,
      WebServiceWorkerEventResult result,
      double event_dispatch_time) {}
  virtual void DidHandleBackgroundFetchedEvent(
      int event_id,
      WebServiceWorkerEventResult result,
      double event_dispatch_time) {}

  // Called after ExtendableMessageEvent is handled by the ServiceWorker's
  // script context.
  virtual void DidHandleExtendableMessageEvent(
      int event_id,
      WebServiceWorkerEventResult result,
      double event_dispatch_time) {}

  // ServiceWorker specific methods. RespondToFetchEvent* will be called after
  // FetchEvent returns a response by the ServiceWorker's script context, and
  // DidHandleFetchEvent will be called after the end of FetchEvent's
  // lifecycle. When no response is provided, the browser should fallback to
  // native fetch. |fetch_event_id|s are the same with the ids passed from
  // DispatchFetchEvent respectively.

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
  virtual void RespondToCanMakePaymentEvent(int event_id,
                                            bool can_make_payment,
                                            double event_dispatch_time) {}
  virtual void RespondToPaymentRequestEvent(
      int event_id,
      const WebPaymentAppResponse& response,
      double event_dispatch_time) {}
  virtual void DidHandleFetchEvent(int fetch_event_id,
                                   WebServiceWorkerEventResult result,
                                   double event_dispatch_time) {}

  // ServiceWorker specific method. Called after InstallEvent (dispatched
  // via WebServiceWorkerContextProxy) is handled by the ServiceWorker's
  // script context.
  virtual void DidHandleInstallEvent(int install_event_id,
                                     WebServiceWorkerEventResult result,
                                     double event_dispatch_time) {}

  // ServiceWorker specific method. Called after NotificationClickEvent
  // (dispatched via WebServiceWorkerContextProxy) is handled by the
  // ServiceWorker's script context.
  virtual void DidHandleNotificationClickEvent(
      int event_id,
      WebServiceWorkerEventResult result,
      double event_dispatch_time) {}

  // ServiceWorker specific method. Called after NotificationCloseEvent
  // (dispatched via WebServiceWorkerContextProxy) is handled by the
  // ServiceWorker's script context.
  virtual void DidHandleNotificationCloseEvent(
      int event_id,
      WebServiceWorkerEventResult result,
      double event_dispatch_time) {}

  // ServiceWorker specific method. Called after PushEvent (dispatched via
  // WebServiceWorkerContextProxy) is handled by the ServiceWorker's script
  // context.
  virtual void DidHandlePushEvent(int push_event_id,
                                  WebServiceWorkerEventResult result,
                                  double event_dispatch_time) {}

  // ServiceWorker specific method. Called after SyncEvent (dispatched via
  // WebServiceWorkerContextProxy) is handled by the ServiceWorker's script
  // context.
  virtual void DidHandleSyncEvent(int sync_event_id,
                                  WebServiceWorkerEventResult result,
                                  double event_dispatch_time) {}

  // ServiceWorker specific method. Called after PaymentRequestEvent (dispatched
  // via WebServiceWorkerContextProxy) is handled by the ServiceWorker's script
  // context.
  virtual void DidHandlePaymentRequestEvent(int payment_request_event_id,
                                            WebServiceWorkerEventResult result,
                                            double event_dispatch_time) {}

  // This is called on the main thread.
  virtual std::unique_ptr<WebServiceWorkerNetworkProvider>
  CreateServiceWorkerNetworkProvider() = 0;

  // Creates a WebWorkerFetchContext for a service worker. This is called on the
  // main thread. This is used only when off-main-thread-fetch is enabled.
  virtual std::unique_ptr<blink::WebWorkerFetchContext>
  CreateServiceWorkerFetchContext() {
    return nullptr;
  }

  // This is called on the main thread.
  virtual std::unique_ptr<WebServiceWorkerProvider>
  CreateServiceWorkerProvider() = 0;

  // Ownership of the passed callbacks is transferred to the callee, callee
  // should delete the callbacks after calling either onSuccess or onError.
  // WebServiceWorkerClientInfo and WebServiceWorkerError ownerships are
  // passed to the WebServiceWorkerClientCallbacks implementation.
  virtual void GetClient(const WebString&,
                         std::unique_ptr<WebServiceWorkerClientCallbacks>) = 0;

  // Ownership of the passed callbacks is transferred to the callee, callee
  // should delete the callbacks after calling either onSuccess or onError.
  // WebServiceWorkerClientsInfo and WebServiceWorkerError ownerships are
  // passed to the WebServiceWorkerClientsCallbacks implementation.
  virtual void GetClients(
      const WebServiceWorkerClientQueryOptions&,
      std::unique_ptr<WebServiceWorkerClientsCallbacks>) = 0;

  // Ownership of the passed callbacks is transferred to the callee, callee
  // should delete the callbacks after calling either onSuccess or onError.
  // WebServiceWorkerClientInfo and WebServiceWorkerError ownerships are
  // passed to the WebServiceWorkerClientsCallbacks implementation.
  virtual void OpenNewTab(const WebURL&,
                          std::unique_ptr<WebServiceWorkerClientCallbacks>) = 0;

  // Similar to OpenNewTab above.
  virtual void OpenNewPopup(
      const WebURL&,
      std::unique_ptr<WebServiceWorkerClientCallbacks>) = 0;

  // A suggestion to cache this metadata in association with this URL.
  virtual void SetCachedMetadata(const WebURL& url,
                                 const char* data,
                                 size_t size) {}

  // A suggestion to clear the cached metadata in association with this URL.
  virtual void ClearCachedMetadata(const WebURL& url) {}

  // Callee receives ownership of the passed vector.
  // FIXME: Blob refs should be passed to maintain ref counts. crbug.com/351753
  virtual void PostMessageToClient(const WebString& uuid,
                                   const WebString&,
                                   WebMessagePortChannelArray) = 0;

  // Ownership of the passed callbacks is transferred to the callee, callee
  // should delete the callbacks after run.
  virtual void SkipWaiting(
      std::unique_ptr<WebServiceWorkerSkipWaitingCallbacks>) = 0;

  // Ownership of the passed callbacks is transferred to the callee, callee
  // should delete the callbacks after run.
  virtual void Claim(
      std::unique_ptr<WebServiceWorkerClientsClaimCallbacks>) = 0;

  // Ownership of the passed callbacks is transferred to the callee, callee
  // should delete the callback after calling either onSuccess or onError.
  virtual void Focus(const WebString& uuid,
                     std::unique_ptr<WebServiceWorkerClientCallbacks>) = 0;

  // Ownership of the passed callbacks is transferred to the callee, callee
  // should delete the callbacks after calling either onSuccess or onError.
  // WebServiceWorkerClientInfo and WebServiceWorkerError ownerships are
  // passed to the WebServiceWorkerClientsCallbacks implementation.
  virtual void Navigate(const WebString& uuid,
                        const WebURL&,
                        std::unique_ptr<WebServiceWorkerClientCallbacks>) = 0;

  // Called when the worker wants to register subscopes to handle via foreign
  // fetch. Will only be called while an install event is in progress.
  virtual void RegisterForeignFetchScopes(
      int install_event_id,
      const WebVector<WebURL>& sub_scopes,
      const WebVector<WebSecurityOrigin>& origins) = 0;
};

}  // namespace blink

#endif  // WebServiceWorkerContextClient_h
