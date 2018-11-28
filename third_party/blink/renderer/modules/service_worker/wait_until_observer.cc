// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/wait_until_observer.h"

#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// Timeout before a service worker that was given window interaction
// permission loses them. The unit is seconds.
const unsigned kWindowInteractionTimeout = 10;
const unsigned kWindowInteractionTimeoutForTest = 1;

TimeDelta WindowInteractionTimeout() {
  return TimeDelta::FromSeconds(WebTestSupport::IsRunningWebTest()
                                    ? kWindowInteractionTimeoutForTest
                                    : kWindowInteractionTimeout);
}

}  // anonymous namespace

class WaitUntilObserver::ThenFunction final : public ScriptFunction {
 public:
  enum ResolveType {
    kFulfilled,
    kRejected,
  };

  static v8::Local<v8::Function> CreateFunction(
      ScriptState* script_state,
      WaitUntilObserver* observer,
      ResolveType type,
      PromiseSettledCallback callback) {
    ThenFunction* self = MakeGarbageCollected<ThenFunction>(
        script_state, observer, type, std::move(callback));
    return self->BindToV8Function();
  }

  ThenFunction(ScriptState* script_state,
               WaitUntilObserver* observer,
               ResolveType type,
               PromiseSettledCallback callback)
      : ScriptFunction(script_state),
        observer_(observer),
        resolve_type_(type),
        callback_(std::move(callback)) {}

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(observer_);
    ScriptFunction::Trace(visitor);
  }

 private:
  ScriptValue Call(ScriptValue value) override {
    DCHECK(observer_);
    DCHECK(resolve_type_ == kFulfilled || resolve_type_ == kRejected);
    if (callback_)
      callback_.Run(value);
    // According from step 4 of ExtendableEvent::waitUntil() in spec:
    // https://w3c.github.io/ServiceWorker/#dom-extendableevent-waituntil
    // "Upon fulfillment or rejection of f, queue a microtask to run these
    // substeps: Decrement the pending promises count by one."

    // At this time point the microtask A running resolve/reject function of
    // this promise has already been queued, in order to allow microtask A to
    // call waitUntil, we enqueue another microtask B to delay the promise
    // settled notification to |observer_|, thus A will run before B so A can
    // call waitUntil well, but any other microtask C possibly enqueued by A
    // will run after B so C maybe can't call waitUntil if there has no any
    // extend lifetime promise at that time.
    if (resolve_type_ == kRejected) {
      Microtask::EnqueueMicrotask(
          WTF::Bind(&WaitUntilObserver::OnPromiseRejected,
                    WrapPersistent(observer_.Get())));
      value =
          ScriptPromise::Reject(value.GetScriptState(), value).GetScriptValue();
    } else {
      Microtask::EnqueueMicrotask(
          WTF::Bind(&WaitUntilObserver::OnPromiseFulfilled,
                    WrapPersistent(observer_.Get())));
    }
    observer_ = nullptr;
    return value;
  }

  Member<WaitUntilObserver> observer_;
  ResolveType resolve_type_;
  PromiseSettledCallback callback_;
};

WaitUntilObserver* WaitUntilObserver::Create(ExecutionContext* context,
                                             EventType type,
                                             int event_id) {
  return new WaitUntilObserver(context, type, event_id);
}

void WaitUntilObserver::WillDispatchEvent() {
  // When handling a notificationclick, paymentrequest, or backgroundfetchclick
  // event, we want to allow one window to be focused or opened. These calls are
  // allowed between the call to willDispatchEvent() and the last call to
  // DecrementPendingPromiseCount(). If waitUntil() isn't called, that means
  // between willDispatchEvent() and didDispatchEvent().
  if (type_ == kNotificationClick || type_ == kPaymentRequest ||
      type_ == kBackgroundFetchClick) {
    execution_context_->AllowWindowInteraction();
  }

  DCHECK_EQ(EventDispatchState::kInitial, event_dispatch_state_);
  event_dispatch_state_ = EventDispatchState::kDispatching;
}

void WaitUntilObserver::DidDispatchEvent(bool event_dispatch_failed) {
  event_dispatch_state_ = event_dispatch_failed
                              ? EventDispatchState::kFailed
                              : EventDispatchState::kDispatched;
  MaybeCompleteEvent();
}

void WaitUntilObserver::WaitUntil(ScriptState* script_state,
                                  ScriptPromise script_promise,
                                  ExceptionState& exception_state,
                                  PromiseSettledCallback on_promise_fulfilled,
                                  PromiseSettledCallback on_promise_rejected) {
  DCHECK_NE(event_dispatch_state_, EventDispatchState::kInitial);

  if (!IsEventActive(script_state)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The event handler is already finished and no extend lifetime "
        "promises are outstanding.");
    return;
  }

  if (!execution_context_)
    return;

  // When handling a notificationclick event, we want to allow one window to
  // be focused or opened. See comments in ::willDispatchEvent(). When
  // waitUntil() is being used, opening or closing a window must happen in a
  // timeframe specified by windowInteractionTimeout(), otherwise the calls
  // will fail.
  if (type_ == kNotificationClick) {
    consume_window_interaction_timer_.StartOneShot(WindowInteractionTimeout(),
                                                   FROM_HERE);
  }

  IncrementPendingPromiseCount();

  // Call Then() separately for fulfilled and rejected cases. This ensures
  // throwing an exception in |on_promise_fulfilled| doesn't invoke
  // |on_promise_rejected|.
  script_promise.Then(
      ThenFunction::CreateFunction(script_state, this, ThenFunction::kFulfilled,
                                   std::move(on_promise_fulfilled)),
      {});
  script_promise.Then({}, ThenFunction::CreateFunction(
                              script_state, this, ThenFunction::kRejected,
                              std::move(on_promise_rejected)));
}

bool WaitUntilObserver::IsEventActive(ScriptState* script_state) const {
  if (pending_promises_ > 0)
    return true;

  switch (event_dispatch_state_) {
    case EventDispatchState::kDispatching:
      // DidDispatchEvent() is called after both the event handler
      // execution finished and microtasks queued by the event handler execution
      // finished, it's hard to get the precise time point between the 2
      // execution phases.
      // So even in EventDispatchState::kDispatching state at this time point,
      // running microtask indicates that event handler execution has actually
      // finished, in such case if there aren't any outstanding extend lifetime
      // promises.
      return !v8::MicrotasksScope::IsRunningMicrotasks(
          script_state->GetIsolate());
    case EventDispatchState::kInitial:
    case EventDispatchState::kDispatched:
    case EventDispatchState::kFailed:
      return false;
  }
  NOTREACHED();
}

WaitUntilObserver::WaitUntilObserver(ExecutionContext* context,
                                     EventType type,
                                     int event_id)
    : execution_context_(context),
      type_(type),
      event_id_(event_id),
      consume_window_interaction_timer_(
          Thread::Current()->GetTaskRunner(),
          this,
          &WaitUntilObserver::ConsumeWindowInteraction) {}

void WaitUntilObserver::OnPromiseFulfilled() {
  DecrementPendingPromiseCount();
}

void WaitUntilObserver::OnPromiseRejected() {
  has_rejected_promise_ = true;
  DecrementPendingPromiseCount();
}

void WaitUntilObserver::IncrementPendingPromiseCount() {
  ++pending_promises_;
}

void WaitUntilObserver::DecrementPendingPromiseCount() {
  DCHECK_GT(pending_promises_, 0);
  --pending_promises_;
  MaybeCompleteEvent();
}

void WaitUntilObserver::MaybeCompleteEvent() {
  if (!execution_context_)
    return;

  switch (event_dispatch_state_) {
    case EventDispatchState::kInitial:
      NOTREACHED();
      return;
    case EventDispatchState::kDispatching:
      // Still dispatching, do not complete the event.
      return;
    case EventDispatchState::kDispatched:
      // Still waiting for a promise, do not complete the event.
      if (pending_promises_ != 0)
        return;
      // Dispatch finished and there are no pending promises, complete the
      // event.
      break;
    case EventDispatchState::kFailed:
      // Dispatch had some error, complete the event immediately.
      break;
  }

  ServiceWorkerGlobalScopeClient* client =
      ServiceWorkerGlobalScopeClient::From(execution_context_);
  mojom::ServiceWorkerEventStatus status =
      (event_dispatch_state_ == EventDispatchState::kFailed ||
       has_rejected_promise_)
          ? mojom::ServiceWorkerEventStatus::REJECTED
          : mojom::ServiceWorkerEventStatus::COMPLETED;
  switch (type_) {
    case kAbortPayment:
      client->DidHandleAbortPaymentEvent(event_id_, status);
      break;
    case kActivate:
      client->DidHandleActivateEvent(event_id_, status);
      break;
    case kCanMakePayment:
      client->DidHandleCanMakePaymentEvent(event_id_, status);
      break;
    case kCookieChange:
      client->DidHandleCookieChangeEvent(event_id_, status);
      break;
    case kFetch:
      client->DidHandleFetchEvent(event_id_, status);
      break;
    case kInstall:
      To<ServiceWorkerGlobalScope>(*execution_context_).SetIsInstalling(false);
      client->DidHandleInstallEvent(event_id_, status);
      break;
    case kMessage:
      client->DidHandleExtendableMessageEvent(event_id_, status);
      break;
    case kNotificationClick:
      client->DidHandleNotificationClickEvent(event_id_, status);
      consume_window_interaction_timer_.Stop();
      ConsumeWindowInteraction(nullptr);
      break;
    case kNotificationClose:
      client->DidHandleNotificationCloseEvent(event_id_, status);
      break;
    case kPush:
      client->DidHandlePushEvent(event_id_, status);
      break;
    case kSync:
      client->DidHandleSyncEvent(event_id_, status);
      break;
    case kPaymentRequest:
      client->DidHandlePaymentRequestEvent(event_id_, status);
      break;
    case kBackgroundFetchAbort:
      client->DidHandleBackgroundFetchAbortEvent(event_id_, status);
      break;
    case kBackgroundFetchClick:
      client->DidHandleBackgroundFetchClickEvent(event_id_, status);
      break;
    case kBackgroundFetchFail:
      client->DidHandleBackgroundFetchFailEvent(event_id_, status);
      break;
    case kBackgroundFetchSuccess:
      client->DidHandleBackgroundFetchSuccessEvent(event_id_, status);
      break;
  }
  execution_context_ = nullptr;
}

void WaitUntilObserver::ConsumeWindowInteraction(TimerBase*) {
  if (!execution_context_)
    return;
  execution_context_->ConsumeWindowInteraction();
}

void WaitUntilObserver::Trace(blink::Visitor* visitor) {
  visitor->Trace(execution_context_);
}

}  // namespace blink
