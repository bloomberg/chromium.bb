// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchEvent_h
#define FetchEvent_h

#include "modules/EventModules.h"
#include "modules/serviceworkers/Request.h"
#include "modules/serviceworkers/RespondWithObserver.h"

namespace blink {

class ExecutionContext;
class Request;
class RespondWithObserver;

// A fetch event is dispatched by the client to a service worker's script
// context. RespondWithObserver can be used to notify the client about the
// service worker's response.
class FetchEvent FINAL : public Event {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<FetchEvent> create();
    static PassRefPtrWillBeRawPtr<FetchEvent> create(RespondWithObserver*, PassRefPtrWillBeRawPtr<Request>);

    PassRefPtrWillBeRawPtr<Request> request() const;
    bool isReload() const;

    void respondWith(ScriptState*, const ScriptValue&);

    virtual const AtomicString& interfaceName() const OVERRIDE;

    void setIsReload(bool);

    virtual void trace(Visitor*) OVERRIDE;

protected:
    FetchEvent();
    FetchEvent(RespondWithObserver*, PassRefPtrWillBeRawPtr<Request>);

private:
    PersistentWillBeMember<RespondWithObserver> m_observer;
    RefPtrWillBeMember<Request> m_request;
    bool m_isReload;
};

} // namespace blink

#endif // FetchEvent_h
