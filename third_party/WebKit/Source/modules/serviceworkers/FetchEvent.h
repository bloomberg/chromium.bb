// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchEvent_h
#define FetchEvent_h

#include "modules/EventModules.h"
#include "modules/serviceworkers/Request.h"
#include "modules/serviceworkers/RespondWithObserver.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class Request;
class RespondWithObserver;

// A fetch event is dispatched by the client to a service worker's script
// context. RespondWithObserver can be used to notify the client about the
// service worker's response.
class FetchEvent final : public Event {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<FetchEvent> create();
    static PassRefPtrWillBeRawPtr<FetchEvent> create(RespondWithObserver*, Request*);

    Request* request() const;
    bool isReload() const;

    void respondWith(ScriptState*, const ScriptValue&, ExceptionState&);

    virtual const AtomicString& interfaceName() const override;

    void setIsReload(bool);

    virtual void trace(Visitor*) override;

protected:
    FetchEvent();
    FetchEvent(RespondWithObserver*, Request*);

private:
    PersistentWillBeMember<RespondWithObserver> m_observer;
    PersistentWillBeMember<Request> m_request;
    bool m_isReload;
};

} // namespace blink

#endif // FetchEvent_h
