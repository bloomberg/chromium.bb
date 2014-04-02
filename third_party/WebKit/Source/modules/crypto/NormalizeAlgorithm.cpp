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
#include "modules/crypto/NormalizeAlgorithm.h"

#include "bindings/v8/Dictionary.h"
#include "platform/CryptoResult.h"
#include "platform/NotImplemented.h"
#include "public/platform/WebCryptoAlgorithmParams.h"
#include "public/platform/WebString.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/ArrayBufferView.h"
#include "wtf/MathExtras.h"
#include "wtf/Uint8Array.h"
#include "wtf/Vector.h"
#include "wtf/text/StringBuilder.h"
#include <algorithm>

namespace WebCore {

namespace {

struct AlgorithmNameMapping {
    // Must be an upper case ASCII string.
    const char* const algorithmName;
    // Must be strlen(algorithmName).
    unsigned char algorithmNameLength;
    blink::WebCryptoAlgorithmId algorithmId;

#if ASSERT_ENABLED
    bool operator<(const AlgorithmNameMapping&) const;
#endif
};

struct OperationParamsMapping {
    blink::WebCryptoAlgorithmId algorithmId;
    AlgorithmOperation operation;
    blink::WebCryptoAlgorithmParamsType params;

    bool operator<(const OperationParamsMapping&) const;
};

// Must be sorted by length, and then by reverse string.
// Also all names must be upper case ASCII.
const AlgorithmNameMapping algorithmNameMappings[] = {
    {"HMAC", 4, blink::WebCryptoAlgorithmIdHmac},
    {"SHA-1", 5, blink::WebCryptoAlgorithmIdSha1},
    {"AES-KW", 6, blink::WebCryptoAlgorithmIdAesKw},
    {"SHA-512", 7, blink::WebCryptoAlgorithmIdSha512},
    {"SHA-384", 7, blink::WebCryptoAlgorithmIdSha384},
    {"SHA-256", 7, blink::WebCryptoAlgorithmIdSha256},
    {"AES-CBC", 7, blink::WebCryptoAlgorithmIdAesCbc},
    {"AES-GCM", 7, blink::WebCryptoAlgorithmIdAesGcm},
    {"AES-CTR", 7, blink::WebCryptoAlgorithmIdAesCtr},
    {"RSAES-PKCS1-V1_5", 16, blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5},
    {"RSASSA-PKCS1-V1_5", 17, blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5},
};

// What operations each algorithm supports, and what parameters it expects.
// Must be sorted by algorithm id and then operation.
const OperationParamsMapping operationParamsMappings[] = {
    // AES-CBC
    {blink::WebCryptoAlgorithmIdAesCbc, Encrypt, blink::WebCryptoAlgorithmParamsTypeAesCbcParams},
    {blink::WebCryptoAlgorithmIdAesCbc, Decrypt, blink::WebCryptoAlgorithmParamsTypeAesCbcParams},
    {blink::WebCryptoAlgorithmIdAesCbc, GenerateKey, blink::WebCryptoAlgorithmParamsTypeAesKeyGenParams},
    {blink::WebCryptoAlgorithmIdAesCbc, ImportKey, blink::WebCryptoAlgorithmParamsTypeNone},
    {blink::WebCryptoAlgorithmIdAesCbc, WrapKey, blink::WebCryptoAlgorithmParamsTypeAesCbcParams},
    {blink::WebCryptoAlgorithmIdAesCbc, UnwrapKey, blink::WebCryptoAlgorithmParamsTypeAesCbcParams},

    // HMAC
    {blink::WebCryptoAlgorithmIdHmac, Sign, blink::WebCryptoAlgorithmParamsTypeNone},
    {blink::WebCryptoAlgorithmIdHmac, Verify, blink::WebCryptoAlgorithmParamsTypeNone},
    {blink::WebCryptoAlgorithmIdHmac, GenerateKey, blink::WebCryptoAlgorithmParamsTypeHmacKeyGenParams},
    {blink::WebCryptoAlgorithmIdHmac, ImportKey, blink::WebCryptoAlgorithmParamsTypeHmacImportParams},

    // RSASSA-PKCS1-v1_5
    {blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5, Sign, blink::WebCryptoAlgorithmParamsTypeNone},
    {blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5, Verify, blink::WebCryptoAlgorithmParamsTypeNone},
    {blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5, GenerateKey, blink::WebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams},
    {blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5, ImportKey, blink::WebCryptoAlgorithmParamsTypeRsaHashedImportParams},

    // RSAES-PKCS1-v1_5
    {blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5, Encrypt, blink::WebCryptoAlgorithmParamsTypeNone},
    {blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5, Decrypt, blink::WebCryptoAlgorithmParamsTypeNone},
    {blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5, GenerateKey, blink::WebCryptoAlgorithmParamsTypeRsaKeyGenParams},
    {blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5, ImportKey, blink::WebCryptoAlgorithmParamsTypeNone},
    {blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5, WrapKey, blink::WebCryptoAlgorithmParamsTypeNone},
    {blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5, UnwrapKey, blink::WebCryptoAlgorithmParamsTypeNone},

    // SHA-*
    {blink::WebCryptoAlgorithmIdSha1, Digest, blink::WebCryptoAlgorithmParamsTypeNone},
    {blink::WebCryptoAlgorithmIdSha256, Digest, blink::WebCryptoAlgorithmParamsTypeNone},
    {blink::WebCryptoAlgorithmIdSha384, Digest, blink::WebCryptoAlgorithmParamsTypeNone},
    {blink::WebCryptoAlgorithmIdSha512, Digest, blink::WebCryptoAlgorithmParamsTypeNone},

    // AES-GCM
    {blink::WebCryptoAlgorithmIdAesGcm, Encrypt, blink::WebCryptoAlgorithmParamsTypeAesGcmParams},
    {blink::WebCryptoAlgorithmIdAesGcm, Decrypt, blink::WebCryptoAlgorithmParamsTypeAesGcmParams},
    {blink::WebCryptoAlgorithmIdAesGcm, GenerateKey, blink::WebCryptoAlgorithmParamsTypeAesKeyGenParams},
    {blink::WebCryptoAlgorithmIdAesGcm, ImportKey, blink::WebCryptoAlgorithmParamsTypeNone},
    {blink::WebCryptoAlgorithmIdAesGcm, WrapKey, blink::WebCryptoAlgorithmParamsTypeAesGcmParams},
    {blink::WebCryptoAlgorithmIdAesGcm, UnwrapKey, blink::WebCryptoAlgorithmParamsTypeAesGcmParams},

    // AES-CTR
    {blink::WebCryptoAlgorithmIdAesCtr, Encrypt, blink::WebCryptoAlgorithmParamsTypeAesCtrParams},
    {blink::WebCryptoAlgorithmIdAesCtr, Decrypt, blink::WebCryptoAlgorithmParamsTypeAesCtrParams},
    {blink::WebCryptoAlgorithmIdAesCtr, GenerateKey, blink::WebCryptoAlgorithmParamsTypeAesKeyGenParams},
    {blink::WebCryptoAlgorithmIdAesCtr, ImportKey, blink::WebCryptoAlgorithmParamsTypeNone},
    {blink::WebCryptoAlgorithmIdAesCtr, WrapKey, blink::WebCryptoAlgorithmParamsTypeAesCtrParams},
    {blink::WebCryptoAlgorithmIdAesCtr, UnwrapKey, blink::WebCryptoAlgorithmParamsTypeAesCtrParams},

    // AES-KW
    {blink::WebCryptoAlgorithmIdAesKw, GenerateKey, blink::WebCryptoAlgorithmParamsTypeAesKeyGenParams},
    {blink::WebCryptoAlgorithmIdAesKw, ImportKey, blink::WebCryptoAlgorithmParamsTypeNone},
    {blink::WebCryptoAlgorithmIdAesKw, WrapKey, blink::WebCryptoAlgorithmParamsTypeNone},
    {blink::WebCryptoAlgorithmIdAesKw, UnwrapKey, blink::WebCryptoAlgorithmParamsTypeNone},
};

#if ASSERT_ENABLED

// Essentially std::is_sorted() (however that function is new to C++11).
template <typename Iterator>
bool isSorted(Iterator begin, Iterator end)
{
    if (begin == end)
        return true;

    Iterator prev = begin;
    Iterator cur = begin + 1;

    while (cur != end) {
        if (*cur < *prev)
            return false;
        cur++;
        prev++;
    }

    return true;
}

bool AlgorithmNameMapping::operator<(const AlgorithmNameMapping& o) const
{
    if (algorithmNameLength < o.algorithmNameLength)
        return true;
    if (algorithmNameLength > o.algorithmNameLength)
        return false;

    for (size_t i = 0; i < algorithmNameLength; ++i) {
        size_t reverseIndex = algorithmNameLength - i - 1;
        char c1 = algorithmName[reverseIndex];
        char c2 = o.algorithmName[reverseIndex];

        if (c1 < c2)
            return true;
        if (c1 > c2)
            return false;
    }

    return false;
}

bool verifyAlgorithmNameMappings(const AlgorithmNameMapping* begin, const AlgorithmNameMapping* end)
{
    for (const AlgorithmNameMapping* it = begin; it != end; ++it) {
        if (it->algorithmNameLength != strlen(it->algorithmName))
            return false;
        String str(it->algorithmName, it->algorithmNameLength);
        if (!str.containsOnlyASCII())
            return false;
        if (str.upper() != str)
            return false;
    }

    return isSorted(begin, end);
}
#endif

bool OperationParamsMapping::operator<(const OperationParamsMapping& o) const
{
    if (algorithmId < o.algorithmId)
        return true;
    if (algorithmId > o.algorithmId)
        return false;
    return operation < o.operation;
}

template <typename CharType>
bool algorithmNameComparator(const AlgorithmNameMapping& a, StringImpl* b)
{
    if (a.algorithmNameLength < b->length())
        return true;
    if (a.algorithmNameLength > b->length())
        return false;

    // Because the algorithm names contain many common prefixes, it is better
    // to compare starting at the end of the string.
    for (size_t i = 0; i < a.algorithmNameLength; ++i) {
        size_t reverseIndex = a.algorithmNameLength - i - 1;
        CharType c1 = a.algorithmName[reverseIndex];
        CharType c2 = b->getCharacters<CharType>()[reverseIndex];
        if (!isASCII(c2))
            return false;
        c2 = toASCIIUpper(c2);

        if (c1 < c2)
            return true;
        if (c1 > c2)
            return false;
    }

    return false;
}

bool lookupAlgorithmIdByName(const String& algorithmName, blink::WebCryptoAlgorithmId& id)
{
    const AlgorithmNameMapping* begin = algorithmNameMappings;
    const AlgorithmNameMapping* end = algorithmNameMappings + WTF_ARRAY_LENGTH(algorithmNameMappings);

    ASSERT(verifyAlgorithmNameMappings(begin, end));

    const AlgorithmNameMapping* it;
    if (algorithmName.impl()->is8Bit())
        it = std::lower_bound(begin, end, algorithmName.impl(), &algorithmNameComparator<LChar>);
    else
        it = std::lower_bound(begin, end, algorithmName.impl(), &algorithmNameComparator<UChar>);

    if (it == end)
        return false;

    if (it->algorithmNameLength != algorithmName.length() || !equalIgnoringCase(algorithmName, it->algorithmName))
        return false;

    id = it->algorithmId;
    return true;
}

bool lookupAlgorithmParamsType(blink::WebCryptoAlgorithmId id, AlgorithmOperation op, blink::WebCryptoAlgorithmParamsType& paramsType)
{
    const OperationParamsMapping* begin = operationParamsMappings;
    const OperationParamsMapping* end = operationParamsMappings + WTF_ARRAY_LENGTH(operationParamsMappings);

    ASSERT(isSorted(begin, end));

    OperationParamsMapping search = { id, op };
    const OperationParamsMapping* it = std::lower_bound(begin, end, search);
    if (it == end)
        return false;
    if (it->algorithmId != id || it->operation != op)
        return false;
    paramsType = it->params;
    return true;
}

// ErrorContext holds a stack of string literals which describe what was
// happening at the time the error occurred. This is helpful because
// parsing of the algorithm dictionary can be recursive and it is difficult to
// tell what went wrong from a failure alone.
class ErrorContext {
public:
    void add(const char* message)
    {
        m_messages.append(message);
    }

    // Join all of the string literals into a single String.
    String toString() const
    {
        if (m_messages.isEmpty())
            return String();

        StringBuilder result;
        const char* Separator = ": ";

        size_t length = (m_messages.size() - 1) * strlen(Separator);
        for (size_t i = 0; i < m_messages.size(); ++i)
            length += strlen(m_messages[i]);
        result.reserveCapacity(length);

        for (size_t i = 0; i < m_messages.size(); ++i) {
            if (i)
                result.append(Separator, strlen(Separator));
            result.append(m_messages[i], strlen(m_messages[i]));
        }

        return result.toString();
    }

    String toString(const char* message) const
    {
        ErrorContext stack(*this);
        stack.add(message);
        return stack.toString();
    }

    String toString(const char* message1, const char* message2) const
    {
        ErrorContext stack(*this);
        stack.add(message1);
        stack.add(message2);
        return stack.toString();
    }

private:
    // This inline size is large enough to avoid having to grow the Vector in
    // the majority of cases (up to 1 nested algorithm identifier).
    Vector<const char*, 10> m_messages;
};

// Defined by the WebCrypto spec as:
//
//     typedef (ArrayBuffer or ArrayBufferView) CryptoOperationData;
//
// FIXME: Currently only supports ArrayBufferView.
bool getOptionalCryptoOperationData(const Dictionary& raw, const char* propertyName, bool& hasProperty, RefPtr<ArrayBufferView>& buffer, const ErrorContext& context, String& errorDetails)
{
    if (!raw.get(propertyName, buffer)) {
        hasProperty = false;
        return true;
    }

    hasProperty = true;

    if (!buffer) {
        errorDetails = context.toString(propertyName, "Not an ArrayBufferView");
        return false;
    }

    return true;
}

// Defined by the WebCrypto spec as:
//
//     typedef (ArrayBuffer or ArrayBufferView) CryptoOperationData;
//
// FIXME: Currently only supports ArrayBufferView.
bool getCryptoOperationData(const Dictionary& raw, const char* propertyName, RefPtr<ArrayBufferView>& buffer, const ErrorContext& context, String& errorDetails)
{
    bool hasProperty;
    bool ok = getOptionalCryptoOperationData(raw, propertyName, hasProperty, buffer, context, errorDetails);
    if (!hasProperty) {
        errorDetails = context.toString(propertyName, "Missing required property");
        return false;
    }
    return ok;
}

bool getUint8Array(const Dictionary& raw, const char* propertyName, RefPtr<Uint8Array>& array, const ErrorContext& context, String& errorDetails)
{
    if (!raw.get(propertyName, array) || !array) {
        errorDetails = context.toString(propertyName, "Missing or not a Uint8Array");
        return false;
    }
    return true;
}

// Defined by the WebCrypto spec as:
//
//     typedef Uint8Array BigInteger;
bool getBigInteger(const Dictionary& raw, const char* propertyName, RefPtr<Uint8Array>& array, const ErrorContext& context, String& errorDetails)
{
    if (!getUint8Array(raw, propertyName, array, context, errorDetails))
        return false;

    if (!array->byteLength()) {
        errorDetails = context.toString(propertyName, "BigInteger should not be empty");
        return false;
    }

    if (!raw.get(propertyName, array) || !array) {
        errorDetails = context.toString(propertyName, "Missing or not a Uint8Array");
        return false;
    }
    return true;
}

// Gets an integer according to WebIDL's [EnforceRange].
bool getOptionalInteger(const Dictionary& raw, const char* propertyName, bool& hasProperty, double& value, double minValue, double maxValue, const ErrorContext& context, String& errorDetails)
{
    double number;
    bool ok = raw.get(propertyName, number, hasProperty);

    if (!hasProperty)
        return true;

    if (!ok || std::isnan(number)) {
        errorDetails = context.toString(propertyName, "Is not a number");
        return false;
    }

    number = trunc(number);

    if (std::isinf(number) || number < minValue || number > maxValue) {
        errorDetails = context.toString(propertyName, "Outside of numeric range");
        return false;
    }

    value = number;
    return true;
}

bool getInteger(const Dictionary& raw, const char* propertyName, double& value, double minValue, double maxValue, const ErrorContext& context, String& errorDetails)
{
    bool hasProperty;
    if (!getOptionalInteger(raw, propertyName, hasProperty, value, minValue, maxValue, context, errorDetails))
        return false;

    if (!hasProperty) {
        errorDetails = context.toString(propertyName, "Missing required property");
        return false;
    }

    return true;
}

bool getUint32(const Dictionary& raw, const char* propertyName, uint32_t& value, const ErrorContext& context, String& errorDetails)
{
    double number;
    if (!getInteger(raw, propertyName, number, 0, 0xFFFFFFFF, context, errorDetails))
        return false;
    value = number;
    return true;
}

bool getUint16(const Dictionary& raw, const char* propertyName, uint16_t& value, const ErrorContext& context, String& errorDetails)
{
    double number;
    if (!getInteger(raw, propertyName, number, 0, 0xFFFF, context, errorDetails))
        return false;
    value = number;
    return true;
}

bool getUint8(const Dictionary& raw, const char* propertyName, uint8_t& value, const ErrorContext& context, String& errorDetails)
{
    double number;
    if (!getInteger(raw, propertyName, number, 0, 0xFF, context, errorDetails))
        return false;
    value = number;
    return true;
}

bool getOptionalUint32(const Dictionary& raw, const char* propertyName, bool& hasValue, uint32_t& value, const ErrorContext& context, String& errorDetails)
{
    double number;
    if (!getOptionalInteger(raw, propertyName, hasValue, number, 0, 0xFFFFFFFF, context, errorDetails))
        return false;
    if (hasValue)
        value = number;
    return true;
}

// Defined by the WebCrypto spec as:
//
//    dictionary AesCbcParams : Algorithm {
//      CryptoOperationData iv;
//    };
bool parseAesCbcParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, String& errorDetails)
{
    RefPtr<ArrayBufferView> iv;
    if (!getCryptoOperationData(raw, "iv", iv, context, errorDetails))
        return false;

    if (iv->byteLength() != 16) {
        errorDetails = context.toString("iv", "Must be 16 bytes");
        return false;
    }

    params = adoptPtr(new blink::WebCryptoAesCbcParams(static_cast<unsigned char*>(iv->baseAddress()), iv->byteLength()));
    return true;
}

// Defined by the WebCrypto spec as:
//
//    dictionary AesKeyGenParams : Algorithm {
//      [EnforceRange] unsigned short length;
//    };
bool parseAesKeyGenParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, String& errorDetails)
{
    uint16_t length;
    if (!getUint16(raw, "length", length, context, errorDetails))
        return false;

    params = adoptPtr(new blink::WebCryptoAesKeyGenParams(length));
    return true;
}

bool parseAlgorithm(const Dictionary&, AlgorithmOperation, blink::WebCryptoAlgorithm&, ErrorContext, String&);

bool parseHash(const Dictionary& raw, blink::WebCryptoAlgorithm& hash, ErrorContext context, String& errorDetails)
{
    Dictionary rawHash;
    if (!raw.get("hash", rawHash)) {
        errorDetails = context.toString("hash", "Missing or not a dictionary");
        return false;
    }

    context.add("hash");
    return parseAlgorithm(rawHash, Digest, hash, context, errorDetails);
}

// Defined by the WebCrypto spec as:
//
//    dictionary HmacImportParams : Algorithm {
//      AlgorithmIdentifier hash;
//    };
bool parseHmacImportParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, String& errorDetails)
{
    blink::WebCryptoAlgorithm hash;
    if (!parseHash(raw, hash, context, errorDetails))
        return false;

    params = adoptPtr(new blink::WebCryptoHmacImportParams(hash));
    return true;
}

// Defined by the WebCrypto spec as:
//
//    dictionary HmacKeyGenParams : Algorithm {
//      AlgorithmIdentifier hash;
//      // The length (in bits) of the key to generate. If unspecified, the
//      // recommended length will be used, which is the size of the associated hash function's block
//      // size.
//      unsigned long length;
//    };
bool parseHmacKeyGenParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, String& errorDetails)
{
    blink::WebCryptoAlgorithm hash;
    if (!parseHash(raw, hash, context, errorDetails))
        return false;

    bool hasLength;
    uint32_t length = 0;
    if (!getOptionalUint32(raw, "length", hasLength, length, context, errorDetails))
        return false;

    params = adoptPtr(new blink::WebCryptoHmacKeyGenParams(hash, hasLength, length));
    return true;
}

// Defined by the WebCrypto spec as:
//
//    dictionary RsaHashedImportParams {
//      AlgorithmIdentifier hash;
//    };
bool parseRsaHashedImportParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, String& errorDetails)
{
    blink::WebCryptoAlgorithm hash;
    if (!parseHash(raw, hash, context, errorDetails))
        return false;

    params = adoptPtr(new blink::WebCryptoRsaHashedImportParams(hash));
    return true;
}

// Defined by the WebCrypto spec as:
//
//    dictionary RsaKeyGenParams : Algorithm {
//      unsigned long modulusLength;
//      BigInteger publicExponent;
//    };
bool parseRsaKeyGenParams(const Dictionary& raw, uint32_t& modulusLength, RefPtr<Uint8Array>& publicExponent, const ErrorContext& context, String& errorDetails)
{
    if (!getUint32(raw, "modulusLength", modulusLength, context, errorDetails))
        return false;

    if (!getBigInteger(raw, "publicExponent", publicExponent, context, errorDetails))
        return false;

    return true;
}

bool parseRsaKeyGenParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, String& errorDetails)
{
    uint32_t modulusLength;
    RefPtr<Uint8Array> publicExponent;
    if (!parseRsaKeyGenParams(raw, modulusLength, publicExponent, context, errorDetails))
        return false;

    params = adoptPtr(new blink::WebCryptoRsaKeyGenParams(modulusLength, static_cast<const unsigned char*>(publicExponent->baseAddress()), publicExponent->byteLength()));
    return true;
}

// Defined by the WebCrypto spec as:
//
//    dictionary RsaHashedKeyGenParams : RsaKeyGenParams {
//      AlgorithmIdentifier hash;
//    };
bool parseRsaHashedKeyGenParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, String& errorDetails)
{
    uint32_t modulusLength;
    RefPtr<Uint8Array> publicExponent;
    if (!parseRsaKeyGenParams(raw, modulusLength, publicExponent, context, errorDetails))
        return false;

    blink::WebCryptoAlgorithm hash;
    if (!parseHash(raw, hash, context, errorDetails))
        return false;

    params = adoptPtr(new blink::WebCryptoRsaHashedKeyGenParams(hash, modulusLength, static_cast<const unsigned char*>(publicExponent->baseAddress()), publicExponent->byteLength()));
    return true;
}

// Defined by the WebCrypto spec as:
//
//    dictionary AesCtrParams : Algorithm {
//      CryptoOperationData counter;
//      [EnforceRange] octet length;
//    };
bool parseAesCtrParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, String& errorDetails)
{
    RefPtr<ArrayBufferView> counter;
    if (!getCryptoOperationData(raw, "counter", counter, context, errorDetails))
        return false;

    uint8_t length;
    if (!getUint8(raw, "length", length, context, errorDetails))
        return false;

    params = adoptPtr(new blink::WebCryptoAesCtrParams(length, static_cast<const unsigned char*>(counter->baseAddress()), counter->byteLength()));
    return true;
}

// Defined by the WebCrypto spec as:
//
//     dictionary AesGcmParams : Algorithm {
//       CryptoOperationData iv;
//       CryptoOperationData? additionalData;
//       [EnforceRange] octet? tagLength;  // May be 0-128
//     }
bool parseAesGcmParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, String& errorDetails)
{
    RefPtr<ArrayBufferView> iv;
    if (!getCryptoOperationData(raw, "iv", iv, context, errorDetails))
        return false;

    bool hasAdditionalData;
    RefPtr<ArrayBufferView> additionalData;
    if (!getOptionalCryptoOperationData(raw, "additionalData", hasAdditionalData, additionalData, context, errorDetails))
        return false;

    double tagLength;
    bool hasTagLength;
    if (!getOptionalInteger(raw, "tagLength", hasTagLength, tagLength, 0, 128, context, errorDetails))
        return false;

    const unsigned char* ivStart = static_cast<const unsigned char*>(iv->baseAddress());
    unsigned ivLength = iv->byteLength();

    const unsigned char* additionalDataStart = hasAdditionalData ? static_cast<const unsigned char*>(additionalData->baseAddress()) : 0;
    unsigned additionalDataLength = hasAdditionalData ? additionalData->byteLength() : 0;

    params = adoptPtr(new blink::WebCryptoAesGcmParams(ivStart, ivLength, hasAdditionalData, additionalDataStart, additionalDataLength, hasTagLength, tagLength));
    return true;
}

bool parseAlgorithmParams(const Dictionary& raw, blink::WebCryptoAlgorithmParamsType type, OwnPtr<blink::WebCryptoAlgorithmParams>& params, ErrorContext& context, String& errorDetails)
{
    switch (type) {
    case blink::WebCryptoAlgorithmParamsTypeNone:
        return true;
    case blink::WebCryptoAlgorithmParamsTypeAesCbcParams:
        context.add("AesCbcParams");
        return parseAesCbcParams(raw, params, context, errorDetails);
    case blink::WebCryptoAlgorithmParamsTypeAesKeyGenParams:
        context.add("AesKeyGenParams");
        return parseAesKeyGenParams(raw, params, context, errorDetails);
    case blink::WebCryptoAlgorithmParamsTypeHmacImportParams:
        context.add("HmacImportParams");
        return parseHmacImportParams(raw, params, context, errorDetails);
    case blink::WebCryptoAlgorithmParamsTypeHmacKeyGenParams:
        context.add("HmacKeyGenParams");
        return parseHmacKeyGenParams(raw, params, context, errorDetails);
    case blink::WebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams:
        context.add("RsaHashedKeyGenParams");
        return parseRsaHashedKeyGenParams(raw, params, context, errorDetails);
    case blink::WebCryptoAlgorithmParamsTypeRsaHashedImportParams:
        context.add("RsaHashedImportParams");
        return parseRsaHashedImportParams(raw, params, context, errorDetails);
    case blink::WebCryptoAlgorithmParamsTypeRsaKeyGenParams:
        context.add("RsaKeyGenParams");
        return parseRsaKeyGenParams(raw, params, context, errorDetails);
    case blink::WebCryptoAlgorithmParamsTypeAesCtrParams:
        context.add("AesCtrParams");
        return parseAesCtrParams(raw, params, context, errorDetails);
    case blink::WebCryptoAlgorithmParamsTypeAesGcmParams:
        context.add("AesGcmParams");
        return parseAesGcmParams(raw, params, context, errorDetails);
    case blink::WebCryptoAlgorithmParamsTypeRsaOaepParams:
        // TODO
        notImplemented();
        break;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool parseAlgorithm(const Dictionary& raw, AlgorithmOperation op, blink::WebCryptoAlgorithm& algorithm, ErrorContext context, String& errorDetails)
{
    context.add("Algorithm");

    if (!raw.isObject()) {
        errorDetails = context.toString("Not an object");
        return false;
    }

    String algorithmName;
    if (!raw.get("name", algorithmName)) {
        errorDetails = context.toString("name", "Missing or not a string");
        return false;
    }

    blink::WebCryptoAlgorithmId algorithmId;
    if (!lookupAlgorithmIdByName(algorithmName, algorithmId)) {
        errorDetails = context.toString("Unrecognized algorithm name");
        return false;
    }

    context.add(algorithmIdToName(algorithmId));

    blink::WebCryptoAlgorithmParamsType paramsType;
    if (!lookupAlgorithmParamsType(algorithmId, op, paramsType)) {
        errorDetails = context.toString("Unsupported operation");
        return false;
    }

    OwnPtr<blink::WebCryptoAlgorithmParams> params;
    if (!parseAlgorithmParams(raw, paramsType, params, context, errorDetails))
        return false;

    algorithm = blink::WebCryptoAlgorithm(algorithmId, params.release());
    return true;
}

} // namespace

bool parseAlgorithm(const Dictionary& raw, AlgorithmOperation op, blink::WebCryptoAlgorithm& algorithm, CryptoResult* result)
{
    String errorDetails;
    if (!parseAlgorithm(raw, op, algorithm, ErrorContext(), errorDetails)) {
        result->completeWithError(errorDetails);
        return false;
    }
    return true;
}

const char* algorithmIdToName(blink::WebCryptoAlgorithmId id)
{
    switch (id) {
    case blink::WebCryptoAlgorithmIdAesCbc:
        return "AES-CBC";
    case blink::WebCryptoAlgorithmIdAesCtr:
        return "AES-CTR";
    case blink::WebCryptoAlgorithmIdAesGcm:
        return "AES-GCM";
    case blink::WebCryptoAlgorithmIdHmac:
        return "HMAC";
    case blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
        return "RSASSA-PKCS1-v1_5";
    case blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5:
        return "RSAES-PKCS1-v1_5";
    case blink::WebCryptoAlgorithmIdSha1:
        return "SHA-1";
    case blink::WebCryptoAlgorithmIdSha256:
        return "SHA-256";
    case blink::WebCryptoAlgorithmIdSha384:
        return "SHA-384";
    case blink::WebCryptoAlgorithmIdSha512:
        return "SHA-512";
    case blink::WebCryptoAlgorithmIdAesKw:
        return "AES-KW";
    case blink::WebCryptoAlgorithmIdRsaOaep:
        return "RSA-OAEP";
    }
    return 0;
}

} // namespace WebCore
