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
#include "bindings/v8/ExceptionState.h"
#include "modules/crypto/CryptoResultImpl.h"
#include "modules/crypto/Key.h"
#include "modules/crypto/NormalizeAlgorithm.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCrypto.h"
#include "public/platform/WebCryptoAlgorithm.h"
#include "wtf/ArrayBufferView.h"

namespace WebCore {

namespace {

bool parseAlgorithm(const Dictionary& rawAlgorithm, AlgorithmOperation operationType, blink::WebCryptoAlgorithm &algorithm, ExceptionState& exceptionState, CryptoResult* result)
{
    if (!rawAlgorithm.isObject()) {
        exceptionState.throwTypeError("Algorithm: Not an object");
        return false;
    }
    return parseAlgorithm(rawAlgorithm, operationType, algorithm, result);
}

ScriptPromise startCryptoOperation(const Dictionary& rawAlgorithm, Key* key, AlgorithmOperation operationType, ArrayBufferView* signature, ArrayBufferView* dataBuffer, ExceptionState& exceptionState)
{
    bool requiresKey = operationType != Digest;

    // Seems like the generated bindings should take care of these however it
    // currently doesn't. See also http://crbugh.com/264520
    if (requiresKey && !key) {
        exceptionState.throwTypeError("Invalid key argument");
        return ScriptPromise();
    }
    if (operationType == Verify && !signature) {
        exceptionState.throwTypeError("Invalid signature argument");
        return ScriptPromise();
    }
    if (!dataBuffer) {
        exceptionState.throwTypeError("Invalid dataBuffer argument");
        return ScriptPromise();
    }

    ScriptPromise promise = ScriptPromise::createPending();
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create(promise);

    blink::WebCryptoAlgorithm algorithm;
    if (!parseAlgorithm(rawAlgorithm, operationType, algorithm, exceptionState, result.get()))
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

ScriptPromise SubtleCrypto::encrypt(const Dictionary& rawAlgorithm, Key* key, ArrayBufferView* data, ExceptionState& exceptionState)
{
    return startCryptoOperation(rawAlgorithm, key, Encrypt, 0, data, exceptionState);
}

ScriptPromise SubtleCrypto::decrypt(const Dictionary& rawAlgorithm, Key* key, ArrayBufferView* data, ExceptionState& exceptionState)
{
    return startCryptoOperation(rawAlgorithm, key, Decrypt, 0, data, exceptionState);
}

ScriptPromise SubtleCrypto::sign(const Dictionary& rawAlgorithm, Key* key, ArrayBufferView* data, ExceptionState& exceptionState)
{
    return startCryptoOperation(rawAlgorithm, key, Sign, 0, data, exceptionState);
}

ScriptPromise SubtleCrypto::verifySignature(const Dictionary& rawAlgorithm, Key* key, ArrayBufferView* signature, ArrayBufferView* data, ExceptionState& exceptionState)
{
    return startCryptoOperation(rawAlgorithm, key, Verify, signature, data, exceptionState);
}

ScriptPromise SubtleCrypto::digest(const Dictionary& rawAlgorithm, ArrayBufferView* data, ExceptionState& exceptionState)
{
    return startCryptoOperation(rawAlgorithm, 0, Digest, 0, data, exceptionState);
}

ScriptPromise SubtleCrypto::generateKey(const Dictionary& rawAlgorithm, bool extractable, const Vector<String>& rawKeyUsages, ExceptionState& exceptionState)
{
    ScriptPromise promise = ScriptPromise::createPending();
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create(promise);

    blink::WebCryptoKeyUsageMask keyUsages;
    if (!Key::parseUsageMask(rawKeyUsages, keyUsages, result.get()))
        return promise;

    blink::WebCryptoAlgorithm algorithm;
    if (!parseAlgorithm(rawAlgorithm, GenerateKey, algorithm, exceptionState, result.get()))
        return promise;

    blink::Platform::current()->crypto()->generateKey(algorithm, extractable, keyUsages, result->result());
    return promise;
}

ScriptPromise SubtleCrypto::importKey(const String& rawFormat, ArrayBufferView* keyData, const Dictionary& rawAlgorithm, bool extractable, const Vector<String>& rawKeyUsages, ExceptionState& exceptionState)
{
    if (!keyData) {
        exceptionState.throwTypeError("Invalid keyData argument");
        return ScriptPromise();
    }

    ScriptPromise promise = ScriptPromise::createPending();
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create(promise);

    blink::WebCryptoKeyFormat format;
    if (!Key::parseFormat(rawFormat, format, result.get()))
        return promise;

    blink::WebCryptoKeyUsageMask keyUsages;
    if (!Key::parseUsageMask(rawKeyUsages, keyUsages, result.get()))
        return promise;

    // The algorithm is optional.
    blink::WebCryptoAlgorithm algorithm;
    if (!rawAlgorithm.isUndefinedOrNull() && !parseAlgorithm(rawAlgorithm, ImportKey, algorithm, exceptionState, result.get()))
        return promise;

    const unsigned char* keyDataBytes = static_cast<unsigned char*>(keyData->baseAddress());

    blink::Platform::current()->crypto()->importKey(format, keyDataBytes, keyData->byteLength(), algorithm, extractable, keyUsages, result->result());
    return promise;
}

ScriptPromise SubtleCrypto::exportKey(const String& rawFormat, Key* key, ExceptionState& exceptionState)
{
    if (!key) {
        exceptionState.throwTypeError("Invalid key argument");
        return ScriptPromise();
    }

    ScriptPromise promise = ScriptPromise::createPending();
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create(promise);

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

ScriptPromise SubtleCrypto::wrapKey(const String& rawFormat, Key* key, Key* wrappingKey, const Dictionary& rawWrapAlgorithm, ExceptionState& exceptionState)
{
    if (!key) {
        exceptionState.throwTypeError("Invalid key argument");
        return ScriptPromise();
    }

    if (!wrappingKey) {
        exceptionState.throwTypeError("Invalid wrappingKey argument");
        return ScriptPromise();
    }

    ScriptPromise promise = ScriptPromise::createPending();
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create(promise);

    blink::WebCryptoKeyFormat format;
    if (!Key::parseFormat(rawFormat, format, result.get()))
        return promise;

    blink::WebCryptoAlgorithm wrapAlgorithm;
    if (!parseAlgorithm(rawWrapAlgorithm, WrapKey, wrapAlgorithm, exceptionState, result.get()))
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

ScriptPromise SubtleCrypto::unwrapKey(const String& rawFormat, ArrayBufferView* wrappedKey, Key* unwrappingKey, const Dictionary& rawUnwrapAlgorithm, const Dictionary& rawUnwrappedKeyAlgorithm, bool extractable, const Vector<String>& rawKeyUsages, ExceptionState& exceptionState)
{
    if (!wrappedKey) {
        exceptionState.throwTypeError("Invalid wrappedKey argument");
        return ScriptPromise();
    }

    if (!unwrappingKey) {
        exceptionState.throwTypeError("Invalid unwrappingKey argument");
        return ScriptPromise();
    }

    ScriptPromise promise = ScriptPromise::createPending();
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create(promise);

    blink::WebCryptoKeyFormat format;
    if (!Key::parseFormat(rawFormat, format, result.get()))
        return promise;

    blink::WebCryptoKeyUsageMask keyUsages;
    if (!Key::parseUsageMask(rawKeyUsages, keyUsages, result.get()))
        return promise;

    blink::WebCryptoAlgorithm unwrapAlgorithm;
    if (!parseAlgorithm(rawUnwrapAlgorithm, UnwrapKey, unwrapAlgorithm, exceptionState, result.get()))
        return promise;

    // The unwrappedKeyAlgorithm is optional.
    blink::WebCryptoAlgorithm unwrappedKeyAlgorithm;
    if (!rawUnwrappedKeyAlgorithm.isUndefinedOrNull() && !parseAlgorithm(rawUnwrappedKeyAlgorithm, ImportKey, unwrappedKeyAlgorithm, exceptionState, result.get()))
        return promise;

    if (!unwrappingKey->canBeUsedForAlgorithm(unwrapAlgorithm, UnwrapKey, result.get()))
        return promise;

    const unsigned char* wrappedKeyData = static_cast<const unsigned char*>(wrappedKey->baseAddress());
    unsigned wrappedKeyDataSize = wrappedKey->byteLength();

    blink::Platform::current()->crypto()->unwrapKey(format, wrappedKeyData, wrappedKeyDataSize, unwrappingKey->key(), unwrapAlgorithm, unwrappedKeyAlgorithm, extractable, keyUsages, result->result());
    return promise;
}

} // namespace WebCore
