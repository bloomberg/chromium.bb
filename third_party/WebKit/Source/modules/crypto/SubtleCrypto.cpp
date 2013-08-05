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
#include "core/platform/NotImplemented.h"
#include "modules/crypto/CryptoOperation.h"
#include "modules/crypto/Key.h"
#include "modules/crypto/KeyOperation.h"
#include "modules/crypto/NormalizeAlgorithm.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCrypto.h"
#include "public/platform/WebCryptoAlgorithmParams.h"
#include "wtf/ArrayBufferView.h"

namespace WebCore {

// FIXME: Outstanding KeyOperations and CryptoOperations should be aborted when
// tearing down SubtleCrypto (to avoid problems completing a
// ScriptPromiseResolver which is no longer valid).

namespace {

WebKit::WebCryptoKeyUsageMask toKeyUsage(AlgorithmOperation operation)
{
    switch (operation) {
    case Encrypt:
        return WebKit::WebCryptoKeyUsageEncrypt;
    case Decrypt:
        return WebKit::WebCryptoKeyUsageDecrypt;
    case Sign:
        return WebKit::WebCryptoKeyUsageSign;
    case Verify:
        return WebKit::WebCryptoKeyUsageVerify;
    case DeriveKey:
        return WebKit::WebCryptoKeyUsageDeriveKey;
    case WrapKey:
        return WebKit::WebCryptoKeyUsageWrapKey;
    case UnwrapKey:
        return WebKit::WebCryptoKeyUsageUnwrapKey;
    case Digest:
    case GenerateKey:
    case ImportKey:
    case NumberOfAlgorithmOperations:
        break;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

bool keyCanBeUsedForAlgorithm(const WebKit::WebCryptoKey& key, const WebKit::WebCryptoAlgorithm& algorithm, AlgorithmOperation op)
{
    if (!(key.usages() & toKeyUsage(op)))
        return false;

    if (key.algorithm().id() != algorithm.id())
        return false;

    if (key.algorithm().paramsType() == WebKit::WebCryptoAlgorithmParamsTypeNone)
        return true;

    // Verify that the algorithm-specific parameters for the key conform to the
    // algorithm.

    if (key.algorithm().paramsType() == WebKit::WebCryptoAlgorithmParamsTypeHmacParams) {
        return key.algorithm().hmacParams()->hash().id() == algorithm.hmacParams()->hash().id();
    }

    ASSERT_NOT_REACHED();
    return false;
}

PassRefPtr<CryptoOperation> createCryptoOperation(const Dictionary& rawAlgorithm, Key* key, AlgorithmOperation operationType, ArrayBufferView* signature, ExceptionState& es)
{
    WebKit::WebCrypto* platformCrypto = WebKit::Platform::current()->crypto();
    if (!platformCrypto) {
        es.throwDOMException(NotSupportedError);
        return 0;
    }

    WebKit::WebCryptoAlgorithm algorithm;
    if (!normalizeAlgorithm(rawAlgorithm, operationType, algorithm, es))
        return 0;

    // All operations other than Digest require a valid Key.
    if (operationType != Digest) {
        if (!key) {
            es.throwTypeError();
            return 0;
        }

        if (!keyCanBeUsedForAlgorithm(key->key(), algorithm, operationType)) {
            es.throwDOMException(NotSupportedError);
            return 0;
        }
    }

    // Only Verify takes a signature.
    if (operationType == Verify && !signature) {
        es.throwTypeError();
        return 0;
    }

    RefPtr<CryptoOperationImpl> opImpl = CryptoOperationImpl::create();
    WebKit::WebCryptoOperationResult result(opImpl.get());

    switch (operationType) {
    case Encrypt:
        platformCrypto->encrypt(algorithm, key->key(), result);
        break;
    case Decrypt:
        platformCrypto->decrypt(algorithm, key->key(), result);
        break;
    case Sign:
        platformCrypto->sign(algorithm, key->key(), result);
        break;
    case Verify:
        platformCrypto->verifySignature(algorithm, key->key(), reinterpret_cast<const unsigned char*>(signature->baseAddress()), signature->byteLength(), result);
        break;
    case Digest:
        platformCrypto->digest(algorithm, result);
        break;
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }

    if (opImpl->throwInitializationError(es))
        return 0;
    return CryptoOperation::create(algorithm, opImpl.get());
}

} // namespace

SubtleCrypto::SubtleCrypto()
{
    ScriptWrappable::init(this);
}

PassRefPtr<CryptoOperation> SubtleCrypto::encrypt(const Dictionary& rawAlgorithm, Key* key, ExceptionState& es)
{
    return createCryptoOperation(rawAlgorithm, key, Encrypt, 0, es);
}

PassRefPtr<CryptoOperation> SubtleCrypto::decrypt(const Dictionary& rawAlgorithm, Key* key, ExceptionState& es)
{
    return createCryptoOperation(rawAlgorithm, key, Decrypt, 0, es);
}

PassRefPtr<CryptoOperation> SubtleCrypto::sign(const Dictionary& rawAlgorithm, Key* key, ExceptionState& es)
{
    return createCryptoOperation(rawAlgorithm, key, Sign, 0, es);
}

PassRefPtr<CryptoOperation> SubtleCrypto::verifySignature(const Dictionary& rawAlgorithm, Key* key, ArrayBufferView* signature, ExceptionState& es)
{
    return createCryptoOperation(rawAlgorithm, key, Verify, signature, es);
}

PassRefPtr<CryptoOperation> SubtleCrypto::digest(const Dictionary& rawAlgorithm, ExceptionState& es)
{
    return createCryptoOperation(rawAlgorithm, 0, Digest, 0, es);
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

    RefPtr<KeyOperation> keyOp = KeyOperation::create();
    WebKit::WebCryptoKeyOperationResult result(keyOp.get());
    platformCrypto->generateKey(algorithm, extractable, keyUsages, result);
    return keyOp->returnValue(es);
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

    RefPtr<KeyOperation> keyOp = KeyOperation::create();
    WebKit::WebCryptoKeyOperationResult result(keyOp.get());
    platformCrypto->importKey(format, keyDataBytes, keyData->byteLength(), algorithm, extractable, keyUsages, result);
    return keyOp->returnValue(es);
}

} // namespace WebCore
