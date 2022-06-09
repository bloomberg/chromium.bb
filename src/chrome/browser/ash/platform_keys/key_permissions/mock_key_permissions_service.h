// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_MOCK_KEY_PERMISSIONS_SERVICE_H_
#define CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_MOCK_KEY_PERMISSIONS_SERVICE_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_service.h"
#include "chrome/browser/platform_keys/platform_keys.h"
#include "content/public/browser/browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class BrowserContext;
}

namespace ash {
namespace platform_keys {

class MockKeyPermissionsService : public KeyPermissionsService {
 public:
  MockKeyPermissionsService();
  ~MockKeyPermissionsService() override;

  MOCK_METHOD(void,
              CanUserGrantPermissionForKey,
              (const std::string& public_key_spki_der,
               CanUserGrantPermissionForKeyCallback callback),
              (override));

  MOCK_METHOD(void,
              IsCorporateKey,
              (const std::string& public_key_spki_der_b64,
               IsCorporateKeyCallback callback),
              (override));

  MOCK_METHOD(void,
              SetCorporateKey,
              (const std::string& public_key_spki_der_b64,
               SetCorporateKeyCallback callback),
              (override));
};

std::unique_ptr<KeyedService> BuildMockKeyPermissionsService(
    content::BrowserContext* browser_context);

}  // namespace platform_keys
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_MOCK_KEY_PERMISSIONS_SERVICE_H_
