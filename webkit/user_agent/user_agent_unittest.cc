// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/user_agent/user_agent.h"

#include <string>

#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/tools/test_shell/test_shell_test.h"

namespace {

class WebkitGlueUserAgentTest : public TestShellTest {
};

bool IsSpoofedUserAgent(const std::string& user_agent) {
  return user_agent.find("TestShell") == std::string::npos;
}

TEST_F(WebkitGlueUserAgentTest, UserAgentSpoofingHack) {
  enum Platform {
    NONE = 0,
    MACOSX = 1,
    WIN = 2,
    OTHER = 4,
  };

  struct Expected {
    const char* url;
    int os_mask;
  };

  Expected expected[] = {
      { "http://wwww.google.com", NONE },
      { "http://www.microsoft.com/getsilverlight", MACOSX },
      { "http://headlines.yahoo.co.jp/videonews/", MACOSX | WIN },
      { "http://downloads.yahoo.co.jp/docs/silverlight/", MACOSX },
      { "http://gyao.yahoo.co.jp/", MACOSX },
      { "http://weather.yahoo.co.jp/weather/zoomradar/", WIN },
      { "http://promotion.shopping.yahoo.co.jp/", WIN },
      { "http://pokemon.kids.yahoo.co.jp", WIN },
  };
#if defined(OS_MACOSX)
  int os_bit = MACOSX;
#elif defined(OS_WIN)
  int os_bit = WIN;
#else
  int os_bit = OTHER;
#endif

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(expected); ++i) {
    EXPECT_EQ((expected[i].os_mask & os_bit) != 0,
              IsSpoofedUserAgent(
                  webkit_glue::GetUserAgent(GURL(expected[i].url))));
  }
}

}  // namespace
