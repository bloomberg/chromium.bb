// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CLIENT_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/id_map.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/common/service_worker/service_worker_types.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/embedded_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_error.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_client.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"
#include "third_party/blink/public/web/web_embedded_worker.h"
#include "v8/include/v8.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}

namespace blink {
struct PlatformNotificationData;
class WebServiceWorkerContextProxy;
class WebServiceWorkerResponse;
class WebURLResponse;
}

namespace content {

class EmbeddedWorkerInstanceClientImpl;
class HostChildURLLoaderFactoryBundle;
class ServiceWorkerTimeoutTimer;
class WebWorkerFetchContext;

// ServiceWorkerContextClient is a "client" of a service worker execution
// context. It enables communication between the embedder and Blink's
// ServiceWorkerGlobalScope. It is created when the service worker begins
// starting up, and destroyed when the service worker stops. It is owned by
// WebEmbeddedWorkerImpl (which is owned by EmbeddedWorkerInstanceClientImpl).
//
// This class is created and destroyed on the main thread. Unless otherwise
// noted (here or in base class documentation), all methods are called on the
// worker thread.
class CONTENT_EXPORT ServiceWorkerContextClient
    : public blink::WebServiceWorkerContextClient,
      public blink::mojom::ServiceWorker {
 public:
  // Called on the main thread.
  // - |is_starting_installed_worker| is true if the script is already installed
  //   and will be streamed from the browser process.
  // - |owner| must outlive this new instance.
  // - |start_timing| should be initially populated with
  //   |start_worker_received_time|. This instance will fill in the rest during
  //   startup.
  ServiceWorkerContextClient(
      int64_t service_worker_version_id,
      const GURL& service_worker_scope,
      const GURL& script_url,
      bool is_starting_installed_worker,
      blink::mojom::RendererPreferencesPtr renderer_preferences,
      blink::mojom::ServiceWorkerRequest service_worker_request,
      blink::mojom::ControllerServiceWorkerRequest controller_request,
      blink::mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo instance_host,
      blink::mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info,
      EmbeddedWorkerInstanceClientImpl* owner,
      blink::mojom::EmbeddedWorkerStartTimingPtr start_timing,
      blink::mojom::RendererPreferenceWatcherRequest preference_watcher_request,
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo> subresource_loaders,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);
  // Called on the main thread.
  ~ServiceWorkerContextClient() override;

  // Called on the main thread.
  void StartWorkerContext(std::unique_ptr<blink::WebEmbeddedWorker> worker,
                          const blink::WebEmbeddedWorkerStartData& start_data);
  // Called on the main thread.
  blink::WebEmbeddedWorker& worker();
  // Called on the main thread.
  void UpdateSubresourceLoaderFactories(
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          subresource_loader_factories);

  // WebServiceWorkerContextClient overrides.
  void WorkerReadyForInspectionOnMainThread() override;
  void WorkerContextFailedToStartOnMainThread() override;
  void FailedToLoadClassicScript() override;
  void FailedToFetchModuleScript() override;
  void WorkerScriptLoadedOnMainThread() override;
  void WorkerScriptLoadedOnWorkerThread() override;
  void WorkerContextStarted(
      blink::WebServiceWorkerContextProxy* proxy,
      scoped_refptr<base::SequencedTaskRunner> worker_task_runner) override;
  void WillEvaluateScript() override;
  void DidEvaluateScript(bool success) override;
  void DidInitializeWorkerContext(v8::Local<v8::Context> context) override;
  void WillDestroyWorkerContext(v8::Local<v8::Context> context) override;
  void WorkerContextDestroyed() override;
  void CountFeature(blink::mojom::WebFeature feature) override;
  void ReportException(const blink::WebString& error_message,
                       int line_number,
                       int column_number,
                       const blink::WebString& source_url) override;
  void ReportConsoleMessage(blink::mojom::ConsoleMessageSource source,
                            blink::mojom::ConsoleMessageLevel level,
                            const blink::WebString& message,
                            int line_number,
                            const blink::WebString& source_url) override;
  void DidHandleActivateEvent(
      int request_id,
      blink::mojom::ServiceWorkerEventStatus status) override;
  void DidHandleBackgroundFetchAbortEvent(
      int request_id,
      blink::mojom::ServiceWorkerEventStatus status) override;
  void DidHandleBackgroundFetchClickEvent(
      int request_id,
      blink::mojom::ServiceWorkerEventStatus status) override;
  void DidHandleBackgroundFetchFailEvent(
      int request_id,
      blink::mojom::ServiceWorkerEventStatus status) override;
  void DidHandleBackgroundFetchSuccessEvent(
      int request_id,
      blink::mojom::ServiceWorkerEventStatus status) override;
  void DidHandleCookieChangeEvent(
      int request_id,
      blink::mojom::ServiceWorkerEventStatus status) override;
  void DidHandleExtendableMessageEvent(
      int request_id,
      blink::mojom::ServiceWorkerEventStatus status) override;
  void DidHandleInstallEvent(
      int event_id,
      blink::mojom::ServiceWorkerEventStatus status) override;
  void RespondToFetchEventWithNoResponse(
      int fetch_event_id,
      base::TimeTicks event_dispatch_time,
      base::TimeTicks respond_with_settled_time) override;
  void RespondToFetchEvent(int fetch_event_id,
                           const blink::WebServiceWorkerResponse& response,
                           base::TimeTicks event_dispatch_time,
                           base::TimeTicks respond_with_settled_time) override;
  void RespondToFetchEventWithResponseStream(
      int fetch_event_id,
      const blink::WebServiceWorkerResponse& response,
      blink::WebServiceWorkerStreamHandle* web_body_as_stream,
      base::TimeTicks event_dispatch_time,
      base::TimeTicks respond_with_settled_time) override;
  void DidHandleFetchEvent(
      int fetch_event_id,
      blink::mojom::ServiceWorkerEventStatus status) override;
  void DidHandleNotificationClickEvent(
      int request_id,
      blink::mojom::ServiceWorkerEventStatus status) override;
  void DidHandleNotificationCloseEvent(
      int request_id,
      blink::mojom::ServiceWorkerEventStatus status) override;
  void DidHandlePushEvent(
      int request_id,
      blink::mojom::ServiceWorkerEventStatus status) override;
  void DidHandleSyncEvent(
      int request_id,
      blink::mojom::ServiceWorkerEventStatus status) override;
  void RespondToAbortPaymentEvent(int event_id, bool payment_aborted) override;
  void DidHandleAbortPaymentEvent(
      int event_id,
      blink::mojom::ServiceWorkerEventStatus status) override;
  void RespondToCanMakePaymentEvent(int event_id,
                                    bool can_make_payment) override;
  void DidHandleCanMakePaymentEvent(
      int event_id,
      blink::mojom::ServiceWorkerEventStatus status) override;
  void RespondToPaymentRequestEvent(
      int payment_request_id,
      const blink::WebPaymentHandlerResponse& response) override;
  void DidHandlePaymentRequestEvent(
      int payment_request_id,
      blink::mojom::ServiceWorkerEventStatus status) override;
  std::unique_ptr<blink::WebServiceWorkerNetworkProvider>
  CreateServiceWorkerNetworkProviderOnMainThread() override;
  scoped_refptr<blink::WebWorkerFetchContext>
  CreateServiceWorkerFetchContextOnMainThread(
      blink::WebServiceWorkerNetworkProvider*) override;
  int WillStartTask() override;
  void DidEndTask(int task_id) override;

  // Dispatches the fetch event if the worker is running normally, and queues it
  // instead if the worker has already requested to be terminated by the
  // browser. If queued, the event will be dispatched once the worker resumes
  // normal operation (if the browser decides not to terminate it, and instead
  // starts another event), or else is dropped if the worker is terminated.
  //
  // This method needs to be used only if the event comes directly from a
  // client, which means it is coming through the ControllerServiceWorkerImpl.
  void DispatchOrQueueFetchEvent(
      blink::mojom::DispatchFetchEventParamsPtr params,
      blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      DispatchFetchEventCallback callback);

  /////////////////////////////////////////////////////////////////////////////
  // The following are for use by NavigationPreloadRequest.
  //
  // Called to resolve the FetchEvent.preloadResponse promise.
  void OnNavigationPreloadResponse(
      int fetch_event_id,
      std::unique_ptr<blink::WebURLResponse> response,
      mojo::ScopedDataPipeConsumerHandle data_pipe);

  // Called when the navigation preload request completed. Either
  // OnNavigationPreloadComplete() or OnNavigationPreloadError() must be
  // called to release the preload related resources.
  void OnNavigationPreloadComplete(int fetch_event_id,
                                   base::TimeTicks completion_time,
                                   int64_t encoded_data_length,
                                   int64_t encoded_body_length,
                                   int64_t decoded_body_length);

  // Called when an error occurred while receiving the response of the
  // navigation preload request.
  void OnNavigationPreloadError(
      int fetch_event_id,
      std::unique_ptr<blink::WebServiceWorkerError> error);
  /////////////////////////////////////////////////////////////////////////////

 private:
  struct WorkerContextData;
  friend class ControllerServiceWorkerImpl;
  friend class ServiceWorkerContextClientTest;
  FRIEND_TEST_ALL_PREFIXES(
      ServiceWorkerContextClientTest,
      DispatchOrQueueFetchEvent_RequestedTerminationAndDie);
  FRIEND_TEST_ALL_PREFIXES(
      ServiceWorkerContextClientTest,
      DispatchOrQueueFetchEvent_RequestedTerminationAndWakeUp);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerContextClientTest,
                           DispatchOrQueueFetchEvent_NotRequestedTermination);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerContextClientTest, TaskInServiceWorker);

  using FetchHandlerExistence = blink::mojom::FetchHandlerExistence;

  static void ToWebServiceWorkerRequestForFetchEvent(
      blink::mojom::FetchAPIRequestPtr request,
      const std::string& client_id,
      blink::WebServiceWorkerRequest* web_request);

  void SendWorkerStarted(blink::mojom::ServiceWorkerStartStatus status);

  // Implements blink::mojom::ServiceWorker.
  void InitializeGlobalScope(
      blink::mojom::ServiceWorkerHostAssociatedPtrInfo service_worker_host,
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info,
      FetchHandlerExistence fetch_hander_existence) override;
  void DispatchInstallEvent(
      DispatchInstallEventCallback callback) override;
  void DispatchActivateEvent(DispatchActivateEventCallback callback) override;
  void DispatchBackgroundFetchAbortEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      DispatchBackgroundFetchAbortEventCallback callback) override;
  void DispatchBackgroundFetchClickEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      DispatchBackgroundFetchClickEventCallback callback) override;
  void DispatchBackgroundFetchFailEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      DispatchBackgroundFetchFailEventCallback callback) override;
  void DispatchBackgroundFetchSuccessEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      DispatchBackgroundFetchSuccessEventCallback callback) override;
  void DispatchExtendableMessageEvent(
      blink::mojom::ExtendableMessageEventPtr event,
      DispatchExtendableMessageEventCallback callback) override;
  void DispatchExtendableMessageEventWithCustomTimeout(
      blink::mojom::ExtendableMessageEventPtr event,
      base::TimeDelta timeout,
      DispatchExtendableMessageEventCallback callback) override;
  void DispatchFetchEvent(
      blink::mojom::DispatchFetchEventParamsPtr params,
      blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      DispatchFetchEventCallback callback) override;
  void DispatchNotificationClickEvent(
      const std::string& notification_id,
      const blink::PlatformNotificationData& notification_data,
      int action_index,
      const base::Optional<base::string16>& reply,
      DispatchNotificationClickEventCallback callback) override;
  void DispatchNotificationCloseEvent(
      const std::string& notification_id,
      const blink::PlatformNotificationData& notification_data,
      DispatchNotificationCloseEventCallback callback) override;
  void DispatchPushEvent(const base::Optional<std::string>& payload,
                         DispatchPushEventCallback callback) override;
  void DispatchSyncEvent(const std::string& tag,
                         bool last_chance,
                         base::TimeDelta timeout,
                         DispatchSyncEventCallback callback) override;
  void DispatchAbortPaymentEvent(
      payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
      DispatchAbortPaymentEventCallback callback) override;
  void DispatchCanMakePaymentEvent(
      payments::mojom::CanMakePaymentEventDataPtr event_data,
      payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
      DispatchCanMakePaymentEventCallback callback) override;
  void DispatchPaymentRequestEvent(
      payments::mojom::PaymentRequestEventDataPtr event_data,
      payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
      DispatchPaymentRequestEventCallback callback) override;
  void DispatchCookieChangeEvent(
      const net::CanonicalCookie& cookie,
      ::network::mojom::CookieChangeCause cause,
      DispatchCookieChangeEventCallback callback) override;
  void Ping(PingCallback callback) override;
  void SetIdleTimerDelayToZero() override;

  void OnNotificationClickEvent(
      int request_id,
      const std::string& notification_id,
      const blink::PlatformNotificationData& notification_data,
      int action_index,
      const base::NullableString16& reply);
  void OnNotificationCloseEvent(
      int request_id,
      const std::string& notification_id,
      const blink::PlatformNotificationData& notification_data);

  void OnFocusClientResponse(
      int request_id,
      const blink::mojom::ServiceWorkerClientInfo& client);
  void OnNavigateClientResponse(
      int request_id,
      const blink::mojom::ServiceWorkerClientInfo& client);
  void OnNavigateClientError(int request_id, const GURL& url);

  void SetupNavigationPreload(
      int fetch_event_id,
      const GURL& url,
      blink::mojom::FetchEventPreloadHandlePtr preload_handle);

  // Called by ServiceWorkerTimeoutTimer when a certain time has passed since
  // the last task finished.
  void OnIdleTimeout();

  void OnRequestedTermination(bool will_be_terminated);

  // Returns true if the worker has requested to be terminated by the browser
  // process. It does this due to idle timeout.
  bool RequestedTermination() const;

  // Stops the worker context. Called on the main thread.
  void StopWorkerOnMainThread();

  base::WeakPtr<ServiceWorkerContextClient> GetWeakPtr();

  void SetTimeoutTimerForTesting(
      std::unique_ptr<ServiceWorkerTimeoutTimer> timeout_timer);
  ServiceWorkerTimeoutTimer* GetTimeoutTimerForTesting();

  const int64_t service_worker_version_id_;
  const GURL service_worker_scope_;
  const GURL script_url_;
  // True if this service worker was already installed at worker
  // startup time.
  const bool is_starting_installed_worker_;

  blink::mojom::RendererPreferencesPtr renderer_preferences_;
  // Passed on creation of ServiceWorkerFetchContext.
  blink::mojom::RendererPreferenceWatcherRequest preference_watcher_request_;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;

  // Not owned; |this| is destroyed when |proxy_| becomes invalid.
  blink::WebServiceWorkerContextProxy* proxy_;

  // These Mojo objects are bound on the worker thread.
  blink::mojom::ServiceWorkerRequest pending_service_worker_request_;
  blink::mojom::ControllerServiceWorkerRequest pending_controller_request_;

  // This is bound on the main thread.
  scoped_refptr<blink::mojom::ThreadSafeEmbeddedWorkerInstanceHostAssociatedPtr>
      instance_host_;

  // This holds blink.mojom.ServiceWorkerContainer(Host) connections to the
  // browser-side ServiceWorkerProviderHost to keep it alive there.
  // Note: |service_worker_provider_info_->script_loader_factory_ptr_info| is
  // moved to WebServiceWorkerNetworkProviderImpl when
  // CreateServiceWorkerNetworkProvider is called.
  blink::mojom::ServiceWorkerProviderInfoForStartWorkerPtr
      service_worker_provider_info_;

  // Must be accessed on the main thread only.
  EmbeddedWorkerInstanceClientImpl* owner_;

  blink::mojom::BlobRegistryPtr blob_registry_;

  // Initialized on the worker thread in WorkerContextStarted and
  // destructed on the worker thread in WillDestroyWorkerContext.
  //
  // WARNING: This can be cleared at nearly any time, since WillDestroyContext
  // is called by Blink when it decides to terminate the worker thread. This
  // includes during event dispatch if a JavaScript debugger breakpoint pauses
  // execution (see issue 934622). It should be safe to assume |context_| is
  // valid at the start of a task that was posted to |worker_task_runner_|, as
  // that is from WorkerThread::GetTaskRunner() which safely drops the task on
  // worker termination.
  std::unique_ptr<WorkerContextData> context_;

  // Accessed on the worker thread. Passed to the browser process after worker
  // startup completes.
  blink::mojom::EmbeddedWorkerStartTimingPtr start_timing_;

  // A URLLoaderFactory instance used for subresource loading.
  scoped_refptr<HostChildURLLoaderFactoryBundle> loader_factories_;

  // Out-of-process NetworkService:
  // Detects disconnection from the network service.
  network::mojom::URLLoaderFactoryPtr
      network_service_connection_error_handler_holder_;

  std::unique_ptr<blink::WebEmbeddedWorker> worker_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContextClient);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CLIENT_H_
