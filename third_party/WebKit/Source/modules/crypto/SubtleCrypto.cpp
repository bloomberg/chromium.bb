/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Google, Inc. ("Google") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/crypto/SubtleCrypto.h"

#include "bindings/v8/ExceptionState.h"
#include "modules/crypto/CryptoOperation.h"
#include "modules/crypto/NormalizeAlgorithm.h"
#include "public/platform/WebArrayBuffer.h" // FIXME: temporary
#include "public/platform/WebCrypto.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/ArrayBufferView.h"
#include "wtf/SHA1.h" // FIXME: temporary


namespace WebCore {

namespace {

// FIXME: The following are temporary implementations of what *should* go on the
//        embedder's side. Since SHA1 is easily implemented, this serves as
//        a useful proof of concept to get layout tests up and running and
//        returning correct results, until the embedder's side is implemented.
//------------------------------------------------------------------------------
class DummyOperation : public WebKit::WebCryptoOperation {
public:
    virtual void process(const unsigned char* bytes, size_t size) OVERRIDE
    {
        ASSERT_NOT_REACHED();
    }
    virtual void abort() OVERRIDE
    {
        delete this;
    }
    virtual void finish(WebKit::WebCryptoOperationResult* result) OVERRIDE
    {
        ASSERT_NOT_REACHED();
    }
};

class MockSha1Operation : public DummyOperation {
public:
    virtual void process(const unsigned char* bytes, size_t size) OVERRIDE
    {
        m_sha1.addBytes(bytes, size);
    }

    virtual void finish(WebKit::WebCryptoOperationResult* result) OVERRIDE
    {
        Vector<uint8_t, 20> hash;
        m_sha1.computeHash(hash);

        WebKit::WebArrayBuffer buffer = WebKit::WebArrayBuffer::create(hash.size(), 1);
        memcpy(buffer.data(), hash.data(), hash.size());

        result->setArrayBuffer(buffer);
        delete this;
    }

private:
    SHA1 m_sha1;
};
//------------------------------------------------------------------------------

} // namespace

SubtleCrypto::SubtleCrypto()
{
    ScriptWrappable::init(this);
}

PassRefPtr<CryptoOperation> SubtleCrypto::encrypt(const Dictionary& rawAlgorithm, ExceptionState& es)
{
    WebKit::WebCryptoAlgorithm algorithm;
    if (!normalizeAlgorithm(rawAlgorithm, Encrypt, algorithm, es))
        return 0;
    return CryptoOperation::create(algorithm, new DummyOperation);
}

PassRefPtr<CryptoOperation> SubtleCrypto::decrypt(const Dictionary& rawAlgorithm, ExceptionState& es)
{
    WebKit::WebCryptoAlgorithm algorithm;
    if (!normalizeAlgorithm(rawAlgorithm, Decrypt, algorithm, es))
        return 0;
    return CryptoOperation::create(algorithm, new DummyOperation);
}

PassRefPtr<CryptoOperation> SubtleCrypto::sign(const Dictionary& rawAlgorithm, ExceptionState& es)
{
    WebKit::WebCryptoAlgorithm algorithm;
    if (!normalizeAlgorithm(rawAlgorithm, Sign, algorithm, es))
        return 0;
    return CryptoOperation::create(algorithm, new DummyOperation);
}

PassRefPtr<CryptoOperation> SubtleCrypto::verifySignature(const Dictionary& rawAlgorithm, ExceptionState& es)
{
    WebKit::WebCryptoAlgorithm algorithm;
    if (!normalizeAlgorithm(rawAlgorithm, Verify, algorithm, es))
        return 0;
    return CryptoOperation::create(algorithm, new DummyOperation);
}

PassRefPtr<CryptoOperation> SubtleCrypto::digest(const Dictionary& rawAlgorithm, ExceptionState& es)
{
    WebKit::WebCryptoAlgorithm algorithm;
    if (!normalizeAlgorithm(rawAlgorithm, Digest, algorithm, es))
        return 0;

    // FIXME: Create the WebCryptoImplementation by calling out to
    // Platform::crypto() instead.
    WebKit::WebCryptoOperation* operationImpl = algorithm.id() == WebKit::WebCryptoAlgorithmIdSha1 ? new MockSha1Operation : new DummyOperation;

    return CryptoOperation::create(algorithm, operationImpl);
}

} // namespace WebCore
