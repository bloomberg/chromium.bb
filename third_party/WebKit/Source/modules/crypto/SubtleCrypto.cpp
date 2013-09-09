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
#include "modules/crypto/CryptoResult.h"
#include "modules/crypto/Key.h"
#include "modules/crypto/NormalizeAlgorithm.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCrypto.h"
#include "public/platform/WebCryptoAlgorithm.h"
#include "wtf/ArrayBufferView.h"

namespace WebCore {

// FIXME: asynchronous completion of CryptoResult. Need to re-enter the
//        v8::Context before trying to fulfill the promise, and enable test.

namespace {

ScriptPromise startCryptoOperation(const Dictionary& rawAlgorithm, Key* key, AlgorithmOperation operationType, ArrayBufferView* signature, ArrayBufferView* dataBuffer, ExceptionState& es)
{
    bool requiresKey = operationType != Digest;

    // Seems like the generated bindings should take care of these however it
    // currently doesn't. See also http://crbugh.com/264520
    if (requiresKey && !key) {
        es.throwTypeError("Invalid key argument");
        return ScriptPromise();
    }
    if (operationType == Verify && !signature) {
        es.throwTypeError("Invalid signature argument");
        return ScriptPromise();
    }
    if (!dataBuffer) {
        es.throwTypeError("Invalid dataBuffer argument");
        return ScriptPromise();
    }

    WebKit::WebCryptoAlgorithm algorithm;
    if (!normalizeAlgorithm(rawAlgorithm, operationType, algorithm, es))
        return ScriptPromise();

    if (requiresKey && !key->canBeUsedForAlgorithm(algorithm, operationType, es))
        return ScriptPromise();

    const unsigned char* data = static_cast<const unsigned char*>(dataBuffer->baseAddress());
    size_t dataSize = dataBuffer->byteLength();

    RefPtr<CryptoResult> result = CryptoResult::create();

    switch (operationType) {
    case Encrypt:
        WebKit::Platform::current()->crypto()->encrypt(algorithm, key->key(), data, dataSize, result->result());
        break;
    case Decrypt:
        WebKit::Platform::current()->crypto()->decrypt(algorithm, key->key(), data, dataSize, result->result());
        break;
    case Sign:
        WebKit::Platform::current()->crypto()->sign(algorithm, key->key(), data, dataSize, result->result());
        break;
    case Verify:
        WebKit::Platform::current()->crypto()->verifySignature(algorithm, key->key(), reinterpret_cast<const unsigned char*>(signature->baseAddress()), signature->byteLength(), data, dataSize, result->result());
        break;
    case Digest:
        WebKit::Platform::current()->crypto()->digest(algorithm, data, dataSize, result->result());
        break;
    default:
        ASSERT_NOT_REACHED();
        return ScriptPromise();
    }

    return result->promise();
}

} // namespace

SubtleCrypto::SubtleCrypto()
{
    ScriptWrappable::init(this);
}

ScriptPromise SubtleCrypto::encrypt(const Dictionary& rawAlgorithm, Key* key, ArrayBufferView* data, ExceptionState& es)
{
    return startCryptoOperation(rawAlgorithm, key, Encrypt, 0, data, es);
}

ScriptPromise SubtleCrypto::decrypt(const Dictionary& rawAlgorithm, Key* key, ArrayBufferView* data, ExceptionState& es)
{
    return startCryptoOperation(rawAlgorithm, key, Decrypt, 0, data, es);
}

ScriptPromise SubtleCrypto::sign(const Dictionary& rawAlgorithm, Key* key, ArrayBufferView* data, ExceptionState& es)
{
    return startCryptoOperation(rawAlgorithm, key, Sign, 0, data, es);
}

ScriptPromise SubtleCrypto::verifySignature(const Dictionary& rawAlgorithm, Key* key, ArrayBufferView* signature, ArrayBufferView* data, ExceptionState& es)
{
    return startCryptoOperation(rawAlgorithm, key, Verify, signature, data, es);
}

ScriptPromise SubtleCrypto::digest(const Dictionary& rawAlgorithm, ArrayBufferView* data, ExceptionState& es)
{
    return startCryptoOperation(rawAlgorithm, 0, Digest, 0, data, es);
}

ScriptPromise SubtleCrypto::generateKey(const Dictionary& rawAlgorithm, bool extractable, const Vector<String>& rawKeyUsages, ExceptionState& es)
{
    WebKit::WebCryptoKeyUsageMask keyUsages;
    if (!Key::parseUsageMask(rawKeyUsages, keyUsages, es))
        return ScriptPromise();

    WebKit::WebCryptoAlgorithm algorithm;
    if (!normalizeAlgorithm(rawAlgorithm, GenerateKey, algorithm, es))
        return ScriptPromise();

    RefPtr<CryptoResult> result = CryptoResult::create();
    WebKit::Platform::current()->crypto()->generateKey(algorithm, extractable, keyUsages, result->result());
    return result->promise();
}

ScriptPromise SubtleCrypto::importKey(const String& rawFormat, ArrayBufferView* keyData, const Dictionary& rawAlgorithm, bool extractable, const Vector<String>& rawKeyUsages, ExceptionState& es)
{
    WebKit::WebCryptoKeyFormat format;
    if (!Key::parseFormat(rawFormat, format, es))
        return ScriptPromise();

    if (!keyData) {
        es.throwTypeError("Invalid keyData argument");
        return ScriptPromise();
    }

    WebKit::WebCryptoKeyUsageMask keyUsages;
    if (!Key::parseUsageMask(rawKeyUsages, keyUsages, es))
        return ScriptPromise();

    WebKit::WebCryptoAlgorithm algorithm;
    if (!normalizeAlgorithm(rawAlgorithm, ImportKey, algorithm, es))
        return ScriptPromise();

    const unsigned char* keyDataBytes = static_cast<unsigned char*>(keyData->baseAddress());

    RefPtr<CryptoResult> result = CryptoResult::create();
    WebKit::Platform::current()->crypto()->importKey(format, keyDataBytes, keyData->byteLength(), algorithm, extractable, keyUsages, result->result());
    return result->promise();
}

ScriptPromise SubtleCrypto::exportKey(const String& rawFormat, Key* key, ExceptionState& es)
{
    WebKit::WebCryptoKeyFormat format;
    if (!Key::parseFormat(rawFormat, format, es))
        return ScriptPromise();

    if (!key) {
        es.throwTypeError("Invalid key argument");
        return ScriptPromise();
    }

    if (!key->extractable()) {
        es.throwDOMException(NotSupportedError, "key is not extractable");
        return ScriptPromise();
    }

    RefPtr<CryptoResult> result = CryptoResult::create();
    WebKit::Platform::current()->crypto()->exportKey(format, key->key(), result->result());
    return result->promise();
}

} // namespace WebCore
