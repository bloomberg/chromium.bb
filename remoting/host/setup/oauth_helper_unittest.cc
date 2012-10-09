// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/oauth_helper.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::string Replace(const std::string& s, const std::string& old_substr,
                    const std::string& new_substr) {
  size_t pos = s.find(old_substr);
  if (pos == std::string::npos) {
    return s;
  }
  return s.substr(0, pos) + new_substr +
      s.substr(pos + old_substr.length(), std::string::npos);
}

}  // namespace

namespace remoting {

TEST(OauthHelperTest, TestNotCode) {
  ASSERT_EQ("", GetOauthCodeInUrl("notURL"));
}

TEST(OauthHelperTest, TestVeryShort) {
  ASSERT_EQ("", GetOauthCodeInUrl(GetOauthRedirectUrl()));
}

TEST(OauthHelperTest, TestEmptyQuery) {
  ASSERT_EQ("", GetOauthCodeInUrl(GetOauthRedirectUrl() + "?"));
}

TEST(OauthHelperTest, TestNoQueryValue) {
  ASSERT_EQ("", GetOauthCodeInUrl(GetOauthRedirectUrl() + "?code"));
}

TEST(OauthHelperTest, TestEmptyCode) {
  ASSERT_EQ("", GetOauthCodeInUrl(GetOauthRedirectUrl() + "?code="));
}

TEST(OauthHelperTest, TestCode) {
  ASSERT_EQ("Dummy", GetOauthCodeInUrl(GetOauthRedirectUrl() + "?code=Dummy"));
}

TEST(OauthHelperTest, TestCodeInLongQuery) {
  ASSERT_EQ("Dummy", GetOauthCodeInUrl(GetOauthRedirectUrl() +
                                       "?x=1&code=Dummy&y=2"));
}

TEST(OauthHelperTest, TestBadScheme) {
  std::string url = GetOauthRedirectUrl() + "?code=Dummy";
  url = Replace(url, "https:", "http");
  ASSERT_EQ("", GetOauthCodeInUrl(url));
}

TEST(OauthHelperTest, TestBadHost) {
  std::string url = GetOauthRedirectUrl() + "?code=Dummy";
  url = Replace(url, "google", "goggle");
  ASSERT_EQ("", GetOauthCodeInUrl(url));
}

}  // namespace remoting
