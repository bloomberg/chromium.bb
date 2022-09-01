// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/favicon/favicon_util.h"

#include "components/favicon_base/favicon_url_parser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

TEST(FaviconUtilUnittest, Parse) {
  struct {
    bool parse_should_succeed;
    const std::string& url;
  } test_cases[] = {
      {false, "chrome-extension://id"},
      {false, "chrome-extension://id/"},
      {false, "chrome-extension://id?"},
      {false, "chrome-extension://id/?"},
      {false, "chrome-extension://id/_favicon"},
      {false, "chrome-extension://id/_favicon/"},
      {false, "chrome-extension://id/_favicon/?"},
      {true, "chrome-extension://id/_favicon?page_url=https://ok.com"},
      {true, "chrome-extension://id/_favicon/?page_url=https://ok.com"},
      {true, "chrome-extension://id/_favicon/?page_url=https://ok.com&size=16"},
      {true,
       "chrome-extension://id/_favicon/?page_url=https://"
       "ok.com&size=16&scale_factor=1.0x&server_fallback=1"}};
  for (const auto& test_case : test_cases) {
    GURL url(test_case.url);
    chrome::ParsedFaviconPath parsed;
    EXPECT_EQ(test_case.parse_should_succeed,
              favicon_util::ParseFaviconPath(url, &parsed));
  }
}

}  // namespace extensions
