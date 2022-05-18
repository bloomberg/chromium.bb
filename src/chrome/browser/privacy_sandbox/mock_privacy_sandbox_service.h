// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_MOCK_PRIVACY_SANDBOX_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_MOCK_PRIVACY_SANDBOX_SERVICE_H_

#include <memory>

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class BrowserContext;
}

class KeyedService;

class MockPrivacySandboxService : public PrivacySandboxService {
 public:
  MockPrivacySandboxService();
  ~MockPrivacySandboxService() override;

  MOCK_METHOD(void,
              DialogActionOccurred,
              (PrivacySandboxService::DialogAction),
              (override));
};

std::unique_ptr<KeyedService> BuildMockPrivacySandboxService(
    content::BrowserContext* context);

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_MOCK_PRIVACY_SANDBOX_SERVICE_H_
