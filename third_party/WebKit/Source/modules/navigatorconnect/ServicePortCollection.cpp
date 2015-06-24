// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/navigatorconnect/ServicePortCollection.h"

#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/EventTargetModules.h"

namespace blink {

PassRefPtrWillBeRawPtr<ServicePortCollection> ServicePortCollection::create(ExecutionContext* context)
{
    return adoptRefWillBeNoop(new ServicePortCollection(context));
}

ServicePortCollection::~ServicePortCollection()
{
}

ScriptPromise ServicePortCollection::connect(ScriptState* scriptState, const String& url, const ServicePortConnectOptions& options)
{
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));
}

ScriptPromise ServicePortCollection::match(ScriptState* scriptState, const ServicePortMatchOptions& options)
{
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));
}

ScriptPromise ServicePortCollection::matchAll(ScriptState* scriptState, const ServicePortMatchOptions& options)
{
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));
}

const AtomicString& ServicePortCollection::interfaceName() const
{
    return EventTargetNames::ServicePortCollection;
}

ExecutionContext* ServicePortCollection::executionContext() const
{
    return ContextLifecycleObserver::executionContext();
}

DEFINE_TRACE(ServicePortCollection)
{
    EventTargetWithInlineData::trace(visitor);
    ContextLifecycleObserver::trace(visitor);
}

ServicePortCollection::ServicePortCollection(ExecutionContext* context)
    : ContextLifecycleObserver(context)
{
}

} // namespace blink
