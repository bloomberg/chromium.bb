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

#include "bindings/v8/SerializedScriptValue.h"
#include "public/platform/WebServiceWorker.h"
#include "public/platform/WebServiceWorkerProxy.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace blink {
class WebServiceWorker;
}

namespace WebCore {

class NewScriptState;

class ServiceWorker
    : public RefCounted<ServiceWorker>
    , public blink::WebServiceWorkerProxy {
public:
    static PassRefPtr<ServiceWorker> create(PassOwnPtr<blink::WebServiceWorker> worker)
    {
        return adoptRef(new ServiceWorker(worker));
    }

    // For CallbackPromiseAdapter
    typedef blink::WebServiceWorker WebType;
    static PassRefPtr<ServiceWorker> from(NewScriptState*, WebType* worker)
    {
        return create(adoptPtr(worker));
    }

    virtual ~ServiceWorker() { }

    void postMessage(PassRefPtr<SerializedScriptValue> message, const MessagePortArray*, ExceptionState&);

    // WebServiceWorkerProxy overrides.
    virtual void dispatchStateChangeEvent() OVERRIDE;

private:
    explicit ServiceWorker(PassOwnPtr<blink::WebServiceWorker>);

    OwnPtr<blink::WebServiceWorker> m_outerWorker;
};

} // namespace WebCore

#endif // ServiceWorker_h
