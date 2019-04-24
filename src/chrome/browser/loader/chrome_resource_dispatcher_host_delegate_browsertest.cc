// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/chrome_resource_dispatcher_host_delegate.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_command_line.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/download/download_browsertest.h"
#include "chrome/browser/loader/chrome_navigation_data.h"
#include "chrome/browser/policy/cloud/policy_header_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/scoped_account_consistency.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/google/core/common/google_util.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/policy_header_service.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_loader_throttle.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "components/signin/core/browser/signin_header_helper.h"
#endif

using content::ResourceType;

namespace {

std::unique_ptr<net::test_server::HttpResponse> HandleTestRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url == "/") {
    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("Success");
    return std::move(http_response);
  }
  return nullptr;
}

class TestDispatcherHostDelegate : public ChromeResourceDispatcherHostDelegate {
 public:
  TestDispatcherHostDelegate() : should_add_data_reduction_proxy_data_(false) {}
  ~TestDispatcherHostDelegate() override {}

  // ResourceDispatcherHostDelegate implementation:
  content::NavigationData* GetNavigationData(
      net::URLRequest* request) const override {
    if (request && should_add_data_reduction_proxy_data_) {
      data_reduction_proxy::DataReductionProxyData* data =
          data_reduction_proxy::DataReductionProxyData::
              GetDataAndCreateIfNecessary(request);
      data->set_used_data_reduction_proxy(true);
    }
    return ChromeResourceDispatcherHostDelegate::GetNavigationData(request);
  }

  // ChromeResourceDispatcherHost implementation:
  void AppendStandardResourceThrottles(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      ResourceType resource_type,
      std::vector<std::unique_ptr<content::ResourceThrottle>>* throttles)
      override {
    ++times_stardard_throttles_added_for_url_[request->url()];
    ChromeResourceDispatcherHostDelegate::AppendStandardResourceThrottles(
        request, resource_context, resource_type, throttles);
  }

  void set_should_add_data_reduction_proxy_data(
      bool should_add_data_reduction_proxy_data) {
    should_add_data_reduction_proxy_data_ =
        should_add_data_reduction_proxy_data;
  }

  // Writes the number of times the standard set of throttles have been added
  // for requests for the speficied URL to |count|.
  void GetTimesStandardThrottlesAddedForURL(const GURL& url, int* count) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    *count = times_stardard_throttles_added_for_url_[url];
  }

 private:
  bool should_add_data_reduction_proxy_data_;

  std::map<GURL, int> times_stardard_throttles_added_for_url_;

  DISALLOW_COPY_AND_ASSIGN(TestDispatcherHostDelegate);
};

// Helper class to track DidFinishNavigation and verify that NavigationData is
// added to NavigationHandle and pause/resume execution of the test.
class DidFinishNavigationObserver : public content::WebContentsObserver {
 public:
  DidFinishNavigationObserver(content::WebContents* web_contents,
                                       bool add_data_reduction_proxy_data)
      : content::WebContentsObserver(web_contents),
        add_data_reduction_proxy_data_(add_data_reduction_proxy_data) {}
  ~DidFinishNavigationObserver() override {}

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    ChromeNavigationData* data = static_cast<ChromeNavigationData*>(
        navigation_handle->GetNavigationData());
    if (add_data_reduction_proxy_data_) {
      EXPECT_TRUE(data->GetDataReductionProxyData());
      EXPECT_TRUE(
          data->GetDataReductionProxyData()->used_data_reduction_proxy());
    } else {
      EXPECT_FALSE(data->GetDataReductionProxyData());
    }
  }

 private:
  bool add_data_reduction_proxy_data_;
  DISALLOW_COPY_AND_ASSIGN(DidFinishNavigationObserver);
};

}  // namespace

class ChromeResourceDispatcherHostDelegateBrowserTest :
    public InProcessBrowserTest {
 public:
  ChromeResourceDispatcherHostDelegateBrowserTest() {}

  void SetUpOnMainThread() override {
    // Hook navigations with our delegate.
    dispatcher_host_delegate_.reset(new TestDispatcherHostDelegate);
    content::ResourceDispatcherHost::Get()->SetDelegate(
        dispatcher_host_delegate_.get());

    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&HandleTestRequest));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    content::ResourceDispatcherHost::Get()->SetDelegate(NULL);
    dispatcher_host_delegate_.reset();
  }

  void SetShouldAddDataReductionProxyData(bool add_data) {
    dispatcher_host_delegate_->set_should_add_data_reduction_proxy_data(
        add_data);
  }

  int GetTimesStandardThrottlesAddedForURL(const GURL& url) {
    int count;
    base::RunLoop run_loop;
    base::PostTaskWithTraitsAndReply(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(
            &TestDispatcherHostDelegate::GetTimesStandardThrottlesAddedForURL,
            base::Unretained(dispatcher_host_delegate_.get()), url, &count),
        run_loop.QuitClosure());
    run_loop.Run();
    return count;
  }

 protected:
  std::unique_ptr<TestDispatcherHostDelegate> dispatcher_host_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeResourceDispatcherHostDelegateBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ChromeResourceDispatcherHostDelegateBrowserTest,
                       NavigationDataProcessed) {
  // The network service code path doesn't go through ResourceDispatcherHost.
  if (base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;
  // Servicified service worker doesn't set NavigationData.
  if (blink::ServiceWorkerUtils::IsServicificationEnabled())
    return;

  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->base_url());
  {
    DidFinishNavigationObserver nav_observer(
        browser()->tab_strip_model()->GetActiveWebContents(), false);
    ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/google/google.html"));
  }
  SetShouldAddDataReductionProxyData(true);
  {
    DidFinishNavigationObserver nav_observer(
        browser()->tab_strip_model()->GetActiveWebContents(), true);
    ui_test_utils::NavigateToURL(browser(), embedded_test_server()->base_url());
  }
}

// Mirror is not supported on Dice platforms.
#if !BUILDFLAG(ENABLE_DICE_SUPPORT)

// A delegate to insert a user generated X-Chrome-Connected header
// to a specifict URL.
class HeaderModifyingThrottle : public content::URLLoaderThrottle {
 public:
  HeaderModifyingThrottle() = default;
  ~HeaderModifyingThrottle() override = default;

  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override {
    request->headers.SetHeader(signin::kChromeConnectedHeader, "User Data");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HeaderModifyingThrottle);
};

class ThrottleContentBrowserClient : public ChromeContentBrowserClient {
 public:
  explicit ThrottleContentBrowserClient(const GURL& watch_url)
      : watch_url_(watch_url) {}
  ~ThrottleContentBrowserClient() override = default;

  // ContentBrowserClient overrides:
  std::vector<std::unique_ptr<content::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      content::ResourceContext* resource_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id) override {
    std::vector<std::unique_ptr<content::URLLoaderThrottle>> throttles;
    if (request.url == watch_url_)
      throttles.push_back(std::make_unique<HeaderModifyingThrottle>());
    return throttles;
  }

 private:
  const GURL watch_url_;

  DISALLOW_COPY_AND_ASSIGN(ThrottleContentBrowserClient);
};

// Subclass of ChromeResourceDispatcherHostDelegateBrowserTest with Mirror
// enabled.
class ChromeResourceDispatcherHostDelegateMirrorBrowserTest
    : public ChromeResourceDispatcherHostDelegateBrowserTest {
 private:
  void SetUpOnMainThread() override {
    // The test makes requests to google.com and other domains which we want to
    // redirect to the test server.
    host_resolver()->AddRule("*", "127.0.0.1");

    // The production code only allows known ports (80 for http and 443 for
    // https), but the test server runs on a random port.
    google_util::IgnorePortNumbersForGoogleURLChecksForTesting();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed to
    // load pages from "www.google.com" without an interstitial.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  ScopedAccountConsistencyMirror scoped_mirror_;
};

// Verify the following items:
// 1- X-Chrome-Connected is appended on Google domains if account
//    consistency is enabled and access is secure.
// 2- The header is stripped in case a request is redirected from a Gooogle
//    domain to non-google domain.
// 3- The header is NOT stripped in case it is added directly by the page
//    and not because it was on a secure Google domain.
// This is a regression test for crbug.com/588492.
IN_PROC_BROWSER_TEST_F(ChromeResourceDispatcherHostDelegateMirrorBrowserTest,
                       MirrorRequestHeader) {
  browser()->profile()->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                              "user@gmail.com");
  browser()->profile()->GetPrefs()->SetString(
      prefs::kGoogleServicesUserAccountId, "account_id");

  base::Lock lock;
  // Map from the path of the URLs that test server sees to the request header.
  // This is the path, and not URL, because the requests use different domains
  // which the mock HostResolver converts to 127.0.0.1.
  std::map<std::string, net::test_server::HttpRequest::HeaderMap> header_map;
  embedded_test_server()->RegisterRequestMonitor(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request) {
        base::AutoLock auto_lock(lock);
        header_map[request.GetURL().path()] = request.headers;
      }));
  ASSERT_TRUE(embedded_test_server()->Start());

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  https_server.RegisterRequestMonitor(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request) {
        base::AutoLock auto_lock(lock);
        header_map[request.GetURL().path()] = request.headers;
      }));
  ASSERT_TRUE(https_server.Start());

  base::FilePath root_http;
  base::PathService::Get(chrome::DIR_TEST_DATA, &root_http);
  root_http = root_http.AppendASCII("mirror_request_header");

  struct TestCase {
    GURL original_url;       // The URL from which the request begins.
    // The path to which navigation is redirected.
    std::string redirected_to_path;
    bool inject_header;  // Should X-Chrome-Connected header be injected to the
                         // original request.
    bool original_url_expects_header;       // Expectation: The header should be
                                            // visible in original URL.
    bool redirected_to_url_expects_header;  // Expectation: The header should be
                                            // visible in redirected URL.
  };

  std::vector<TestCase> all_tests;

  // Neither should have the header.
  // Note we need to replace the port of the redirect's URL.
  base::StringPairs replacement_text;
  replacement_text.push_back(std::make_pair(
      "{{PORT}}", base::NumberToString(embedded_test_server()->port())));
  std::string replacement_path = net::test_server::GetFilePathWithReplacements(
      "/mirror_request_header/http.www.google.com.html", replacement_text);
  all_tests.push_back(
      {embedded_test_server()->GetURL("www.google.com", replacement_path),
       "/simple.html", false, false, false});

  // First one adds the header and transfers it to the second.
  replacement_path = net::test_server::GetFilePathWithReplacements(
      "/mirror_request_header/http.www.header_adder.com.html",
      replacement_text);
  all_tests.push_back(
      {embedded_test_server()->GetURL("www.header_adder.com", replacement_path),
       "/simple.html", true, true, true});

  // First one should have the header, but not transfered to second one.
  replacement_text.clear();
  replacement_text.push_back(
      std::make_pair("{{PORT}}", base::NumberToString(https_server.port())));
  replacement_path = net::test_server::GetFilePathWithReplacements(
      "/mirror_request_header/https.www.google.com.html", replacement_text);
  all_tests.push_back({https_server.GetURL("www.google.com", replacement_path),
                       "/simple.html", false, true, false});

  for (const auto& test_case : all_tests) {
    SCOPED_TRACE(test_case.original_url);

    // If test case requires adding header for the first url add a throttle.
    ThrottleContentBrowserClient browser_client(test_case.original_url);
    content::ContentBrowserClient* old_browser_client = nullptr;
    if (test_case.inject_header)
      old_browser_client = content::SetBrowserClientForTesting(&browser_client);

    // Navigate to first url.
    ui_test_utils::NavigateToURL(browser(), test_case.original_url);

    if (test_case.inject_header)
      content::SetBrowserClientForTesting(old_browser_client);

    base::AutoLock auto_lock(lock);

    // Check if header exists and X-Chrome-Connected is correctly provided.
    ASSERT_EQ(1u, header_map.count(test_case.original_url.path()));
    if (test_case.original_url_expects_header) {
      ASSERT_TRUE(!!header_map[test_case.original_url.path()].count(
          signin::kChromeConnectedHeader));
    } else {
      ASSERT_FALSE(!!header_map[test_case.original_url.path()].count(
          signin::kChromeConnectedHeader));
    }

    ASSERT_EQ(1u, header_map.count(test_case.redirected_to_path));
    if (test_case.redirected_to_url_expects_header) {
      ASSERT_TRUE(!!header_map[test_case.redirected_to_path].count(
          signin::kChromeConnectedHeader));
    } else {
      ASSERT_FALSE(!!header_map[test_case.redirected_to_path].count(
          signin::kChromeConnectedHeader));
    }

    header_map.clear();
  }
}

#endif  // !BUILDFLAG(ENABLE_DICE_SUPPORT)

// Check that exactly one set of throttles is added to smaller downloads, which
// have their mime type determined only after the response is completely
// received.
// See https://crbug.com/640545
IN_PROC_BROWSER_TEST_F(ChromeResourceDispatcherHostDelegateBrowserTest,
                       ThrottlesAddedExactlyOnceToTinySniffedDownloads) {
  // This code path isn't used when the network service is enabled.
  if (base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;

  GURL url = embedded_test_server()->GetURL("/downloads/tiny_binary.bin");
  DownloadTestObserverNotInProgress download_observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()), 1);
  download_observer.StartObserving();
  ui_test_utils::NavigateToURL(browser(), url);
  download_observer.WaitForFinished();
  EXPECT_EQ(1, GetTimesStandardThrottlesAddedForURL(url));
}

// Check that exactly one set of throttles is added to larger downloads, which
// have their mime type determined before the end of the response is reported.
IN_PROC_BROWSER_TEST_F(ChromeResourceDispatcherHostDelegateBrowserTest,
                       ThrottlesAddedExactlyOnceToLargeSniffedDownloads) {
  // This code path isn't used when the network service is enabled.
  if (base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;

  GURL url = embedded_test_server()->GetURL("/downloads/thisdayinhistory.xls");
  DownloadTestObserverNotInProgress download_observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()), 1);
  download_observer.StartObserving();
  ui_test_utils::NavigateToURL(browser(), url);
  download_observer.WaitForFinished();
  EXPECT_EQ(1, GetTimesStandardThrottlesAddedForURL(url));
}

// Check that exactly one set of throttles is added to downloads started by an
// <a download> click.
IN_PROC_BROWSER_TEST_F(ChromeResourceDispatcherHostDelegateBrowserTest,
                       ThrottlesAddedExactlyOnceToADownloads) {
  // This code path isn't used when the network service is enabled.
  if (base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;

  DownloadTestObserverNotInProgress download_observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()), 1);
  download_observer.StartObserving();
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL(
                                              "/download-anchor-attrib.html"));
  download_observer.WaitForFinished();
  EXPECT_EQ(1,
            GetTimesStandardThrottlesAddedForURL(
                embedded_test_server()->GetURL("/anchor_download_test.png")));
}
