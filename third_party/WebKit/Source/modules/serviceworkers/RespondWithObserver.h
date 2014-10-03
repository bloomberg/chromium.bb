// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RespondWithObserver_h
#define RespondWithObserver_h

#include "core/dom/ContextLifecycleObserver.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class Response;
class ScriptState;
class ScriptValue;

// This class observes the service worker's handling of a FetchEvent and
// notifies the client.
class RespondWithObserver FINAL : public GarbageCollectedFinalized<RespondWithObserver>, public ContextLifecycleObserver {
public:
    static RespondWithObserver* create(ExecutionContext*, int eventID, WebURLRequest::FetchRequestMode, WebURLRequest::FrameType);

    virtual void contextDestroyed() OVERRIDE;

    void didDispatchEvent();

    // Observes the promise and delays calling didHandleFetchEvent() until the
    // given promise is resolved or rejected.
    void respondWith(ScriptState*, const ScriptValue&, ExceptionState&);

    void responseWasRejected();
    void responseWasFulfilled(const ScriptValue&);

    void trace(Visitor*) { }

private:
    class ThenFunction;

    RespondWithObserver(ExecutionContext*, int eventID, WebURLRequest::FetchRequestMode, WebURLRequest::FrameType);

    int m_eventID;
    WebURLRequest::FetchRequestMode m_requestMode;
    WebURLRequest::FrameType m_frameType;

    enum State { Initial, Pending, Done };
    State m_state;
};

} // namespace blink

#endif // RespondWithObserver_h
