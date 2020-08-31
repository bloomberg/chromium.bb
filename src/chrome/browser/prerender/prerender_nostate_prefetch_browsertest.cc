// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_test_utils.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/appcache_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/url_loader_monitor.h"
#include "net/base/escape.h"
#include "net/base/features.h"
#include "net/base/load_flags.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"

using prerender::test_utils::DestructionWaiter;
using prerender::test_utils::TestPrerender;

namespace {

const char kExpectedPurposeHeaderOnPrefetch[] = "Purpose";

std::string CreateServerRedirect(const std::string& dest_url) {
  const char* const kServerRedirectBase = "/server-redirect?";
  return kServerRedirectBase + net::EscapeQueryParamValue(dest_url, false);
}

}  // namespace

namespace prerender {

const char k302RedirectPage[] = "/prerender/302_redirect.html";
const char kPrefetchAppcache[] = "/prerender/prefetch_appcache.html";
const char kPrefetchAppcacheManifest[] = "/prerender/appcache.manifest";
const char kPrefetchCookiePage[] = "/prerender/cookie.html";
const char kPrefetchFromSubframe[] = "/prerender/prefetch_from_subframe.html";
const char kPrefetchImagePage[] = "/prerender/prefetch_image.html";
const char kPrefetchJpeg[] = "/prerender/image.jpeg";
const char kPrefetchLoaderPath[] = "/prerender/prefetch_loader.html";
const char kPrefetchLoopPage[] = "/prerender/prefetch_loop.html";
const char kPrefetchMetaCSP[] = "/prerender/prefetch_meta_csp.html";
const char kPrefetchNostorePage[] = "/prerender/prefetch_nostore_page.html";
const char kPrefetchPage[] = "/prerender/prefetch_page.html";
const char kPrefetchPage2[] = "/prerender/prefetch_page2.html";
const char kPrefetchPageBigger[] = "/prerender/prefetch_page_bigger.html";
const char kPrefetchPageMultipleResourceTypes[] =
    "/prerender/prefetch_page_multiple_resource_types.html";
const char kPrefetchPng[] = "/prerender/image.png";
const char kPrefetchPng2[] = "/prerender/image2.png";
const char kPrefetchPngRedirect[] = "/prerender/image-redirect.png";
const char kPrefetchRecursePage[] = "/prerender/prefetch_recurse.html";
const char kPrefetchResponseHeaderCSP[] =
    "/prerender/prefetch_response_csp.html";
const char kPrefetchScript[] = "/prerender/prefetch.js";
const char kPrefetchScript2[] = "/prerender/prefetch2.js";
const char kPrefetchCss[] = "/prerender/style.css";
const char kPrefetchFont[] = "/prerender/font.woff";
const char kPrefetchDownloadFile[] = "/download-test1.lib";
const char kPrefetchSubresourceRedirectPage[] =
    "/prerender/prefetch_subresource_redirect.html";
const char kServiceWorkerLoader[] = "/prerender/service_worker.html";

class NoStatePrefetchBrowserTest
    : public test_utils::PrerenderInProcessBrowserTest {
 public:
  NoStatePrefetchBrowserTest() {}

  void SetUpOnMainThread() override {
    test_utils::PrerenderInProcessBrowserTest::SetUpOnMainThread();
    PrerenderManager::SetMode(
        PrerenderManager::PRERENDER_MODE_NOSTATE_PREFETCH);
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void OverridePrerenderManagerTimeTicks() {
    // The default zero time causes the prerender manager to do strange things.
    clock_.Advance(base::TimeDelta::FromSeconds(1));
    GetPrerenderManager()->SetTickClockForTesting(&clock_);
  }

  // Block until an AppCache exists for |manifest_url|.
  void WaitForAppcache(const GURL& manifest_url) {
    bool found_manifest = false;
    content::AppCacheService* appcache_service =
        content::BrowserContext::GetDefaultStoragePartition(
            current_browser()->profile())
            ->GetAppCacheService();
    do {
      base::RunLoop wait_loop;
      WaitForAppcache(manifest_url, appcache_service, base::DoNothing(),
                      &found_manifest);
      // There seems to be some flakiness in the appcache getting back to us, so
      // use a timeout task to try the appcache query again.
      base::PostDelayedTask(FROM_HERE, {content::BrowserThread::UI},
                            wait_loop.QuitClosure(),
                            base::TimeDelta::FromMilliseconds(2000));
      wait_loop.Run();
    } while (!found_manifest);
  }

 protected:
  // Loads kPrefetchLoaderPath and specifies |target_url| as a query param. The
  // |loader_url| looks something like:
  // http://127.0.0.1:port_number/prerender/prefetch_loader.html?replace_text=\
  // UkVQTEFDRV9XSVRIX1BSRUZFVENIX1VSTA==:aHR0cDovL3d3dy52dlci5odG1s.
  // When the embedded test server receives the request, it uses the specified
  // query params to replace the "REPLACE_WITH_PREFETCH_URL" string in the HTML
  // response with |target_url|. See method UpdateReplacedText() from embedded
  // test server.
  std::unique_ptr<TestPrerender> PrefetchFromURL(
      const GURL& target_url,
      FinalStatus expected_final_status) {
    GURL loader_url = ServeLoaderURL(
        kPrefetchLoaderPath, "REPLACE_WITH_PREFETCH_URL", target_url, "");
    std::vector<FinalStatus> expected_final_status_queue(1,
                                                         expected_final_status);
    std::vector<std::unique_ptr<TestPrerender>> prerenders =
        NavigateWithPrerenders(loader_url, expected_final_status_queue);
    prerenders[0]->WaitForStop();
    return std::move(prerenders[0]);
  }

  std::unique_ptr<TestPrerender> PrefetchFromFile(
      const std::string& html_file,
      FinalStatus expected_final_status) {
    return PrefetchFromURL(src_server()->GetURL(html_file),
                           expected_final_status);
  }

  base::SimpleTestTickClock clock_;

 private:
  // Schedule a task to retrieve AppCacheInfo from |appcache_service|. This sets
  // |found_manifest| if an appcache exists for |manifest_url|. |callback| will
  // be called on the UI thread after the info is retrieved, whether or not the
  // manifest exists.
  static void WaitForAppcache(const GURL& manifest_url,
                              content::AppCacheService* appcache_service,
                              base::Closure callback,
                              bool* found_manifest) {
    scoped_refptr<content::AppCacheInfoCollection> info_collection =
        new content::AppCacheInfoCollection();
    appcache_service->GetAllAppCacheInfo(
        info_collection.get(),
        base::BindOnce(ProcessAppCacheInfo, manifest_url, callback,
                       found_manifest, info_collection));
  }

  // Look through |info_collection| for an entry matching |target_manifest|,
  // setting |found_manifest| appropriately. Then |callback| will be invoked on
  // the UI thread.
  static void ProcessAppCacheInfo(
      const GURL& target_manifest,
      base::Closure callback,
      bool* found_manifest,
      scoped_refptr<content::AppCacheInfoCollection> info_collection,
      int status) {
    if (status == net::OK) {
      for (const auto& origin_pair : info_collection->infos_by_origin) {
        for (const auto& info : origin_pair.second) {
          if (info.manifest_url == target_manifest) {
            *found_manifest = true;
            break;
          }
        }
      }
    }
    base::PostTask(FROM_HERE, {content::BrowserThread::UI}, callback);
  }

  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(NoStatePrefetchBrowserTest);
};

class NoStatePrefetchBrowserTestHttpCache
    : public NoStatePrefetchBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 protected:
  void SetUp() override {
    bool split_cache_by_network_isolation_key;
    bool append_frame_origin_to_network_isolation_key;
    std::tie(split_cache_by_network_isolation_key,
             append_frame_origin_to_network_isolation_key) = GetParam();

    std::vector<base::Feature> disabled_and_enabled_features[2];

    disabled_and_enabled_features[split_cache_by_network_isolation_key]
        .push_back(net::features::kSplitCacheByNetworkIsolationKey);
    disabled_and_enabled_features[append_frame_origin_to_network_isolation_key]
        .push_back(net::features::kAppendFrameOriginToNetworkIsolationKey);

    feature_list_.InitWithFeatures(disabled_and_enabled_features[true],
                                   disabled_and_enabled_features[false]);
    NoStatePrefetchBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

using NoStatePrefetchBrowserTestHttpCache_DefaultAndAppendFrameOrigin =
    NoStatePrefetchBrowserTestHttpCache;

// Test that the network isolation key is correctly populated during a prefetch,
// with feature kAppendFrameOriginToNetworkIsolationKey disabled and enabled
// respectively.
IN_PROC_BROWSER_TEST_P(
    NoStatePrefetchBrowserTestHttpCache_DefaultAndAppendFrameOrigin,
    PrefetchTwoCrossOriginFrames) {
  bool append_frame_origin_to_network_isolation_key;
  std::tie(std::ignore, append_frame_origin_to_network_isolation_key) =
      GetParam();

  GURL image_src =
      embedded_test_server()->GetURL("/prerender/cacheable_image.png");
  base::StringPairs replacement_text_img_src;
  replacement_text_img_src.push_back(
      std::make_pair("IMAGE_SRC", image_src.spec()));
  std::string iframe_path = net::test_server::GetFilePathWithReplacements(
      "/prerender/one_image.html", replacement_text_img_src);

  GURL iframe_src_1 = embedded_test_server()->GetURL("www.a.com", iframe_path);
  GURL iframe_src_2 = embedded_test_server()->GetURL("www.b.com", iframe_path);

  base::StringPairs replacement_text_iframe_src;
  replacement_text_iframe_src.push_back(
      std::make_pair("IFRAME_1_SRC", iframe_src_1.spec()));
  replacement_text_iframe_src.push_back(
      std::make_pair("IFRAME_2_SRC", iframe_src_2.spec()));
  std::string prerender_path = net::test_server::GetFilePathWithReplacements(
      "/prerender/two_iframes.html", replacement_text_iframe_src);
  ui_test_utils::NavigateToURL(current_browser(),
                               src_server()->GetURL(prerender_path));

  WaitForRequestCount(image_src,
                      append_frame_origin_to_network_isolation_key ? 2 : 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    NoStatePrefetchBrowserTestHttpCache_DefaultAndAppendFrameOrigin,
    ::testing::Combine(::testing::Values(true), ::testing::Bool()));

// Checks that a page is correctly prefetched in the case of a
// <link rel=prerender> tag and the JavaScript on the page is not executed.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchSimple) {
  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  // Verify that the page load did not happen.
  test_prerender->WaitForLoads(0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPage), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 0);
}

// Checks that prefetching is not stopped forever by aggressive background load
// limits.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchBigger) {
  std::unique_ptr<TestPrerender> test_prerender = PrefetchFromFile(
      kPrefetchPageBigger, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  WaitForRequestCount(src_server()->GetURL(kPrefetchPageBigger), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchJpeg), 1);
  // The |kPrefetchPng| is requested twice because the |kPrefetchPngRedirect|
  // redirects to it.
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng), 2);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng2), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPngRedirect), 1);
}

using NoStatePrefetchBrowserTestHttpCache_DefaultAndDoubleKeyedHttpCache =
    NoStatePrefetchBrowserTestHttpCache;

// Checks that a page load following a prefetch reuses preload-scanned resources
// and link rel 'prerender' main resource from cache without failing over to
// network.
IN_PROC_BROWSER_TEST_P(
    NoStatePrefetchBrowserTestHttpCache_DefaultAndDoubleKeyedHttpCache,
    LoadAfterPrefetch) {
  {
    std::unique_ptr<TestPrerender> test_prerender = PrefetchFromFile(
        kPrefetchPageBigger, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
    WaitForRequestCount(src_server()->GetURL(kPrefetchPageBigger), 1);
    WaitForRequestCount(src_server()->GetURL(kPrefetchJpeg), 1);
    WaitForRequestCount(src_server()->GetURL(kPrefetchPng2), 1);
  }
  ui_test_utils::NavigateToURL(current_browser(),
                               src_server()->GetURL(kPrefetchPageBigger));
  // Check that the request counts did not increase.
  WaitForRequestCount(src_server()->GetURL(kPrefetchPageBigger), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchJpeg), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng2), 1);
}

// Checks that a page load following a cross origin prefetch reuses
// preload-scanned resources and link rel 'prerender' main resource
// from cache without failing over to network.
IN_PROC_BROWSER_TEST_P(
    NoStatePrefetchBrowserTestHttpCache_DefaultAndDoubleKeyedHttpCache,
    LoadAfterPrefetchCrossOrigin) {
  static const std::string kSecondaryDomain = "www.foo.com";
  GURL cross_domain_url =
      embedded_test_server()->GetURL(kSecondaryDomain, kPrefetchPageBigger);

  PrefetchFromURL(cross_domain_url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPageBigger), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchJpeg), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng2), 1);

  ui_test_utils::NavigateToURL(current_browser(), cross_domain_url);
  // Check that the request counts did not increase.
  WaitForRequestCount(src_server()->GetURL(kPrefetchPageBigger), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchJpeg), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng2), 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    NoStatePrefetchBrowserTestHttpCache_DefaultAndDoubleKeyedHttpCache,
    ::testing::Combine(::testing::Bool(), ::testing::Values(false)));

// Checks that the expected resource types are fetched via NoState Prefetch.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchAllResourceTypes) {
  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kPrefetchPageMultipleResourceTypes,
                       FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  // Verify that the page load did not happen.
  test_prerender->WaitForLoads(0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPageMultipleResourceTypes),
                      1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchCss), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchFont), 1);
}

// Test and Test Class for lightweight prefetch under the HTML configuration.
class HTMLOnlyNoStatePrefetchBrowserTest : public NoStatePrefetchBrowserTest {
 public:
  void SetUp() override {
    std::map<std::string, std::string> parameters;
    parameters["html_only"] = "true";
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kLightweightNoStatePrefetch, parameters}}, {});
    NoStatePrefetchBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Checks that the expected resource types are fetched via NoState Prefetch.
IN_PROC_BROWSER_TEST_F(HTMLOnlyNoStatePrefetchBrowserTest, PrefetchHTMLOnly) {
  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kPrefetchPageMultipleResourceTypes,
                       FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  // Verify that the page load did not happen.
  test_prerender->WaitForLoads(0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPageMultipleResourceTypes),
                      1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchCss), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchFont), 0);
}

// Test and Test Class for lightweight prefetch under the HTML+CSS
// configuration.
class HTMLCSSNoStatePrefetchBrowserTest : public NoStatePrefetchBrowserTest {
 public:
  void SetUp() override {
    std::map<std::string, std::string> parameters;
    parameters["skip_script"] = "true";
    parameters["skip_other"] = "true";
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kLightweightNoStatePrefetch, parameters}}, {});
    NoStatePrefetchBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Checks that the expected resource types are fetched via NoState Prefetch.
IN_PROC_BROWSER_TEST_F(HTMLCSSNoStatePrefetchBrowserTest, PrefetchHTMLCSS) {
  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kPrefetchPageMultipleResourceTypes,
                       FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  // Verify that the page load did not happen.
  test_prerender->WaitForLoads(0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPageMultipleResourceTypes),
                      1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchCss), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchFont), 0);
}

// Test and Test Class for lightweight prefetch under the HTML+CSS+SyncScript
// configuration.
class HTMLCSSSyncScriptNoStatePrefetchBrowserTest
    : public NoStatePrefetchBrowserTest {
 public:
  void SetUp() override {
    std::map<std::string, std::string> parameters;
    parameters["skip_other"] = "true";
    parameters["skip_async_script"] = "true";
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kLightweightNoStatePrefetch, parameters}}, {});
    NoStatePrefetchBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Checks that the expected resource types are fetched via NoState Prefetch.
IN_PROC_BROWSER_TEST_F(HTMLCSSSyncScriptNoStatePrefetchBrowserTest,
                       PrefetchHTMLCSSSyncScript) {
  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kPrefetchPageMultipleResourceTypes,
                       FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  // Verify that the page load did not happen.
  test_prerender->WaitForLoads(0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPageMultipleResourceTypes),
                      1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchCss), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchFont), 0);
}

// Test and Test Class for lightweight prefetch under the
// HTML+CSS+SyncScript+Font configuration.
class HTMLCSSSyncScriptFontNoStatePrefetchBrowserTest
    : public NoStatePrefetchBrowserTest {
 public:
  void SetUp() override {
    std::map<std::string, std::string> parameters;
    parameters["skip_other"] = "true";
    parameters["skip_async_script"] = "true";
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kLightweightNoStatePrefetch, parameters}}, {});
    fonts_feature_list_.InitAndEnableFeature(
        blink::features::kLightweightNoStatePrefetch_FetchFonts);
    NoStatePrefetchBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedFeatureList fonts_feature_list_;
};

// Checks that the expected resource types are fetched via NoState Prefetch.
IN_PROC_BROWSER_TEST_F(HTMLCSSSyncScriptFontNoStatePrefetchBrowserTest,
                       PrefetchHTMLCSSSyncScript) {
  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kPrefetchPageMultipleResourceTypes,
                       FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  // Verify that the page load did not happen.
  test_prerender->WaitForLoads(0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPageMultipleResourceTypes),
                      1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchCss), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchFont), 1);
}

// Test and Test Class for lightweight prefetch under the HTML+CSS+Script
// configuration.
class HTMLCSSScriptNoStatePrefetchBrowserTest
    : public NoStatePrefetchBrowserTest {
 public:
  void SetUp() override {
    std::map<std::string, std::string> parameters;
    parameters["skip_other"] = "true";
    parameters["skip_async_script"] = "false";
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kLightweightNoStatePrefetch, parameters}}, {});
    NoStatePrefetchBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Checks that the expected resource types are fetched via NoState Prefetch.
IN_PROC_BROWSER_TEST_F(HTMLCSSScriptNoStatePrefetchBrowserTest,
                       PrefetchHTMLCSSScript) {
  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kPrefetchPageMultipleResourceTypes,
                       FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  // Verify that the page load did not happen.
  test_prerender->WaitForLoads(0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPageMultipleResourceTypes),
                      1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchCss), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchFont), 0);
}

void GetCookieCallback(base::RepeatingClosure callback,
                       const net::CookieStatusList& cookie_with_status_list,
                       const net::CookieStatusList& excluded_cookies) {
  bool found_chocolate = false;
  bool found_oatmeal = false;
  for (const auto& c : cookie_with_status_list) {
    if (c.cookie.Name() == "chocolate-chip") {
      EXPECT_EQ("the-best", c.cookie.Value());
      found_chocolate = true;
    }
    if (c.cookie.Name() == "oatmeal") {
      EXPECT_EQ("sublime", c.cookie.Value());
      found_oatmeal = true;
    }
  }
  CHECK(found_chocolate && found_oatmeal);
  callback.Run();
}

// Check cookie loading for prefetched pages.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchCookie) {
  GURL url = src_server()->GetURL(kPrefetchCookiePage);
  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromURL(url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  content::StoragePartition* storage_partition =
      content::BrowserContext::GetStoragePartitionForSite(
          current_browser()->profile(), url, false);
  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  base::RunLoop loop;
  storage_partition->GetCookieManagerForBrowserProcess()->GetCookieList(
      url, options, base::BindOnce(GetCookieCallback, loop.QuitClosure()));
  loop.Run();
}

// Check cookie loading for a cross-domain prefetched pages.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchCookieCrossDomain) {
  static const std::string secondary_domain = "www.foo.com";
  GURL cross_domain_url(base::StringPrintf(
      "http://%s:%d%s", secondary_domain.c_str(),
      embedded_test_server()->host_port_pair().port(), kPrefetchCookiePage));

  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromURL(cross_domain_url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  // While the request is cross-site, it's permitted to set (implicitly) lax
  // cookies on a cross-site navigation.
  content::StoragePartition* storage_partition =
      content::BrowserContext::GetStoragePartitionForSite(
          current_browser()->profile(), cross_domain_url, false);
  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  base::RunLoop loop;
  storage_partition->GetCookieManagerForBrowserProcess()->GetCookieList(
      cross_domain_url, options,
      base::BindOnce(GetCookieCallback, loop.QuitClosure()));
  loop.Run();
}

// Check cookie loading for a cross-domain prefetched pages.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       PrefetchCookieCrossDomainSameSiteStrict) {
  constexpr char kSecondaryDomain[] = "www.foo.com";
  GURL cross_domain_url =
      embedded_test_server()->GetURL(kSecondaryDomain, "/echoall");

  EXPECT_TRUE(SetCookie(current_browser()->profile(), cross_domain_url,
                        "cookie_A=A; SameSite=Strict;"));
  EXPECT_TRUE(SetCookie(current_browser()->profile(), cross_domain_url,
                        "cookie_B=B; SameSite=Lax;"));

  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromURL(cross_domain_url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  ui_test_utils::NavigateToURL(current_browser(), cross_domain_url);

  EXPECT_TRUE(WaitForLoadStop(
      current_browser()->tab_strip_model()->GetActiveWebContents()));

  std::string html_content;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      current_browser()->tab_strip_model()->GetActiveWebContents(),
      "domAutomationController.send(document.body.innerHTML)", &html_content));

  // For any cross origin navigation (including prerender), SameSite Strict
  // cookies should not be sent, but Lax should.
  EXPECT_EQ(std::string::npos, html_content.find("cookie_A=A"));
  EXPECT_NE(std::string::npos, html_content.find("cookie_B=B"));
}

// Check cookie loading for a same-domain prefetched pages.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       PrefetchCookieSameDomainSameSiteStrict) {
  GURL same_domain_url = embedded_test_server()->GetURL("/echoall");

  EXPECT_TRUE(SetCookie(current_browser()->profile(), same_domain_url,
                        "cookie_A=A; SameSite=Strict;"));
  EXPECT_TRUE(SetCookie(current_browser()->profile(), same_domain_url,
                        "cookie_B=B; SameSite=Lax;"));

  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromURL(same_domain_url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  ui_test_utils::NavigateToURL(current_browser(), same_domain_url);

  EXPECT_TRUE(WaitForLoadStop(
      current_browser()->tab_strip_model()->GetActiveWebContents()));

  std::string html_content;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      current_browser()->tab_strip_model()->GetActiveWebContents(),
      "domAutomationController.send(document.body.innerHTML)", &html_content));

  // For any same origin navigation (including prerender), SameSite Strict
  // cookies should not be sent, but Lax should.
  EXPECT_NE(std::string::npos, html_content.find("cookie_A=A"));
  EXPECT_NE(std::string::npos, html_content.find("cookie_B=B"));
}

// Check that the LOAD_PREFETCH flag is set.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchLoadFlag) {
  GURL prefetch_page = src_server()->GetURL(kPrefetchPage);
  GURL prefetch_script = src_server()->GetURL(kPrefetchScript);

  content::URLLoaderMonitor monitor({prefetch_page, prefetch_script});

  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(prefetch_page, 1);
  WaitForRequestCount(prefetch_script, 1);

  // Verify that the page load did not happen.
  test_prerender->WaitForLoads(0);
  monitor.WaitForUrls();

  base::Optional<network::ResourceRequest> page_request =
      monitor.GetRequestInfo(prefetch_page);
  EXPECT_TRUE(page_request->load_flags & net::LOAD_PREFETCH);
  base::Optional<network::ResourceRequest> script_request =
      monitor.GetRequestInfo(prefetch_script);
  EXPECT_TRUE(script_request->load_flags & net::LOAD_PREFETCH);
}

// Check that prefetched resources and subresources set the 'Purpose: prefetch'
// header.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PurposeHeaderIsSet) {
  GURL prefetch_page = src_server()->GetURL(kPrefetchPage);
  GURL prefetch_script = src_server()->GetURL(kPrefetchScript);

  content::URLLoaderMonitor monitor({prefetch_page, prefetch_script});

  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(prefetch_page, 1);
  WaitForRequestCount(prefetch_script, 1);
  monitor.WaitForUrls();
  for (const GURL& url : {prefetch_page, prefetch_script}) {
    base::Optional<network::ResourceRequest> request =
        monitor.GetRequestInfo(url);
    EXPECT_TRUE(request->load_flags & net::LOAD_PREFETCH);
    EXPECT_FALSE(request->headers.HasHeader(kExpectedPurposeHeaderOnPrefetch));
    EXPECT_TRUE(request->cors_exempt_headers.HasHeader(
        kExpectedPurposeHeaderOnPrefetch));
    std::string purpose_header;
    request->cors_exempt_headers.GetHeader(kExpectedPurposeHeaderOnPrefetch,
                                           &purpose_header);
    EXPECT_EQ("prefetch", purpose_header);
  }
}

// Check that on normal navigations the 'Purpose: prefetch' header is not set.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       PurposeHeaderNotSetWhenNotPrefetching) {
  GURL prefetch_page = src_server()->GetURL(kPrefetchPage);
  GURL prefetch_script = src_server()->GetURL(kPrefetchScript);
  GURL prefetch_script2 = src_server()->GetURL(kPrefetchScript2);

  content::URLLoaderMonitor monitor(
      {prefetch_page, prefetch_script, prefetch_script2});

  ui_test_utils::NavigateToURL(current_browser(), prefetch_page);
  WaitForRequestCount(prefetch_page, 1);
  WaitForRequestCount(prefetch_script, 1);
  WaitForRequestCount(prefetch_script2, 1);
  monitor.WaitForUrls();
  for (const GURL& url : {prefetch_page, prefetch_script, prefetch_script2}) {
    base::Optional<network::ResourceRequest> request =
        monitor.GetRequestInfo(url);
    EXPECT_FALSE(request->load_flags & net::LOAD_PREFETCH);
    EXPECT_FALSE(request->headers.HasHeader(kExpectedPurposeHeaderOnPrefetch));
    EXPECT_FALSE(request->cors_exempt_headers.HasHeader(
        kExpectedPurposeHeaderOnPrefetch));
  }
}

// Check that a prefetch followed by a load produces the appropriate
// histograms. Note that other histogram testing is done in
// browser/page_load_metrics, in particular, testing the combinations of
// Warm/Cold and Cacheable/NoCacheable.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchHistograms) {
  PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  histogram_tester().ExpectTotalCount(
      "Prerender.websame_PrefetchTTFCP.Warm.Cacheable.Visible", 0);

  test_utils::FirstContentfulPaintManagerWaiter* fcp_waiter =
      test_utils::FirstContentfulPaintManagerWaiter::Create(
          GetPrerenderManager());
  ui_test_utils::NavigateToURL(current_browser(),
                               src_server()->GetURL(kPrefetchPage));
  fcp_waiter->Wait();

  histogram_tester().ExpectTotalCount(
      "Prerender.websame_PrefetchTTFCP.Warm.Cacheable.Visible", 1);
  histogram_tester().ExpectTotalCount(
      "Prerender.websame_NoStatePrefetchResponseTypes", 2);
  histogram_tester().ExpectTotalCount("Prerender.websame_PrefetchAge", 1);
}

// Checks the prefetch of an img tag.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchImage) {
  GURL main_page_url = GetURLWithReplacement(
      kPrefetchImagePage, "REPLACE_WITH_IMAGE_URL", kPrefetchJpeg);
  PrefetchFromURL(main_page_url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchJpeg), 1);
}

// Checks that a cross-domain prefetching works correctly.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchCrossDomain) {
  static const std::string secondary_domain = "www.foo.com";
  GURL cross_domain_url(base::StringPrintf(
      "http://%s:%d%s", secondary_domain.c_str(),
      embedded_test_server()->host_port_pair().port(), kPrefetchPage));
  PrefetchFromURL(cross_domain_url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPage), 1);
}

// Checks that prefetching from a cross-domain subframe works correctly.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       PrefetchFromCrossDomainSubframe) {
  const std::string secondary_domain = "www.foo.com";

  GURL target_url(base::StringPrintf(
      "http://%s:%d%s", secondary_domain.c_str(),
      embedded_test_server()->host_port_pair().port(), kPrefetchPage));

  GURL inner_frame_url = ServeLoaderURLWithHostname(
      kPrefetchLoaderPath, "REPLACE_WITH_PREFETCH_URL", target_url, "",
      secondary_domain);

  GURL outer_frame_url = ServeLoaderURL(
      kPrefetchFromSubframe, "REPLACE_WITH_SUBFRAME_URL", inner_frame_url, "");

  std::vector<FinalStatus> expected_final_status_queue(
      1, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  std::vector<std::unique_ptr<TestPrerender>> prerenders =
      NavigateWithPrerenders(outer_frame_url, expected_final_status_queue);
  prerenders[0]->WaitForStop();

  WaitForRequestCount(src_server()->GetURL(kPrefetchPage), 1);
}

// Checks that response header CSP is respected.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, ResponseHeaderCSP) {
  static const std::string secondary_domain = "foo.bar";
  GURL second_script_url(std::string("http://foo.bar") + kPrefetchScript2);
  GURL prefetch_response_header_csp = GetURLWithReplacement(
      kPrefetchResponseHeaderCSP, "REPLACE_WITH_PORT",
      base::NumberToString(src_server()->host_port_pair().port()));

  PrefetchFromURL(prefetch_response_header_csp,
                  FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  // The second script is in the correct domain for CSP, but the first script is
  // not.
  WaitForRequestCount(prefetch_response_header_csp, 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 0);
}

// Checks that CSP in the meta tag cancels the prefetch.
// TODO(mattcary): probably this behavior should be consistent with
// response-header CSP. See crbug/656581.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, MetaTagCSP) {
  static const std::string secondary_domain = "foo.bar";
  GURL second_script_url(std::string("http://foo.bar") + kPrefetchScript2);
  GURL prefetch_meta_tag_csp = GetURLWithReplacement(
      kPrefetchMetaCSP, "REPLACE_WITH_PORT",
      base::NumberToString(src_server()->host_port_pair().port()));

  PrefetchFromURL(prefetch_meta_tag_csp,
                  FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  // TODO(mattcary): See test comment above. If the meta CSP tag were parsed,
  // |second_script| would be loaded. Instead as the background scanner bails as
  // soon as the meta CSP tag is seen, only |main_page| is fetched.
  WaitForRequestCount(prefetch_meta_tag_csp, 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 0);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 0);
}

// Checks that the second prefetch request succeeds. This test waits for
// Prerender Stop before starting the second request.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchMultipleRequest) {
  PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  PrefetchFromFile(kPrefetchPage2, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPage), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPage2), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 1);
}

// Checks that a second prefetch request, started before the first stops,
// succeeds.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchSimultaneous) {
  GURL first_url = src_server()->GetURL("/hung");

  // Start the first prefetch directly instead of via PrefetchFromFile for the
  // first prefetch to avoid the wait on prerender stop.
  GURL first_loader_url = ServeLoaderURL(
      kPrefetchLoaderPath, "REPLACE_WITH_PREFETCH_URL", first_url, "");
  std::vector<FinalStatus> first_expected_status_queue(1,
                                                       FINAL_STATUS_CANCELLED);
  NavigateWithPrerenders(first_loader_url, first_expected_status_queue);

  PrefetchFromFile(kPrefetchPage2, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPage2), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 1);
}

// Checks that a prefetch does not recursively prefetch.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, NoPrefetchRecursive) {
  // A prefetch of a page with a prefetch of the image page should not load the
  // image page.
  PrefetchFromFile(kPrefetchRecursePage,
                   FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchRecursePage), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchNostorePage), 0);

  // When the first page is loaded, the image page should be prefetched. The
  // test may finish before the prerender is torn down, so
  // IgnorePrerenderContents() is called to skip the final status check.
  prerender_contents_factory()->IgnorePrerenderContents();
  ui_test_utils::NavigateToURL(current_browser(),
                               src_server()->GetURL(kPrefetchRecursePage));
  WaitForRequestCount(src_server()->GetURL(kPrefetchNostorePage), 1);
}

// Checks a prefetch to a nonexisting page.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchNonexisting) {
  std::unique_ptr<TestPrerender> test_prerender = PrefetchFromFile(
      "/nonexisting-page.html", FINAL_STATUS_UNSUPPORTED_SCHEME);
}

// Checks that a 301 redirect is followed.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Prefetch301Redirect) {
  PrefetchFromFile(CreateServerRedirect(kPrefetchPage),
                   FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
}

// Checks that non-HTTP(S) main resource redirects are marked as unsupported
// scheme.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       PrefetchRedirectUnsupportedScheme) {
  PrefetchFromFile(
      CreateServerRedirect("invalidscheme://www.google.com/test.html"),
      FINAL_STATUS_UNSUPPORTED_SCHEME);
}

// Checks that a 302 redirect is followed.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Prefetch302Redirect) {
  PrefetchFromFile(k302RedirectPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
}

// Checks that the load flags are set correctly for all resources in a 301
// redirect chain.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Prefetch301LoadFlags) {
  std::string redirect_path = CreateServerRedirect(kPrefetchPage);
  GURL redirect_url = src_server()->GetURL(redirect_path);
  GURL page_url = src_server()->GetURL(kPrefetchPage);
  content::URLLoaderMonitor monitor({redirect_url});

  PrefetchFromFile(redirect_path, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(redirect_url, 1);
  WaitForRequestCount(page_url, 1);
  monitor.WaitForUrls();

  base::Optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(redirect_url);
  EXPECT_TRUE(request->load_flags & net::LOAD_PREFETCH);
}

// Checks that a subresource 301 redirect is followed.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Prefetch301Subresource) {
  PrefetchFromFile(kPrefetchSubresourceRedirectPage,
                   FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
}

// Checks a client redirect is not followed.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchClientRedirect) {
  PrefetchFromFile(
      "/client-redirect/?" + net::EscapeQueryParamValue(kPrefetchPage, false),
      FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  ui_test_utils::NavigateToURL(current_browser(),
                               src_server()->GetURL(kPrefetchPage2));
  // A complete load of kPrefetchPage2 is used as a sentinel. Otherwise the test
  // ends before script_counter would reliably see the load of kPrefetchScript,
  // were it to happen.
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 0);
}

// Prefetches a page that contains an automatic download triggered through an
// iframe. The request to download should not reach the server.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchDownloadIframe) {
  PrefetchFromFile("/prerender/prerender_download_iframe.html",
                   FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  // A complete load of kPrefetchPage2 is used as a sentinel as in test
  // |PrefetchClientRedirect| above.
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchDownloadFile), 0);
}

IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       PrefetchDownloadViaClientRedirect) {
  PrefetchFromFile("/prerender/prerender_download_refresh.html",
                   FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  // A complete load of kPrefetchPage2 is used as a sentinel as in test
  // |PrefetchClientRedirect| above.
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript2), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchDownloadFile), 0);
}

// Checks that a prefetch of a CRX will result in a cancellation due to
// download.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchCrx) {
  PrefetchFromFile("/prerender/extension.crx", FINAL_STATUS_DOWNLOAD);
}

IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchHttps) {
  UseHttpsSrcServer();
  PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPage), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
}

// Checks that an SSL error prevents prefetch.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, SSLError) {
  // Only send the loaded page, not the loader, through SSL.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());
  std::unique_ptr<TestPrerender> prerender = PrefetchFromURL(
      https_server.GetURL(kPrefetchPage), FINAL_STATUS_SSL_ERROR);
  DestructionWaiter waiter(prerender->contents(), FINAL_STATUS_SSL_ERROR);
  EXPECT_TRUE(waiter.WaitForDestroy());
}

// Checks that a subresource failing SSL does not prevent prefetch on the rest
// of the page.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, SSLSubresourceError) {
  // First confirm that the image loads as expected.

  // A separate HTTPS server is started for the subresource; src_server() is
  // non-HTTPS.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("/prerender/image.jpeg");
  GURL main_page_url = GetURLWithReplacement(
      kPrefetchImagePage, "REPLACE_WITH_IMAGE_URL", https_url.spec());

  std::unique_ptr<TestPrerender> prerender =
      PrefetchFromURL(main_page_url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  // Checks that the presumed failure of the image load didn't affect the script
  // fetch. This assumes waiting for the script load is enough to see any error
  // from the image load.
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
}

IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Loop) {
  std::unique_ptr<TestPrerender> test_prerender = PrefetchFromFile(
      kPrefetchLoopPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchLoopPage), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
}

// Flaky timeouts on Win7 Tests; see https://crbug.com/985255.
#if defined(OS_WIN)
#define MAYBE_RendererCrash DISABLED_RendererCrash
#else
#define MAYBE_RendererCrash RendererCrash
#endif
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, MAYBE_RendererCrash) {
  // Navigate to about:blank to get the session storage namespace.
  ui_test_utils::NavigateToURL(current_browser(), GURL(url::kAboutBlankURL));
  content::SessionStorageNamespace* storage_namespace =
      GetActiveWebContents()
          ->GetController()
          .GetDefaultSessionStorageNamespace();

  // Navigate to about:crash without an intermediate loader because chrome://
  // URLs are ignored in renderers, and the test server has no support for them.
  const gfx::Size kSize(640, 480);
  std::unique_ptr<TestPrerender> test_prerender =
      prerender_contents_factory()->ExpectPrerenderContents(
          FINAL_STATUS_RENDERER_CRASHED);
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
  std::unique_ptr<PrerenderHandle> prerender_handle(
      GetPrerenderManager()->AddPrerenderFromExternalRequest(
          GURL(content::kChromeUICrashURL), content::Referrer(),
          storage_namespace, gfx::Rect(kSize)));
  ASSERT_EQ(prerender_handle->contents(), test_prerender->contents());
  test_prerender->WaitForStop();
}

// Checks that the prefetch of png correctly loads the png.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Png) {
  PrefetchFromFile(kPrefetchPng, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng), 1);
}

// Checks that the prefetch of jpeg correctly loads the jpeg.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Jpeg) {
  PrefetchFromFile(kPrefetchJpeg, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchJpeg), 1);
}

// If the main resource is unsafe, the whole prefetch is cancelled.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       PrerenderSafeBrowsingTopLevel) {
  GURL url = src_server()->GetURL(kPrefetchPage);
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      url, safe_browsing::SB_THREAT_TYPE_URL_MALWARE);

  std::unique_ptr<TestPrerender> prerender =
      PrefetchFromFile(kPrefetchPage, FINAL_STATUS_SAFE_BROWSING);

  // The frame request may have been started, but SafeBrowsing must have already
  // blocked it. Verify that the page load did not happen.
  prerender->WaitForLoads(0);

  // The frame resource has been blocked by SafeBrowsing, the subresource on
  // the page shouldn't be requested at all.
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 0);
}

// If a subresource is unsafe, the corresponding request is cancelled.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       PrerenderSafeBrowsingSubresource) {
  GURL url = src_server()->GetURL(kPrefetchScript);
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      url, safe_browsing::SB_THREAT_TYPE_URL_MALWARE);

  constexpr char kPrefetchCanceledHistogram[] =
      "SB2Test.ResourceTypes2.UnsafePrefetchCanceled";

  base::RunLoop run_loop;
  bool prefetch_canceled_histogram_added = false;
  EXPECT_TRUE(base::StatisticsRecorder::SetCallback(
      kPrefetchCanceledHistogram,
      base::Bind(
          [](const base::Closure& quit_closure, bool* called,
             base::HistogramBase::Sample sample) {
            *called = true;
            quit_closure.Run();
          },
          run_loop.QuitClosure(), &prefetch_canceled_histogram_added)));

  std::unique_ptr<TestPrerender> prerender =
      PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  // The frame resource was loaded.
  WaitForRequestCount(src_server()->GetURL(kPrefetchPage), 1);

  // There should be a histogram sample recorded for SafeBrowsing canceling an
  // unsafe prefetch, which corresponded to the subresource.
  run_loop.Run();
  EXPECT_TRUE(prefetch_canceled_histogram_added);

  base::StatisticsRecorder::ClearCallback(kPrefetchCanceledHistogram);
}

// Checks that prefetching a page does not add it to browsing history.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, HistoryUntouchedByPrefetch) {
  // Initialize.
  Profile* profile = current_browser()->profile();
  ASSERT_TRUE(profile);
  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS));

  // Prefetch a page.
  GURL prefetched_url = src_server()->GetURL(kPrefetchPage);
  PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForHistoryBackendToRun(profile);

  // Navigate to another page.
  GURL navigated_url = src_server()->GetURL(kPrefetchPage2);
  ui_test_utils::NavigateToURL(current_browser(), navigated_url);
  WaitForHistoryBackendToRun(profile);

  // Check that the URL that was explicitly navigated to is already in history.
  ui_test_utils::HistoryEnumerator enumerator(profile);
  std::vector<GURL>& urls = enumerator.urls();
  EXPECT_TRUE(base::Contains(urls, navigated_url));

  // Check that the URL that was prefetched is not in history.
  EXPECT_FALSE(base::Contains(urls, prefetched_url));

  // The loader URL is the remaining entry.
  EXPECT_EQ(2U, urls.size());
}

// Checks that prefetch requests have net::IDLE priority.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, IssuesIdlePriorityRequests) {
  // TODO(pasko): Figure out how to test that while a prefetched URL is in IDLE
  // priority state, a high-priority request with the same URL from a foreground
  // navigation hits the server.
  GURL script_url = src_server()->GetURL(kPrefetchScript);
  content::URLLoaderMonitor monitor({script_url});

  PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(script_url, 1);
  monitor.WaitForUrls();

#if defined(OS_ANDROID)
  // On Android requests from prerenders do not get downgraded
  // priority. See: https://crbug.com/652746.
  constexpr net::RequestPriority kExpectedPriority = net::HIGHEST;
#else
  constexpr net::RequestPriority kExpectedPriority = net::IDLE;
#endif
  base::Optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(script_url);
  EXPECT_EQ(kExpectedPriority, request->priority);
}

// Checks that a registered ServiceWorker (SW) that is not currently running
// will intercepts a prefetch request.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, ServiceWorkerIntercept) {
  // Register and launch a SW.
  base::string16 expected_title = base::ASCIIToUTF16("SW READY");
  content::TitleWatcher title_watcher(GetActiveWebContents(), expected_title);
  ui_test_utils::NavigateToURL(current_browser(),
                               src_server()->GetURL(kServiceWorkerLoader));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Stop any SW, killing the render process in order to test that the
  // lightweight renderer created for NoState prefetch does not interfere with
  // SW startup.
  int host_count = 0;
  for (content::RenderProcessHost::iterator iter(
           content::RenderProcessHost::AllHostsIterator());
       !iter.IsAtEnd(); iter.Advance()) {
    // Don't count spare RenderProcessHosts.
    if (!iter.GetCurrentValue()->HostHasNotBeenUsed())
      ++host_count;

    content::RenderProcessHostWatcher process_exit_observer(
        iter.GetCurrentValue(),
        content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    // TODO(wez): This used to use wait=true.
    iter.GetCurrentValue()->Shutdown(content::RESULT_CODE_KILLED);
    process_exit_observer.Wait();
  }
  // There should be at most one render_process_host, that created for the SW.
  EXPECT_EQ(1, host_count);

  // Open a new tab to replace the one closed with all the RenderProcessHosts.
  ui_test_utils::NavigateToURLWithDisposition(
      current_browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // The SW intercepts kPrefetchPage and replaces it with a body that contains
  // an <img> tage for kPrefetchPng. This verifies that the SW ran correctly by
  // observing the fetch of the image.
  PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng), 1);
}

// Checks that prefetching happens if an appcache is mentioned in the html tag
// but is uninitialized.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, AppCacheHtmlUninitialized) {
  PrefetchFromFile(kPrefetchAppcache, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForRequestCount(src_server()->GetURL(kPrefetchPng), 1);
}

// Checks that prefetching does not if an initialized appcache is mentioned in
// the html tag.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, AppCacheHtmlInitialized) {
  base::TimeTicks current_time = GetPrerenderManager()->GetCurrentTimeTicks();
  OverridePrerenderManagerTimeTicks();
  // Some navigations have already occurred in test setup. In order to track
  // duplicate prefetches correctly the test clock needs to be beyond those
  // navigations.
  clock_.SetNowTicks(current_time);
  clock_.Advance(base::TimeDelta::FromSeconds(600));

  // Fill manifest with the image url. The main resource will be cached
  // implicitly.
  GURL image_url = src_server()->GetURL(kPrefetchPng);
  GURL manifest_url = GetURLWithReplacement(
      kPrefetchAppcacheManifest, "REPLACE_WITH_URL", image_url.spec());
  GURL appcache_page_url = GetURLWithReplacement(
      kPrefetchAppcache, "REPLACE_WITH_MANIFEST", manifest_url.spec());

  // Load the page into the appcache.
  ui_test_utils::NavigateToURL(current_browser(), appcache_page_url);

  WaitForAppcache(manifest_url);

  // If a page is prefetch shortly after being loading, the prefetch is
  // canceled. Advancing the clock prevents the cancelation.
  clock_.Advance(base::TimeDelta::FromSeconds(6000));

  // While the prefetch stops when it sees the AppCache manifest, from the point
  // of view of the prerender manager the prefetch stops normally.
  PrefetchFromURL(appcache_page_url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  // The prefetch should have been canceled before the script in
  // kPrefetchAppcache is loaded (note the script is not mentioned in the
  // manifest).
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
}

// If a page has been cached by another AppCache, the prefetch should be
// canceled.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, AppCacheRegistered) {
  base::TimeTicks current_time = GetPrerenderManager()->GetCurrentTimeTicks();
  OverridePrerenderManagerTimeTicks();
  // Some navigations have already occurred in test setup. In order to track
  // duplicate prefetches correctly the test clock needs to be beyond those
  // navigations.
  clock_.SetNowTicks(current_time);
  clock_.Advance(base::TimeDelta::FromSeconds(600));

  // Fill manifest with kPrefetchPage so that it is cached without explicitly
  // listing a manifest.
  GURL prefetch_page_url = src_server()->GetURL(kPrefetchPage);
  GURL manifest_url = GetURLWithReplacement(
      kPrefetchAppcacheManifest, "REPLACE_WITH_URL", prefetch_page_url.spec());

  GURL appcache_page_url = GetURLWithReplacement(
      kPrefetchAppcache, "REPLACE_WITH_MANIFEST", manifest_url.spec());

  // Load the page into the appcache, and then the prefetch page so it can be
  // cached. After each navigation, wait for the appcache to catch up. This
  // avoids timeouts which for an unknown reason occur if the Appcache is
  // queried only after both navitations.
  ui_test_utils::NavigateToURL(current_browser(), appcache_page_url);
  WaitForAppcache(manifest_url);
  ui_test_utils::NavigateToURL(current_browser(), prefetch_page_url);
  WaitForAppcache(manifest_url);

  // If a page is prefetch shortly after being loading, the prefetch is
  // canceled. Advancing the clock prevents the cancelation.
  clock_.Advance(base::TimeDelta::FromSeconds(6000));

  PrefetchFromURL(prefetch_page_url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  // Neither the page nor the script should be prefetched.
  WaitForRequestCount(src_server()->GetURL(kPrefetchPage), 1);
  WaitForRequestCount(src_server()->GetURL(kPrefetchScript), 1);
}
}  // namespace prerender
