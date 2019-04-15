// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_MOCK_WEB_CRYPTO_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_MOCK_WEB_CRYPTO_H_

#include <memory>
#include "base/single_thread_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/platform/web_crypto.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class MockWebCrypto : public WebCrypto {
 public:
  MockWebCrypto() = default;
  ~MockWebCrypto() override = default;

  MOCK_METHOD5(Encrypt,
               void(const WebCryptoAlgorithm&,
                    const WebCryptoKey&,
                    WebVector<unsigned char>,
                    WebCryptoResult,
                    scoped_refptr<base::SingleThreadTaskRunner>));
  MOCK_METHOD5(Decrypt,
               void(const WebCryptoAlgorithm&,
                    const WebCryptoKey&,
                    WebVector<unsigned char>,
                    WebCryptoResult,
                    scoped_refptr<base::SingleThreadTaskRunner>));
  MOCK_METHOD5(Sign,
               void(const WebCryptoAlgorithm&,
                    const WebCryptoKey&,
                    WebVector<unsigned char>,
                    WebCryptoResult,
                    scoped_refptr<base::SingleThreadTaskRunner>));
  MOCK_METHOD6(VerifySignature,
               void(const WebCryptoAlgorithm&,
                    const WebCryptoKey&,
                    WebVector<unsigned char>,
                    WebVector<unsigned char>,
                    WebCryptoResult,
                    scoped_refptr<base::SingleThreadTaskRunner>));
  MOCK_METHOD4(Digest,
               void(const WebCryptoAlgorithm&,
                    WebVector<unsigned char>,
                    WebCryptoResult,
                    scoped_refptr<base::SingleThreadTaskRunner>));
  MOCK_METHOD5(GenerateKey,
               void(const WebCryptoAlgorithm&,
                    bool,
                    WebCryptoKeyUsageMask,
                    WebCryptoResult,
                    scoped_refptr<base::SingleThreadTaskRunner>));
  MOCK_METHOD7(ImportKey,
               void(WebCryptoKeyFormat,
                    WebVector<unsigned char>,
                    const WebCryptoAlgorithm&,
                    bool,
                    WebCryptoKeyUsageMask,
                    WebCryptoResult,
                    scoped_refptr<base::SingleThreadTaskRunner>));
  MOCK_METHOD4(ExportKey,
               void(WebCryptoKeyFormat,
                    const WebCryptoKey&,
                    WebCryptoResult,
                    scoped_refptr<base::SingleThreadTaskRunner>));
  MOCK_METHOD6(WrapKey,
               void(WebCryptoKeyFormat,
                    const WebCryptoKey&,
                    const WebCryptoKey&,
                    const WebCryptoAlgorithm&,
                    WebCryptoResult,
                    scoped_refptr<base::SingleThreadTaskRunner>));
  MOCK_METHOD9(UnwrapKey,
               void(WebCryptoKeyFormat,
                    WebVector<unsigned char>,
                    const WebCryptoKey&,
                    const WebCryptoAlgorithm&,
                    const WebCryptoAlgorithm&,
                    bool,
                    WebCryptoKeyUsageMask,
                    WebCryptoResult,
                    scoped_refptr<base::SingleThreadTaskRunner>));
  MOCK_METHOD5(DeriveBits,
               void(const WebCryptoAlgorithm&,
                    const WebCryptoKey&,
                    unsigned,
                    WebCryptoResult,
                    scoped_refptr<base::SingleThreadTaskRunner>));
  MOCK_METHOD8(DeriveKey,
               void(const WebCryptoAlgorithm&,
                    const WebCryptoKey&,
                    const WebCryptoAlgorithm&,
                    const WebCryptoAlgorithm&,
                    bool,
                    WebCryptoKeyUsageMask,
                    WebCryptoResult,
                    scoped_refptr<base::SingleThreadTaskRunner>));
  MOCK_METHOD7(DeserializeKeyForClone,
               bool(const WebCryptoKeyAlgorithm&,
                    WebCryptoKeyType,
                    bool,
                    WebCryptoKeyUsageMask,
                    const unsigned char*,
                    unsigned,
                    WebCryptoKey&));
  MOCK_METHOD2(SerializeKeyForClone,
               bool(const WebCryptoKey&, WebVector<unsigned char>&));

 protected:
  DISALLOW_COPY_AND_ASSIGN(MockWebCrypto);
};

}  // namespace blink

#endif
