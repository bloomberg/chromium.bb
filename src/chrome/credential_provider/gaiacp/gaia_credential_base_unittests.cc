// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_base.h"
#include "chrome/credential_provider/gaiacp/gaia_resources.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/test/gls_runner_test_base.h"
#include "chrome/credential_provider/test/test_credential.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace testing {

class GcpGaiaCredentialBaseTest : public GlsRunnerTestBase {};

TEST_F(GcpGaiaCredentialBaseTest, Advise) {
  // Create provider with credentials. This should Advise the credential.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  // Release ref count so the credential can be deleted by the call to
  // ReleaseProvider.
  cred.Release();

  // Release the provider. This should unadvise the credential.
  ASSERT_EQ(S_OK, ReleaseProvider());
}

TEST_F(GcpGaiaCredentialBaseTest, SetSelected) {
  // Create provider and credential only.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  // A credential that has not attempted to sign in a user yet should return
  // false for |auto_login|.
  BOOL auto_login;
  ASSERT_EQ(S_OK, cred->SetSelected(&auto_login));
  ASSERT_FALSE(auto_login);
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_NoInternet) {
  FakeInternetAvailabilityChecker internet_checker(
      FakeInternetAvailabilityChecker::kHicForceNo);

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  ASSERT_EQ(S_OK, StartLogonProcess(/*succeeds=*/false));
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_Start) {
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_Finish) {
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  // Make sure a "foo" user was created.
  PSID sid;
  EXPECT_EQ(S_OK, fake_os_user_manager()->GetUserSID(
                      OSUserManager::GetLocalDomain().c_str(), kDefaultUsername,
                      &sid));
  ::LocalFree(sid);

  // New user should be created.
  EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_Abort) {
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetDefaultExitCode(kUiecAbort));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Logon process should not signal credentials change or raise an error
  // message.
  ASSERT_EQ(S_OK, FinishLogonProcess(false, false, 0));
}

TEST_F(GcpGaiaCredentialBaseTest,
       GetSerialization_AssociateToMatchingAssociatedUser) {
  USES_CONVERSION;
  // Create a fake user that has the same gaia id as the test gaia id.
  CComBSTR first_sid;
  base::string16 username(L"foo");
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      username, L"password", L"name", L"comment",
                      base::UTF8ToUTF16(kDefaultGaiaId), base::string16(),
                      &first_sid));
  ASSERT_EQ(2ul, fake_os_user_manager()->GetUserCount());
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // User should have been associated.
  EXPECT_EQ(test->GetFinalUsername(), username);
  // Email should be the same as the default one.
  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  // No new user should be created.
  EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_MultipleCalls) {
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  constexpr wchar_t kStartGlsEventName[] =
      L"GetSerialization_MultipleCalls_Wait";
  base::win::ScopedHandle start_event_handle(
      ::CreateEvent(nullptr, false, false, kStartGlsEventName));
  ASSERT_TRUE(start_event_handle.IsValid());
  ASSERT_EQ(S_OK, test->SetStartGlsEventName(kStartGlsEventName));
  base::WaitableEvent start_event(std::move(start_event_handle));

  ASSERT_EQ(S_OK, StartLogonProcess(/*succeeds=*/true));

  // Calling GetSerialization again while the credential is waiting for the
  // logon process should yield CPGSR_NO_CREDENTIAL_NOT_FINISHED as a
  // response.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  EXPECT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(nullptr, status_text);
  EXPECT_EQ(CPSI_NONE, status_icon);
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Signal that the gls process can finish.
  start_event.Signal();

  ASSERT_EQ(S_OK, WaitForLogonProcess());
}

TEST_F(GcpGaiaCredentialBaseTest,
       GetSerialization_PasswordChangedForAssociatedUser) {
  USES_CONVERSION;

  // Create a fake user for which the windows password does not match the gaia
  // password supplied by the test gls process.
  CComBSTR sid;
  CComBSTR windows_password = L"password2";
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                L"foo", (BSTR)windows_password, L"Full Name", L"comment",
                base::UTF8ToUTF16(kDefaultGaiaId), base::string16(), &sid));

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_EQ(S_OK, test->IsWindowsPasswordValidForStoredUser());

  // Check that the process has not finished yet.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(nullptr, status_text);
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Credentials should still be available.
  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_EQ(S_OK, test->IsWindowsPasswordValidForStoredUser());

  // Set an invalid password and try to get serialization again. Credentials
  // should still be valid but serialization is not complete.
  CComBSTR invalid_windows_password = L"a";
  cred->SetStringValue(FID_CURRENT_PASSWORD_FIELD, invalid_windows_password);
  EXPECT_EQ(nullptr, status_text);
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Update the Windows password to be the real password created for the user.
  cred->SetStringValue(FID_CURRENT_PASSWORD_FIELD, windows_password);
  // Sign in information should still be available.
  EXPECT_TRUE(test->GetFinalEmail().length());

  // Both Windows and Gaia credentials should be valid now
  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_EQ(S_FALSE, test->IsWindowsPasswordValidForStoredUser());

  // Finish logon successfully but with no credential changed event.
  ASSERT_EQ(S_OK, FinishLogonProcess(true, false, 0));
}

TEST_F(GcpGaiaCredentialBaseTest,
       GetSerialization_ForgotPasswordForAssociatedUser) {
  USES_CONVERSION;

  // Create a fake user for which the windows password does not match the gaia
  // password supplied by the test gls process.
  CComBSTR sid;
  CComBSTR windows_password = L"password2";
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                L"foo", (BSTR)windows_password, L"Full Name", L"comment",
                base::UTF8ToUTF16(kDefaultGaiaId), base::string16(), &sid));

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_EQ(S_OK, test->IsWindowsPasswordValidForStoredUser());

  // Check that the process has not finished yet.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(nullptr, status_text);
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Credentials should still be available.
  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_EQ(S_OK, test->IsWindowsPasswordValidForStoredUser());

  // Simulate a click on the "Forgot Password" link.
  cred->CommandLinkClicked(FID_FORGOT_PASSWORD_LINK);

  // Finish logon successfully but with no credential changed event.
  ASSERT_EQ(S_OK, FinishLogonProcess(true, false, 0));
}

TEST_F(GcpGaiaCredentialBaseTest,
       GetSerialization_AlternateForgotPasswordAssociatedUser) {
  USES_CONVERSION;

  // Create a fake user for which the windows password does not match the gaia
  // password supplied by the test gls process.
  CComBSTR sid;
  CComBSTR windows_password = L"password2";
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                L"foo", (BSTR)windows_password, L"Full Name", L"comment",
                base::UTF8ToUTF16(kDefaultGaiaId), base::string16(), &sid));

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_EQ(S_OK, test->IsWindowsPasswordValidForStoredUser());

  // Check that the process has not finished yet.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(nullptr, status_text);
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Credentials should still be available.
  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_EQ(S_OK, test->IsWindowsPasswordValidForStoredUser());

  // Simulate a click on the "Forgot Password" link.
  cred->CommandLinkClicked(FID_FORGOT_PASSWORD_LINK);

  // Go back to windows password entry.
  cred->CommandLinkClicked(FID_FORGOT_PASSWORD_LINK);

  // Set an invalid password and try to get serialization again. Credentials
  // should still be valid but serialization is not complete.
  CComBSTR invalid_windows_password = L"a";
  cred->SetStringValue(FID_CURRENT_PASSWORD_FIELD, invalid_windows_password);
  EXPECT_EQ(nullptr, status_text);
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Update the Windows password to be the real password created for the user.
  cred->SetStringValue(FID_CURRENT_PASSWORD_FIELD, windows_password);
  // Sign in information should still be available.
  EXPECT_TRUE(test->GetFinalEmail().length());

  // Both Windows and Gaia credentials should be valid now
  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_EQ(S_FALSE, test->IsWindowsPasswordValidForStoredUser());

  // Finish logon successfully but with no credential changed event.
  ASSERT_EQ(S_OK, FinishLogonProcess(true, false, 0));
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_Cancel) {
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  // This event is merely used to keep the gls running while it is cancelled
  // through SetDeselected().
  constexpr wchar_t kStartGlsEventName[] = L"GetSerialization_Cancel_Signal";
  base::win::ScopedHandle start_event_handle(
      ::CreateEvent(nullptr, false, false, kStartGlsEventName));
  ASSERT_TRUE(start_event_handle.IsValid());
  ASSERT_EQ(S_OK, test->SetStartGlsEventName(kStartGlsEventName));
  base::WaitableEvent start_event(std::move(start_event_handle));

  ASSERT_EQ(S_OK, StartLogonProcess(/*succeeds=*/true));

  // Deselect the credential provider so that it cancels the GLS process and
  // returns.
  ASSERT_EQ(S_OK, cred->SetDeselected());

  ASSERT_EQ(S_OK, WaitForLogonProcess());

  // Logon process should not signal credentials change or raise an error
  // message.
  ASSERT_EQ(S_OK, FinishLogonProcess(false, false, 0));
}

TEST_F(GcpGaiaCredentialBaseTest, FailedUserCreation) {
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  // Fail user creation.
  fake_os_user_manager()->SetShouldFailUserCreation(true);

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Logon process should fail with an internal error.
  ASSERT_EQ(S_OK, FinishLogonProcess(false, false, IDS_INTERNAL_ERROR_BASE));
}

TEST_F(GcpGaiaCredentialBaseTest, StripEmailTLD) {
  USES_CONVERSION;
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  constexpr char email[] = "foo@imfl.info";

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"foo_imfl"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, NewUserDisabledThroughUsageScenario) {
  USES_CONVERSION;
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  // Set the other user tile so that we can get the anonymous credential
  // that may try create a new user.
  fake_user_array()->SetAccountOptions(CPAO_EMPTY_LOCAL);

  SetUsageScenario(CPUS_UNLOCK_WORKSTATION);
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Sign in should fail with an error stating that no new users can be created.
  ASSERT_EQ(S_OK, FinishLogonProcess(false, false,
                                     IDS_INVALID_UNLOCK_WORKSTATION_USER_BASE));
}

TEST_F(GcpGaiaCredentialBaseTest, NewUserDisabledThroughMdm) {
  USES_CONVERSION;
  // Enforce single user mode for MDM.
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 0));
  GoogleMdmEnrolledStatusForTesting force_success(true);

  // Create a fake user that is already associated so when the user tries to
  // sign on and create a new user, it fails.
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"foo_registered", L"password", L"name", L"comment",
                      L"gaia-id-registered", base::string16(), &sid));

  // Populate the associated users list. The created user's token handle
  // should be valid so that no reauth credential is created.
  fake_associated_user_validator()->StartRefreshingTokenHandleValidity();

  // Set the other user tile so that we can get the anonymous credential
  // that may try to sign in a user.
  fake_user_array()->SetAccountOptions(CPAO_EMPTY_LOCAL);

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Sign in should fail with an error stating that no new users can be created.
  ASSERT_EQ(S_OK,
            FinishLogonProcess(false, false, IDS_ADD_USER_DISALLOWED_BASE));
}

TEST_F(GcpGaiaCredentialBaseTest, InvalidUserUnlockedAfterSignin) {
  // Enforce token handle verification with user locking when the token handle
  // is not valid.
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));
  GoogleMdmEnrollmentStatusForTesting force_success(true);

  USES_CONVERSION;
  // Create a fake user that has the same gaia id as the test gaia id.
  CComBSTR sid;
  base::string16 username(L"foo");
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                username, L"password", L"name", L"comment",
                base::UTF8ToUTF16(kDefaultGaiaId), base::string16(), &sid));
  ASSERT_EQ(2ul, fake_os_user_manager()->GetUserCount());

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  // Create with invalid token handle response.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  // User should have invalid token handle and be locked.
  EXPECT_FALSE(
      fake_associated_user_validator()->IsTokenHandleValidForUser(OLE2W(sid)));
  EXPECT_EQ(true,
            fake_associated_user_validator()->IsUserAccessBlockedForTesting(
                OLE2W(sid)));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // User should have been associated.
  EXPECT_EQ(test->GetFinalUsername(), username);
  // Email should be the same as the default one.
  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  // Now finish the logon, this should unlock the user.
  ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));

  EXPECT_EQ(false,
            fake_associated_user_validator()->IsUserAccessBlockedForTesting(
                OLE2W(sid)));

  // No new user should be created.
  EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());
}

TEST_F(GcpGaiaCredentialBaseTest, DenySigninBlockedDuringSignin) {
  USES_CONVERSION;

  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));
  GoogleMdmEnrolledStatusForTesting force_success(true);

  // Create a fake user that has the same gaia id as the test gaia id.
  CComBSTR first_sid;
  base::string16 username(L"foo");
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      username, L"password", L"name", L"comment",
                      base::UTF8ToUTF16(kDefaultGaiaId), base::string16(),
                      &first_sid));
  ASSERT_EQ(2ul, fake_os_user_manager()->GetUserCount());

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  // Create with valid token handle response and sign in the anonymous
  // credential with the user that should still be valid.
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Change token response to an invalid one.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(AssociatedUserValidator::kTokenInfoUrl),
      FakeWinHttpUrlFetcher::Headers(), "{}");

  // Force refresh of all token handles on the next query.
  fake_associated_user_validator()->ForceRefreshTokenHandlesForTesting();

  // Signin process has already started. User should not be locked even if their
  // token handle is invalid.
  EXPECT_FALSE(fake_associated_user_validator()
                   ->DenySigninForUsersWithInvalidTokenHandles(CPUS_LOGON));
  EXPECT_FALSE(fake_associated_user_validator()->IsUserAccessBlockedForTesting(
      OLE2W(first_sid)));

  // Now finish the logon.
  ASSERT_EQ(S_OK, FinishLogonProcessWithCred(true, true, 0, cred));

  // User should have been associated.
  EXPECT_EQ(test->GetFinalUsername(), username);
  // Email should be the same as the default one.
  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  // Result has not been reported yet, user signin should still not be denied.
  EXPECT_FALSE(fake_associated_user_validator()
                   ->DenySigninForUsersWithInvalidTokenHandles(CPUS_LOGON));
  EXPECT_FALSE(fake_associated_user_validator()->IsUserAccessBlockedForTesting(
      OLE2W(first_sid)));

  ReportLogonProcessResult(cred);

  // Now signin can be denied for the user if their token handle is invalid.
  EXPECT_TRUE(fake_associated_user_validator()
                  ->DenySigninForUsersWithInvalidTokenHandles(CPUS_LOGON));
  EXPECT_TRUE(fake_associated_user_validator()->IsUserAccessBlockedForTesting(
      OLE2W(first_sid)));

  // No new user should be created.
  EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());
}

TEST_F(GcpGaiaCredentialBaseTest, StripEmailTLD_Gmail) {
  USES_CONVERSION;

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  constexpr char email[] = "bar@gmail.com";

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"bar"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, StripEmailTLD_Googlemail) {
  USES_CONVERSION;

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  constexpr char email[] = "toto@googlemail.com";

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"toto"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, InvalidUsernameCharacters) {
  USES_CONVERSION;
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  constexpr char email[] = "a\\[]:|<>+=;?*z@gmail.com";

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"a____________z"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, EmailTooLong) {
  USES_CONVERSION;

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  constexpr char email[] = "areallylongemailadressdude@gmail.com";

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"areallylongemailadre"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, EmailTooLong2) {
  USES_CONVERSION;
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  constexpr char email[] = "foo@areallylongdomaindude.com";

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"foo_areallylongdomai"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, EmailIsNoAt) {
  USES_CONVERSION;
  constexpr char email[] = "foo";

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"foo_gmail"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, EmailIsAtCom) {
  USES_CONVERSION;

  constexpr char email[] = "@com";

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"_com"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, EmailIsAtDotCom) {
  USES_CONVERSION;

  constexpr char email[] = "@.com";

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"_.com"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

// Tests various sign in scenarios with consumer and non-consumer domains.
// Parameters are:
// 1. Is mdm enrollment enabled.
// 2. The mdm_aca reg key setting:
//    - 0: Set reg key to 0.
//    - 1: Set reg key to 1.
//    - 2: Don't set reg key.
// 3. Whether the mdm_aca reg key is set to 1 or 0.
// 4. Whether an existing associated user is already present.
// 5. Whether the user being created (or existing) uses a consumer account.
class GcpGaiaCredentialBaseConsumerEmailTest
    : public GcpGaiaCredentialBaseTest,
      public ::testing::WithParamInterface<std::tuple<bool, int, bool, bool>> {
};

TEST_P(GcpGaiaCredentialBaseConsumerEmailTest, ConsumerEmailSignin) {
  USES_CONVERSION;
  const bool mdm_enabled = std::get<0>(GetParam());
  const int mdm_consumer_accounts_reg_key_setting = std::get<1>(GetParam());
  const bool user_created = std::get<2>(GetParam());
  const bool user_is_consumer = std::get<3>(GetParam());

  FakeAssociatedUserValidator validator;
  FakeInternetAvailabilityChecker internet_checker;
  GoogleMdmEnrollmentStatusForTesting force_success(true);

  if (mdm_enabled)
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));

  const bool mdm_consumer_accounts_reg_key_set =
      mdm_consumer_accounts_reg_key_setting >= 0 &&
      mdm_consumer_accounts_reg_key_setting < 2;
  if (mdm_consumer_accounts_reg_key_set) {
    ASSERT_EQ(S_OK,
              SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts,
                                      mdm_consumer_accounts_reg_key_setting));
  }

  std::string user_email = user_is_consumer ? kDefaultEmail : "foo@imfl.info";

  CComBSTR sid;
  base::string16 username(user_is_consumer ? L"foo" : L"foo_imfl");

  // Create a fake user that has the same gaia id as the test gaia id.
  if (user_created) {
    ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                        username, L"password", L"name", L"comment",
                        base::UTF8ToUTF16(kDefaultGaiaId),
                        base::UTF8ToUTF16(user_email), &sid));
    ASSERT_EQ(2ul, fake_os_user_manager()->GetUserCount());
  }

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  test->SetGlsEmailAddress(user_email);

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  bool should_signin_succeed = !mdm_enabled ||
                               (mdm_consumer_accounts_reg_key_set &&
                                mdm_consumer_accounts_reg_key_setting) ||
                               !user_is_consumer;

  // Sign in success.
  if (should_signin_succeed) {
    // User should have been associated.
    EXPECT_EQ(test->GetFinalUsername(), username);
    // Email should be the same as the default one.
    EXPECT_EQ(test->GetFinalEmail(), user_email);

    ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));
  } else {
    // Error message concerning invalid domain is sent.
    ASSERT_EQ(S_OK,
              FinishLogonProcess(false, false, IDS_INVALID_EMAIL_DOMAIN_BASE));
  }

  if (user_created) {
    // No new user should be created.
    EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());
  } else {
    // New user created only if their domain is valid for the sign in.
    EXPECT_EQ(1ul + (should_signin_succeed ? 1ul : 0ul),
              fake_os_user_manager()->GetUserCount());
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         GcpGaiaCredentialBaseConsumerEmailTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Values(0, 1, 2),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

}  // namespace testing
}  // namespace credential_provider
