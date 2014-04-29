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

#include "bindings/v8/ScriptPromiseResolverWithContext.h"
#include "bindings/v8/ScriptState.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/DOMError.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExecutionContext.h"
#include "modules/crypto/Key.h"
#include "modules/crypto/KeyPair.h"
#include "modules/crypto/NormalizeAlgorithm.h"
#include "public/platform/Platform.h"
#include "public/platform/WebArrayBuffer.h"
#include "public/platform/WebCryptoAlgorithm.h"
#include "wtf/ArrayBufferView.h"

namespace WebCore {

namespace {

ExceptionCode toExceptionCode(blink::WebCryptoErrorType errorType)
{
    switch (errorType) {
    case blink::WebCryptoErrorTypeNotSupported:
        return NotSupportedError;
    case blink::WebCryptoErrorTypeSyntax:
        return SyntaxError;
    case blink::WebCryptoErrorTypeInvalidState:
        return InvalidStateError;
    case blink::WebCryptoErrorTypeInvalidAccess:
        return InvalidAccessError;
    case blink::WebCryptoErrorTypeUnknown:
        return UnknownError;
    case blink::WebCryptoErrorTypeData:
        return DataError;
    case blink::WebCryptoErrorTypeOperation:
        // FIXME: This exception type is new to WebCrypto and not yet defined.
        // Use a placeholder for now.
        return InvalidStateError;
    case blink::WebCryptoErrorTypeType:
        // FIXME: This should construct a TypeError instead. For now do
        //        something to facilitate refactor, but this will need to be
        //        revisited.
        return DataError;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

} // namespace

// The PromiseState class contains all the state which is tied to an
// ExecutionContext. Whereas CryptoResultImpl can be deleted from any thread,
// PromiseState is not thread safe and must only be accessed and deleted from
// the blink thread.
//
// This is achieved by making CryptoResultImpl hold a WeakPtr to the PromiseState.
// The PromiseState deletes itself after being notified of completion.
// Additionally the PromiseState deletes itself when the ExecutionContext is
// destroyed (necessary to avoid leaks when dealing with WebWorker threads,
// which may die before the operation is completed).
class CryptoResultImpl::PromiseState FINAL : public ContextLifecycleObserver {
public:
    static WeakPtr<PromiseState> create(ExecutionContext* context)
    {
        PromiseState* promiseState = new PromiseState(context);
        return promiseState->m_weakFactory.createWeakPtr();
    }

    // Override from ContextLifecycleObserver
    virtual void contextDestroyed() OVERRIDE
    {
        ContextLifecycleObserver::contextDestroyed();
        delete this;
    }

    ScriptPromise promise()
    {
        return m_promiseResolver->promise();
    }

    void completeWithError(blink::WebCryptoErrorType errorType, const blink::WebString& errorDetails)
    {
        m_promiseResolver->reject(DOMException::create(toExceptionCode(errorType), errorDetails));
        delete this;
    }

    void completeWithBuffer(const blink::WebArrayBuffer& buffer)
    {
        m_promiseResolver->resolve(PassRefPtr<ArrayBuffer>(buffer));
        delete this;
    }

    void completeWithBoolean(bool b)
    {
        m_promiseResolver->resolve(b);
        delete this;
    }

    void completeWithKey(const blink::WebCryptoKey& key)
    {
        m_promiseResolver->resolve(Key::create(key));
        delete this;
    }

    void completeWithKeyPair(const blink::WebCryptoKey& publicKey, const blink::WebCryptoKey& privateKey)
    {
        m_promiseResolver->resolve(KeyPair::create(publicKey, privateKey));
        delete this;
    }

private:
    explicit PromiseState(ExecutionContext* context)
        : ContextLifecycleObserver(context)
        , m_weakFactory(this)
        , m_promiseResolver(ScriptPromiseResolverWithContext::create(ScriptState::current(toIsolate(context))))
    {
    }

    WeakPtrFactory<PromiseState> m_weakFactory;
    RefPtr<ScriptPromiseResolverWithContext> m_promiseResolver;
};

CryptoResultImpl::~CryptoResultImpl()
{
}

PassRefPtr<CryptoResultImpl> CryptoResultImpl::create()
{
    return adoptRef(new CryptoResultImpl(callingExecutionContext(v8::Isolate::GetCurrent())));
}

void CryptoResultImpl::completeWithError(blink::WebCryptoErrorType errorType, const blink::WebString& errorDetails)
{
    if (m_promiseState)
        m_promiseState->completeWithError(errorType, errorDetails);
}

void CryptoResultImpl::completeWithBuffer(const blink::WebArrayBuffer& buffer)
{
    if (m_promiseState)
        m_promiseState->completeWithBuffer(buffer);
}

void CryptoResultImpl::completeWithBoolean(bool b)
{
    if (m_promiseState)
        m_promiseState->completeWithBoolean(b);
}

void CryptoResultImpl::completeWithKey(const blink::WebCryptoKey& key)
{
    if (m_promiseState)
        m_promiseState->completeWithKey(key);
}

void CryptoResultImpl::completeWithKeyPair(const blink::WebCryptoKey& publicKey, const blink::WebCryptoKey& privateKey)
{
    if (m_promiseState)
        m_promiseState->completeWithKeyPair(publicKey, privateKey);
}

CryptoResultImpl::CryptoResultImpl(ExecutionContext* context)
    : m_promiseState(PromiseState::create(context))
{
}

ScriptPromise CryptoResultImpl::promise()
{
    return m_promiseState->promise();
}

} // namespace WebCore
