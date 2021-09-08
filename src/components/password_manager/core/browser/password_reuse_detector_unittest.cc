// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_detector.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using testing::_;
using testing::IsEmpty;
using testing::UnorderedElementsAreArray;

namespace password_manager {

namespace {

using StringVector = std::vector<std::string>;

// Constants to make the tests more readable.
const absl::optional<PasswordHashData> NO_GAIA_OR_ENTERPRISE_REUSE =
    absl::nullopt;

struct TestData {
  // Comma separated list of domains.
  std::string domains;
  std::string username;
  std::string password;
};

std::vector<TestData> GetTestDomainsPasswords() {
  return {
      {"https://accounts.google.com", "gUsername", "saved_password"},
      {"https://facebook.com", "fbUsername", "123456789"},
      {"https://a.appspot.com", "appspotUsername", "abcdefghi"},
      {"https://twitter.com", "twitterUsername", "short"},
      {"https://example1.com", "example1Username", "secretword"},
      {"https://example2.com", "example2Username", "secretword"},
  };
}

std::unique_ptr<PasswordForm> GetForm(const std::string& domain,
                                      const std::string& username,
                                      const std::string& password) {
  auto form = std::make_unique<PasswordForm>();
  form->signon_realm = domain;
  form->password_value = ASCIIToUTF16(password);
  form->username_value = ASCIIToUTF16(username);
  return form;
}

// Convert a vector of TestData structs into a vector of PasswordForms.
std::vector<std::unique_ptr<PasswordForm>> GetForms(
    std::vector<TestData> test_data) {
  std::vector<std::unique_ptr<PasswordForm>> result;
  for (const auto& data : test_data) {
    // Some passwords are used on multiple domains.
    for (const auto& domain : base::SplitString(
             data.domains, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      result.push_back(GetForm(domain, data.username, data.password));
    }
  }
  return result;
}

PasswordStoreChangeList GetChangeList(
    PasswordStoreChange::Type type,
    const std::vector<std::unique_ptr<PasswordForm>>& forms) {
  PasswordStoreChangeList changes;
  for (const auto& form : forms)
    changes.push_back(PasswordStoreChange(type, *form));

  return changes;
}

std::vector<PasswordHashData> PrepareGaiaPasswordData(
    const std::vector<std::string>& passwords) {
  std::vector<PasswordHashData> result;
  for (const auto& password : passwords) {
    PasswordHashData password_hash("username_" + password,
                                   ASCIIToUTF16(password),
                                   /*force_update=*/true);
    result.push_back(password_hash);
  }
  return result;
}

std::vector<PasswordHashData> PrepareEnterprisePasswordData(
    const std::vector<std::string>& passwords) {
  std::vector<PasswordHashData> result;
  for (const auto& password : passwords) {
    PasswordHashData password_hash("enterpriseUsername_" + password,
                                   ASCIIToUTF16(password),
                                   /*force_update=*/false);
    result.push_back(password_hash);
  }
  return result;
}

void ConfigureEnterprisePasswordProtection(
    PasswordReuseDetector* reuse_detector) {
  absl::optional<std::vector<GURL>> login_urls =
      absl::make_optional<std::vector<GURL>>();
  login_urls->push_back(GURL("https://login.example.com"));
  reuse_detector->UseEnterprisePasswordURLs(
      login_urls, GURL("https://changepassword.example.com/"));
}

TEST(PasswordReuseDetectorTest, TypingPasswordOnDifferentSite) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _));
  reuse_detector.CheckReuse(u"123saved_passwo", "https://evil.com",
                            &mockConsumer);
  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _));
  reuse_detector.CheckReuse(u"123saved_passwor", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  std::vector<MatchingReusedCredential> credentials = {
      {"https://accounts.google.com", u"gUsername"}};
  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("saved_password"),
                               Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
                               UnorderedElementsAreArray(credentials), 5));
  reuse_detector.CheckReuse(u"123saved_password", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("saved_password"),
                               Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
                               UnorderedElementsAreArray(credentials), 5));
  reuse_detector.CheckReuse(u"saved_password", "https://evil.com",
                            &mockConsumer);

  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  credentials = {{"https://example1.com", u"example1Username"},
                 {"https://example2.com", u"example2Username"}};
  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("secretword"),
                               Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
                               UnorderedElementsAreArray(credentials), 5));
  reuse_detector.CheckReuse(u"abcdsecretword", "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, PSLMatchNoReuseEvent) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _));
  reuse_detector.CheckReuse(u"123456789", "https://m.facebook.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, NoPSLMatchReuseEvent) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  const std::vector<MatchingReusedCredential> credentials = {
      {"https://a.appspot.com", u"appspotUsername"}};
  // a.appspot.com and b.appspot.com are not PSL matches. So reuse event should
  // be raised.
  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("abcdefghi"),
                               Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
                               UnorderedElementsAreArray(credentials), 5));
  reuse_detector.CheckReuse(u"abcdefghi", "https://b.appspot.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, TooShortPasswordNoReuseEvent) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _));
  reuse_detector.CheckReuse(u"short", "evil.com", &mockConsumer);
}

TEST(PasswordReuseDetectorTest, PasswordNotInputSuffixNoReuseEvent) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _));
  reuse_detector.CheckReuse(u"password123", "https://evil.com", &mockConsumer);
  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _));
  reuse_detector.CheckReuse(u"123password456", "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, OnLoginsChanged) {
  for (PasswordStoreChange::Type type :
       {PasswordStoreChange::ADD, PasswordStoreChange::UPDATE,
        PasswordStoreChange::REMOVE}) {
    PasswordReuseDetector reuse_detector;
    PasswordStoreChangeList changes =
        GetChangeList(type, GetForms(GetTestDomainsPasswords()));
    reuse_detector.OnLoginsChanged(changes);
    MockPasswordReuseDetectorConsumer mockConsumer;

    if (type == PasswordStoreChange::REMOVE) {
      EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _));
    } else {
      const std::vector<MatchingReusedCredential> credentials = {
          {"https://accounts.google.com", u"gUsername"}};
      EXPECT_CALL(mockConsumer,
                  OnReuseCheckDone(true, strlen("saved_password"),
                                   Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
                                   UnorderedElementsAreArray(credentials), 5));
    }
    reuse_detector.CheckReuse(u"123saved_password", "https://evil.com",
                              &mockConsumer);
  }
}

TEST(PasswordReuseDetectorTest, AddAndRemoveSameLogin) {
  PasswordReuseDetector reuse_detector;
  std::vector<std::unique_ptr<PasswordForm>> login_credentials =
      GetForms(GetTestDomainsPasswords());
  // Add the test domain passwords into the saved passwords map.
  PasswordStoreChangeList add_changes =
      GetChangeList(PasswordStoreChange::ADD, login_credentials);
  reuse_detector.OnLoginsChanged(add_changes);

  const std::vector<MatchingReusedCredential>
      expected_matching_reused_credentials = {
          {"https://accounts.google.com", u"gUsername"}};
  MockPasswordReuseDetectorConsumer mockConsumer;
  // One of the passwords in |login_credentials| has less than the minimum
  // requirement of characters in a password so it will not be stored.
  int valid_passwords = login_credentials.size() - 1;
  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(
          /*is_reuse_found=*/true, strlen("saved_password"),
          Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
          UnorderedElementsAreArray(expected_matching_reused_credentials),
          valid_passwords));

  // "saved_password" is a substring of "123saved_password" so it should trigger
  // a reuse and get the matching credentials.
  reuse_detector.CheckReuse(u"123saved_password", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  // Remove the test domain passwords from the saved passwords map.
  PasswordStoreChangeList remove_changes =
      GetChangeList(PasswordStoreChange::REMOVE, login_credentials);
  reuse_detector.OnLoginsChanged(remove_changes);
  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(/*is_reuse_found=*/false, _, _, _, _));
  // The stored credentials were removed so no reuse should be found.
  reuse_detector.CheckReuse(u"123saved_password", "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, MatchMultiplePasswords) {
  // These all have different length passwods so we can check the
  // returned length.
  const std::vector<TestData> domain_passwords = {
      {"https://a.com, https://all.com", "aUsername", "34567890"},
      {"https://b.com, https://b2.com, https://all.com", "bUsername",
       "01234567890"},
      {"https://c.com, https://all.com", "cUsername", "1234567890"},
      {"https://d.com", "dUsername", "123456789"},
  };

  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(domain_passwords));

  MockPasswordReuseDetectorConsumer mockConsumer;

  std::vector<MatchingReusedCredential> credentials = {
      {"https://a.com", u"aUsername"},   {"https://all.com", u"aUsername"},
      {"https://all.com", u"bUsername"}, {"https://all.com", u"cUsername"},
      {"https://b.com", u"bUsername"},   {"https://b2.com", u"bUsername"},
      {"https://c.com", u"cUsername"}};
  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("01234567890"),
                               Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
                               UnorderedElementsAreArray(credentials), 8));
  reuse_detector.CheckReuse(u"abcd01234567890", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  credentials = {{"https://a.com", u"aUsername"},
                 {"https://all.com", u"aUsername"},
                 {"https://all.com", u"cUsername"},
                 {"https://c.com", u"cUsername"}};
  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("1234567890"),
                               Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
                               UnorderedElementsAreArray(credentials), 8));
  reuse_detector.CheckReuse(u"1234567890", "https://evil.com", &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _));
  reuse_detector.CheckReuse(u"4567890", "https://evil.com", &mockConsumer);
}

TEST(PasswordReuseDetectorTest, GaiaPasswordNoReuse) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  reuse_detector.UseGaiaPasswordHash(
      PrepareGaiaPasswordData({"gaia_pw1", "gaia_pw2"}));

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _));
  // Typing gaia password on https://accounts.google.com is OK.
  reuse_detector.CheckReuse(u"gaia_pw1", "https://accounts.google.com",
                            &mockConsumer);
  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _));
  reuse_detector.CheckReuse(u"gaia_pw2", "https://accounts.google.com",
                            &mockConsumer);
  // Only suffixes are verifed.
  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _));
  reuse_detector.CheckReuse(u"sync_password123", "https://evil.com",
                            &mockConsumer);
  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _));
  reuse_detector.CheckReuse(u"other_password", "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, GaiaPasswordReuseFound) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  std::vector<PasswordHashData> gaia_password_hashes =
      PrepareGaiaPasswordData({"gaia_pw1", "gaia_pw2"});
  absl::optional<PasswordHashData> expected_reused_password_hash(
      gaia_password_hashes[0]);
  reuse_detector.UseGaiaPasswordHash(gaia_password_hashes);

  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(true, strlen("gaia_pw1"),
                       Matches(expected_reused_password_hash), IsEmpty(), 5));

  reuse_detector.CheckReuse(u"gaia_pw1", "https://phishing.example.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, EnterprisePasswordNoReuse) {
  PasswordReuseDetector reuse_detector;
  ConfigureEnterprisePasswordProtection(&reuse_detector);
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  std::vector<PasswordHashData> enterprise_password_hashes =
      PrepareEnterprisePasswordData({"enterprise_pw1", "enterprise_pw2"});
  absl::optional<PasswordHashData> expected_reused_password_hash(
      enterprise_password_hashes[1]);
  reuse_detector.UseNonGaiaEnterprisePasswordHash(enterprise_password_hashes);

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _));
  // Typing enterprise password on change password page is OK.
  reuse_detector.CheckReuse(
      u"enterprise_pw1", "https://changepassword.example.com/", &mockConsumer);
  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _));
  reuse_detector.CheckReuse(
      u"enterprise_pw2", "https://changepassword.example.com/", &mockConsumer);

  // Suffix match is not reuse.
  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _));
  reuse_detector.CheckReuse(u"enterprise", "https://evil.com", &mockConsumer);
  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _));
  reuse_detector.CheckReuse(u"other_password", "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, EnterprisePasswordReuseFound) {
  PasswordReuseDetector reuse_detector;
  ConfigureEnterprisePasswordProtection(&reuse_detector);
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  std::vector<PasswordHashData> enterprise_password_hashes =
      PrepareEnterprisePasswordData({"enterprise_pw1", "enterprise_pw2"});
  absl::optional<PasswordHashData> expected_reused_password_hash(
      enterprise_password_hashes[1]);
  reuse_detector.UseNonGaiaEnterprisePasswordHash(enterprise_password_hashes);

  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(true, strlen("enterprise_pw2"),
                       Matches(expected_reused_password_hash), IsEmpty(), 5));
  reuse_detector.CheckReuse(u"enterprise_pw2", "https://phishing.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, MatchGaiaAndMultipleSavedPasswords) {
  const std::vector<TestData> domain_passwords = {
      {"https://a.com", "aUsername", "34567890"},
      {"https://b.com", "bUsername", "01234567890"},
  };
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(domain_passwords));

  std::string gaia_password = "1234567890";
  std::vector<PasswordHashData> gaia_password_hashes =
      PrepareGaiaPasswordData({gaia_password});
  ASSERT_EQ(1u, gaia_password_hashes.size());
  absl::optional<PasswordHashData> expected_reused_password_hash(
      gaia_password_hashes[0]);
  reuse_detector.UseGaiaPasswordHash(gaia_password_hashes);

  MockPasswordReuseDetectorConsumer mockConsumer;

  const std::vector<MatchingReusedCredential> credentials = {
      {"https://a.com", u"aUsername"}, {"https://b.com", u"bUsername"}};
  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("01234567890"),
                               Matches(expected_reused_password_hash),
                               UnorderedElementsAreArray(credentials), 2));
  reuse_detector.CheckReuse(u"abcd01234567890", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(true, strlen("1234567890"),
                       Matches(expected_reused_password_hash), IsEmpty(), 2));
  reuse_detector.CheckReuse(u"xyz1234567890", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _));
  reuse_detector.CheckReuse(u"4567890", "https://evil.com", &mockConsumer);
}

TEST(PasswordReuseDetectorTest, MatchSavedPasswordButNotGaiaPassword) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  std::string gaia_password = "gaia_password";
  reuse_detector.UseGaiaPasswordHash(PrepareGaiaPasswordData({gaia_password}));

  const std::vector<MatchingReusedCredential> credentials = {
      {"https://accounts.google.com", u"gUsername"}};
  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("saved_password"),
                               Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
                               UnorderedElementsAreArray(credentials), 5));
  reuse_detector.CheckReuse(u"saved_password", "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest,
     MatchSavedPasswordButNotGaiaPasswordInAccountStore) {
  PasswordReuseDetector reuse_detector;

  auto account_store_form = std::make_unique<PasswordForm>();
  account_store_form->signon_realm = "https://twitter.com";
  account_store_form->username_value = u"twitterUsername";
  account_store_form->password_value = u"saved_password";
  account_store_form->in_store = PasswordForm::Store::kAccountStore;
  std::vector<std::unique_ptr<PasswordForm>> account_store_forms;
  account_store_forms.push_back(std::move(account_store_form));
  reuse_detector.OnGetPasswordStoreResults(std::move(account_store_forms));

  std::string gaia_password = "gaia_password";
  reuse_detector.UseGaiaPasswordHash(PrepareGaiaPasswordData({gaia_password}));

  const std::vector<MatchingReusedCredential> credentials = {
      {"https://twitter.com", u"twitterUsername",
       PasswordForm::Store::kAccountStore}};

  MockPasswordReuseDetectorConsumer mockConsumer;
  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("saved_password"),
                               Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
                               UnorderedElementsAreArray(credentials),
                               /*saved_passwords=*/1));
  reuse_detector.CheckReuse(u"saved_password", "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, MatchEnterpriseAndMultipleSavedPasswords) {
  const std::vector<TestData> domain_passwords = {
      {"https://a.com", "aUsername", "34567890"},
      {"https://b.com", "bUsername", "01234567890"},
  };
  PasswordReuseDetector reuse_detector;
  ConfigureEnterprisePasswordProtection(&reuse_detector);
  reuse_detector.OnGetPasswordStoreResults(GetForms(domain_passwords));

  std::string enterprise_password = "1234567890";
  std::vector<PasswordHashData> enterprise_password_hashes =
      PrepareEnterprisePasswordData({enterprise_password});
  ASSERT_EQ(1u, enterprise_password_hashes.size());
  absl::optional<PasswordHashData> expected_reused_password_hash(
      enterprise_password_hashes[0]);
  reuse_detector.UseNonGaiaEnterprisePasswordHash(enterprise_password_hashes);

  MockPasswordReuseDetectorConsumer mockConsumer;

  const std::vector<MatchingReusedCredential> credentials = {
      {"https://a.com", u"aUsername"}, {"https://b.com", u"bUsername"}};
  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("01234567890"),
                               Matches(expected_reused_password_hash),
                               UnorderedElementsAreArray(credentials), 2));
  reuse_detector.CheckReuse(u"abcd01234567890", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(true, strlen("1234567890"),
                       Matches(expected_reused_password_hash), IsEmpty(), 2));
  reuse_detector.CheckReuse(u"xyz1234567890", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _));
  reuse_detector.CheckReuse(u"4567890", "https://evil.com", &mockConsumer);
}

TEST(PasswordReuseDetectorTest, MatchSavedPasswordButNotEnterprisePassword) {
  PasswordReuseDetector reuse_detector;
  ConfigureEnterprisePasswordProtection(&reuse_detector);
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  std::string enterprise_password = "enterprise_password";
  reuse_detector.UseNonGaiaEnterprisePasswordHash(
      PrepareEnterprisePasswordData({enterprise_password}));

  const std::vector<MatchingReusedCredential> credentials = {
      {"https://accounts.google.com", u"gUsername"}};
  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("saved_password"),
                               Matches(NO_GAIA_OR_ENTERPRISE_REUSE),
                               UnorderedElementsAreArray(credentials), 5));
  reuse_detector.CheckReuse(u"saved_password", "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, MatchGaiaEnterpriseAndSavedPassword) {
  const std::vector<TestData> domain_passwords = {
      {"https://a.com", "aUsername", "34567890"},
      {"https://b.com", "bUsername", "01234567890"},
  };
  PasswordReuseDetector reuse_detector;
  ConfigureEnterprisePasswordProtection(&reuse_detector);
  reuse_detector.OnGetPasswordStoreResults(GetForms(domain_passwords));

  std::string gaia_password = "123456789";
  reuse_detector.UseGaiaPasswordHash(PrepareGaiaPasswordData({gaia_password}));

  std::string enterprise_password = "1234567890";
  std::vector<PasswordHashData> enterprise_password_hashes =
      PrepareEnterprisePasswordData({enterprise_password});
  ASSERT_EQ(1u, enterprise_password_hashes.size());
  absl::optional<PasswordHashData> expected_reused_password_hash(
      enterprise_password_hashes[0]);
  reuse_detector.UseNonGaiaEnterprisePasswordHash(enterprise_password_hashes);

  MockPasswordReuseDetectorConsumer mockConsumer;

  const std::vector<MatchingReusedCredential> credentials = {
      {"https://a.com", u"aUsername"}, {"https://b.com", u"bUsername"}};
  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("01234567890"),
                               Matches(expected_reused_password_hash),
                               UnorderedElementsAreArray(credentials), 2));
  reuse_detector.CheckReuse(u"abcd01234567890", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(
      mockConsumer,
      OnReuseCheckDone(true, strlen("1234567890"),
                       Matches(expected_reused_password_hash), IsEmpty(), 2));
  reuse_detector.CheckReuse(u"xyz1234567890", "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _));
  reuse_detector.CheckReuse(u"4567890", "https://evil.com", &mockConsumer);
}

TEST(PasswordReuseDetectorTest, ClearGaiaPasswordHash) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  reuse_detector.UseGaiaPasswordHash(
      PrepareGaiaPasswordData({"gaia_pw1", "gaia_pw12"}));
  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("gaia_pw1"), _, _, _));
  reuse_detector.CheckReuse(u"gaia_pw1", "https://evil.com", &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer,
              OnReuseCheckDone(true, strlen("gaia_pw12"), _, _, _));
  reuse_detector.CheckReuse(u"gaia_pw12", "https://evil.com", &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  reuse_detector.ClearGaiaPasswordHash("username_gaia_pw1");
  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _));
  reuse_detector.CheckReuse(u"gaia_pw1", "https://evil.com", &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  reuse_detector.ClearAllGaiaPasswordHash();
  EXPECT_CALL(mockConsumer, OnReuseCheckDone(false, _, _, _, _));
  reuse_detector.CheckReuse(u"gaia_pw12", "https://evil.com", &mockConsumer);
}

}  // namespace

}  // namespace password_manager
