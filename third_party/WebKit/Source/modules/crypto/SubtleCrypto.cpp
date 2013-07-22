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

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/crypto/CryptoOperation.h"
#include "modules/crypto/NormalizeAlgorithm.h"
#include "public/platform/Platform.h"
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
    explicit DummyOperation(WebKit::WebCryptoOperationResult* result) : m_result(result) { }

    virtual void process(const unsigned char* bytes, size_t size) OVERRIDE
    {
        m_result->completeWithError();
        delete this;
    }

    virtual void abort() OVERRIDE
    {
        delete this;
    }

    virtual void finish() OVERRIDE
    {
        m_result->completeWithError();
        delete this;
    }

protected:
    WebKit::WebCryptoOperationResult* m_result;
};

class MockSha1Operation : public DummyOperation {
public:
    explicit MockSha1Operation(WebKit::WebCryptoOperationResult* result) : DummyOperation(result) { }

    virtual void process(const unsigned char* bytes, size_t size) OVERRIDE
    {
        m_sha1.addBytes(bytes, size);
    }

    virtual void finish() OVERRIDE
    {
        Vector<uint8_t, 20> hash;
        m_sha1.computeHash(hash);

        WebKit::WebArrayBuffer buffer = WebKit::WebArrayBuffer::create(hash.size(), 1);
        memcpy(buffer.data(), hash.data(), hash.size());

        m_result->completeWithArrayBuffer(buffer);
        delete this;
    }

private:
    SHA1 m_sha1;
};

class MockPlatformCrypto : public WebKit::WebCrypto {
public:
    virtual void digest(const WebKit::WebCryptoAlgorithm& algorithm, WebKit::WebCryptoOperationResult* result) OVERRIDE
    {
        if (algorithm.id() == WebKit::WebCryptoAlgorithmIdSha1) {
            result->initializationSucceded(new MockSha1Operation(result));
        } else {
            // Don't fail synchronously, since existing layout tests rely on
            // digest for testing algorithm normalization.
            result->initializationSucceded(new DummyOperation(result));
        }
    }
};

WebKit::WebCrypto* mockPlatformCrypto()
{
    DEFINE_STATIC_LOCAL(MockPlatformCrypto, crypto, ());
    return &crypto;
}

PassRefPtr<CryptoOperation> doDummyOperation(const Dictionary& rawAlgorithm, AlgorithmOperation operationType, ExceptionState& es)
{
    WebKit::WebCryptoAlgorithm algorithm;
    if (!normalizeAlgorithm(rawAlgorithm, operationType, algorithm, es))
        return 0;

    RefPtr<CryptoOperation> op = CryptoOperation::create(algorithm, &es);
    op->initializationSucceded(new DummyOperation(op.get()));
    return op.release();
}
//------------------------------------------------------------------------------

} // namespace

SubtleCrypto::SubtleCrypto()
{
    ScriptWrappable::init(this);
}

PassRefPtr<CryptoOperation> SubtleCrypto::encrypt(const Dictionary& rawAlgorithm, ExceptionState& es)
{
    return doDummyOperation(rawAlgorithm, Encrypt, es);
}

PassRefPtr<CryptoOperation> SubtleCrypto::decrypt(const Dictionary& rawAlgorithm, ExceptionState& es)
{
    return doDummyOperation(rawAlgorithm, Decrypt, es);
}

PassRefPtr<CryptoOperation> SubtleCrypto::sign(const Dictionary& rawAlgorithm, ExceptionState& es)
{
    return doDummyOperation(rawAlgorithm, Sign, es);
}

PassRefPtr<CryptoOperation> SubtleCrypto::verifySignature(const Dictionary& rawAlgorithm, ExceptionState& es)
{
    return doDummyOperation(rawAlgorithm, Verify, es);
}

PassRefPtr<CryptoOperation> SubtleCrypto::digest(const Dictionary& rawAlgorithm, ExceptionState& es)
{
    WebKit::WebCrypto* platformCrypto = mockPlatformCrypto();
    if (!platformCrypto) {
        es.throwDOMException(NotSupportedError);
        return 0;
    }

    WebKit::WebCryptoAlgorithm algorithm;
    if (!normalizeAlgorithm(rawAlgorithm, Digest, algorithm, es))
        return 0;

    RefPtr<CryptoOperation> op = CryptoOperation::create(algorithm, &es);
    platformCrypto->digest(algorithm, op.get());
    return op.release();
}

} // namespace WebCore
