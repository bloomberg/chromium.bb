// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WaitUntilObserver_h
#define WaitUntilObserver_h

#include "modules/ModulesExport.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "platform/Timer.h"
#include "wtf/Forward.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class ScriptPromise;
class ScriptState;
class ScriptValue;

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

  // Must be called before and after dispatching the event.
  void WillDispatchEvent();
  void DidDispatchEvent(bool error_occurred);

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

  WaitUntilObserver(ExecutionContext*, EventType, int event_id);

  void ReportError(const ScriptValue&);

  void ConsumeWindowInteraction(TimerBase*);

  Member<ExecutionContext> execution_context_;
  EventType type_;
  int event_id_;
  int pending_activity_ = 0;
  bool has_error_ = false;
  bool event_dispatched_ = false;
  double event_dispatch_time_ = 0;
  TaskRunnerTimer<WaitUntilObserver> consume_window_interaction_timer_;
};

}  // namespace blink

#endif  // WaitUntilObserver_h
