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
#include "modules/crypto/Key.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/crypto/Algorithm.h"
#include "public/platform/WebCryptoAlgorithmParams.h"

namespace WebCore {

namespace {

const char* keyTypeToString(WebKit::WebCryptoKeyType type)
{
    switch (type) {
    case WebKit::WebCryptoKeyTypeSecret:
        return "secret";
    case WebKit::WebCryptoKeyTypePublic:
        return "public";
    case WebKit::WebCryptoKeyTypePrivate:
        return "private";
    }
    ASSERT_NOT_REACHED();
    return 0;
}

struct KeyUsageMapping {
    WebKit::WebCryptoKeyUsage value;
    const char* const name;
};

const KeyUsageMapping keyUsageMappings[] = {
    { WebKit::WebCryptoKeyUsageEncrypt, "encrypt" },
    { WebKit::WebCryptoKeyUsageDecrypt, "decrypt" },
    { WebKit::WebCryptoKeyUsageSign, "sign" },
    { WebKit::WebCryptoKeyUsageVerify, "verify" },
    { WebKit::WebCryptoKeyUsageDeriveKey, "deriveKey" },
    { WebKit::WebCryptoKeyUsageWrapKey, "wrapKey" },
    { WebKit::WebCryptoKeyUsageUnwrapKey, "unwrapKey" },
};

COMPILE_ASSERT(WebKit::EndOfWebCryptoKeyUsage == (1 << 6) + 1, update_keyUsageMappings);

const char* keyUsageToString(WebKit::WebCryptoKeyUsage usage)
{
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(keyUsageMappings); ++i) {
        if (keyUsageMappings[i].value == usage)
            return keyUsageMappings[i].name;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

WebKit::WebCryptoKeyUsageMask keyUsageStringToMask(const String& usageString)
{
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(keyUsageMappings); ++i) {
        if (keyUsageMappings[i].name == usageString)
            return keyUsageMappings[i].value;
    }
    return 0;
}

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

bool getHmacHashId(const WebKit::WebCryptoAlgorithm& algorithm, WebKit::WebCryptoAlgorithmId& hashId)
{
    if (algorithm.hmacParams()) {
        hashId = algorithm.hmacParams()->hash().id();
        return true;
    }
    if (algorithm.hmacKeyParams()) {
        hashId = algorithm.hmacKeyParams()->hash().id();
        return true;
    }
    return false;
}

} // namespace

Key::~Key()
{
}

Key::Key(const WebKit::WebCryptoKey& key)
    : m_key(key)
{
    ScriptWrappable::init(this);
}

String Key::type() const
{
    return keyTypeToString(m_key.type());
}

bool Key::extractable() const
{
    return m_key.extractable();
}

Algorithm* Key::algorithm()
{
    if (!m_algorithm)
        m_algorithm = Algorithm::create(m_key.algorithm());
    return m_algorithm.get();
}

// FIXME: This creates a new javascript array each time. What should happen
//        instead is return the same (immutable) array. (Javascript callers can
//        distinguish this by doing an == test on the arrays and seeing they are
//        different).
Vector<String> Key::usages() const
{
    Vector<String> result;
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(keyUsageMappings); ++i) {
        WebKit::WebCryptoKeyUsage usage = keyUsageMappings[i].value;
        if (m_key.usages() & usage)
            result.append(keyUsageToString(usage));
    }
    return result;
}

bool Key::canBeUsedForAlgorithm(const WebKit::WebCryptoAlgorithm& algorithm, AlgorithmOperation op, ExceptionState& es) const
{
    if (!(m_key.usages() & toKeyUsage(op))) {
        es.throwDOMException(NotSupportedError, "key.usages does not permit this operation");
        return false;
    }

    if (m_key.algorithm().id() != algorithm.id()) {
        es.throwDOMException(NotSupportedError, "key.algorithm does not match that of operation");
        return false;
    }

    // Verify that the algorithm-specific parameters for the key conform to the
    // algorithm.

    if (m_key.algorithm().id() == WebKit::WebCryptoAlgorithmIdHmac) {
        WebKit::WebCryptoAlgorithmId keyHash;
        WebKit::WebCryptoAlgorithmId algorithmHash;
        if (!getHmacHashId(m_key.algorithm(), keyHash) || !getHmacHashId(algorithm, algorithmHash) || keyHash != algorithmHash) {
            es.throwDOMException(NotSupportedError, "key.algorithm does not match that of operation (HMAC's hash differs)");
            return false;
        }
    }

    return true;
}

bool Key::parseFormat(const String& formatString, WebKit::WebCryptoKeyFormat& format, ExceptionState& es)
{
    // There are few enough values that testing serially is fast enough.
    if (formatString == "raw") {
        format = WebKit::WebCryptoKeyFormatRaw;
        return true;
    }
    if (formatString == "pkcs8") {
        format = WebKit::WebCryptoKeyFormatPkcs8;
        return true;
    }
    if (formatString == "spki") {
        format = WebKit::WebCryptoKeyFormatSpki;
        return true;
    }
    if (formatString == "jwk") {
        format = WebKit::WebCryptoKeyFormatJwk;
        return true;
    }

    es.throwTypeError("Invalid keyFormat argument");
    return false;
}

bool Key::parseUsageMask(const Vector<String>& usages, WebKit::WebCryptoKeyUsageMask& mask, ExceptionState& es)
{
    mask = 0;
    for (size_t i = 0; i < usages.size(); ++i) {
        WebKit::WebCryptoKeyUsageMask usage = keyUsageStringToMask(usages[i]);
        if (!usage) {
            es.throwTypeError("Invalid keyUsages argument");
            return false;
        }
        mask |= usage;
    }
    return true;
}

} // namespace WebCore
