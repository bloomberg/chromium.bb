// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::ErrorUtils;
using extensions::Extension;

namespace errors = extensions::manifest_errors;

TEST_F(ChromeManifestTest, AppWebUrls) {
  Testcase testcases[] = {
    Testcase("web_urls_wrong_type.json", errors::kInvalidWebURLs),
    Testcase("web_urls_invalid_1.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidWebURL,
                 base::IntToString(0),
                 errors::kExpectString)),
    Testcase("web_urls_invalid_2.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidWebURL,
                 base::IntToString(0),
                 URLPattern::GetParseResultString(
                 URLPattern::PARSE_ERROR_MISSING_SCHEME_SEPARATOR))),
    Testcase("web_urls_invalid_3.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidWebURL,
                 base::IntToString(0),
                 errors::kNoWildCardsInPaths)),
    Testcase("web_urls_invalid_4.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidWebURL,
                 base::IntToString(0),
                 errors::kCannotClaimAllURLsInExtent)),
    Testcase("web_urls_invalid_5.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidWebURL,
                 base::IntToString(1),
                 errors::kCannotClaimAllHostsInExtent))
  };
  RunTestcases(testcases, arraysize(testcases),
               EXPECT_TYPE_ERROR);

  LoadAndExpectSuccess("web_urls_has_port.json");

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("web_urls_default.json"));
  ASSERT_EQ(1u, extension->web_extent().patterns().size());
  EXPECT_EQ("*://www.google.com/*",
            extension->web_extent().patterns().begin()->GetAsString());
}
