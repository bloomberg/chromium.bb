// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/presentation/PresentationRequest.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/ExecutionContext.h"
#include "modules/EventTargetModules.h"

namespace blink {

// static
PresentationRequest* PresentationRequest::create(ExecutionContext* executionContext, const String& url, ExceptionState& exceptionState)
{
    KURL parsedUrl = KURL(executionContext->url(), url);
    if (!parsedUrl.isValid() || parsedUrl.protocolIsAbout()) {
        exceptionState.throwTypeError("'" + url + "' can't be resolved to a valid URL.");
        return nullptr;
    }

    PresentationRequest* request = new PresentationRequest(executionContext, parsedUrl);
    request->suspendIfNeeded();
    return request;
}

const AtomicString& PresentationRequest::interfaceName() const
{
    return EventTargetNames::PresentationRequest;
}

ExecutionContext* PresentationRequest::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

bool PresentationRequest::hasPendingActivity() const
{
    // Prevents garbage collecting of this object when not hold by another
    // object but still has listeners registered.
    return hasEventListeners();
}

ScriptPromise PresentationRequest::start(ScriptState* scriptState)
{
    // TODO(mlamouri): migrate from Presentation, see https://crbug.com/510483
    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    return promise;
}

ScriptPromise PresentationRequest::join(ScriptState* scriptState, const String& id)
{
    // TODO(mlamouri): migrate from Presentation, see https://crbug.com/510483
    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    return promise;
}

ScriptPromise PresentationRequest::getAvailability(ScriptState* scriptState)
{
    // TODO(mlamouri): migrate from Presentation, see https://crbug.com/510483
    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    return promise;
}

const KURL& PresentationRequest::url() const
{
    return m_url;
}

DEFINE_TRACE(PresentationRequest)
{
    RefCountedGarbageCollectedEventTargetWithInlineData<PresentationRequest>::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

PresentationRequest::PresentationRequest(ExecutionContext* executionContext, const KURL& url)
    : ActiveDOMObject(executionContext)
    , m_url(url)
{
}

} // namespace blink
