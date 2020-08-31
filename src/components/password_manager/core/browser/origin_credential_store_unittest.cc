// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/origin_credential_store.h"

#include <memory>
#include <string>

#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {
namespace {

using base::ASCIIToUTF16;
using testing::ElementsAre;

using BlacklistedStatus = OriginCredentialStore::BlacklistedStatus;
using IsPublicSuffixMatch = UiCredential::IsPublicSuffixMatch;
using IsAffiliationBasedMatch = UiCredential::IsAffiliationBasedMatch;

constexpr char kExampleSite[] = "https://example.com/";

UiCredential MakeUiCredential(
    base::StringPiece username,
    base::StringPiece password,
    base::StringPiece origin = kExampleSite,
    IsPublicSuffixMatch is_public_suffix_match = IsPublicSuffixMatch(false),
    IsAffiliationBasedMatch is_affiliation_based_match =
        IsAffiliationBasedMatch(false)) {
  return UiCredential(base::UTF8ToUTF16(username), base::UTF8ToUTF16(password),
                      url::Origin::Create(GURL(origin)), is_public_suffix_match,
                      is_affiliation_based_match);
}

}  // namespace

class OriginCredentialStoreTest : public testing::Test {
 public:
  OriginCredentialStore* store() { return &store_; }

 private:
  OriginCredentialStore store_{url::Origin::Create(GURL(kExampleSite))};
};

TEST_F(OriginCredentialStoreTest, StoresCredentials) {
  store()->SaveCredentials(
      {MakeUiCredential("Berta", "30948"), MakeUiCredential("Adam", "Pas83B"),
       MakeUiCredential("Dora", "PakudC"), MakeUiCredential("Carl", "P1238C")});

  EXPECT_THAT(store()->GetCredentials(),
              ElementsAre(MakeUiCredential("Berta", "30948"),
                          MakeUiCredential("Adam", "Pas83B"),
                          MakeUiCredential("Dora", "PakudC"),
                          MakeUiCredential("Carl", "P1238C")));
}

TEST_F(OriginCredentialStoreTest, StoresOnlyNormalizedOrigins) {
  store()->SaveCredentials(
      {MakeUiCredential("Berta", "30948", kExampleSite),
       MakeUiCredential("Adam", "Pas83B", std::string(kExampleSite) + "path"),
       MakeUiCredential("Dora", "PakudC", kExampleSite,
                        IsPublicSuffixMatch(false),
                        IsAffiliationBasedMatch(true))});

  EXPECT_THAT(store()->GetCredentials(),
              ElementsAre(

                  // The URL that equals an origin stays untouched.
                  MakeUiCredential("Berta", "30948", kExampleSite),

                  // The longer URL is reduced to an origin.
                  MakeUiCredential("Adam", "Pas83B", kExampleSite),

                  // The android credential stays untouched.
                  MakeUiCredential("Dora", "PakudC", kExampleSite,
                                   IsPublicSuffixMatch(false),
                                   IsAffiliationBasedMatch(true))));
}

TEST_F(OriginCredentialStoreTest, ReplacesCredentials) {
  store()->SaveCredentials({MakeUiCredential("Ben", "S3cur3")});
  ASSERT_THAT(store()->GetCredentials(),
              ElementsAre(MakeUiCredential("Ben", "S3cur3")));

  store()->SaveCredentials({MakeUiCredential(std::string(), "M1@u")});
  EXPECT_THAT(store()->GetCredentials(),
              ElementsAre(MakeUiCredential(std::string(), "M1@u")));
}

TEST_F(OriginCredentialStoreTest, ClearsCredentials) {
  store()->SaveCredentials({MakeUiCredential("Ben", "S3cur3")});
  ASSERT_THAT(store()->GetCredentials(),
              ElementsAre(MakeUiCredential("Ben", "S3cur3")));

  store()->ClearCredentials();
  EXPECT_EQ(store()->GetCredentials().size(), 0u);
}

TEST_F(OriginCredentialStoreTest, SetBlacklistedAfterNeverBlacklisted) {
  store()->SetBlacklistedStatus(true);
  EXPECT_EQ(BlacklistedStatus::kIsBlacklisted, store()->GetBlacklistedStatus());
}

TEST_F(OriginCredentialStoreTest, CorrectlyUpdatesBlacklistedStatus) {
  store()->SetBlacklistedStatus(true);
  ASSERT_EQ(BlacklistedStatus::kIsBlacklisted, store()->GetBlacklistedStatus());

  store()->SetBlacklistedStatus(false);
  EXPECT_EQ(BlacklistedStatus::kWasBlacklisted,
            store()->GetBlacklistedStatus());

  store()->SetBlacklistedStatus(true);
  EXPECT_EQ(BlacklistedStatus::kIsBlacklisted, store()->GetBlacklistedStatus());
}

TEST_F(OriginCredentialStoreTest, WasBlacklistedStaysTheSame) {
  store()->SetBlacklistedStatus(true);
  ASSERT_EQ(BlacklistedStatus::kIsBlacklisted, store()->GetBlacklistedStatus());

  store()->SetBlacklistedStatus(false);
  ASSERT_EQ(BlacklistedStatus::kWasBlacklisted,
            store()->GetBlacklistedStatus());

  // If unblacklisting is communicated twice in a row, the status shouldn't
  // change.
  store()->SetBlacklistedStatus(false);
  EXPECT_EQ(BlacklistedStatus::kWasBlacklisted,
            store()->GetBlacklistedStatus());
}

TEST_F(OriginCredentialStoreTest, NeverBlacklistedStaysTheSame) {
  ASSERT_EQ(BlacklistedStatus::kNeverBlacklisted,
            store()->GetBlacklistedStatus());

  store()->SetBlacklistedStatus(false);
  EXPECT_EQ(BlacklistedStatus::kNeverBlacklisted,
            store()->GetBlacklistedStatus());
}

}  // namespace password_manager
