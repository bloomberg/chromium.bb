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

#include "MockWebCrypto.h"

#include "public/platform/WebCryptoAlgorithm.h"
#include <string>

using namespace WebKit;

namespace WebTestRunner {

namespace {

std::string mockSign(const unsigned char* bytes, unsigned size)
{
    return "signed HMAC:" + std::string(reinterpret_cast<const char*>(bytes), size);
}

} // namespace

MockWebCrypto* MockWebCrypto::get()
{
    static MockWebCrypto crypto;
    return &crypto;
}

void MockWebCrypto::encrypt(const WebKit::WebCryptoAlgorithm& algorithm, const WebKit::WebCryptoKey& key, const unsigned char* data, unsigned dataSize, WebKit::WebCryptoResult result)
{
    result.completeWithError();
}

void MockWebCrypto::decrypt(const WebKit::WebCryptoAlgorithm& algorithm, const WebKit::WebCryptoKey& key, const unsigned char* data, unsigned dataSize, WebKit::WebCryptoResult result)
{
    result.completeWithError();
}

void MockWebCrypto::sign(const WebKit::WebCryptoAlgorithm& algorithm, const WebKit::WebCryptoKey& key, const unsigned char* data, unsigned dataSize, WebKit::WebCryptoResult result)
{
    if (algorithm.id() != WebKit::WebCryptoAlgorithmIdHmac)
        return result.completeWithError();

    std::string signedData = mockSign(data, dataSize);
    result.completeWithBuffer(signedData.data(), signedData.size());
}

void MockWebCrypto::verifySignature(const WebKit::WebCryptoAlgorithm& algorithm, const WebKit::WebCryptoKey& key, const unsigned char* signatureBytes, unsigned signatureSize, const unsigned char* data, unsigned dataSize, WebKit::WebCryptoResult result)
{
    if (algorithm.id() != WebKit::WebCryptoAlgorithmIdHmac)
        return result.completeWithError();

    std::string signature = std::string(reinterpret_cast<const char*>(signatureBytes), signatureSize);
    std::string expectedSignature = mockSign(data, dataSize);
    result.completeWithBoolean(expectedSignature == signature);
}

void MockWebCrypto::digest(const WebKit::WebCryptoAlgorithm& algorithm, const unsigned char* data, unsigned dataSize, WebKit::WebCryptoResult result)
{
    if (algorithm.id() != WebKit::WebCryptoAlgorithmIdSha1)
        return result.completeWithError();

    std::string input = std::string(reinterpret_cast<const char*>(data), dataSize);

    if (input.empty()) {
        const unsigned char resultBytes[] = {0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55, 0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09};
        result.completeWithBuffer(resultBytes, sizeof(resultBytes));
    } else if (input == std::string("\x00", 1)) {
        const unsigned char resultBytes[] = {0x5b, 0xa9, 0x3c, 0x9d, 0xb0, 0xcf, 0xf9, 0x3f, 0x52, 0xb5, 0x21, 0xd7, 0x42, 0x0e, 0x43, 0xf6, 0xed, 0xa2, 0x78, 0x4f};
        result.completeWithBuffer(resultBytes, sizeof(resultBytes));
    } else if (input == std::string("\x00\x01\x02\x03\x04\x05", 6)) {
        const unsigned char resultBytes[] = {0x86, 0x84, 0x60, 0xd9, 0x8d, 0x09, 0xd8, 0xbb, 0xb9, 0x3d, 0x7b, 0x6c, 0xdd, 0x15, 0xcc, 0x7f, 0xbe, 0xc6, 0x76, 0xb9};
        result.completeWithBuffer(resultBytes, sizeof(resultBytes));
    } else {
        result.completeWithError();
    }
}

void MockWebCrypto::generateKey(const WebKit::WebCryptoAlgorithm& algorithm, bool extractable, WebKit::WebCryptoKeyUsageMask usages, WebKit::WebCryptoResult result)
{
    if (algorithm.id() == WebKit::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5) {
        result.completeWithKeyPair(WebKit::WebCryptoKey::create(0, WebKit::WebCryptoKeyTypePublic, extractable, algorithm, usages), WebKit::WebCryptoKey::create(0, WebKit::WebCryptoKeyTypePrivate, extractable, algorithm, usages));
    } else {
        result.completeWithKey(WebKit::WebCryptoKey::create(0, WebKit::WebCryptoKeyTypePrivate, extractable, algorithm, usages));
    }
}

void MockWebCrypto::importKey(WebKit::WebCryptoKeyFormat, const unsigned char* keyData, unsigned keyDataSize, const WebKit::WebCryptoAlgorithm& algorithm, bool extractable, WebKit::WebCryptoKeyUsageMask usages, WebKit::WebCryptoResult result)
{
    std::string keyDataString(reinterpret_cast<const char*>(keyData), keyDataSize);

    if (keyDataString == "error")
        return result.completeWithError();

    WebKit::WebCryptoKeyType type = WebKit::WebCryptoKeyTypePrivate;
    if (keyDataString == "public")
        type = WebKit::WebCryptoKeyTypePublic;

    result.completeWithKey(WebKit::WebCryptoKey::create(0, type, extractable, algorithm, usages));
}

void MockWebCrypto::exportKey(WebKit::WebCryptoKeyFormat format, const WebKit::WebCryptoKey& key, WebKit::WebCryptoResult result)
{
    std::string buffer;

    if (format == WebKit::WebCryptoKeyFormatRaw)
        buffer = "raw";
    else if (format == WebKit::WebCryptoKeyFormatPkcs8)
        buffer = "pkcs8";

    result.completeWithBuffer(buffer.data(), buffer.size());
}


} // namespace WebTestRunner
