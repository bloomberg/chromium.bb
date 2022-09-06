// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_TEST_UTIL_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_TEST_UTIL_H_

#include <string>

#include "components/content_settings/core/common/content_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace sync_preferences {
class TestingPrefServiceSyncable;
}

class HostContentSettingsMap;

namespace privacy_sandbox_test_util {

class MockPrivacySandboxObserver
    : public privacy_sandbox::PrivacySandboxSettings::Observer {
 public:
  MockPrivacySandboxObserver();
  ~MockPrivacySandboxObserver();
  MOCK_METHOD(void, OnTopicsDataAccessibleSinceUpdated, (), (override));
  MOCK_METHOD1(OnTrustTokenBlockingChanged, void(bool));
};

class MockPrivacySandboxSettingsDelegate
    : public privacy_sandbox::PrivacySandboxSettings::Delegate {
 public:
  MockPrivacySandboxSettingsDelegate();
  ~MockPrivacySandboxSettingsDelegate() override;
  void SetupDefaultResponse(bool restricted, bool confirmed) {
    ON_CALL(*this, IsPrivacySandboxRestricted).WillByDefault([=]() {
      return restricted;
    });
    ON_CALL(*this, IsPrivacySandboxConfirmed).WillByDefault([=]() {
      return confirmed;
    });
  }
  MOCK_METHOD(bool, IsPrivacySandboxRestricted, (), (override));
  MOCK_METHOD(bool, IsPrivacySandboxConfirmed, (), (override));
};

// Define an additional content setting value to simulate an unmanaged default
// content setting.
const ContentSetting kNoSetting = static_cast<ContentSetting>(-1);

struct CookieContentSettingException {
  std::string primary_pattern;
  std::string secondary_pattern;
  ContentSetting content_setting;
};

// Sets up preferences and content settings based on provided parameters.
void SetupTestState(
    sync_preferences::TestingPrefServiceSyncable* testing_pref_service,
    HostContentSettingsMap* map,
    bool privacy_sandbox_enabled,
    bool block_third_party_cookies,
    ContentSetting default_cookie_setting,
    const std::vector<CookieContentSettingException>& user_cookie_exceptions,
    ContentSetting managed_cookie_setting,
    const std::vector<CookieContentSettingException>&
        managed_cookie_exceptions);

}  // namespace privacy_sandbox_test_util

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_TEST_UTIL_H_
