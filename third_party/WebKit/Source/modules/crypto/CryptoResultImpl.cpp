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

#include "config.h"
#include "modules/crypto/CryptoResultImpl.h"

#include "bindings/v8/ScriptPromiseResolver.h"
#include "core/dom/ExecutionContext.h"
#include "modules/crypto/Key.h"
#include "modules/crypto/KeyPair.h"
#include "modules/crypto/NormalizeAlgorithm.h"
#include "public/platform/Platform.h"
#include "public/platform/WebArrayBuffer.h"
#include "public/platform/WebCryptoAlgorithm.h"
#include "wtf/ArrayBufferView.h"

namespace WebCore {

CryptoResultImpl::~CryptoResultImpl()
{
}

PassRefPtr<CryptoResultImpl> CryptoResultImpl::create()
{
    return adoptRef(new CryptoResultImpl(callingExecutionContext(v8::Isolate::GetCurrent())));
}

void CryptoResultImpl::completeWithError(const blink::WebString& errorDetails)
{
    ASSERT(!m_finished);

    if (canCompletePromise()) {
        DOMRequestState::Scope scope(m_requestState);
        if (!errorDetails.isEmpty()) {
            // FIXME: Include the line number which started the crypto operation.
            executionContext()->addConsoleMessage(JSMessageSource, ErrorMessageLevel, errorDetails);
        }
        m_promiseResolver->reject(ScriptValue::createNull());
    }
}

void CryptoResultImpl::completeWithError()
{
    completeWithError(blink::WebString());
}

void CryptoResultImpl::completeWithBuffer(const blink::WebArrayBuffer& buffer)
{
    ASSERT(!m_finished);

    if (canCompletePromise()) {
        DOMRequestState::Scope scope(m_requestState);
        m_promiseResolver->resolve(PassRefPtr<ArrayBuffer>(buffer));
    }

    finish();
}

void CryptoResultImpl::completeWithBoolean(bool b)
{
    ASSERT(!m_finished);

    if (canCompletePromise()) {
        DOMRequestState::Scope scope(m_requestState);
        m_promiseResolver->resolve(ScriptValue::createBoolean(b));
    }

    finish();
}

void CryptoResultImpl::completeWithKey(const blink::WebCryptoKey& key)
{
    ASSERT(!m_finished);

    if (canCompletePromise()) {
        DOMRequestState::Scope scope(m_requestState);
        m_promiseResolver->resolve(Key::create(key));
    }

    finish();
}

void CryptoResultImpl::completeWithKeyPair(const blink::WebCryptoKey& publicKey, const blink::WebCryptoKey& privateKey)
{
    ASSERT(!m_finished);

    if (canCompletePromise()) {
        DOMRequestState::Scope scope(m_requestState);
        m_promiseResolver->resolve(KeyPair::create(publicKey, privateKey));
    }

    finish();
}

CryptoResultImpl::CryptoResultImpl(ExecutionContext* context)
    : ContextLifecycleObserver(context)
    , m_promiseResolver(ScriptPromiseResolver::create(context))
    , m_requestState(toIsolate(context))
#if !ASSERT_DISABLED
    , m_owningThread(currentThread())
    , m_finished(false)
#endif
{
}

void CryptoResultImpl::finish()
{
#if !ASSERT_DISABLED
    m_finished = true;
#endif
    clearPromiseResolver();
}

void CryptoResultImpl::clearPromiseResolver()
{
    m_promiseResolver.clear();
    m_requestState.clear();
}

void CryptoResultImpl::CheckValidThread() const
{
    ASSERT(m_owningThread == currentThread());
}

void CryptoResultImpl::contextDestroyed()
{
    ContextLifecycleObserver::contextDestroyed();

    // Abandon the promise without completing it when the context goes away.
    clearPromiseResolver();
    ASSERT(!canCompletePromise());
}

bool CryptoResultImpl::canCompletePromise() const
{
    CheckValidThread();
    ExecutionContext* context = executionContext();
    return context && !context->activeDOMObjectsAreSuspended() && !context->activeDOMObjectsAreStopped();
}

} // namespace WebCore
