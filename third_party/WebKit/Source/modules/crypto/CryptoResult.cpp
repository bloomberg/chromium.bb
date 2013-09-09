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
#include "modules/crypto/CryptoResult.h"

#include "V8Key.h" // Must precede ScriptPromiseResolver.h
#include "V8KeyPair.h" // Must precede ScriptPromiseResolver.h
#include "bindings/v8/custom/V8ArrayBufferCustom.h" // Must precede ScriptPromiseResolver.h
#include "bindings/v8/ScriptPromiseResolver.h"
#include "modules/crypto/Key.h"
#include "modules/crypto/NormalizeAlgorithm.h"
#include "public/platform/Platform.h"
#include "public/platform/WebArrayBuffer.h"
#include "public/platform/WebCrypto.h"
#include "public/platform/WebCryptoAlgorithm.h"
#include "wtf/ArrayBufferView.h"

namespace WebCore {

CryptoResult::~CryptoResult()
{
    ASSERT(m_finished);
}

PassRefPtr<CryptoResult> CryptoResult::create()
{
    return adoptRef(new CryptoResult);
}

void CryptoResult::completeWithError()
{
    m_promiseResolver->reject(ScriptValue::createNull());
    finish();
}

void CryptoResult::completeWithBuffer(const WebKit::WebArrayBuffer& buffer)
{
    m_promiseResolver->fulfill(PassRefPtr<ArrayBuffer>(buffer));
    finish();
}

void CryptoResult::completeWithBoolean(bool b)
{
    m_promiseResolver->fulfill(ScriptValue::createBoolean(b));
    finish();
}

void CryptoResult::completeWithKey(const WebKit::WebCryptoKey& key)
{
    m_promiseResolver->fulfill(Key::create(key));
    finish();
}

void CryptoResult::completeWithKeyPair(const WebKit::WebCryptoKey& publicKey, const WebKit::WebCryptoKey& privateKey)
{
    m_promiseResolver->fulfill(KeyPair::create(publicKey, privateKey));
    finish();
}

ScriptPromise CryptoResult::promise()
{
    return m_promiseResolver->promise();
}

CryptoResult::CryptoResult()
    : m_promiseResolver(ScriptPromiseResolver::create())
    , m_finished(false) { }

void CryptoResult::finish()
{
    ASSERT(!m_finished);
    m_finished = true;
}

} // namespace WebCore
