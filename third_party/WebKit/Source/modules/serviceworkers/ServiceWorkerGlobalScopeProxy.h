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

#ifndef ServiceWorkerGlobalScopeProxy_h
#define ServiceWorkerGlobalScopeProxy_h

#include <memory>
#include "core/workers/WorkerReportingProxy.h"
#include "platform/heap/Handle.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Forward.h"
#include "public/platform/WebString.h"
#include "public/web/modules/serviceworker/WebServiceWorkerContextProxy.h"

namespace blink {

class FetchEvent;
class ParentFrameTaskRunners;
class ServiceWorkerGlobalScope;
class WebDataConsumerHandle;
class WebEmbeddedWorkerImpl;
class WebServiceWorkerContextClient;
struct WebServiceWorkerError;
class WebServiceWorkerRequest;
class WebURLResponse;

// This class is created and destructed on the main thread, but live most
// of its time as a resident of the worker thread. All methods other than its
// ctor/dtor and Detach() are called on the worker thread.
//
// This implements WebServiceWorkerContextProxy, which connects ServiceWorker's
// WorkerGlobalScope and embedder/chrome, and implements ServiceWorker-specific
// events/upcall methods that are to be called by embedder/chromium,
// e.g. onfetch.
//
// An instance of this class is supposed to outlive until
// workerThreadTerminated() is called by its corresponding
// WorkerGlobalScope.
class ServiceWorkerGlobalScopeProxy final
    : public GarbageCollectedFinalized<ServiceWorkerGlobalScopeProxy>,
      public WebServiceWorkerContextProxy,
      public WorkerReportingProxy {
  WTF_MAKE_NONCOPYABLE(ServiceWorkerGlobalScopeProxy);

 public:
  static ServiceWorkerGlobalScopeProxy* Create(WebEmbeddedWorkerImpl&,
                                               WebServiceWorkerContextClient&);
  ~ServiceWorkerGlobalScopeProxy() override;

  // WebServiceWorkerContextProxy overrides:
  void SetRegistration(
      std::unique_ptr<WebServiceWorkerRegistration::Handle>) override;
  void DispatchActivateEvent(int) override;
  void DispatchBackgroundFetchAbortEvent(int, const WebString& tag) override;
  void DispatchBackgroundFetchClickEvent(int,
                                         const WebString& tag,
                                         BackgroundFetchState) override;
  void DispatchBackgroundFetchFailEvent(
      int,
      const WebString& tag,
      const WebVector<WebBackgroundFetchSettledFetch>& fetches) override;
  void DispatchBackgroundFetchedEvent(
      int,
      const WebString& tag,
      const WebVector<WebBackgroundFetchSettledFetch>& fetches) override;
  void DispatchExtendableMessageEvent(
      int event_id,
      const WebString& message,
      const WebSecurityOrigin& source_origin,
      WebVector<MessagePortChannel>,
      const WebServiceWorkerClientInfo&) override;
  void DispatchExtendableMessageEvent(
      int event_id,
      const WebString& message,
      const WebSecurityOrigin& source_origin,
      WebVector<MessagePortChannel>,
      std::unique_ptr<WebServiceWorker::Handle>) override;
  void DispatchFetchEvent(int fetch_event_id,
                          const WebServiceWorkerRequest&,
                          bool navigation_preload_sent) override;
  void DispatchForeignFetchEvent(int fetch_event_id,
                                 const WebServiceWorkerRequest&) override;
  void DispatchInstallEvent(int) override;
  void DispatchNotificationClickEvent(int,
                                      const WebString& notification_id,
                                      const WebNotificationData&,
                                      int action_index,
                                      const WebString& reply) override;
  void DispatchNotificationCloseEvent(int,
                                      const WebString& notification_id,
                                      const WebNotificationData&) override;
  void DispatchPushEvent(int, const WebString& data) override;
  void DispatchSyncEvent(int, const WebString& tag, LastChanceOption) override;
  void DispatchAbortPaymentEvent(int) override;
  void DispatchCanMakePaymentEvent(int,
                                   const WebCanMakePaymentEventData&) override;
  void DispatchPaymentRequestEvent(int,
                                   const WebPaymentRequestEventData&) override;
  bool HasFetchEventHandler() override;
  void OnNavigationPreloadResponse(
      int fetch_event_id,
      std::unique_ptr<WebURLResponse>,
      std::unique_ptr<WebDataConsumerHandle>) override;
  void OnNavigationPreloadError(
      int fetch_event_id,
      std::unique_ptr<WebServiceWorkerError>) override;
  void OnNavigationPreloadComplete(int fetch_event_id,
                                   double completion_time,
                                   int64_t encoded_data_length,
                                   int64_t encoded_body_length,
                                   int64_t decoded_body_length) override;

  // WorkerReportingProxy overrides:
  void CountFeature(WebFeature) override;
  void CountDeprecation(WebFeature) override;
  void ReportException(const String& error_message,
                       std::unique_ptr<SourceLocation>,
                       int exception_id) override;
  void ReportConsoleMessage(MessageSource,
                            MessageLevel,
                            const String& message,
                            SourceLocation*) override;
  void PostMessageToPageInspector(int session_id, const String&) override;
  void DidCreateWorkerGlobalScope(WorkerOrWorkletGlobalScope*) override;
  void DidInitializeWorkerContext() override;
  void DidLoadInstalledScript(
      const ContentSecurityPolicyResponseHeaders&,
      const String& referrer_policy_on_worker_thread) override;
  void WillEvaluateWorkerScript(size_t script_size,
                                size_t cached_metadata_size) override;
  void WillEvaluateImportedScript(size_t script_size,
                                  size_t cached_metadata_size) override;
  void DidEvaluateWorkerScript(bool success) override;
  void DidCloseWorkerGlobalScope() override;
  void WillDestroyWorkerGlobalScope() override;
  void DidTerminateWorkerThread() override;

  DECLARE_TRACE();

  // Detaches this proxy object entirely from the outside world, clearing out
  // all references.
  //
  // It is called on the main thread during WebEmbeddedWorkerImpl finalization
  // _after_ the worker thread using the proxy has been terminated.
  void Detach();

 private:
  ServiceWorkerGlobalScopeProxy(WebEmbeddedWorkerImpl&,
                                WebServiceWorkerContextClient&);

  WebServiceWorkerContextClient& Client() const;
  ServiceWorkerGlobalScope* WorkerGlobalScope() const;

  // Non-null until the WebEmbeddedWorkerImpl explicitly detach()es
  // as part of its finalization.
  WebEmbeddedWorkerImpl* embedded_worker_;

  Member<ParentFrameTaskRunners> parent_frame_task_runners_;

  // The worker thread uses this map to track |FetchEvent|s created
  // on the worker thread (heap.) But as the proxy object is created
  // on the main thread & its heap, we must use a cross-heap reference
  // to each |FetchEvent| so as to obey the "per-thread heap rule" that
  // a heap should only have per-thread heap references. Keeping a
  // cross-heap reference requires the use of a CrossThreadPersistent<>
  // to remain safe and sound.
  //
  HashMap<int, CrossThreadPersistent<FetchEvent>> pending_preload_fetch_events_;

  WebServiceWorkerContextClient* client_;

  CrossThreadPersistent<ServiceWorkerGlobalScope> worker_global_scope_;
};

}  // namespace blink

#endif  // ServiceWorkerGlobalScopeProxy_h
