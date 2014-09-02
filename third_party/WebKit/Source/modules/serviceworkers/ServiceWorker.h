/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ServiceWorker_h
#define ServiceWorker_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "core/workers/AbstractWorker.h"
#include "public/platform/WebServiceWorker.h"
#include "public/platform/WebServiceWorkerProxy.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace blink {

class ScriptState;
class ScriptPromiseResolver;

class ServiceWorker FINAL : public AbstractWorker, public WebServiceWorkerProxy {
    DEFINE_WRAPPERTYPEINFO();
public:
    // For CallbackPromiseAdapter
    typedef WebServiceWorker WebType;
    static PassRefPtrWillBeRawPtr<ServiceWorker> take(ScriptPromiseResolver*, WebType* worker);

    static PassRefPtrWillBeRawPtr<ServiceWorker> from(ExecutionContext*, WebType*);
    static void dispose(WebType*);

    void postMessage(ExecutionContext*, PassRefPtr<SerializedScriptValue> message, const MessagePortArray*, ExceptionState&);
    void terminate(ExceptionState&);

    String scriptURL() const;
    const AtomicString& state() const;
    DEFINE_ATTRIBUTE_EVENT_LISTENER(statechange);

    // WebServiceWorkerProxy overrides.
    virtual bool isReady() OVERRIDE;
    virtual void dispatchStateChangeEvent() OVERRIDE;

    // AbstractWorker overrides.
    virtual const AtomicString& interfaceName() const OVERRIDE;

private:
    class ThenFunction;

    enum ProxyState {
        Initial,
        RegisterPromisePending,
        Ready,
        ContextStopped
    };

    static PassRefPtrWillBeRawPtr<ServiceWorker> getOrCreate(ExecutionContext*, WebType*);
    ServiceWorker(ExecutionContext*, PassOwnPtr<WebServiceWorker>);
    void setProxyState(ProxyState);
    void onPromiseResolved();
    void waitOnPromise(ScriptPromiseResolver*);

    // ActiveDOMObject overrides.
    virtual bool hasPendingActivity() const OVERRIDE;
    virtual void stop() OVERRIDE;

    OwnPtr<WebServiceWorker> m_outerWorker;
    ProxyState m_proxyState;
};

} // namespace blink

#endif // ServiceWorker_h
