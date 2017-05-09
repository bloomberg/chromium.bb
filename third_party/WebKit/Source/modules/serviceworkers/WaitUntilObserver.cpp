// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/WaitUntilObserver.h"

#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScope.h"
#include "platform/LayoutTestSupport.h"
#include "platform/wtf/Assertions.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerEventResult.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// Timeout before a service worker that was given window interaction
// permission loses them. The unit is seconds.
const unsigned kWindowInteractionTimeout = 10;
const unsigned kWindowInteractionTimeoutForTest = 1;

unsigned WindowInteractionTimeout() {
  return LayoutTestSupport::IsRunningLayoutTest()
             ? kWindowInteractionTimeoutForTest
             : kWindowInteractionTimeout;
}

}  // anonymous namespace

class WaitUntilObserver::ThenFunction final : public ScriptFunction {
 public:
  enum ResolveType {
    kFulfilled,
    kRejected,
  };

  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state,
                                                WaitUntilObserver* observer,
                                                ResolveType type) {
    ThenFunction* self = new ThenFunction(script_state, observer, type);
    return self->BindToV8Function();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(observer_);
    ScriptFunction::Trace(visitor);
  }

 private:
  ThenFunction(ScriptState* script_state,
               WaitUntilObserver* observer,
               ResolveType type)
      : ScriptFunction(script_state),
        observer_(observer),
        resolve_type_(type) {}

  ScriptValue Call(ScriptValue value) override {
    DCHECK(observer_);
    ASSERT(resolve_type_ == kFulfilled || resolve_type_ == kRejected);
    if (resolve_type_ == kRejected) {
      observer_->ReportError(value);
      value =
          ScriptPromise::Reject(value.GetScriptState(), value).GetScriptValue();
    }
    observer_->DecrementPendingActivity();
    observer_ = nullptr;
    return value;
  }

  Member<WaitUntilObserver> observer_;
  ResolveType resolve_type_;
};

WaitUntilObserver* WaitUntilObserver::Create(ExecutionContext* context,
                                             EventType type,
                                             int event_id) {
  return new WaitUntilObserver(context, type, event_id);
}

void WaitUntilObserver::WillDispatchEvent() {
  event_dispatch_time_ = WTF::CurrentTime();
  // When handling a notificationclick or paymentrequest event, we want to
  // allow one window to be focused or opened. These calls are allowed between
  // the call to willDispatchEvent() and the last call to
  // decrementPendingActivity(). If waitUntil() isn't called, that means
  // between willDispatchEvent() and didDispatchEvent().
  if (type_ == kNotificationClick || type_ == kPaymentRequest)
    execution_context_->AllowWindowInteraction();

  IncrementPendingActivity();
}

void WaitUntilObserver::DidDispatchEvent(bool error_occurred) {
  if (error_occurred)
    has_error_ = true;
  DecrementPendingActivity();
  event_dispatched_ = true;
}

void WaitUntilObserver::WaitUntil(ScriptState* script_state,
                                  ScriptPromise script_promise,
                                  ExceptionState& exception_state) {
  if (event_dispatched_) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "The event handler is already finished.");
    return;
  }

  if (!execution_context_)
    return;

  // When handling a notificationclick event, we want to allow one window to
  // be focused or opened. See comments in ::willDispatchEvent(). When
  // waitUntil() is being used, opening or closing a window must happen in a
  // timeframe specified by windowInteractionTimeout(), otherwise the calls
  // will fail.
  if (type_ == kNotificationClick)
    consume_window_interaction_timer_.StartOneShot(WindowInteractionTimeout(),
                                                   BLINK_FROM_HERE);

  IncrementPendingActivity();
  script_promise.Then(ThenFunction::CreateFunction(script_state, this,
                                                   ThenFunction::kFulfilled),
                      ThenFunction::CreateFunction(script_state, this,
                                                   ThenFunction::kRejected));
}

WaitUntilObserver::WaitUntilObserver(ExecutionContext* context,
                                     EventType type,
                                     int event_id)
    : execution_context_(context),
      type_(type),
      event_id_(event_id),
      consume_window_interaction_timer_(
          Platform::Current()->CurrentThread()->GetWebTaskRunner(),
          this,
          &WaitUntilObserver::ConsumeWindowInteraction) {}

void WaitUntilObserver::ReportError(const ScriptValue& value) {
  // FIXME: Propagate error message to the client for onerror handling.
  NOTIMPLEMENTED();

  has_error_ = true;
}

void WaitUntilObserver::IncrementPendingActivity() {
  ++pending_activity_;
}

void WaitUntilObserver::DecrementPendingActivity() {
  ASSERT(pending_activity_ > 0);
  if (!execution_context_ || (!has_error_ && --pending_activity_))
    return;

  ServiceWorkerGlobalScopeClient* client =
      ServiceWorkerGlobalScopeClient::From(execution_context_);
  WebServiceWorkerEventResult result =
      has_error_ ? kWebServiceWorkerEventResultRejected
                 : kWebServiceWorkerEventResultCompleted;
  switch (type_) {
    case kActivate:
      client->DidHandleActivateEvent(event_id_, result, event_dispatch_time_);
      break;
    case kFetch:
      client->DidHandleFetchEvent(event_id_, result, event_dispatch_time_);
      break;
    case kInstall:
      client->DidHandleInstallEvent(event_id_, result, event_dispatch_time_);
      break;
    case kMessage:
      client->DidHandleExtendableMessageEvent(event_id_, result,
                                              event_dispatch_time_);
      break;
    case kNotificationClick:
      client->DidHandleNotificationClickEvent(event_id_, result,
                                              event_dispatch_time_);
      consume_window_interaction_timer_.Stop();
      ConsumeWindowInteraction(nullptr);
      break;
    case kNotificationClose:
      client->DidHandleNotificationCloseEvent(event_id_, result,
                                              event_dispatch_time_);
      break;
    case kPush:
      client->DidHandlePushEvent(event_id_, result, event_dispatch_time_);
      break;
    case kSync:
      client->DidHandleSyncEvent(event_id_, result, event_dispatch_time_);
      break;
    case kPaymentRequest:
      client->DidHandlePaymentRequestEvent(event_id_, result,
                                           event_dispatch_time_);
      break;
    case kBackgroundFetchAbort:
      client->DidHandleBackgroundFetchAbortEvent(event_id_, result,
                                                 event_dispatch_time_);
      break;
    case kBackgroundFetchClick:
      client->DidHandleBackgroundFetchClickEvent(event_id_, result,
                                                 event_dispatch_time_);
      break;
    case kBackgroundFetchFail:
      client->DidHandleBackgroundFetchFailEvent(event_id_, result,
                                                event_dispatch_time_);
      break;
    case kBackgroundFetched:
      client->DidHandleBackgroundFetchedEvent(event_id_, result,
                                              event_dispatch_time_);
      break;
  }
  execution_context_ = nullptr;
}

void WaitUntilObserver::ConsumeWindowInteraction(TimerBase*) {
  if (!execution_context_)
    return;
  execution_context_->ConsumeWindowInteraction();
}

DEFINE_TRACE(WaitUntilObserver) {
  visitor->Trace(execution_context_);
}

}  // namespace blink
