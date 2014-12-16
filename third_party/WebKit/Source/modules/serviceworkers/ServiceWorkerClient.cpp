// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/ServiceWorkerClient.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "public/platform/WebString.h"
#include "wtf/RefPtr.h"

namespace blink {

ServiceWorkerClient* ServiceWorkerClient::create(const WebServiceWorkerClientInfo& info)
{
    return new ServiceWorkerClient(info);
}

ServiceWorkerClient::ServiceWorkerClient(const WebServiceWorkerClientInfo& info)
    : m_id(info.clientID)
    , m_visibilityState(info.visibilityState)
    , m_isFocused(info.isFocused)
    , m_url(info.url.string())
    , m_frameType(info.frameType)
{
}

ServiceWorkerClient::~ServiceWorkerClient()
{
}

String ServiceWorkerClient::frameType() const
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

void ServiceWorkerClient::postMessage(ExecutionContext* context, PassRefPtr<SerializedScriptValue> message, const MessagePortArray* ports, ExceptionState& exceptionState)
{
    // Disentangle the port in preparation for sending it to the remote context.
    OwnPtr<MessagePortChannelArray> channels = MessagePort::disentanglePorts(ports, exceptionState);
    if (exceptionState.hadException())
        return;

    WebString messageString = message->toWireString();
    OwnPtr<WebMessagePortChannelArray> webChannels = MessagePort::toWebMessagePortChannelArray(channels.release());
    ServiceWorkerGlobalScopeClient::from(context)->postMessageToClient(m_id, messageString, webChannels.release());
}

ScriptPromise ServiceWorkerClient::focus(ScriptState* scriptState)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    // FIXME: there should be a check for whether the caller is allowed to focus
    // the client. The promise should succeed with |false| if the call is not
    // allowed. https://crbug.com/437149

    ServiceWorkerGlobalScopeClient::from(scriptState->executionContext())->focus(m_id, new CallbackPromiseAdapter<bool, ServiceWorkerError>(resolver));
    return promise;
}

} // namespace blink
