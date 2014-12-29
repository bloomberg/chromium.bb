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

class ConnectCallbacks : public WebNavigatorConnectCallbacks {
public:
    ConnectCallbacks(PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver, PassRefPtrWillBeRawPtr<MessagePort> port)
        : m_resolver(resolver), m_port(port)
    {
        ASSERT(m_resolver);
    }
    ~ConnectCallbacks() override { }

    void onSuccess() override
    {
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
            return;
        }
        m_resolver->resolve(m_port);
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
    RefPtrWillBePersistent<MessagePort> m_port;
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
    // Create a new MessageChannel, but immediately disentangle port2 (extract
    // the WebMessagePortChannel from the port and mark it as being transfered).
    // This way we have:
    // - Ownership of port1 is kept in the ConnectCallbacks and later passed to
    //     javascript (or released if the connection is not accepted).
    // - Ownership of port2 (or more precisely the WebMessagePortChannel backing
    //     it) is passed to the content layer to be transfered to the service
    //     provider.
    // - Ownership of the MessageChannel object itself will be released at the
    //    end of this method. This is okay as it's the ports themselves that
    //    represent the actual message channel. MessageChannel is only used to
    //    create a pair of ports.
    RefPtrWillBeRawPtr<MessageChannel> channel(MessageChannel::create(scriptState->executionContext()));
    OwnPtr<WebMessagePortChannel> webchannel = channel->port2()->disentangle();
    provider->connect(
        scriptState->executionContext()->completeURL(url),
        scriptState->executionContext()->securityOrigin()->toString(),
        webchannel.leakPtr(),
        new ConnectCallbacks(resolver, channel->port1()));
    return promise;
}


} // namespace blink
