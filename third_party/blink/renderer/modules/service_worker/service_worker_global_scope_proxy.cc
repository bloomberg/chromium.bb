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

#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope_proxy.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom-blink.h"
#include "third_party/blink/public/mojom/payments/payment_handler_host.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom-blink.h"
#include "third_party/blink/public/platform/modules/notifications/web_notification_data.h"
#include "third_party/blink/public/platform/modules/payments/web_payment_request_event_data.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_error.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_request.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_client.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/headers.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/workers/parent_execution_context_task_runners.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_event.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_event_init.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_registration.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_update_ui_event.h"
#include "third_party/blink/renderer/modules/background_sync/sync_event.h"
#include "third_party/blink/renderer/modules/cookie_store/cookie_change_event.h"
#include "third_party/blink/renderer/modules/cookie_store/extendable_cookie_change_event.h"
#include "third_party/blink/renderer/modules/exported/web_embedded_worker_impl.h"
#include "third_party/blink/renderer/modules/notifications/notification.h"
#include "third_party/blink/renderer/modules/notifications/notification_event.h"
#include "third_party/blink/renderer/modules/notifications/notification_event_init.h"
#include "third_party/blink/renderer/modules/payments/abort_payment_event.h"
#include "third_party/blink/renderer/modules/payments/abort_payment_respond_with_observer.h"
#include "third_party/blink/renderer/modules/payments/can_make_payment_event.h"
#include "third_party/blink/renderer/modules/payments/can_make_payment_respond_with_observer.h"
#include "third_party/blink/renderer/modules/payments/payment_event_data_conversion.h"
#include "third_party/blink/renderer/modules/payments/payment_request_event.h"
#include "third_party/blink/renderer/modules/payments/payment_request_event_init.h"
#include "third_party/blink/renderer/modules/payments/payment_request_respond_with_observer.h"
#include "third_party/blink/renderer/modules/push_messaging/push_event.h"
#include "third_party/blink/renderer/modules/push_messaging/push_message_data.h"
#include "third_party/blink/renderer/modules/service_worker/extendable_event.h"
#include "third_party/blink/renderer/modules/service_worker/extendable_message_event.h"
#include "third_party/blink/renderer/modules/service_worker/fetch_event.h"
#include "third_party/blink/renderer/modules/service_worker/fetch_respond_with_observer.h"
#include "third_party/blink/renderer/modules/service_worker/install_event.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_client.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_window_client.h"
#include "third_party/blink/renderer/modules/service_worker/wait_until_observer.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace mojo {

namespace {

blink::mojom::NotificationActionType ToMojomNotificationActionType(
    blink::WebNotificationAction::Type input) {
  switch (input) {
    case blink::WebNotificationAction::kButton:
      return blink::mojom::NotificationActionType::BUTTON;
    case blink::WebNotificationAction::kText:
      return blink::mojom::NotificationActionType::TEXT;
  }

  NOTREACHED();
  return blink::mojom::NotificationActionType::BUTTON;
}

}  // namespace

// Inside Blink we're using mojom structs to represent notification data, not
// WebNotification{Action,Data}, however, we still need WebNotificationData to
// carry data from Content into Blink, so, for now we need these type
// converters. They would disappear once we eliminate the abstract interface
// layer blink::WebServiceWorkerContextProxy via Onion Soup effort later.
template <>
struct TypeConverter<blink::mojom::blink::NotificationActionPtr,
                     blink::WebNotificationAction> {
  static blink::mojom::blink::NotificationActionPtr Convert(
      const blink::WebNotificationAction& input) {
    return blink::mojom::blink::NotificationAction::New(
        ToMojomNotificationActionType(input.type), input.action, input.title,
        input.icon, input.placeholder);
  }
};

template <>
struct TypeConverter<blink::mojom::blink::NotificationDataPtr,
                     blink::WebNotificationData> {
  static blink::mojom::blink::NotificationDataPtr Convert(
      const blink::WebNotificationData& input) {
    Vector<int32_t> vibration_pattern;
    vibration_pattern.Append(input.vibrate.Data(),
                             SafeCast<wtf_size_t>(input.vibrate.size()));

    Vector<uint8_t> data;
    data.Append(input.data.Data(), SafeCast<wtf_size_t>(input.data.size()));

    Vector<blink::mojom::blink::NotificationActionPtr> actions;
    for (const auto& web_action : input.actions) {
      actions.push_back(
          blink::mojom::blink::NotificationAction::From(web_action));
    }

    return blink::mojom::blink::NotificationData::New(
        input.title, input.direction, input.lang, input.body, input.tag,
        input.image, input.icon, input.badge, std::move(vibration_pattern),
        input.timestamp, input.renotify, input.silent,
        input.require_interaction, std::move(data), std::move(actions),
        input.show_trigger_timestamp);
  }
};

}  // namespace mojo

namespace blink {
ServiceWorkerGlobalScopeProxy* ServiceWorkerGlobalScopeProxy::Create(
    WebEmbeddedWorkerImpl& embedded_worker,
    WebServiceWorkerContextClient& client) {
  return MakeGarbageCollected<ServiceWorkerGlobalScopeProxy>(embedded_worker,
                                                             client);
}

ServiceWorkerGlobalScopeProxy::~ServiceWorkerGlobalScopeProxy() {
  DCHECK(IsMainThread());
  // Verify that the proxy has been detached.
  DCHECK(!embedded_worker_);
}

void ServiceWorkerGlobalScopeProxy::Trace(blink::Visitor* visitor) {
  visitor->Trace(parent_execution_context_task_runners_);
}

void ServiceWorkerGlobalScopeProxy::BindServiceWorkerHost(
    mojo::ScopedInterfaceEndpointHandle service_worker_host) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WorkerGlobalScope()->BindServiceWorkerHost(
      mojom::blink::ServiceWorkerHostAssociatedPtrInfo(
          std::move(service_worker_host),
          mojom::blink::ServiceWorkerHost::Version_));
}

void ServiceWorkerGlobalScopeProxy::SetRegistration(
    WebServiceWorkerRegistrationObjectInfo info) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WorkerGlobalScope()->SetRegistration(std::move(info));
}

void ServiceWorkerGlobalScopeProxy::ReadyToEvaluateScript() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WorkerGlobalScope()->ReadyToEvaluateScript();
}

void ServiceWorkerGlobalScopeProxy::DispatchBackgroundFetchAbortEvent(
    int event_id,
    WebBackgroundFetchRegistration registration) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kBackgroundFetchAbort, event_id);

  ScriptState* script_state =
      WorkerGlobalScope()->ScriptController()->GetScriptState();

  // Do not remove this, |scope| is needed by
  // BackgroundFetchEvent::Create which eventually calls ToV8.
  ScriptState::Scope scope(script_state);

  BackgroundFetchEventInit* init = BackgroundFetchEventInit::Create();
  init->setRegistration(MakeGarbageCollected<BackgroundFetchRegistration>(
      WorkerGlobalScope()->registration() /* service_worker_registration */,
      std::move(registration)));

  BackgroundFetchEvent* event = BackgroundFetchEvent::Create(
      event_type_names::kBackgroundfetchabort, init, observer);

  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchBackgroundFetchClickEvent(
    int event_id,
    WebBackgroundFetchRegistration registration) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kBackgroundFetchClick, event_id);

  BackgroundFetchEventInit* init = BackgroundFetchEventInit::Create();
  init->setRegistration(MakeGarbageCollected<BackgroundFetchRegistration>(
      WorkerGlobalScope()->registration() /* service_worker_registration */,
      std::move(registration)));

  BackgroundFetchEvent* event = BackgroundFetchEvent::Create(
      event_type_names::kBackgroundfetchclick, init, observer);

  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchBackgroundFetchFailEvent(
    int event_id,
    WebBackgroundFetchRegistration registration) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kBackgroundFetchFail, event_id);

  ScriptState* script_state =
      WorkerGlobalScope()->ScriptController()->GetScriptState();

  // Do not remove this, |scope| is needed by
  // BackgroundFetchSettledEvent::Create which eventually calls ToV8.
  ScriptState::Scope scope(script_state);

  BackgroundFetchEventInit* init = BackgroundFetchEventInit::Create();
  init->setRegistration(MakeGarbageCollected<BackgroundFetchRegistration>(
      WorkerGlobalScope()->registration() /* service_worker_registration */,
      std::move(registration)));

  BackgroundFetchUpdateUIEvent* event = BackgroundFetchUpdateUIEvent::Create(
      event_type_names::kBackgroundfetchfail, init, observer,
      worker_global_scope_->registration());

  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchBackgroundFetchSuccessEvent(
    int event_id,
    WebBackgroundFetchRegistration registration) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kBackgroundFetchSuccess,
      event_id);

  ScriptState* script_state =
      WorkerGlobalScope()->ScriptController()->GetScriptState();

  // Do not remove this, |scope| is needed by
  // BackgroundFetchSettledEvent::Create which eventually calls ToV8.
  ScriptState::Scope scope(script_state);

  BackgroundFetchEventInit* init = BackgroundFetchEventInit::Create();
  init->setRegistration(MakeGarbageCollected<BackgroundFetchRegistration>(
      WorkerGlobalScope()->registration() /* service_worker_registration */,
      std::move(registration)));

  BackgroundFetchUpdateUIEvent* event = BackgroundFetchUpdateUIEvent::Create(
      event_type_names::kBackgroundfetchsuccess, init, observer,
      worker_global_scope_->registration());

  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchActivateEvent(int event_id) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kActivate, event_id);
  Event* event = ExtendableEvent::Create(
      event_type_names::kActivate, ExtendableEventInit::Create(), observer);
  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchCookieChangeEvent(
    int event_id,
    const WebCanonicalCookie& cookie,
    network::mojom::CookieChangeCause change_cause) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kCookieChange, event_id);

  HeapVector<Member<CookieListItem>> changed;
  HeapVector<Member<CookieListItem>> deleted;
  CookieChangeEvent::ToEventInfo(cookie, change_cause, changed, deleted);
  Event* event = ExtendableCookieChangeEvent::Create(
      event_type_names::kCookiechange, std::move(changed), std::move(deleted),
      observer);

  // TODO(pwnall): Handle handle the case when
  //               (changed.IsEmpty() && deleted.IsEmpty()).

  // TODO(pwnall): Investigate dispatching this on cookieStore.
  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchExtendableMessageEvent(
    int event_id,
    TransferableMessage message,
    const WebSecurityOrigin& source_origin,
    const WebServiceWorkerClientInfo& client) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  auto msg = ToBlinkTransferableMessage(std::move(message));
  MessagePortArray* ports =
      MessagePort::EntanglePorts(*worker_global_scope_, std::move(msg.ports));
  String origin;
  if (!source_origin.IsOpaque())
    origin = source_origin.ToString();
  ServiceWorkerClient* source = nullptr;
  if (client.client_type == mojom::ServiceWorkerClientType::kWindow)
    source = ServiceWorkerWindowClient::Create(client);
  else
    source = ServiceWorkerClient::Create(client);
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kMessage, event_id);

  Event* event = ExtendableMessageEvent::Create(std::move(msg.message), origin,
                                                ports, source, observer);
  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchExtendableMessageEvent(
    int event_id,
    TransferableMessage message,
    const WebSecurityOrigin& source_origin,
    WebServiceWorkerObjectInfo info) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  auto msg = ToBlinkTransferableMessage(std::move(message));
  MessagePortArray* ports =
      MessagePort::EntanglePorts(*worker_global_scope_, std::move(msg.ports));
  String origin;
  if (!source_origin.IsOpaque())
    origin = source_origin.ToString();
  ServiceWorker* source = ServiceWorker::From(
      worker_global_scope_->GetExecutionContext(), std::move(info));
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kMessage, event_id);

  Event* event = ExtendableMessageEvent::Create(std::move(msg.message), origin,
                                                ports, source, observer);
  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchFetchEvent(
    int fetch_event_id,
    const WebServiceWorkerRequest& web_request,
    bool navigation_preload_sent) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  ScriptState::Scope scope(
      WorkerGlobalScope()->ScriptController()->GetScriptState());
  WaitUntilObserver* wait_until_observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kFetch, fetch_event_id);
  FetchRespondWithObserver* respond_with_observer =
      FetchRespondWithObserver::Create(
          WorkerGlobalScope(), fetch_event_id, web_request.Url(),
          web_request.Mode(), web_request.RedirectMode(),
          web_request.GetFrameType(), web_request.GetRequestContext(),
          wait_until_observer);
  Request* request = Request::Create(
      WorkerGlobalScope()->ScriptController()->GetScriptState(), web_request);
  request->getHeaders()->SetGuard(Headers::kImmutableGuard);
  FetchEventInit* event_init = FetchEventInit::Create();
  event_init->setCancelable(true);
  event_init->setRequest(request);
  event_init->setClientId(
      web_request.IsMainResourceLoad() ? WebString() : web_request.ClientId());
  event_init->setResultingClientId(
      !web_request.IsMainResourceLoad() ? WebString() : web_request.ClientId());
  event_init->setIsReload(web_request.IsReload());
  ScriptState* script_state =
      WorkerGlobalScope()->ScriptController()->GetScriptState();
  FetchEvent* fetch_event = FetchEvent::Create(
      script_state, event_type_names::kFetch, event_init, respond_with_observer,
      wait_until_observer, navigation_preload_sent);
  if (navigation_preload_sent) {
    // Keep |fetchEvent| until OnNavigationPreloadComplete() or
    // onNavigationPreloadError() will be called.
    pending_preload_fetch_events_.insert(fetch_event_id, fetch_event);
  }

  WorkerGlobalScope()->DispatchExtendableEventWithRespondWith(
      fetch_event, wait_until_observer, respond_with_observer);
}

void ServiceWorkerGlobalScopeProxy::OnNavigationPreloadResponse(
    int fetch_event_id,
    std::unique_ptr<WebURLResponse> response,
    mojo::ScopedDataPipeConsumerHandle data_pipe) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  auto it = pending_preload_fetch_events_.find(fetch_event_id);
  DCHECK(it != pending_preload_fetch_events_.end());
  FetchEvent* fetch_event = it->value.Get();
  DCHECK(fetch_event);
  fetch_event->OnNavigationPreloadResponse(
      WorkerGlobalScope()->ScriptController()->GetScriptState(),
      std::move(response), std::move(data_pipe));
}

void ServiceWorkerGlobalScopeProxy::OnNavigationPreloadError(
    int fetch_event_id,
    std::unique_ptr<WebServiceWorkerError> error) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  FetchEvent* fetch_event = pending_preload_fetch_events_.Take(fetch_event_id);
  DCHECK(fetch_event);
  // Display an error message to the console, preferring the unsanitized one if
  // available.
  const WebString& error_message = error->unsanitized_message.IsEmpty()
                                       ? error->message
                                       : error->unsanitized_message;
  if (!error_message.IsEmpty()) {
    WorkerGlobalScope()->AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kWorker,
        mojom::ConsoleMessageLevel::kError, error_message));
  }
  // Reject the preloadResponse promise.
  fetch_event->OnNavigationPreloadError(
      WorkerGlobalScope()->ScriptController()->GetScriptState(),
      std::move(error));
}

void ServiceWorkerGlobalScopeProxy::OnNavigationPreloadComplete(
    int fetch_event_id,
    TimeTicks completion_time,
    int64_t encoded_data_length,
    int64_t encoded_body_length,
    int64_t decoded_body_length) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  FetchEvent* fetch_event = pending_preload_fetch_events_.Take(fetch_event_id);
  DCHECK(fetch_event);
  fetch_event->OnNavigationPreloadComplete(
      WorkerGlobalScope(), completion_time, encoded_data_length,
      encoded_body_length, decoded_body_length);
}

void ServiceWorkerGlobalScopeProxy::DispatchInstallEvent(int event_id) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kInstall, event_id);
  Event* event =
      InstallEvent::Create(event_type_names::kInstall,
                           ExtendableEventInit::Create(), event_id, observer);
  WorkerGlobalScope()->SetIsInstalling(true);
  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchNotificationClickEvent(
    int event_id,
    const WebString& notification_id,
    const WebNotificationData& data,
    int action_index,
    const WebString& reply) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kNotificationClick, event_id);
  NotificationEventInit* event_init = NotificationEventInit::Create();
  event_init->setNotification(Notification::Create(
      WorkerGlobalScope(), notification_id,
      mojom::blink::NotificationData::From(data), true /* showing */));
  if (0 <= action_index && action_index < static_cast<int>(data.actions.size()))
    event_init->setAction(data.actions[action_index].action);
  event_init->setReply(reply);
  Event* event = NotificationEvent::Create(event_type_names::kNotificationclick,
                                           event_init, observer);
  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchNotificationCloseEvent(
    int event_id,
    const WebString& notification_id,
    const WebNotificationData& data) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kNotificationClose, event_id);
  NotificationEventInit* event_init = NotificationEventInit::Create();
  event_init->setAction(WTF::String());  // initialize as null.
  event_init->setNotification(Notification::Create(
      WorkerGlobalScope(), notification_id,
      mojom::blink::NotificationData::From(data), false /* showing */));
  Event* event = NotificationEvent::Create(event_type_names::kNotificationclose,
                                           event_init, observer);
  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchPushEvent(int event_id,
                                                      const WebString& data) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kPush, event_id);
  Event* event = PushEvent::Create(event_type_names::kPush,
                                   PushMessageData::Create(data), observer);
  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchSyncEvent(int event_id,
                                                      const WebString& id,
                                                      bool last_chance) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kSync, event_id);
  Event* event =
      SyncEvent::Create(event_type_names::kSync, id, last_chance, observer);
  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchAbortPaymentEvent(int event_id) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WaitUntilObserver* wait_until_observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kAbortPayment, event_id);
  AbortPaymentRespondWithObserver* respond_with_observer =
      MakeGarbageCollected<AbortPaymentRespondWithObserver>(
          WorkerGlobalScope(), event_id, wait_until_observer);

  Event* event = AbortPaymentEvent::Create(
      event_type_names::kAbortpayment, ExtendableEventInit::Create(),
      respond_with_observer, wait_until_observer);

  WorkerGlobalScope()->DispatchExtendableEventWithRespondWith(
      event, wait_until_observer, respond_with_observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchCanMakePaymentEvent(
    int event_id,
    const WebCanMakePaymentEventData& web_event_data) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WaitUntilObserver* wait_until_observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kCanMakePayment, event_id);
  CanMakePaymentRespondWithObserver* respond_with_observer =
      MakeGarbageCollected<CanMakePaymentRespondWithObserver>(
          WorkerGlobalScope(), event_id, wait_until_observer);

  Event* event = CanMakePaymentEvent::Create(
      event_type_names::kCanmakepayment,
      PaymentEventDataConversion::ToCanMakePaymentEventInit(
          WorkerGlobalScope()->ScriptController()->GetScriptState(),
          web_event_data),
      respond_with_observer, wait_until_observer);

  WorkerGlobalScope()->DispatchExtendableEventWithRespondWith(
      event, wait_until_observer, respond_with_observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchPaymentRequestEvent(
    int event_id,
    std::unique_ptr<WebPaymentRequestEventData> web_app_request) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WaitUntilObserver* wait_until_observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kPaymentRequest, event_id);
  PaymentRequestRespondWithObserver* respond_with_observer =
      PaymentRequestRespondWithObserver::Create(WorkerGlobalScope(), event_id,
                                                wait_until_observer);
  Event* event = PaymentRequestEvent::Create(
      event_type_names::kPaymentrequest,
      PaymentEventDataConversion::ToPaymentRequestEventInit(
          WorkerGlobalScope()->ScriptController()->GetScriptState(),
          *web_app_request),
      payments::mojom::blink::PaymentHandlerHostPtrInfo(
          std::move(web_app_request->payment_handler_host_handle),
          payments::mojom::blink::PaymentHandlerHost::Version_),
      respond_with_observer, wait_until_observer);

  WorkerGlobalScope()->DispatchExtendableEventWithRespondWith(
      event, wait_until_observer, respond_with_observer);
}

bool ServiceWorkerGlobalScopeProxy::HasFetchEventHandler() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  return WorkerGlobalScope()->HasEventListeners(event_type_names::kFetch);
}

void ServiceWorkerGlobalScopeProxy::CountFeature(WebFeature feature) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  Client().CountFeature(feature);
}

void ServiceWorkerGlobalScopeProxy::CountDeprecation(WebFeature feature) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  // Go through the same code path with countFeature() because a deprecation
  // message is already shown on the worker console and a remaining work is
  // just to record an API use.
  CountFeature(feature);
}

void ServiceWorkerGlobalScopeProxy::ReportException(
    const String& error_message,
    std::unique_ptr<SourceLocation> location,
    int exception_id) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  Client().ReportException(error_message, location->LineNumber(),
                           location->ColumnNumber(), location->Url());
}

void ServiceWorkerGlobalScopeProxy::ReportConsoleMessage(
    mojom::ConsoleMessageSource source,
    mojom::ConsoleMessageLevel level,
    const String& message,
    SourceLocation* location) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  Client().ReportConsoleMessage(source, level, message, location->LineNumber(),
                                location->Url());
}

void ServiceWorkerGlobalScopeProxy::WillInitializeWorkerContext() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  TRACE_EVENT_ASYNC_BEGIN0(
      "ServiceWorker", "ServiceWorkerGlobalScopeProxy::InitializeWorkerContext",
      this);
}

void ServiceWorkerGlobalScopeProxy::DidCreateWorkerGlobalScope(
    WorkerOrWorkletGlobalScope* worker_global_scope) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(!worker_global_scope_);
  worker_global_scope_ =
      static_cast<ServiceWorkerGlobalScope*>(worker_global_scope);
  // ServiceWorkerContextClient uses this task runner to bind its Mojo
  // interface, so use kInternalIPC type.
  // TODO(falken): Consider adding task types for "the handle fetch task source"
  // and "handle functional event task source" defined in the service worker
  // spec and use them when dispatching events.
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner =
      worker_global_scope->GetThread()->GetTaskRunner(TaskType::kInternalIPC);
  Client().WorkerContextStarted(this, std::move(worker_task_runner));
}

void ServiceWorkerGlobalScopeProxy::DidInitializeWorkerContext() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  ScriptState::Scope scope(
      WorkerGlobalScope()->ScriptController()->GetScriptState());
  Client().DidInitializeWorkerContext(
      WorkerGlobalScope()->ScriptController()->GetContext());
  TRACE_EVENT_ASYNC_END1(
      "ServiceWorker", "ServiceWorkerGlobalScopeProxy::InitializeWorkerContext",
      this, "success", true);
}

void ServiceWorkerGlobalScopeProxy::DidFailToInitializeWorkerContext() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  TRACE_EVENT_ASYNC_END1(
      "ServiceWorker", "ServiceWorkerGlobalScopeProxy::InitializeWorkerContext",
      this, "success", false);
}

void ServiceWorkerGlobalScopeProxy::DidLoadClassicScript() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  Client().WorkerScriptLoadedOnWorkerThread();
}

void ServiceWorkerGlobalScopeProxy::DidFailToLoadClassicScript() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  // Tell ServiceWorkerContextClient about the failure. The generic
  // WorkerContextFailedToStart() wouldn't make sense because
  // WorkerContextStarted() was already called.
  Client().FailedToLoadClassicScript();
}

void ServiceWorkerGlobalScopeProxy::DidFetchScript() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  Client().WorkerScriptLoadedOnWorkerThread();
}

void ServiceWorkerGlobalScopeProxy::DidFailToFetchClassicScript() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  Client().FailedToLoadClassicScript();
}

void ServiceWorkerGlobalScopeProxy::DidFailToFetchModuleScript() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  Client().FailedToFetchModuleScript();
}

void ServiceWorkerGlobalScopeProxy::WillEvaluateClassicScript(
    size_t script_size,
    size_t cached_metadata_size) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  TRACE_EVENT_ASYNC_BEGIN0(
      "ServiceWorker", "ServiceWorkerGlobalScopeProxy::EvaluateClassicScript",
      this);
  // TODO(asamidoi): Remove CountWorkerScript which is called for recording
  // metrics if the metrics are no longer referenced, and then merge
  // WillEvaluateClassicScript and WillEvaluateModuleScript for cleanup.
  worker_global_scope_->CountWorkerScript(script_size, cached_metadata_size);
  Client().WillEvaluateScript();
}

void ServiceWorkerGlobalScopeProxy::WillEvaluateImportedClassicScript(
    size_t script_size,
    size_t cached_metadata_size) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  worker_global_scope_->CountImportedScript(script_size, cached_metadata_size);
}

void ServiceWorkerGlobalScopeProxy::WillEvaluateModuleScript() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  Client().WillEvaluateScript();
}

void ServiceWorkerGlobalScopeProxy::DidEvaluateClassicScript(bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WorkerGlobalScope()->DidEvaluateScript();
  Client().DidEvaluateScript(success);
  TRACE_EVENT_ASYNC_END1("ServiceWorker",
                         "ServiceWorkerGlobalScopeProxy::EvaluateClassicScript",
                         this, "success", success);
}

void ServiceWorkerGlobalScopeProxy::DidEvaluateModuleScript(bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WorkerGlobalScope()->DidEvaluateScript();
  Client().DidEvaluateScript(success);
}

void ServiceWorkerGlobalScopeProxy::DidCloseWorkerGlobalScope() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  // close() is not web-exposed for ServiceWorker. This is called when
  // ServiceWorkerGlobalScope internally requests close(), for example, due to
  // failure on startup when installed scripts couldn't be read.
  //
  // This may look like a roundabout way to terminate the thread, but close()
  // seems like the standard way to initiate termination from inside the thread.

  // ServiceWorkerGlobalScope expects us to terminate the thread, so request
  // that here.
  PostCrossThreadTask(
      *parent_execution_context_task_runners_->Get(TaskType::kInternalDefault),
      FROM_HERE,
      CrossThreadBind(&WebEmbeddedWorkerImpl::TerminateWorkerContext,
                      CrossThreadUnretained(embedded_worker_)));

  // NOTE: WorkerThread calls WillDestroyWorkerGlobalScope() synchronously after
  // this function returns, since it calls DidCloseWorkerGlobalScope() then
  // PrepareForShutdownOnWorkerThread().
}

void ServiceWorkerGlobalScopeProxy::WillDestroyWorkerGlobalScope() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  v8::HandleScope handle_scope(WorkerGlobalScope()->GetThread()->GetIsolate());
  Client().WillDestroyWorkerContext(
      WorkerGlobalScope()->ScriptController()->GetContext());
  worker_global_scope_ = nullptr;
}

void ServiceWorkerGlobalScopeProxy::DidTerminateWorkerThread() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  // This must be called after WillDestroyWorkerGlobalScope().
  DCHECK(!worker_global_scope_);
  Client().WorkerContextDestroyed();
}

ServiceWorkerGlobalScopeProxy::ServiceWorkerGlobalScopeProxy(
    WebEmbeddedWorkerImpl& embedded_worker,
    WebServiceWorkerContextClient& client)
    : embedded_worker_(&embedded_worker),
      client_(&client),
      worker_global_scope_(nullptr) {
  DCHECK(IsMainThread());
  DETACH_FROM_THREAD(worker_thread_checker_);
  // ServiceWorker can sometimes run tasks that are initiated by/associated
  // with a document's frame but these documents can be from a different
  // process. So we intentionally populate the task runners with default task
  // runners of the main thread.
  parent_execution_context_task_runners_ =
      ParentExecutionContextTaskRunners::Create();
}

void ServiceWorkerGlobalScopeProxy::Detach() {
  DCHECK(IsMainThread());
  embedded_worker_ = nullptr;
  client_ = nullptr;
}

void ServiceWorkerGlobalScopeProxy::TerminateWorkerContext() {
  DCHECK(IsMainThread());
  embedded_worker_->TerminateWorkerContext();
}

WebServiceWorkerContextClient& ServiceWorkerGlobalScopeProxy::Client() const {
  DCHECK(client_);
  return *client_;
}

ServiceWorkerGlobalScope* ServiceWorkerGlobalScopeProxy::WorkerGlobalScope()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(worker_global_scope_);
  return worker_global_scope_;
}

}  // namespace blink
