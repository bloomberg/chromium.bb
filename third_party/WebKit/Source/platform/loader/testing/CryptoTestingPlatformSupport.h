// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CryptoTestingPlatformSupport_h
#define CryptoTestingPlatformSupport_h

#include <memory>
#include "platform/loader/testing/FetchTestingPlatformSupport.h"
#include "platform/testing/MockWebCrypto.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CryptoTestingPlatformSupport : public FetchTestingPlatformSupport {
 public:
  CryptoTestingPlatformSupport() {}
  ~CryptoTestingPlatformSupport() override {}

  // Platform:
  WebCrypto* Crypto() override { return mock_web_crypto_.get(); }

  class SetMockCryptoScope final {
    STACK_ALLOCATED();

   public:
    explicit SetMockCryptoScope(CryptoTestingPlatformSupport& platform)
        : platform_(platform) {
      DCHECK(!platform_.Crypto());
      platform_.SetMockCrypto(MockWebCrypto::Create());
    }
    ~SetMockCryptoScope() { platform_.SetMockCrypto(nullptr); }
    MockWebCrypto& MockCrypto() { return *platform_.mock_web_crypto_; }

   private:
    CryptoTestingPlatformSupport& platform_;
  };

 private:
  void SetMockCrypto(std::unique_ptr<MockWebCrypto> crypto) {
    mock_web_crypto_ = std::move(crypto);
  }

  std::unique_ptr<MockWebCrypto> mock_web_crypto_;

  DISALLOW_COPY_AND_ASSIGN(CryptoTestingPlatformSupport);
};

}  // namespace blink

#endif
