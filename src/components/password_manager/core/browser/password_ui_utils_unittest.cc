// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_ui_utils.h"

#include <tuple>

#include "components/autofill/core/common/password_form.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {

TEST(GetShownOriginTest, RemovePrefixes) {
  const struct {
    const std::string input;
    const std::string output;
  } kTestCases[] = {
      {"http://subdomain.example.com:80/login/index.html?h=90&ind=hello#first",
       "subdomain.example.com"},
      {"https://www.example.com", "example.com"},
      {"https://mobile.example.com", "example.com"},
      {"https://m.example.com", "example.com"},
      {"https://m.subdomain.example.net", "subdomain.example.net"},
      {"https://mobile.de", "mobile.de"},
      {"https://www.de", "www.de"},
      {"https://m.de", "m.de"},
      {"https://www.mobile.de", "mobile.de"},
      {"https://m.mobile.de", "mobile.de"},
      {"https://m.www.de", "www.de"},
      {"https://Mobile.example.de", "example.de"},
      {"https://WWW.Example.DE", "example.de"}};

  for (const auto& test_case : kTestCases) {
    autofill::PasswordForm password_form;
    password_form.origin = GURL(test_case.input);
    EXPECT_EQ(test_case.output, GetShownOrigin(password_form.origin))
        << "for input " << test_case.input;
  }
}

TEST(GetShownOriginAndLinkUrlTest, OriginFromAndroidForm_NoAppDisplayName) {
  autofill::PasswordForm android_form;
  android_form.signon_realm = "android://hash@com.example.android";
  android_form.app_display_name.clear();

  std::string shown_origin;
  GURL link_url;
  std::tie(shown_origin, link_url) = GetShownOriginAndLinkUrl(android_form);

  EXPECT_EQ("android.example.com", shown_origin);
  EXPECT_EQ("https://play.google.com/store/apps/details?id=com.example.android",
            link_url.spec());
}

TEST(GetShownOriginAndLinkUrlTest, OriginFromAndroidForm_WithAppDisplayName) {
  autofill::PasswordForm android_form;
  android_form.signon_realm = "android://hash@com.example.android";
  android_form.app_display_name = "Example Android App";

  std::string shown_origin;
  GURL link_url;
  std::tie(shown_origin, link_url) = GetShownOriginAndLinkUrl(android_form);

  EXPECT_EQ("Example Android App", shown_origin);
  EXPECT_EQ("https://play.google.com/store/apps/details?id=com.example.android",
            link_url.spec());
}

TEST(GetShownOriginAndLinkUrlTest, OriginFromNonAndroidForm) {
  autofill::PasswordForm form;
  form.signon_realm = "https://example.com/";
  form.origin = GURL("https://example.com/login?ref=1");

  std::string shown_origin;
  GURL link_url;
  std::tie(shown_origin, link_url) = GetShownOriginAndLinkUrl(form);

  EXPECT_EQ("example.com", shown_origin);
  EXPECT_EQ(GURL("https://example.com/login?ref=1"), link_url);
}

TEST(SplitByDotAndReverseTest, ReversedHostname) {
  const struct {
    const char* input;
    const char* output;
  } kTestCases[] = {{"com.example.android", "android.example.com"},
                    {"com.example.mobile", "mobile.example.com"},
                    {"net.example.subdomain", "subdomain.example.net"}};
  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.output, SplitByDotAndReverse(test_case.input));
  }
}

}  // namespace password_manager
