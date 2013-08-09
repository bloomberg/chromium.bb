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
#include "public/platform/WebCryptoAlgorithm.h"
#include "public/platform/WebCryptoAlgorithmParams.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/ArrayBufferView.h"
#include "wtf/HashMap.h"
#include "wtf/Uint8Array.h"
#include "wtf/Vector.h"
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
    {WebKit::WebCryptoAlgorithmIdHmac, GenerateKey, WebKit::WebCryptoAlgorithmParamsTypeHmacParams},
    {WebKit::WebCryptoAlgorithmIdHmac, ImportKey, WebKit::WebCryptoAlgorithmParamsTypeHmacParams},

    // RSASSA-PKCS1-v1_5
    {WebKit::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5, Sign, WebKit::WebCryptoAlgorithmParamsTypeRsaSsaParams},
    {WebKit::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5, Verify, WebKit::WebCryptoAlgorithmParamsTypeRsaSsaParams},
    {WebKit::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5, GenerateKey, WebKit::WebCryptoAlgorithmParamsTypeRsaKeyGenParams},
    {WebKit::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5, ImportKey, WebKit::WebCryptoAlgorithmParamsTypeNone},

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
    static const AlgorithmInfo* lookupAlgorithmByName(const String& algorithmName);

private:
    AlgorithmRegistry();

    // Algorithm name to ID.
    typedef HashMap<String, WebKit::WebCryptoAlgorithmId, CaseFoldingHash> AlgorithmNameToIdMap;
    AlgorithmNameToIdMap m_algorithmNameToId;

    // Algorithm ID to information.
    AlgorithmInfo m_algorithms[WebKit::NumberOfWebCryptoAlgorithmId];
};

const AlgorithmInfo* AlgorithmRegistry::lookupAlgorithmByName(const String& algorithmName)
{
    DEFINE_STATIC_LOCAL(AlgorithmRegistry, registry, ());

    AlgorithmNameToIdMap::const_iterator it = registry.m_algorithmNameToId.find(algorithmName);
    if (it == registry.m_algorithmNameToId.end())
        return 0;
    return &registry.m_algorithms[it->value];
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

PassOwnPtr<WebKit::WebCryptoAlgorithmParams> parseAesCbcParams(const Dictionary& raw)
{
    RefPtr<ArrayBufferView> iv;
    if (!raw.get("iv", iv) || !iv)
        return nullptr;

    if (iv->byteLength() != 16)
        return nullptr;

    return adoptPtr(new WebKit::WebCryptoAesCbcParams(static_cast<unsigned char*>(iv->baseAddress()), iv->byteLength()));
}

PassOwnPtr<WebKit::WebCryptoAlgorithmParams> parseAesKeyGenParams(const Dictionary& raw)
{
    int32_t length;
    if (!raw.get("length", length))
        return nullptr;
    if (length < 0 || length > 0xFFFF)
        return nullptr;
    return adoptPtr(new WebKit::WebCryptoAesKeyGenParams(length));
}

bool parseHash(const Dictionary& raw, WebKit::WebCryptoAlgorithm& hash)
{
    Dictionary rawHash;
    if (!raw.get("hash", rawHash))
        return false;

    TrackExceptionState es;
    return normalizeAlgorithm(rawHash, Digest, hash, es);
}

PassOwnPtr<WebKit::WebCryptoAlgorithmParams> parseHmacParams(const Dictionary& raw)
{
    WebKit::WebCryptoAlgorithm hash;
    if (!parseHash(raw, hash))
        return nullptr;
    return adoptPtr(new WebKit::WebCryptoHmacParams(hash));
}

PassOwnPtr<WebKit::WebCryptoAlgorithmParams> parseRsaSsaParams(const Dictionary& raw)
{
    WebKit::WebCryptoAlgorithm hash;
    if (!parseHash(raw, hash))
        return nullptr;
    return adoptPtr(new WebKit::WebCryptoRsaSsaParams(hash));
}

PassOwnPtr<WebKit::WebCryptoAlgorithmParams> parseRsaKeyGenParams(const Dictionary& raw)
{
    // FIXME: This is losing precision; modulusLength is supposed to be a uint32
    int32_t modulusLength;
    if (!raw.get("modulusLength", modulusLength))
        return nullptr;
    if (modulusLength < 0)
        return nullptr;

    RefPtr<Uint8Array> publicExponent;
    if (!raw.get("publicExponent", publicExponent) || !publicExponent)
        return nullptr;
    return adoptPtr(new WebKit::WebCryptoRsaKeyGenParams(modulusLength, static_cast<const unsigned char*>(publicExponent->baseAddress()), publicExponent->byteLength()));
}

PassOwnPtr<WebKit::WebCryptoAlgorithmParams> parseAlgorithmParams(const Dictionary& raw, WebKit::WebCryptoAlgorithmParamsType type)
{
    switch (type) {
    case WebKit::WebCryptoAlgorithmParamsTypeNone:
        return nullptr;
    case WebKit::WebCryptoAlgorithmParamsTypeAesCbcParams:
        return parseAesCbcParams(raw);
    case WebKit::WebCryptoAlgorithmParamsTypeAesKeyGenParams:
        return parseAesKeyGenParams(raw);
    case WebKit::WebCryptoAlgorithmParamsTypeHmacParams:
        return parseHmacParams(raw);
    case WebKit::WebCryptoAlgorithmParamsTypeRsaSsaParams:
        return parseRsaSsaParams(raw);
    case WebKit::WebCryptoAlgorithmParamsTypeRsaKeyGenParams:
        return parseRsaKeyGenParams(raw);
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

const AlgorithmInfo* algorithmInfo(const Dictionary& raw, ExceptionState& es)
{
    String algorithmName;
    if (!raw.get("name", algorithmName)) {
        es.throwDOMException(NotSupportedError);
        return 0;
    }

    if (!algorithmName.containsOnlyASCII()) {
        es.throwDOMException(SyntaxError);
        return 0;
    }

    const AlgorithmInfo* info = AlgorithmRegistry::lookupAlgorithmByName(algorithmName);
    if (!info) {
        es.throwDOMException(NotSupportedError);
        return 0;
    }

    return info;
}

} // namespace

// FIXME: Throw the correct exception types!
// This implementation corresponds with:
// http://www.w3.org/TR/WebCryptoAPI/#algorithm-normalizing-rules
bool normalizeAlgorithm(const Dictionary& raw, AlgorithmOperation op, WebKit::WebCryptoAlgorithm& algorithm, ExceptionState& es)
{
    const AlgorithmInfo* info = algorithmInfo(raw, es);
    if (!info)
        return false;

    if (info->paramsForOperation[op] == UnsupportedOp) {
        es.throwDOMException(NotSupportedError);
        return false;
    }

    WebKit::WebCryptoAlgorithmParamsType paramsType = static_cast<WebKit::WebCryptoAlgorithmParamsType>(info->paramsForOperation[op]);
    OwnPtr<WebKit::WebCryptoAlgorithmParams> params = parseAlgorithmParams(raw, paramsType);

    if (!params && paramsType != WebKit::WebCryptoAlgorithmParamsTypeNone) {
        es.throwDOMException(NotSupportedError);
        return false;
    }

    algorithm = WebKit::WebCryptoAlgorithm(info->algorithmId, info->algorithmName, params.release());
    return true;
}

} // namespace WebCore
