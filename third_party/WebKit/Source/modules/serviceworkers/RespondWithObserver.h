// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RespondWithObserver_h
#define RespondWithObserver_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/events/EventTarget.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponseError.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class ScriptPromise;
class ScriptState;
class ScriptValue;
class WaitUntilObserver;

// This is a base class to implement respondWith. The respondWith has the three
// types of results: fulfilled, rejected and not called. Derived classes for
// each event should implement the procedure of the three behaviors by
// overriding onResponseFulfilled, onResponseRejected and onNoResponse.
class MODULES_EXPORT RespondWithObserver
    : public GarbageCollectedFinalized<RespondWithObserver>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(RespondWithObserver);

 public:
  virtual ~RespondWithObserver() = default;

  void ContextDestroyed(ExecutionContext*) override;

  void WillDispatchEvent();
  void DidDispatchEvent(DispatchEventResult dispatch_result);

  // The respondWith() observes the promise until the given promise is resolved
  // or rejected and then delays calling ServiceWorkerGlobalScopeClient::
  // didHandle*Event() in order to notify the result to the client.
  void RespondWith(ScriptState*, ScriptPromise, ExceptionState&);

  // Called when the respondWith() promise was rejected.
  virtual void OnResponseRejected(WebServiceWorkerResponseError) = 0;

  // Called when the respondWith() promise was fulfilled.
  virtual void OnResponseFulfilled(const ScriptValue&) = 0;

  // Called when the event handler finished without calling respondWith().
  virtual void OnNoResponse() = 0;

  DECLARE_VIRTUAL_TRACE();

 protected:
  RespondWithObserver(ExecutionContext*, int event_id, WaitUntilObserver*);
  const int event_id_;
  double event_dispatch_time_ = 0;

 private:
  class ThenFunction;

  void ResponseWasRejected(WebServiceWorkerResponseError, const ScriptValue&);
  void ResponseWasFulfilled(const ScriptValue&);

  enum State { kInitial, kPending, kDone };
  State state_;

  // RespondWith should ensure the ExtendableEvent is alive until the promise
  // passed to RespondWith is resolved. The lifecycle of the ExtendableEvent
  // is controlled by WaitUntilObserver, so not only
  // WaitUntilObserver::ThenFunction but RespondWith needs to have a strong
  // reference to the WaitUntilObserver.
  Member<WaitUntilObserver> observer_;
};

}  // namespace blink

#endif  // RespondWithObserver_h
