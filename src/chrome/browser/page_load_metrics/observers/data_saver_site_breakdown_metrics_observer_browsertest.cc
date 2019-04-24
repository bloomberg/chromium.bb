// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/proto/data_store.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Return a plaintext response.
std::unique_ptr<net::test_server::HttpResponse>
HandleResourceRequestWithPlaintextMimeType(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response =
      std::make_unique<net::test_server::BasicHttpResponse>();

  response->set_code(net::HttpStatusCode::HTTP_OK);
  response->set_content("Some non-HTML content.");
  response->set_content_type("text/plain");

  return response;
}

}  // namespace

class DataSaverSiteBreakdownMetricsObserverBrowserTest
    : public InProcessBrowserTest {
 protected:
  void EnableDataSaver() {
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(prefs::kDataSaverEnabled, true);
    prefs->SetBoolean(data_reduction_proxy::prefs::kDataUsageReportingEnabled,
                      true);
    // Give the setting notification a chance to propagate.
    base::RunLoop().RunUntilIdle();
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        data_reduction_proxy::features::
            kDataSaverSiteBreakdownUsingPageLoadMetrics);
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        data_reduction_proxy::switches::kEnableDataReductionProxy);
  }

  // Gets the data usage recorded against the host the embedded server runs on.
  uint64_t GetDataUsage(const std::string& host) {
    const auto& data_usage_map =
        DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
            browser()->profile())
            ->data_reduction_proxy_service()
            ->compression_stats()
            ->DataUsageMapForTesting();
    const auto& it = data_usage_map.find(host);
    if (it != data_usage_map.end())
      return it->second->data_used();
    return 0;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DataSaverSiteBreakdownMetricsObserverBrowserTest,
                       NavigateToSimplePage) {
  const struct {
    std::string url;
    size_t expected_min_page_size;
    size_t expected_max_page_size;
  } tests[] = {
      // The range of the pages is calculated approximately from the html size
      // and the size of the subresources it includes.
      {"/google/google.html", 5000, 20000},
      {"/simple.html", 100, 1000},
      {"/media/youtube.html", 5000, 20000},
  };
  ASSERT_TRUE(embedded_test_server()->Start());

  EnableDataSaver();
  for (const auto& test : tests) {
    GURL test_url(embedded_test_server()->GetURL(test.url));
    uint64_t data_usage_before_navigation =
        GetDataUsage(test_url.HostNoBrackets());
    ui_test_utils::NavigateToURL(browser(),
                                 embedded_test_server()->GetURL(test.url));

    base::RunLoop().RunUntilIdle();
    // Navigate away to force the histogram recording.
    ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

    EXPECT_LE(
        test.expected_min_page_size,
        GetDataUsage(test_url.HostNoBrackets()) - data_usage_before_navigation);
    EXPECT_GE(
        test.expected_max_page_size,
        GetDataUsage(test_url.HostNoBrackets()) - data_usage_before_navigation);
  }
}

IN_PROC_BROWSER_TEST_F(DataSaverSiteBreakdownMetricsObserverBrowserTest,
                       NavigateToPlaintext) {
  std::unique_ptr<net::EmbeddedTestServer> plaintext_server =
      std::make_unique<net::EmbeddedTestServer>(
          net::EmbeddedTestServer::TYPE_HTTPS);
  plaintext_server->RegisterRequestHandler(
      base::BindRepeating(&HandleResourceRequestWithPlaintextMimeType));
  ASSERT_TRUE(plaintext_server->Start());

  EnableDataSaver();

  GURL test_url(plaintext_server->GetURL("/page"));

  uint64_t data_usage_before_navigation =
      GetDataUsage(test_url.HostNoBrackets());

  ui_test_utils::NavigateToURL(browser(), test_url);
  base::RunLoop().RunUntilIdle();

  // Navigate away to force the histogram recording.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Choose reasonable minimum (10 is the content length).
  EXPECT_LE(10u, GetDataUsage(test_url.HostNoBrackets()) -
                     data_usage_before_navigation);
  // Choose reasonable maximum (500 is the most we expect from headers).
  EXPECT_GE(500u, GetDataUsage(test_url.HostNoBrackets()) -
                      data_usage_before_navigation);
}
