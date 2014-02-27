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
#include "public/platform/WebCryptoAlgorithm.h"

#include "public/platform/WebCryptoAlgorithmParams.h"
#include "wtf/OwnPtr.h"
#include "wtf/ThreadSafeRefCounted.h"

namespace blink {

class WebCryptoAlgorithmPrivate : public ThreadSafeRefCounted<WebCryptoAlgorithmPrivate> {
public:
    WebCryptoAlgorithmPrivate(WebCryptoAlgorithmId id, PassOwnPtr<WebCryptoAlgorithmParams> params)
        : id(id)
        , params(params)
    {
    }

    WebCryptoAlgorithmId id;
    OwnPtr<WebCryptoAlgorithmParams> params;
};

WebCryptoAlgorithm::WebCryptoAlgorithm(WebCryptoAlgorithmId id, PassOwnPtr<WebCryptoAlgorithmParams> params)
    : m_private(adoptRef(new WebCryptoAlgorithmPrivate(id, params)))
{
}

WebCryptoAlgorithm WebCryptoAlgorithm::createNull()
{
    return WebCryptoAlgorithm();
}

WebCryptoAlgorithm WebCryptoAlgorithm::adoptParamsAndCreate(WebCryptoAlgorithmId id, WebCryptoAlgorithmParams* params)
{
    return WebCryptoAlgorithm(id, adoptPtr(params));
}

bool WebCryptoAlgorithm::isNull() const
{
    return m_private.isNull();
}

WebCryptoAlgorithmId WebCryptoAlgorithm::id() const
{
    ASSERT(!isNull());
    return m_private->id;
}

WebCryptoAlgorithmParamsType WebCryptoAlgorithm::paramsType() const
{
    ASSERT(!isNull());
    if (!m_private->params)
        return WebCryptoAlgorithmParamsTypeNone;
    return m_private->params->type();
}

const WebCryptoAesCbcParams* WebCryptoAlgorithm::aesCbcParams() const
{
    ASSERT(!isNull());
    if (paramsType() == WebCryptoAlgorithmParamsTypeAesCbcParams)
        return static_cast<WebCryptoAesCbcParams*>(m_private->params.get());
    return 0;
}

const WebCryptoAesCtrParams* WebCryptoAlgorithm::aesCtrParams() const
{
    ASSERT(!isNull());
    if (paramsType() == WebCryptoAlgorithmParamsTypeAesCtrParams)
        return static_cast<WebCryptoAesCtrParams*>(m_private->params.get());
    return 0;
}

const WebCryptoAesKeyGenParams* WebCryptoAlgorithm::aesKeyGenParams() const
{
    ASSERT(!isNull());
    if (paramsType() == WebCryptoAlgorithmParamsTypeAesKeyGenParams)
        return static_cast<WebCryptoAesKeyGenParams*>(m_private->params.get());
    return 0;
}

const WebCryptoHmacImportParams* WebCryptoAlgorithm::hmacImportParams() const
{
    ASSERT(!isNull());
    if (paramsType() == WebCryptoAlgorithmParamsTypeHmacImportParams)
        return static_cast<WebCryptoHmacImportParams*>(m_private->params.get());
    return 0;
}

const WebCryptoHmacKeyGenParams* WebCryptoAlgorithm::hmacKeyGenParams() const
{
    ASSERT(!isNull());
    if (paramsType() == WebCryptoAlgorithmParamsTypeHmacKeyGenParams)
        return static_cast<WebCryptoHmacKeyGenParams*>(m_private->params.get());
    return 0;
}

const WebCryptoRsaKeyGenParams* WebCryptoAlgorithm::rsaKeyGenParams() const
{
    ASSERT(!isNull());
    if (paramsType() == WebCryptoAlgorithmParamsTypeRsaKeyGenParams)
        return static_cast<WebCryptoRsaKeyGenParams*>(m_private->params.get());
    return 0;
}

const WebCryptoAesGcmParams* WebCryptoAlgorithm::aesGcmParams() const
{
    ASSERT(!isNull());
    if (paramsType() == WebCryptoAlgorithmParamsTypeAesGcmParams)
        return static_cast<WebCryptoAesGcmParams*>(m_private->params.get());
    return 0;
}

const WebCryptoRsaOaepParams* WebCryptoAlgorithm::rsaOaepParams() const
{
    ASSERT(!isNull());
    if (paramsType() == WebCryptoAlgorithmParamsTypeRsaOaepParams)
        return static_cast<WebCryptoRsaOaepParams*>(m_private->params.get());
    return 0;
}

const WebCryptoRsaHashedImportParams* WebCryptoAlgorithm::rsaHashedImportParams() const
{
    ASSERT(!isNull());
    if (paramsType() == WebCryptoAlgorithmParamsTypeRsaHashedImportParams)
        return static_cast<WebCryptoRsaHashedImportParams*>(m_private->params.get());
    return 0;
}

const WebCryptoRsaHashedKeyGenParams* WebCryptoAlgorithm::rsaHashedKeyGenParams() const
{
    ASSERT(!isNull());
    if (paramsType() == WebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams)
        return static_cast<WebCryptoRsaHashedKeyGenParams*>(m_private->params.get());
    return 0;
}

void WebCryptoAlgorithm::assign(const WebCryptoAlgorithm& other)
{
    m_private = other.m_private;
}

void WebCryptoAlgorithm::reset()
{
    m_private.reset();
}

} // namespace blink
