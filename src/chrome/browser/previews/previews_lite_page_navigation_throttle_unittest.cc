// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_navigation_throttle.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/previews/previews_lite_page_decider.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/previews/core/previews_features.h"
#include "content/public/browser/navigation_handle.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(PreviewsLitePageNavigationThrottleTest, TestGetPreviewsURL) {
  struct TestCase {
    std::string previews_host;
    std::string original_url;
    std::string previews_url;
  };
  const TestCase kTestCases[]{
      // Use https://play.golang.org/p/HUM2HxmUTOW to compute |previews_url|.
      {
          "https://previews.host.com",
          "https://original.host.com/path/path/path?query=yes",
          "https://shta44dh4bi7rc6fnpjnkrtytwlabygjhk53v2trlot2wddylwua."
          "previews.host.com/p?u="
          "https%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
      },
      {
          "https://previews.host.com",
          "http://original.host.com/path/path/path?query=yes",
          "https://6p7dar4ju6r4ynz7x3pucmlcltuqsf7z5auhvckzln7voglkt56q."
          "previews.host.com/p?u="
          "http%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
      },
      {
          "https://previews.host.com",
          "https://original.host.com:1443/path/path/path?query=yes",
          "https://mil6oxtqb4zpsbmutm4d7wrx5nlr6tzlxjp7y44u55zqhzsdzjpq."
          "previews.host.com/p?u=https%3A%2F%2Foriginal.host.com%3A1443"
          "%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
      },
      {
          "https://previews.host.com:1443",
          "http://original.host.com/path/path/path?query=yes",
          "https://6p7dar4ju6r4ynz7x3pucmlcltuqsf7z5auhvckzln7voglkt56q."
          "previews.host.com:1443/p?u="
          "http%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
      },
      {
          "https://previews.host.com:1443",
          "https://original.host.com:1443/path/path/path?query=yes",
          "https://mil6oxtqb4zpsbmutm4d7wrx5nlr6tzlxjp7y44u55zqhzsdzjpq."
          "previews.host.com:1443/p?u=https%3A%2F%2Foriginal.host.com%3A1443"
          "%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
      },
      {
          "https://previews.host.com",
          "https://original.host.com/path/path/path?query=yes#fragment",
          "https://shta44dh4bi7rc6fnpjnkrtytwlabygjhk53v2trlot2wddylwua."
          "previews.host.com/p?u="
          "https%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes"
          "%23fragment",
      },
  };

  for (const TestCase& test_case : kTestCases) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        previews::features::kLitePageServerPreviews,
        {{"previews_host", test_case.previews_host}});

    EXPECT_EQ(PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(
                  GURL(test_case.original_url)),
              GURL(test_case.previews_url));
  }
}

TEST(PreviewsLitePageNavigationThrottleTest, TestGetOriginalURL) {
  struct TestCase {
    std::string previews_host;
    std::string original_url;
    std::string previews_url;
    bool want_ok;
  };
  const TestCase kTestCases[]{
      // Use https://play.golang.org/p/HUM2HxmUTOW to compute |previews_url|.
      {
          "https://previews.host.com",
          "https://original.host.com/path/path/path?query=yes",
          "https://shta44dh4bi7rc6fnpjnkrtytwlabygjhk53v2trlot2wddylwua."
          "previews.host.com/p?u="
          "https%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
          true,
      },
      {
          "https://previews.host.com",
          "http://original.host.com/path/path/path?query=yes",
          "https://6p7dar4ju6r4ynz7x3pucmlcltuqsf7z5auhvckzln7voglkt56q."
          "previews.host.com/p?u="
          "http%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
          true,
      },
  };

  for (const TestCase& test_case : kTestCases) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        previews::features::kLitePageServerPreviews,
        {{"previews_host", test_case.previews_host}});

    std::string original_url;
    bool got_ok = PreviewsLitePageNavigationThrottle::GetOriginalURL(
        GURL(test_case.previews_url), &original_url);
    EXPECT_EQ(got_ok, test_case.want_ok);
    EXPECT_EQ(original_url, test_case.original_url);
  }
}
