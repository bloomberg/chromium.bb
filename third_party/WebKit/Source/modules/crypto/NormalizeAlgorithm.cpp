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
#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "platform/NotImplemented.h"
#include "public/platform/WebCryptoAlgorithm.h"
#include "public/platform/WebCryptoAlgorithmParams.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/ArrayBufferView.h"
#include "wtf/HashMap.h"
#include "wtf/MathExtras.h"
#include "wtf/Uint8Array.h"
#include "wtf/Vector.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringHash.h"

namespace WebCore {

namespace {

struct AlgorithmNameMapping {
    const char* const algorithmName;
    WebKit::WebCryptoAlgorithmId algorithmId;
};

// Indicates that the algorithm doesn't support the specified operation.
const int UnsupportedOp = -1;

// Either UnsupportedOp, or a value from WebKit::WebCryptoAlgorithmParamsType
typedef int AlgorithmParamsForOperation;

struct OperationParamsMapping {
    WebKit::WebCryptoAlgorithmId algorithmId;
    AlgorithmOperation operation;
    AlgorithmParamsForOperation params;
};

const AlgorithmNameMapping algorithmNameMappings[] = {
    {"AES-CBC", WebKit::WebCryptoAlgorithmIdAesCbc},
    {"HMAC", WebKit::WebCryptoAlgorithmIdHmac},
    {"RSASSA-PKCS1-v1_5", WebKit::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5},
    {"RSAES-PKCS1-v1_5", WebKit::WebCryptoAlgorithmIdRsaEsPkcs1v1_5},
    {"SHA-1", WebKit::WebCryptoAlgorithmIdSha1},
    {"SHA-224", WebKit::WebCryptoAlgorithmIdSha224},
    {"SHA-256", WebKit::WebCryptoAlgorithmIdSha256},
    {"SHA-384", WebKit::WebCryptoAlgorithmIdSha384},
    {"SHA-512", WebKit::WebCryptoAlgorithmIdSha512},
};

// What operations each algorithm supports, and what parameters it expects.
const OperationParamsMapping operationParamsMappings[] = {
    // AES-CBC
    {WebKit::WebCryptoAlgorithmIdAesCbc, Decrypt, WebKit::WebCryptoAlgorithmParamsTypeAesCbcParams},
    {WebKit::WebCryptoAlgorithmIdAesCbc, Encrypt, WebKit::WebCryptoAlgorithmParamsTypeAesCbcParams},
    {WebKit::WebCryptoAlgorithmIdAesCbc, GenerateKey, WebKit::WebCryptoAlgorithmParamsTypeAesKeyGenParams},
    {WebKit::WebCryptoAlgorithmIdAesCbc, ImportKey, WebKit::WebCryptoAlgorithmParamsTypeNone},

    // HMAC
    {WebKit::WebCryptoAlgorithmIdHmac, Sign, WebKit::WebCryptoAlgorithmParamsTypeHmacParams},
    {WebKit::WebCryptoAlgorithmIdHmac, Verify, WebKit::WebCryptoAlgorithmParamsTypeHmacParams},
    {WebKit::WebCryptoAlgorithmIdHmac, GenerateKey, WebKit::WebCryptoAlgorithmParamsTypeHmacKeyParams},
    {WebKit::WebCryptoAlgorithmIdHmac, ImportKey, WebKit::WebCryptoAlgorithmParamsTypeHmacParams},

    // RSASSA-PKCS1-v1_5
    {WebKit::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5, Sign, WebKit::WebCryptoAlgorithmParamsTypeRsaSsaParams},
    {WebKit::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5, Verify, WebKit::WebCryptoAlgorithmParamsTypeRsaSsaParams},
    {WebKit::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5, GenerateKey, WebKit::WebCryptoAlgorithmParamsTypeRsaKeyGenParams},
    {WebKit::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5, ImportKey, WebKit::WebCryptoAlgorithmParamsTypeNone},

    // RSAES-PKCS1-v1_5
    {WebKit::WebCryptoAlgorithmIdRsaEsPkcs1v1_5, Encrypt, WebKit::WebCryptoAlgorithmParamsTypeNone},
    {WebKit::WebCryptoAlgorithmIdRsaEsPkcs1v1_5, Decrypt, WebKit::WebCryptoAlgorithmParamsTypeNone},
    {WebKit::WebCryptoAlgorithmIdRsaEsPkcs1v1_5, GenerateKey, WebKit::WebCryptoAlgorithmParamsTypeRsaKeyGenParams},
    {WebKit::WebCryptoAlgorithmIdRsaEsPkcs1v1_5, ImportKey, WebKit::WebCryptoAlgorithmParamsTypeNone},

    // SHA-*
    {WebKit::WebCryptoAlgorithmIdSha1, Digest, WebKit::WebCryptoAlgorithmParamsTypeNone},
    {WebKit::WebCryptoAlgorithmIdSha224, Digest, WebKit::WebCryptoAlgorithmParamsTypeNone},
    {WebKit::WebCryptoAlgorithmIdSha256, Digest, WebKit::WebCryptoAlgorithmParamsTypeNone},
    {WebKit::WebCryptoAlgorithmIdSha384, Digest, WebKit::WebCryptoAlgorithmParamsTypeNone},
    {WebKit::WebCryptoAlgorithmIdSha512, Digest, WebKit::WebCryptoAlgorithmParamsTypeNone},
};

// This structure describes an algorithm and its supported operations.
struct AlgorithmInfo {
    AlgorithmInfo()
        : algorithmName(0)
    {
        for (size_t i = 0; i < WTF_ARRAY_LENGTH(paramsForOperation); ++i)
            paramsForOperation[i] = UnsupportedOp;
    }

    WebKit::WebCryptoAlgorithmId algorithmId;
    const char* algorithmName;
    AlgorithmParamsForOperation paramsForOperation[NumberOfAlgorithmOperations];
};

// AlgorithmRegistry enumerates each of the different algorithms and its
// parameters. This describes the same information as the static tables above,
// but in a more convenient runtime form.
class AlgorithmRegistry {
public:
    static AlgorithmRegistry& instance();

    const AlgorithmInfo* lookupAlgorithmByName(const String&) const;
    const AlgorithmInfo* lookupAlgorithmById(WebKit::WebCryptoAlgorithmId) const;

private:
    AlgorithmRegistry();

    // Algorithm name to ID.
    typedef HashMap<String, WebKit::WebCryptoAlgorithmId, CaseFoldingHash> AlgorithmNameToIdMap;
    AlgorithmNameToIdMap m_algorithmNameToId;

    // Algorithm ID to information.
    AlgorithmInfo m_algorithms[WebKit::NumberOfWebCryptoAlgorithmId];
};

AlgorithmRegistry& AlgorithmRegistry::instance()
{
    DEFINE_STATIC_LOCAL(AlgorithmRegistry, registry, ());
    return registry;
}

const AlgorithmInfo* AlgorithmRegistry::lookupAlgorithmByName(const String& algorithmName) const
{
    AlgorithmNameToIdMap::const_iterator it = m_algorithmNameToId.find(algorithmName);
    if (it == m_algorithmNameToId.end())
        return 0;
    return lookupAlgorithmById(it->value);
}

const AlgorithmInfo* AlgorithmRegistry::lookupAlgorithmById(WebKit::WebCryptoAlgorithmId algorithmId) const
{
    ASSERT(algorithmId >= 0 && algorithmId < WTF_ARRAY_LENGTH(m_algorithms));
    return &m_algorithms[algorithmId];
}

AlgorithmRegistry::AlgorithmRegistry()
{
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(algorithmNameMappings); ++i) {
        const AlgorithmNameMapping& mapping = algorithmNameMappings[i];
        m_algorithmNameToId.add(mapping.algorithmName, mapping.algorithmId);
        m_algorithms[mapping.algorithmId].algorithmName = mapping.algorithmName;
        m_algorithms[mapping.algorithmId].algorithmId = mapping.algorithmId;
    }

    for (size_t i = 0; i < WTF_ARRAY_LENGTH(operationParamsMappings); ++i) {
        const OperationParamsMapping& mapping = operationParamsMappings[i];
        m_algorithms[mapping.algorithmId].paramsForOperation[mapping.operation] = mapping.params;
    }
}

// ExceptionContext holds a stack of string literals which describe what was
// happening at the time the exception was thrown. This is helpful because
// parsing of the algorithm dictionary can be recursive and it is difficult to
// tell what went wrong from the exception type alone (TypeError).
class ExceptionContext {
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
        ExceptionContext stack(*this);
        stack.add(message);
        return stack.toString();
    }

    String toString(const char* message1, const char* message2) const
    {
        ExceptionContext stack(*this);
        stack.add(message1);
        stack.add(message2);
        return stack.toString();
    }

private:
    // This inline size is large enough to avoid having to grow the Vector in
    // the majority of cases (up to 1 nested algorithm identifier).
    Vector<const char*, 10> m_messages;
};

bool getArrayBufferView(const Dictionary& raw, const char* propertyName, RefPtr<ArrayBufferView>& buffer, const ExceptionContext& context, ExceptionState& es)
{
    if (!raw.get(propertyName, buffer) || !buffer) {
        es.throwTypeError(context.toString(propertyName, "Missing or not a ArrayBufferView"));
        return false;
    }
    return true;
}

bool getUint8Array(const Dictionary& raw, const char* propertyName, RefPtr<Uint8Array>& array, const ExceptionContext& context, ExceptionState& es)
{
    if (!raw.get(propertyName, array) || !array) {
        es.throwTypeError(context.toString(propertyName, "Missing or not a Uint8Array"));
        return false;
    }
    return true;
}

// Gets an integer according to WebIDL's [EnforceRange].
bool getOptionalInteger(const Dictionary& raw, const char* propertyName, bool& hasProperty, double& value, double minValue, double maxValue, const ExceptionContext& context, ExceptionState& es)
{
    double number;
    bool ok = raw.get(propertyName, number, hasProperty);

    if (!hasProperty)
        return true;

    if (!ok || std::isnan(number)) {
        es.throwTypeError(context.toString(propertyName, "Is not a number"));
        return false;
    }

    number = trunc(number);

    if (std::isinf(number) || number < minValue || number > maxValue) {
        es.throwTypeError(context.toString(propertyName, "Outside of numeric range"));
        return false;
    }

    value = number;
    return true;
}

bool getInteger(const Dictionary& raw, const char* propertyName, double& value, double minValue, double maxValue, const ExceptionContext& context, ExceptionState& es)
{
    bool hasProperty;
    if (!getOptionalInteger(raw, propertyName, hasProperty, value, minValue, maxValue, context, es))
        return false;

    if (!hasProperty) {
        es.throwTypeError(context.toString(propertyName, "Missing required property"));
        return false;
    }

    return true;
}

bool getUint32(const Dictionary& raw, const char* propertyName, uint32_t& value, const ExceptionContext& context, ExceptionState& es)
{
    double number;
    if (!getInteger(raw, propertyName, number, 0, 0xFFFFFFFF, context, es))
        return false;
    value = number;
    return true;
}

bool getUint16(const Dictionary& raw, const char* propertyName, uint16_t& value, const ExceptionContext& context, ExceptionState& es)
{
    double number;
    if (!getInteger(raw, propertyName, number, 0, 0xFFFF, context, es))
        return false;
    value = number;
    return true;
}

bool getOptionalUint32(const Dictionary& raw, const char* propertyName, bool& hasValue, uint32_t& value, const ExceptionContext& context, ExceptionState& es)
{
    double number;
    if (!getOptionalInteger(raw, propertyName, hasValue, number, 0, 0xFFFFFFFF, context, es))
        return false;
    if (hasValue)
        value = number;
    return true;
}

bool parseAesCbcParams(const Dictionary& raw, OwnPtr<WebKit::WebCryptoAlgorithmParams>& params, const ExceptionContext& context, ExceptionState& es)
{
    RefPtr<ArrayBufferView> iv;
    if (!getArrayBufferView(raw, "iv", iv, context, es))
        return false;

    if (iv->byteLength() != 16) {
        es.throwTypeError(context.toString("iv", "Must be 16 bytes"));
        return false;
    }

    params = adoptPtr(new WebKit::WebCryptoAesCbcParams(static_cast<unsigned char*>(iv->baseAddress()), iv->byteLength()));
    return true;
}

bool parseAesKeyGenParams(const Dictionary& raw, OwnPtr<WebKit::WebCryptoAlgorithmParams>& params, const ExceptionContext& context, ExceptionState& es)
{
    uint16_t length;
    if (!getUint16(raw, "length", length, context, es))
        return false;

    params = adoptPtr(new WebKit::WebCryptoAesKeyGenParams(length));
    return true;
}

bool normalizeAlgorithm(const Dictionary&, AlgorithmOperation, WebKit::WebCryptoAlgorithm&, ExceptionContext, ExceptionState&);

bool parseHash(const Dictionary& raw, WebKit::WebCryptoAlgorithm& hash, ExceptionContext context, ExceptionState& es)
{
    Dictionary rawHash;
    if (!raw.get("hash", rawHash)) {
        es.throwTypeError(context.toString("hash", "Missing or not a dictionary"));
        return false;
    }

    context.add("hash");
    return normalizeAlgorithm(rawHash, Digest, hash, context, es);
}

bool parseHmacParams(const Dictionary& raw, OwnPtr<WebKit::WebCryptoAlgorithmParams>& params, const ExceptionContext& context, ExceptionState& es)
{
    WebKit::WebCryptoAlgorithm hash;
    if (!parseHash(raw, hash, context, es))
        return false;

    params = adoptPtr(new WebKit::WebCryptoHmacParams(hash));
    return true;
}

bool parseHmacKeyParams(const Dictionary& raw, OwnPtr<WebKit::WebCryptoAlgorithmParams>& params, const ExceptionContext& context, ExceptionState& es)
{
    WebKit::WebCryptoAlgorithm hash;
    if (!parseHash(raw, hash, context, es))
        return false;

    bool hasLength;
    uint32_t length = 0;
    if (!getOptionalUint32(raw, "length", hasLength, length, context, es))
        return false;

    params = adoptPtr(new WebKit::WebCryptoHmacKeyParams(hash, hasLength, length));
    return true;
}

bool parseRsaSsaParams(const Dictionary& raw, OwnPtr<WebKit::WebCryptoAlgorithmParams>& params, const ExceptionContext& context, ExceptionState& es)
{
    WebKit::WebCryptoAlgorithm hash;
    if (!parseHash(raw, hash, context, es))
        return false;

    params = adoptPtr(new WebKit::WebCryptoRsaSsaParams(hash));
    return true;
}

bool parseRsaKeyGenParams(const Dictionary& raw, OwnPtr<WebKit::WebCryptoAlgorithmParams>& params, const ExceptionContext& context, ExceptionState& es)
{
    uint32_t modulusLength;
    if (!getUint32(raw, "modulusLength", modulusLength, context, es))
        return false;

    RefPtr<Uint8Array> publicExponent;
    if (!getUint8Array(raw, "publicExponent", publicExponent, context, es))
        return false;

    params = adoptPtr(new WebKit::WebCryptoRsaKeyGenParams(modulusLength, static_cast<const unsigned char*>(publicExponent->baseAddress()), publicExponent->byteLength()));
    return true;
}

bool parseAlgorithmParams(const Dictionary& raw, WebKit::WebCryptoAlgorithmParamsType type, OwnPtr<WebKit::WebCryptoAlgorithmParams>& params, ExceptionContext& context, ExceptionState& es)
{
    switch (type) {
    case WebKit::WebCryptoAlgorithmParamsTypeNone:
        return true;
    case WebKit::WebCryptoAlgorithmParamsTypeAesCbcParams:
        context.add("AesCbcParams");
        return parseAesCbcParams(raw, params, context, es);
    case WebKit::WebCryptoAlgorithmParamsTypeAesKeyGenParams:
        context.add("AesKeyGenParams");
        return parseAesKeyGenParams(raw, params, context, es);
    case WebKit::WebCryptoAlgorithmParamsTypeHmacParams:
        context.add("HmacParams");
        return parseHmacParams(raw, params, context, es);
    case WebKit::WebCryptoAlgorithmParamsTypeHmacKeyParams:
        context.add("HmacKeyParams");
        return parseHmacKeyParams(raw, params, context, es);
    case WebKit::WebCryptoAlgorithmParamsTypeRsaSsaParams:
        context.add("RsaSSaParams");
        return parseRsaSsaParams(raw, params, context, es);
    case WebKit::WebCryptoAlgorithmParamsTypeRsaKeyGenParams:
        context.add("RsaKeyGenParams");
        return parseRsaKeyGenParams(raw, params, context, es);
    case WebKit::WebCryptoAlgorithmParamsTypeAesGcmParams:
    case WebKit::WebCryptoAlgorithmParamsTypeRsaOaepParams:
        // TODO
        notImplemented();
        break;
    }
    ASSERT_NOT_REACHED();
    return false;
}

const AlgorithmInfo* algorithmInfo(const Dictionary& raw, const ExceptionContext& context, ExceptionState& es)
{
    if (!raw.isObject()) {
        es.throwTypeError(context.toString("Not an object"));
        return 0;
    }

    String algorithmName;
    if (!raw.get("name", algorithmName)) {
        es.throwTypeError(context.toString("name", "Missing or not a string"));
        return 0;
    }

    const AlgorithmInfo* info = AlgorithmRegistry::instance().lookupAlgorithmByName(algorithmName);
    if (!info) {
        es.throwDOMException(NotSupportedError, context.toString("Unrecognized algorithm name"));
        return 0;
    }

    return info;
}

// This implementation corresponds with:
// http://www.w3.org/TR/WebCryptoAPI/#algorithm-normalizing-rules
bool normalizeAlgorithm(const Dictionary& raw, AlgorithmOperation op, WebKit::WebCryptoAlgorithm& algorithm, ExceptionContext context, ExceptionState& es)
{
    context.add("Algorithm");

    const AlgorithmInfo* info = algorithmInfo(raw, context, es);
    if (!info)
        return false;

    context.add(info->algorithmName);

    if (info->paramsForOperation[op] == UnsupportedOp) {
        es.throwDOMException(NotSupportedError, context.toString("Unsupported operation"));
        return false;
    }

    WebKit::WebCryptoAlgorithmParamsType paramsType = static_cast<WebKit::WebCryptoAlgorithmParamsType>(info->paramsForOperation[op]);
    OwnPtr<WebKit::WebCryptoAlgorithmParams> params;
    if (!parseAlgorithmParams(raw, paramsType, params, context, es))
        return false;

    algorithm = WebKit::WebCryptoAlgorithm(info->algorithmId, params.release());
    return true;
}

} // namespace

bool normalizeAlgorithm(const Dictionary& raw, AlgorithmOperation op, WebKit::WebCryptoAlgorithm& algorithm, ExceptionState& es)
{
    return normalizeAlgorithm(raw, op, algorithm, ExceptionContext(), es);
}

const char* algorithmIdToName(WebKit::WebCryptoAlgorithmId id)
{
    return AlgorithmRegistry::instance().lookupAlgorithmById(id)->algorithmName;
}

} // namespace WebCore
