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

namespace WebKit {

// NOTE: For documentation on the meaning of each of the parameters see the
//       Web crypto spec:
//
//       http://www.w3.org/TR/WebCryptoAPI
//
//       The parameters in the spec have the same name, minus the "WebCrypto" prefix.

class WebCryptoAlgorithmParams {
public:
    explicit WebCryptoAlgorithmParams(WebCryptoAlgorithmParamsType type)
        : m_type(type)
    {
    }

    virtual ~WebCryptoAlgorithmParams() { }

    WebCryptoAlgorithmParamsType type() const { return m_type; }

private:
    WebCryptoAlgorithmParamsType m_type;
};

class WebCryptoAesCbcParams : public WebCryptoAlgorithmParams {
public:
    WebCryptoAesCbcParams(unsigned char* iv, unsigned ivSize)
        : WebCryptoAlgorithmParams(WebCryptoAlgorithmParamsTypeAesCbcParams)
        , m_iv(iv, ivSize)
    {
    }

    const WebVector<unsigned char>& iv() const { return m_iv; }

private:
    const WebVector<unsigned char> m_iv;
};

class WebCryptoAesKeyGenParams : public WebCryptoAlgorithmParams {
public:
    explicit WebCryptoAesKeyGenParams(unsigned short length)
        : WebCryptoAlgorithmParams(WebCryptoAlgorithmParamsTypeAesKeyGenParams)
        , m_length(length)
    {
    }

    unsigned short length() const { return m_length; }

private:
    const unsigned short m_length;
};

class WebCryptoHmacParams : public WebCryptoAlgorithmParams {
public:
    explicit WebCryptoHmacParams(const WebCryptoAlgorithm& hash)
        : WebCryptoAlgorithmParams(WebCryptoAlgorithmParamsTypeHmacParams)
        , m_hash(hash)
    {
    }

    const WebCryptoAlgorithm& hash() const { return m_hash; }

private:
    WebCryptoAlgorithm m_hash;
};

class WebCryptoHmacKeyParams : public WebCryptoAlgorithmParams {
public:
    WebCryptoHmacKeyParams(const WebCryptoAlgorithm& hash, bool hasLength, unsigned length)
        : WebCryptoAlgorithmParams(WebCryptoAlgorithmParamsTypeHmacKeyParams)
        , m_hash(hash)
        , m_hasLength(hasLength)
        , m_length(length)
    {
    }

    const WebCryptoAlgorithm& hash() const { return m_hash; }

    bool hasLength() const { return m_hasLength; }

    bool getLength(unsigned& length) const
    {
        if (!m_hasLength)
            return false;
        length = m_length;
        return true;
    }

private:
    WebCryptoAlgorithm m_hash;
    bool m_hasLength;
    unsigned m_length;
};

class WebCryptoRsaSsaParams : public WebCryptoAlgorithmParams {
public:
    explicit WebCryptoRsaSsaParams(const WebCryptoAlgorithm& hash)
        : WebCryptoAlgorithmParams(WebCryptoAlgorithmParamsTypeRsaSsaParams)
        , m_hash(hash)
    {
    }

    const WebCryptoAlgorithm& hash() const { return m_hash; }

private:
    WebCryptoAlgorithm m_hash;
};

class WebCryptoRsaKeyGenParams : public WebCryptoAlgorithmParams {
public:
    WebCryptoRsaKeyGenParams(unsigned modulusLength, const unsigned char* publicExponent, unsigned publicExponentSize)
        : WebCryptoAlgorithmParams(WebCryptoAlgorithmParamsTypeRsaKeyGenParams)
        , m_modulusLength(modulusLength)
        , m_publicExponent(publicExponent, publicExponentSize)
    {
    }

    unsigned modulusLength() const { return m_modulusLength; }
    const WebVector<unsigned char>& publicExponent() const { return m_publicExponent; }

private:
    const unsigned m_modulusLength;
    const WebVector<unsigned char> m_publicExponent;
};

} // namespace WebKit

#endif
