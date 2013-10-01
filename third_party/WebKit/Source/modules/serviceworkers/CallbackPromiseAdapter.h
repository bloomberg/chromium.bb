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

#ifndef CallbackPromiseAdapter_h
#define CallbackPromiseAdapter_h

#include "bindings/v8/DOMRequestState.h"
#include "public/platform/WebCallbacks.h"

namespace WebCore {

// FIXME: this class can be generalized
class CallbackPromiseAdapter : public WebKit::WebCallbacks<WebKit::WebServiceWorker, WebKit::WebServiceWorker> {
public:
    explicit CallbackPromiseAdapter(PassRefPtr<ScriptPromiseResolver> resolver, ScriptExecutionContext* context)
        : m_resolver(resolver)
        , m_requestState(context)
    {
    }
    virtual ~CallbackPromiseAdapter() { }

    virtual void onSuccess(WebKit::WebServiceWorker* worker) OVERRIDE
    {
        // FIXME: When the same worker is "registered" twice, we should return the same object.
        DOMRequestState::Scope scope(m_requestState);
        m_resolver->resolve(ServiceWorker::create(adoptPtr(worker)));
    }
    void onError(WebKit::WebServiceWorker* worker) OVERRIDE
    {
        // FIXME: need to propagate some kind of reason for failure.
        DOMRequestState::Scope scope(m_requestState);
        m_resolver->reject(ServiceWorker::create(adoptPtr(worker)));
    }
private:
    RefPtr<ScriptPromiseResolver> m_resolver;
    DOMRequestState m_requestState;
};

} // namespace WebCore

#endif
