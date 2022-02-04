// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_util.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/borealis/borealis_window_manager_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace borealis {
namespace {

class BorealisUtilTest : public testing::Test {
 protected:
  GURL GetFeedbackFormUrl(TestingProfile* profile,
                          const std::string& app_id,
                          const std::string& window_title) {
    base::RunLoop run_loop;
    GURL returned_url;
    FeedbackFormUrl(profile, app_id, window_title,
                    base::BindLambdaForTesting([&](GURL url) {
                      returned_url = url;
                      run_loop.Quit();
                    }));
    run_loop.Run();
    return returned_url;
  }

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BorealisUtilTest, GetBorealisAppIdReturnsEmptyOnFailure) {
  EXPECT_EQ(GetBorealisAppId("foo"), absl::nullopt);
}

TEST_F(BorealisUtilTest, GetBorealisAppIdReturnsId) {
  EXPECT_EQ(GetBorealisAppId("borealis/123").value(), 123);
}

TEST_F(BorealisUtilTest, GetBorealisAppIdFromWindowReturnsEmptyOnFailure) {
  std::unique_ptr<aura::Window> window =
      MakeWindow("org.chromium.borealis.wmclass.foo");
  EXPECT_EQ(GetBorealisAppId(window.get()), absl::nullopt);
}

TEST_F(BorealisUtilTest, GetBorealisAppIdFromWindowReturnsId) {
  std::unique_ptr<aura::Window> window =
      MakeWindow("org.chromium.borealis.xprop.123");
  EXPECT_EQ(GetBorealisAppId(window.get()).value(), 123);
}

TEST_F(BorealisUtilTest, FeedbackFormUrlExcludesNonGames) {
  TestingProfile profile;

  EXPECT_FALSE(GetFeedbackFormUrl(&profile,
                                  "borealisanon:org.chromium.borealis.xid.100",
                                  "CoolApp")
                   .is_valid());
}

TEST_F(BorealisUtilTest, FeedbackFormUrlPrefillsWindowTitle) {
  TestingProfile profile;

  EXPECT_THAT(GetFeedbackFormUrl(
                  &profile, "borealisanon:org.chromium.borealis.app", "CoolApp")
                  .spec(),
              testing::HasSubstr("=CoolApp"));
}

TEST_F(BorealisUtilTest, FeedbackFormUrlIsPrefilled) {
  TestingProfile profile;

  GURL url = GetFeedbackFormUrl(
      &profile, "borealisanon:org.chromium.borealis.app", "CoolApp");

  // Count the number of query parameters beginning with "entry"; these are
  // form fields that we're prefilling.
  int entries = 0;
  net::QueryIterator it(url);
  while (!it.IsAtEnd()) {
    if (base::StartsWith(it.GetKey(), "entry")) {
      ++entries;

      // All prefilled entries should have a value.
      EXPECT_THAT(it.GetValue(), testing::Not(testing::IsEmpty()));
    }
    it.Advance();
  }

  EXPECT_EQ(entries, 4);  // we currently prefill this many form fields
}

TEST_F(BorealisUtilTest, ProtonVersionProtonTitle) {
  absl::optional<int> game_id;
  std::string output =
      "GameID: 123, Proton: Proton 1.2-3, SLR: SLR - Name, "
      "Timestamp: 2021-01-01 00:00:00";
  borealis::ProtonVersionInfo info =
      borealis::ParseProtonVersionInfo(game_id, output);
  EXPECT_EQ(info.proton, "Proton 1.2-3");
  EXPECT_EQ(info.slr, "SLR - Name");
}

TEST_F(BorealisUtilTest, ProtonVersionProtonTitleMultiLine) {
  absl::optional<int> game_id;
  std::string output =
      "GameID: 123, Proton: Proton 1.2-3, SLR: SLR - Name, "
      "Timestamp: 2021-01-01 00:00:00\n"
      "GameID: 456, Proton: Proton 4.5-6, SLR: SLR - Name2, "
      "Timestamp: 2021-01-01 00:00:00";
  borealis::ProtonVersionInfo info =
      borealis::ParseProtonVersionInfo(game_id, output);
  EXPECT_EQ(info.proton, "Proton 1.2-3");
  EXPECT_EQ(info.slr, "SLR - Name");
}

TEST_F(BorealisUtilTest, ProtonVersionProtonTitleWithGameId) {
  absl::optional<int> game_id = 123;
  std::string output =
      "GameID: 123, Proton: Proton 1.2-3, SLR: SLR - Name, "
      "Timestamp: 2021-01-01 00:00:00";
  borealis::ProtonVersionInfo info =
      borealis::ParseProtonVersionInfo(game_id, output);
  EXPECT_EQ(info.proton, "Proton 1.2-3");
  EXPECT_EQ(info.slr, "SLR - Name");
}

TEST_F(BorealisUtilTest, ProtonVersionProtonTitleWithGameIdMismatch) {
  absl::optional<int> game_id = 123;
  std::string output =
      "GameID: 456, Proton: Proton 1.2-3, SLR: SLR - Name, "
      "Timestamp: 2021-01-01 00:00:00";
  borealis::ProtonVersionInfo info =
      borealis::ParseProtonVersionInfo(game_id, output);
  EXPECT_EQ(info.proton, borealis::kProtonVersionGameMismatch);
  EXPECT_EQ(info.slr, borealis::kProtonVersionGameMismatch);
}

TEST_F(BorealisUtilTest, ProtonVersionLinuxTitle) {
  absl::optional<int> game_id;
  std::string output =
      "INFO: Found a session for a Linux game at timestamp 2021-01-01 00:00:00";
  borealis::ProtonVersionInfo info =
      borealis::ParseProtonVersionInfo(game_id, output);
  EXPECT_THAT(info.proton, testing::IsEmpty());
  EXPECT_THAT(info.slr, testing::IsEmpty());
}

TEST_F(BorealisUtilTest, ProtonVersionLinuxTitleWithGameId) {
  absl::optional<int> game_id = 123;
  std::string output =
      "INFO: Found a session for a Linux game at timestamp 2021-01-01 00:00:00";
  borealis::ProtonVersionInfo info =
      borealis::ParseProtonVersionInfo(game_id, output);
  EXPECT_THAT(info.proton, testing::IsEmpty());
  EXPECT_THAT(info.slr, testing::IsEmpty());
}

}  // namespace
}  // namespace borealis
