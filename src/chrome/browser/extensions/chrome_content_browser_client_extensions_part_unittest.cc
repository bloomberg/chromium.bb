// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"

#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/common/chrome_content_client.h"
#include "extensions/common/url_pattern_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

typedef testing::Test ChromeContentBrowserClientExtensionsPartTest;

// Check that empty site URLs get recorded properly in ShouldAllowOpenURL
// failures.
TEST_F(ChromeContentBrowserClientExtensionsPartTest,
       ShouldAllowOpenURLMetricsForEmptySiteURL) {
  base::HistogramTester uma;

  auto failure_reason = ChromeContentBrowserClientExtensionsPart::
      FAILURE_SCHEME_NOT_HTTP_OR_HTTPS_OR_EXTENSION;
  ChromeContentBrowserClientExtensionsPart::RecordShouldAllowOpenURLFailure(
      failure_reason, GURL());
  uma.ExpectUniqueSample("Extensions.ShouldAllowOpenURL.Failure",
                         failure_reason, 1);
  uma.ExpectUniqueSample("Extensions.ShouldAllowOpenURL.Failure.Scheme",
                         1 /* SCHEME_EMPTY */, 1);
}

// Check that a non-exhaustive list of some known schemes get recorded properly
// in ShouldAllowOpenURL failures.
TEST_F(ChromeContentBrowserClientExtensionsPartTest,
       ShouldAllowOpenURLMetricsForKnownSchemes) {
  base::HistogramTester uma;

  ChromeContentClient content_client;
  content::ContentClient::Schemes schemes;
  content_client.AddAdditionalSchemes(&schemes);

  std::vector<std::string> test_schemes(schemes.savable_schemes);
  test_schemes.insert(test_schemes.end(), schemes.secure_schemes.begin(),
                      schemes.secure_schemes.end());
  test_schemes.insert(test_schemes.end(),
                      schemes.empty_document_schemes.begin(),
                      schemes.empty_document_schemes.end());
  test_schemes.push_back(url::kHttpScheme);
  test_schemes.push_back(url::kHttpsScheme);
  test_schemes.push_back(url::kFileScheme);

  auto failure_reason = ChromeContentBrowserClientExtensionsPart::
      FAILURE_RESOURCE_NOT_WEB_ACCESSIBLE;
  for (auto scheme : test_schemes) {
    ChromeContentBrowserClientExtensionsPart::RecordShouldAllowOpenURLFailure(
        failure_reason, GURL(scheme + "://foo.com/"));
  }

  // There should be no unknown schemes recorded.
  uma.ExpectUniqueSample("Extensions.ShouldAllowOpenURL.Failure",
                         failure_reason, test_schemes.size());
  uma.ExpectTotalCount("Extensions.ShouldAllowOpenURL.Failure.Scheme",
                       test_schemes.size());
  uma.ExpectBucketCount("Extensions.ShouldAllowOpenURL.Failure.Scheme",
                        0 /* SCHEME_UNKNOWN */, 0);
}

}  // namespace extensions
