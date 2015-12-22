// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/InstallEvent.h"

#include "core/dom/ExceptionCode.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"

namespace blink {

PassRefPtrWillBeRawPtr<InstallEvent> InstallEvent::create()
{
    return adoptRefWillBeNoop(new InstallEvent());
}

PassRefPtrWillBeRawPtr<InstallEvent> InstallEvent::create(const AtomicString& type, const ExtendableEventInit& eventInit)
{
    return adoptRefWillBeNoop(new InstallEvent(type, eventInit));
}

PassRefPtrWillBeRawPtr<InstallEvent> InstallEvent::create(const AtomicString& type, const ExtendableEventInit& eventInit, WaitUntilObserver* observer)
{
    return adoptRefWillBeNoop(new InstallEvent(type, eventInit, observer));
}

InstallEvent::~InstallEvent()
{
}

void InstallEvent::registerForeignFetchScopes(ExecutionContext* executionContext, const Vector<String>& subScopes, ExceptionState& exceptionState)
{
    if (!isBeingDispatched()) {
        exceptionState.throwDOMException(InvalidStateError, "The event handler is already finished.");
        return;
    }

    ServiceWorkerGlobalScopeClient* client = ServiceWorkerGlobalScopeClient::from(executionContext);

    String scopePath = static_cast<KURL>(client->scope()).path();
    RefPtr<SecurityOrigin> origin = executionContext->securityOrigin();

    Vector<KURL> subScopeURLs(subScopes.size());
    for (size_t i = 0; i < subScopes.size(); ++i) {
        subScopeURLs[i] = executionContext->completeURL(subScopes[i]);
        if (!subScopeURLs[i].isValid()) {
            exceptionState.throwTypeError("Invalid URL: " + subScopes[i]);
            return;
        }
        subScopeURLs[i].removeFragmentIdentifier();
        if (!origin->canRequest(subScopeURLs[i])) {
            exceptionState.throwTypeError("URL is not within scope: " + subScopes[i]);
            return;
        }
        String subScopePath = subScopeURLs[i].path();
        if (!subScopePath.startsWith(scopePath)) {
            exceptionState.throwTypeError("URL is not within scope: " + subScopes[i]);
            return;
        }
    }
    client->registerForeignFetchScopes(subScopeURLs);
}

const AtomicString& InstallEvent::interfaceName() const
{
    return EventNames::InstallEvent;
}

InstallEvent::InstallEvent()
{
}

InstallEvent::InstallEvent(const AtomicString& type, const ExtendableEventInit& initializer)
    : ExtendableEvent(type, initializer)
{
}

InstallEvent::InstallEvent(const AtomicString& type, const ExtendableEventInit& initializer, WaitUntilObserver* observer)
    : ExtendableEvent(type, initializer, observer)
{
}

} // namespace blink
