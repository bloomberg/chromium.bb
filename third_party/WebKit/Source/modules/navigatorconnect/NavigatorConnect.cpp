// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/navigatorconnect/NavigatorConnect.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/MessageChannel.h"
#include "core/dom/MessagePort.h"
#include "public/platform/Platform.h"
#include "public/platform/WebNavigatorConnectProvider.h"

namespace blink {

namespace {

class ConnectCallbacks : public WebNavigatorConnectPortCallbacks {
public:
    ConnectCallbacks(PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver)
        : m_resolver(resolver)
    {
        ASSERT(m_resolver);
    }
    ~ConnectCallbacks() override { }

    void onSuccess(WebMessagePortChannel* channelRaw) override
    {
        OwnPtr<WebMessagePortChannel> channel = adoptPtr(channelRaw);
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
            return;
        }
        RefPtrWillBeRawPtr<MessagePort> port = MessagePort::create(*m_resolver->executionContext());
        port->entangle(channel.release());
        m_resolver->resolve(port);
    }

    void onError() override
    {
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
            return;
        }
        m_resolver->reject(DOMException::create(AbortError));
    }

private:
    RefPtrWillBePersistent<ScriptPromiseResolver> m_resolver;
    WTF_MAKE_NONCOPYABLE(ConnectCallbacks);
};

} // namespace

ScriptPromise NavigatorConnect::connect(ScriptState* scriptState, const String& url)
{
    WebNavigatorConnectProvider* provider = Platform::current()->navigatorConnectProvider();
    if (!provider)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));

    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    provider->connect(
        scriptState->executionContext()->completeURL(url),
        scriptState->executionContext()->securityOrigin()->toString(),
        new ConnectCallbacks(resolver));
    return promise;
}


} // namespace blink
