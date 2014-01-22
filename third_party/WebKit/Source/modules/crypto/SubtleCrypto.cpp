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
#include "core/dom/ExceptionCode.h"
#include "modules/crypto/CryptoResultImpl.h"
#include "modules/crypto/Key.h"
#include "modules/crypto/NormalizeAlgorithm.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCrypto.h"
#include "public/platform/WebCryptoAlgorithm.h"
#include "wtf/ArrayBufferView.h"

namespace WebCore {

namespace {

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

    blink::WebCryptoAlgorithm algorithm;
    if (!normalizeAlgorithm(rawAlgorithm, operationType, algorithm, exceptionState))
        return ScriptPromise();

    if (requiresKey && !key->canBeUsedForAlgorithm(algorithm, operationType, exceptionState))
        return ScriptPromise();

    const unsigned char* data = static_cast<const unsigned char*>(dataBuffer->baseAddress());
    unsigned dataSize = dataBuffer->byteLength();

    ScriptPromise promise = ScriptPromise::createPending();
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create(promise);

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
    blink::WebCryptoKeyUsageMask keyUsages;
    if (!Key::parseUsageMask(rawKeyUsages, keyUsages, exceptionState))
        return ScriptPromise();

    blink::WebCryptoAlgorithm algorithm;
    if (!normalizeAlgorithm(rawAlgorithm, GenerateKey, algorithm, exceptionState))
        return ScriptPromise();

    ScriptPromise promise = ScriptPromise::createPending();
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create(promise);
    blink::Platform::current()->crypto()->generateKey(algorithm, extractable, keyUsages, result->result());
    return promise;
}

ScriptPromise SubtleCrypto::importKey(const String& rawFormat, ArrayBufferView* keyData, const Dictionary& rawAlgorithm, bool extractable, const Vector<String>& rawKeyUsages, ExceptionState& exceptionState)
{
    blink::WebCryptoKeyFormat format;
    if (!Key::parseFormat(rawFormat, format, exceptionState))
        return ScriptPromise();

    if (!keyData) {
        exceptionState.throwTypeError("Invalid keyData argument");
        return ScriptPromise();
    }

    blink::WebCryptoKeyUsageMask keyUsages;
    if (!Key::parseUsageMask(rawKeyUsages, keyUsages, exceptionState))
        return ScriptPromise();

    // The algorithm is optional.
    blink::WebCryptoAlgorithm algorithm;
    if (!rawAlgorithm.isUndefinedOrNull() && !normalizeAlgorithm(rawAlgorithm, ImportKey, algorithm, exceptionState))
        return ScriptPromise();

    const unsigned char* keyDataBytes = static_cast<unsigned char*>(keyData->baseAddress());

    ScriptPromise promise = ScriptPromise::createPending();
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create(promise);
    blink::Platform::current()->crypto()->importKey(format, keyDataBytes, keyData->byteLength(), algorithm, extractable, keyUsages, result->result());
    return promise;
}

ScriptPromise SubtleCrypto::exportKey(const String& rawFormat, Key* key, ExceptionState& exceptionState)
{
    blink::WebCryptoKeyFormat format;
    if (!Key::parseFormat(rawFormat, format, exceptionState))
        return ScriptPromise();

    if (!key) {
        exceptionState.throwTypeError("Invalid key argument");
        return ScriptPromise();
    }

    if (!key->extractable()) {
        exceptionState.throwDOMException(NotSupportedError, "key is not extractable");
        return ScriptPromise();
    }

    ScriptPromise promise = ScriptPromise::createPending();
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create(promise);
    blink::Platform::current()->crypto()->exportKey(format, key->key(), result->result());
    return promise;
}

ScriptPromise SubtleCrypto::wrapKey(const String& rawFormat, Key* key, Key* wrappingKey, const Dictionary& rawWrapAlgorithm, ExceptionState& exceptionState)
{
    blink::WebCryptoKeyFormat format;
    if (!Key::parseFormat(rawFormat, format, exceptionState))
        return ScriptPromise();

    if (!key) {
        exceptionState.throwTypeError("Invalid key argument");
        return ScriptPromise();
    }

    if (!wrappingKey) {
        exceptionState.throwTypeError("Invalid wrappingKey argument");
        return ScriptPromise();
    }

    blink::WebCryptoAlgorithm wrapAlgorithm;
    if (!normalizeAlgorithm(rawWrapAlgorithm, WrapKey, wrapAlgorithm, exceptionState))
        return ScriptPromise();

    if (!key->extractable()) {
        exceptionState.throwDOMException(NotSupportedError, "key is not extractable");
        return ScriptPromise();
    }

    if (!wrappingKey->canBeUsedForAlgorithm(wrapAlgorithm, WrapKey, exceptionState))
        return ScriptPromise();

    ScriptPromise promise = ScriptPromise::createPending();
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create(promise);
    blink::Platform::current()->crypto()->wrapKey(format, key->key(), wrappingKey->key(), wrapAlgorithm, result->result());
    return promise;
}

ScriptPromise SubtleCrypto::unwrapKey(const String& rawFormat, ArrayBufferView* wrappedKey, Key* unwrappingKey, const Dictionary& rawUnwrapAlgorithm, const Dictionary& rawUnwrappedKeyAlgorithm, bool extractable, const Vector<String>& rawKeyUsages, ExceptionState& exceptionState)
{
    blink::WebCryptoKeyFormat format;
    if (!Key::parseFormat(rawFormat, format, exceptionState))
        return ScriptPromise();

    if (!wrappedKey) {
        exceptionState.throwTypeError("Invalid wrappedKey argument");
        return ScriptPromise();
    }

    if (!unwrappingKey) {
        exceptionState.throwTypeError("Invalid unwrappingKey argument");
        return ScriptPromise();
    }

    blink::WebCryptoAlgorithm unwrapAlgorithm;
    if (!normalizeAlgorithm(rawUnwrapAlgorithm, UnwrapKey, unwrapAlgorithm, exceptionState))
        return ScriptPromise();

    // The unwrappedKeyAlgorithm is optional.
    blink::WebCryptoAlgorithm unwrappedKeyAlgorithm;
    if (!rawUnwrappedKeyAlgorithm.isUndefinedOrNull() && !normalizeAlgorithm(rawUnwrappedKeyAlgorithm, ImportKey, unwrappedKeyAlgorithm, exceptionState))
        return ScriptPromise();

    blink::WebCryptoKeyUsageMask keyUsages;
    if (!Key::parseUsageMask(rawKeyUsages, keyUsages, exceptionState))
        return ScriptPromise();

    if (!unwrappingKey->canBeUsedForAlgorithm(unwrapAlgorithm, UnwrapKey, exceptionState))
        return ScriptPromise();

    const unsigned char* wrappedKeyData = static_cast<const unsigned char*>(wrappedKey->baseAddress());
    unsigned wrappedKeyDataSize = wrappedKey->byteLength();

    ScriptPromise promise = ScriptPromise::createPending();
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create(promise);
    blink::Platform::current()->crypto()->unwrapKey(format, wrappedKeyData, wrappedKeyDataSize, unwrappingKey->key(), unwrapAlgorithm, unwrappedKeyAlgorithm, extractable, keyUsages, result->result());
    return promise;
}


} // namespace WebCore
