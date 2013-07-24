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

#include "public/platform/WebArrayBuffer.h"
#include "public/platform/WebCryptoAlgorithm.h"
#include <string>
#include <string.h>

using namespace WebKit;

namespace WebTestRunner {

namespace {

class MockCryptoOperation : public WebKit::WebCryptoOperation {
public:
    MockCryptoOperation(const WebKit::WebCryptoAlgorithm& algorithm, const WebKit::WebCryptoOperationResult& result) : m_algorithm(algorithm), m_result(result) { }

    virtual void process(const unsigned char* bytes, size_t size) OVERRIDE
    {
        // Don't buffer too much data.
        if (m_data.size() + size > 6)
            m_result.completeWithError();

        if (size)
            m_data.append(reinterpret_cast<const char*>(bytes), size);
    }

    virtual void abort() OVERRIDE
    {
        delete this;
    }

    virtual void finish() OVERRIDE
    {
        const unsigned char digest1[20] = {0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55, 0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09};
        const unsigned char digest2[20] = {0x5b, 0xa9, 0x3c, 0x9d, 0xb0, 0xcf, 0xf9, 0x3f, 0x52, 0xb5, 0x21, 0xd7, 0x42, 0x0e, 0x43, 0xf6, 0xed, 0xa2, 0x78, 0x4f};
        const unsigned char digest3[20] = {0x86, 0x84, 0x60, 0xd9, 0x8d, 0x09, 0xd8, 0xbb, 0xb9, 0x3d, 0x7b, 0x6c, 0xdd, 0x15, 0xcc, 0x7f, 0xbe, 0xc6, 0x76, 0xb9};

        const unsigned char* result = 0;
        size_t resultSize;

        if (m_algorithm.id() == WebKit::WebCryptoAlgorithmIdSha1) {
            resultSize = 20;

            if (m_data.empty()) {
                result = digest1;
            } else if (m_data == std::string("\x00", 1)) {
                result = digest2;
            } else if (m_data == std::string("\x00\x01\x02\x03\x04\x05", 6)) {
                result = digest3;
            }
        }

        if (result) {
            WebKit::WebArrayBuffer buffer = WebKit::WebArrayBuffer::create(resultSize, 1);
            memcpy(buffer.data(), result, resultSize);
            m_result.completeWithArrayBuffer(buffer);
        } else {
            m_result.completeWithError();
        }
        delete this;
    }

protected:
    WebKit::WebCryptoAlgorithm m_algorithm;
    WebKit::WebCryptoOperationResult m_result;
    std::string m_data;
};

} // namespace

MockWebCrypto* MockWebCrypto::get()
{
    static MockWebCrypto crypto;
    return &crypto;
}

void MockWebCrypto::digest(const WebKit::WebCryptoAlgorithm& algorithm, WebKit::WebCryptoOperationResult& result)
{
    result.initializationSucceeded(new MockCryptoOperation(algorithm, result));
}

void MockWebCrypto::importKey(WebKit::WebCryptoKeyFormat, const unsigned char* keyData, size_t keyDataSize, const WebKit::WebCryptoAlgorithm& algorithm, bool extractable, WebKit::WebCryptoKeyUsageMask usages, WebKit::WebCryptoKeyOperationResult& result)
{
    std::string keyDataString(reinterpret_cast<const char*>(keyData), keyDataSize);

    WebKit::WebCryptoKeyType type;
    if (keyDataString == "reject") {
        result.completeWithError();
    } else if (keyDataString == "throw") {
        result.initializationFailed();
    } else {
        if (keyDataString == "public") {
            type = WebKit::WebCryptoKeyTypePublic;
        } else if (keyDataString == "private") {
            type = WebKit::WebCryptoKeyTypePrivate;
        }
        result.completeWithKey(WebKit::WebCryptoKey::create(0, type, extractable, algorithm, usages));
    }
}

} // namespace WebTestRunner
