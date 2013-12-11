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

#ifndef WebCryptoAlgorithmParams_h
#define WebCryptoAlgorithmParams_h

#include "WebCommon.h"
#include "WebCryptoAlgorithm.h"
#include "WebVector.h"

namespace blink {

// NOTE: For documentation on the meaning of each of the parameters see the
//       Web crypto spec:
//
//       http://www.w3.org/TR/WebCryptoAPI
//
//       For the most part, the parameters in the spec have the same name,
//       except that in the blink code:
//
//         - Structure names are prefixed by "WebCrypto"
//         - Optional fields are prefixed by "optional"
//         - Data length properties are suffixed by either "Bits" or "Bytes"

class WebCryptoAlgorithmParams {
public:
    explicit WebCryptoAlgorithmParams(WebCryptoAlgorithmParamsType type)
        : m_type(type)
    {
    }

    virtual ~WebCryptoAlgorithmParams() { }

    WebCryptoAlgorithmParamsType type() const { return m_type; }

private:
    const WebCryptoAlgorithmParamsType m_type;
};

class WebCryptoAesCbcParams : public WebCryptoAlgorithmParams {
public:
    WebCryptoAesCbcParams(const unsigned char* iv, unsigned ivSize)
        : WebCryptoAlgorithmParams(WebCryptoAlgorithmParamsTypeAesCbcParams)
        , m_iv(iv, ivSize)
    {
    }

    const WebVector<unsigned char>& iv() const { return m_iv; }

private:
    const WebVector<unsigned char> m_iv;
};

class WebCryptoAesCtrParams : public WebCryptoAlgorithmParams {
public:
    WebCryptoAesCtrParams(unsigned char lengthBits, const unsigned char* counter, unsigned counterSize)
        : WebCryptoAlgorithmParams(WebCryptoAlgorithmParamsTypeAesCtrParams)
        , m_counter(counter, counterSize)
        , m_lengthBits(lengthBits)
    {
    }

    const WebVector<unsigned char>& counter() const { return m_counter; }
    unsigned char lengthBits() const { return m_lengthBits; }

private:
    const WebVector<unsigned char> m_counter;
    const unsigned char m_lengthBits;
};

class WebCryptoAesKeyGenParams : public WebCryptoAlgorithmParams {
public:
    explicit WebCryptoAesKeyGenParams(unsigned short lengthBits)
        : WebCryptoAlgorithmParams(WebCryptoAlgorithmParamsTypeAesKeyGenParams)
        , m_lengthBits(lengthBits)
    {
    }

    // FIXME: Delete once no longer referenced by chromium.
    unsigned short length() const { return m_lengthBits; }

    unsigned short lengthBits() const { return m_lengthBits; }

private:
    const unsigned short m_lengthBits;
};

class WebCryptoHmacParams : public WebCryptoAlgorithmParams {
public:
    explicit WebCryptoHmacParams(const WebCryptoAlgorithm& hash)
        : WebCryptoAlgorithmParams(WebCryptoAlgorithmParamsTypeHmacParams)
        , m_hash(hash)
    {
        BLINK_ASSERT(!hash.isNull());
    }

    const WebCryptoAlgorithm& hash() const { return m_hash; }

private:
    const WebCryptoAlgorithm m_hash;
};

class WebCryptoHmacKeyParams : public WebCryptoAlgorithmParams {
public:
    WebCryptoHmacKeyParams(const WebCryptoAlgorithm& hash, bool hasLengthBytes, unsigned lengthBytes)
        : WebCryptoAlgorithmParams(WebCryptoAlgorithmParamsTypeHmacKeyParams)
        , m_hash(hash)
        , m_hasLengthBytes(hasLengthBytes)
        , m_optionalLengthBytes(lengthBytes)
    {
        BLINK_ASSERT(!hash.isNull());
        BLINK_ASSERT(hasLengthBytes || !lengthBytes);
    }

    const WebCryptoAlgorithm& hash() const { return m_hash; }

    bool hasLengthBytes() const { return m_hasLengthBytes; }

    // FIXME: Delete once no longer referenced by chromium.
    bool getLength(unsigned& length) const
    {
        if (!m_hasLengthBytes)
            return false;
        length = m_optionalLengthBytes;
        return true;
    }

    unsigned optionalLengthBytes() const { return m_optionalLengthBytes; }

private:
    const WebCryptoAlgorithm m_hash;
    const bool m_hasLengthBytes;
    const unsigned m_optionalLengthBytes;
};

class WebCryptoRsaSsaParams : public WebCryptoAlgorithmParams {
public:
    explicit WebCryptoRsaSsaParams(const WebCryptoAlgorithm& hash)
        : WebCryptoAlgorithmParams(WebCryptoAlgorithmParamsTypeRsaSsaParams)
        , m_hash(hash)
    {
        BLINK_ASSERT(!hash.isNull());
    }

    const WebCryptoAlgorithm& hash() const { return m_hash; }

private:
    const WebCryptoAlgorithm m_hash;
};

class WebCryptoRsaKeyGenParams : public WebCryptoAlgorithmParams {
public:
    WebCryptoRsaKeyGenParams(unsigned modulusLengthBits, const unsigned char* publicExponent, unsigned publicExponentSize)
        : WebCryptoAlgorithmParams(WebCryptoAlgorithmParamsTypeRsaKeyGenParams)
        , m_modulusLengthBits(modulusLengthBits)
        , m_publicExponent(publicExponent, publicExponentSize)
    {
    }

    // FIXME: Delete once no longer referenced by chromium.
    unsigned modulusLength() const { return m_modulusLengthBits; }

    unsigned modulusLengthBits() const { return m_modulusLengthBits; }
    const WebVector<unsigned char>& publicExponent() const { return m_publicExponent; }

private:
    const unsigned m_modulusLengthBits;
    const WebVector<unsigned char> m_publicExponent;
};

class WebCryptoAesGcmParams : public WebCryptoAlgorithmParams {
public:
    WebCryptoAesGcmParams(const unsigned char* iv, unsigned ivSize, bool hasAdditionalData, const unsigned char* additionalData, unsigned additionalDataSize, bool hasTagLengthBits, unsigned char tagLengthBits)
        : WebCryptoAlgorithmParams(WebCryptoAlgorithmParamsTypeAesGcmParams)
        , m_iv(iv, ivSize)
        , m_hasAdditionalData(hasAdditionalData)
        , m_optionalAdditionalData(additionalData, additionalDataSize)
        , m_hasTagLengthBits(hasTagLengthBits)
        , m_optionalTagLengthBits(tagLengthBits)
    {
        BLINK_ASSERT(hasAdditionalData || !additionalDataSize);
        BLINK_ASSERT(hasTagLengthBits || !tagLengthBits);
    }

    const WebVector<unsigned char>& iv() const { return m_iv; }

    bool hasAdditionalData() const { return m_hasAdditionalData; }
    const WebVector<unsigned char>& optionalAdditionalData() const { return m_optionalAdditionalData; }

    bool hasTagLengthBits() const { return m_hasTagLengthBits; }
    unsigned optionalTagLengthBits() const { return m_optionalTagLengthBits; }

private:
    const WebVector<unsigned char> m_iv;
    const bool m_hasAdditionalData;
    const WebVector<unsigned char> m_optionalAdditionalData;
    const bool m_hasTagLengthBits;
    const unsigned char m_optionalTagLengthBits;
};

class WebCryptoRsaOaepParams : public WebCryptoAlgorithmParams {
public:
    WebCryptoRsaOaepParams(const WebCryptoAlgorithm& hash, bool hasLabel, const unsigned char* label, unsigned labelSize)
        : WebCryptoAlgorithmParams(WebCryptoAlgorithmParamsTypeRsaOaepParams)
        , m_hash(hash)
        , m_hasLabel(hasLabel)
        , m_optionalLabel(label, labelSize)
    {
        BLINK_ASSERT(!hash.isNull());
        BLINK_ASSERT(hasLabel || !labelSize);
    }

    const WebCryptoAlgorithm& hash() const { return m_hash; }

    bool hasLabel() const { return m_hasLabel; }
    const WebVector<unsigned char>& optionalLabel() const { return m_optionalLabel; }

private:
    const WebCryptoAlgorithm m_hash;
    const bool m_hasLabel;
    const WebVector<unsigned char> m_optionalLabel;
};

} // namespace blink

#endif
