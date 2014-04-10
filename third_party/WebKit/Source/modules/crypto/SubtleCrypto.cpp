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

#include "bindings/v8/Dictionary.h"
#include "modules/crypto/CryptoResultImpl.h"
#include "modules/crypto/Key.h"
#include "modules/crypto/NormalizeAlgorithm.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCrypto.h"
#include "public/platform/WebCryptoAlgorithm.h"
#include "wtf/ArrayBufferView.h"

namespace WebCore {

namespace {

// Seems like the generated bindings should take care of these however it
// currently doesn't. See also http://crbug.com/264520
template <typename T>
bool ensureNotNull(T* x, const char* paramName, CryptoResult* result)
{
    if (!x) {
        String message = String("Invalid ") + paramName + String(" argument");
        result->completeWithError(blink::WebString(message));
        return false;
    }
    return true;
}

ScriptPromise startCryptoOperation(const Dictionary& rawAlgorithm, Key* key, AlgorithmOperation operationType, ArrayBufferView* signature, ArrayBufferView* dataBuffer)
{
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create();
    ScriptPromise promise = result->promise();

    bool requiresKey = operationType != Digest;

    if (requiresKey && !ensureNotNull(key, "key", result.get()))
        return promise;
    if (operationType == Verify && !ensureNotNull(signature, "signature", result.get()))
        return promise;
    if (!ensureNotNull(dataBuffer, "dataBuffer", result.get()))
        return promise;

    blink::WebCryptoAlgorithm algorithm;
    if (!parseAlgorithm(rawAlgorithm, operationType, algorithm, result.get()))
        return promise;

    if (requiresKey && !key->canBeUsedForAlgorithm(algorithm, operationType, result.get()))
        return promise;

    const unsigned char* data = static_cast<const unsigned char*>(dataBuffer->baseAddress());
    unsigned dataSize = dataBuffer->byteLength();

    switch (operationType) {
    case Encrypt:
        blink::Platform::current()->crypto()->encrypt(algorithm, key->key(), data, dataSize, result->result());
        break;
    case Decrypt:
        blink::Platform::current()->crypto()->decrypt(algorithm, key->key(), data, dataSize, result->result());
        break;
    case Sign:
        blink::Platform::current()->crypto()->sign(algorithm, key->key(), data, dataSize, result->result());
        break;
    case Verify:
        blink::Platform::current()->crypto()->verifySignature(algorithm, key->key(), reinterpret_cast<const unsigned char*>(signature->baseAddress()), signature->byteLength(), data, dataSize, result->result());
        break;
    case Digest:
        blink::Platform::current()->crypto()->digest(algorithm, data, dataSize, result->result());
        break;
    default:
        ASSERT_NOT_REACHED();
        return ScriptPromise();
    }

    return promise;
}

} // namespace

SubtleCrypto::SubtleCrypto()
{
    ScriptWrappable::init(this);
}

ScriptPromise SubtleCrypto::encrypt(const Dictionary& rawAlgorithm, Key* key, ArrayBufferView* data)
{
    return startCryptoOperation(rawAlgorithm, key, Encrypt, 0, data);
}

ScriptPromise SubtleCrypto::decrypt(const Dictionary& rawAlgorithm, Key* key, ArrayBufferView* data)
{
    return startCryptoOperation(rawAlgorithm, key, Decrypt, 0, data);
}

ScriptPromise SubtleCrypto::sign(const Dictionary& rawAlgorithm, Key* key, ArrayBufferView* data)
{
    return startCryptoOperation(rawAlgorithm, key, Sign, 0, data);
}

ScriptPromise SubtleCrypto::verifySignature(const Dictionary& rawAlgorithm, Key* key, ArrayBufferView* signature, ArrayBufferView* data)
{
    return startCryptoOperation(rawAlgorithm, key, Verify, signature, data);
}

ScriptPromise SubtleCrypto::digest(const Dictionary& rawAlgorithm, ArrayBufferView* data)
{
    return startCryptoOperation(rawAlgorithm, 0, Digest, 0, data);
}

ScriptPromise SubtleCrypto::generateKey(const Dictionary& rawAlgorithm, bool extractable, const Vector<String>& rawKeyUsages)
{
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create();
    ScriptPromise promise = result->promise();

    blink::WebCryptoKeyUsageMask keyUsages;
    if (!Key::parseUsageMask(rawKeyUsages, keyUsages, result.get()))
        return promise;

    blink::WebCryptoAlgorithm algorithm;
    if (!parseAlgorithm(rawAlgorithm, GenerateKey, algorithm, result.get()))
        return promise;

    blink::Platform::current()->crypto()->generateKey(algorithm, extractable, keyUsages, result->result());
    return promise;
}

ScriptPromise SubtleCrypto::importKey(const String& rawFormat, ArrayBufferView* keyData, const Dictionary& rawAlgorithm, bool extractable, const Vector<String>& rawKeyUsages)
{
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create();
    ScriptPromise promise = result->promise();

    if (!ensureNotNull(keyData, "keyData", result.get()))
        return promise;

    blink::WebCryptoKeyFormat format;
    if (!Key::parseFormat(rawFormat, format, result.get()))
        return promise;

    blink::WebCryptoKeyUsageMask keyUsages;
    if (!Key::parseUsageMask(rawKeyUsages, keyUsages, result.get()))
        return promise;

    blink::WebCryptoAlgorithm algorithm;
    if (!parseAlgorithm(rawAlgorithm, ImportKey, algorithm, result.get()))
        return promise;

    const unsigned char* keyDataBytes = static_cast<unsigned char*>(keyData->baseAddress());

    blink::Platform::current()->crypto()->importKey(format, keyDataBytes, keyData->byteLength(), algorithm, extractable, keyUsages, result->result());
    return promise;
}

ScriptPromise SubtleCrypto::exportKey(const String& rawFormat, Key* key)
{
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create();
    ScriptPromise promise = result->promise();

    if (!ensureNotNull(key, "key", result.get()))
        return promise;

    blink::WebCryptoKeyFormat format;
    if (!Key::parseFormat(rawFormat, format, result.get()))
        return promise;

    if (!key->extractable()) {
        result->completeWithError("key is not extractable");
        return promise;
    }

    blink::Platform::current()->crypto()->exportKey(format, key->key(), result->result());
    return promise;
}

ScriptPromise SubtleCrypto::wrapKey(const String& rawFormat, Key* key, Key* wrappingKey, const Dictionary& rawWrapAlgorithm)
{
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create();
    ScriptPromise promise = result->promise();

    if (!ensureNotNull(key, "key", result.get()))
        return promise;

    if (!ensureNotNull(wrappingKey, "wrappingKey", result.get()))
        return promise;

    blink::WebCryptoKeyFormat format;
    if (!Key::parseFormat(rawFormat, format, result.get()))
        return promise;

    blink::WebCryptoAlgorithm wrapAlgorithm;
    if (!parseAlgorithm(rawWrapAlgorithm, WrapKey, wrapAlgorithm, result.get()))
        return promise;

    if (!key->extractable()) {
        result->completeWithError("key is not extractable");
        return promise;
    }

    if (!wrappingKey->canBeUsedForAlgorithm(wrapAlgorithm, WrapKey, result.get()))
        return promise;

    blink::Platform::current()->crypto()->wrapKey(format, key->key(), wrappingKey->key(), wrapAlgorithm, result->result());
    return promise;
}

ScriptPromise SubtleCrypto::unwrapKey(const String& rawFormat, ArrayBufferView* wrappedKey, Key* unwrappingKey, const Dictionary& rawUnwrapAlgorithm, const Dictionary& rawUnwrappedKeyAlgorithm, bool extractable, const Vector<String>& rawKeyUsages)
{
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create();
    ScriptPromise promise = result->promise();

    if (!ensureNotNull(wrappedKey, "wrappedKey", result.get()))
        return promise;
    if (!ensureNotNull(unwrappingKey, "unwrappingKey", result.get()))
        return promise;

    blink::WebCryptoKeyFormat format;
    if (!Key::parseFormat(rawFormat, format, result.get()))
        return promise;

    blink::WebCryptoKeyUsageMask keyUsages;
    if (!Key::parseUsageMask(rawKeyUsages, keyUsages, result.get()))
        return promise;

    blink::WebCryptoAlgorithm unwrapAlgorithm;
    if (!parseAlgorithm(rawUnwrapAlgorithm, UnwrapKey, unwrapAlgorithm, result.get()))
        return promise;

    blink::WebCryptoAlgorithm unwrappedKeyAlgorithm;
    if (!parseAlgorithm(rawUnwrappedKeyAlgorithm, ImportKey, unwrappedKeyAlgorithm, result.get()))
        return promise;

    if (!unwrappingKey->canBeUsedForAlgorithm(unwrapAlgorithm, UnwrapKey, result.get()))
        return promise;

    const unsigned char* wrappedKeyData = static_cast<const unsigned char*>(wrappedKey->baseAddress());
    unsigned wrappedKeyDataSize = wrappedKey->byteLength();

    blink::Platform::current()->crypto()->unwrapKey(format, wrappedKeyData, wrappedKeyDataSize, unwrappingKey->key(), unwrapAlgorithm, unwrappedKeyAlgorithm, extractable, keyUsages, result->result());
    return promise;
}

} // namespace WebCore
