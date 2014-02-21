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
#include "platform/CryptoResult.h"
#include "public/platform/WebCryptoAlgorithmParams.h"
#include "public/platform/WebString.h"

namespace WebCore {

DEFINE_GC_INFO(Key);

namespace {

const char* keyTypeToString(blink::WebCryptoKeyType type)
{
    switch (type) {
    case blink::WebCryptoKeyTypeSecret:
        return "secret";
    case blink::WebCryptoKeyTypePublic:
        return "public";
    case blink::WebCryptoKeyTypePrivate:
        return "private";
    }
    ASSERT_NOT_REACHED();
    return 0;
}

struct KeyUsageMapping {
    blink::WebCryptoKeyUsage value;
    const char* const name;
};

// Keep this array sorted.
const KeyUsageMapping keyUsageMappings[] = {
    { blink::WebCryptoKeyUsageDecrypt, "decrypt" },
    { blink::WebCryptoKeyUsageDeriveKey, "deriveKey" },
    { blink::WebCryptoKeyUsageEncrypt, "encrypt" },
    { blink::WebCryptoKeyUsageSign, "sign" },
    { blink::WebCryptoKeyUsageUnwrapKey, "unwrapKey" },
    { blink::WebCryptoKeyUsageVerify, "verify" },
    { blink::WebCryptoKeyUsageWrapKey, "wrapKey" },
};

COMPILE_ASSERT(blink::EndOfWebCryptoKeyUsage == (1 << 6) + 1, update_keyUsageMappings);

const char* keyUsageToString(blink::WebCryptoKeyUsage usage)
{
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(keyUsageMappings); ++i) {
        if (keyUsageMappings[i].value == usage)
            return keyUsageMappings[i].name;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

blink::WebCryptoKeyUsageMask keyUsageStringToMask(const String& usageString)
{
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(keyUsageMappings); ++i) {
        if (keyUsageMappings[i].name == usageString)
            return keyUsageMappings[i].value;
    }
    return 0;
}

blink::WebCryptoKeyUsageMask toKeyUsage(AlgorithmOperation operation)
{
    switch (operation) {
    case Encrypt:
        return blink::WebCryptoKeyUsageEncrypt;
    case Decrypt:
        return blink::WebCryptoKeyUsageDecrypt;
    case Sign:
        return blink::WebCryptoKeyUsageSign;
    case Verify:
        return blink::WebCryptoKeyUsageVerify;
    case DeriveKey:
        return blink::WebCryptoKeyUsageDeriveKey;
    case WrapKey:
        return blink::WebCryptoKeyUsageWrapKey;
    case UnwrapKey:
        return blink::WebCryptoKeyUsageUnwrapKey;
    case Digest:
    case GenerateKey:
    case ImportKey:
        break;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

bool getHmacHashId(const blink::WebCryptoAlgorithm& algorithm, blink::WebCryptoAlgorithmId& hashId)
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

Key::Key(const blink::WebCryptoKey& key)
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
        blink::WebCryptoKeyUsage usage = keyUsageMappings[i].value;
        if (m_key.usages() & usage)
            result.append(keyUsageToString(usage));
    }
    return result;
}

bool Key::canBeUsedForAlgorithm(const blink::WebCryptoAlgorithm& algorithm, AlgorithmOperation op, CryptoResult* result) const
{
    if (!(m_key.usages() & toKeyUsage(op))) {
        result->completeWithError("key.usages does not permit this operation");
        return false;
    }

    if (m_key.algorithm().id() != algorithm.id()) {
        result->completeWithError("key.algorithm does not match that of operation");
        return false;
    }

    // Verify that the algorithm-specific parameters for the key conform to the
    // algorithm.
    // FIXME: This is incomplete and not future proof. Operational parameters
    //        should be enumerated when defining new parameters.

    if (m_key.algorithm().id() == blink::WebCryptoAlgorithmIdHmac) {
        blink::WebCryptoAlgorithmId keyHash;
        blink::WebCryptoAlgorithmId algorithmHash;
        if (!getHmacHashId(m_key.algorithm(), keyHash) || !getHmacHashId(algorithm, algorithmHash) || keyHash != algorithmHash) {
            result->completeWithError("key.algorithm does not match that of operation (HMAC's hash differs)");
            return false;
        }
    }

    return true;
}

bool Key::parseFormat(const String& formatString, blink::WebCryptoKeyFormat& format, CryptoResult* result)
{
    // There are few enough values that testing serially is fast enough.
    if (formatString == "raw") {
        format = blink::WebCryptoKeyFormatRaw;
        return true;
    }
    if (formatString == "pkcs8") {
        format = blink::WebCryptoKeyFormatPkcs8;
        return true;
    }
    if (formatString == "spki") {
        format = blink::WebCryptoKeyFormatSpki;
        return true;
    }
    if (formatString == "jwk") {
        format = blink::WebCryptoKeyFormatJwk;
        return true;
    }

    result->completeWithError("Invalid keyFormat argument");
    return false;
}

bool Key::parseUsageMask(const Vector<String>& usages, blink::WebCryptoKeyUsageMask& mask, CryptoResult* result)
{
    mask = 0;
    for (size_t i = 0; i < usages.size(); ++i) {
        blink::WebCryptoKeyUsageMask usage = keyUsageStringToMask(usages[i]);
        if (!usage) {
            result->completeWithError("Invalid keyUsages argument");
            return false;
        }
        mask |= usage;
    }
    return true;
}

void Key::trace(Visitor* visitor)
{
    visitor->trace(m_algorithm);
}

} // namespace WebCore
