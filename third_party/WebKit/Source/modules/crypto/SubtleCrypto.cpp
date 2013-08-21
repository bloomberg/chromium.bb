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
#include "modules/crypto/SubtleCrypto.h"

#include "V8Key.h" // Must precede ScriptPromiseResolver.h
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/custom/V8ArrayBufferCustom.h" // Must precede ScriptPromiseResolver.h
#include "bindings/v8/ScriptPromiseResolver.h"
#include "core/dom/ExceptionCode.h"
#include "modules/crypto/Key.h"
#include "modules/crypto/NormalizeAlgorithm.h"
#include "public/platform/Platform.h"
#include "public/platform/WebArrayBuffer.h"
#include "public/platform/WebCrypto.h"
#include "public/platform/WebCryptoAlgorithm.h"
#include "wtf/ArrayBufferView.h"

namespace WebCore {

// FIXME: asynchronous completion of CryptoResult. Need to re-enter the
//        v8::Context before trying to fulfill the promise, and enable test.

namespace {

class CryptoResult : public WebKit::WebCryptoResultPrivate, public ThreadSafeRefCounted<CryptoResult> {
public:
    static PassRefPtr<CryptoResult> create()
    {
        return adoptRef(new CryptoResult);
    }

    virtual void ref() OVERRIDE
    {
        ThreadSafeRefCounted<CryptoResult>::ref();
    }

    virtual void deref() OVERRIDE
    {
        ThreadSafeRefCounted<CryptoResult>::deref();
    }

    virtual void completeWithError() OVERRIDE
    {
        m_promiseResolver->reject(ScriptValue::createNull());
        finish();
    }

    virtual void completeWithBuffer(const WebKit::WebArrayBuffer& buffer) OVERRIDE
    {
        m_promiseResolver->fulfill(PassRefPtr<ArrayBuffer>(buffer));
        finish();
    }

    virtual void completeWithBoolean(bool b) OVERRIDE
    {
        m_promiseResolver->fulfill(ScriptValue::createBoolean(b));
        finish();
    }

    virtual void completeWithKey(const WebKit::WebCryptoKey& key) OVERRIDE
    {
        m_promiseResolver->fulfill(Key::create(key));
        finish();
    }

    WebKit::WebCryptoResult result()
    {
        return WebKit::WebCryptoResult(this);
    }

    ScriptObject promise()
    {
        return m_promiseResolver->promise();
    }

private:
    CryptoResult()
        : m_promiseResolver(ScriptPromiseResolver::create())
        , m_finished(false) { }

    void finish()
    {
        ASSERT(!m_finished);
        m_finished = true;
    }

    RefPtr<ScriptPromiseResolver> m_promiseResolver;
    bool m_finished;
};

ScriptObject startCryptoOperation(const Dictionary& rawAlgorithm, Key* key, AlgorithmOperation operationType, ArrayBufferView* signature, ArrayBufferView* dataBuffer, ExceptionState& es)
{
    WebKit::WebCrypto* platformCrypto = WebKit::Platform::current()->crypto();
    if (!platformCrypto) {
        es.throwDOMException(NotSupportedError);
        return ScriptObject();
    }

    WebKit::WebCryptoAlgorithm algorithm;
    if (!normalizeAlgorithm(rawAlgorithm, operationType, algorithm, es))
        return ScriptObject();

    // All operations other than Digest require a valid Key.
    if (operationType != Digest) {
        if (!key) {
            es.throwTypeError();
            return ScriptObject();
        }

        if (!key->canBeUsedForAlgorithm(algorithm, operationType, es))
            return ScriptObject();
    }

    // Only Verify takes a signature.
    if (operationType == Verify && !signature) {
        es.throwTypeError();
        return ScriptObject();
    }

    if (!dataBuffer) {
        es.throwTypeError();
        return ScriptObject();
    }

    const unsigned char* data = static_cast<const unsigned char*>(dataBuffer->baseAddress());
    size_t dataSize = dataBuffer->byteLength();

    RefPtr<CryptoResult> result = CryptoResult::create();

    switch (operationType) {
    case Encrypt:
        platformCrypto->encrypt(algorithm, key->key(), data, dataSize, result->result());
        break;
    case Decrypt:
        platformCrypto->decrypt(algorithm, key->key(), data, dataSize, result->result());
        break;
    case Sign:
        platformCrypto->sign(algorithm, key->key(), data, dataSize, result->result());
        break;
    case Verify:
        platformCrypto->verifySignature(algorithm, key->key(), reinterpret_cast<const unsigned char*>(signature->baseAddress()), signature->byteLength(), data, dataSize, result->result());
        break;
    case Digest:
        platformCrypto->digest(algorithm, data, dataSize, result->result());
        break;
    default:
        ASSERT_NOT_REACHED();
        return ScriptObject();
    }

    return result->promise();
}

} // namespace

SubtleCrypto::SubtleCrypto()
{
    ScriptWrappable::init(this);
}

ScriptObject SubtleCrypto::encrypt(const Dictionary& rawAlgorithm, Key* key, ArrayBufferView* data, ExceptionState& es)
{
    return startCryptoOperation(rawAlgorithm, key, Encrypt, 0, data, es);
}

ScriptObject SubtleCrypto::decrypt(const Dictionary& rawAlgorithm, Key* key, ArrayBufferView* data, ExceptionState& es)
{
    return startCryptoOperation(rawAlgorithm, key, Decrypt, 0, data, es);
}

ScriptObject SubtleCrypto::sign(const Dictionary& rawAlgorithm, Key* key, ArrayBufferView* data, ExceptionState& es)
{
    return startCryptoOperation(rawAlgorithm, key, Sign, 0, data, es);
}

ScriptObject SubtleCrypto::verifySignature(const Dictionary& rawAlgorithm, Key* key, ArrayBufferView* signature, ArrayBufferView* data, ExceptionState& es)
{
    return startCryptoOperation(rawAlgorithm, key, Verify, signature, data, es);
}

ScriptObject SubtleCrypto::digest(const Dictionary& rawAlgorithm, ArrayBufferView* data, ExceptionState& es)
{
    return startCryptoOperation(rawAlgorithm, 0, Digest, 0, data, es);
}

ScriptObject SubtleCrypto::generateKey(const Dictionary& rawAlgorithm, bool extractable, const Vector<String>& rawKeyUsages, ExceptionState& es)
{
    WebKit::WebCrypto* platformCrypto = WebKit::Platform::current()->crypto();
    if (!platformCrypto) {
        es.throwDOMException(NotSupportedError);
        return ScriptObject();
    }

    WebKit::WebCryptoKeyUsageMask keyUsages;
    if (!Key::parseUsageMask(rawKeyUsages, keyUsages)) {
        es.throwTypeError();
        return ScriptObject();
    }

    WebKit::WebCryptoAlgorithm algorithm;
    if (!normalizeAlgorithm(rawAlgorithm, GenerateKey, algorithm, es))
        return ScriptObject();

    RefPtr<CryptoResult> result = CryptoResult::create();
    platformCrypto->generateKey(algorithm, extractable, keyUsages, result->result());
    return result->promise();
}

ScriptObject SubtleCrypto::importKey(const String& rawFormat, ArrayBufferView* keyData, const Dictionary& rawAlgorithm, bool extractable, const Vector<String>& rawKeyUsages, ExceptionState& es)
{
    WebKit::WebCrypto* platformCrypto = WebKit::Platform::current()->crypto();
    if (!platformCrypto) {
        es.throwDOMException(NotSupportedError);
        return ScriptObject();
    }

    if (!keyData) {
        es.throwTypeError();
        return ScriptObject();
    }

    WebKit::WebCryptoKeyUsageMask keyUsages;
    if (!Key::parseUsageMask(rawKeyUsages, keyUsages)) {
        es.throwTypeError();
        return ScriptObject();
    }

    WebKit::WebCryptoKeyFormat format;
    if (!Key::parseFormat(rawFormat, format)) {
        es.throwTypeError();
        return ScriptObject();
    }

    WebKit::WebCryptoAlgorithm algorithm;
    if (!normalizeAlgorithm(rawAlgorithm, ImportKey, algorithm, es))
        return ScriptObject();

    const unsigned char* keyDataBytes = static_cast<unsigned char*>(keyData->baseAddress());

    RefPtr<CryptoResult> result = CryptoResult::create();
    platformCrypto->importKey(format, keyDataBytes, keyData->byteLength(), algorithm, extractable, keyUsages, result->result());
    return result->promise();
}

} // namespace WebCore
