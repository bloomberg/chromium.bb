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
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/test/scoped_command_line.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_browsertest.h"
#include "chrome/browser/loader/chrome_navigation_data.h"
#include "chrome/browser/policy/cloud/policy_header_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
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
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

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
