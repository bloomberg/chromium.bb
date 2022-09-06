// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/canonical_url_retriever.h"

#import <Foundation/Foundation.h>

#import "base/test/ios/wait_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/ui_metrics/canonical_url_share_metrics_types.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for the retrieving canonical URLs.
class CanonicalURLRetrieverTest : public PlatformTest {
 public:
  CanonicalURLRetrieverTest()
      : web_client_(std::make_unique<web::FakeWebClient>()) {
    browser_state_ = TestChromeBrowserState::Builder().Build();

    web::WebState::CreateParams params(browser_state_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
  }

  CanonicalURLRetrieverTest(const CanonicalURLRetrieverTest&) = delete;
  CanonicalURLRetrieverTest& operator=(const CanonicalURLRetrieverTest&) =
      delete;

  ~CanonicalURLRetrieverTest() override = default;

  void SetUp() override { PlatformTest::SetUp(); }

 protected:
  // Retrieves the canonical URL and returns it through the `url` out parameter.
  // Returns whether the operation was successful.
  bool RetrieveCanonicalUrl(GURL* url) {
    __block GURL result;
    __block bool canonical_url_received = false;
    activity_services::RetrieveCanonicalUrl(web_state(), ^(const GURL& url) {
      result = url;
      canonical_url_received = true;
    });

    bool success = base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForJSCompletionTimeout, ^{
          return canonical_url_received;
        });

    *url = result;
    return success;
  }

  web::WebState* web_state() { return web_state_.get(); }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;

  // Used to verify histogram logging.
  base::HistogramTester histogram_tester_;
};

// Validates that if the canonical URL is different from the visible URL, it is
// found and given to the completion block.
TEST_F(CanonicalURLRetrieverTest, TestCanonicalURLDifferentFromVisible) {
  web::test::LoadHtml(
      @"<link rel=\"canonical\" href=\"https://chromium.test\">",
      GURL("https://m.chromium.test/"), web_state());

  GURL url = GURL("garbage");
  bool success = RetrieveCanonicalUrl(&url);

  ASSERT_TRUE(success);
  EXPECT_EQ("https://chromium.test/", url);
  histogram_tester_.ExpectUniqueSample(
      ui_metrics::kCanonicalURLResultHistogram,
      ui_metrics::SUCCESS_CANONICAL_URL_DIFFERENT_FROM_VISIBLE, 1);
}

// Validates that if the canonical URL is the same as the visible URL, it is
// found and given to the completion block.
TEST_F(CanonicalURLRetrieverTest, TestCanonicalURLSameAsVisible) {
  web::test::LoadHtml(
      @"<link rel=\"canonical\" href=\"https://chromium.test\">",
      GURL("https://chromium.test/"), web_state());

  GURL url = GURL("garbage");
  bool success = RetrieveCanonicalUrl(&url);

  ASSERT_TRUE(success);
  EXPECT_EQ("https://chromium.test/", url);
  histogram_tester_.ExpectUniqueSample(
      ui_metrics::kCanonicalURLResultHistogram,
      ui_metrics::SUCCESS_CANONICAL_URL_SAME_AS_VISIBLE, 1);
}

// Validates that if there is no canonical URL, an empty GURL is given to the
// completion block.
TEST_F(CanonicalURLRetrieverTest, TestNoCanonicalURLFound) {
  web::test::LoadHtml(@"No canonical link on this page.",
                      GURL("https://m.chromium.test/"), web_state());

  GURL url = GURL("garbage");
  bool success = RetrieveCanonicalUrl(&url);

  ASSERT_TRUE(success);
  EXPECT_TRUE(url.is_empty());
  histogram_tester_.ExpectUniqueSample(
      ui_metrics::kCanonicalURLResultHistogram,
      ui_metrics::FAILED_NO_CANONICAL_URL_DEFINED, 1);
}

// Validates that if the found canonical URL is invalid, an empty GURL is
// given to the completion block.
TEST_F(CanonicalURLRetrieverTest, TestInvalidCanonicalFound) {
  web::test::LoadHtml(@"<link rel=\"canonical\" href=\"chromium\">",
                      GURL("https://m.chromium.test/"), web_state());

  GURL url = GURL("garbage");
  bool success = RetrieveCanonicalUrl(&url);

  ASSERT_TRUE(success);
  EXPECT_TRUE(url.is_empty());
  histogram_tester_.ExpectUniqueSample(ui_metrics::kCanonicalURLResultHistogram,
                                       ui_metrics::FAILED_CANONICAL_URL_INVALID,
                                       1);
}

// Validates that if multiple canonical URLs are found, the first one is given
// to the completion block.
TEST_F(CanonicalURLRetrieverTest, TestMultipleCanonicalURLsFound) {
  web::test::LoadHtml(
      @"<link rel=\"canonical\" href=\"https://chromium.test\">"
      @"<link rel=\"canonical\" href=\"https://chromium1.test\">",
      GURL("https://m.chromium.test/"), web_state());

  GURL url = GURL("garbage");
  bool success = RetrieveCanonicalUrl(&url);

  ASSERT_TRUE(success);
  EXPECT_EQ("https://chromium.test/", url);
  histogram_tester_.ExpectUniqueSample(
      ui_metrics::kCanonicalURLResultHistogram,
      ui_metrics::SUCCESS_CANONICAL_URL_DIFFERENT_FROM_VISIBLE, 1);
}

// Validates that if the visible and canonical URLs are http, an empty GURL is
// given to the completion block.
TEST_F(CanonicalURLRetrieverTest, TestCanonicalURLHTTP) {
  web::test::LoadHtml(@"<link rel=\"canonical\" href=\"http://chromium.test\">",
                      GURL("http://m.chromium.test/"), web_state());

  GURL url = GURL("garbage");
  bool success = RetrieveCanonicalUrl(&url);

  ASSERT_TRUE(success);
  EXPECT_TRUE(url.is_empty());
  histogram_tester_.ExpectUniqueSample(ui_metrics::kCanonicalURLResultHistogram,
                                       ui_metrics::FAILED_VISIBLE_URL_NOT_HTTPS,
                                       1);
}

// Validates that if the visible URL is HTTP but the canonical URL is HTTPS, an
// empty GURL is given to the completion block.
TEST_F(CanonicalURLRetrieverTest, TestCanonicalURLHTTPSUpgrade) {
  web::test::LoadHtml(
      @"<link rel=\"canonical\" href=\"https://chromium.test\">",
      GURL("http://m.chromium.test/"), web_state());

  GURL url = GURL("garbage");
  bool success = RetrieveCanonicalUrl(&url);

  ASSERT_TRUE(success);
  EXPECT_TRUE(url.is_empty());
  histogram_tester_.ExpectUniqueSample(ui_metrics::kCanonicalURLResultHistogram,
                                       ui_metrics::FAILED_VISIBLE_URL_NOT_HTTPS,
                                       1);
}

// Validates that if the visible URL is HTTPS but the canonical URL is HTTP, it
// is found and given to the completion block.
TEST_F(CanonicalURLRetrieverTest, TestCanonicalLinkHTTPSDowngrade) {
  web::test::LoadHtml(@"<link rel=\"canonical\" href=\"http://chromium.test\">",
                      GURL("https://m.chromium.test/"), web_state());

  GURL url = GURL("garbage");
  bool success = RetrieveCanonicalUrl(&url);

  ASSERT_TRUE(success);
  EXPECT_EQ("http://chromium.test/", url);
  histogram_tester_.ExpectUniqueSample(
      ui_metrics::kCanonicalURLResultHistogram,
      ui_metrics::SUCCESS_CANONICAL_URL_NOT_HTTPS, 1);
}
