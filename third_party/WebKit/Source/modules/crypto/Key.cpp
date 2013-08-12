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

#include "modules/crypto/Algorithm.h"

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
    for (int i = 0; i < WTF_ARRAY_LENGTH(keyUsageMappings); ++i) {
        WebKit::WebCryptoKeyUsage usage = keyUsageMappings[i].value;
        if (m_key.usages() & usage)
            result.append(keyUsageToString(usage));
    }
    return result;
}

bool Key::parseFormat(const String& formatString, WebKit::WebCryptoKeyFormat& format)
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

    return false;
}

bool Key::parseUsageMask(const Vector<String>& usages, WebKit::WebCryptoKeyUsageMask& mask)
{
    mask = 0;
    for (size_t i = 0; i < usages.size(); ++i) {
        WebKit::WebCryptoKeyUsageMask usage = keyUsageStringToMask(usages[i]);
        if (!usage)
            return false;
        mask |= usage;
    }
    return true;
}

} // namespace WebCore
