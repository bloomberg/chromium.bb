// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <map>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/infobars/mock_infobar_service.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/previews/previews_lite_page_decider.h"
#include "chrome/browser/previews/previews_lite_page_navigation_throttle.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/proto/data_store.pb.h"
#include "components/history/core/browser/history_service.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/test_hints_component_creator.h"
#include "components/prefs/pref_service.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_constants.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_lite_page_redirect.h"
#include "components/previews/core/previews_switches.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/nqe/effective_connection_type.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/test/test_network_quality_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {
const int kTimeoutMs = 250;
}

class PreviewsLitePageServerBrowserTest : public InProcessBrowserTest {
 public:
  PreviewsLitePageServerBrowserTest() {}

  ~PreviewsLitePageServerBrowserTest() override {}

  enum PreviewsServerAction {
    // Previews server will respond with HTTP 200 OK, OFCL=60,
    // Content-Length=20.
    kSuccess = 0,

    // Previews server will respond with a bypass signal (HTTP 307).
    kBypass = 1,

    // Previews server will respond with HTTP 307 to a non-preview page.
    kRedirectNonPreview = 2,

    // Previews server will respond with HTTP 503.
    kLoadshed = 3,

    // Previews server will respond with HTTP 403.
    kAuthFailure = 4,

    // Previews server will respond with HTTP 307 to a non-preview page and set
    // the host-blacklist header value.
    kHostBlacklist = 5,

    // Previews server will respond with HTTP 200 and a content body that loads
    // a subresource. When the subresource is loaded, |subresources_requested|_
    // will be incremented if the X-Client-Data header if in the request.
    kSubresources = 6,

    // Previews server will respond with HTTP 307 to a preview page.
    kRedirectPreview = 7,
  };

  void SetUpCommandLine(base::CommandLine* cmd) override {
    // Due to race conditions, it's possible that blacklist data is not loaded
    // at the time of first navigation. That may prevent Preview from
    // triggering, and causing the test to flake.
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlacklist);
    cmd->AppendSwitch("enable-spdy-proxy-auth");
    cmd->AppendSwitchASCII("data-reduction-proxy-client-config",
                           data_reduction_proxy::DummyBase64Config());
    cmd->AppendSwitchASCII("force-effective-connection-type", "Slow-2G");
    cmd->AppendSwitchASCII("force-variation-ids", "42");
    // Resolve all localhost subdomains to plain localhost so that Chrome's Test
    // DNS resolver doesn't get upset.
    cmd->AppendSwitchASCII("host-rules",
                           "MAP *.localhost 127.0.0.1,"
                           "MAP *.127.0.0.1 127.0.0.1,"
                           "MAP *.litepages.googlezip.net 127.0.0.1");
  }

  void SetUp() override {
    SetUpLitePageTest(false /* use_timeout */, false /* is_control */);

    InProcessBrowserTest::SetUp();
  }

  void SetUpLitePageTest(bool use_timeout, bool is_control) {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    https_server_->RegisterRequestHandler(base::BindRepeating(
        &PreviewsLitePageServerBrowserTest::HandleRedirectRequest,
        base::Unretained(this)));
    ASSERT_TRUE(https_server_->Start());

    https_url_ = https_server_->GetURL("/previews/noscript_test.html");
    ASSERT_TRUE(https_url_.SchemeIs(url::kHttpsScheme));

    https_to_https_redirect_url_ =
        https_server_->GetURL("/previews/to_https_redirect.html");
    ASSERT_TRUE(https_to_https_redirect_url_.SchemeIs(url::kHttpsScheme));

    base_https_lite_page_url_ =
        https_server_->GetURL("/previews/lite_page_test.html");
    ASSERT_TRUE(base_https_lite_page_url_.SchemeIs(url::kHttpsScheme));

    https_media_url_ = https_server_->GetURL("/image_decoding/droids.jpg");
    ASSERT_TRUE(https_media_url_.SchemeIs(url::kHttpsScheme));

    // Set up http server with resource monitor and redirect handler.
    http_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    http_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    http_server_->RegisterRequestHandler(base::BindRepeating(
        &PreviewsLitePageServerBrowserTest::HandleRedirectRequest,
        base::Unretained(this)));
    ASSERT_TRUE(http_server_->Start());

    http_url_ = http_server_->GetURL("/previews/noscript_test.html");
    ASSERT_TRUE(http_url_.SchemeIs(url::kHttpScheme));

    base_http_lite_page_url_ =
        http_server_->GetURL("/previews/lite_page_test.html");
    ASSERT_TRUE(base_http_lite_page_url_.SchemeIs(url::kHttpScheme));

    subframe_url_ = http_server_->GetURL("/previews/iframe_blank.html");
    ASSERT_TRUE(subframe_url_.SchemeIs(url::kHttpScheme));

    http_to_https_redirect_url_ =
        http_server_->GetURL("/previews/to_https_redirect.html");
    ASSERT_TRUE(http_to_https_redirect_url_.SchemeIs(url::kHttpScheme));

    client_redirect_url_ =
        http_server_->GetURL("/previews/client_redirect.html");
    ASSERT_TRUE(client_redirect_url_.SchemeIs(url::kHttpScheme));

    // Set up previews server with resource handler.
    previews_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    previews_server_->RegisterRequestHandler(base::BindRepeating(
        &PreviewsLitePageServerBrowserTest::HandleResourceRequest,
        base::Unretained(this)));
    ASSERT_TRUE(previews_server_->Start());

    // Set up the slow HTTP server with delayed resource handler.
    slow_http_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    slow_http_server_->RegisterRequestHandler(base::BindRepeating(
        &PreviewsLitePageServerBrowserTest::HandleSlowResourceRequest,
        base::Unretained(this)));
    ASSERT_TRUE(slow_http_server_->Start());

    std::unique_ptr<base::FeatureList> feature_list =
        std::make_unique<base::FeatureList>();
    {
      // The trial and group names are dummy values.
      scoped_refptr<base::FieldTrial> trial =
          base::FieldTrialList::CreateFieldTrial("TrialName1", "GroupName1");
      std::map<std::string, std::string> feature_parameters = {
          {"previews_host", previews_server().spec()},
          {"blacklisted_path_suffixes", ".mp4,.jpg"},
          {"trigger_on_localhost", "true"},
          {"navigation_timeout_milliseconds",
           use_timeout ? base::IntToString(kTimeoutMs) : "0"},
          {"control_group", is_control ? "true" : "false"}};
      base::FieldTrialParamAssociator::GetInstance()->AssociateFieldTrialParams(
          "TrialName1", "GroupName1", feature_parameters);

      feature_list->RegisterFieldTrialOverride(
          previews::features::kLitePageServerPreviews.name,
          base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
    }
    {
      // The trial and group names are dummy values.
      scoped_refptr<base::FieldTrial> trial =
          base::FieldTrialList::CreateFieldTrial("TrialName3", "GroupName3");
      feature_list->RegisterFieldTrialOverride(
          previews::features::kPreviews.name,
          base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
      feature_list->RegisterFieldTrialOverride(
          previews::features::kOptimizationHints.name,
          base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
      feature_list->RegisterFieldTrialOverride(
          previews::features::kResourceLoadingHints.name,
          base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
    }
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_2G);

    // Some tests shouldn't bother with the notification InfoBar. Just set the
    // state on the decider so it isn't required.
    PreviewsService* previews_service =
        PreviewsServiceFactory::GetForProfile(browser()->profile());
    PreviewsLitePageDecider* decider =
        previews_service->previews_lite_page_decider();
    decider->SetUserHasSeenUINotification();
  }

  content::WebContents* GetWebContents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  InfoBarService* GetInfoBarService() {
    return InfoBarService::FromWebContents(GetWebContents());
  }

  void ExecuteScript(const std::string& script) const {
    EXPECT_TRUE(content::ExecuteScript(GetWebContents(), script));
  }

  // Returns the loaded, non-virtual URL, of the current visible
  // NavigationEntry.
  GURL GetLoadedURL() const {
    return GetWebContents()->GetController().GetVisibleEntry()->GetURL();
  }

  void VerifyInfoStatus(previews::ServerLitePageStatus status) {
    PreviewsUITabHelper* ui_tab_helper =
        PreviewsUITabHelper::FromWebContents(GetWebContents());
    previews::PreviewsUserData* previews_data =
        ui_tab_helper->previews_user_data();

    EXPECT_TRUE(previews_data->server_lite_page_info());
    EXPECT_EQ(previews_data->server_lite_page_info()->status, status);
  }

  void VerifyPreviewLoaded() const {
    // The Virtual URL is set in a WebContentsObserver::OnFinishNavigation.
    // Since |ui_test_utils::NavigationToURL| uses the same signal to stop
    // waiting, there is sometimes a race condition between the two, causing
    // this validation to flake. Waiting for the load stop on the page will
    // ensure that the Virtual URL has been set.
    base::RunLoop().RunUntilIdle();
    content::WaitForLoadStop(GetWebContents());

    PreviewsUITabHelper* ui_tab_helper =
        PreviewsUITabHelper::FromWebContents(GetWebContents());
    EXPECT_TRUE(ui_tab_helper->displayed_preview_ui());

    previews::PreviewsUserData* previews_data =
        ui_tab_helper->previews_user_data();
    EXPECT_TRUE(previews_data->HasCommittedPreviewsType());
    EXPECT_EQ(previews_data->committed_previews_type(),
              previews::PreviewsType::LITE_PAGE_REDIRECT);

    const GURL loaded_url = GetLoadedURL();
    const GURL previews_host = previews_server();
    EXPECT_TRUE(loaded_url.DomainIs(previews_host.host()) &&
                loaded_url.EffectiveIntPort() ==
                    previews_host.EffectiveIntPort());

    content::NavigationEntry* entry =
        GetWebContents()->GetController().GetVisibleEntry();

    // server_lite_page_info does not exist on forward/back navigations.
    if (!(entry->GetTransitionType() & ui::PAGE_TRANSITION_FORWARD_BACK)) {
      EXPECT_TRUE(previews_data->server_lite_page_info());
      EXPECT_NE(
          previews_data->server_lite_page_info()->original_navigation_start,
          base::TimeTicks());
      EXPECT_NE(previews_data->server_lite_page_info()->page_id, 0U);
      EXPECT_NE(previews_data->server_lite_page_info()->drp_session_key, "");
    }

    EXPECT_EQ(content::PAGE_TYPE_NORMAL, entry->GetPageType());
    const GURL virtual_url = entry->GetVirtualURL();

    // The loaded url should be the previews version of the virtual url.
    EXPECT_EQ(
        loaded_url,
        PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(virtual_url));

    // The Virtual URL should not be on the previews server.
    // TODO(crbug.com/894854): Use a different hostname and check that here.
    EXPECT_FALSE(virtual_url.DomainIs(previews_host.host()) &&
                 virtual_url.EffectiveIntPort() ==
                     previews_host.EffectiveIntPort());
  }

  void VerifyPreviewNotLoaded() const {
    // The Virtual URL is set in a |WebContentsObserver::OnFinishNavigation|.
    // Since |ui_test_utils::NavigationToURL| uses the same signal to stop
    // waiting, there is sometimes a race condition between the two, causing
    // this validation to flake. Waiting for the load stop on the page will
    // ensure that the Virtual URL has been set.
    base::RunLoop().RunUntilIdle();
    content::WaitForLoadStop(GetWebContents());

    PreviewsUITabHelper* ui_tab_helper =
        PreviewsUITabHelper::FromWebContents(GetWebContents());
    EXPECT_FALSE(ui_tab_helper->displayed_preview_ui());

    previews::PreviewsUserData* previews_data =
        ui_tab_helper->previews_user_data();
    EXPECT_FALSE(previews_data->HasCommittedPreviewsType());
    EXPECT_NE(previews_data->committed_previews_type(),
              previews::PreviewsType::LITE_PAGE_REDIRECT);

    const GURL loaded_url = GetLoadedURL();
    const GURL previews_host = previews_server();
    EXPECT_FALSE(loaded_url.DomainIs(previews_host.host()) &&
                 loaded_url.EffectiveIntPort() ==
                     previews_host.EffectiveIntPort());

    content::NavigationEntry* entry =
        GetWebContents()->GetController().GetVisibleEntry();
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, entry->GetPageType());

    // The Virtual URL and the loaded URL should be the same.
    EXPECT_EQ(loaded_url, entry->GetVirtualURL());
  }

  void VerifyErrorPageLoaded() const {
    const GURL loaded_url = GetLoadedURL();
    const GURL previews_host = previews_server();
    EXPECT_FALSE(loaded_url.DomainIs(previews_host.host()) &&
                 loaded_url.EffectiveIntPort() ==
                     previews_host.EffectiveIntPort());

    content::NavigationEntry* entry =
        GetWebContents()->GetController().GetVisibleEntry();
    EXPECT_EQ(content::PAGE_TYPE_ERROR, entry->GetPageType());
  }

  void ResetDataSavings() const {
    DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
        browser()->profile())
        ->data_reduction_proxy_service()
        ->compression_stats()
        ->ResetStatistics();
  }

  // Gets the data usage recorded against the host the origin server runs on.
  uint64_t GetDataUsage() const {
    const auto& data_usage_map =
        DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
            browser()->profile())
            ->data_reduction_proxy_service()
            ->compression_stats()
            ->DataUsageMapForTesting();
    const auto& it =
        data_usage_map.find(https_server_->host_port_pair().host());
    if (it != data_usage_map.end())
      return it->second->data_used();
    return 0;
  }

  // Gets the data usage recorded against all hosts.
  uint64_t GetTotalDataUsage() const {
    return DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
               browser()->profile())
        ->data_reduction_proxy_service()
        ->compression_stats()
        ->GetHttpReceivedContentLength();
  }

  // Gets the original content length recorded against all hosts.
  uint64_t GetTotalOriginalContentLength() const {
    return DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
               browser()->profile())
        ->data_reduction_proxy_service()
        ->compression_stats()
        ->GetHttpOriginalContentLength();
  }

  // Returns a HTTP URL that will respond with the given action and headers when
  // used by the previews server. The response can be delayed a number of
  // milliseconds by passing a value > 0 for |delay_ms| or pass -1 to make the
  // response hang indefinitely.
  GURL HttpLitePageURL(PreviewsServerAction action,
                       std::string* headers = nullptr,
                       int delay_ms = 0) const {
    std::string query = "resp=" + base::IntToString(action);
    if (delay_ms != 0)
      query += "&delay_ms=" + base::IntToString(delay_ms);
    if (headers)
      query += "&headers=" + *headers;
    GURL::Replacements replacements;
    replacements.SetQuery(query.c_str(), url::Component(0, query.length()));
    return base_http_lite_page_url().ReplaceComponents(replacements);
  }

  // Returns a HTTPS URL that will respond with the given action and headers
  // when used by the previews server. The response can be delayed a number of
  // milliseconds by passing a value > 0 for |delay_ms| or pass -1 to make the
  // response hang indefinitely.
  GURL HttpsLitePageURL(PreviewsServerAction action,
                        std::string* headers = nullptr,
                        int delay_ms = 0) const {
    std::string query = "resp=" + base::IntToString(action);
    if (delay_ms != 0)
      query += "&delay_ms=" + base::IntToString(delay_ms);
    if (headers)
      query += "&headers=" + *headers;
    GURL::Replacements replacements;
    replacements.SetQuery(query.c_str(), url::Component(0, query.length()));
    return base_https_lite_page_url().ReplaceComponents(replacements);
  }

  void ClearDeciderState() const {
    PreviewsService* previews_service =
        PreviewsServiceFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(previews_service);
    PreviewsLitePageDecider* decider =
        previews_service->previews_lite_page_decider();
    ASSERT_TRUE(decider);
    decider->ClearStateForTesting();
  }

  virtual GURL previews_server() const { return previews_server_->base_url(); }

  const GURL& https_url() const { return https_url_; }
  const GURL& base_https_lite_page_url() const {
    return base_https_lite_page_url_;
  }
  const GURL& https_media_url() const { return https_media_url_; }
  const GURL& http_url() const { return http_url_; }
  const GURL& slow_http_url() const { return slow_http_server_->base_url(); }
  const GURL& base_http_lite_page_url() const {
    return base_http_lite_page_url_;
  }
  const GURL& http_to_https_redirect_url() const {
    return http_to_https_redirect_url_;
  }
  const GURL& https_to_https_redirect_url() const {
    return https_to_https_redirect_url_;
  }
  const GURL& client_redirect_url() const { return client_redirect_url_; }
  const GURL& subframe_url() const { return subframe_url_; }
  int subresources_requested() const { return subresources_requested_; }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRedirectRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().spec().find("to_https_redirect") !=
        std::string::npos) {
      std::unique_ptr<net::test_server::BasicHttpResponse> response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader("Location", https_url().spec());
      return std::move(response);
    }

    if (request.GetURL().spec().find("client_redirect") != std::string::npos) {
      std::unique_ptr<net::test_server::BasicHttpResponse> response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->set_content("<html><body><script>window.location = \"" +
                            HttpsLitePageURL(kSuccess).spec() +
                            "\";</script></body></html>");
      return std::move(response);
    }
    return nullptr;
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleSlowResourceRequest(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::DelayedHttpResponse> response =
        std::make_unique<net::test_server::DelayedHttpResponse>(
            base::TimeDelta::FromMilliseconds(500));
    response->set_code(net::HttpStatusCode::HTTP_OK);
    return std::move(response);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleResourceRequest(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();

    // If this request is for a subresource, record if the X-Client-Data header
    // exists.
    if (request.GetURL().spec().find("subresource.png") != std::string::npos) {
      if (request.headers.find("X-Client-Data") != request.headers.end()) {
        subresources_requested_++;
      }
      response->set_code(net::HTTP_OK);
      return response;
    }

    std::string original_url_str;

    // Ignore anything that's not a previews request with an unused status.
    if (!previews::ExtractOriginalURLFromLitePageRedirectURL(
            request.GetURL(), &original_url_str)) {
      response->set_code(net::HttpStatusCode::HTTP_BAD_REQUEST);
      return response;
    }
    GURL original_url = GURL(original_url_str);

    // Reject anything that doesn't have the DataSaver headers.
    const std::string want_headers[] = {"chrome-proxy-ect", "chrome-proxy"};
    for (const std::string& header : want_headers) {
      if (request.headers.find(header) == request.headers.end()) {
        response->set_code(
            net::HttpStatusCode::HTTP_PROXY_AUTHENTICATION_REQUIRED);
        return response;
      }
    }

    // The chrome-proxy header should have the pid option.
    if (request.headers.find("chrome-proxy")->second.find(", pid=") ==
        std::string::npos) {
      response->set_code(
          net::HttpStatusCode::HTTP_PROXY_AUTHENTICATION_REQUIRED);
      return response;
    }

    std::string delay_query_param;
    int delay_ms = 0;

    // Determine whether to delay the preview response using the |original_url|.
    if (net::GetValueForKeyInQuery(original_url, "delay_ms",
                                   &delay_query_param)) {
      base::StringToInt(delay_query_param, &delay_ms);
    }

    if (delay_ms == -1) {
      return std::make_unique<net::test_server::HungResponse>();
    }
    if (delay_ms > 0) {
      response = std::make_unique<net::test_server::DelayedHttpResponse>(
          base::TimeDelta::FromMilliseconds(delay_ms));
    }

    std::string code_query_param;
    int return_code = 0;
    if (net::GetValueForKeyInQuery(original_url, "resp", &code_query_param))
      base::StringToInt(code_query_param, &return_code);

    GURL subresource_url(
        "https://foo.litepages.googlezip.net:" +
        base::IntToString(previews_server().EffectiveIntPort()) +
        "/subresource.png");
    std::string subresource_body = "<html><body><img src=\"" +
                                   subresource_url.spec() +
                                   "\"/></body></html>";
    switch (return_code) {
      case kSuccess:
        response->set_code(net::HTTP_OK);
        response->set_content("porgporgporgporgporg" /* length = 20 */);
        response->AddCustomHeader("chrome-proxy", "ofcl=60");
        break;
      case kRedirectNonPreview:
        response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        response->AddCustomHeader("Location", HttpLitePageURL(kSuccess).spec());
        break;
      case kRedirectPreview:
        response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        response->AddCustomHeader("Location",
                                  HttpsLitePageURL(kSuccess).spec());
        break;
      case kBypass:
        response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        // This will not cause a redirect loop because on following this
        // redirect, the URL will no longer be a preview URL and the embedded
        // test server will respond with HTTP 400.
        response->AddCustomHeader("Location", HttpsLitePageURL(kBypass).spec());
        break;
      case kHostBlacklist:
        response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        // This will not cause a redirect loop because on following this
        // redirect, the URL will no longer be a preview URL and the embedded
        // test server will respond with HTTP 400.
        response->AddCustomHeader("Location",
                                  HttpsLitePageURL(kHostBlacklist).spec());
        response->AddCustomHeader("chrome-proxy", "host-blacklisted");
        break;
      case kAuthFailure:
        response->set_code(net::HTTP_FORBIDDEN);
        break;
      case kLoadshed:
        response->set_code(net::HTTP_SERVICE_UNAVAILABLE);
        break;
      case kSubresources:
        response->set_content_type("text/html");
        response->set_content(subresource_body);
        break;
      default:
        response->set_code(net::HTTP_OK);
        break;
    }

    std::string headers_query_param;
    if (net::GetValueForKeyInQuery(original_url, "headers",
                                   &headers_query_param)) {
      net::HttpRequestHeaders headers;
      headers.AddHeadersFromString(headers_query_param);
      net::HttpRequestHeaders::Iterator iter(headers);
      while (iter.GetNext())
        response->AddCustomHeader(iter.name(), iter.value());
    }
    return std::move(response);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> previews_server_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<net::EmbeddedTestServer> http_server_;
  std::unique_ptr<net::EmbeddedTestServer> slow_http_server_;
  GURL https_url_;
  GURL base_https_lite_page_url_;
  GURL https_media_url_;
  GURL http_url_;
  GURL base_http_lite_page_url_;
  GURL http_to_https_redirect_url_;
  GURL https_to_https_redirect_url_;
  GURL client_redirect_url_;
  GURL subframe_url_;
  int subresources_requested_ = 0;
};

// Previews InfoBar (which these tests trigger) does not work on Mac.
// See https://crbug.com/782322 for detail.
// Also occasional flakes on win7 (https://crbug.com/789542).
#if defined(OS_WIN) || defined(OS_MACOSX)
#define DISABLE_ON_WIN_MAC(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC(x) x
#endif

IN_PROC_BROWSER_TEST_F(PreviewsLitePageServerBrowserTest,
                       DISABLE_ON_WIN_MAC(LitePagePreviewsTriggering)) {
  // TODO(crbug.com/874150): Use ExpectUniqueSample in these tests.
  // The histograms in these tests can only be checked by the expected bucket,
  // and not by a unique sample. This is because each navigation to a preview
  // will cause two navigations and two records, one for the original navigation
  // under test, and another one for loading the preview.

  {
    // Verify the preview is not triggered on HTTP pageloads.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), HttpLitePageURL(kSuccess));
    VerifyPreviewNotLoaded();
    ClearDeciderState();
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.IneligibleReasons",
        PreviewsLitePageNavigationThrottle::IneligibleReason::kNonHttpsScheme,
        1);
    histogram_tester.ExpectBucketCount("Previews.ServerLitePage.Triggered",
                                       false, 1);
  }

  {
    // Verify the preview is triggered on HTTPS pageloads.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
    VerifyPreviewLoaded();
    VerifyInfoStatus(previews::ServerLitePageStatus::kSuccess);
    histogram_tester.ExpectBucketCount("Previews.ServerLitePage.Triggered",
                                       true, 1);
  }

  {
    // Verify the preview is not triggered when loading a media resource.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), https_media_url());
    VerifyPreviewNotLoaded();
    ClearDeciderState();
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.BlacklistReasons",
        PreviewsLitePageNavigationThrottle::BlacklistReason::
            kPathSuffixBlacklisted,
        1);
    histogram_tester.ExpectBucketCount("Previews.ServerLitePage.Triggered",
                                       false, 1);
  }

  {
    // Verify the preview is not triggered for POST navigations.
    base::HistogramTester histogram_tester;
    std::string post_data = "helloworld";
    NavigateParams params(browser(), https_url(), ui::PAGE_TRANSITION_LINK);
    params.window_action = NavigateParams::SHOW_WINDOW;
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    params.is_renderer_initiated = false;
    params.uses_post = true;
    params.post_data = network::ResourceRequestBody::CreateFromBytes(
        post_data.data(), post_data.size());
    ui_test_utils::NavigateToURL(&params);
    VerifyPreviewNotLoaded();
    ClearDeciderState();
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.IneligibleReasons",
        PreviewsLitePageNavigationThrottle::IneligibleReason::kHttpPost, 1);
    histogram_tester.ExpectBucketCount("Previews.ServerLitePage.Triggered",
                                       false, 1);
  }

  {
    // Verify the preview is not triggered when navigating to the previews
    // server.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), previews_server());
    EXPECT_EQ(GetLoadedURL(), previews_server());
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.BlacklistReasons",
        PreviewsLitePageNavigationThrottle::BlacklistReason::
            kNavigationToPreviewsDomain,
        1);
    histogram_tester.ExpectBucketCount("Previews.ServerLitePage.Triggered",
                                       false, 1);
  }

  {
    // Verify the preview is not triggered when navigating to a private IP.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), GURL("https://0.0.0.0/"));
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.BlacklistReasons",
        PreviewsLitePageNavigationThrottle::BlacklistReason::
            kNavigationToPrivateDomain,
        1);
    histogram_tester.ExpectBucketCount("Previews.ServerLitePage.Triggered",
                                       false, 1);
    VerifyErrorPageLoaded();
  }

  {
    // Verify the preview is not triggered when navigating to a domain without a
    // dot.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), GURL("https://no-dots-here/"));
    VerifyErrorPageLoaded();
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.BlacklistReasons",
        PreviewsLitePageNavigationThrottle::BlacklistReason::
            kNavigationToPrivateDomain,
        1);
    histogram_tester.ExpectBucketCount("Previews.ServerLitePage.Triggered",
                                       false, 1);
  }

  {
    // Verify a preview is only shown on slow networks.
    base::HistogramTester histogram_tester;
    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_3G);

    ui_test_utils::NavigateToURL(browser(), HttpLitePageURL(kSuccess));

    VerifyPreviewNotLoaded();
    ClearDeciderState();
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.IneligibleReasons",
        PreviewsLitePageNavigationThrottle::IneligibleReason::kNetworkNotSlow,
        1);
    histogram_tester.ExpectBucketCount("Previews.ServerLitePage.Triggered",
                                       false, 1);

    // Reset ECT for future tests.
    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_2G);
  }

  {
    // Verify a preview is not shown if cookies are blocked.
    base::HistogramTester histogram_tester;
    int before_subresources_requested = subresources_requested();
    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSubresources));
    VerifyPreviewLoaded();
    EXPECT_EQ(before_subresources_requested + 1, subresources_requested());

    CookieSettingsFactory::GetForProfile(browser()->profile())
        ->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSubresources));
    VerifyPreviewNotLoaded();
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.IneligibleReasons",
        PreviewsLitePageNavigationThrottle::IneligibleReason::kCookiesBlocked,
        1);

    // Reset state for other tests.
    CookieSettingsFactory::GetForProfile(browser()->profile())
        ->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);
  }
}

IN_PROC_BROWSER_TEST_F(PreviewsLitePageServerBrowserTest,
                       DISABLE_ON_WIN_MAC(LitePagePreviewsReload)) {
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewLoaded();
  VerifyInfoStatus(previews::ServerLitePageStatus::kSuccess);

  GetWebContents()->GetController().Reload(content::ReloadType::NORMAL, false);
  VerifyPreviewLoaded();
  VerifyInfoStatus(previews::ServerLitePageStatus::kSuccess);

  // Set the ECT so that a preview won't be triggered on the reload.
  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_4G);

  GetWebContents()->GetController().Reload(content::ReloadType::NORMAL, false);
  VerifyPreviewNotLoaded();

  // Reset ECT for future tests.
  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_2G);
}

IN_PROC_BROWSER_TEST_F(PreviewsLitePageServerBrowserTest,
                       DISABLE_ON_WIN_MAC(LitePagePreviewsLoadOriginal)) {
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewLoaded();
  VerifyInfoStatus(previews::ServerLitePageStatus::kSuccess);

  PreviewsUITabHelper::FromWebContents(GetWebContents())
      ->ReloadWithoutPreviews();
  VerifyPreviewNotLoaded();
}

IN_PROC_BROWSER_TEST_F(PreviewsLitePageServerBrowserTest,
                       DISABLE_ON_WIN_MAC(LitePagePreviewsRedirect)) {
  {
    // Verify the preview is triggered when an HTTP page redirects to HTTPS.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), http_to_https_redirect_url());
    VerifyPreviewLoaded();
    VerifyInfoStatus(previews::ServerLitePageStatus::kSuccess);
    histogram_tester.ExpectBucketCount("Previews.ServerLitePage.Triggered",
                                       true, 1);
  }

  {
    // Verify the preview is triggered when an HTTPS page redirects to HTTPS.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), https_to_https_redirect_url());
    VerifyPreviewLoaded();
    VerifyInfoStatus(previews::ServerLitePageStatus::kSuccess);
    histogram_tester.ExpectBucketCount("Previews.ServerLitePage.Triggered",
                                       true, 1);
  }

  {
    // Verify the preview is not triggered when the previews server redirects to
    // a non-preview page.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(),
                                 HttpsLitePageURL(kRedirectNonPreview));
    VerifyPreviewNotLoaded();
    VerifyInfoStatus(previews::ServerLitePageStatus::kRedirect);
    ClearDeciderState();
    histogram_tester.ExpectBucketCount("Previews.ServerLitePage.Triggered",
                                       true, 1);
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.ServerResponse",
        PreviewsLitePageNavigationThrottle::ServerResponse::kRedirect, 1);
  }

  {
    // Verify the preview is triggered when the previews server redirects to a
    // preview page.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kRedirectPreview));
    VerifyPreviewLoaded();
    VerifyInfoStatus(previews::ServerLitePageStatus::kSuccess);
    ClearDeciderState();
    histogram_tester.ExpectBucketCount("Previews.ServerLitePage.Triggered",
                                       true, 2);
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.ServerResponse",
        PreviewsLitePageNavigationThrottle::ServerResponse::kRedirect, 1);
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.ServerResponse",
        PreviewsLitePageNavigationThrottle::ServerResponse::kOk, 1);
  }
}

IN_PROC_BROWSER_TEST_F(PreviewsLitePageServerBrowserTest,
                       DISABLE_ON_WIN_MAC(LitePagePreviewsResponse)) {
  {
    // Verify the preview is not triggered when the server responds with bypass
    // 307.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kBypass));
    VerifyPreviewNotLoaded();
    VerifyInfoStatus(previews::ServerLitePageStatus::kBypass);
    ClearDeciderState();
    histogram_tester.ExpectBucketCount("Previews.ServerLitePage.Triggered",
                                       true, 1);
    histogram_tester.ExpectTotalCount(
        "Previews.ServerLitePage.HttpOnlyFallbackPenalty", 1);
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.ServerResponse",
        PreviewsLitePageNavigationThrottle::ServerResponse::kPreviewUnavailable,
        1);
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.HostBlacklistedOnBypass", false, 1);
  }

  {
    // Verify the preview is not triggered when the server responds with bypass
    // 307 and host-blacklist.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kHostBlacklist));
    VerifyPreviewNotLoaded();
    VerifyInfoStatus(previews::ServerLitePageStatus::kBypass);

    histogram_tester.ExpectBucketCount("Previews.ServerLitePage.Triggered",
                                       true, 1);
    histogram_tester.ExpectTotalCount(
        "Previews.ServerLitePage.HttpOnlyFallbackPenalty", 1);
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.ServerResponse",
        PreviewsLitePageNavigationThrottle::ServerResponse::kPreviewUnavailable,
        1);
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.HostBlacklistedOnBypass", true, 1);

    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
    VerifyPreviewNotLoaded();
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.BlacklistReasons",
        PreviewsLitePageNavigationThrottle::BlacklistReason::kHostBlacklisted,
        1);
    ClearDeciderState();

    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
    VerifyPreviewLoaded();
  }

  {
    // Verify the preview is not triggered when the server responds with 403.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kAuthFailure));
    VerifyPreviewNotLoaded();
    VerifyInfoStatus(previews::ServerLitePageStatus::kFailure);
    ClearDeciderState();
    histogram_tester.ExpectBucketCount("Previews.ServerLitePage.Triggered",
                                       true, 1);
    histogram_tester.ExpectTotalCount(
        "Previews.ServerLitePage.HttpOnlyFallbackPenalty", 1);
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.ServerResponse",
        PreviewsLitePageNavigationThrottle::ServerResponse::kAuthFailure, 1);
  }

  {
    // Verify the preview is not triggered when the server responds with 503.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kLoadshed));
    VerifyPreviewNotLoaded();
    VerifyInfoStatus(previews::ServerLitePageStatus::kFailure);
    ClearDeciderState();
    histogram_tester.ExpectBucketCount("Previews.ServerLitePage.Triggered",
                                       true, 1);
    histogram_tester.ExpectTotalCount(
        "Previews.ServerLitePage.HttpOnlyFallbackPenalty", 1);
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.ServerResponse",
        PreviewsLitePageNavigationThrottle::ServerResponse::kServiceUnavailable,
        1);
  }
}

IN_PROC_BROWSER_TEST_F(PreviewsLitePageServerBrowserTest,
                       DISABLE_ON_WIN_MAC(LitePagePreviewsLoadshed)) {
  PreviewsService* previews_service =
      PreviewsServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(previews_service);
  PreviewsLitePageDecider* decider =
      previews_service->previews_lite_page_decider();
  ASSERT_TRUE(decider);

  std::unique_ptr<base::SimpleTestTickClock> clock =
      std::make_unique<base::SimpleTestTickClock>();
  decider->SetClockForTesting(clock.get());

  // Send a loadshed response. Client should not retry for a randomly chosen
  // duration [1 min, 5 mins).
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kLoadshed));
  VerifyPreviewNotLoaded();
  ClearDeciderState();

  clock->Advance(base::TimeDelta::FromMinutes(1));
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewNotLoaded();
  ClearDeciderState();

  clock->Advance(base::TimeDelta::FromMinutes(4));
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewLoaded();

  // Send a loadshed response with a specific time duration, 30 seconds, to
  // retry after.
  std::string headers = "Retry-After: 30";
  ui_test_utils::NavigateToURL(browser(),
                               HttpsLitePageURL(kLoadshed, &headers));
  VerifyPreviewNotLoaded();
  ClearDeciderState();

  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewNotLoaded();
  ClearDeciderState();

  clock->Advance(base::TimeDelta::FromSeconds(31));
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewLoaded();
}

IN_PROC_BROWSER_TEST_F(PreviewsLitePageServerBrowserTest,
                       DISABLE_ON_WIN_MAC(LitePageURLNotReportedToHistory)) {
  base::CancelableTaskTracker tracker_;
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(browser()->profile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);

  // Verify the lite pages URL doesn't make it into the History Service via
  // the committed URL.
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewLoaded();
  {
    base::RunLoop loop;
    history_service->QueryURL(
        HttpsLitePageURL(kSuccess), false /* want_visits */,
        base::BindLambdaForTesting([&](bool success, const history::URLRow& row,
                                       const history::VisitVector&) {
          EXPECT_TRUE(success);
          loop.Quit();
        }),
        &tracker_);
    loop.Run();
  }
  {
    base::RunLoop loop;
    history_service->QueryURL(
        PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(
            HttpsLitePageURL(kSuccess)),
        false /* want_visits */,
        base::BindLambdaForTesting([&](bool success, const history::URLRow& row,
                                       const history::VisitVector&) {
          EXPECT_FALSE(success);
          loop.Quit();
        }),
        &tracker_);
    loop.Run();
  }

  // Verify the lite pages URL doesn't make it into the History Service via the
  // redirect chain.
  ui_test_utils::NavigateToURL(browser(),
                               HttpsLitePageURL(kRedirectNonPreview));
  VerifyPreviewNotLoaded();
  {
    base::RunLoop loop;
    history_service->QueryRedirectsFrom(
        HttpsLitePageURL(kRedirectNonPreview),
        base::BindLambdaForTesting([&](const history::RedirectList* redirects) {
          for (const GURL& url : *redirects) {
            EXPECT_FALSE(previews::IsLitePageRedirectPreviewURL(url));
          }
          loop.Quit();
        }),
        &tracker_);
    loop.Run();
  }
  ClearDeciderState();
}

IN_PROC_BROWSER_TEST_F(PreviewsLitePageServerBrowserTest,
                       DISABLE_ON_WIN_MAC(LitePagePreviewsReportSavings)) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(data_reduction_proxy::prefs::kDataUsageReportingEnabled,
                    true);
  // Give the setting notification a chance to propagate.
  base::RunLoop().RunUntilIdle();

  ResetDataSavings();

  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewLoaded();

  EXPECT_EQ(GetTotalOriginalContentLength() - GetTotalDataUsage(), 40U);
  EXPECT_EQ(GetDataUsage(), 20U);
}

IN_PROC_BROWSER_TEST_F(PreviewsLitePageServerBrowserTest,
                       DISABLE_ON_WIN_MAC(LitePagePreviewsClientRedirect)) {
  // Navigate to a non-preview first.
  ui_test_utils::NavigateToURL(browser(), https_media_url());
  VerifyPreviewNotLoaded();

  // Navigate to a page that causes a client redirect to a page that will
  // get a preview.
  ui_test_utils::NavigateToURL(browser(), client_redirect_url());
  VerifyPreviewLoaded();
  EXPECT_EQ(GetWebContents()->GetController().GetEntryAtOffset(-1)->GetURL(),
            https_media_url());
}

IN_PROC_BROWSER_TEST_F(PreviewsLitePageServerBrowserTest,
                       DISABLE_ON_WIN_MAC(LitePagePreviewsNavigation)) {
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewLoaded();

  // Go to a new page that doesn't Preview.
  ui_test_utils::NavigateToURL(browser(), http_url());
  VerifyPreviewNotLoaded();
  ClearDeciderState();

  // Note: |VerifyPreviewLoaded| calls |content::WaitForLoadStop()| so these are
  // safe.

  // Navigate back.
  GetWebContents()->GetController().GoBack();
  VerifyPreviewLoaded();

  // Navigate forward.
  GetWebContents()->GetController().GoForward();
  VerifyPreviewNotLoaded();
  ClearDeciderState();

  // Navigate back again.
  GetWebContents()->GetController().GoBack();
  VerifyPreviewLoaded();
}

class PreviewsLitePageServerTimeoutBrowserTest
    : public PreviewsLitePageServerBrowserTest {
 public:
  PreviewsLitePageServerTimeoutBrowserTest() = default;

  ~PreviewsLitePageServerTimeoutBrowserTest() override = default;

  void SetUp() override {
    SetUpLitePageTest(true /* use_timeout */, false /* is_control */);

    InProcessBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(PreviewsLitePageServerTimeoutBrowserTest,
                       DISABLE_ON_WIN_MAC(LitePagePreviewsTimeout)) {
  {
    // Ensure that a hung previews navigation doesn't wind up at the previews
    // server.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(),
                                 HttpsLitePageURL(kSuccess, nullptr, -1));
    VerifyPreviewNotLoaded();
    VerifyInfoStatus(previews::ServerLitePageStatus::kFailure);
    ClearDeciderState();
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.ServerResponse",
        PreviewsLitePageNavigationThrottle::ServerResponse::kTimeout, 1);
  }

  {
    // Ensure that a hung normal navigation eventually loads.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), slow_http_url());
    VerifyPreviewNotLoaded();
    ClearDeciderState();
    histogram_tester.ExpectTotalCount("Previews.ServerLitePage.ServerResponse",
                                      0);
  }
}

class PreviewsLitePageServerBadServerBrowserTest
    : public PreviewsLitePageServerBrowserTest {
 public:
  PreviewsLitePageServerBadServerBrowserTest() = default;

  ~PreviewsLitePageServerBadServerBrowserTest() override = default;

  // Override the previews_server URL so that a bad value will be configured.
  GURL previews_server() const override {
    return GURL("https://bad-server.com");
  }
};

IN_PROC_BROWSER_TEST_F(PreviewsLitePageServerBadServerBrowserTest,
                       DISABLE_ON_WIN_MAC(LitePagePreviewsBadServer)) {
  // TODO(crbug.com/874150): Use ExpectUniqueSample in this tests.
  // The histograms in this tests can only be checked by the expected bucket,
  // and not by a unique sample. This is because each navigation to a preview
  // will cause two navigations and two records, one for the original navigation
  // under test, and another one for loading the preview.

  {
    // Verify the preview is not shown on a bad previews server.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
    VerifyPreviewNotLoaded();
    VerifyInfoStatus(previews::ServerLitePageStatus::kFailure);
    ClearDeciderState();

    histogram_tester.ExpectBucketCount("Previews.ServerLitePage.Triggered",
                                       true, 1);
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.ServerResponse",
        PreviewsLitePageNavigationThrottle::ServerResponse::kFailed, 1);
  }
}

class PreviewsLitePageServerDataSaverBrowserTest
    : public PreviewsLitePageServerBrowserTest {
 public:
  PreviewsLitePageServerDataSaverBrowserTest() = default;

  ~PreviewsLitePageServerDataSaverBrowserTest() override = default;

  // Overrides the cmd line in PreviewsLitePageServerBrowserTest and leave out
  // the flag to enable DataSaver.
  void SetUpCommandLine(base::CommandLine* cmd) override {
    // Due to race conditions, it's possible that blacklist data is not loaded
    // at the time of first navigation. That may prevent Preview from
    // triggering, and causing the test to flake.
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlacklist);
    cmd->AppendSwitchASCII("force-effective-connection-type", "Slow-2G");
    // Resolve all localhost subdomains to plain localhost so that Chrome's Test
    // DNS resolver doesn't get upset.
    cmd->AppendSwitchASCII(
        "host-rules", "MAP *.localhost 127.0.0.1, MAP *.127.0.0.1 127.0.0.1");
  }
};

IN_PROC_BROWSER_TEST_F(PreviewsLitePageServerDataSaverBrowserTest,
                       DISABLE_ON_WIN_MAC(LitePagePreviewsDSTriggering)) {
  // Verify the preview is not triggered on HTTPS pageloads without DataSaver.
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewNotLoaded();
  ClearDeciderState();
}

class PreviewsLitePageServerNoDataSaverHeaderBrowserTest
    : public PreviewsLitePageServerBrowserTest {
 public:
  PreviewsLitePageServerNoDataSaverHeaderBrowserTest() = default;

  ~PreviewsLitePageServerNoDataSaverHeaderBrowserTest() override = default;

  // Overrides the command line in PreviewsLitePageServerBrowserTest to leave
  // out the flag that manually adds the chrome-proxy header.
  void SetUpCommandLine(base::CommandLine* cmd) override {
    // Due to race conditions, it's possible that blacklist data is not loaded
    // at the time of first navigation. That may prevent Preview from
    // triggering, and causing the test to flake.
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlacklist);
    cmd->AppendSwitch("enable-spdy-proxy-auth");
    cmd->AppendSwitchASCII("force-effective-connection-type", "Slow-2G");
    // Resolve all localhost subdomains to plain localhost so that Chrome's Test
    // DNS resolver doesn't get upset.
    cmd->AppendSwitchASCII(
        "host-rules", "MAP *.localhost 127.0.0.1, MAP *.127.0.0.1 127.0.0.1");
  }
};

IN_PROC_BROWSER_TEST_F(
    PreviewsLitePageServerNoDataSaverHeaderBrowserTest,
    DISABLE_ON_WIN_MAC(LitePagePreviewsDSNoHeaderTriggering)) {
  // Verify the preview is not triggered on HTTPS pageloads without data saver.
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewNotLoaded();
  ClearDeciderState();
}

class PreviewsLitePageNotificationDSEnabledBrowserTest
    : public PreviewsLitePageServerBrowserTest {
 public:
  PreviewsLitePageNotificationDSEnabledBrowserTest() = default;

  ~PreviewsLitePageNotificationDSEnabledBrowserTest() override = default;

  void SetUp() override {
    SetUpLitePageTest(false /* use_timeout */, false /* is_control */);

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_2G);
  }
};

IN_PROC_BROWSER_TEST_F(
    PreviewsLitePageNotificationDSEnabledBrowserTest,
    DISABLE_ON_WIN_MAC(LitePagePreviewsInfoBarDataSaverUser)) {
  // Ensure the preview is not shown the first time before the infobar is shown
  // for users who have DRP enabled.
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));

  VerifyPreviewNotLoaded();
  ClearDeciderState();
  ASSERT_EQ(1U, GetInfoBarService()->infobar_count());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_LITE_PAGE_PREVIEWS_MESSAGE),
            static_cast<ConfirmInfoBarDelegate*>(
                GetInfoBarService()->infobar_at(0)->delegate())
                ->GetMessageText());
  histogram_tester.ExpectBucketCount(
      "Previews.ServerLitePage.IneligibleReasons",
      PreviewsLitePageNavigationThrottle::IneligibleReason::kInfoBarNotSeen, 1);

  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  // Expect the "Saved Data" InfoBar.
  ASSERT_EQ(1U, GetInfoBarService()->infobar_count());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PREVIEWS_INFOBAR_SAVED_DATA_TITLE),
            static_cast<ConfirmInfoBarDelegate*>(
                GetInfoBarService()->infobar_at(0)->delegate())
                ->GetMessageText());
  VerifyPreviewLoaded();
}

class PreviewsLitePageNotificationDSDisabledBrowserTest
    : public PreviewsLitePageServerBrowserTest {
 public:
  PreviewsLitePageNotificationDSDisabledBrowserTest() = default;

  ~PreviewsLitePageNotificationDSDisabledBrowserTest() override = default;

  void SetUp() override {
    SetUpLitePageTest(false /* use_timeout */, false /* is_control */);

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_2G);
  }

  // Overrides the cmd line in PreviewsLitePageServerBrowserTest and leave out
  // the flag to enable DataSaver.
  void SetUpCommandLine(base::CommandLine* cmd) override {
    // Due to race conditions, it's possible that blacklist data is not loaded
    // at the time of first navigation. That may prevent Preview from
    // triggering, and causing the test to flake.
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlacklist);
    cmd->AppendSwitchASCII("force-effective-connection-type", "Slow-2G");
    // Resolve all localhost subdomains to plain localhost so that Chrome's Test
    // DNS resolver doesn't get upset.
    cmd->AppendSwitchASCII(
        "host-rules", "MAP *.localhost 127.0.0.1, MAP *.127.0.0.1 127.0.0.1");
  }
};

IN_PROC_BROWSER_TEST_F(
    PreviewsLitePageNotificationDSDisabledBrowserTest,
    DISABLE_ON_WIN_MAC(LitePagePreviewsInfoBarNonDataSaverUser)) {
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewNotLoaded();
  ClearDeciderState();
  EXPECT_EQ(0U, GetInfoBarService()->infobar_count());
}

class PreviewsLitePageControlBrowserTest
    : public PreviewsLitePageServerBrowserTest {
 public:
  PreviewsLitePageControlBrowserTest() = default;

  ~PreviewsLitePageControlBrowserTest() override = default;

  void SetUp() override {
    SetUpLitePageTest(false /* use_timeout */, true /* is_control */);

    InProcessBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(PreviewsLitePageControlBrowserTest,
                       DISABLE_ON_WIN_MAC(LitePagePreviewsControlGroup)) {
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewNotLoaded();
  VerifyInfoStatus(previews::ServerLitePageStatus::kControl);
  ClearDeciderState();
}

class PreviewsLitePageAndPageHintsBrowserTest
    : public PreviewsLitePageServerBrowserTest {
 public:
  PreviewsLitePageAndPageHintsBrowserTest() = default;

  ~PreviewsLitePageAndPageHintsBrowserTest() override = default;

  void ProcessHintsComponent(
      const optimization_guide::HintsComponentInfo& component_info) {
    base::HistogramTester histogram_tester;

    g_browser_process->optimization_guide_service()->MaybeUpdateHintsComponent(
        component_info);

    RetryForHistogramUntilCountReached(
        &histogram_tester,
        previews::kPreviewsOptimizationGuideUpdateHintsResultHistogramString,
        1);
  }

  void SetResourceLoadingHints(const std::vector<std::string>& hints_sites) {
    std::vector<std::string> resource_patterns;
    resource_patterns.push_back("foo.jpg");
    resource_patterns.push_back("png");
    resource_patterns.push_back("woff2");

    ProcessHintsComponent(
        test_hints_component_creator_.CreateHintsComponentInfoWithPageHints(
            optimization_guide::proto::RESOURCE_LOADING, hints_sites,
            resource_patterns));
  }

 private:
  // Retries fetching |histogram_name| until it contains at least |count|
  // samples.
  void RetryForHistogramUntilCountReached(
      base::HistogramTester* histogram_tester,
      const std::string& histogram_name,
      size_t count) {
    while (true) {
      base::TaskScheduler::GetInstance()->FlushForTesting();
      base::RunLoop().RunUntilIdle();

      content::FetchHistogramsFromChildProcesses();
      SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

      const std::vector<base::Bucket> buckets =
          histogram_tester->GetAllSamples(histogram_name);
      size_t total_count = 0;
      for (const auto& bucket : buckets) {
        total_count += bucket.count;
      }
      if (total_count >= count) {
        break;
      }
    }
  }

  optimization_guide::testing::TestHintsComponentCreator
      test_hints_component_creator_;
};

IN_PROC_BROWSER_TEST_F(
    PreviewsLitePageAndPageHintsBrowserTest,
    DISABLE_ON_WIN_MAC(LitePagePreviewsDoesNotOverridePageHints)) {
  // Whitelist test URL for resource loading hints.
  GURL url = HttpsLitePageURL(kSuccess);
  SetResourceLoadingHints({url.host()});

  ui_test_utils::NavigateToURL(browser(), url);

  base::RunLoop().RunUntilIdle();
  content::WaitForLoadStop(GetWebContents());

  // Verify the committed previews type is resource loading hints.
  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(GetWebContents());
  EXPECT_TRUE(ui_tab_helper->displayed_preview_ui());
  previews::PreviewsUserData* previews_data =
      ui_tab_helper->previews_user_data();
  EXPECT_TRUE(previews_data->HasCommittedPreviewsType());
  EXPECT_EQ(previews_data->committed_previews_type(),
            previews::PreviewsType::RESOURCE_LOADING_HINTS);

  ClearDeciderState();
}
