// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_cookie_access_policy.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace android_webview {

namespace {
const GURL kUrlFirstParty("http://first.example");
const GURL kUrlThirdParty("http://third.example");
const GURL kFileUrl1("file:///path/to/any_file.html");
const GURL kFileUrl2("file:///path/to/some/other_file.html");
}  // namespace

class AwCookieAccessPolicyTest : public testing::Test {
 public:
  AwCookieAccessPolicyTest() {}

  bool CanAccessCookies(const GURL& url,
                        const GURL& site_for_cookies,
                        bool accept_cookies,
                        bool accept_third_party_cookies) {
    AwCookieAccessPolicy policy;
    policy.SetShouldAcceptCookies(accept_cookies);
    return policy.CanAccessCookies(url, site_for_cookies,
                                   accept_third_party_cookies);
  }
};

TEST_F(AwCookieAccessPolicyTest, BlockAllCookies) {
  EXPECT_FALSE(CanAccessCookies(kUrlFirstParty, kUrlFirstParty,
                                false /* allow_cookies */,
                                false /* allow_third_party_cookies */));
  EXPECT_FALSE(CanAccessCookies(kUrlFirstParty, kUrlThirdParty,
                                false /* allow_cookies */,
                                false /* allow_third_party_cookies */));
  EXPECT_FALSE(CanAccessCookies(kFileUrl1, kFileUrl1, false /* allow_cookies */,
                                false /* allow_third_party_cookies */));
  EXPECT_FALSE(CanAccessCookies(kFileUrl1, kFileUrl2, false /* allow_cookies */,
                                false /* allow_third_party_cookies */));
}

TEST_F(AwCookieAccessPolicyTest, BlockAllCookiesWithThirdPartySet) {
  EXPECT_FALSE(CanAccessCookies(kUrlFirstParty, kUrlFirstParty,
                                false /* allow_cookies */,
                                true /* allow_third_party_cookies */));
  EXPECT_FALSE(CanAccessCookies(kUrlFirstParty, kUrlThirdParty,
                                false /* allow_cookies */,
                                true /* allow_third_party_cookies */));
  EXPECT_FALSE(CanAccessCookies(kFileUrl1, kFileUrl1, false /* allow_cookies */,
                                true /* allow_third_party_cookies */));
  EXPECT_FALSE(CanAccessCookies(kFileUrl1, kFileUrl2, false /* allow_cookies */,
                                true /* allow_third_party_cookies */));
}

TEST_F(AwCookieAccessPolicyTest, FirstPartyCookiesOnly) {
  EXPECT_TRUE(CanAccessCookies(kUrlFirstParty, kUrlFirstParty,
                               true /* allow_cookies */,
                               false /* allow_third_party_cookies */));
  EXPECT_FALSE(CanAccessCookies(kUrlFirstParty, kUrlThirdParty,
                                true /* allow_cookies */,
                                false /* allow_third_party_cookies */));
  EXPECT_TRUE(CanAccessCookies(kFileUrl1, kFileUrl1, true /* allow_cookies */,
                               false /* allow_third_party_cookies */));
  EXPECT_TRUE(CanAccessCookies(kFileUrl1, kFileUrl2, true /* allow_cookies */,
                               false /* allow_third_party_cookies */));
}

TEST_F(AwCookieAccessPolicyTest, AllowAllCookies) {
  EXPECT_TRUE(CanAccessCookies(kUrlFirstParty, kUrlFirstParty,
                               true /* allow_cookies */,
                               true /* allow_third_party_cookies */));
  EXPECT_TRUE(CanAccessCookies(kUrlFirstParty, kUrlThirdParty,
                               true /* allow_cookies */,
                               true /* allow_third_party_cookies */));
  EXPECT_TRUE(CanAccessCookies(kFileUrl1, kFileUrl1, true /* allow_cookies */,
                               true /* allow_third_party_cookies */));
  EXPECT_TRUE(CanAccessCookies(kFileUrl1, kFileUrl2, true /* allow_cookies */,
                               true /* allow_third_party_cookies */));
}

}  // namespace android_webview
