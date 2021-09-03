// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/weak_check_utility.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

constexpr char16_t kWeakShortPassword[] = u"123456";
constexpr char16_t kWeakLongPassword[] =
    u"abcdabcdabcdabcdabcdabcdabcdabcdabcdabcda";
constexpr char16_t kStrongShortPassword[] = u"fnlsr4@cm^mdls@fkspnsg3d";
constexpr char16_t kStrongLongPassword[] =
    u"pmsFlsnoab4nsl#losb@skpfnsbkjb^klsnbs!cns";

using ::testing::ElementsAre;

}  // namespace

TEST(WeakCheckUtilityTest, IsWeak) {
  EXPECT_TRUE(IsWeak(kWeakShortPassword));
  EXPECT_TRUE(IsWeak(kWeakLongPassword));
  EXPECT_FALSE(IsWeak(kStrongShortPassword));
  EXPECT_FALSE(IsWeak(kStrongLongPassword));
}

TEST(WeakCheckUtilityTest, WeakPasswordsNotFound) {
  base::flat_set<std::u16string> passwords = {kStrongShortPassword,
                                              kStrongLongPassword};

  EXPECT_THAT(BulkWeakCheck(passwords), testing::IsEmpty());
}

TEST(WeakCheckUtilityTest, DetectedShortAndLongWeakPasswords) {
  base::flat_set<std::u16string> passwords = {
      kStrongLongPassword, kWeakShortPassword, kStrongShortPassword,
      kWeakLongPassword};

  base::flat_set<std::u16string> weak_passwords = BulkWeakCheck(passwords);

  EXPECT_THAT(weak_passwords,
              ElementsAre(kWeakShortPassword, kWeakLongPassword));
}

}  // namespace password_manager
