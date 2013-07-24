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
#include "modules/crypto/KeyOperation.h"

#include "V8Key.h" // NOTE: This must appear before ScriptPromiseResolver to define toV8()
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "core/dom/ExceptionCode.h"
#include "modules/crypto/Key.h"

namespace WebCore {

PassRefPtr<KeyOperation> KeyOperation::create()
{
    return adoptRef(new KeyOperation);
}

KeyOperation::KeyOperation()
    : m_state(Initializing)
    , m_impl(0)
    , m_initializationError(0)
{
}

KeyOperation::~KeyOperation()
{
    ASSERT(!m_impl);
}

void KeyOperation::initializationFailed()
{
    ASSERT(m_state == Initializing);
    ASSERT(!m_impl);

    m_initializationError = NotSupportedError;
    m_state = Done;
}

void KeyOperation::initializationSucceeded(WebKit::WebCryptoKeyOperation* operationImpl)
{
    ASSERT(m_state == Initializing);
    ASSERT(operationImpl);
    ASSERT(!m_impl);

    m_impl = operationImpl;
    m_state = InProgress;
}

void KeyOperation::completeWithError()
{
    ASSERT(m_state == Initializing || m_state == InProgress);

    m_impl = 0;
    m_state = Done;

    promiseResolver()->reject(ScriptValue::createNull());
}

void KeyOperation::completeWithKey(const WebKit::WebCryptoKey& key)
{
    ASSERT(m_state == Initializing || m_state == InProgress);

    m_impl = 0;
    m_state = Done;

    promiseResolver()->fulfill(Key::create(key));
}

void KeyOperation::ref()
{
    ThreadSafeRefCounted<KeyOperation>::ref();
}

void KeyOperation::deref()
{
    ThreadSafeRefCounted<KeyOperation>::deref();
}

ScriptObject KeyOperation::returnValue(ExceptionState& es)
{
    ASSERT(m_state != Initializing);

    if (m_initializationError) {
        es.throwDOMException(m_initializationError);
        return ScriptObject();
    }

    return promiseResolver()->promise();
}

ScriptPromiseResolver* KeyOperation::promiseResolver()
{
    if (!m_promiseResolver)
        m_promiseResolver = ScriptPromiseResolver::create();
    return m_promiseResolver.get();
}

} // namespace WebCore
