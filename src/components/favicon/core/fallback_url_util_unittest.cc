// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/fallback_url_util.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace favicon {
namespace {

TEST(FallbackURLUtilTest, GetFallbackIconText) {
  struct {
    const char* url_str;
    const char* expected;
  } test_cases[] = {
      // Test vacuous or invalid cases.
      {"", ""},
      {"http:///", ""},
      {"this is not an URL", ""},
      {"!@#$%^&*()", ""},
      // Test URLs with a domain in the registry.
      {"http://www.google.com/", "G"},
      {"ftp://GOogLE.com/", "G"},
      {"https://www.google.com:8080/path?query#ref", "G"},
      {"http://www.amazon.com", "A"},
      {"http://zmzaon.co.uk/", "Z"},
      {"http://w-3.137.org", "1"},
      // Test URLs with a domian not in the registry.
      {"http://localhost/", "L"},
      {"chrome-search://local-ntp/local-ntp.html", "L"},
      // Test IP URLs.
      {"http://192.168.0.1/", "IP"},
      {"http://[2001:4860:4860::8888]/", "IP"},
      // Miscellaneous edge cases.
      {"http://www..com/", "."},
      {"http://ip.ip/", "I"},
      // xn-- related cases: we're not supporint xn-- yet
      {"http://xn--oogle-60a/", "X"},
      {"http://xn-oogle-60a/", "X"},
  };
  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    base::string16 expected = base::ASCIIToUTF16(test_cases[i].expected);
    GURL url(test_cases[i].url_str);
    EXPECT_EQ(expected, GetFallbackIconText(url)) << " for test_cases[" << i
                                                  << "]";
  }
}

}  // namespace
}  // namespace favicon
