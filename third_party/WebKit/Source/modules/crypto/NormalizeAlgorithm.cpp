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
    {"RSA-OAEP", 8, blink::WebCryptoAlgorithmIdRsaOaep},
    {"RSAES-PKCS1-V1_5", 16, blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5},
    {"RSASSA-PKCS1-V1_5", 17, blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5},
};

typedef char ParamsTypeOrUndefined;
const ParamsTypeOrUndefined Undefined = -1;

struct AlgorithmInfo {
    // The canonical (case-sensitive) name for the algorithm.
    const char* name;

    // A map from the operation to the expected parameter type of the algorithm.
    // If an operation is not applicable for the algorithm, set to Undefined.
    const ParamsTypeOrUndefined operationToParamsType[LastAlgorithmOperation + 1];
};

// A mapping from the algorithm ID to information about the algorithm.
const AlgorithmInfo algorithmIdToInfo[] = {
    { // Index 0
        "AES-CBC", {
            blink::WebCryptoAlgorithmParamsTypeAesCbcParams, // Encrypt
            blink::WebCryptoAlgorithmParamsTypeAesCbcParams, // Decrypt
            Undefined, // Sign
            Undefined, // Verify
            Undefined, // Digest
            blink::WebCryptoAlgorithmParamsTypeAesKeyGenParams, // GenerateKey
            blink::WebCryptoAlgorithmParamsTypeNone, // ImportKey
            Undefined, // DeriveKey
            Undefined, // DeriveBits
            blink::WebCryptoAlgorithmParamsTypeAesCbcParams, // WrapKey
            blink::WebCryptoAlgorithmParamsTypeAesCbcParams // UnwrapKey
        }
    }, { // Index 1
        "HMAC", {
            Undefined, // Encrypt
            Undefined, // Decrypt
            blink::WebCryptoAlgorithmParamsTypeNone, // Sign
            blink::WebCryptoAlgorithmParamsTypeNone, // Verify
            Undefined, // Digest
            blink::WebCryptoAlgorithmParamsTypeHmacKeyGenParams, // GenerateKey
            blink::WebCryptoAlgorithmParamsTypeHmacImportParams, // ImportKey
            Undefined, // DeriveKey
            Undefined, // DeriveBits
            Undefined, // WrapKey
            Undefined // UnwrapKey
        }
    }, { // Index 2
        "RSASSA-PKCS1-v1_5", {
            Undefined, // Encrypt
            Undefined, // Decrypt
            blink::WebCryptoAlgorithmParamsTypeNone, // Sign
            blink::WebCryptoAlgorithmParamsTypeNone, // Verify
            Undefined, // Digest
            blink::WebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams, // GenerateKey
            blink::WebCryptoAlgorithmParamsTypeRsaHashedImportParams, // ImportKey
            Undefined, // DeriveKey
            Undefined, // DeriveBits
            Undefined, // WrapKey
            Undefined // UnwrapKey
        }
    }, { // Index 3
        "RSAES-PKCS1-v1_5", {
            blink::WebCryptoAlgorithmParamsTypeNone, // Encrypt
            blink::WebCryptoAlgorithmParamsTypeNone, // Decrypt
            Undefined, // Sign
            Undefined, // Verify
            Undefined, // Digest
            blink::WebCryptoAlgorithmParamsTypeRsaKeyGenParams, // GenerateKey
            blink::WebCryptoAlgorithmParamsTypeNone, // ImportKey
            Undefined, // DeriveKey
            Undefined, // DeriveBits
            blink::WebCryptoAlgorithmParamsTypeNone, // WrapKey
            blink::WebCryptoAlgorithmParamsTypeNone // UnwrapKey
        }
    }, { // Index 4
        "SHA-1", {
            Undefined, // Encrypt
            Undefined, // Decrypt
            Undefined, // Sign
            Undefined, // Verify
            blink::WebCryptoAlgorithmParamsTypeNone, // Digest
            Undefined, // GenerateKey
            Undefined, // ImportKey
            Undefined, // DeriveKey
            Undefined, // DeriveBits
            Undefined, // WrapKey
            Undefined // UnwrapKey
        }
    }, { // Index 5
        "SHA-256", {
            Undefined, // Encrypt
            Undefined, // Decrypt
            Undefined, // Sign
            Undefined, // Verify
            blink::WebCryptoAlgorithmParamsTypeNone, // Digest
            Undefined, // GenerateKey
            Undefined, // ImportKey
            Undefined, // DeriveKey
            Undefined, // DeriveBits
            Undefined, // WrapKey
            Undefined // UnwrapKey
        }
    }, { // Index 6
        "SHA-384", {
            Undefined, // Encrypt
            Undefined, // Decrypt
            Undefined, // Sign
            Undefined, // Verify
            blink::WebCryptoAlgorithmParamsTypeNone, // Digest
            Undefined, // GenerateKey
            Undefined, // ImportKey
            Undefined, // DeriveKey
            Undefined, // DeriveBits
            Undefined, // WrapKey
            Undefined // UnwrapKey
        }
    }, { // Index 7
        "SHA-512", {
            Undefined, // Encrypt
            Undefined, // Decrypt
            Undefined, // Sign
            Undefined, // Verify
            blink::WebCryptoAlgorithmParamsTypeNone, // Digest
            Undefined, // GenerateKey
            Undefined, // ImportKey
            Undefined, // DeriveKey
            Undefined, // DeriveBits
            Undefined, // WrapKey
            Undefined // UnwrapKey
        }
    }, { // Index 8
        "AES-GCM", {
            blink::WebCryptoAlgorithmParamsTypeAesGcmParams, // Encrypt
            blink::WebCryptoAlgorithmParamsTypeAesGcmParams, // Decrypt
            Undefined, // Sign
            Undefined, // Verify
            Undefined, // Digest
            blink::WebCryptoAlgorithmParamsTypeAesKeyGenParams, // GenerateKey
            blink::WebCryptoAlgorithmParamsTypeNone, // ImportKey
            Undefined, // DeriveKey
            Undefined, // DeriveBits
            blink::WebCryptoAlgorithmParamsTypeAesGcmParams, // WrapKey
            blink::WebCryptoAlgorithmParamsTypeAesGcmParams // UnwrapKey
        }
    }, { // Index 9
        "RSA-OAEP", {
            blink::WebCryptoAlgorithmParamsTypeRsaOaepParams, // Encrypt
            blink::WebCryptoAlgorithmParamsTypeRsaOaepParams, // Decrypt
            Undefined, // Sign
            Undefined, // Verify
            Undefined, // Digest
            blink::WebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams, // GenerateKey
            blink::WebCryptoAlgorithmParamsTypeRsaHashedImportParams, // ImportKey
            Undefined, // DeriveKey
            Undefined, // DeriveBits
            blink::WebCryptoAlgorithmParamsTypeRsaOaepParams, // WrapKey
            blink::WebCryptoAlgorithmParamsTypeRsaOaepParams // UnwrapKey
        }
    }, { // Index 10
        "AES-CTR", {
            blink::WebCryptoAlgorithmParamsTypeAesCtrParams, // Encrypt
            blink::WebCryptoAlgorithmParamsTypeAesCtrParams, // Decrypt
            Undefined, // Sign
            Undefined, // Verify
            Undefined, // Digest
            blink::WebCryptoAlgorithmParamsTypeAesKeyGenParams, // GenerateKey
            blink::WebCryptoAlgorithmParamsTypeNone, // ImportKey
            Undefined, // DeriveKey
            Undefined, // DeriveBits
            blink::WebCryptoAlgorithmParamsTypeAesCtrParams, // WrapKey
            blink::WebCryptoAlgorithmParamsTypeAesCtrParams // UnwrapKey
        }
    }, { // Index 11
        "AES-KW", {
            Undefined, // Encrypt
            Undefined, // Decrypt
            Undefined, // Sign
            Undefined, // Verify
            Undefined, // Digest
            blink::WebCryptoAlgorithmParamsTypeAesKeyGenParams, // GenerateKey
            blink::WebCryptoAlgorithmParamsTypeNone, // ImportKey
            Undefined, // DeriveKey
            Undefined, // DeriveBits
            blink::WebCryptoAlgorithmParamsTypeNone, // WrapKey
            blink::WebCryptoAlgorithmParamsTypeNone // UnwrapKey
        }
    },
};

// Initializing the algorithmIdToInfo table above depends on knowing the enum
// values for algorithm IDs. If those ever change, the table will need to be
// updated.
COMPILE_ASSERT(blink::WebCryptoAlgorithmIdAesCbc == 0, AesCbc_idDoesntMatch);
COMPILE_ASSERT(blink::WebCryptoAlgorithmIdHmac == 1, Hmac_idDoesntMatch);
COMPILE_ASSERT(blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5 == 2, RsaSsaPkcs1v1_5_idDoesntMatch);
COMPILE_ASSERT(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5 == 3, RsaEsPkcs1v1_5_idDoesntMatch);
COMPILE_ASSERT(blink::WebCryptoAlgorithmIdSha1 == 4, Sha1_idDoesntMatch);
COMPILE_ASSERT(blink::WebCryptoAlgorithmIdSha256 == 5, Sha256_idDoesntMatch);
COMPILE_ASSERT(blink::WebCryptoAlgorithmIdSha384 == 6, Sha384_idDoesntMatch);
COMPILE_ASSERT(blink::WebCryptoAlgorithmIdSha512 == 7, Sha512_idDoesntMatch);
COMPILE_ASSERT(blink::WebCryptoAlgorithmIdAesGcm == 8, AesGcm_idDoesntMatch);
COMPILE_ASSERT(blink::WebCryptoAlgorithmIdRsaOaep == 9, RsaOaep_idDoesntMatch);
COMPILE_ASSERT(blink::WebCryptoAlgorithmIdAesCtr == 10, AesCtr_idDoesntMatch);
COMPILE_ASSERT(blink::WebCryptoAlgorithmIdAesKw == 11, AesKw_idDoesntMatch);
COMPILE_ASSERT(blink::WebCryptoAlgorithmIdLast == 11, Last_idDoesntMatch);
COMPILE_ASSERT(10 == LastAlgorithmOperation, UpdateParamsMapping);

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

const AlgorithmInfo* lookupAlgorithmInfo(blink::WebCryptoAlgorithmId id)
{
    if (id < 0 || id >= WTF_ARRAY_LENGTH(algorithmIdToInfo))
        return 0;
    return &algorithmIdToInfo[id];
}

void completeWithSyntaxError(const String& message, CryptoResult* result)
{
    result->completeWithError(blink::WebCryptoErrorTypeSyntax, message);
}

void completeWithNotSupportedError(const String& message, CryptoResult* result)
{
    result->completeWithError(blink::WebCryptoErrorTypeNotSupported, message);
}

void completeWithDataError(const String& message, CryptoResult* result)
{
    result->completeWithError(blink::WebCryptoErrorTypeData, message);
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

    void removeLast()
    {
        m_messages.removeLast();
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
bool getOptionalCryptoOperationData(const Dictionary& raw, const char* propertyName, bool& hasProperty, RefPtr<ArrayBufferView>& buffer, const ErrorContext& context, CryptoResult* result)
{
    if (!raw.get(propertyName, buffer)) {
        hasProperty = false;
        return true;
    }

    hasProperty = true;

    if (!buffer) {
        completeWithSyntaxError(context.toString(propertyName, "Not an ArrayBufferView"), result);
        return false;
    }

    return true;
}

// Defined by the WebCrypto spec as:
//
//     typedef (ArrayBuffer or ArrayBufferView) CryptoOperationData;
//
// FIXME: Currently only supports ArrayBufferView.
bool getCryptoOperationData(const Dictionary& raw, const char* propertyName, RefPtr<ArrayBufferView>& buffer, const ErrorContext& context, CryptoResult* result)
{
    bool hasProperty;
    bool ok = getOptionalCryptoOperationData(raw, propertyName, hasProperty, buffer, context, result);
    if (!hasProperty) {
        completeWithSyntaxError(context.toString(propertyName, "Missing required property"), result);
        return false;
    }
    return ok;
}

bool getUint8Array(const Dictionary& raw, const char* propertyName, RefPtr<Uint8Array>& array, const ErrorContext& context, CryptoResult* result)
{
    if (!raw.get(propertyName, array) || !array) {
        completeWithSyntaxError(context.toString(propertyName, "Missing or not a Uint8Array"), result);
        return false;
    }
    return true;
}

// Defined by the WebCrypto spec as:
//
//     typedef Uint8Array BigInteger;
bool getBigInteger(const Dictionary& raw, const char* propertyName, RefPtr<Uint8Array>& array, const ErrorContext& context, CryptoResult* result)
{
    if (!getUint8Array(raw, propertyName, array, context, result))
        return false;

    if (!array->byteLength()) {
        completeWithSyntaxError(context.toString(propertyName, "BigInteger should not be empty"), result);
        return false;
    }

    if (!raw.get(propertyName, array) || !array) {
        completeWithSyntaxError(context.toString(propertyName, "Missing or not a Uint8Array"), result);
        return false;
    }
    return true;
}

// Gets an integer according to WebIDL's [EnforceRange].
bool getOptionalInteger(const Dictionary& raw, const char* propertyName, bool& hasProperty, double& value, double minValue, double maxValue, const ErrorContext& context, CryptoResult* result)
{
    double number;
    bool ok = raw.get(propertyName, number, hasProperty);

    if (!hasProperty)
        return true;

    if (!ok || std::isnan(number)) {
        completeWithSyntaxError(context.toString(propertyName, "Is not a number"), result);
        return false;
    }

    number = trunc(number);

    if (std::isinf(number) || number < minValue || number > maxValue) {
        completeWithSyntaxError(context.toString(propertyName, "Outside of numeric range"), result);
        return false;
    }

    value = number;
    return true;
}

bool getInteger(const Dictionary& raw, const char* propertyName, double& value, double minValue, double maxValue, const ErrorContext& context, CryptoResult* result)
{
    bool hasProperty;
    if (!getOptionalInteger(raw, propertyName, hasProperty, value, minValue, maxValue, context, result))
        return false;

    if (!hasProperty) {
        completeWithSyntaxError(context.toString(propertyName, "Missing required property"), result);
        return false;
    }

    return true;
}

bool getUint32(const Dictionary& raw, const char* propertyName, uint32_t& value, const ErrorContext& context, CryptoResult* result)
{
    double number;
    if (!getInteger(raw, propertyName, number, 0, 0xFFFFFFFF, context, result))
        return false;
    value = number;
    return true;
}

bool getUint16(const Dictionary& raw, const char* propertyName, uint16_t& value, const ErrorContext& context, CryptoResult* result)
{
    double number;
    if (!getInteger(raw, propertyName, number, 0, 0xFFFF, context, result))
        return false;
    value = number;
    return true;
}

bool getUint8(const Dictionary& raw, const char* propertyName, uint8_t& value, const ErrorContext& context, CryptoResult* result)
{
    double number;
    if (!getInteger(raw, propertyName, number, 0, 0xFF, context, result))
        return false;
    value = number;
    return true;
}

bool getOptionalUint32(const Dictionary& raw, const char* propertyName, bool& hasValue, uint32_t& value, const ErrorContext& context, CryptoResult* result)
{
    double number;
    if (!getOptionalInteger(raw, propertyName, hasValue, number, 0, 0xFFFFFFFF, context, result))
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
bool parseAesCbcParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, CryptoResult* result)
{
    RefPtr<ArrayBufferView> iv;
    if (!getCryptoOperationData(raw, "iv", iv, context, result))
        return false;

    if (iv->byteLength() != 16) {
        completeWithDataError(context.toString("iv", "Must be 16 bytes"), result);
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
bool parseAesKeyGenParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, CryptoResult* result)
{
    uint16_t length;
    if (!getUint16(raw, "length", length, context, result))
        return false;

    params = adoptPtr(new blink::WebCryptoAesKeyGenParams(length));
    return true;
}

bool parseAlgorithm(const Dictionary&, AlgorithmOperation, blink::WebCryptoAlgorithm&, ErrorContext, CryptoResult*);

bool parseHash(const Dictionary& raw, blink::WebCryptoAlgorithm& hash, ErrorContext context, CryptoResult* result)
{
    Dictionary rawHash;
    if (!raw.get("hash", rawHash)) {
        completeWithSyntaxError(context.toString("hash", "Missing or not a dictionary"), result);
        return false;
    }

    context.add("hash");
    return parseAlgorithm(rawHash, Digest, hash, context, result);
}

// Defined by the WebCrypto spec as:
//
//    dictionary HmacImportParams : Algorithm {
//      AlgorithmIdentifier hash;
//    };
bool parseHmacImportParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, CryptoResult* result)
{
    blink::WebCryptoAlgorithm hash;
    if (!parseHash(raw, hash, context, result))
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
bool parseHmacKeyGenParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, CryptoResult* result)
{
    blink::WebCryptoAlgorithm hash;
    if (!parseHash(raw, hash, context, result))
        return false;

    bool hasLength;
    uint32_t length = 0;
    if (!getOptionalUint32(raw, "length", hasLength, length, context, result))
        return false;

    params = adoptPtr(new blink::WebCryptoHmacKeyGenParams(hash, hasLength, length));
    return true;
}

// Defined by the WebCrypto spec as:
//
//    dictionary RsaHashedImportParams {
//      AlgorithmIdentifier hash;
//    };
bool parseRsaHashedImportParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, CryptoResult* result)
{
    blink::WebCryptoAlgorithm hash;
    if (!parseHash(raw, hash, context, result))
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
bool parseRsaKeyGenParams(const Dictionary& raw, uint32_t& modulusLength, RefPtr<Uint8Array>& publicExponent, const ErrorContext& context, CryptoResult* result)
{
    if (!getUint32(raw, "modulusLength", modulusLength, context, result))
        return false;

    if (!getBigInteger(raw, "publicExponent", publicExponent, context, result))
        return false;

    return true;
}

bool parseRsaKeyGenParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, CryptoResult* result)
{
    uint32_t modulusLength;
    RefPtr<Uint8Array> publicExponent;
    if (!parseRsaKeyGenParams(raw, modulusLength, publicExponent, context, result))
        return false;

    params = adoptPtr(new blink::WebCryptoRsaKeyGenParams(modulusLength, static_cast<const unsigned char*>(publicExponent->baseAddress()), publicExponent->byteLength()));
    return true;
}

// Defined by the WebCrypto spec as:
//
//    dictionary RsaHashedKeyGenParams : RsaKeyGenParams {
//      AlgorithmIdentifier hash;
//    };
bool parseRsaHashedKeyGenParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, CryptoResult* result)
{
    uint32_t modulusLength;
    RefPtr<Uint8Array> publicExponent;
    if (!parseRsaKeyGenParams(raw, modulusLength, publicExponent, context, result))
        return false;

    blink::WebCryptoAlgorithm hash;
    if (!parseHash(raw, hash, context, result))
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
bool parseAesCtrParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, CryptoResult* result)
{
    RefPtr<ArrayBufferView> counter;
    if (!getCryptoOperationData(raw, "counter", counter, context, result))
        return false;

    uint8_t length;
    if (!getUint8(raw, "length", length, context, result))
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
bool parseAesGcmParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, CryptoResult* result)
{
    RefPtr<ArrayBufferView> iv;
    if (!getCryptoOperationData(raw, "iv", iv, context, result))
        return false;

    bool hasAdditionalData;
    RefPtr<ArrayBufferView> additionalData;
    if (!getOptionalCryptoOperationData(raw, "additionalData", hasAdditionalData, additionalData, context, result))
        return false;

    double tagLength;
    bool hasTagLength;
    if (!getOptionalInteger(raw, "tagLength", hasTagLength, tagLength, 0, 128, context, result))
        return false;

    const unsigned char* ivStart = static_cast<const unsigned char*>(iv->baseAddress());
    unsigned ivLength = iv->byteLength();

    const unsigned char* additionalDataStart = hasAdditionalData ? static_cast<const unsigned char*>(additionalData->baseAddress()) : 0;
    unsigned additionalDataLength = hasAdditionalData ? additionalData->byteLength() : 0;

    params = adoptPtr(new blink::WebCryptoAesGcmParams(ivStart, ivLength, hasAdditionalData, additionalDataStart, additionalDataLength, hasTagLength, tagLength));
    return true;
}

// Defined by the WebCrypto spec as:
//
//     dictionary RsaOaepParams : Algorithm {
//       CryptoOperationData? label;
//     };
bool parseRsaOaepParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, CryptoResult* result)
{
    bool hasLabel;
    RefPtr<ArrayBufferView> label;
    if (!getOptionalCryptoOperationData(raw, "label", hasLabel, label, context, result))
        return false;

    const unsigned char* labelStart = hasLabel ? static_cast<const unsigned char*>(label->baseAddress()) : 0;
    unsigned labelLength = hasLabel ? label->byteLength() : 0;

    params = adoptPtr(new blink::WebCryptoRsaOaepParams(hasLabel, labelStart, labelLength));
    return true;
}

bool parseAlgorithmParams(const Dictionary& raw, blink::WebCryptoAlgorithmParamsType type, OwnPtr<blink::WebCryptoAlgorithmParams>& params, ErrorContext& context, CryptoResult* result)
{
    switch (type) {
    case blink::WebCryptoAlgorithmParamsTypeNone:
        return true;
    case blink::WebCryptoAlgorithmParamsTypeAesCbcParams:
        context.add("AesCbcParams");
        return parseAesCbcParams(raw, params, context, result);
    case blink::WebCryptoAlgorithmParamsTypeAesKeyGenParams:
        context.add("AesKeyGenParams");
        return parseAesKeyGenParams(raw, params, context, result);
    case blink::WebCryptoAlgorithmParamsTypeHmacImportParams:
        context.add("HmacImportParams");
        return parseHmacImportParams(raw, params, context, result);
    case blink::WebCryptoAlgorithmParamsTypeHmacKeyGenParams:
        context.add("HmacKeyGenParams");
        return parseHmacKeyGenParams(raw, params, context, result);
    case blink::WebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams:
        context.add("RsaHashedKeyGenParams");
        return parseRsaHashedKeyGenParams(raw, params, context, result);
    case blink::WebCryptoAlgorithmParamsTypeRsaHashedImportParams:
        context.add("RsaHashedImportParams");
        return parseRsaHashedImportParams(raw, params, context, result);
    case blink::WebCryptoAlgorithmParamsTypeRsaKeyGenParams:
        context.add("RsaKeyGenParams");
        return parseRsaKeyGenParams(raw, params, context, result);
    case blink::WebCryptoAlgorithmParamsTypeAesCtrParams:
        context.add("AesCtrParams");
        return parseAesCtrParams(raw, params, context, result);
    case blink::WebCryptoAlgorithmParamsTypeAesGcmParams:
        context.add("AesGcmParams");
        return parseAesGcmParams(raw, params, context, result);
    case blink::WebCryptoAlgorithmParamsTypeRsaOaepParams:
        context.add("RsaOaepParams");
        return parseRsaOaepParams(raw, params, context, result);
        break;
    }
    ASSERT_NOT_REACHED();
    return false;
}

const char* operationToString(AlgorithmOperation op)
{
    switch (op) {
    case Encrypt:
        return "encrypt";
    case Decrypt:
        return "decrypt";
    case Sign:
        return "sign";
    case Verify:
        return "verify";
    case Digest:
        return "digest";
    case GenerateKey:
        return "generateKey";
    case ImportKey:
        return "importKey";
    case DeriveKey:
        return "deriveKey";
    case DeriveBits:
        return "deriveBits";
    case WrapKey:
        return "wrapKey";
    case UnwrapKey:
        return "unwrapKey";
    }
    return 0;
}

bool parseAlgorithm(const Dictionary& raw, AlgorithmOperation op, blink::WebCryptoAlgorithm& algorithm, ErrorContext context, CryptoResult* result)
{
    context.add("Algorithm");

    if (!raw.isObject()) {
        completeWithSyntaxError(context.toString("Not an object"), result);
        return false;
    }

    String algorithmName;
    if (!raw.get("name", algorithmName)) {
        completeWithSyntaxError(context.toString("name", "Missing or not a string"), result);
        return false;
    }

    blink::WebCryptoAlgorithmId algorithmId;
    if (!lookupAlgorithmIdByName(algorithmName, algorithmId)) {
        // FIXME: The spec says to return a SyntaxError if the input contains
        //        any non-ASCII characters.
        completeWithNotSupportedError(context.toString("Unrecognized name"), result);
        return false;
    }

    // Remove the "Algorithm:" prefix for all subsequent errors.
    context.removeLast();

    const AlgorithmInfo* algorithmInfo = lookupAlgorithmInfo(algorithmId);

    if (algorithmInfo->operationToParamsType[op] == Undefined) {
        context.add(algorithmIdToName(algorithmId));
        completeWithNotSupportedError(context.toString("Unsupported operation", operationToString(op)), result);
        return false;
    }

    blink::WebCryptoAlgorithmParamsType paramsType = static_cast<blink::WebCryptoAlgorithmParamsType>(algorithmInfo->operationToParamsType[op]);

    OwnPtr<blink::WebCryptoAlgorithmParams> params;
    if (!parseAlgorithmParams(raw, paramsType, params, context, result))
        return false;

    algorithm = blink::WebCryptoAlgorithm(algorithmId, params.release());
    return true;
}

} // namespace

bool parseAlgorithm(const Dictionary& raw, AlgorithmOperation op, blink::WebCryptoAlgorithm& algorithm, CryptoResult* result)
{
    return parseAlgorithm(raw, op, algorithm, ErrorContext(), result);
}

const char* algorithmIdToName(blink::WebCryptoAlgorithmId id)
{
    return lookupAlgorithmInfo(id)->name;
}

} // namespace WebCore
