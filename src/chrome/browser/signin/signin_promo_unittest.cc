// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace signin {

#if !defined(OS_CHROMEOS)
TEST(SigninPromoTest, TestPromoURL) {
  GURL expected_url_1(
      "chrome://chrome-signin/?access_point=0&reason=0&auto_close=1");
  EXPECT_EQ(expected_url_1,
            GetEmbeddedPromoURL(
                signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE,
                signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT, true));
  GURL expected_url_2("chrome://chrome-signin/?access_point=15&reason=3");
  EXPECT_EQ(expected_url_2,
            GetEmbeddedPromoURL(
                signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO,
                signin_metrics::Reason::REASON_UNLOCK, false));
}

TEST(SigninPromoTest, TestReauthURL) {
  GURL expected_url_1(
      "chrome://chrome-signin/"
      "?access_point=0&reason=3&auto_close=1&email=example%40domain.com"
      "&validateEmail=1&readOnlyEmail=1");
  EXPECT_EQ(expected_url_1,
            GetEmbeddedReauthURLWithEmail(
                signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE,
                signin_metrics::Reason::REASON_UNLOCK, "example@domain.com"));
}
#endif  // !defined(OS_CHROMEOS)

TEST(SigninPromoTest, TestLandingURL) {
  GURL expected_url_1(
      "chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik/"
      "success.html?access_point=1&source=13");
  EXPECT_EQ(expected_url_1,
            GetLandingURL(signin_metrics::AccessPoint::ACCESS_POINT_NTP_LINK));
  GURL expected_url_2(
      "chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik/"
      "success.html?access_point=0&source=0");
  EXPECT_EQ(
      expected_url_2,
      GetLandingURL(signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE));
  GURL expected_url_3(
      "chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik/"
      "success.html?access_point=3&source=3");
  EXPECT_EQ(expected_url_3,
            GetLandingURL(signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS));
}

TEST(SigninPromoTest, SigninURLForDice) {
  EXPECT_EQ(
      "https://accounts.google.com/signin/chrome/sync?ssp=1&"
      "email_hint=email%40gmail.com&continue=https%3A%2F%2Fcontinue_url%2F",
      GetChromeSyncURLForDice("email@gmail.com", "https://continue_url/"));
  EXPECT_EQ(
      "https://accounts.google.com/AddSession?"
      "Email=email%40gmail.com&continue=https%3A%2F%2Fcontinue_url%2F",
      GetAddAccountURLForDice("email@gmail.com", "https://continue_url/"));
}

}  // namespace signin
