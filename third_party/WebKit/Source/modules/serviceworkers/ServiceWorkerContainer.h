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

#ifndef ServiceWorkerContainer_h
#define ServiceWorkerContainer_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseProperty.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "modules/serviceworkers/ServiceWorker.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebServiceWorkerProviderClient.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace blink {

class Dictionary;
class ExecutionContext;
class ServiceWorker;
class WebServiceWorkerProvider;
class WebServiceWorker;

class ServiceWorkerContainer FINAL
    : public RefCountedWillBeGarbageCollectedFinalized<ServiceWorkerContainer>
    , public ScriptWrappable
    , public ContextLifecycleObserver
    , public WebServiceWorkerProviderClient {
public:
    static PassRefPtrWillBeRawPtr<ServiceWorkerContainer> create(ExecutionContext*);
    ~ServiceWorkerContainer();

    void willBeDetachedFromFrame();

    void trace(Visitor*);

    PassRefPtrWillBeRawPtr<ServiceWorker> active() { return m_active.get(); }
    PassRefPtrWillBeRawPtr<ServiceWorker> controller() { return m_controller.get(); }
    PassRefPtrWillBeRawPtr<ServiceWorker> installing() { return m_installing.get(); }
    PassRefPtrWillBeRawPtr<ServiceWorker> waiting() { return m_waiting.get(); }
    ScriptPromise ready(ScriptState*);
    WebServiceWorkerProvider* provider() { return m_provider; }

    ScriptPromise registerServiceWorker(ScriptState*, const String& pattern, const Dictionary&);
    ScriptPromise unregisterServiceWorker(ScriptState*, const String& scope);

    // WebServiceWorkerProviderClient overrides.
    virtual void setActive(WebServiceWorker*) OVERRIDE;
    virtual void setController(WebServiceWorker*) OVERRIDE;
    virtual void setInstalling(WebServiceWorker*) OVERRIDE;
    virtual void setWaiting(WebServiceWorker*) OVERRIDE;
    virtual void dispatchMessageEvent(const WebString& message, const WebMessagePortChannelArray&) OVERRIDE;

private:
    explicit ServiceWorkerContainer(ExecutionContext*);

    typedef ScriptPromiseProperty<RawPtrWillBeMember<ServiceWorkerContainer>, RefPtrWillBeMember<ServiceWorker>, RefPtrWillBeMember<ServiceWorker> > ReadyProperty;
    ReadyProperty* createReadyProperty();
    void checkReadyChanged(PassRefPtrWillBeRawPtr<ServiceWorker> previousReadyWorker);

    WebServiceWorkerProvider* m_provider;
    RefPtrWillBeMember<ServiceWorker> m_active;
    RefPtrWillBeMember<ServiceWorker> m_controller;
    RefPtrWillBeMember<ServiceWorker> m_installing;
    RefPtrWillBeMember<ServiceWorker> m_waiting;
    PersistentWillBeMember<ReadyProperty> m_ready;
};

} // namespace blink

#endif // ServiceWorkerContainer_h
