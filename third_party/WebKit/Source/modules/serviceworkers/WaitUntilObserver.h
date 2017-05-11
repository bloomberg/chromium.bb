// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WaitUntilObserver_h
#define WaitUntilObserver_h

#include "modules/ModulesExport.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "platform/Timer.h"
#include "platform/wtf/Forward.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class ScriptPromise;
class ScriptState;

// Created for each ExtendableEvent instance.
class MODULES_EXPORT WaitUntilObserver final
    : public GarbageCollectedFinalized<WaitUntilObserver> {
 public:
  enum EventType {
    kActivate,
    kFetch,
    kInstall,
    kMessage,
    kNotificationClick,
    kNotificationClose,
    kPaymentRequest,
    kPush,
    kSync,
    kBackgroundFetchAbort,
    kBackgroundFetchClick,
    kBackgroundFetchFail,
    kBackgroundFetched
  };

  static WaitUntilObserver* Create(ExecutionContext*, EventType, int event_id);

  // Must be called before dispatching the event.
  void WillDispatchEvent();
  // Must be called after dispatching the event. If |event_dispatch_failed| is
  // true, then DidDispatchEvent() immediately reports to
  // ServiceWorkerGlobalScopeClient that the event finished, without waiting for
  // all waitUntil promises to settle.
  void DidDispatchEvent(bool event_dispatch_failed);

  // Observes the promise and delays calling the continuation until
  // the given promise is resolved or rejected.
  void WaitUntil(ScriptState*, ScriptPromise, ExceptionState&);

  // These methods can be called when the lifecycle of ExtendableEvent
  // observed by this WaitUntilObserver should be extended by other reason
  // than ExtendableEvent.waitUntil.
  // Note: There is no need to call decrementPendingActivity() after the context
  // is being destroyed.
  void IncrementPendingActivity();
  void DecrementPendingActivity();

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class InternalsServiceWorker;
  class ThenFunction;

  enum class EventDispatchState {
    // Event dispatch has not yet finished.
    kInitial,
    // Event dispatch completed. There may still be outstanding waitUntil
    // promises that must settle before notifying ServiceWorkerGlobalScopeClient
    // that the event finished.
    kCompleted,
    // Event dispatch failed. Any outstanding waitUntil promises are ignored.
    kFailed
  };

  WaitUntilObserver(ExecutionContext*, EventType, int event_id);

  // Called when a promise passed to a waitUntil() call that is associated with
  // this observer was fulfilled.
  void OnPromiseFulfilled();
  // Called when a promise passed to a waitUntil() call that is associated with
  // this observer was rejected.
  void OnPromiseRejected();

  void ConsumeWindowInteraction(TimerBase*);

  Member<ExecutionContext> execution_context_;
  EventType type_;
  int event_id_;
  int pending_activity_ = 0;
  EventDispatchState event_dispatch_state_ = EventDispatchState::kInitial;
  bool has_rejected_promise_ = false;
  double event_dispatch_time_ = 0;
  TaskRunnerTimer<WaitUntilObserver> consume_window_interaction_timer_;
};

}  // namespace blink

#endif  // WaitUntilObserver_h
