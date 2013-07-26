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
#include "modules/crypto/Key.h"
#include "modules/crypto/KeyOperation.h"
#include "modules/crypto/NormalizeAlgorithm.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCrypto.h"
#include "wtf/ArrayBufferView.h"

namespace WebCore {

// FIXME: Outstanding KeyOperations and CryptoOperations should be aborted when
// tearing down SubtleCrypto (to avoid problems completing a
// ScriptPromiseResolver which is no longer valid).

namespace {

// FIXME: Temporary
PassRefPtr<CryptoOperation> dummyOperation(const Dictionary& rawAlgorithm, AlgorithmOperation operationType, ExceptionState& es)
{
    WebKit::WebCrypto* platformCrypto = WebKit::Platform::current()->crypto();
    if (!platformCrypto) {
        es.throwDOMException(NotSupportedError);
        return 0;
    }

    WebKit::WebCryptoAlgorithm algorithm;
    if (!normalizeAlgorithm(rawAlgorithm, operationType, algorithm, es))
        return 0;

    RefPtr<CryptoOperationImpl> opImpl = CryptoOperationImpl::create();
    WebKit::WebCryptoOperationResult result(opImpl.get());
    platformCrypto->digest(algorithm, result);
    if (opImpl->throwInitializationError(es))
        return 0;
    return CryptoOperation::create(algorithm, opImpl.get());
}

} // namespace

SubtleCrypto::SubtleCrypto()
{
    ScriptWrappable::init(this);
}

PassRefPtr<CryptoOperation> SubtleCrypto::encrypt(const Dictionary& rawAlgorithm, Key*, ExceptionState& es)
{
    return dummyOperation(rawAlgorithm, Encrypt, es);
}

PassRefPtr<CryptoOperation> SubtleCrypto::decrypt(const Dictionary& rawAlgorithm, Key*, ExceptionState& es)
{
    return dummyOperation(rawAlgorithm, Decrypt, es);
}

PassRefPtr<CryptoOperation> SubtleCrypto::sign(const Dictionary& rawAlgorithm, Key*, ExceptionState& es)
{
    return dummyOperation(rawAlgorithm, Sign, es);
}

PassRefPtr<CryptoOperation> SubtleCrypto::verifySignature(const Dictionary& rawAlgorithm, Key*, ExceptionState& es)
{
    return dummyOperation(rawAlgorithm, Verify, es);
}

PassRefPtr<CryptoOperation> SubtleCrypto::digest(const Dictionary& rawAlgorithm, ExceptionState& es)
{
    WebKit::WebCrypto* platformCrypto = WebKit::Platform::current()->crypto();
    if (!platformCrypto) {
        es.throwDOMException(NotSupportedError);
        return 0;
    }

    WebKit::WebCryptoAlgorithm algorithm;
    if (!normalizeAlgorithm(rawAlgorithm, Digest, algorithm, es))
        return 0;

    RefPtr<CryptoOperationImpl> opImpl = CryptoOperationImpl::create();
    WebKit::WebCryptoOperationResult result(opImpl.get());
    platformCrypto->digest(algorithm, result);
    if (opImpl->throwInitializationError(es))
        return 0;
    return CryptoOperation::create(algorithm, opImpl.get());
}

ScriptObject SubtleCrypto::importKey(const String& rawFormat, ArrayBufferView* keyData, const Dictionary& rawAlgorithm, bool extractable, const Vector<String>& rawKeyUsages, ExceptionState& es)
{
    WebKit::WebCrypto* platformCrypto = WebKit::Platform::current()->crypto();
    if (!platformCrypto) {
        es.throwDOMException(NotSupportedError);
        return ScriptObject();
    }

    if (!keyData) {
        es.throwDOMException(TypeError);
        return ScriptObject();
    }

    WebKit::WebCryptoKeyUsageMask keyUsages;
    if (!Key::parseUsageMask(rawKeyUsages, keyUsages)) {
        es.throwDOMException(TypeError);
        return ScriptObject();
    }

    WebKit::WebCryptoKeyFormat format;
    if (!Key::parseFormat(rawFormat, format)) {
        es.throwDOMException(TypeError);
        return ScriptObject();
    }

    WebKit::WebCryptoAlgorithm algorithm;
    if (!normalizeAlgorithmForImportKey(rawAlgorithm, algorithm, es))
        return ScriptObject();

    const unsigned char* keyDataBytes = static_cast<unsigned char*>(keyData->baseAddress());

    RefPtr<KeyOperation> keyOp = KeyOperation::create();
    WebKit::WebCryptoKeyOperationResult result(keyOp.get());
    platformCrypto->importKey(format, keyDataBytes, keyData->byteLength(), algorithm, extractable, keyUsages, result);
    return keyOp->returnValue(es);
}

} // namespace WebCore
