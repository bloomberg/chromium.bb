// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/stdafx.h"

#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_reg_util_win.h"
#include "base/time/time_override.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/associated_user_validator.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace {

base::string16 GetNewSidString(FakeOSUserManager* fake_os_user_manager) {
  PSID sid;
  HRESULT hr = fake_os_user_manager->CreateNewSID(&sid);
  if (FAILED(hr))
    return base::string16();

  LPWSTR sid_string;
  bool convert = ::ConvertSidToStringSid(sid, &sid_string);
  ::FreeSid(sid);

  base::string16 result;
  if (convert)
    result = (sid_string);

  ::LocalFree(sid_string);
  return result;
}

}  // namespace

namespace testing {

class AssociatedUserValidatorTest : public ::testing::Test {
 protected:
  void CreateDeletedGCPWUser(BSTR* sid) {
    PSID sid_deleted;
    ASSERT_EQ(S_OK, fake_os_user_manager_.CreateNewSID(&sid_deleted));
    wchar_t* user_sid_string = nullptr;
    ASSERT_TRUE(ConvertSidToStringSid(sid_deleted, &user_sid_string));
    *sid = SysAllocString(W2COLE(user_sid_string));

    ASSERT_EQ(S_OK, SetUserProperty(user_sid_string, kUserId, L"id_value"));
    ASSERT_EQ(S_OK,
              SetUserProperty(user_sid_string, kUserTokenHandle, L"th_value"));
    LocalFree(user_sid_string);
  }

  AssociatedUserValidatorTest();
  ~AssociatedUserValidatorTest() override;

  void SetUp() override;

  FakeOSUserManager* fake_os_user_manager() { return &fake_os_user_manager_; }
  FakeWinHttpUrlFetcherFactory* fake_http_url_fetcher_factory() {
    return &fake_http_url_fetcher_factory_;
  }

  FakeInternetAvailabilityChecker* fake_internet_checker() {
    return &fake_internet_checker_;
  }

 private:
  FakeOSUserManager fake_os_user_manager_;
  FakeWinHttpUrlFetcherFactory fake_http_url_fetcher_factory_;
  registry_util::RegistryOverrideManager registry_override_;
  FakeInternetAvailabilityChecker fake_internet_checker_;
};

AssociatedUserValidatorTest::AssociatedUserValidatorTest() = default;
AssociatedUserValidatorTest ::~AssociatedUserValidatorTest() = default;

void AssociatedUserValidatorTest::SetUp() {
  InitializeRegistryOverrideForTesting(&registry_override_);
}

TEST_F(AssociatedUserValidatorTest, CleanupStaleUsers) {
  // Simulate a user created by GCPW that does not have a stale handle.
  CComBSTR sid_good;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username", L"password", L"Full Name", L"Comment",
                      L"gaia-id", L"foo@gmail.com", &sid_good));
  ASSERT_EQ(S_OK,
            SetUserProperty(OLE2W(sid_good), kUserTokenHandle, L"good-th"));

  // Simulate a user created by GCPW that was deleted from the machine.
  CComBSTR sid_bad;
  CreateDeletedGCPWUser(&sid_bad);

  // Simulate a user created by GCPW that has no gaia id.
  CComBSTR sid_no_gaia_id;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username2", L"password", L"Full Name", L"Comment", L"",
                      L"foo2@gmail.com", &sid_no_gaia_id));

  // Simulate a user created by GCPW that has a gaia id, but no token handle
  // set.
  CComBSTR sid_no_token_handle;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username3", L"password", L"Full Name", L"Comment",
                      L"gaia-id3", L"foo3@gmail.com", &sid_no_token_handle));
  // Clear the token handle automatically created by CreateTestOSUser.
  EXPECT_EQ(S_OK,
            SetUserProperty((BSTR)sid_no_token_handle, kUserTokenHandle, L""));

  wchar_t token_handle[256];
  DWORD length = base::size(token_handle);
  EXPECT_NE(S_OK, GetUserProperty((BSTR)sid_no_token_handle, kUserTokenHandle,
                                  token_handle, &length));

  // Create a token handle validator and start a refresh so that
  // stale token handles are cleaned.
  FakeAssociatedUserValidator validator;
  validator.StartRefreshingTokenHandleValidity();

  // Expect "good" sid to still in the registry.
  base::win::RegKey key;
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, kGcpUsersRootKeyName, KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS, key.OpenKey(OLE2CW(sid_good), KEY_READ));

  // For all bad users, token handle should be valid since the assumption is
  // that if no user entry is found internally in the validator then it is an
  // unassociated user and thus always has a valid token handle.
  EXPECT_TRUE(validator.IsTokenHandleValidForUser(OLE2CW(sid_bad)));
  EXPECT_TRUE(validator.IsTokenHandleValidForUser(OLE2CW(sid_no_gaia_id)));
  EXPECT_FALSE(
      validator.IsTokenHandleValidForUser(OLE2CW(sid_no_token_handle)));

  // Expect deleted user and user with no gaia id to be deleted.
  EXPECT_NE(ERROR_SUCCESS, key.OpenKey(OLE2CW(sid_bad), KEY_READ));
  EXPECT_NE(ERROR_SUCCESS, key.OpenKey(OLE2CW(sid_no_gaia_id), KEY_READ));

  // Expect user with no token handle to still not have a token handle set in
  // the registry.
  length = base::size(token_handle);
  EXPECT_NE(S_OK, GetUserProperty((BSTR)sid_no_token_handle, kUserTokenHandle,
                                  token_handle, &length));
}

TEST_F(AssociatedUserValidatorTest, NoTokenHandles) {
  FakeAssociatedUserValidator validator;

  validator.StartRefreshingTokenHandleValidity();

  // If there is no associated user then all token handles are valid.
  EXPECT_TRUE(validator.IsTokenHandleValidForUser(
      GetNewSidString(fake_os_user_manager())));
  EXPECT_EQ(0u, fake_http_url_fetcher_factory()->requests_created());
}

TEST_F(AssociatedUserValidatorTest, ValidTokenHandle) {
  FakeAssociatedUserValidator validator;

  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username", L"password", L"fullname", L"comment",
                      L"gaia-id", base::string16(), &sid));

  // Valid token fetch result.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(AssociatedUserValidator::kTokenInfoUrl),
      FakeWinHttpUrlFetcher::Headers(), "{\"expires_in\":1}");

  validator.StartRefreshingTokenHandleValidity();

  EXPECT_TRUE(validator.IsTokenHandleValidForUser(OLE2W(sid)));
  EXPECT_EQ(1u, fake_http_url_fetcher_factory()->requests_created());
}

TEST_F(AssociatedUserValidatorTest, InvalidTokenHandle) {
  FakeAssociatedUserValidator validator;

  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username", L"password", L"fullname", L"comment",
                      L"gaia-id", base::string16(), &sid));

  // Invalid token fetch result.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(AssociatedUserValidator::kTokenInfoUrl),
      FakeWinHttpUrlFetcher::Headers(), "{}");

  validator.StartRefreshingTokenHandleValidity();

  EXPECT_FALSE(validator.IsTokenHandleValidForUser(OLE2W(sid)));
  EXPECT_EQ(1u, fake_http_url_fetcher_factory()->requests_created());
}

TEST_F(AssociatedUserValidatorTest, InvalidTokenHandleNoInternet) {
  FakeAssociatedUserValidator validator;
  fake_internet_checker()->SetHasInternetConnection(
      FakeInternetAvailabilityChecker::kHicForceNo);

  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username", L"password", L"fullname", L"comment",
                      L"gaia-id", base::string16(), &sid));

  validator.StartRefreshingTokenHandleValidity();
  EXPECT_TRUE(validator.IsTokenHandleValidForUser(OLE2W(sid)));
  EXPECT_EQ(0u, fake_http_url_fetcher_factory()->requests_created());
}

TEST_F(AssociatedUserValidatorTest, InvalidTokenHandleTimeout) {
  FakeAssociatedUserValidator validator(base::TimeDelta::FromMilliseconds(50));
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username", L"password", L"fullname", L"comment",
                      L"gaia-id", base::string16(), &sid));

  base::WaitableEvent http_fetcher_event;
  // Invalid token fetch result.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(AssociatedUserValidator::kTokenInfoUrl),
      FakeWinHttpUrlFetcher::Headers(), "{}", http_fetcher_event.handle());
  validator.StartRefreshingTokenHandleValidity();

  EXPECT_TRUE(validator.IsTokenHandleValidForUser(OLE2W(sid)));
  EXPECT_EQ(1u, fake_http_url_fetcher_factory()->requests_created());

  http_fetcher_event.Signal();
}

TEST_F(AssociatedUserValidatorTest, TokenHandleValidityStillFresh) {
  FakeAssociatedUserValidator validator;

  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username", L"password", L"fullname", L"comment",
                      L"gaia-id", base::string16(), &sid));

  // Valid token fetch result.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(AssociatedUserValidator::kTokenInfoUrl),
      FakeWinHttpUrlFetcher::Headers(), "{\"expires_in\":1}");

  validator.StartRefreshingTokenHandleValidity();

  EXPECT_TRUE(validator.IsTokenHandleValidForUser(OLE2W(sid)));
  EXPECT_TRUE(validator.IsTokenHandleValidForUser(OLE2W(sid)));
  EXPECT_EQ(1u, fake_http_url_fetcher_factory()->requests_created());
}

TEST_F(AssociatedUserValidatorTest, BlockDenyUserAccess) {
  FakeAssociatedUserValidator validator;

  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));

  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username", L"password", L"fullname", L"comment",
                      L"gaia-id", base::string16(), &sid));

  // Invalid token fetch result.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(AssociatedUserValidator::kTokenInfoUrl),
      FakeWinHttpUrlFetcher::Headers(), "{}");

  validator.StartRefreshingTokenHandleValidity();

  // Apply two levels blocks to deny access. This should prevent users from
  // being blocked from accessing the system.
  {
    AssociatedUserValidator::ScopedBlockDenyAccessUpdate deny_blocker_outer(
        &validator);
    {
      AssociatedUserValidator::ScopedBlockDenyAccessUpdate deny_blocker_inner(
          &validator);
      EXPECT_FALSE(
          validator.DenySigninForUsersWithInvalidTokenHandles(CPUS_LOGON));
      EXPECT_FALSE(validator.IsUserAccessBlocked(OLE2W(sid)));
    }

    EXPECT_FALSE(
        validator.DenySigninForUsersWithInvalidTokenHandles(CPUS_LOGON));
    EXPECT_FALSE(validator.IsUserAccessBlocked(OLE2W(sid)));
  }
  // Unblock deny access. User should not be blocked.
  EXPECT_TRUE(validator.DenySigninForUsersWithInvalidTokenHandles(CPUS_LOGON));
  EXPECT_TRUE(validator.IsUserAccessBlocked(OLE2W(sid)));

  EXPECT_EQ(1u, fake_http_url_fetcher_factory()->requests_created());
}

// Tests various scenarios where user access is blocked.
// Parameters are:
// 1. CREDENTIAL_PROVIDER_USAGE_SCENARIO - Usage scenario.
// 2. bool - User token handle is valid.
// 3. bool - Mdm url is set.
// 4. bool - Mdm enrollment is already done.
// 5. bool - Internet is available.
class AssociatedUserValidatorUserAccessBlockingTest
    : public AssociatedUserValidatorTest,
      public ::testing::WithParamInterface<
          std::tuple<CREDENTIAL_PROVIDER_USAGE_SCENARIO,
                     bool,
                     bool,
                     bool,
                     bool>> {
 private:
  FakeScopedLsaPolicyFactory fake_scoped_lsa_policy_factory_;
};

TEST_P(AssociatedUserValidatorUserAccessBlockingTest, BlockUserAccessAsNeeded) {
  const CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus = std::get<0>(GetParam());
  const bool token_handle_valid = std::get<1>(GetParam());
  const bool mdm_url_set = std::get<2>(GetParam());
  const bool mdm_enrolled = std::get<3>(GetParam());
  const bool internet_available = std::get<4>(GetParam());
  GoogleMdmEnrolledStatusForTesting forced_status(mdm_enrolled);

  FakeAssociatedUserValidator validator;
  fake_internet_checker()->SetHasInternetConnection(
      internet_available ? FakeInternetAvailabilityChecker::kHicForceYes
                         : FakeInternetAvailabilityChecker::kHicForceNo);

  if (mdm_url_set)
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));

  bool should_user_locking_be_enabled =
      internet_available && mdm_url_set &&
      CGaiaCredentialProvider::IsUsageScenarioSupported(cpus);

  EXPECT_EQ(should_user_locking_be_enabled,
            validator.IsUserAccessBlockingEnforced(cpus));

  CComBSTR sid;
  constexpr wchar_t username[] = L"username";
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      username, L"password", L"fullname", L"comment",
                      L"gaia-id", base::string16(), &sid));

  // Token handle fetch result.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(AssociatedUserValidator::kTokenInfoUrl),
      FakeWinHttpUrlFetcher::Headers(),
      token_handle_valid ? "{\"expires_in\":1}" : "{}");

  validator.StartRefreshingTokenHandleValidity();
  validator.DenySigninForUsersWithInvalidTokenHandles(cpus);

  DWORD reg_value = 0;

  bool should_user_be_blocked =
      should_user_locking_be_enabled && (!mdm_enrolled || !token_handle_valid);

  EXPECT_EQ(!internet_available || (!mdm_url_set && token_handle_valid) ||
                (mdm_url_set && mdm_enrolled && token_handle_valid),
            validator.IsTokenHandleValidForUser(OLE2W(sid)));
  EXPECT_EQ(should_user_be_blocked, validator.IsUserAccessBlocked(OLE2W(sid)));

  // Unlock the user.
  validator.AllowSigninForUsersWithInvalidTokenHandles();

  EXPECT_EQ(false, validator.IsUserAccessBlocked(OLE2W(sid)));
  EXPECT_NE(S_OK,
            GetMachineRegDWORD(kWinlogonUserListRegKey, username, &reg_value));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AssociatedUserValidatorUserAccessBlockingTest,
    ::testing::Combine(::testing::Values(CPUS_INVALID,
                                         CPUS_LOGON,
                                         CPUS_UNLOCK_WORKSTATION,
                                         CPUS_CHANGE_PASSWORD,
                                         CPUS_CREDUI),
                       ::testing::Bool(),
                       ::testing::Bool(),
                       ::testing::Bool(),
                       ::testing::Bool()));

class TimeClockOverrideValue {
 public:
  static base::Time NowOverride() { return current_time_; }
  static base::Time current_time_;
};

base::Time TimeClockOverrideValue::current_time_;

TEST_F(AssociatedUserValidatorTest, ValidTokenHandle_Refresh) {
  // Save the current time and then override the time clock to return a fake
  // time.
  TimeClockOverrideValue::current_time_ = base::Time::Now();
  base::subtle::ScopedTimeClockOverrides time_override(
      &TimeClockOverrideValue::NowOverride, nullptr, nullptr);
  FakeAssociatedUserValidator validator;

  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username", L"password", L"fullname", L"comment",
                      L"gaia-id", base::string16(), &sid));
  ASSERT_EQ(S_OK, SetUserProperty(OLE2W(sid), kUserTokenHandle, L"th"));

  // Valid token fetch result.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(AssociatedUserValidator::kTokenInfoUrl),
      FakeWinHttpUrlFetcher::Headers(), "{\"expires_in\":1}");

  validator.StartRefreshingTokenHandleValidity();

  EXPECT_TRUE(validator.IsTokenHandleValidForUser(OLE2W(sid)));

  // Make the next token fetch result invalid.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(AssociatedUserValidator::kTokenInfoUrl),
      FakeWinHttpUrlFetcher::Headers(), "{}");

  // If the lifetime of the validity has not expired, even if the token is
  // invalid, no new fetch will be performed yet.
  EXPECT_TRUE(validator.IsTokenHandleValidForUser(OLE2W(sid)));
  EXPECT_EQ(1u, fake_http_url_fetcher_factory()->requests_created());

  // Advance the time so that a new fetch will be done and retrieve the
  // invalid result now.
  TimeClockOverrideValue::current_time_ +=
      AssociatedUserValidator::kTokenHandleValidityLifetime +
      base::TimeDelta::FromMilliseconds(1);
  EXPECT_FALSE(validator.IsTokenHandleValidForUser(OLE2W(sid)));
  EXPECT_EQ(2u, fake_http_url_fetcher_factory()->requests_created());
}

}  // namespace testing

}  // namespace credential_provider
