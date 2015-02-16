// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AcceptConnectionObserver_h
#define AcceptConnectionObserver_h

#include "core/dom/ContextLifecycleObserver.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class ScriptPromise;
class ScriptState;
class ScriptValue;

// This class observes the service worker's handling of a CrossOriginConnectEvent
// and notified the client of the result. Created for each instance of
// CrossOriginConnectEvent.
class AcceptConnectionObserver final : public GarbageCollectedFinalized<AcceptConnectionObserver>, public ContextLifecycleObserver {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(AcceptConnectionObserver);
public:
    static AcceptConnectionObserver* create(ExecutionContext*, int eventID);

    void contextDestroyed() override;

    // Must be called after dispatching the event. Will cause the connection to
    // be rejected if no call to acceptConnection was made.
    void didDispatchEvent();

    // Observes the promise and delays calling didHandleCrossOriginConnectEvent()
    // until the given promise is resolved or rejected.
    void acceptConnection(ScriptState*, ScriptPromise, ExceptionState&);

    void connectionWasRejected();
    void connectionWasAccepted(const ScriptValue&);

    DECLARE_VIRTUAL_TRACE();

private:
    class ThenFunction;

    AcceptConnectionObserver(ExecutionContext*, int eventID);

    int m_eventID;

    enum State { Initial, Pending, Done };
    State m_state;
};

} // namespace blink

#endif // AcceptConnectionObserver_h
