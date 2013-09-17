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

#ifndef MockWebCrypto_h
#define MockWebCrypto_h

#include "TestCommon.h"
#include "public/platform/WebCrypto.h"
#include "public/platform/WebNonCopyable.h"

namespace WebTestRunner {

class MockWebCrypto : public WebKit::WebCrypto, public WebKit::WebNonCopyable {
public:
    static MockWebCrypto* get();

    virtual void encrypt(const WebKit::WebCryptoAlgorithm&, const WebKit::WebCryptoKey&, const unsigned char*, unsigned, WebKit::WebCryptoResult) OVERRIDE;
    virtual void decrypt(const WebKit::WebCryptoAlgorithm&, const WebKit::WebCryptoKey&, const unsigned char*, unsigned, WebKit::WebCryptoResult) OVERRIDE;
    virtual void sign(const WebKit::WebCryptoAlgorithm&, const WebKit::WebCryptoKey&, const unsigned char*, unsigned, WebKit::WebCryptoResult) OVERRIDE;
    virtual void verifySignature(const WebKit::WebCryptoAlgorithm&, const WebKit::WebCryptoKey&, const unsigned char*, unsigned, const unsigned char*, unsigned, WebKit::WebCryptoResult) OVERRIDE;
    virtual void digest(const WebKit::WebCryptoAlgorithm&, const unsigned char*, unsigned, WebKit::WebCryptoResult) OVERRIDE;
    virtual void generateKey(const WebKit::WebCryptoAlgorithm&, bool extractable, WebKit::WebCryptoKeyUsageMask, WebKit::WebCryptoResult) OVERRIDE;
    virtual void importKey(WebKit::WebCryptoKeyFormat, const unsigned char* keyData, unsigned keyDataSize, const WebKit::WebCryptoAlgorithm&, bool extractable, WebKit::WebCryptoKeyUsageMask, WebKit::WebCryptoResult) OVERRIDE;
    virtual void exportKey(WebKit::WebCryptoKeyFormat, const WebKit::WebCryptoKey&, WebKit::WebCryptoResult) OVERRIDE;
};

} // namespace WebTestRunner

#endif // MockWebCrypto_h
