// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/login_detection/login_detection_type.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_redirect/https_image_compression_infobar_decider.h"
#include "chrome/browser/subresource_redirect/subresource_redirect_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_redirect/common/subresource_redirect_result.h"
#include "components/subresource_redirect/subresource_redirect_browser_test_util.h"
#include "components/subresource_redirect/subresource_redirect_test_util.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace subresource_redirect {

const int kMaxRobotsRulesParsersCacheSize = 20;

class SubresourceRedirectLoginRobotsBrowserTest : public InProcessBrowserTest {
 public:
  explicit SubresourceRedirectLoginRobotsBrowserTest(
      const std::vector<std::pair<std::string, std::string>>&
          additional_feature_params = {},
      bool enable_lite_mode = true,
      bool enable_login_robots_compression_feature = true)
      : enable_lite_mode_(enable_lite_mode),
        enable_login_robots_compression_feature_(
            enable_login_robots_compression_feature),
        additional_feature_params_(additional_feature_params),
        https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ~SubresourceRedirectLoginRobotsBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII("host-rules", "MAP * 127.0.0.1");
    if (enable_lite_mode_)
      command_line->AppendSwitch("enable-spdy-proxy-auth");

    // Disable infobar shown check to actually compress the pages.
    command_line->AppendSwitch("override-https-image-compression-infobar");
  }

  void SetUp() override {
    ASSERT_TRUE(robots_rules_server_.Start());
    ASSERT_TRUE(image_compression_server_.Start());
    https_test_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(https_test_server_.Start());

    std::vector<base::test::ScopedFeatureList::FeatureAndParams>
        enabled_features;
    if (enable_login_robots_compression_feature_) {
      base::FieldTrialParams params, login_detection_params;
      params["enable_public_image_hints_based_compression"] = "false";
      params["enable_login_robots_based_compression"] = "true";
      params["lite_page_robots_origin"] = robots_rules_server_.GetURL();
      params["lite_page_subresource_origin"] =
          image_compression_server_.GetURL();
      // This rules fetch timeout is chosen such that the tests would have
      // enough time to fetch the rules without causing a timeout.
      params["robots_rules_receive_timeout_ms"] = "3000";
      // Allow first 3 images to be loaded faster.
      params["first_k_subresource_limit"] = "3";
      params["robots_rules_receive_first_k_timeout_ms"] = "2000";
      for (const auto& param : additional_feature_params_) {
        params[param.first] = param.second;
      }
      params["max_robots_rules_parsers_cache_size"] =
          base::NumberToString(kMaxRobotsRulesParsersCacheSize);
      enabled_features.emplace_back(blink::features::kSubresourceRedirect,
                                    params);
      login_detection_params["logged_in_sites"] = "https://loggedin.com";
      enabled_features.emplace_back(login_detection::kLoginDetection,
                                    login_detection_params);
    }
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
    InProcessBrowserTest::SetUp();
  }

  GURL GetHttpsTestURL(const std::string& path) const {
    return https_test_server_.GetURL("test_https_server.com", path);
  }

  void NavigateAndWaitForLoad(Browser* browser, const GURL& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, url));
    EXPECT_EQ(true, EvalJs(browser->tab_strip_model()->GetActiveWebContents(),
                           "checkImage()"));
    FetchHistogramsFromChildProcesses();
  }

  bool RunScriptExtractBool(const std::string& script,
                            content::WebContents* web_contents = nullptr) {
    if (!web_contents)
      web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    return EvalJs(web_contents, script).ExtractBool();
  }

  void VerifyImageCompressionPageInfoState(
      bool is_https_image_compression_applied,
      content::WebContents* web_contents = nullptr) {
    if (!web_contents)
      web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(is_https_image_compression_applied,
              subresource_redirect::SubresourceRedirectObserver::
                  IsHttpsImageCompressionApplied(web_contents));
  }

  void CreateUkmRecorder() {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  std::map<uint64_t, size_t> GetImageCompressionUkmMetrics() {
    base::RunLoop().RunUntilIdle();
    using ImageCompressionUkm = ukm::builders::PublicImageCompressionImageLoad;
    std::map<uint64_t, size_t> merged_metrics;
    // Flatten the metrics from multiple ukm sources.
    for (const auto* metrics :
         ukm_recorder_->GetEntriesByName(ImageCompressionUkm::kEntryName)) {
      for (const auto& metric : metrics->metrics) {
        if (merged_metrics.find(metric.first) == merged_metrics.end())
          merged_metrics[metric.first] = metric.second;
      }
    }
    return merged_metrics;
  }

  void VerifyRobotsRulesFetch(
      const std::set<std::string>& expected_robots_requests) {
    if (!expected_robots_requests.empty()) {
      histogram_tester_.ExpectBucketCount(
          "SubresourceRedirect.RobotsRulesFetcher.ResponseCode", net::HTTP_OK,
          expected_robots_requests.size());
      histogram_tester_.ExpectBucketCount(
          "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", false,
          expected_robots_requests.size());
    } else {
      histogram_tester_.ExpectTotalCount(
          "SubresourceRedirect.RobotsRulesFetcher.ResponseCode", 0);
      histogram_tester_.ExpectTotalCount(
          "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", 0);
    }
    robots_rules_server_.VerifyRequestedOrigins(expected_robots_requests);
  }

  void VerifyCompressedImageFetch(
      const std::set<std::string>& expected_image_requests) {
    if (!expected_image_requests.empty()) {
      histogram_tester_.ExpectUniqueSample(
          "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
          SubresourceRedirectResult::kRedirectable, 1);
      histogram_tester_.ExpectBucketCount(
          "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK,
          1);
      histogram_tester_.ExpectBucketCount(
          "SubresourceRedirect.CompressionAttempt.ResponseCode",
          net::HTTP_TEMPORARY_REDIRECT, 1);
      histogram_tester_.ExpectUniqueSample(
          "SubresourceRedirect.CompressionAttempt.ServerResponded", true, 1);
    } else {
      histogram_tester_.ExpectTotalCount(
          "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult", 0);
      histogram_tester_.ExpectTotalCount(
          "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);
      histogram_tester_.ExpectTotalCount(
          "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
    }
    histogram_tester_.ExpectTotalCount(
        "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);
    VerifyImageCompressionPageInfoState(true);
    image_compression_server_.VerifyRequestedImagePaths(
        expected_image_requests);

    using ImageCompressionUkm = ukm::builders::PublicImageCompressionImageLoad;
    auto ukm_metrics = GetImageCompressionUkmMetrics();
    EXPECT_LT(100U, ukm_metrics[ImageCompressionUkm::kOriginalBytesNameHash]);
    EXPECT_THAT(ukm_metrics,
                testing::Contains(testing::Key(
                    ImageCompressionUkm::kNavigationToRequestStartNameHash)));
    EXPECT_THAT(ukm_metrics,
                testing::Contains(testing::Key(
                    ImageCompressionUkm::kNavigationToRequestSentNameHash)));
    EXPECT_THAT(
        ukm_metrics,
        testing::Contains(testing::Key(
            ImageCompressionUkm::kNavigationToResponseReceivedNameHash)));
    EXPECT_THAT(ukm_metrics,
                testing::Contains(testing::Key(
                    ImageCompressionUkm::kRobotsRulesFetchLatencyNameHash)));
    if (!expected_image_requests.empty()) {
      EXPECT_EQ(SubresourceRedirectResult::kRedirectable,
                static_cast<SubresourceRedirectResult>(
                    ukm_metrics[ImageCompressionUkm::kRedirectResultNameHash]));
      EXPECT_LT(
          10U,
          ukm_metrics[ImageCompressionUkm::kCompressionPercentageNameHash]);
    } else {
      EXPECT_EQ(SubresourceRedirectResult::kIneligibleBlinkDisallowed,
                static_cast<SubresourceRedirectResult>(
                    ukm_metrics[ImageCompressionUkm::kRedirectResultNameHash]));
    }
  }

 protected:
  bool enable_lite_mode_;
  bool enable_login_robots_compression_feature_;

  // Additional feature params that are set for the subresource redirect
  // feature.
  std::vector<std::pair<std::string, std::string>> additional_feature_params_;

  base::test::ScopedFeatureList scoped_feature_list_;

  // Simulates the LitePages servers that return the robots rules and compress
  // images.
  RobotsRulesTestServer robots_rules_server_;
  ImageCompressionTestServer image_compression_server_;
  net::EmbeddedTestServer https_test_server_;

  base::HistogramTester histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

class SubresourceRedirectLoginRobotsLowMemoryBrowserTest
    : public SubresourceRedirectLoginRobotsBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  SubresourceRedirectLoginRobotsLowMemoryBrowserTest()
      : SubresourceRedirectLoginRobotsBrowserTest(
            {{"enable_login_robots_for_low_memory",
              is_login_robots_for_low_memory_feature_enabled() ? "true"
                                                               : "false"}},
            true, /* enable_lite_mode */
            true  /* enable_login_robots_compression_feature */
        ) {}

  ~SubresourceRedirectLoginRobotsLowMemoryBrowserTest() override = default;

  bool is_low_end_device() const { return std::get<0>(GetParam()); }

  bool is_login_robots_for_low_memory_feature_enabled() const {
    return std::get<1>(GetParam());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SubresourceRedirectLoginRobotsBrowserTest::SetUpCommandLine(command_line);
    if (is_low_end_device())
      command_line->AppendSwitch("enable-low-end-device-mode");
  }
};

// Enable tests for linux since LiteMode is enabled only for Android.
#if defined(OS_WIN) || defined(OS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) x
#endif

IN_PROC_BROWSER_TEST_F(SubresourceRedirectLoginRobotsBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(TestImageAllowedByRobots)) {
  CreateUkmRecorder();
  robots_rules_server_.AddRobotsRules(
      GetHttpsTestURL("/"),
      {{kRuleTypeAllow, "/load_image/image.png"}, {kRuleTypeDisallow, ""}});
  NavigateAndWaitForLoad(browser(), GetHttpsTestURL("/load_image/image.html"));

  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kRedirectable, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 1);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  VerifyRobotsRulesFetch({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png"});

  using ImageCompressionUkm = ukm::builders::PublicImageCompressionImageLoad;
  auto ukm_metrics = GetImageCompressionUkmMetrics();
  EXPECT_LT(100U, ukm_metrics[ImageCompressionUkm::kOriginalBytesNameHash]);
  EXPECT_LT(10U,
            ukm_metrics[ImageCompressionUkm::kCompressionPercentageNameHash]);
  EXPECT_THAT(ukm_metrics,
              testing::Contains(testing::Key(
                  ImageCompressionUkm::kNavigationToRequestStartNameHash)));
  EXPECT_THAT(ukm_metrics,
              testing::Contains(testing::Key(
                  ImageCompressionUkm::kNavigationToRequestSentNameHash)));
  EXPECT_THAT(ukm_metrics,
              testing::Contains(testing::Key(
                  ImageCompressionUkm::kNavigationToResponseReceivedNameHash)));
  EXPECT_THAT(ukm_metrics,
              testing::Contains(testing::Key(
                  ImageCompressionUkm::kRobotsRulesFetchLatencyNameHash)));
  EXPECT_EQ(SubresourceRedirectResult::kRedirectable,
            static_cast<SubresourceRedirectResult>(
                ukm_metrics[ImageCompressionUkm::kRedirectResultNameHash]));
  VerifyImageCompressionPageInfoState(true);
}

IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestImageDisallowedByRobots)) {
  CreateUkmRecorder();
  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeDisallow, ""}});
  NavigateAndWaitForLoad(browser(), GetHttpsTestURL("/load_image/image.html"));

  // The image will start redirect and pause when robots rules are getting
  // fetched. But when the robots rules disallows the image, it will reset and
  // fetch the original URL.
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kIneligibleRobotsDisallowed, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  VerifyRobotsRulesFetch({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths({});

  using ImageCompressionUkm = ukm::builders::PublicImageCompressionImageLoad;
  auto ukm_metrics = GetImageCompressionUkmMetrics();
  EXPECT_LT(100U, ukm_metrics[ImageCompressionUkm::kOriginalBytesNameHash]);
  EXPECT_THAT(ukm_metrics,
              testing::Not(testing::Contains(testing::Key(
                  ImageCompressionUkm::kCompressionPercentageNameHash))));
  EXPECT_THAT(ukm_metrics,
              testing::Contains(testing::Key(
                  ImageCompressionUkm::kNavigationToRequestStartNameHash)));
  EXPECT_THAT(ukm_metrics,
              testing::Contains(testing::Key(
                  ImageCompressionUkm::kNavigationToRequestSentNameHash)));
  EXPECT_THAT(ukm_metrics,
              testing::Contains(testing::Key(
                  ImageCompressionUkm::kNavigationToResponseReceivedNameHash)));
  EXPECT_THAT(ukm_metrics,
              testing::Contains(testing::Key(
                  ImageCompressionUkm::kRobotsRulesFetchLatencyNameHash)));
  EXPECT_EQ(SubresourceRedirectResult::kIneligibleRobotsDisallowed,
            static_cast<SubresourceRedirectResult>(
                ukm_metrics[ImageCompressionUkm::kRedirectResultNameHash]));

  // Page info would still show compression is enabled even when no image got
  // compressed.
  VerifyImageCompressionPageInfoState(true);
}

IN_PROC_BROWSER_TEST_F(SubresourceRedirectLoginRobotsBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(NoTriggerWhenDataSaverOff)) {
  data_reduction_proxy::DataReductionProxySettings::
      SetDataSaverEnabledForTesting(browser()->profile()->GetPrefs(), false);
  base::RunLoop().RunUntilIdle();

  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeAllow, ""}});
  NavigateAndWaitForLoad(browser(), GetHttpsTestURL("/load_image/image.html"));

  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  VerifyRobotsRulesFetch({});
  image_compression_server_.VerifyRequestedImagePaths({});
  VerifyImageCompressionPageInfoState(false);
}

IN_PROC_BROWSER_TEST_F(SubresourceRedirectLoginRobotsBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(NoTriggerInIncognito)) {
  auto* incognito_browser = CreateIncognitoBrowser();

  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeAllow, ""}});
  NavigateAndWaitForLoad(incognito_browser,
                         GetHttpsTestURL("/load_image/image.html"));

  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  VerifyRobotsRulesFetch({});
  image_compression_server_.VerifyRequestedImagePaths({});
  VerifyImageCompressionPageInfoState(false);
}

IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestRobotsRulesFetchTimeout)) {
  CreateUkmRecorder();
  robots_rules_server_.set_failure_mode(
      RobotsRulesTestServer::FailureMode::kTimeout);
  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeAllow, ""}});
  NavigateAndWaitForLoad(browser(), GetHttpsTestURL("/load_image/image.html"));

  // The image will start redirect and pause when robots rules are getting
  // fetched. But when the fetch timesout, it will reset and fetch the original
  // URL.
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);

  // Wait until the robots rules fetch times-out.
  RetryForHistogramUntilCountReached(
      &histogram_tester_, "SubresourceRedirect.RobotsRulesFetcher.ResponseCode",
      1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kIneligibleRobotsTimeout, 1);

  VerifyRobotsRulesFetch({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths({});

  using ImageCompressionUkm = ukm::builders::PublicImageCompressionImageLoad;
  auto ukm_metrics = GetImageCompressionUkmMetrics();
  EXPECT_LT(100U, ukm_metrics[ImageCompressionUkm::kOriginalBytesNameHash]);
  EXPECT_THAT(ukm_metrics,
              testing::Not(testing::Contains(testing::Key(
                  ImageCompressionUkm::kCompressionPercentageNameHash))));
  EXPECT_THAT(ukm_metrics,
              testing::Contains(testing::Key(
                  ImageCompressionUkm::kNavigationToRequestStartNameHash)));
  // Verify robots rules fetch took closer to the 1 second timeout.
  EXPECT_LT(900U,
            ukm_metrics[ImageCompressionUkm::kRobotsRulesFetchLatencyNameHash]);
  EXPECT_LT(900U,
            ukm_metrics[ImageCompressionUkm::kNavigationToRequestSentNameHash]);
  EXPECT_LT(
      900U,
      ukm_metrics[ImageCompressionUkm::kNavigationToResponseReceivedNameHash]);
  EXPECT_EQ(SubresourceRedirectResult::kIneligibleRobotsTimeout,
            static_cast<SubresourceRedirectResult>(
                ukm_metrics[ImageCompressionUkm::kRedirectResultNameHash]));
  VerifyImageCompressionPageInfoState(true);
}

IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestOneImageAllowedOneDisallowed)) {
  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeDisallow, "*foo"}});
  NavigateAndWaitForLoad(browser(),
                         GetHttpsTestURL("/load_image/two_images.html"));

  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kRedirectable, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kIneligibleRobotsDisallowed, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 2);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  VerifyRobotsRulesFetch({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png"});
  VerifyImageCompressionPageInfoState(true);
}

IN_PROC_BROWSER_TEST_F(SubresourceRedirectLoginRobotsBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(TestTwoImagesAllowed)) {
  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeAllow, ""}});
  NavigateAndWaitForLoad(browser(),
                         GetHttpsTestURL("/load_image/two_images.html"));

  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kRedirectable, 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 2);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 2);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  VerifyRobotsRulesFetch({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png", "/load_image/image.png?foo"});
  VerifyImageCompressionPageInfoState(true);
}

// Verify an new image loads fine after robots rules fetch is complete.
IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestImageLoadAfterRobotsFetch)) {
  robots_rules_server_.AddRobotsRules(
      GetHttpsTestURL("/"),
      {{kRuleTypeAllow, "/load_image/image.png"}, {kRuleTypeDisallow, ""}});
  NavigateAndWaitForLoad(browser(), GetHttpsTestURL("/load_image/image.html"));

  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kRedirectable, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 1);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", true, 1);

  VerifyRobotsRulesFetch({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png"});

  // Load another image and that will be immediately redirected as well.
  EXPECT_TRUE(RunScriptExtractBool(R"(loadNewImage("image.png?foo"))"));
  FetchHistogramsFromChildProcesses();
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kRedirectable, 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 2);
  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));

  // No more new robots rules fetches.
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", 1);
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png", "/load_image/image.png?foo"});
  VerifyImageCompressionPageInfoState(true);
}

IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestDifferentOriginImageLoad)) {
  robots_rules_server_.AddRobotsRules(
      GetHttpsTestURL("/"),
      {{kRuleTypeAllow, "/load_image/image.png"}, {kRuleTypeDisallow, ""}});
  NavigateAndWaitForLoad(browser(), GetHttpsTestURL("/load_image/image.html"));

  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kRedirectable, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 1);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotRulesDecider.ApplyDuration", 1);

  VerifyRobotsRulesFetch({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png"});

  // Load a compressible image from different origin and that will trigger
  // robots rules fetch.
  robots_rules_server_.AddRobotsRules(
      https_test_server_.GetURL("differentorigin.com", "/"),
      {{kRuleTypeDisallow, "*disallowed*"}});
  EXPECT_TRUE(RunScriptExtractBool(content::JsReplace(
      "loadNewImage($1)",
      https_test_server_.GetURL("differentorigin.com",
                                "/load_image/image.png?allowed"))));
  FetchHistogramsFromChildProcesses();
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kRedirectable, 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 2);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotRulesDecider.ApplyDuration", 2);

  // Another robots rules fetch happened.
  VerifyRobotsRulesFetch(
      {GetHttpsTestURL("/").spec(),
       https_test_server_.GetURL("differentorigin.com", "/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png", "/load_image/image.png?allowed"});

  // Load a disallowed image from the different origin.
  EXPECT_TRUE(RunScriptExtractBool(content::JsReplace(
      "loadNewImage($1)",
      https_test_server_.GetURL("differentorigin.com",
                                "/load_image/image.png?disallowed"))));
  FetchHistogramsFromChildProcesses();
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kIneligibleRobotsDisallowed, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 4);

  // No more new robots rules fetches.
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", 2);
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png", "/load_image/image.png?allowed"});
  VerifyImageCompressionPageInfoState(true);
}

// Verifies that LitePages gets blocked due to robots fetch failure, and
// subsequent robots rules fetch does not happen.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectLoginRobotsBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(TestRobotsFetchLoadshed)) {
  robots_rules_server_.set_failure_mode(
      RobotsRulesTestServer::FailureMode::kLoadshed503RetryAfterResponse);
  NavigateAndWaitForLoad(browser(), GetHttpsTestURL("/load_image/image.html"));
  RetryForHistogramUntilCountReached(
      &histogram_tester_,
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult", 1);

  // One robots rules fetch failure should result in LitePages block.
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode",
      net::HTTP_SERVICE_UNAVAILABLE, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", false, 1);
  // Bypass check happens twice - once for pageload, and once for robots
  // fetch.
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.LitePagesService.BypassResult", false, 2);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotRulesDecider.ApplyDuration", 0);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kIneligibleRobotsDisallowed, 1);

  robots_rules_server_.VerifyRequestedOrigins({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths({});

  // Load an image from different origin and that should not trigger robots
  // rules fetch, since LitePages is blocked.
  EXPECT_TRUE(RunScriptExtractBool(content::JsReplace(
      "loadNewImage($1)",
      https_test_server_.GetURL("differentorigin.com",
                                "/load_image/image.png?allowed"))));
  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));
  RetryForHistogramUntilCountReached(
      &histogram_tester_,
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult", 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.LitePagesService.BypassResult", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotRulesDecider.ApplyDuration", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kIneligibleRobotsDisallowed, 2);

  // No more additional fetches.
  robots_rules_server_.VerifyRequestedOrigins({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths({});
  VerifyImageCompressionPageInfoState(true);
}

// Verifies that when an image load fails, LitePages gets blocked, and
// subsequent robots rules fetch, LitePages image loads does not happen.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectLoginRobotsBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(TestImageFetchLoadshed)) {
  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeAllow, ""}});
  image_compression_server_.set_failure_mode(
      ImageCompressionTestServer::FailureMode::kLoadshed503RetryAfterResponse);
  NavigateAndWaitForLoad(browser(), GetHttpsTestURL("/load_image/image.html"));

  // Robots rules fetch was success.
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotRulesDecider.ApplyDuration", 1);

  // One compressed image fetch failed and then loaded directly.
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kRedirectable, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_SERVICE_UNAVAILABLE, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);

  // Bypass check happens twice - once for pageload, and once for robots
  // fetch.
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.LitePagesService.BypassResult", false, 2);

  VerifyRobotsRulesFetch({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png"});

  // Load an image from different origin and that should not trigger robots
  // rules fetch, since LitePages is blocked.
  EXPECT_TRUE(RunScriptExtractBool(content::JsReplace(
      "loadNewImage($1)",
      https_test_server_.GetURL("differentorigin.com",
                                "/load_image/image.png?allowed"))));
  FetchHistogramsFromChildProcesses();
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kIneligibleRobotsDisallowed, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.LitePagesService.BypassResult", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotRulesDecider.ApplyDuration", 1);
  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));

  // No more additional fetches.
  VerifyRobotsRulesFetch({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png"});
  VerifyImageCompressionPageInfoState(true);
}

IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestNoCompressionOnLoggedInPage)) {
  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeAllow, "*"}});
  // Trigger OAuth login by triggering OAuth start and complete.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetHttpsTestURL("/simple.html?initial")));
  histogram_tester_.ExpectUniqueSample(
      "Login.PageLoad.DetectionType",
      login_detection::LoginDetectionType::kNoLogin, 1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server_.GetURL("oauth_server.com",
                                           "/simple.html?client_id=user")));
  histogram_tester_.ExpectBucketCount(
      "Login.PageLoad.DetectionType",
      login_detection::LoginDetectionType::kNoLogin, 2);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetHttpsTestURL("/simple.html?code=123")));
  histogram_tester_.ExpectBucketCount(
      "Login.PageLoad.DetectionType",
      login_detection::LoginDetectionType::kOauthFirstTimeLoginFlow, 1);

  // The next navigation will be treated as logged-in.
  NavigateAndWaitForLoad(browser(), GetHttpsTestURL("/load_image/image.html"));
  histogram_tester_.ExpectBucketCount(
      "Login.PageLoad.DetectionType",
      login_detection::LoginDetectionType::kOauthLogin, 1);

  // No image compression will be triggered.
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kIneligibleLoginDetected, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  VerifyRobotsRulesFetch({});
  image_compression_server_.VerifyRequestedImagePaths({});
  VerifyImageCompressionPageInfoState(false);
}

// Tests images in subframe are compressed.
IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestSubframeImageAllowedByRobots)) {
  robots_rules_server_.AddRobotsRules(
      GetHttpsTestURL("/"),
      {{kRuleTypeAllow, "/load_image/image.png"}, {kRuleTypeDisallow, ""}});
  NavigateAndWaitForLoad(browser(),
                         GetHttpsTestURL("/load_image/page_with_iframe.html"));
  EXPECT_EQ(true, EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                         "checkSubframeImage()"));
  FetchHistogramsFromChildProcesses();

  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kRedirectable, 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 2);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", true, 2);
  // The robots rules are fetched once, since both images are from the same
  // origin.
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  VerifyRobotsRulesFetch({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png?mainframe", "/load_image/image.png"});
  VerifyImageCompressionPageInfoState(true);
}

// Tests images in cross-origin subframe are compressed.
IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestCrossOriginSubframeImageAllowedByRobots)) {
  robots_rules_server_.AddRobotsRules(
      GetHttpsTestURL("/"),
      {{kRuleTypeAllow, "/load_image/image.png"}, {kRuleTypeDisallow, ""}});
  NavigateAndWaitForLoad(
      browser(), GetHttpsTestURL(net::test_server::GetFilePathWithReplacements(
                     "/load_image/page_with_crossorigin_iframe.html",
                     {{"REPLACE_WITH_BASE_URL",
                       https_test_server_.GetURL("foo.com", "/").spec()}})));

  // Wait for the histograms, since javascript cannot be used to wait for
  // loading of the image in the subframe.
  RetryForHistogramUntilCountReached(
      &histogram_tester_,
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 2);

  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kRedirectable, 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 2);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", true, 2);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  VerifyRobotsRulesFetch({GetHttpsTestURL("/").spec(),
                          https_test_server_.GetURL("foo.com", "/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png?mainframe", "/load_image/image.png"});
  VerifyImageCompressionPageInfoState(true);
}

IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestLoggedInSubframeDisallowed)) {
  robots_rules_server_.AddRobotsRules(
      GetHttpsTestURL("/"),
      {{kRuleTypeAllow, "/load_image/image.png"}, {kRuleTypeDisallow, ""}});
  NavigateAndWaitForLoad(
      browser(),
      GetHttpsTestURL(net::test_server::GetFilePathWithReplacements(
          "/load_image/page_with_crossorigin_iframe.html",
          {{"REPLACE_WITH_BASE_URL",
            https_test_server_.GetURL("loggedin.com", "/").spec()}})));

  // Wait for the histograms, since javascript cannot be used to wait for
  // loading of the image in the crossorigin subframe.
  RetryForHistogramUntilCountReached(&histogram_tester_,
                                     "Blink.DecodedImageType", 2);
  RetryForHistogramUntilCountReached(
      &histogram_tester_,
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult", 2);

  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kRedirectable, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kIneligibleLoginDetected, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 1);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  VerifyRobotsRulesFetch({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png?mainframe"});
  // Main frame still enables image compression.
  VerifyImageCompressionPageInfoState(true);
}

IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestLoggedInMainframeDisallowsSubframe)) {
  robots_rules_server_.AddRobotsRules(
      https_test_server_.GetURL("loggedin.com", "/"),
      {{kRuleTypeAllow, "/load_image/image.png"}, {kRuleTypeDisallow, ""}});
  NavigateAndWaitForLoad(
      browser(), https_test_server_.GetURL(
                     "loggedin.com", "/load_image/page_with_iframe.html"));

  // Wait for the histograms, since javascript cannot be used to wait for
  // loading of the image in the crossorigin subframe.
  RetryForHistogramUntilCountReached(&histogram_tester_,
                                     "Blink.DecodedImageType", 2);
  RetryForHistogramUntilCountReached(
      &histogram_tester_,
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult", 2);

  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kIneligibleLoginDetected, 2);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  VerifyRobotsRulesFetch({});
  image_compression_server_.VerifyRequestedImagePaths({});
  VerifyImageCompressionPageInfoState(false);
}

IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestFirstKImagesLoadFaster)) {
  robots_rules_server_.set_failure_mode(
      RobotsRulesTestServer::FailureMode::kTimeout);
  NavigateAndWaitForLoad(browser(), GetHttpsTestURL("/load_image/image.html"));

  // Load 2 more images from different domain, so that they will fetch different
  // robots rules and will all timeout with the shorter first k timeout
  // duration.
  for (const char* image_origin : {
           "foo1.com",
           "foo2.com",
       }) {
    base::ElapsedTimer elapsed_timer;
    std::string load_image_url = base::StrCat(
        {"loadNewImage('",
         https_test_server_.GetURL(image_origin, "/load_image/image.png")
             .spec(),
         "')"});
    EXPECT_TRUE(RunScriptExtractBool(load_image_url));
    EXPECT_TRUE(RunScriptExtractBool("checkImage()"));
    // The image should load closer to 2 seconds.
    EXPECT_LT(base::Seconds(0.9), elapsed_timer.Elapsed());
    EXPECT_GT(base::Seconds(2.6), elapsed_timer.Elapsed());
  }

  FetchHistogramsFromChildProcesses();
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 3);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult", 3);

  // The next image should use longer robots rules fetch timeout duration,
  // since this is past the first K limit.
  base::ElapsedTimer elapsed_timer;
  std::string load_image_url = base::StrCat(
      {"loadNewImage('",
       https_test_server_.GetURL("bar1.com", "/load_image/image.png").spec(),
       "')"});
  EXPECT_TRUE(RunScriptExtractBool(load_image_url));
  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));
  // The image should load closer to 3 seconds.
  EXPECT_LT(base::Seconds(2.9), elapsed_timer.Elapsed());

  FetchHistogramsFromChildProcesses();
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 4);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult", 4);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  VerifyImageCompressionPageInfoState(true);
}

IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestRobotsRulesFetchedInPreloadScanner)) {
  CreateUkmRecorder();
  robots_rules_server_.AddRobotsRules(
      GetHttpsTestURL("/"),
      {{kRuleTypeAllow, "/load_image/image.png"}, {kRuleTypeDisallow, ""}});
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetHttpsTestURL("/load_image/preload_scanner_image.html")));

  // The robots rules will be fetched, but the image will not load.
  RetryForHistogramUntilCountReached(
      &histogram_tester_, "SubresourceRedirect.RobotsRulesFetcher.ResponseCode",
      1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);

  VerifyRobotsRulesFetch({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths({});

  // Now start loading the image.
  EXPECT_TRUE(
      content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "loadBelowViewportImage()"));
  EXPECT_EQ(true, EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                         "checkImage()"));
  FetchHistogramsFromChildProcesses();

  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kRedirectable, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 1);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  VerifyRobotsRulesFetch({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png"});
}

IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestRobotsRulesInMemoryCacheEviction)) {
  std::set<std::string> expected_robots_rules_requests;
  std::set<std::string> expected_compressed_image_requests;
  robots_rules_server_.set_failure_mode(
      RobotsRulesTestServer::FailureMode::kTimeout);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetHttpsTestURL("/load_image/image.html")));
  expected_robots_rules_requests.emplace(GetHttpsTestURL("/").spec());
  expected_compressed_image_requests.emplace(
      GetHttpsTestURL("/load_image/image.html").spec());

  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", false, 1);

  // Load images from lot of different origins which will hit the robots
  // rules in-memory cache limit specified by feature param
  // max_robots_rules_parsers_cache_size.
  for (int i = 0; i < kMaxRobotsRulesParsersCacheSize + 5; i++) {
    GURL image_url = https_test_server_.GetURL(
        base::StringPrintf("foo%d.com", i), "/load_image/image.png?allowed");
    ASSERT_TRUE(
        ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
               content::JsReplace("document.images[0].src = $1;", image_url)));
    expected_robots_rules_requests.emplace(image_url.GetWithEmptyPath().spec());
    if (expected_compressed_image_requests.size() <=
        kMaxRobotsRulesParsersCacheSize) {
      expected_compressed_image_requests.emplace(image_url.spec());
    }
  }

  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", false,
      kMaxRobotsRulesParsersCacheSize + 5 + 1);
}

// Verifies that the image is only compressed in low memory device with the low
// memory feature flag enabled, and in non-low memory devices.
IN_PROC_BROWSER_TEST_P(SubresourceRedirectLoginRobotsLowMemoryBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(TestLowMemoryDisableMode)) {
  CreateUkmRecorder();
  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeAllow, ""}});
  NavigateAndWaitForLoad(browser(), GetHttpsTestURL("/load_image/image.html"));

  if (!is_login_robots_for_low_memory_feature_enabled() &&
      is_low_end_device()) {
    // No compression when the low memory feature is disabled on low memory
    // device
    histogram_tester_.ExpectTotalCount(
        "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);
    histogram_tester_.ExpectTotalCount(
        "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
    histogram_tester_.ExpectTotalCount(
        "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

    VerifyRobotsRulesFetch({});
    image_compression_server_.VerifyRequestedImagePaths({});
    VerifyImageCompressionPageInfoState(false);
    return;
  }

  // Compression happens in all the other cases:
  //  1. Low end device with low memory feature flag enabled.
  //  3. Non-low end device.
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 1);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  VerifyRobotsRulesFetch({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png"});

  using ImageCompressionUkm = ukm::builders::PublicImageCompressionImageLoad;
  auto ukm_metrics = GetImageCompressionUkmMetrics();
  EXPECT_LT(100U, ukm_metrics[ImageCompressionUkm::kOriginalBytesNameHash]);
  EXPECT_LT(10U,
            ukm_metrics[ImageCompressionUkm::kCompressionPercentageNameHash]);
  EXPECT_THAT(ukm_metrics,
              testing::Contains(testing::Key(
                  ImageCompressionUkm::kNavigationToRequestStartNameHash)));
  EXPECT_THAT(ukm_metrics,
              testing::Contains(testing::Key(
                  ImageCompressionUkm::kNavigationToRequestSentNameHash)));
  EXPECT_THAT(ukm_metrics,
              testing::Contains(testing::Key(
                  ImageCompressionUkm::kNavigationToResponseReceivedNameHash)));
  EXPECT_THAT(ukm_metrics,
              testing::Contains(testing::Key(
                  ImageCompressionUkm::kRobotsRulesFetchLatencyNameHash)));
  EXPECT_THAT(ukm_metrics, testing::Contains(testing::Key(
                               ImageCompressionUkm::kRedirectResultNameHash)));
  VerifyImageCompressionPageInfoState(true);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SubresourceRedirectLoginRobotsLowMemoryBrowserTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

class SubresourceRedirectLoginRobotsFirstKDisableBrowserTest
    : public SubresourceRedirectLoginRobotsBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  SubresourceRedirectLoginRobotsFirstKDisableBrowserTest()
      : SubresourceRedirectLoginRobotsBrowserTest(
            {{"first_k_disable_subresource_redirect_limit", "1"}},
            true, /* enable_lite_mode */
            true  /* enable_login_robots_compression_feature */
        ) {}
};

// Test that first image is disallowed for compression and only the second image
// is compressed.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectLoginRobotsFirstKDisableBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(TestFirstKImageDisallowed)) {
  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeAllow, ""}});
  NavigateAndWaitForLoad(browser(),
                         GetHttpsTestURL("/load_image/two_images.html"));

  EXPECT_TRUE(RunScriptExtractBool("checkBothImagesLoaded()"));
  RetryForHistogramUntilCountReached(
      &histogram_tester_,
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult", 2);

  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kRedirectable, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kIneligibleFirstKDisableSubresourceRedirect,
      1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  VerifyRobotsRulesFetch({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png?foo"});
  VerifyImageCompressionPageInfoState(true);
}

class SubresourceRedirectLoginRobotsJavascriptImageBrowserTest
    : public SubresourceRedirectLoginRobotsBrowserTest,
      public testing::WithParamInterface<
          std::tuple<bool /* allow_javascript_crossorigin_images */,
                     bool /* is_crossorigin_image */>> {
 public:
  SubresourceRedirectLoginRobotsJavascriptImageBrowserTest()
      : SubresourceRedirectLoginRobotsBrowserTest(
            {{"allow_javascript_crossorigin_images",
              allow_javascript_crossorigin_images() ? "true" : "false"}},
            true, /* enable_lite_mode */
            true  /* enable_login_robots_compression_feature */
        ) {}

  bool allow_javascript_crossorigin_images() const {
    return std::get<0>(GetParam());
  }
  bool is_crossorigin_image() const { return std::get<1>(GetParam()); }
};

IN_PROC_BROWSER_TEST_P(
    SubresourceRedirectLoginRobotsJavascriptImageBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestJavascriptCrossOriginImageAllowed)) {
  bool is_compression_expected =
      is_crossorigin_image() && allow_javascript_crossorigin_images();
  GURL image_url =
      is_crossorigin_image()
          ? https_test_server_.GetURL("foo.com", "/load_image/image.png")
          : GetHttpsTestURL("/load_image/image.png");

  CreateUkmRecorder();
  robots_rules_server_.AddRobotsRules(
      image_url.GetWithEmptyPath(),
      {{kRuleTypeAllow, "/load_image/image.png"}, {kRuleTypeDisallow, ""}});
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetHttpsTestURL("/simple.html")));

  EXPECT_EQ(true, EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                         content::JsReplace(
                             R"(
    new Promise(resolve => {
        const img = document.createElement("img");
        img.onload = () => {
            resolve(true);
        }
        img.src = $1;
        document.body.appendChild(img);
    });)",
                             image_url)));
  FetchHistogramsFromChildProcesses();

  if (is_compression_expected) {
    VerifyRobotsRulesFetch({image_url.GetWithEmptyPath().spec()});
    VerifyCompressedImageFetch({"/load_image/image.png"});
  } else {
    VerifyRobotsRulesFetch({});
    VerifyCompressedImageFetch({});
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SubresourceRedirectLoginRobotsJavascriptImageBrowserTest,
    testing::Combine(testing::Bool() /* allow_javascript_crossorigin_images */,
                     testing::Bool() /* is_crossorigin_image */));

// Disables the actual subresource redirect and enables only recording metrics.
class SubresourceRedirectLoginRobotsRedirectDisabledBrowserTest
    : public SubresourceRedirectLoginRobotsBrowserTest {
 public:
  SubresourceRedirectLoginRobotsRedirectDisabledBrowserTest()
      : SubresourceRedirectLoginRobotsBrowserTest(
            {{"enable_subresource_server_redirect", "false"}},
            true, /* enable_lite_mode */
            true  /* enable_login_robots_compression_feature */
        ) {}
};

IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsRedirectDisabledBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestImageAllowedByRobots)) {
  CreateUkmRecorder();
  robots_rules_server_.AddRobotsRules(
      GetHttpsTestURL("/"),
      {{kRuleTypeAllow, "/load_image/image.png"}, {kRuleTypeDisallow, ""}});
  NavigateAndWaitForLoad(browser(), GetHttpsTestURL("/load_image/image.html"));

  VerifyRobotsRulesFetch({GetHttpsTestURL("/").spec()});

  // The image will not be redirected.
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kIneligibleCompressionDisabled, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);
  image_compression_server_.VerifyRequestedImagePaths({});

  // Image load UKM should get recorded.
  using ImageCompressionUkm = ukm::builders::PublicImageCompressionImageLoad;
  auto ukm_metrics = GetImageCompressionUkmMetrics();
  EXPECT_LT(100U, ukm_metrics[ImageCompressionUkm::kOriginalBytesNameHash]);
  EXPECT_THAT(ukm_metrics,
              testing::Not(testing::Contains(testing::Key(
                  ImageCompressionUkm::kCompressionPercentageNameHash))));
  EXPECT_THAT(ukm_metrics,
              testing::Contains(testing::Key(
                  ImageCompressionUkm::kNavigationToRequestStartNameHash)));
  EXPECT_THAT(ukm_metrics,
              testing::Contains(testing::Key(
                  ImageCompressionUkm::kNavigationToRequestSentNameHash)));
  EXPECT_THAT(ukm_metrics,
              testing::Contains(testing::Key(
                  ImageCompressionUkm::kNavigationToResponseReceivedNameHash)));
  EXPECT_THAT(ukm_metrics,
              testing::Contains(testing::Key(
                  ImageCompressionUkm::kRobotsRulesFetchLatencyNameHash)));
  EXPECT_EQ(SubresourceRedirectResult::kIneligibleCompressionDisabled,
            static_cast<SubresourceRedirectResult>(
                ukm_metrics[ImageCompressionUkm::kRedirectResultNameHash]));
  VerifyImageCompressionPageInfoState(false);
}

IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsRedirectDisabledBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestImageDisallowedByRobots)) {
  CreateUkmRecorder();
  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeDisallow, ""}});
  NavigateAndWaitForLoad(browser(), GetHttpsTestURL("/load_image/image.html"));

  VerifyRobotsRulesFetch({GetHttpsTestURL("/").spec()});

  // The image will not be redirected.
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kIneligibleRobotsDisallowed, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);
  image_compression_server_.VerifyRequestedImagePaths({});

  // Image load UKM should get recorded.
  using ImageCompressionUkm = ukm::builders::PublicImageCompressionImageLoad;
  auto ukm_metrics = GetImageCompressionUkmMetrics();
  EXPECT_LT(100U, ukm_metrics[ImageCompressionUkm::kOriginalBytesNameHash]);
  EXPECT_THAT(ukm_metrics,
              testing::Not(testing::Contains(testing::Key(
                  ImageCompressionUkm::kCompressionPercentageNameHash))));
  EXPECT_THAT(ukm_metrics,
              testing::Contains(testing::Key(
                  ImageCompressionUkm::kNavigationToRequestStartNameHash)));
  EXPECT_THAT(ukm_metrics,
              testing::Contains(testing::Key(
                  ImageCompressionUkm::kNavigationToRequestSentNameHash)));
  EXPECT_THAT(ukm_metrics,
              testing::Contains(testing::Key(
                  ImageCompressionUkm::kNavigationToResponseReceivedNameHash)));
  EXPECT_THAT(ukm_metrics,
              testing::Contains(testing::Key(
                  ImageCompressionUkm::kRobotsRulesFetchLatencyNameHash)));
  EXPECT_EQ(SubresourceRedirectResult::kIneligibleRobotsDisallowed,
            static_cast<SubresourceRedirectResult>(
                ukm_metrics[ImageCompressionUkm::kRedirectResultNameHash]));
  VerifyImageCompressionPageInfoState(false);
}

class SubresourceRedirectLoginRobotsCSPRestrictedImageBrowserTest
    : public SubresourceRedirectLoginRobotsBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  SubresourceRedirectLoginRobotsCSPRestrictedImageBrowserTest()
      : SubresourceRedirectLoginRobotsBrowserTest(
            {{"allow_csp_restricted_images",
              allow_csp_restricted_images() ? "true" : "false"}},
            true, /* enable_lite_mode */
            true  /* enable_login_robots_compression_feature */
        ) {}

  bool allow_csp_restricted_images() const { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(
    SubresourceRedirectLoginRobotsCSPRestrictedImageBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestDefaultSrcRestrictedImageAllowed)) {
  CreateUkmRecorder();
  robots_rules_server_.AddRobotsRules(
      GetHttpsTestURL("/"),
      {{kRuleTypeAllow, "/load_image/image.png"}, {kRuleTypeDisallow, ""}});
  NavigateAndWaitForLoad(browser(),
                         GetHttpsTestURL("/load_image/image_csp_img_src.html"));

  if (allow_csp_restricted_images()) {
    VerifyRobotsRulesFetch({GetHttpsTestURL("/").spec()});
    VerifyCompressedImageFetch({"/load_image/image.png"});
  } else {
    VerifyRobotsRulesFetch({});
    VerifyCompressedImageFetch({});
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SubresourceRedirectLoginRobotsCSPRestrictedImageBrowserTest,
    testing::Bool() /* allow_csp_restricted_images */);

}  // namespace subresource_redirect
