// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_tab_helper.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_features.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service_factory.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service_workers_observer.h"
#include "chrome/browser/prerender/isolated/prefetched_mainframe_response_container.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const int kTotalTimeDuration = 1337;

const int kConnectTimeDuration = 123;

const char kHTMLMimeType[] = "text/html";

const char kHTMLBody[] = R"(
      <!DOCTYPE HTML>
      <html>
        <head></head>
        <body></body>
      </html>)";

}  // namespace

class TestIsolatedPrerenderTabHelper : public IsolatedPrerenderTabHelper {
 public:
  explicit TestIsolatedPrerenderTabHelper(content::WebContents* web_contents)
      : IsolatedPrerenderTabHelper(web_contents) {}

  void SetURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
    url_loader_factory_ = url_loader_factory;
  }

  network::mojom::URLLoaderFactory* GetURLLoaderFactory() override {
    return url_loader_factory_.get();
  }

 private:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

class IsolatedPrerenderTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  IsolatedPrerenderTabHelperTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}
  ~IsolatedPrerenderTabHelperTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    IsolatedPrerenderService* isolated_prerender_service =
        IsolatedPrerenderServiceFactory::GetForProfile(profile());
    isolated_prerender_service->service_workers_observer()
        ->CallOnHasUsageInfoForTesting({});

    tab_helper_ =
        std::make_unique<TestIsolatedPrerenderTabHelper>(web_contents());
    tab_helper_->SetURLLoaderFactory(test_shared_loader_factory_);

    SetDataSaverEnabled(true);
  }

  void TearDown() override {
    tab_helper_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetDataSaverEnabled(bool enabled) {
    data_reduction_proxy::DataReductionProxySettings::
        SetDataSaverEnabledForTesting(profile()->GetPrefs(), enabled);
  }

  void MakeNavigationPrediction(const content::WebContents* web_contents,
                                const GURL& doc_url,
                                const std::vector<GURL>& predicted_urls) {
    NavigationPredictorKeyedServiceFactory::GetForProfile(profile())
        ->OnPredictionUpdated(
            web_contents, doc_url,
            NavigationPredictorKeyedService::PredictionSource::
                kAnchorElementsParsedFromWebPage,
            predicted_urls);
    task_environment()->RunUntilIdle();
  }

  void MakeExternalAndroidAppNavigationPrediction(
      const std::vector<GURL>& predicted_urls) {
    NavigationPredictorKeyedServiceFactory::GetForProfile(profile())
        ->OnPredictionUpdatedByExternalAndroidApp({"com.example.foo"},
                                                  predicted_urls);
    task_environment()->RunUntilIdle();
  }

  void TriggerRedirectHistogramRecording() {
    content::MockNavigationHandle handle(web_contents());
    tab_helper_->DidStartNavigation(&handle);
  }

  int RequestCount() { return test_url_loader_factory_.NumPending(); }

  IsolatedPrerenderTabHelper* tab_helper() const { return tab_helper_.get(); }

  int64_t ordered_eligible_pages_bitmask() const {
    return tab_helper_->srp_metrics().ordered_eligible_pages_bitmask_;
  }

  size_t prefetch_eligible_count() const {
    return tab_helper_->srp_metrics().prefetch_eligible_count_;
  }

  size_t prefetch_attempted_count() const {
    return tab_helper_->srp_metrics().prefetch_attempted_count_;
  }

  size_t prefetch_successful_count() const {
    return tab_helper_->srp_metrics().prefetch_successful_count_;
  }

  size_t prefetch_total_redirect_count() const {
    return tab_helper_->srp_metrics().prefetch_total_redirect_count_;
  }

  size_t predicted_urls_count() const {
    return tab_helper_->srp_metrics().predicted_urls_count_;
  }

  base::Optional<base::TimeDelta> navigation_to_prefetch_start() const {
    return tab_helper_->srp_metrics().navigation_to_prefetch_start_;
  }

  bool HasAfterSRPMetrics() {
    return tab_helper_->after_srp_metrics().has_value();
  }

  size_t after_srp_prefetch_eligible_count() const {
    DCHECK(tab_helper_->after_srp_metrics());
    return tab_helper_->after_srp_metrics()->prefetch_eligible_count_;
  }

  base::Optional<size_t> after_srp_clicked_link_srp_position() const {
    DCHECK(tab_helper_->after_srp_metrics());
    return tab_helper_->after_srp_metrics()->clicked_link_srp_position_;
  }

  void Navigate(const GURL& url) {
    content::MockNavigationHandle handle(web_contents());
    handle.set_url(url);
    tab_helper_->DidStartNavigation(&handle);
    handle.set_has_committed(true);
    tab_helper_->DidFinishNavigation(&handle);
  }

  void NavigateSomewhere() { Navigate(GURL("https://test.com")); }

  void NavigateAndVerifyPrefetchStatus(
      const GURL& url,
      IsolatedPrerenderTabHelper::PrefetchStatus expected_status) {
    // Navigate to trigger an after-srp page load where the status for the given
    // url should be placed into the after srp metrics.
    Navigate(url);

    ASSERT_TRUE(tab_helper_->after_srp_metrics().has_value());
    ASSERT_TRUE(tab_helper_->after_srp_metrics()->prefetch_status_.has_value());
    EXPECT_EQ(expected_status,
              tab_helper_->after_srp_metrics()->prefetch_status_.value());
  }

  void VerifyIsolationInfo(const net::IsolationInfo& isolation_info) {
    EXPECT_FALSE(isolation_info.IsEmpty());
    EXPECT_TRUE(isolation_info.opaque_and_non_transient());
    net::NetworkIsolationKey key = isolation_info.network_isolation_key();
    EXPECT_TRUE(key.IsFullyPopulated());
    EXPECT_FALSE(key.IsTransient());
    EXPECT_TRUE(base::StartsWith(key.ToString(), "opaque non-transient ",
                                 base::CompareCase::SENSITIVE));
  }

  network::ResourceRequest VerifyCommonRequestState(const GURL& url) {
    SCOPED_TRACE(url.spec());
    EXPECT_EQ(RequestCount(), 1);

    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);

    EXPECT_EQ(request->request.url, url);
    EXPECT_EQ(request->request.method, "GET");
    EXPECT_EQ(request->request.load_flags,
              net::LOAD_DISABLE_CACHE | net::LOAD_PREFETCH);
    EXPECT_EQ(request->request.credentials_mode,
              network::mojom::CredentialsMode::kOmit);

    EXPECT_TRUE(request->request.trusted_params.has_value());
    VerifyIsolationInfo(request->request.trusted_params->isolation_info);

    return request->request;
  }

  std::string RequestHeader(const std::string& key) {
    if (test_url_loader_factory_.NumPending() != 1)
      return std::string();

    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);

    std::string value;
    if (request->request.headers.GetHeader(key, &value))
      return value;

    return std::string();
  }

  void MakeResponseAndWait(
      net::HttpStatusCode http_status,
      net::Error net_error,
      const std::string& mime_type,
      std::vector<std::pair<std::string, std::string>> headers,
      const std::string& body) {
    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);
    ASSERT_TRUE(request);

    auto head = network::CreateURLResponseHead(http_status);

    head->response_time = base::Time::Now();
    head->request_time = head->response_time -
                         base::TimeDelta::FromMilliseconds(kTotalTimeDuration);

    head->load_timing.connect_timing.connect_end =
        base::TimeTicks::Now() - base::TimeDelta::FromMinutes(2);
    head->load_timing.connect_timing.connect_start =
        head->load_timing.connect_timing.connect_end -
        base::TimeDelta::FromMilliseconds(kConnectTimeDuration);

    head->mime_type = mime_type;
    for (const auto& header : headers) {
      head->headers->AddHeader(header.first, header.second);
    }
    network::URLLoaderCompletionStatus status(net_error);
    test_url_loader_factory_.AddResponse(request->request.url, std::move(head),
                                         body, status);
    task_environment()->RunUntilIdle();
    // Clear responses in the network service so we can inspect the next request
    // that comes in before it is responded to.
    ClearResponses();
  }

  void ClearResponses() { test_url_loader_factory_.ClearResponses(); }

  bool SetCookie(content::BrowserContext* browser_context,
                 const GURL& url,
                 const std::string& value) {
    bool result = false;
    base::RunLoop run_loop;
    mojo::Remote<network::mojom::CookieManager> cookie_manager;
    content::BrowserContext::GetDefaultStoragePartition(browser_context)
        ->GetNetworkContext()
        ->GetCookieManager(cookie_manager.BindNewPipeAndPassReceiver());
    std::unique_ptr<net::CanonicalCookie> cc(net::CanonicalCookie::Create(
        url, value, base::Time::Now(), base::nullopt /* server_time */));
    EXPECT_TRUE(cc.get());

    net::CookieOptions options;
    options.set_include_httponly();
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::MakeInclusive());
    cookie_manager->SetCanonicalCookie(
        *cc.get(), url, options,
        base::BindOnce(
            [](bool* result, base::RunLoop* run_loop,
               net::CanonicalCookie::CookieInclusionStatus set_cookie_status) {
              *result = set_cookie_status.IsInclude();
              run_loop->Quit();
            },
            &result, &run_loop));
    run_loop.Run();
    return result;
  }

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

 private:
  std::unique_ptr<TestIsolatedPrerenderTabHelper> tab_helper_;
};

TEST_F(IsolatedPrerenderTabHelperTest, FeatureDisabled) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kIsolatePrerenders);

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 0U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.RespCode", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.ConnectTime", 0);

  Navigate(prediction_url);

  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects", 0);

  EXPECT_FALSE(HasAfterSRPMetrics());
}

TEST_F(IsolatedPrerenderTabHelperTest, DataSaverDisabled) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  SetDataSaverEnabled(false);

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 0U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.RespCode", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.ConnectTime", 0);

  Navigate(prediction_url);

  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects", 0);

  EXPECT_FALSE(HasAfterSRPMetrics());
}

TEST_F(IsolatedPrerenderTabHelperTest, GoogleSRPOnly) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  NavigateSomewhere();
  GURL doc_url("https://www.not-google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 0U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.RespCode", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.ConnectTime", 0);

  Navigate(prediction_url);

  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects", 0);

  EXPECT_FALSE(HasAfterSRPMetrics());
}

TEST_F(IsolatedPrerenderTabHelperTest, SRPOnly) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/photos?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 0U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.RespCode", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.ConnectTime", 0);

  Navigate(prediction_url);

  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects", 0);

  EXPECT_FALSE(HasAfterSRPMetrics());
}

TEST_F(IsolatedPrerenderTabHelperTest, HTTPSPredictionsOnly) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("http://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.RespCode", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.ConnectTime", 0);

  NavigateAndVerifyPrefetchStatus(prediction_url,
                                  IsolatedPrerenderTabHelper::PrefetchStatus::
                                      kPrefetchNotEligibleSchemeIsNotHttps);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 0U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects", 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, DontFetchGoogleLinks) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("http://www.google.com/user");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.RespCode", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.ConnectTime", 0);

  NavigateAndVerifyPrefetchStatus(prediction_url,
                                  IsolatedPrerenderTabHelper::PrefetchStatus::
                                      kPrefetchNotEligibleGoogleDomain);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 0U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects", 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, DontFetchIPAddresses) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://123.234.123.234/meow");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.RespCode", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.ConnectTime", 0);

  NavigateAndVerifyPrefetchStatus(prediction_url,
                                  IsolatedPrerenderTabHelper::PrefetchStatus::
                                      kPrefetchNotEligibleHostIsIPAddress);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 0U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects", 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, WrongWebContents) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(nullptr, doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 0U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.RespCode", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.ConnectTime", 0);

  Navigate(prediction_url);

  EXPECT_FALSE(HasAfterSRPMetrics());

  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects", 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, HasPurposePrefetchHeader) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  VerifyCommonRequestState(prediction_url);
  EXPECT_EQ(RequestHeader("Purpose"), "prefetch");

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());
}

TEST_F(IsolatedPrerenderTabHelperTest, NoCookies) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");

  ASSERT_TRUE(SetCookie(profile(), prediction_url, "testing"));

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.RespCode", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.ConnectTime", 0);

  NavigateAndVerifyPrefetchStatus(prediction_url,
                                  IsolatedPrerenderTabHelper::PrefetchStatus::
                                      kPrefetchNotEligibleUserHasCookies);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 0U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects", 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, 2XXOnly) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_NOT_FOUND, net::OK, kHTMLMimeType,
                      /*headers=*/{}, kHTMLBody);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.RespCode", net::HTTP_NOT_FOUND, 1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.BodyLength", base::size(kHTMLBody),
      1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration,
      1);

  NavigateAndVerifyPrefetchStatus(
      prediction_url,
      IsolatedPrerenderTabHelper::PrefetchStatus::kPrefetchFailedNon2XX);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects", 0, 1);
}

TEST_F(IsolatedPrerenderTabHelperTest, NetErrorOKOnly) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED, kHTMLMimeType,
                      /*headers=*/{}, kHTMLBody);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.NetError",
      std::abs(net::ERR_FAILED), 1);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.RespCode", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.ConnectTime", 0);

  NavigateAndVerifyPrefetchStatus(
      prediction_url,
      IsolatedPrerenderTabHelper::PrefetchStatus::kPrefetchFailedNetError);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects", 0, 1);
}

TEST_F(IsolatedPrerenderTabHelperTest, NonHTML) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  std::string body = "console.log('Hello world');";
  VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_OK, net::OK, "application/javascript",
                      /*headers=*/{}, body);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.BodyLength", body.size(), 1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration,
      1);

  NavigateAndVerifyPrefetchStatus(
      prediction_url,
      IsolatedPrerenderTabHelper::PrefetchStatus::kPrefetchFailedNotHTML);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects", 0, 1);
}

TEST_F(IsolatedPrerenderTabHelperTest, UserSettingDisabled) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  profile()->GetPrefs()->SetInteger(
      prefs::kNetworkPredictionOptions,
      chrome_browser_net::NETWORK_PREDICTION_NEVER);

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 0U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.RespCode", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.ConnectTime", 0);

  Navigate(prediction_url);

  EXPECT_FALSE(HasAfterSRPMetrics());

  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects", 0);
}

// Verify that isolated prerender is not triggered if the predictions for next
// likely navigations are provided by external Android app.
TEST_F(IsolatedPrerenderTabHelperTest, ExternalAndroidApp) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeExternalAndroidAppNavigationPrediction({prediction_url});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, SuccessCase) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  network::ResourceRequest request = VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  std::unique_ptr<PrefetchedMainframeResponseContainer> resp =
      tab_helper()->TakePrefetchResponse(prediction_url);
  ASSERT_TRUE(resp);
  EXPECT_EQ(*resp->TakeBody(), kHTMLBody);

  network::mojom::URLResponseHeadPtr head = resp->TakeHead();
  EXPECT_TRUE(head->headers->HasHeaderValue("X-Testing", "Hello World"));

  EXPECT_TRUE(resp->isolation_info().IsEqualForTesting(
      request.trusted_params->isolation_info));
  VerifyIsolationInfo(resp->isolation_info());

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 1U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.BodyLength", base::size(kHTMLBody),
      1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration,
      1);

  NavigateAndVerifyPrefetchStatus(
      prediction_url,
      IsolatedPrerenderTabHelper::PrefetchStatus::kPrefetchSuccessful);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects", 0, 1);
}

TEST_F(IsolatedPrerenderTabHelperTest, AfterSRPLinkNotOnSRP) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  network::ResourceRequest request = VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 1U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.BodyLength", base::size(kHTMLBody),
      1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration,
      1);

  NavigateAndVerifyPrefetchStatus(
      GURL("https://wasnt-on-srp.com"),
      IsolatedPrerenderTabHelper::PrefetchStatus::kNavigatedToLinkNotOnSRP);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::nullopt, after_srp_clicked_link_srp_position());

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects", 0, 1);
}

TEST_F(IsolatedPrerenderTabHelperTest, LimitedNumberOfPrefetches_Zero) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders, {{"max_srp_prefetches", "0"}});

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.RespCode", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.ConnectTime", 0);

  NavigateAndVerifyPrefetchStatus(
      prediction_url,
      IsolatedPrerenderTabHelper::PrefetchStatus::kPrefetchNotStarted);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects", 0);
}

TEST_F(IsolatedPrerenderTabHelperTest,
       NumberOfPrefetches_UnlimitedByExperiment) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders, {{"max_srp_prefetches", "-1"}});

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url_1("https://www.cat-food.com/");
  GURL prediction_url_2("https://www.dogs-r-dumb.com/");
  GURL prediction_url_3("https://www.catz-rule.com/");
  MakeNavigationPrediction(
      web_contents(), doc_url,
      {prediction_url_1, prediction_url_2, prediction_url_3});

  VerifyCommonRequestState(prediction_url_1);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, {}, kHTMLBody);
  VerifyCommonRequestState(prediction_url_2);
  // Failed responses do not retry or attempt more requests in the list.
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED, kHTMLMimeType, {},
                      kHTMLBody);
  VerifyCommonRequestState(prediction_url_3);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, {}, kHTMLBody);

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 3U);
  EXPECT_EQ(prefetch_eligible_count(), 3U);
  EXPECT_EQ(prefetch_attempted_count(), 3U);
  EXPECT_EQ(prefetch_successful_count(), 2U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectBucketCount(
      "IsolatedPrerender.Prefetch.Mainframe.NetError", net::OK, 2);
  histogram_tester.ExpectBucketCount(
      "IsolatedPrerender.Prefetch.Mainframe.NetError",
      std::abs(net::ERR_FAILED), 1);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.NetError", 3);

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.RespCode", net::HTTP_OK, 2);

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.BodyLength", base::size(kHTMLBody),
      2);

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 2);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration,
      2);

  NavigateAndVerifyPrefetchStatus(
      prediction_url_3,
      IsolatedPrerenderTabHelper::PrefetchStatus::kPrefetchSuccessful);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 3U);
  EXPECT_EQ(base::Optional<size_t>(2), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects", 0, 1);
}

TEST_F(IsolatedPrerenderTabHelperTest, OrderedBitMask) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "isolated-prerender-unlimited-prefetches");

  NavigateSomewhere();
  MakeNavigationPrediction(web_contents(),
                           GURL("https://www.google.com/search?q=cats"),
                           {
                               GURL("http://not-eligible-1.com"),
                               GURL("http://not-eligible-2.com"),
                               GURL("https://eligible-1.com"),
                               GURL("https://eligible-2.com"),
                               GURL("https://eligible-3.com"),
                               GURL("http://not-eligible-3.com"),
                           });

  EXPECT_EQ(predicted_urls_count(), 6U);
  EXPECT_EQ(prefetch_eligible_count(), 3U);
  EXPECT_EQ(ordered_eligible_pages_bitmask(), 0b011100);
}

TEST_F(IsolatedPrerenderTabHelperTest, NumberOfPrefetches_UnlimitedByCmdLine) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "isolated-prerender-unlimited-prefetches");

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url_1("https://www.cat-food.com/");
  GURL prediction_url_2("https://www.dogs-r-dumb.com/");
  GURL prediction_url_3("https://www.catz-rule.com/");
  MakeNavigationPrediction(
      web_contents(), doc_url,
      {prediction_url_1, prediction_url_2, prediction_url_3});

  VerifyCommonRequestState(prediction_url_1);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, {}, kHTMLBody);
  VerifyCommonRequestState(prediction_url_2);
  // Failed responses do not retry or attempt more requests in the list.
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED, kHTMLMimeType, {},
                      kHTMLBody);
  VerifyCommonRequestState(prediction_url_3);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, {}, kHTMLBody);

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 3U);
  EXPECT_EQ(prefetch_eligible_count(), 3U);
  EXPECT_EQ(prefetch_attempted_count(), 3U);
  EXPECT_EQ(prefetch_successful_count(), 2U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectBucketCount(
      "IsolatedPrerender.Prefetch.Mainframe.NetError", net::OK, 2);
  histogram_tester.ExpectBucketCount(
      "IsolatedPrerender.Prefetch.Mainframe.NetError",
      std::abs(net::ERR_FAILED), 1);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.NetError", 3);

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.RespCode", net::HTTP_OK, 2);

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.BodyLength", base::size(kHTMLBody),
      2);

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 2);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration,
      2);

  NavigateAndVerifyPrefetchStatus(
      prediction_url_1,
      IsolatedPrerenderTabHelper::PrefetchStatus::kPrefetchSuccessful);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 3U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects", 0, 1);
}

TEST_F(IsolatedPrerenderTabHelperTest, LimitedNumberOfPrefetches) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders, {{"max_srp_prefetches", "2"}});

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url_1("https://www.cat-food.com/");
  GURL prediction_url_2("https://www.dogs-r-dumb.com/");
  GURL prediction_url_3("https://www.catz-rule.com/");
  MakeNavigationPrediction(
      web_contents(), doc_url,
      {prediction_url_1, prediction_url_2, prediction_url_3});

  VerifyCommonRequestState(prediction_url_1);
  // Failed responses do not retry or attempt more requests in the list.
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED, kHTMLMimeType, {},
                      kHTMLBody);
  VerifyCommonRequestState(prediction_url_2);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, {}, kHTMLBody);

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 3U);
  EXPECT_EQ(prefetch_eligible_count(), 3U);
  EXPECT_EQ(prefetch_attempted_count(), 2U);
  EXPECT_EQ(prefetch_successful_count(), 1U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectBucketCount(
      "IsolatedPrerender.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectBucketCount(
      "IsolatedPrerender.Prefetch.Mainframe.NetError",
      std::abs(net::ERR_FAILED), 1);
  histogram_tester.ExpectTotalCount(
      "IsolatedPrerender.Prefetch.Mainframe.NetError", 2);

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.BodyLength", base::size(kHTMLBody),
      1);

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration,
      1);

  TriggerRedirectHistogramRecording();
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects", 0, 1);
}

TEST_F(IsolatedPrerenderTabHelperTest, PrefetchingNotStartedWhileInvisible) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  web_contents()->WasHidden();

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());
}

TEST_F(IsolatedPrerenderTabHelperTest, PrefetchingPausedWhenInvisible) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url_1("https://www.cat-food.com/");
  GURL prediction_url_2("https://www.dogs-r-dumb.com/");

  MakeNavigationPrediction(web_contents(), doc_url,
                           {prediction_url_1, prediction_url_2});
  VerifyCommonRequestState(prediction_url_1);

  // When hidden, the current prefetch is allowed to finish.
  web_contents()->WasHidden();
  VerifyCommonRequestState(prediction_url_1);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, {}, kHTMLBody);

  // But no more prefetches should start when hidden.
  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 2U);
  EXPECT_EQ(prefetch_eligible_count(), 2U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 1U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.BodyLength", base::size(kHTMLBody),
      1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration,
      1);

  TriggerRedirectHistogramRecording();
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects", 0, 1);
}

TEST_F(IsolatedPrerenderTabHelperTest, PrefetchingRestartedWhenVisible) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  web_contents()->WasHidden();

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());
  base::TimeDelta first_nav_to_prefetch_start =
      navigation_to_prefetch_start().value();

  web_contents()->WasShown();

  VerifyCommonRequestState(prediction_url);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());
  EXPECT_EQ(first_nav_to_prefetch_start,
            navigation_to_prefetch_start().value());
}

TEST_F(IsolatedPrerenderTabHelperTest, ServiceWorkerRegistered) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");

  IsolatedPrerenderService* isolated_prerender_service =
      IsolatedPrerenderServiceFactory::GetForProfile(profile());
  content::ServiceWorkerContextObserver* observer =
      isolated_prerender_service->service_workers_observer();
  observer->OnRegistrationCompleted(prediction_url);

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 0U);
  EXPECT_EQ(prefetch_attempted_count(), 0U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_FALSE(navigation_to_prefetch_start().has_value());

  NavigateAndVerifyPrefetchStatus(prediction_url,
                                  IsolatedPrerenderTabHelper::PrefetchStatus::
                                      kPrefetchNotEligibleUserHasServiceWorker);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 0U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());
}

TEST_F(IsolatedPrerenderTabHelperTest, ServiceWorkerNotRegistered) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  GURL service_worker_registration("https://www.service-worker.com/");

  IsolatedPrerenderService* isolated_prerender_service =
      IsolatedPrerenderServiceFactory::GetForProfile(profile());
  content::ServiceWorkerContextObserver* observer =
      isolated_prerender_service->service_workers_observer();
  observer->OnRegistrationCompleted(service_worker_registration);

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  VerifyCommonRequestState(prediction_url);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 0U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());
}

class IsolatedPrerenderTabHelperRedirectTest
    : public IsolatedPrerenderTabHelperTest {
 public:
  IsolatedPrerenderTabHelperRedirectTest() = default;
  ~IsolatedPrerenderTabHelperRedirectTest() override = default;

  void WalkRedirectChainUntilFinalRequest(std::vector<GURL> redirect_chain) {
    ASSERT_GE(redirect_chain.size(), 2U)
        << "redirect_chain must contain the full redirect chain, with the "
           "first element being the first request url, the last element being "
           "the final request url, and any intermediate steps in the middle";

    // Since the prefetches do not follow redirects, but instead have to pause
    // and query the cookie jar every time, each step in the redirect chain
    // needs to be handled like a separate request/response pair.
    for (size_t i = 0; i < redirect_chain.size() - 1; i++) {
      network::TestURLLoaderFactory::Redirects redirects;
      net::RedirectInfo info;
      info.new_url = redirect_chain[i + 1];
      info.status_code = net::HTTP_TEMPORARY_REDIRECT;
      auto head = network::CreateURLResponseHead(net::HTTP_TEMPORARY_REDIRECT);
      redirects.push_back(std::make_pair(info, head->Clone()));

      network::TestURLLoaderFactory::PendingRequest* request =
          test_url_loader_factory_.GetPendingRequest(0);
      ASSERT_TRUE(request);
      EXPECT_EQ(request->request.url, redirect_chain[i]);

      test_url_loader_factory_.AddResponse(
          redirect_chain[i], std::move(head), "unused body during redirect",
          network::URLLoaderCompletionStatus(net::OK), std::move(redirects));

      task_environment()->RunUntilIdle();
    }
    // Clear responses in the network service so we can inspect the next
    // request that comes in before it is responded to.
    ClearResponses();
  }

  void MakeFinalResponse(
      const GURL& final_url,
      net::HttpStatusCode final_status,
      std::vector<std::pair<std::string, std::string>> final_headers,
      const std::string& final_body) {
    auto final_head = network::CreateURLResponseHead(final_status);

    final_head->response_time = base::Time::Now();
    final_head->request_time =
        final_head->response_time -
        base::TimeDelta::FromMilliseconds(kTotalTimeDuration);

    final_head->load_timing.connect_timing.connect_end =
        base::TimeTicks::Now() - base::TimeDelta::FromMinutes(2);
    final_head->load_timing.connect_timing.connect_start =
        final_head->load_timing.connect_timing.connect_end -
        base::TimeDelta::FromMilliseconds(kConnectTimeDuration);

    final_head->mime_type = kHTMLMimeType;

    for (const auto& header : final_headers) {
      final_head->headers->AddHeader(header.first, header.second);
    }
    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);
    ASSERT_TRUE(request);
    EXPECT_EQ(final_url, request->request.url);
    test_url_loader_factory_.AddResponse(
        final_url, std::move(final_head), final_body,
        network::URLLoaderCompletionStatus(net::OK));
    task_environment()->RunUntilIdle();

    // Clear responses in the network service so we can inspect the next request
    // that comes in before it is responded to.
    ClearResponses();
  }

  void RunNoRedirectTest(const GURL& redirect_url) {
    GURL doc_url("https://www.google.com/search?q=cats");
    GURL prediction_url("https://www.cat-food.com/");

    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

    MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

    VerifyCommonRequestState(prediction_url);
    WalkRedirectChainUntilFinalRequest({prediction_url, redirect_url});
    // Redirect should not be followed.
    EXPECT_EQ(RequestCount(), 0);
  }
};

// Fails on TSAN builders: https://crbug.com/1078965.
#if defined(THREAD_SANITIZER)
#define MAYBE_NoRedirect_Cookies DISABLED_NoRedirect_Cookies
#else
#define MAYBE_NoRedirect_Cookies NoRedirect_Cookies
#endif
TEST_F(IsolatedPrerenderTabHelperRedirectTest, MAYBE_NoRedirect_Cookies) {
  NavigateSomewhere();

  GURL site_with_cookies("https://cookies.com");
  ASSERT_TRUE(SetCookie(profile(), site_with_cookies, "testing"));
  RunNoRedirectTest(site_with_cookies);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 1U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  NavigateAndVerifyPrefetchStatus(site_with_cookies,
                                  IsolatedPrerenderTabHelper::PrefetchStatus::
                                      kPrefetchNotEligibleUserHasCookies);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());
}

TEST_F(IsolatedPrerenderTabHelperRedirectTest, NoRedirect_Insecure) {
  NavigateSomewhere();

  GURL url("http://insecure.com");

  RunNoRedirectTest(url);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 1U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  NavigateAndVerifyPrefetchStatus(url,
                                  IsolatedPrerenderTabHelper::PrefetchStatus::
                                      kPrefetchNotEligibleSchemeIsNotHttps);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());
}

TEST_F(IsolatedPrerenderTabHelperRedirectTest, NoRedirect_Google) {
  NavigateSomewhere();

  GURL url("https://www.google.com");

  RunNoRedirectTest(url);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 1U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  NavigateAndVerifyPrefetchStatus(url,
                                  IsolatedPrerenderTabHelper::PrefetchStatus::
                                      kPrefetchNotEligibleGoogleDomain);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());
}

TEST_F(IsolatedPrerenderTabHelperRedirectTest, NoRedirect_ServiceWorker) {
  NavigateSomewhere();

  GURL site_with_worker("https://service-worker.com");

  IsolatedPrerenderService* isolated_prerender_service =
      IsolatedPrerenderServiceFactory::GetForProfile(profile());
  content::ServiceWorkerContextObserver* observer =
      isolated_prerender_service->service_workers_observer();
  observer->OnRegistrationCompleted(site_with_worker);

  RunNoRedirectTest(site_with_worker);

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 1U);
  EXPECT_EQ(prefetch_attempted_count(), 1U);
  EXPECT_EQ(prefetch_successful_count(), 0U);
  EXPECT_EQ(prefetch_total_redirect_count(), 1U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  NavigateAndVerifyPrefetchStatus(site_with_worker,
                                  IsolatedPrerenderTabHelper::PrefetchStatus::
                                      kPrefetchNotEligibleUserHasServiceWorker);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 1U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());
}

TEST_F(IsolatedPrerenderTabHelperRedirectTest, SuccessfulRedirect) {
  base::HistogramTester histogram_tester;
  NavigateSomewhere();
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  GURL redirect_url("https://redirect-here.com");

  // Enable unlimited prefetches so we can follow the redirect chain all the
  // way.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders, {{"max_srp_prefetches", "-1"}});

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});
  VerifyCommonRequestState(prediction_url);

  WalkRedirectChainUntilFinalRequest({prediction_url, redirect_url});
  MakeFinalResponse(redirect_url, net::HTTP_OK, {{"X-Testing", "Hello World"}},
                    kHTMLBody);

  std::unique_ptr<PrefetchedMainframeResponseContainer> resp =
      tab_helper()->TakePrefetchResponse(redirect_url);
  ASSERT_TRUE(resp);
  EXPECT_EQ(*resp->TakeBody(), kHTMLBody);

  network::mojom::URLResponseHeadPtr head = resp->TakeHead();
  EXPECT_TRUE(head->headers->HasHeaderValue("X-Testing", "Hello World"));

  EXPECT_EQ(predicted_urls_count(), 1U);
  EXPECT_EQ(prefetch_eligible_count(), 2U);
  EXPECT_EQ(prefetch_attempted_count(), 2U);
  EXPECT_EQ(prefetch_successful_count(), 1U);
  EXPECT_EQ(prefetch_total_redirect_count(), 1U);
  EXPECT_TRUE(navigation_to_prefetch_start().has_value());

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.BodyLength", base::size(kHTMLBody),
      1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration,
      1);

  NavigateAndVerifyPrefetchStatus(
      redirect_url,
      IsolatedPrerenderTabHelper::PrefetchStatus::kPrefetchSuccessful);
  EXPECT_EQ(after_srp_prefetch_eligible_count(), 2U);
  EXPECT_EQ(base::Optional<size_t>(0), after_srp_clicked_link_srp_position());

  histogram_tester.ExpectUniqueSample(
      "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects", 1, 1);
}
