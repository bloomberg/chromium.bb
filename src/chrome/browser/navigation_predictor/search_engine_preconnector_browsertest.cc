// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/preconnect_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/url_constants.h"

namespace {

// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
GURL FakeSearch() {
  return GURL("https://www.fakesearch.com/");
}

GURL GoogleSearch() {
  return GURL("https://www.google.com/");
}

class SearchEnginePreconnectorBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest,
      public predictors::PreconnectManager::Observer {
 public:
  SearchEnginePreconnectorBrowserTest()
      : subresource_filter::SubresourceFilterBrowserTest() {}

  ~SearchEnginePreconnectorBrowserTest() override = default;

  void SetUp() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory(
        "chrome/test/data/navigation_predictor");
    ASSERT_TRUE(https_server_->Start());

    preresolve_counts_[GetTestURL("/").GetOrigin()] = 0;
    preresolve_counts_[GoogleSearch()] = 0;
    preresolve_counts_[FakeSearch()] = 0;

    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    subresource_filter::SubresourceFilterBrowserTest::SetUpOnMainThread();
    host_resolver()->ClearRules();

    auto* loading_predictor =
        predictors::LoadingPredictorFactory::GetForProfile(
            browser()->profile());
    ASSERT_TRUE(loading_predictor);
    loading_predictor->preconnect_manager()->SetObserverForTesting(this);
  }

  const GURL GetTestURL(const char* file) const {
    return https_server_->GetURL(file);
  }

  void OnPreresolveFinished(
      const GURL& url,
      const net::NetworkIsolationKey& network_isolation_key,
      bool success) override {
    if (!base::Contains(preresolve_counts_, url.GetOrigin())) {
      return;
    }

    // Only the test URL should successfully preconnect.
    EXPECT_EQ(url.GetOrigin() == GetTestURL("/").GetOrigin(), success);

    preresolve_counts_[url.GetOrigin()]++;
    if (run_loops_[url.GetOrigin()])
      run_loops_[url.GetOrigin()]->Quit();
  }

  void WaitForPreresolveCountForURL(const GURL url, int expected_count) {
    EXPECT_TRUE(base::Contains(preresolve_counts_, url.GetOrigin()));
    while (preresolve_counts_[url.GetOrigin()] < expected_count) {
      run_loops_[url.GetOrigin()] = std::make_unique<base::RunLoop>();
      run_loops_[url.GetOrigin()]->Run();
      run_loops_[url.GetOrigin()].reset();
    }
  }

  void WaitForDelay(base::TimeDelta delay) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), delay);
    run_loop.Run();
  }

 protected:
  std::map<GURL, int> preresolve_counts_;
  base::test::ScopedFeatureList feature_list_;

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::map<GURL, std::unique_ptr<base::RunLoop>> run_loops_;

  DISALLOW_COPY_AND_ASSIGN(SearchEnginePreconnectorBrowserTest);
};

class SearchEnginePreconnectorNoDelaysBrowserTest
    : public SearchEnginePreconnectorBrowserTest {
 public:
  SearchEnginePreconnectorNoDelaysBrowserTest() {
    {
      feature_list_.InitWithFeaturesAndParameters(
          {{features::kPreconnectToSearch, {{"startup_delay_ms", "1000000"}}},
           {features::kPreconnectToSearchNonGoogle, {{}}},
           {net::features::kNetUnusedIdleSocketTimeout,
            {{"unused_idle_socket_timeout_seconds", "0"}}}},
          {});
    }
  }

  ~SearchEnginePreconnectorNoDelaysBrowserTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchEnginePreconnectorNoDelaysBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SearchEnginePreconnectorNoDelaysBrowserTest,
                       PreconnectSearch) {
  // Put the fake search URL to be preconnected in foreground.
  NavigationPredictorKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser()->profile()))
      ->search_engine_preconnector()
      ->StartPreconnecting(/*with_startup_delay=*/false);
  // Verifies that the default search is preconnected.
  static const char kShortName[] = "test";
  static const char kSearchURL[] =
      "/anchors_different_area.html?q={searchTerms}";
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  // Check default URL is being preconnected and test URL is not.
  WaitForPreresolveCountForURL(GoogleSearch(), 2);
  EXPECT_EQ(2, preresolve_counts_[GoogleSearch().GetOrigin()]);
  EXPECT_EQ(0, preresolve_counts_[GetTestURL("/").GetOrigin()]);

  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16(kShortName));
  data.SetKeyword(data.short_name());
  data.SetURL(GetTestURL(kSearchURL).spec());

  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  // Put the fake search URL to be preconnected in foreground.
  NavigationPredictorKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser()->profile()))
      ->search_engine_preconnector()
      ->StartPreconnecting(/*with_startup_delay=*/false);

  // After switching search providers, the test URL should now start being
  // preconnected.
  WaitForPreresolveCountForURL(GetTestURL("/"), 2);
  // Preconnect should occur for DSE.
  EXPECT_EQ(2, preresolve_counts_[GetTestURL("/").GetOrigin()]);

  WaitForPreresolveCountForURL(GetTestURL("/"), 4);
  // Preconnect should occur again for DSE.
  EXPECT_EQ(4, preresolve_counts_[GetTestURL("/").GetOrigin()]);
}

IN_PROC_BROWSER_TEST_F(SearchEnginePreconnectorNoDelaysBrowserTest,
                       PreconnectOnlyInForeground) {
  static const char kShortName[] = "test";
  static const char kSearchURL[] =
      "/anchors_different_area.html?q={searchTerms}";
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16(kShortName));
  data.SetKeyword(data.short_name());
  data.SetURL(GetTestURL(kSearchURL).spec());

  // Set the DSE to the test URL.
  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  // Ensure that we wait long enough to trigger preconnects.
  WaitForDelay(base::TimeDelta::FromMilliseconds(200));

  TemplateURLData data_fake_search;
  data_fake_search.SetShortName(base::ASCIIToUTF16(kShortName));
  data_fake_search.SetKeyword(data.short_name());
  data_fake_search.SetURL(FakeSearch().spec());

  template_url = model->Add(std::make_unique<TemplateURL>(data_fake_search));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  // Put the fake search URL to be preconnected in foreground.
  NavigationPredictorKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser()->profile()))
      ->search_engine_preconnector()
      ->StartPreconnecting(/*with_startup_delay=*/false);
  WaitForPreresolveCountForURL(FakeSearch(), 2);

  // Preconnect should occur for fake search (2 since there are 2 NIKs).
  EXPECT_EQ(2, preresolve_counts_[FakeSearch()]);

  // No preconnects should have been issued for the test URL.
  EXPECT_EQ(0, preresolve_counts_[GetTestURL("/").GetOrigin()]);
}

class SearchEnginePreconnectorKeepSocketBrowserTest
    : public SearchEnginePreconnectorBrowserTest {
 public:
  SearchEnginePreconnectorKeepSocketBrowserTest() {
    {
      feature_list_.InitWithFeaturesAndParameters(
          {{features::kPreconnectToSearch, {{"startup_delay_ms", "1000000"}}},
           {features::kPreconnectToSearchNonGoogle, {{}}},
           {net::features::kNetUnusedIdleSocketTimeout,
            {{"unused_idle_socket_timeout_seconds", "60"}}}},
          {});
    }
  }

  ~SearchEnginePreconnectorKeepSocketBrowserTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchEnginePreconnectorKeepSocketBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SearchEnginePreconnectorKeepSocketBrowserTest,
                       SocketWarmForSearch) {
  // Verifies that a navigation to search will use a warm socket.
  static const char kShortName[] = "test";
  static const char kSearchURL[] =
      "/anchors_different_area.html?q={searchTerms}";
  static const char kSearchURLWithQuery[] =
      "/anchors_different_area.html?q=porgs";

  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16(kShortName));
  data.SetKeyword(data.short_name());
  data.SetURL(GetTestURL(kSearchURL).spec());

  // Set the DSE to the test URL.
  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  // Put the fake search URL to be preconnected in foreground.
  NavigationPredictorKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser()->profile()))
      ->search_engine_preconnector()
      ->StartPreconnecting(/*with_startup_delay=*/false);

  WaitForPreresolveCountForURL(GetTestURL(kSearchURL), 1);

  ui_test_utils::NavigateToURL(browser(), GetTestURL(kSearchURLWithQuery));

  auto ukm_recorder = std::make_unique<ukm::TestAutoSetUkmRecorder>();

  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  const auto& entries =
      ukm_recorder->GetMergedEntriesByName(ukm::builders::PageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());

  for (const auto& kv : entries) {
    EXPECT_TRUE(ukm_recorder->EntryHasMetric(
        kv.second.get(),
        ukm::builders::PageLoad::kMainFrameResource_SocketReusedName));
  }
}

class SearchEnginePreconnectorDesktopAutoStartBrowserTest
    : public SearchEnginePreconnectorBrowserTest {
 public:
  SearchEnginePreconnectorDesktopAutoStartBrowserTest() {
    {
      feature_list_.InitWithFeaturesAndParameters(
          {{features::kPreconnectToSearch, {{"startup_delay_ms", "0"}}},
           {net::features::kNetUnusedIdleSocketTimeout,
            {{"unused_idle_socket_timeout_seconds", "0"}}}},
          {});
    }
  }

  ~SearchEnginePreconnectorDesktopAutoStartBrowserTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchEnginePreconnectorDesktopAutoStartBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SearchEnginePreconnectorDesktopAutoStartBrowserTest,
                       AutoStartDesktop) {
  // Verifies that the default search is preconnected.
  WaitForPreresolveCountForURL(GoogleSearch(), 2);
}

class SearchEnginePreconnectorGoogleOnlyBrowserTest
    : public SearchEnginePreconnectorBrowserTest {
 public:
  SearchEnginePreconnectorGoogleOnlyBrowserTest() {
    {
      feature_list_.InitWithFeaturesAndParameters(
          {{features::kPreconnectToSearch, {{"startup_delay_ms", "1000000"}}},
           {net::features::kNetUnusedIdleSocketTimeout,
            {{"unused_idle_socket_timeout_seconds", "60"}}}},
          {
              {features::kPreconnectToSearchNonGoogle, {{}}},
          });
    }
  }

  ~SearchEnginePreconnectorGoogleOnlyBrowserTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchEnginePreconnectorGoogleOnlyBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SearchEnginePreconnectorGoogleOnlyBrowserTest,
                       GoogleOnly) {
  static const char kShortName[] = "test";
  static const char kSearchURL[] =
      "/anchors_different_area.html?q={searchTerms}";
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16(kShortName));
  data.SetKeyword(data.short_name());
  data.SetURL(GetTestURL(kSearchURL).spec());

  // Set the DSE to the test URL.
  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  NavigationPredictorKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser()->profile()))
      ->search_engine_preconnector()
      ->StartPreconnecting(/*with_startup_delay=*/false);

  TemplateURLData data_google_search;
  data_google_search.SetShortName(base::ASCIIToUTF16(kShortName));
  data_google_search.SetKeyword(data.short_name());
  data_google_search.SetURL(GoogleSearch().spec());

  template_url = model->Add(std::make_unique<TemplateURL>(data_google_search));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  NavigationPredictorKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser()->profile()))
      ->search_engine_preconnector()
      ->StartPreconnecting(/*with_startup_delay=*/false);

  WaitForPreresolveCountForURL(GoogleSearch(), 2);

  // Preconnect should occur for Google search (2 since there are 2 NIKs).
  EXPECT_EQ(2, preresolve_counts_[GoogleSearch()]);

  // No preconnects should have been issued for the test URL.
  EXPECT_EQ(0, preresolve_counts_[GetTestURL("/").GetOrigin()]);
}

}  // namespace
