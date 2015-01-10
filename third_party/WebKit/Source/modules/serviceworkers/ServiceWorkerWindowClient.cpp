// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/ServiceWorkerWindowClient.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/page/PageVisibilityState.h"
#include "core/page/WindowFocusAllowedIndicator.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "public/platform/WebString.h"
#include "wtf/RefPtr.h"

namespace blink {

ServiceWorkerWindowClient* ServiceWorkerWindowClient::create(const WebServiceWorkerClientInfo& info)
{
    return new ServiceWorkerWindowClient(info);
}

ServiceWorkerWindowClient::ServiceWorkerWindowClient(const WebServiceWorkerClientInfo& info)
    : ServiceWorkerClient(info)
    , m_pageVisibilityState(info.pageVisibilityState)
    , m_isFocused(info.isFocused)
    , m_frameType(info.frameType)
{
}

ServiceWorkerWindowClient::~ServiceWorkerWindowClient()
{
}

String ServiceWorkerWindowClient::visibilityState() const
{
    return pageVisibilityStateString(static_cast<PageVisibilityState>(m_pageVisibilityState));
}

String ServiceWorkerWindowClient::frameType() const
{
    DEFINE_STATIC_LOCAL(const String, auxiliary, ("auxiliary"));
    DEFINE_STATIC_LOCAL(const String, nested, ("nested"));
    DEFINE_STATIC_LOCAL(const String, none, ("none"));
    DEFINE_STATIC_LOCAL(const String, topLevel, ("top-level"));

    switch (m_frameType) {
    case WebURLRequest::FrameTypeAuxiliary:
        return auxiliary;
    case WebURLRequest::FrameTypeNested:
        return nested;
    case WebURLRequest::FrameTypeNone:
        return none;
    case WebURLRequest::FrameTypeTopLevel:
        return topLevel;
    }

    ASSERT_NOT_REACHED();
    return String();
}

ScriptPromise ServiceWorkerWindowClient::focus(ScriptState* scriptState)
{
    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (!scriptState->executionContext()->isWindowFocusAllowed()) {
        resolver->resolve(false);
        return promise;
    }
    scriptState->executionContext()->consumeWindowFocus();

    ServiceWorkerGlobalScopeClient::from(scriptState->executionContext())->focus(id(), new CallbackPromiseAdapter<bool, ServiceWorkerError>(resolver));
    return promise;
}

void ServiceWorkerWindowClient::trace(Visitor* visitor)
{
    ServiceWorkerClient::trace(visitor);
}

} // namespace blink
