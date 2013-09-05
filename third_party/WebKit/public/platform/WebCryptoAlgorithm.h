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

#ifndef WebCryptoAlgorithm_h
#define WebCryptoAlgorithm_h

#include "WebCommon.h"
#include "WebPrivatePtr.h"

#if WEBKIT_IMPLEMENTATION
#include "wtf/PassOwnPtr.h"
#endif

namespace WebKit {

enum WebCryptoAlgorithmId {
    WebCryptoAlgorithmIdAesCbc,
    WebCryptoAlgorithmIdHmac,
    WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
    WebCryptoAlgorithmIdRsaEsPkcs1v1_5,
    WebCryptoAlgorithmIdSha1,
    WebCryptoAlgorithmIdSha224,
    WebCryptoAlgorithmIdSha256,
    WebCryptoAlgorithmIdSha384,
    WebCryptoAlgorithmIdSha512,
#if WEBKIT_IMPLEMENTATION
    NumberOfWebCryptoAlgorithmId,
#endif
};

enum WebCryptoAlgorithmParamsType {
    WebCryptoAlgorithmParamsTypeNone,
    WebCryptoAlgorithmParamsTypeAesCbcParams,
    WebCryptoAlgorithmParamsTypeAesKeyGenParams,
    WebCryptoAlgorithmParamsTypeHmacParams,
    WebCryptoAlgorithmParamsTypeHmacKeyParams,
    WebCryptoAlgorithmParamsTypeRsaSsaParams,
    WebCryptoAlgorithmParamsTypeRsaKeyGenParams,
};

class WebCryptoAesCbcParams;
class WebCryptoAesKeyGenParams;
class WebCryptoHmacParams;
class WebCryptoHmacKeyParams;
class WebCryptoRsaSsaParams;
class WebCryptoRsaKeyGenParams;

class WebCryptoAlgorithmParams;
class WebCryptoAlgorithmPrivate;

// The WebCryptoAlgorithm represents a normalized algorithm and its parameters.
//   * Immutable
//   * Threadsafe
//   * Copiable (cheaply)
class WebCryptoAlgorithm {
public:
#if WEBKIT_IMPLEMENTATION
    WebCryptoAlgorithm() { }
    WebCryptoAlgorithm(WebCryptoAlgorithmId, PassOwnPtr<WebCryptoAlgorithmParams>);
#endif

    WEBKIT_EXPORT static WebCryptoAlgorithm adoptParamsAndCreate(WebCryptoAlgorithmId, WebCryptoAlgorithmParams*);

    ~WebCryptoAlgorithm() { reset(); }

    WebCryptoAlgorithm(const WebCryptoAlgorithm& other) { assign(other); }
    WebCryptoAlgorithm& operator=(const WebCryptoAlgorithm& other)
    {
        assign(other);
        return *this;
    }

    WEBKIT_EXPORT WebCryptoAlgorithmId id() const;

    WEBKIT_EXPORT WebCryptoAlgorithmParamsType paramsType() const;

    // Retrieves the type-specific parameters. The algorithm contains at most 1
    // type of parameters. Retrieving an invalid parameter will return 0.
    WEBKIT_EXPORT const WebCryptoAesCbcParams* aesCbcParams() const;
    WEBKIT_EXPORT const WebCryptoAesKeyGenParams* aesKeyGenParams() const;
    WEBKIT_EXPORT const WebCryptoHmacParams* hmacParams() const;
    WEBKIT_EXPORT const WebCryptoHmacKeyParams* hmacKeyParams() const;
    WEBKIT_EXPORT const WebCryptoRsaSsaParams* rsaSsaParams() const;
    WEBKIT_EXPORT const WebCryptoRsaKeyGenParams* rsaKeyGenParams() const;

private:
    WEBKIT_EXPORT void assign(const WebCryptoAlgorithm& other);
    WEBKIT_EXPORT void reset();

    WebPrivatePtr<WebCryptoAlgorithmPrivate> m_private;
};

} // namespace WebKit

#endif
