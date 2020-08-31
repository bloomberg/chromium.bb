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
#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_GLOBAL_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_GLOBAL_SCOPE_H_

#include <memory>

#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_context.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/request_or_usv_string.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_event_queue.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_installed_scripts_manager.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class CrossOriginResourcePolicyChecker;
class ExceptionState;
class FetchEvent;
class PendingURLLoaderFactoryBundle;
class RespondWithObserver;
class RequestInit;
class ScriptPromise;
class ScriptState;
class ServiceWorker;
class ServiceWorkerClients;
class ServiceWorkerInstalledScriptsManager;
class ServiceWorkerRegistration;
class ServiceWorkerThread;
class WaitUntilObserver;
class WebURLResponse;
class WorkerClassicScriptLoader;
struct GlobalScopeCreationParams;
struct WebServiceWorkerError;
struct WebServiceWorkerObjectInfo;

typedef RequestOrUSVString RequestInfo;

class MODULES_EXPORT ServiceWorkerGlobalScope final
    : public WorkerGlobalScope,
      public mojom::blink::ControllerServiceWorker,
      public mojom::blink::ServiceWorker {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using FetchHandlerExistence = mojom::FetchHandlerExistence;

  static ServiceWorkerGlobalScope* Create(
      ServiceWorkerThread*,
      std::unique_ptr<GlobalScopeCreationParams>,
      std::unique_ptr<ServiceWorkerInstalledScriptsManager>,
      mojo::PendingRemote<mojom::blink::CacheStorage>,
      base::TimeTicks time_origin);

  ServiceWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams>,
      ServiceWorkerThread*,
      std::unique_ptr<ServiceWorkerInstalledScriptsManager>,
      mojo::PendingRemote<mojom::blink::CacheStorage>,
      base::TimeTicks time_origin);
  ~ServiceWorkerGlobalScope() override;

  // ExecutionContext overrides:
  bool IsServiceWorkerGlobalScope() const override { return true; }
  bool ShouldInstallV8Extensions() const final;

  // Implements WorkerGlobalScope:
  void Initialize(const KURL& response_url,
                  network::mojom::ReferrerPolicy response_referrer_policy,
                  network::mojom::IPAddressSpace response_address_space,
                  const Vector<CSPHeaderAndType>& response_csp_headers,
                  const Vector<String>* response_origin_trial_tokens,
                  int64_t appcache_id) override;
  // Fetches and runs the top-level classic worker script.
  void FetchAndRunClassicScript(
      const KURL& script_url,
      const FetchClientSettingsObjectSnapshot& outside_settings_object,
      WorkerResourceTimingNotifier& outside_resource_timing_notifier,
      const v8_inspector::V8StackTraceId& stack_id) override;
  // Fetches and runs the top-level module worker script.
  void FetchAndRunModuleScript(
      const KURL& module_url_record,
      const FetchClientSettingsObjectSnapshot& outside_settings_object,
      WorkerResourceTimingNotifier& outside_resource_timing_notifier,
      network::mojom::CredentialsMode,
      RejectCoepUnsafeNone reject_coep_unsafe_none) override;
  void Dispose() override;
  InstalledScriptsManager* GetInstalledScriptsManager() override;

  // Counts an evaluated script and its size. Called for the main worker script.
  void CountWorkerScript(size_t script_size, size_t cached_metadata_size);

  // Counts an evaluated script and its size. Called for each of imported
  // scripts.
  void CountImportedScript(size_t script_size, size_t cached_metadata_size);

  // Called when the main worker script is evaluated.
  void DidEvaluateScript();

  // ServiceWorkerGlobalScope.idl
  ServiceWorkerClients* clients();
  ServiceWorkerRegistration* registration();
  ::blink::ServiceWorker* serviceWorker();

  ScriptPromise fetch(ScriptState*,
                      const RequestInfo&,
                      const RequestInit*,
                      ExceptionState&);

  ScriptPromise skipWaiting(ScriptState*);

  void BindServiceWorker(mojo::PendingReceiver<mojom::blink::ServiceWorker>);
  void BindControllerServiceWorker(
      mojo::PendingReceiver<mojom::blink::ControllerServiceWorker>);
  void OnNavigationPreloadResponse(
      int fetch_event_id,
      std::unique_ptr<WebURLResponse>,
      mojo::ScopedDataPipeConsumerHandle data_pipe);
  void OnNavigationPreloadError(int fetch_event_id,
                                std::unique_ptr<WebServiceWorkerError>);
  void OnNavigationPreloadComplete(int fetch_event_id,
                                   base::TimeTicks completion_time,
                                   int64_t encoded_data_length,
                                   int64_t encoded_body_length,
                                   int64_t decoded_body_length);
  // Pauses the toplevel script evaluation until ResumeEvaluation is called.
  // Must be called before InitializeGlobalScope().
  void PauseEvaluation();
  // Resumes the toplevel script evaluation. Must be called only after
  // PauseEvaluation() is called.
  void ResumeEvaluation();

  // Creates a ServiceWorkerEventQueue::StayAwakeToken to ensure that the idle
  // timer won't be triggered while any of these are alive.
  std::unique_ptr<ServiceWorkerEventQueue::StayAwakeToken>
  CreateStayAwakeToken();

  // Returns the ServiceWorker object described by the given info. Creates a new
  // object if needed, or else returns the existing one.
  ::blink::ServiceWorker* GetOrCreateServiceWorker(WebServiceWorkerObjectInfo);

  // EventTarget
  const AtomicString& InterfaceName() const override;

  void DispatchExtendableEvent(Event*, WaitUntilObserver*);

  // For ExtendableEvents that also have a respondWith() function.
  void DispatchExtendableEventWithRespondWith(Event*,
                                              WaitUntilObserver*,
                                              RespondWithObserver*);

  bool IsInstalling() const { return is_installing_; }
  void SetIsInstalling(bool is_installing);

  void CountCacheStorageInstalledScript(uint64_t script_size,
                                        uint64_t script_metadata_size);

  void DidHandleInstallEvent(int install_event_id,
                             mojom::ServiceWorkerEventStatus);
  void DidHandleActivateEvent(int event_id, mojom::ServiceWorkerEventStatus);
  void DidHandleBackgroundFetchAbortEvent(int event_id,
                                          mojom::ServiceWorkerEventStatus);
  void DidHandleBackgroundFetchClickEvent(int event_id,
                                          mojom::ServiceWorkerEventStatus);
  void DidHandleBackgroundFetchFailEvent(int event_id,
                                         mojom::ServiceWorkerEventStatus);
  void DidHandleBackgroundFetchSuccessEvent(int event_id,
                                            mojom::ServiceWorkerEventStatus);
  void DidHandleExtendableMessageEvent(int event_id,
                                       mojom::ServiceWorkerEventStatus);

  // RespondToFetchEvent* will be called after the service worker returns a
  // response to a FetchEvent, and DidHandleFetchEvent will be called after the
  // end of FetchEvent's lifecycle. |fetch_event_id| is the id that was passed
  // to DispatchFetchEvent.

  // Used when respondWith() is not called. Tells the browser to fall back to
  // native fetch.
  void RespondToFetchEventWithNoResponse(
      int fetch_event_id,
      const KURL& request_url,
      base::TimeTicks event_dispatch_time,
      base::TimeTicks respond_with_settled_time);
  // Responds to the fetch event with |response|.
  void RespondToFetchEvent(int fetch_event_id,
                           const KURL& request_url,
                           mojom::blink::FetchAPIResponsePtr,
                           base::TimeTicks event_dispatch_time,
                           base::TimeTicks respond_with_settled_time);
  // Responds to the fetch event with |response|, where body is
  // |body_as_stream|.
  void RespondToFetchEventWithResponseStream(
      int fetch_event_id,
      const KURL& request_url,
      mojom::blink::FetchAPIResponsePtr,
      mojom::blink::ServiceWorkerStreamHandlePtr,
      base::TimeTicks event_dispatch_time,
      base::TimeTicks respond_with_settled_time);

  // RespondToAbortPaymentEvent will be called after the service worker
  // returns a response to a AbortPaymentEvent, and DidHandleAbortPaymentEvent
  // will be called after the end of AbortPaymentEvent's lifecycle.
  // |event_id| is the id that was passed to DispatchAbortPaymentEvent.
  void RespondToAbortPaymentEvent(int event_id, bool abort_payment);
  // RespondToCanMakePaymentEvent will be called after the service worker
  // returns the |response| to a CanMakePaymentEvent, and
  // DidHandleCanMakePaymentEvent will be called after the end of
  // CanMakePaymentEvent's lifecycle. |event_id| is the id that was passed to
  // DispatchCanMakePaymentEvent.
  void RespondToCanMakePaymentEvent(
      int event_id,
      payments::mojom::blink::CanMakePaymentResponsePtr response);
  // RespondToPaymentRequestEvent will be called after the service worker
  // returns a response to a PaymentRequestEvent, and
  // DidHandlePaymentRequestEvent will be called after the end of
  // PaymentRequestEvent's lifecycle. |event_id| is the id that was passed
  // to DispatchPaymentRequestEvent.
  void RespondToPaymentRequestEvent(
      int event_id,
      payments::mojom::blink::PaymentHandlerResponsePtr);
  void DidHandleFetchEvent(int fetch_event_id, mojom::ServiceWorkerEventStatus);
  void DidHandleNotificationClickEvent(int event_id,
                                       mojom::ServiceWorkerEventStatus);
  void DidHandleNotificationCloseEvent(int event_id,
                                       mojom::ServiceWorkerEventStatus);
  void DidHandlePushEvent(int push_event_id, mojom::ServiceWorkerEventStatus);
  void DidHandlePushSubscriptionChangeEvent(
      int event_id,
      mojom::ServiceWorkerEventStatus status);
  void DidHandleSyncEvent(int sync_event_id, mojom::ServiceWorkerEventStatus);
  void DidHandlePeriodicSyncEvent(int event_id,
                                  mojom::ServiceWorkerEventStatus status);
  void DidHandleAbortPaymentEvent(int abort_payment_event_id,
                                  mojom::ServiceWorkerEventStatus);
  void DidHandleCanMakePaymentEvent(int payment_request_event_id,
                                    mojom::ServiceWorkerEventStatus);
  void DidHandlePaymentRequestEvent(int payment_request_event_id,
                                    mojom::ServiceWorkerEventStatus);
  void DidHandleCookieChangeEvent(int event_id,
                                  mojom::ServiceWorkerEventStatus);
  void DidHandleContentDeleteEvent(int event_id,
                                   mojom::ServiceWorkerEventStatus);

  mojo::PendingRemote<mojom::blink::CacheStorage> TakeCacheStorage();

  mojom::blink::ServiceWorkerHost* GetServiceWorkerHost();

  DEFINE_ATTRIBUTE_EVENT_LISTENER(install, kInstall)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(activate, kActivate)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(fetch, kFetch)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(message, kMessage)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(messageerror, kMessageerror)

  void Trace(Visitor*) override;

  // Returns true if a FetchEvent exists with the given request URL and
  // is still waiting for a Response.
  bool HasRelatedFetchEvent(const KURL& request_url) const;

  int GetOutstandingThrottledLimit() const override;

 protected:
  // EventTarget
  bool AddEventListenerInternal(
      const AtomicString& event_type,
      EventListener*,
      const AddEventListenerOptionsResolved*) override;

  // WorkerGlobalScope
  bool FetchClassicImportedScript(
      const KURL& script_url,
      KURL* out_response_url,
      String* out_source_code,
      std::unique_ptr<Vector<uint8_t>>* out_cached_meta_data) override;

  ResourceLoadScheduler::ThrottleOptionOverride GetThrottleOptionOverride()
      const override;

 private:
  void importScripts(const Vector<String>& urls, ExceptionState&) override;
  SingleCachedMetadataHandler* CreateWorkerScriptCachedMetadataHandler(
      const KURL& script_url,
      std::unique_ptr<Vector<uint8_t>> meta_data) override;
  void ExceptionThrown(ErrorEvent*) override;

  void DidReceiveResponseForClassicScript(
      WorkerClassicScriptLoader* classic_script_loader);
  void DidFetchClassicScript(WorkerClassicScriptLoader* classic_script_loader,
                             const v8_inspector::V8StackTraceId& stack_id);

  // Loads and runs the installed top-level classic worker script.
  void LoadAndRunInstalledClassicScript(
      const KURL& script_url,
      const v8_inspector::V8StackTraceId& stack_id);

  // https://w3c.github.io/ServiceWorker/#run-service-worker-algorithm
  void RunClassicScript(const KURL& response_url,
                        network::mojom::ReferrerPolicy response_referrer_policy,
                        network::mojom::IPAddressSpace response_address_space,
                        const Vector<CSPHeaderAndType> response_csp_headers,
                        const Vector<String>* response_origin_trial_tokens,
                        const String& source_code,
                        std::unique_ptr<Vector<uint8_t>> cached_meta_data,
                        const v8_inspector::V8StackTraceId&);

  // Counts the |script_size| and |cached_metadata_size| for UMA to measure the
  // number of scripts and the total bytes of scripts.
  void CountScriptInternal(size_t script_size, size_t cached_metadata_size);

  // Called by ServiceWorkerEventQueue just before they start an event.
  void OnBeforeStartEvent(bool is_offline_event);

  // Called by ServiceWorkerEventQueue when a certain time has passed since
  // the last task finished.
  void OnIdleTimeout();

  void OnRequestedTermination(bool will_be_terminated);

  // Returns true if the worker has requested to be terminated by the browser
  // process. It does this due to idle timeout.
  bool RequestedTermination() const;

  void DispatchExtendableMessageEventInternal(
      int event_id,
      mojom::blink::ExtendableMessageEventPtr event);

  using DispatchFetchEventInternalCallback =
      base::OnceCallback<void(mojom::blink::ServiceWorkerEventStatus)>;
  void SetFetchHandlerExistence(FetchHandlerExistence fetch_handler_existence);

  // Implements mojom::blink::ControllerServiceWorker.
  //
  // Dispatches the fetch event if the worker is running normally, and queues it
  // instead if the worker has already requested to be terminated by the
  // browser. If queued, the event will be dispatched once the worker resumes
  // normal operation (if the browser decides not to terminate it, and instead
  // starts another event), or else is dropped if the worker is terminated.
  void DispatchFetchEventForSubresource(
      mojom::blink::DispatchFetchEventParamsPtr params,
      mojo::PendingRemote<mojom::blink::ServiceWorkerFetchResponseCallback>
          response_callback,
      DispatchFetchEventForSubresourceCallback callback) override;
  void Clone(
      mojo::PendingReceiver<mojom::blink::ControllerServiceWorker> receiver,
      const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
      mojo::PendingRemote<
          network::mojom::blink::CrossOriginEmbedderPolicyReporter>
          coep_reporter) override;

  // Implements mojom::blink::ServiceWorker.
  void InitializeGlobalScope(
      mojo::PendingAssociatedRemote<mojom::blink::ServiceWorkerHost>
          service_worker_host,
      mojom::blink::ServiceWorkerRegistrationObjectInfoPtr registration_info,
      mojom::blink::ServiceWorkerObjectInfoPtr service_worker_info,
      mojom::blink::FetchHandlerExistence fetch_handler_existence,
      std::unique_ptr<PendingURLLoaderFactoryBundle>
          subresource_loader_factories,
      mojo::PendingReceiver<mojom::blink::ReportingObserver>) override;
  void DispatchInstallEvent(DispatchInstallEventCallback callback) override;
  void DispatchActivateEvent(DispatchActivateEventCallback callback) override;
  void DispatchBackgroundFetchAbortEvent(
      mojom::blink::BackgroundFetchRegistrationPtr registration,
      DispatchBackgroundFetchAbortEventCallback callback) override;
  void DispatchBackgroundFetchClickEvent(
      mojom::blink::BackgroundFetchRegistrationPtr registration,
      DispatchBackgroundFetchClickEventCallback callback) override;
  void DispatchBackgroundFetchFailEvent(
      mojom::blink::BackgroundFetchRegistrationPtr registration,
      DispatchBackgroundFetchFailEventCallback callback) override;
  void DispatchBackgroundFetchSuccessEvent(
      mojom::blink::BackgroundFetchRegistrationPtr registration,
      DispatchBackgroundFetchSuccessEventCallback callback) override;
  void DispatchExtendableMessageEvent(
      mojom::blink::ExtendableMessageEventPtr event,
      DispatchExtendableMessageEventCallback callback) override;
  void DispatchFetchEventForMainResource(
      mojom::blink::DispatchFetchEventParamsPtr params,
      mojo::PendingRemote<mojom::blink::ServiceWorkerFetchResponseCallback>
          response_callback,
      DispatchFetchEventForMainResourceCallback callback) override;
  void DispatchNotificationClickEvent(
      const String& notification_id,
      mojom::blink::NotificationDataPtr notification_data,
      int action_index,
      const String& reply,
      DispatchNotificationClickEventCallback callback) override;
  void DispatchNotificationCloseEvent(
      const String& notification_id,
      mojom::blink::NotificationDataPtr notification_data,
      DispatchNotificationCloseEventCallback callback) override;
  void DispatchPushEvent(const String& payload,
                         DispatchPushEventCallback callback) override;
  void DispatchPushSubscriptionChangeEvent(
      mojom::blink::PushSubscriptionPtr old_subscription,
      mojom::blink::PushSubscriptionPtr new_subscription,
      DispatchPushSubscriptionChangeEventCallback callback) override;
  void DispatchSyncEvent(const String& tag,
                         bool last_chance,
                         base::TimeDelta timeout,
                         DispatchSyncEventCallback callback) override;
  void DispatchPeriodicSyncEvent(
      const String& tag,
      base::TimeDelta timeout,
      DispatchPeriodicSyncEventCallback callback) override;
  void DispatchAbortPaymentEvent(
      mojo::PendingRemote<
          payments::mojom::blink::PaymentHandlerResponseCallback>
          response_callback,
      DispatchAbortPaymentEventCallback callback) override;
  void DispatchCanMakePaymentEvent(
      payments::mojom::blink::CanMakePaymentEventDataPtr event_data,
      mojo::PendingRemote<
          payments::mojom::blink::PaymentHandlerResponseCallback>
          response_callback,
      DispatchCanMakePaymentEventCallback callback) override;
  void DispatchPaymentRequestEvent(
      payments::mojom::blink::PaymentRequestEventDataPtr event_data,
      mojo::PendingRemote<
          payments::mojom::blink::PaymentHandlerResponseCallback>
          response_callback,
      DispatchPaymentRequestEventCallback callback) override;
  void DispatchCookieChangeEvent(
      network::mojom::blink::CookieChangeInfoPtr change,
      DispatchCookieChangeEventCallback callback) override;
  void DispatchContentDeleteEvent(
      const String& id,
      DispatchContentDeleteEventCallback callback) override;
  void Ping(PingCallback callback) override;
  void SetIdleDelay(base::TimeDelta delay) override;
  void AddMessageToConsole(mojom::blink::ConsoleMessageLevel,
                           const String& message) override;

  void NoteNewFetchEvent(const KURL& request_url);
  void NoteRespondedToFetchEvent(const KURL& request_url);

  // Dispatches the event synchronously. Enqueued by Dispatch*Event methods to
  // the event queue, and executed immediately or sometimes later.
  void StartFetchEvent(
      mojom::blink::DispatchFetchEventParamsPtr params,
      base::WeakPtr<CrossOriginResourcePolicyChecker> corp_checker,
      mojo::PendingRemote<mojom::blink::ServiceWorkerFetchResponseCallback>
          response_callback,
      DispatchFetchEventInternalCallback callback,
      int event_id);
  void StartInstallEvent(DispatchInstallEventCallback callback, int event_id);
  void StartActivateEvent(DispatchActivateEventCallback callback, int event_id);
  void StartBackgroundFetchAbortEvent(
      mojom::blink::BackgroundFetchRegistrationPtr registration,
      DispatchBackgroundFetchAbortEventCallback callback,
      int event_id);
  void StartBackgroundFetchClickEvent(
      mojom::blink::BackgroundFetchRegistrationPtr registration,
      DispatchBackgroundFetchClickEventCallback callback,
      int event_id);
  void StartBackgroundFetchFailEvent(
      mojom::blink::BackgroundFetchRegistrationPtr registration,
      DispatchBackgroundFetchFailEventCallback callback,
      int event_id);
  void StartBackgroundFetchSuccessEvent(
      mojom::blink::BackgroundFetchRegistrationPtr registration,
      DispatchBackgroundFetchSuccessEventCallback callback,
      int event_id);
  void StartExtendableMessageEvent(
      mojom::blink::ExtendableMessageEventPtr event,
      DispatchExtendableMessageEventCallback callback,
      int event_id);
  void StartFetchEventForMainResource(
      mojom::blink::DispatchFetchEventParamsPtr params,
      mojo::PendingRemote<mojom::blink::ServiceWorkerFetchResponseCallback>
          response_callback,
      int event_id);
  void StartNotificationClickEvent(
      String notification_id,
      mojom::blink::NotificationDataPtr notification_data,
      int action_index,
      String reply,
      DispatchNotificationClickEventCallback callback,
      int event_id);
  void StartNotificationCloseEvent(
      String notification_id,
      mojom::blink::NotificationDataPtr notification_data,
      DispatchNotificationCloseEventCallback callback,
      int event_id);
  void StartPushEvent(String payload,
                      DispatchPushEventCallback callback,
                      int event_id);
  void StartPushSubscriptionChangeEvent(
      mojom::blink::PushSubscriptionPtr old_subscription,
      mojom::blink::PushSubscriptionPtr new_subscription,
      DispatchPushSubscriptionChangeEventCallback callback,
      int event_id);
  void StartSyncEvent(String tag,
                      bool last_chance,
                      DispatchSyncEventCallback callback,
                      int event_id);
  void StartPeriodicSyncEvent(String tag,
                              DispatchPeriodicSyncEventCallback callback,
                              int event_id);
  void StartAbortPaymentEvent(
      mojo::PendingRemote<
          payments::mojom::blink::PaymentHandlerResponseCallback>
          response_callback,
      DispatchAbortPaymentEventCallback callback,
      int event_id);
  void StartCanMakePaymentEvent(
      payments::mojom::blink::CanMakePaymentEventDataPtr event_data,
      mojo::PendingRemote<
          payments::mojom::blink::PaymentHandlerResponseCallback>
          response_callback,
      DispatchCanMakePaymentEventCallback callback,
      int event_id);
  void StartPaymentRequestEvent(
      payments::mojom::blink::PaymentRequestEventDataPtr event_data,
      mojo::PendingRemote<
          payments::mojom::blink::PaymentHandlerResponseCallback>
          response_callback,
      DispatchPaymentRequestEventCallback callback,
      int event_id);
  void StartCookieChangeEvent(network::mojom::blink::CookieChangeInfoPtr change,
                              DispatchCookieChangeEventCallback callback,
                              int event_id);
  void StartContentDeleteEvent(String id,
                               DispatchContentDeleteEventCallback callback,
                               int event_id);

  Member<ServiceWorkerClients> clients_;
  Member<ServiceWorkerRegistration> registration_;
  Member<::blink::ServiceWorker> service_worker_;

  // Map from service worker version id to JavaScript ServiceWorker object in
  // current execution context.
  HeapHashMap<int64_t,
              WeakMember<::blink::ServiceWorker>,
              WTF::IntHash<int64_t>,
              WTF::UnsignedWithZeroKeyHashTraits<int64_t>>
      service_worker_objects_;
  bool did_evaluate_script_ = false;
  size_t script_count_ = 0;
  size_t script_total_size_ = 0;
  size_t script_cached_metadata_total_size_ = 0;
  bool is_installing_ = false;
  size_t cache_storage_installed_script_count_ = 0;
  uint64_t cache_storage_installed_script_total_size_ = 0;
  uint64_t cache_storage_installed_script_metadata_total_size_ = 0;

  // Non-null only when this service worker is already installed.
  std::unique_ptr<ServiceWorkerInstalledScriptsManager>
      installed_scripts_manager_;

  // May be provided in the constructor as an optimization so InterfaceProvider
  // doesn't need to be used. Taken at the initial call to
  // ServiceWorkerGlobalScope#caches.
  mojo::PendingRemote<mojom::blink::CacheStorage> cache_storage_remote_;

  // Bound by the first Mojo call received on the service worker thread
  // mojom::blink::ServiceWorker::InitializeGlobalScope().
  mojo::AssociatedRemote<mojom::blink::ServiceWorkerHost> service_worker_host_;

  mojo::Receiver<mojom::blink::ServiceWorker> receiver_{this};

  // Maps for inflight event callbacks.
  // These are mapped from an event id issued from ServiceWorkerEventQueue to
  // the Mojo callback to notify the end of the event.
  HashMap<int, DispatchInstallEventCallback> install_event_callbacks_;
  HashMap<int, DispatchActivateEventCallback> activate_event_callbacks_;
  HashMap<int, DispatchBackgroundFetchAbortEventCallback>
      background_fetch_abort_event_callbacks_;
  HashMap<int, DispatchBackgroundFetchClickEventCallback>
      background_fetch_click_event_callbacks_;
  HashMap<int, DispatchBackgroundFetchFailEventCallback>
      background_fetch_fail_event_callbacks_;
  HashMap<int, DispatchBackgroundFetchSuccessEventCallback>
      background_fetched_event_callbacks_;
  HashMap<int, DispatchSyncEventCallback> sync_event_callbacks_;
  HashMap<int, DispatchPeriodicSyncEventCallback>
      periodic_sync_event_callbacks_;
  HashMap<int,
          mojo::Remote<payments::mojom::blink::PaymentHandlerResponseCallback>>
      abort_payment_result_callbacks_;
  HashMap<int, DispatchCanMakePaymentEventCallback>
      abort_payment_event_callbacks_;
  HashMap<int, DispatchCanMakePaymentEventCallback>
      can_make_payment_event_callbacks_;
  HashMap<int, DispatchPaymentRequestEventCallback>
      payment_request_event_callbacks_;
  HashMap<int, DispatchNotificationClickEventCallback>
      notification_click_event_callbacks_;
  HashMap<int, DispatchNotificationCloseEventCallback>
      notification_close_event_callbacks_;
  HashMap<int, DispatchPushEventCallback> push_event_callbacks_;
  HashMap<int, DispatchPushSubscriptionChangeEventCallback>
      push_subscription_change_event_callbacks_;
  HashMap<int, DispatchFetchEventInternalCallback> fetch_event_callbacks_;
  HashMap<int, DispatchCookieChangeEventCallback>
      cookie_change_event_callbacks_;
  HashMap<int, DispatchExtendableMessageEventCallback> message_event_callbacks_;
  HashMap<int, DispatchContentDeleteEventCallback> content_delete_callbacks_;

  // Maps for response callbacks.
  // These are mapped from an event id to the Mojo interface pointer which is
  // passed from the relevant DispatchSomeEvent() method.
  HashMap<int,
          mojo::Remote<payments::mojom::blink::PaymentHandlerResponseCallback>>
      can_make_payment_result_callbacks_;
  HashMap<int,
          mojo::Remote<payments::mojom::blink::PaymentHandlerResponseCallback>>
      payment_response_callbacks_;
  HashMap<int, mojo::Remote<mojom::blink::ServiceWorkerFetchResponseCallback>>
      fetch_response_callbacks_;

  HeapHashMap<int, Member<FetchEvent>> pending_preload_fetch_events_;

  // Track outstanding FetchEvent objects still waiting for a response by
  // request URL.  This information can be used as a hint that cache_storage
  // or fetch requests to the same URL is likely to be used to satisfy a
  // FetchEvent.  This in turn can allow us to use more aggressive
  // optimizations in these cases.
  HashMap<KURL, int> unresponded_fetch_event_counts_;

  // ServiceWorker event queue where all events are queued before
  // they are dispatched.
  std::unique_ptr<ServiceWorkerEventQueue> event_queue_;

  // InitializeGlobalScope() pauses the top level script evaluation when this
  // flag is true.
  bool pause_evaluation_ = false;
  // ResumeEvaluation() evaluates the top level script when this flag is true.
  bool global_scope_initialized_ = false;

  // Connected by the ServiceWorkerProviderHost in the browser process and by
  // the controllees. |controller_bindings_| should be destroyed before
  // |event_queue_| since the pipe needs to be disconnected before callbacks
  // passed by DispatchSomeEvent() get destructed, which may be stored in
  // |event_queue_|.
  // network::CrossOriginEmbedderPolicy set as the context of
  // mojo::ReceiverSet is the policy for the client which dispatches FetchEvents
  // to the ControllerServiceWorker. It should be referred to before sending the
  // response back to the client.
  mojo::ReceiverSet<mojom::blink::ControllerServiceWorker,
                    std::unique_ptr<CrossOriginResourcePolicyChecker>>
      controller_receivers_;
};

template <>
struct DowncastTraits<ServiceWorkerGlobalScope> {
  static bool AllowFrom(const ExecutionContext& context) {
    return context.IsServiceWorkerGlobalScope();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_GLOBAL_SCOPE_H_
