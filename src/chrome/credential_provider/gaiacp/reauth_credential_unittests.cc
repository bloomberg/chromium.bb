// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>
#include <atlcomcli.h>

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win_test_data.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/gaia_resources.h"
#include "chrome/credential_provider/gaiacp/reauth_credential.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/test/com_fakes.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "chrome/credential_provider/test/gls_runner_test_base.h"
#include "chrome/credential_provider/test/test_credential.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace testing {

class GcpReauthCredentialTest : public ::testing::Test {
 protected:
  FakeOSUserManager* fake_os_user_manager() { return &fake_os_user_manager_; }

 private:
  void SetUp() override;

  registry_util::RegistryOverrideManager registry_override_;
  FakeOSUserManager fake_os_user_manager_;
  FakeScopedLsaPolicyFactory fake_scoped_lsa_policy_factory_;
};

void GcpReauthCredentialTest::SetUp() {
  InitializeRegistryOverrideForTesting(&registry_override_);
}

TEST_F(GcpReauthCredentialTest, SetOSUserInfoAndReauthEmail) {
  USES_CONVERSION;
  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  CComPtr<IReauthCredential> reauth;
  ASSERT_EQ(S_OK, CComCreator<CComObject<CReauthCredential>>::CreateInstance(
                      nullptr, IID_IReauthCredential, (void**)&reauth));
  ASSERT_TRUE(!!reauth);

  const CComBSTR kSid(W2COLE(L"sid"));
  ASSERT_EQ(S_OK, reauth->SetOSUserInfo(
                      kSid, CComBSTR(OSUserManager::GetLocalDomain().c_str()),
                      CComBSTR(W2COLE(L"username"))));
  ASSERT_EQ(S_OK, reauth->SetEmailForReauth(CComBSTR(
                      A2COLE(test_data_storage.GetSuccessEmail().c_str()))));

  CComPtr<ICredentialProviderCredential2> cpc2;
  ASSERT_EQ(S_OK, reauth->QueryInterface(IID_ICredentialProviderCredential2,
                                         reinterpret_cast<void**>(&cpc2)));
  wchar_t* sid;
  CComBSTR username;
  ASSERT_EQ(S_OK, cpc2->GetUserSid(&sid));
  ASSERT_EQ(kSid, CComBSTR(W2COLE(sid)));
  ::CoTaskMemFree(sid);
}

class GcpReauthCredentialGlsRunnerTest : public GlsRunnerTestBase {};

TEST_F(GcpReauthCredentialGlsRunnerTest, UserGaiaIdMismatch) {
  USES_CONVERSION;

  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  std::string unexpected_gaia_id = "unexpected-gaia-id";

  // Create an signin result with the unexpected gaia id.
  base::Value unexpected_full_result =
      test_data_storage.expected_full_result().Clone();
  unexpected_full_result.SetKey(kKeyId, base::Value(unexpected_gaia_id));
  std::string signin_result_utf8;
  EXPECT_TRUE(
      base::JSONWriter::Write(unexpected_full_result, &signin_result_utf8));
  CComBSTR unexpected_signin_result = A2COLE(signin_result_utf8.c_str());

  CComBSTR username = L"foo_bar";
  CComBSTR full_name = A2COLE(test_data_storage.GetSuccessFullName().c_str());
  CComBSTR password = A2COLE(test_data_storage.GetSuccessPassword().c_str());
  CComBSTR email = A2COLE(test_data_storage.GetSuccessEmail().c_str());

  // Create two fake users to reauth. One associated with the valid Gaia id
  // and the other associated to the invalid gaia id.
  CComBSTR first_sid;
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                OLE2CW(username), OLE2CW(password), OLE2CW(full_name),
                L"comment", base::UTF8ToUTF16(test_data_storage.GetSuccessId()),
                base::string16(), &first_sid));

  CComBSTR second_sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"foo_bar2", L"pwd2", L"name2", L"comment2",
                      base::UTF8ToUTF16(unexpected_gaia_id), base::string16(),
                      &second_sid));

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  // Create with invalid token handle response so that a reauth occurs.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(1, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  // Force the GLS to return an invalid Gaia Id without reporting the usual
  // kUiecEMailMissmatch exit code when this happens. This will test whether
  // the credential can perform necessary validation in case the GLS ever
  // does not do the validation for us.
  test->SetGaiaIdOverride(unexpected_gaia_id, /*ignore_expected_gaia_id=*/true);

  // The logon should have failed with an error about another user already
  // associated to this Google account.
  ASSERT_EQ(S_OK, FinishLogonProcess(false, false, IDS_ACCOUNT_IN_USE_BASE));
}

TEST_F(GcpReauthCredentialGlsRunnerTest, NormalReauth) {
  USES_CONVERSION;
  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  CComBSTR username = L"foo_bar";
  CComBSTR full_name = A2COLE(test_data_storage.GetSuccessFullName().c_str());
  CComBSTR password = A2COLE(test_data_storage.GetSuccessPassword().c_str());
  CComBSTR email = A2COLE(test_data_storage.GetSuccessEmail().c_str());

  // Create a fake user to reauth.
  CComBSTR sid;
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                OLE2CW(username), OLE2CW(password), OLE2CW(full_name),
                L"comment", base::UTF8ToUTF16(test_data_storage.GetSuccessId()),
                OLE2CW(email), &sid));

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  // Create with invalid token handle response so that a reauth occurs.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(1, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(std::string()));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Teardown of the test should confirm that the logon was successful.
}

TEST_F(GcpReauthCredentialGlsRunnerTest, NormalReauthWithoutEmail) {
  USES_CONVERSION;
  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  CComBSTR username = L"foo_bar";
  CComBSTR full_name = A2COLE(test_data_storage.GetSuccessFullName().c_str());
  CComBSTR password = A2COLE(test_data_storage.GetSuccessPassword().c_str());
  CComBSTR email = A2COLE(test_data_storage.GetSuccessEmail().c_str());

  // Create a fake user to reauth with no e-mail specified.
  CComBSTR sid;
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                OLE2CW(username), OLE2CW(password), OLE2CW(full_name),
                L"comment", base::UTF8ToUTF16(test_data_storage.GetSuccessId()),
                base::string16(), &sid));

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  // Create with invalid token handle response so that a reauth occurs.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(1, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Email associated should be the default one
  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  // Teardown of the test should confirm that the logon was successful.
}

TEST_F(GcpReauthCredentialGlsRunnerTest, GaiaIdMismatch) {
  USES_CONVERSION;
  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  CComBSTR username = L"foo_bar";
  CComBSTR full_name = A2COLE(test_data_storage.GetSuccessFullName().c_str());
  CComBSTR password = A2COLE(test_data_storage.GetSuccessPassword().c_str());
  CComBSTR email = A2COLE(test_data_storage.GetSuccessEmail().c_str());

  // Create a fake user to reauth.
  CComBSTR sid;
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                OLE2CW(username), OLE2CW(password), OLE2CW(full_name),
                L"comment", base::UTF8ToUTF16(test_data_storage.GetSuccessId()),
                OLE2CW(email), &sid));

  std::string unexpected_gaia_id = "unexpected-gaia-id";

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  // Create with invalid token handle response so that a reauth occurs.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(1, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(std::string()));
  ASSERT_EQ(S_OK, test->SetGaiaIdOverride(unexpected_gaia_id,
                                          /*ignore_expected_gaia_id=*/false));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // The logon should have failed with an email mismatch error.
  ASSERT_EQ(S_OK, FinishLogonProcess(false, false, IDS_EMAIL_MISMATCH_BASE));
}

}  // namespace testing

}  // namespace credential_provider
