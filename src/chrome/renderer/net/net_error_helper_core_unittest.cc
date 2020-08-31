// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/net_error_helper_core.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/timer/mock_timer.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/available_offline_content.mojom.h"
#include "chrome/renderer/net/available_offline_content_helper.h"
#include "components/error_page/common/error.h"
#include "components/error_page/common/error_page_params.h"
#include "components/error_page/common/net_error_info.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/mock_render_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/net_errors.h"
#include "net/dns/public/resolve_error_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chrome/common/offline_page_auto_fetcher.mojom.h"
#endif

namespace {

const char kFailedUrl[] = "http://failed/";
const char kFailedHttpsUrl[] = "https://failed/";

const char kNavigationCorrectionUrl[] = "http://navigation.corrections/";
const char kLanguage[] = "en";
const char kCountry[] = "us";
const char kApiKey[] = "api_key";
const char kSearchUrl[] = "http://www.google.com/search";

const char kSuggestedSearchTerms[] = "Happy Goats";
const char kNavigationCorrectionEventId[] = "#007";
const char kNavigationCorrectionFingerprint[] = "RandumStuff";

struct NavigationCorrection {
  const char* correction_type;
  const char* url_correction;
  const char* click_type;
  const char* click_data;
  bool is_porn;
  bool is_soft_porn;

  std::unique_ptr<base::DictionaryValue> ToValue() const {
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    dict->SetString("correctionType", correction_type);
    dict->SetString("urlCorrection", url_correction);
    dict->SetString("clickType", click_type);
    dict->SetString("clickData", click_data);
    dict->SetBoolean("isPorn", is_porn);
    dict->SetBoolean("isSoftPorn", is_soft_porn);
    return dict;
  }
};

const NavigationCorrection kDefaultCorrections[] = {
    {"reloadPage", kFailedUrl, "rld", "data1", false, false},
    {"urlCorrection", "http://somewhere_else/", "btn", "data2", false, false},
    {"contentOverlap", "http://pony_island/", "btn", "data3", false, false},

    // Porn should be ignored.
    {"emphasizedUrlCorrection", "http://porn/", "btn", "data4", true, false},
    {"sitemap", "http://more_porn/", "btn", "data5", false, true},

    {"webSearchQuery", kSuggestedSearchTerms, "frm", "data6", false, false},
};

std::string SuggestionsToResponse(const NavigationCorrection* corrections,
                                  int num_corrections) {
  auto url_corrections = std::make_unique<base::ListValue>();
  for (int i = 0; i < num_corrections; ++i)
    url_corrections->Append(corrections[i].ToValue());

  base::DictionaryValue response;
  response.Set("result.UrlCorrections", std::move(url_corrections));
  response.SetString("result.eventId", kNavigationCorrectionEventId);
  response.SetString("result.fingerprint", kNavigationCorrectionFingerprint);

  std::string json;
  base::JSONWriter::Write(response, &json);
  return json;
}

testing::AssertionResult StringValueEquals(const base::DictionaryValue& dict,
                                           const std::string& key,
                                           const std::string& expected_value) {
  std::string actual_value;
  if (!dict.GetString(key, &actual_value))
    return testing::AssertionFailure() << key << " not found.";
  if (actual_value != expected_value) {
    return testing::AssertionFailure()
           << "actual: " << actual_value << "\n expected: " << expected_value;
  }
  return testing::AssertionSuccess();
}

// Creates a string from an error that is used as a mock locally generated
// error page for that error.
std::string ErrorToString(const error_page::Error& error, bool is_failed_post) {
  std::ostringstream ss;
  ss << "(" << error.url() << ", " << error.domain() << ", " << error.reason()
     << ", " << (is_failed_post ? "POST" : "NOT POST") << ")";
  return ss.str();
}

error_page::Error ProbeError(error_page::DnsProbeStatus status) {
  return error_page::Error::DnsProbeError(GURL(kFailedUrl), status, false);
}

error_page::Error ProbeErrorForURL(error_page::DnsProbeStatus status,
                                   const GURL& url) {
  return error_page::Error::DnsProbeError(url, status, false);
}

error_page::Error NetErrorForURL(net::Error net_error, const GURL& url) {
  return error_page::Error::NetError(url, net_error,
                                     net::ResolveErrorInfo(net::OK), false);
}

error_page::Error NetError(net::Error net_error) {
  return error_page::Error::NetError(GURL(kFailedUrl), net_error,
                                     net::ResolveErrorInfo(net::OK), false);
}

// Convenience functions that create an error string for a non-POST request.

std::string ProbeErrorString(error_page::DnsProbeStatus status) {
  return ErrorToString(ProbeError(status), false);
}

std::string NetErrorStringForURL(net::Error net_error, const GURL& url) {
  return ErrorToString(NetErrorForURL(net_error, url), false);
}

std::string NetErrorString(net::Error net_error) {
  return ErrorToString(NetError(net_error), false);
}

class NetErrorHelperCoreTest : public testing::Test,
                               public NetErrorHelperCore::Delegate {
 public:
  NetErrorHelperCoreTest()
      : timer_(NULL),
        update_count_(0),
        error_html_update_count_(0),
        reload_count_(0),
        diagnose_error_count_(0),
        download_count_(0),
        list_visible_by_prefs_(true),
        enable_page_helper_functions_count_(0),
        default_url_(GURL(kFailedUrl)),
        error_url_(GURL(content::kUnreachableWebDataURL)),
        tracking_request_count_(0) {
    SetUpCore(false, true);
  }

  ~NetErrorHelperCoreTest() override {
    // No test finishes while an error page is being fetched.
    EXPECT_FALSE(is_url_being_fetched());
  }

  void SetUpCore(bool auto_reload_enabled,
                 bool visible) {
    // The old value of timer_, if any, will be freed by the old core_ being
    // destructed, since core_ takes ownership of the timer.
    timer_ = new base::MockOneShotTimer();
    core_.reset(new NetErrorHelperCore(this, auto_reload_enabled, visible));
    core_->set_timer_for_testing(base::WrapUnique(timer_));
  }

  NetErrorHelperCore* core() { return core_.get(); }

  const GURL& url_being_fetched() const { return url_being_fetched_; }
  bool is_url_being_fetched() const { return !url_being_fetched_.is_empty(); }

  int reload_count() const { return reload_count_; }

  int diagnose_error_count() const { return diagnose_error_count_; }

  const GURL& diagnose_error_url() const { return diagnose_error_url_; }

  int download_count() const { return download_count_; }

  const GURL& default_url() const { return default_url_; }

  const GURL& error_url() const { return error_url_; }

  int enable_page_helper_functions_count() const {
    return enable_page_helper_functions_count_;
  }

  const std::string& last_update_string() const { return last_update_string_; }
  int update_count() const { return update_count_; }

  const std::string& last_error_html() const { return last_error_html_; }
  int error_html_update_count() const { return error_html_update_count_; }

  bool last_can_show_network_diagnostics_dialog() const {
    return last_can_show_network_diagnostics_dialog_;
  }

  const error_page::ErrorPageParams* last_error_page_params() const {
    return last_error_page_params_.get();
  }

  const GURL& last_tracking_url() const { return last_tracking_url_; }
  const std::string& last_tracking_request_body() const {
    return last_tracking_request_body_;
  }
  int tracking_request_count() const { return tracking_request_count_; }

  void set_offline_content_feature_enabled(
      bool offline_content_feature_enabled) {
    offline_content_feature_enabled_ = offline_content_feature_enabled;
  }

  bool list_visible_by_prefs() const { return list_visible_by_prefs_; }

  void set_auto_fetch_allowed(bool allowed) { auto_fetch_allowed_ = allowed; }

  void set_is_offline_error(bool is_offline_error) {
    is_offline_error_ = is_offline_error;
  }

  const std::string& offline_content_json() const {
    return offline_content_json_;
  }

  const std::string& offline_content_summary_json() const {
    return offline_content_summary_json_;
  }

#if defined(OS_ANDROID)
  // State of auto fetch, as reported to Delegate. Unset if SetAutoFetchState
  // was not called.
  base::Optional<chrome::mojom::OfflinePageAutoFetcherScheduleResult>
  auto_fetch_state() const {
    return auto_fetch_state_;
  }
#endif

  base::MockOneShotTimer* timer() { return timer_; }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }
  content::MockRenderThread* render_thread() { return &render_thread_; }

  void NavigationCorrectionsLoadSuccess(const NavigationCorrection* corrections,
                                        int num_corrections) {
    NavigationCorrectionsLoadFinished(
        SuggestionsToResponse(corrections, num_corrections));
  }

  void NavigationCorrectionsLoadFailure() {
    NavigationCorrectionsLoadFinished("");
  }

  void NavigationCorrectionsLoadFinished(const std::string& result) {
    url_being_fetched_ = GURL();
    core()->OnNavigationCorrectionsFetched(result, false);
  }

  void DoErrorLoadOfURL(net::Error error, const GURL& url) {
    core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                        NetErrorHelperCore::NON_ERROR_PAGE);
    std::string html;
    core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                             NetErrorForURL(error, url),
                             false /* is_failed_post */, &html);
    EXPECT_FALSE(html.empty());
    EXPECT_EQ(NetErrorStringForURL(error, url), html);

    core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                        NetErrorHelperCore::ERROR_PAGE);
    core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
    core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  }

  void DoErrorLoad(net::Error error) {
    DoErrorLoadOfURL(error, GURL(kFailedUrl));
  }

  void DoSuccessLoad() {
    core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                        NetErrorHelperCore::NON_ERROR_PAGE);
    core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, default_url());
    core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  }

  void DoDnsProbe(error_page::DnsProbeStatus final_status) {
    core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
    core()->OnNetErrorInfo(final_status);
  }

  void EnableNavigationCorrections() {
    SetNavigationCorrectionURL(GURL(kNavigationCorrectionUrl));
  }

  void DisableNavigationCorrections() { SetNavigationCorrectionURL(GURL()); }

  void ExpectDefaultNavigationCorrections() const {
    // Checks that the last error page params correspond to kDefaultSuggestions.
    ASSERT_TRUE(last_error_page_params());
    EXPECT_TRUE(last_error_page_params()->suggest_reload);
    EXPECT_EQ(1u, last_error_page_params()->override_suggestions->GetSize());
    EXPECT_EQ(GURL(kSearchUrl), last_error_page_params()->search_url);
    EXPECT_EQ(kSuggestedSearchTerms, last_error_page_params()->search_terms);
  }

 private:
  void SetNavigationCorrectionURL(const GURL& navigation_correction_url) {
    core()->OnSetNavigationCorrectionInfo(navigation_correction_url, kLanguage,
                                          kCountry, kApiKey, GURL(kSearchUrl));
  }
  error_page::LocalizedError::PageState GetPageState() const {
    error_page::LocalizedError::PageState result;
    result.auto_fetch_allowed = auto_fetch_allowed_;
    result.offline_content_feature_enabled = offline_content_feature_enabled_;
    result.is_offline_error = is_offline_error_;
    return result;
  }

  // NetErrorHelperCore::Delegate implementation:
  error_page::LocalizedError::PageState GenerateLocalizedErrorPage(
      const error_page::Error& error,
      bool is_failed_post,
      bool can_show_network_diagnostics_dialog,
      std::unique_ptr<error_page::ErrorPageParams> params,
      std::string* html) const override {
    last_can_show_network_diagnostics_dialog_ =
        can_show_network_diagnostics_dialog;
    last_error_page_params_ = std::move(params);
    *html = ErrorToString(error, is_failed_post);

    return GetPageState();
  }

  void LoadErrorPage(const std::string& html, const GURL& failed_url) override {
    error_html_update_count_++;
    last_error_html_ = html;
  }

  void EnablePageHelperFunctions() override {
    enable_page_helper_functions_count_++;
  }

  error_page::LocalizedError::PageState UpdateErrorPage(
      const error_page::Error& error,
      bool is_failed_post,
      bool can_show_network_diagnostics_dialog) override {
    update_count_++;
    last_can_show_network_diagnostics_dialog_ =
        can_show_network_diagnostics_dialog;
    last_error_page_params_.reset(nullptr);
    last_error_html_ = ErrorToString(error, is_failed_post);

    return GetPageState();
  }

  void InitializeErrorPageEasterEggHighScore(int high_score) override {}
  void RequestEasterEggHighScore() override {}

  void FetchNavigationCorrections(
      const GURL& navigation_correction_url,
      const std::string& navigation_correction_request_body) override {
    EXPECT_TRUE(url_being_fetched_.is_empty());
    EXPECT_TRUE(request_body_.empty());
    EXPECT_EQ(GURL(kNavigationCorrectionUrl), navigation_correction_url);

    url_being_fetched_ = navigation_correction_url;
    request_body_ = navigation_correction_request_body;

    // Check the body of the request.

    base::Optional<base::Value> parsed_body =
        base::JSONReader::Read(navigation_correction_request_body);
    ASSERT_TRUE(parsed_body);
    base::DictionaryValue* dict = NULL;
    ASSERT_TRUE(parsed_body->GetAsDictionary(&dict));

    EXPECT_TRUE(StringValueEquals(*dict, "params.urlQuery", kFailedUrl));
    EXPECT_TRUE(StringValueEquals(*dict, "params.language", kLanguage));
    EXPECT_TRUE(StringValueEquals(*dict, "params.originCountry", kCountry));
    EXPECT_TRUE(StringValueEquals(*dict, "params.key", kApiKey));
  }

  void CancelFetchNavigationCorrections() override {
    url_being_fetched_ = GURL();
    request_body_.clear();
  }

  void ReloadFrame() override { reload_count_++; }

  void DiagnoseError(const GURL& page_url) override {
    diagnose_error_count_++;
    diagnose_error_url_ = page_url;
  }

  void DownloadPageLater() override { download_count_++; }

  void SetIsShowingDownloadButton(bool show) override {}

  void OfflineContentAvailable(
      bool list_visible_by_prefs,
      const std::string& offline_content_json) override {
    list_visible_by_prefs_ = list_visible_by_prefs;
    offline_content_json_ = offline_content_json;
  }

#if defined(OS_ANDROID)
  void SetAutoFetchState(
      chrome::mojom::OfflinePageAutoFetcherScheduleResult result) override {
    auto_fetch_state_ = result;
  }
#endif

  void SendTrackingRequest(const GURL& tracking_url,
                           const std::string& tracking_request_body) override {
    last_tracking_url_ = tracking_url;
    last_tracking_request_body_ = tracking_request_body;
    tracking_request_count_++;

    // Check the body of the request.

    base::Optional<base::Value> parsed_body =
        base::JSONReader::Read(tracking_request_body);
    ASSERT_TRUE(parsed_body);
    base::DictionaryValue* dict = NULL;
    ASSERT_TRUE(parsed_body->GetAsDictionary(&dict));

    EXPECT_TRUE(
        StringValueEquals(*dict, "params.originalUrlQuery", kFailedUrl));
    EXPECT_TRUE(StringValueEquals(*dict, "params.language", kLanguage));
    EXPECT_TRUE(StringValueEquals(*dict, "params.originCountry", kCountry));
    EXPECT_TRUE(StringValueEquals(*dict, "params.key", kApiKey));
  }

  content::RenderFrame* GetRenderFrame() override { return nullptr; }

  base::MockOneShotTimer* timer_;

  base::test::TaskEnvironment task_environment_;
  content::MockRenderThread render_thread_;

  std::unique_ptr<NetErrorHelperCore> core_;

  GURL url_being_fetched_;
  std::string request_body_;

  // Contains the information passed to the last call to UpdateErrorPage, as a
  // string.
  std::string last_update_string_;
  // Number of times |last_update_string_| has been changed.
  int update_count_;

  // Contains the HTML set by the last call to LoadErrorPageInMainFrame.
  std::string last_error_html_;
  // Number of times |last_error_html_| has been changed.
  int error_html_update_count_;

  // Values passed in to the last call of GenerateLocalizedErrorPage or
  // UpdateErrorPage.  Mutable because GenerateLocalizedErrorPage is const.
  mutable bool last_can_show_network_diagnostics_dialog_;
  mutable std::unique_ptr<error_page::ErrorPageParams> last_error_page_params_;

  int reload_count_;
  int diagnose_error_count_;
  GURL diagnose_error_url_;
  int download_count_;
  bool list_visible_by_prefs_;
  std::string offline_content_json_;
  std::string offline_content_summary_json_;
#if defined(OS_ANDROID)
  base::Optional<chrome::mojom::OfflinePageAutoFetcherScheduleResult>
      auto_fetch_state_;
#endif
  bool offline_content_feature_enabled_ = false;
  bool is_offline_error_ = false;
  bool auto_fetch_allowed_ = false;

  int enable_page_helper_functions_count_;

  const GURL default_url_;
  const GURL error_url_;

  GURL last_tracking_url_;
  std::string last_tracking_request_body_;
  int tracking_request_count_;
};

//------------------------------------------------------------------------------
// Basic tests that don't update the error page for probes or load navigation
// corrections.
//------------------------------------------------------------------------------

TEST_F(NetErrorHelperCoreTest, Null) {}

TEST_F(NetErrorHelperCoreTest, SuccessfulPageLoad) {
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, default_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

TEST_F(NetErrorHelperCoreTest, SuccessfulPageLoadWithNavigationCorrections) {
  EnableNavigationCorrections();
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, default_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

TEST_F(NetErrorHelperCoreTest, MainFrameNonDnsError) {
  // Original page starts loading.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_CONNECTION_RESET),
                           false /* is_failed_post */, &html);
  // Should have returned a local error page.
  EXPECT_FALSE(html.empty());
  EXPECT_EQ(NetErrorString(net::ERR_CONNECTION_RESET), html);

  // Error page loads.
  EXPECT_EQ(0, enable_page_helper_functions_count());
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, error_html_update_count());
  EXPECT_EQ(1, enable_page_helper_functions_count());
}

TEST_F(NetErrorHelperCoreTest, MainFrameNonDnsErrorWithCorrections) {
  EnableNavigationCorrections();

  // Original page starts loading.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_CONNECTION_RESET),
                           false /* is_failed_post */, &html);
  // Should have returned a local error page.
  EXPECT_FALSE(html.empty());
  EXPECT_EQ(NetErrorString(net::ERR_CONNECTION_RESET), html);

  // Error page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

// Much like above tests, but with a bunch of spurious DNS status messages that
// should have no effect.
TEST_F(NetErrorHelperCoreTest, MainFrameNonDnsErrorSpuriousStatus) {
  // Original page starts loading.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);

  // It fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_CONNECTION_RESET),
                           false /* is_failed_post */, &html);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);

  // Should have returned a local error page.
  EXPECT_FALSE(html.empty());
  EXPECT_EQ(NetErrorString(net::ERR_CONNECTION_RESET), html);

  // Error page loads.

  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);

  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);

  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);

  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

TEST_F(NetErrorHelperCoreTest, SubFrameDnsError) {
  // Original page starts loading.
  core()->OnStartLoad(NetErrorHelperCore::SUB_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::SUB_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  // Should have returned a local error page.
  EXPECT_EQ(NetErrorString(net::ERR_NAME_NOT_RESOLVED), html);

  // Error page loads.
  core()->OnStartLoad(NetErrorHelperCore::SUB_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::SUB_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::SUB_FRAME);
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

TEST_F(NetErrorHelperCoreTest, SubFrameDnsErrorWithCorrections) {
  EnableNavigationCorrections();

  // Original page starts loading.
  core()->OnStartLoad(NetErrorHelperCore::SUB_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::SUB_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  // Should have returned a local error page.
  EXPECT_EQ(NetErrorString(net::ERR_NAME_NOT_RESOLVED), html);

  // Error page loads.
  core()->OnStartLoad(NetErrorHelperCore::SUB_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::SUB_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::SUB_FRAME);
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

// Much like above tests, but with a bunch of spurious DNS status messages that
// should have no effect.
TEST_F(NetErrorHelperCoreTest, SubFrameDnsErrorSpuriousStatus) {
  // Original page starts loading.
  core()->OnStartLoad(NetErrorHelperCore::SUB_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);

  // It fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::SUB_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);

  // Should have returned a local error page.
  EXPECT_EQ(NetErrorString(net::ERR_NAME_NOT_RESOLVED), html);

  // Error page loads.

  core()->OnStartLoad(NetErrorHelperCore::SUB_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);

  core()->OnCommitLoad(NetErrorHelperCore::SUB_FRAME, error_url());
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);

  core()->OnFinishLoad(NetErrorHelperCore::SUB_FRAME);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);

  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

//------------------------------------------------------------------------------
// Tests for updating the error page in response to DNS probe results.  None
// of these have navigation corrections enabled.
//------------------------------------------------------------------------------

// Test case where the error page finishes loading before receiving any DNS
// probe messages.
TEST_F(NetErrorHelperCoreTest, FinishedBeforeProbe) {
  // Original page starts loading.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());

  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_STARTED), last_error_html());

  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());

  // Any other probe updates should be ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

// Same as above, but the probe is not run.
TEST_F(NetErrorHelperCoreTest, FinishedBeforeProbeNotRun) {
  // Original page starts loading.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());

  // When the not run status arrives, the page should revert to the normal dns
  // error page.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_NOT_RUN);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(NetErrorString(net::ERR_NAME_NOT_RESOLVED), last_error_html());

  // Any other probe updates should be ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

// Same as above, but the probe result is inconclusive.
TEST_F(NetErrorHelperCoreTest, FinishedBeforeProbeInconclusive) {
  // Original page starts loading.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());

  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_STARTED), last_error_html());

  // When the inconclusive status arrives, the page should revert to the normal
  // dns error page.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_INCONCLUSIVE);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(NetErrorString(net::ERR_NAME_NOT_RESOLVED), last_error_html());

  // Any other probe updates should be ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_INCONCLUSIVE);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

// Same as above, but the probe result is no internet.
TEST_F(NetErrorHelperCoreTest, FinishedBeforeProbeNoInternet) {
  base::HistogramTester histogram_tester_;

  // Original page starts loading.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());

  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_STARTED), last_error_html());

  // The final status arrives, and should display the offline error page.
  set_is_offline_error(true);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NO_INTERNET),
            last_error_html());
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts", error_page::NETWORK_ERROR_PAGE_OFFLINE_ERROR_SHOWN,
      1);

  // Any other probe updates should be ignored.
  set_is_offline_error(false);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(0, error_html_update_count());

  // Perform a second error page load, and confirm that the previous load
  // doesn't affect the result.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_STARTED), last_error_html());
  set_is_offline_error(true);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NO_INTERNET),
            last_error_html());
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts", error_page::NETWORK_ERROR_PAGE_OFFLINE_ERROR_SHOWN,
      2);
}

// Same as above, but the probe result is bad config.
TEST_F(NetErrorHelperCoreTest, FinishedBeforeProbeBadConfig) {
  // Original page starts loading.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());

  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_STARTED), last_error_html());

  // When the inconclusive status arrives, the page should revert to the normal
  // dns error page.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_BAD_CONFIG);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_BAD_CONFIG),
            last_error_html());

  // Any other probe updates should be ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_BAD_CONFIG);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

// Test case where the error page finishes loading after receiving the start
// DNS probe message.
TEST_F(NetErrorHelperCoreTest, FinishedAfterStartProbe) {
  // Original page starts loading.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());

  // Nothing should be done when a probe status comes in before loading
  // finishes.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  EXPECT_EQ(0, update_count());

  // When loading finishes, however, the buffered probe status should be sent
  // to the page.
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_STARTED), last_error_html());

  // Should update the page again when the probe result comes in.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());

  // Any other probe updates should be ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_NOT_RUN);
  EXPECT_EQ(2, update_count());
}

// Test case where the error page finishes loading before receiving any DNS
// probe messages and the request is a POST.
TEST_F(NetErrorHelperCoreTest, FinishedBeforeProbePost) {
  // Original page starts loading.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           true /* is_failed_post */, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ErrorToString(ProbeError(error_page::DNS_PROBE_POSSIBLE), true),
            html);

  // Error page loads.
  EXPECT_EQ(0, enable_page_helper_functions_count());
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(1, enable_page_helper_functions_count());

  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ErrorToString(ProbeError(error_page::DNS_PROBE_STARTED), true),
            last_error_html());

  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(
      ErrorToString(ProbeError(error_page::DNS_PROBE_FINISHED_NXDOMAIN), true),
      last_error_html());
  EXPECT_EQ(0, error_html_update_count());
}

// Test case where the probe finishes before the page is committed.
TEST_F(NetErrorHelperCoreTest, ProbeFinishesEarly) {
  // Original page starts loading.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page starts loading.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);

  // Nothing should be done when the probe statuses come in before loading
  // finishes.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(0, update_count());

  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  EXPECT_EQ(0, update_count());

  // When loading finishes, however, the buffered probe status should be sent
  // to the page.
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());

  // Any other probe updates should be ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(1, update_count());
}

// Test case where one error page loads completely before a new navigation
// results in another error page.  Probes are run for both pages.
TEST_F(NetErrorHelperCoreTest, TwoErrorsWithProbes) {
  // Original page starts loading.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // Probe results come in.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());

  // The process starts again.

  // Normal page starts loading.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(2, update_count());

  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  EXPECT_EQ(3, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_STARTED), last_error_html());

  // The probe returns a different result this time.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
  EXPECT_EQ(4, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NO_INTERNET),
            last_error_html());
  EXPECT_EQ(0, error_html_update_count());
}

// Test case where one error page loads completely before a new navigation
// results in another error page.  Probe results for the first probe are only
// received after the second load starts, but before it commits.
TEST_F(NetErrorHelperCoreTest, TwoErrorsWithProbesAfterSecondStarts) {
  // Original page starts loading.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // The process starts again.

  // Normal page starts loading.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page starts to load.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);

  // Probe results come in, and the first page is updated.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());

  // Second page finishes loading, and is updated using the same probe result.
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(3, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());

  // Other probe results should be ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
  EXPECT_EQ(3, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

// Same as above, but a new page is loaded before the error page commits.
TEST_F(NetErrorHelperCoreTest, ErrorPageLoadInterrupted) {
  // Original page starts loading.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page starts loading.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  // Probe statuses come in, but should be ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(0, update_count());

  // A new navigation begins while the error page is loading.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // And fails.
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page finishes loading.
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // Probe results come in.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_STARTED), last_error_html());

  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NO_INTERNET),
            last_error_html());
  EXPECT_EQ(0, error_html_update_count());
}

//------------------------------------------------------------------------------
// Navigation correction tests.
//------------------------------------------------------------------------------

// Check that corrections are not used for HTTPS URLs.
TEST_F(NetErrorHelperCoreTest, NoCorrectionsForHttps) {
  // Original page starts loading.
  EnableNavigationCorrections();
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // The HTTPS page fails to load.
  std::string html;
  auto error =
      NetErrorForURL(net::ERR_NAME_NOT_RESOLVED, GURL(kFailedHttpsUrl));
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME, error,
                           false /* is_failed_post */, &html);

  auto probe_error =
      ProbeErrorForURL(error_page::DNS_PROBE_POSSIBLE, GURL(kFailedHttpsUrl));
  EXPECT_EQ(ErrorToString(probe_error, false), html);
  EXPECT_FALSE(is_url_being_fetched());
  EXPECT_FALSE(last_error_page_params());

  // The blank page loads, no error page is loaded.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_FALSE(is_url_being_fetched());
  EXPECT_FALSE(last_error_page_params());

  // Page is updated in response to DNS probes as normal.
  EXPECT_EQ(0, update_count());
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_FALSE(last_error_page_params());
  auto final_probe_error = ProbeErrorForURL(
      error_page::DNS_PROBE_FINISHED_NXDOMAIN, GURL(kFailedHttpsUrl));
  EXPECT_EQ(ErrorToString(final_probe_error, false), last_error_html());
}

// The blank page loads, then the navigation corrections request succeeds and is
// loaded.  Then the probe results come in.
TEST_F(NetErrorHelperCoreTest, CorrectionsReceivedBeforeProbe) {
  // Original page starts loading.
  EnableNavigationCorrections();
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  EXPECT_TRUE(html.empty());
  EXPECT_FALSE(is_url_being_fetched());
  EXPECT_FALSE(last_error_page_params());

  // The blank page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());

  // Corrections retrieval starts when the error page finishes loading.
  EXPECT_FALSE(is_url_being_fetched());
  EXPECT_FALSE(last_error_page_params());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_TRUE(is_url_being_fetched());
  EXPECT_FALSE(last_error_page_params());

  // Corrections are retrieved.
  NavigationCorrectionsLoadSuccess(kDefaultCorrections,
                                   base::size(kDefaultCorrections));
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(NetErrorString(net::ERR_NAME_NOT_RESOLVED), last_error_html());
  ExpectDefaultNavigationCorrections();
  EXPECT_FALSE(is_url_being_fetched());

  // Corrections load.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // Any probe statuses should be ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);

  EXPECT_EQ(0, update_count());
  EXPECT_EQ(1, error_html_update_count());
}

// The blank page finishes loading, then probe results come in, and then
// the navigation corrections request succeeds.
TEST_F(NetErrorHelperCoreTest, CorrectionsRetrievedAfterProbes) {
  // Original page starts loading.
  EnableNavigationCorrections();
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and corrections are requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  EXPECT_TRUE(html.empty());

  // The blank page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_TRUE(is_url_being_fetched());
  EXPECT_FALSE(last_error_page_params());

  // Probe statuses should be ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, error_html_update_count());
  EXPECT_FALSE(last_error_page_params());

  // Corrections are retrieved.
  EXPECT_TRUE(is_url_being_fetched());
  NavigationCorrectionsLoadSuccess(kDefaultCorrections,
                                   base::size(kDefaultCorrections));
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(NetErrorString(net::ERR_NAME_NOT_RESOLVED), last_error_html());
  ExpectDefaultNavigationCorrections();
  EXPECT_FALSE(is_url_being_fetched());

  // Corrections load.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(0, update_count());
}

// The corrections request fails and then the error page loads for an error that
// does not trigger DNS probes.
TEST_F(NetErrorHelperCoreTest, CorrectionsFailLoadNoProbes) {
  // Original page starts loading.
  EnableNavigationCorrections();
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and corrections are requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_CONNECTION_FAILED),
                           false /* is_failed_post */, &html);
  EXPECT_TRUE(html.empty());

  // The blank page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // Corrections request fails, final error page is shown.
  EXPECT_TRUE(is_url_being_fetched());
  NavigationCorrectionsLoadFailure();
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(last_error_html(), NetErrorString(net::ERR_CONNECTION_FAILED));
  EXPECT_FALSE(is_url_being_fetched());
  EXPECT_EQ(0, update_count());
  EXPECT_FALSE(last_error_page_params());

  // Error page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // If probe statuses come in last from another page load, they should be
  // ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(1, error_html_update_count());
}

// The corrections request fails and then the error page loads before probe
// results are received.
TEST_F(NetErrorHelperCoreTest, CorrectionsFailLoadBeforeProbe) {
  // Original page starts loading.
  EnableNavigationCorrections();
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and corrections are requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  EXPECT_TRUE(html.empty());

  // The blank page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // Corrections request fails, probe pending page shown.
  EXPECT_TRUE(is_url_being_fetched());
  NavigationCorrectionsLoadFailure();
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(last_error_html(),
            ProbeErrorString(error_page::DNS_PROBE_POSSIBLE));
  EXPECT_FALSE(is_url_being_fetched());
  EXPECT_EQ(0, update_count());

  // Probe page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // Probe statuses comes in, and page is updated.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_STARTED), last_error_html());

  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());

  // The commit results in sending a second probe status, which is ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(1, error_html_update_count());
}

// The corrections request fails after receiving probe results.
TEST_F(NetErrorHelperCoreTest, CorrectionsFailAfterProbe) {
  // Original page starts loading.
  EnableNavigationCorrections();
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and corrections are requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  EXPECT_TRUE(html.empty());

  // The blank page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // Results come in, but end up being ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(0, update_count());

  // Corrections request fails, probe pending page shown.
  EXPECT_TRUE(is_url_being_fetched());
  NavigationCorrectionsLoadFailure();
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(last_error_html(),
            ProbeErrorString(error_page::DNS_PROBE_POSSIBLE));
  EXPECT_FALSE(is_url_being_fetched());
  EXPECT_EQ(0, update_count());

  // Probe page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // Probe statuses comes in, and page is updated.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());
  EXPECT_EQ(1, error_html_update_count());
}

// An error page load that would normally load correction is interrupted
// by a new navigation before the blank page commits.
TEST_F(NetErrorHelperCoreTest, CorrectionsInterruptedBeforeCommit) {
  // Original page starts loading.
  EnableNavigationCorrections();
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and corrections are requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  EXPECT_TRUE(html.empty());

  // The blank page starts loading.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);

  // A new page load starts.
  EXPECT_FALSE(is_url_being_fetched());
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // A new page load interrupts the original load.
  EXPECT_FALSE(is_url_being_fetched());
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);
  EXPECT_FALSE(is_url_being_fetched());
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, default_url());
  EXPECT_FALSE(is_url_being_fetched());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  EXPECT_FALSE(is_url_being_fetched());
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

// An error page load that would normally load corrections is interrupted
// by a new navigation before the blank page finishes loading.
TEST_F(NetErrorHelperCoreTest, CorrectionsInterruptedBeforeLoad) {
  // Original page starts loading.
  EnableNavigationCorrections();
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and corrections are requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  EXPECT_TRUE(html.empty());

  // The blank page starts loading and is committed.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());

  // A new page load interrupts the original load.
  EXPECT_FALSE(is_url_being_fetched());
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);
  EXPECT_FALSE(is_url_being_fetched());
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, default_url());
  EXPECT_FALSE(is_url_being_fetched());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  EXPECT_FALSE(is_url_being_fetched());
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

// The corrections request is cancelled due to a new navigation.  The new
// navigation fails and then loads corrections successfully.
TEST_F(NetErrorHelperCoreTest, CorrectionsInterrupted) {
  // Original page starts loading.
  EnableNavigationCorrections();
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and corrections are requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  EXPECT_TRUE(html.empty());

  // The blank page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_TRUE(is_url_being_fetched());

  // Results come in, but end up being ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(0, update_count());

  // A new load appears!
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);
  EXPECT_FALSE(is_url_being_fetched());

  // It fails, and corrections are requested again once a blank page is loaded.
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  EXPECT_TRUE(html.empty());
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  EXPECT_FALSE(is_url_being_fetched());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_TRUE(is_url_being_fetched());

  // Corrections request succeeds.
  NavigationCorrectionsLoadSuccess(kDefaultCorrections,
                                   base::size(kDefaultCorrections));
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(NetErrorString(net::ERR_NAME_NOT_RESOLVED), last_error_html());
  ExpectDefaultNavigationCorrections();
  EXPECT_FALSE(is_url_being_fetched());

  // Probe statuses come in, and are ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(0, update_count());
}

// The corrections request is cancelled due to call to Stop().  The cross
// process navigation is cancelled, and then a new load fails and tries to load
// corrections again, unsuccessfully.
TEST_F(NetErrorHelperCoreTest, CorrectionsStopped) {
  // Original page starts loading.
  EnableNavigationCorrections();
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and corrections are requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  EXPECT_TRUE(html.empty());

  // The blank page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  EXPECT_TRUE(is_url_being_fetched());
  core()->OnStop();
  EXPECT_FALSE(is_url_being_fetched());

  // Results come in, but end up being ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(0, update_count());

  // Cross process navigation must have been cancelled, and a new load appears!
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and corrections are requested again.
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  EXPECT_TRUE(html.empty());

  // The blank page loads again.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_TRUE(is_url_being_fetched());

  // Corrections request fails, probe pending page shown.
  NavigationCorrectionsLoadFailure();
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(last_error_html(),
            ProbeErrorString(error_page::DNS_PROBE_POSSIBLE));
  EXPECT_FALSE(is_url_being_fetched());

  // Probe page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // Probe statuses comes in, and page is updated.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_STARTED), last_error_html());
  EXPECT_EQ(1, update_count());

  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());
  EXPECT_EQ(1, error_html_update_count());
}

// Check the case corrections are disabled while the blank page (Loaded
// before the corrections page) is being loaded.
TEST_F(NetErrorHelperCoreTest, CorrectionsDisabledBeforeFetch) {
  // Original page starts loading.
  EnableNavigationCorrections();
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and corrections are requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  EXPECT_TRUE(html.empty());

  // The blank page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  // Corrections is disabled.
  DisableNavigationCorrections();
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_TRUE(is_url_being_fetched());
  EXPECT_FALSE(last_error_page_params());

  // Corrections are retrieved.
  NavigationCorrectionsLoadSuccess(kDefaultCorrections,
                                   base::size(kDefaultCorrections));
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(NetErrorString(net::ERR_NAME_NOT_RESOLVED), last_error_html());
  EXPECT_FALSE(is_url_being_fetched());
  ExpectDefaultNavigationCorrections();

  // Corrections load.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(0, update_count());
}

// Check the case corrections is disabled while fetching the corrections for
// a failed page load.
TEST_F(NetErrorHelperCoreTest, CorrectionsDisabledDuringFetch) {
  // Original page starts loading.
  EnableNavigationCorrections();
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and corrections are requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  EXPECT_TRUE(html.empty());

  // The blank page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_TRUE(is_url_being_fetched());
  EXPECT_FALSE(last_error_page_params());

  // Corrections are disabled.
  DisableNavigationCorrections();

  // Corrections are retrieved.
  NavigationCorrectionsLoadSuccess(kDefaultCorrections,
                                   base::size(kDefaultCorrections));
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(NetErrorString(net::ERR_NAME_NOT_RESOLVED), last_error_html());
  EXPECT_FALSE(is_url_being_fetched());
  ExpectDefaultNavigationCorrections();

  // Corrections load.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(0, update_count());
}

// Checks corrections are is used when there are no search suggestions.
TEST_F(NetErrorHelperCoreTest, CorrectionsWithoutSearch) {
  const NavigationCorrection kCorrections[] = {
      {"urlCorrection", "http://somewhere_else/", "btn", "data", false, false},
  };

  // Original page starts loading.
  EnableNavigationCorrections();
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and corrections are requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  EXPECT_TRUE(html.empty());

  // The blank page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_TRUE(is_url_being_fetched());
  EXPECT_FALSE(last_error_page_params());

  // Corrections are retrieved.
  NavigationCorrectionsLoadSuccess(kCorrections, base::size(kCorrections));
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(NetErrorString(net::ERR_NAME_NOT_RESOLVED), last_error_html());
  EXPECT_FALSE(is_url_being_fetched());

  // Check params.
  ASSERT_TRUE(last_error_page_params());
  EXPECT_FALSE(last_error_page_params()->suggest_reload);
  EXPECT_EQ(1u, last_error_page_params()->override_suggestions->GetSize());
  EXPECT_FALSE(last_error_page_params()->search_url.is_valid());
  EXPECT_EQ("", last_error_page_params()->search_terms);

  // Corrections load.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(0, update_count());
}

// Checks corrections are used when there are only search suggestions.
TEST_F(NetErrorHelperCoreTest, CorrectionsOnlySearchSuggestion) {
  const NavigationCorrection kCorrections[] = {
      {"webSearchQuery", kSuggestedSearchTerms, "frm", "data", false, false},
  };

  // Original page starts loading.
  EnableNavigationCorrections();
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and corrections are requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  EXPECT_TRUE(html.empty());

  // The blank page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_TRUE(is_url_being_fetched());
  EXPECT_FALSE(last_error_page_params());

  // Corrections are retrieved.
  NavigationCorrectionsLoadSuccess(kCorrections, base::size(kCorrections));
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(NetErrorString(net::ERR_NAME_NOT_RESOLVED), last_error_html());
  EXPECT_FALSE(is_url_being_fetched());

  // Check params.
  ASSERT_TRUE(last_error_page_params());
  EXPECT_FALSE(last_error_page_params()->suggest_reload);
  EXPECT_EQ(0u, last_error_page_params()->override_suggestions->GetSize());
  EXPECT_EQ(GURL(kSearchUrl), last_error_page_params()->search_url);
  EXPECT_EQ(kSuggestedSearchTerms, last_error_page_params()->search_terms);

  // Corrections load.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(0, update_count());
}

// The correction service returns a non-JSON result.
TEST_F(NetErrorHelperCoreTest, CorrectionServiceReturnsNonJsonResult) {
  // Original page starts loading.
  EnableNavigationCorrections();
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and corrections are requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_CONNECTION_FAILED),
                           false /* is_failed_post */, &html);
  EXPECT_TRUE(html.empty());

  // The blank page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // Corrections request fails, final error page is shown.
  EXPECT_TRUE(is_url_being_fetched());
  NavigationCorrectionsLoadFinished("Weird Response");
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(last_error_html(), NetErrorString(net::ERR_CONNECTION_FAILED));
  EXPECT_FALSE(is_url_being_fetched());
  EXPECT_EQ(0, update_count());
  EXPECT_FALSE(last_error_page_params());

  // Error page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
}

// The correction service returns a JSON result that isn't a valid list of
// corrections.
TEST_F(NetErrorHelperCoreTest, CorrectionServiceReturnsInvalidJsonResult) {
  // Original page starts loading.
  EnableNavigationCorrections();
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and corrections are requested.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_CONNECTION_FAILED),
                           false /* is_failed_post */, &html);
  EXPECT_TRUE(html.empty());

  // The blank page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // Corrections request fails, final error page is shown.
  EXPECT_TRUE(is_url_being_fetched());
  NavigationCorrectionsLoadFinished("{\"result\": 42}");
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(last_error_html(), NetErrorString(net::ERR_CONNECTION_FAILED));
  EXPECT_FALSE(is_url_being_fetched());
  EXPECT_EQ(0, update_count());
  EXPECT_FALSE(last_error_page_params());

  // Error page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
}

TEST_F(NetErrorHelperCoreTest, CorrectionClickTracking) {
  // Go through the standard navigation correction steps.

  // Original page starts loading.
  EnableNavigationCorrections();
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails.
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_NAME_NOT_RESOLVED),
                           false /* is_failed_post */, &html);
  EXPECT_TRUE(html.empty());
  EXPECT_FALSE(is_url_being_fetched());
  EXPECT_FALSE(last_error_page_params());

  // The blank page loads.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());

  // Corrections retrieval starts when the error page finishes loading.
  EXPECT_FALSE(is_url_being_fetched());
  EXPECT_FALSE(last_error_page_params());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_TRUE(is_url_being_fetched());
  EXPECT_FALSE(last_error_page_params());

  // Corrections are retrieved.
  NavigationCorrectionsLoadSuccess(kDefaultCorrections,
                                   base::size(kDefaultCorrections));
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(NetErrorString(net::ERR_NAME_NOT_RESOLVED), last_error_html());
  ExpectDefaultNavigationCorrections();
  EXPECT_FALSE(is_url_being_fetched());

  // Corrections load.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  EXPECT_EQ(0, tracking_request_count());

  // Invalid clicks should be ignored.
  core()->TrackClick(-1);
  core()->TrackClick(base::size(kDefaultCorrections));
  EXPECT_EQ(0, tracking_request_count());

  for (size_t i = 0; i < base::size(kDefaultCorrections); ++i) {
    // Skip links that do not appear in the page.
    if (kDefaultCorrections[i].is_porn || kDefaultCorrections[i].is_soft_porn)
      continue;

    int old_tracking_request_count = tracking_request_count();
    core()->TrackClick(i);
    EXPECT_EQ(old_tracking_request_count + 1, tracking_request_count());
    EXPECT_EQ(GURL(kNavigationCorrectionUrl), last_tracking_url());

    // Make sure all expected strings appear in output.
    EXPECT_TRUE(last_tracking_request_body().find(
        kDefaultCorrections[i].url_correction));
    EXPECT_TRUE(
        last_tracking_request_body().find(kDefaultCorrections[i].click_data));
    EXPECT_TRUE(
        last_tracking_request_body().find(kDefaultCorrections[i].click_type));
    EXPECT_TRUE(
        last_tracking_request_body().find(kNavigationCorrectionEventId));
    EXPECT_TRUE(
        last_tracking_request_body().find(kNavigationCorrectionFingerprint));
  }

  // Make sure duplicate tracking requests are ignored.
  for (size_t i = 0; i < base::size(kDefaultCorrections); ++i) {
    // Skip links that do not appear in the page.
    if (kDefaultCorrections[i].is_porn || kDefaultCorrections[i].is_soft_porn)
      continue;

    int old_tracking_request_count = tracking_request_count();
    core()->TrackClick(i);
    EXPECT_EQ(old_tracking_request_count, tracking_request_count());
  }

  EXPECT_EQ(0, update_count());
  EXPECT_EQ(1, error_html_update_count());
}

TEST_F(NetErrorHelperCoreTest, AutoReloadDisabled) {
  DoErrorLoad(net::ERR_CONNECTION_RESET);

  EXPECT_FALSE(timer()->IsRunning());
  EXPECT_EQ(0, reload_count());
}

class NetErrorHelperCoreAutoReloadTest : public NetErrorHelperCoreTest {
 public:
  void SetUp() override {
    NetErrorHelperCoreTest::SetUp();
    SetUpCore(true, true);
  }
};

TEST_F(NetErrorHelperCoreAutoReloadTest, Succeeds) {
  DoErrorLoad(net::ERR_CONNECTION_RESET);

  EXPECT_TRUE(timer()->IsRunning());
  EXPECT_EQ(0, reload_count());

  timer()->Fire();
  EXPECT_FALSE(timer()->IsRunning());
  EXPECT_EQ(1, reload_count());

  DoSuccessLoad();

  EXPECT_FALSE(timer()->IsRunning());
}

TEST_F(NetErrorHelperCoreAutoReloadTest, Retries) {
  DoErrorLoad(net::ERR_CONNECTION_RESET);

  EXPECT_TRUE(timer()->IsRunning());
  base::TimeDelta first_delay = timer()->GetCurrentDelay();
  EXPECT_EQ(0, reload_count());

  timer()->Fire();
  EXPECT_FALSE(timer()->IsRunning());
  EXPECT_EQ(1, reload_count());

  DoErrorLoad(net::ERR_CONNECTION_RESET);

  EXPECT_TRUE(timer()->IsRunning());
  EXPECT_GT(timer()->GetCurrentDelay(), first_delay);
}

TEST_F(NetErrorHelperCoreAutoReloadTest, StopsTimerOnStop) {
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_TRUE(timer()->IsRunning());
  core()->OnStop();
  EXPECT_FALSE(timer()->IsRunning());
}

TEST_F(NetErrorHelperCoreAutoReloadTest, StopsLoadingOnStop) {
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_EQ(0, core()->auto_reload_count());
  timer()->Fire();
  EXPECT_EQ(1, core()->auto_reload_count());
  EXPECT_EQ(1, reload_count());
  core()->OnStop();
  EXPECT_FALSE(timer()->IsRunning());
  EXPECT_EQ(0, core()->auto_reload_count());
}

TEST_F(NetErrorHelperCoreAutoReloadTest, StopsOnOtherLoadStart) {
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_TRUE(timer()->IsRunning());
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);
  EXPECT_FALSE(timer()->IsRunning());
  EXPECT_EQ(0, core()->auto_reload_count());
}

// This is a regression test for http://crbug.com/881208.
TEST_F(NetErrorHelperCoreAutoReloadTest, StopsOnErrorLoadCommit) {
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_TRUE(timer()->IsRunning());

  // Simulate manually reloading the error page while the timer is still
  // running.
  std::string html;
  core()->PrepareErrorPage(
      NetErrorHelperCore::MAIN_FRAME,
      NetErrorForURL(net::ERR_CONNECTION_RESET, GURL(kFailedUrl)),
      false /* is_failed_post */, &html);

  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  EXPECT_FALSE(timer()->IsRunning());
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_TRUE(timer()->IsRunning());
}

TEST_F(NetErrorHelperCoreAutoReloadTest, ResetsCountOnSuccess) {
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  base::TimeDelta delay = timer()->GetCurrentDelay();
  EXPECT_EQ(0, core()->auto_reload_count());
  timer()->Fire();
  EXPECT_EQ(1, core()->auto_reload_count());
  EXPECT_EQ(1, reload_count());
  DoSuccessLoad();
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_EQ(0, core()->auto_reload_count());
  EXPECT_EQ(timer()->GetCurrentDelay(), delay);
  timer()->Fire();
  EXPECT_EQ(1, core()->auto_reload_count());
  EXPECT_EQ(2, reload_count());
  DoSuccessLoad();
  EXPECT_EQ(0, core()->auto_reload_count());
}

TEST_F(NetErrorHelperCoreAutoReloadTest, RestartsOnOnline) {
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  base::TimeDelta delay = timer()->GetCurrentDelay();
  timer()->Fire();
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_TRUE(timer()->IsRunning());
  EXPECT_NE(delay, timer()->GetCurrentDelay());
  core()->NetworkStateChanged(false);
  EXPECT_FALSE(timer()->IsRunning());
  core()->NetworkStateChanged(true);
  EXPECT_TRUE(timer()->IsRunning());
  EXPECT_EQ(delay, timer()->GetCurrentDelay());
}

TEST_F(NetErrorHelperCoreAutoReloadTest, DoesNotStartOnOnline) {
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  timer()->Fire();
  DoSuccessLoad();
  EXPECT_FALSE(timer()->IsRunning());
  core()->NetworkStateChanged(true);
  EXPECT_FALSE(timer()->IsRunning());
}

TEST_F(NetErrorHelperCoreAutoReloadTest, DoesNotStartOffline) {
  core()->NetworkStateChanged(false);
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_FALSE(timer()->IsRunning());
  core()->NetworkStateChanged(true);
  EXPECT_TRUE(timer()->IsRunning());
}

TEST_F(NetErrorHelperCoreAutoReloadTest, DoesNotRestartOnOnlineAfterStop) {
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  timer()->Fire();
  core()->OnStop();
  core()->NetworkStateChanged(true);
  EXPECT_FALSE(timer()->IsRunning());
}

TEST_F(NetErrorHelperCoreAutoReloadTest, WithDnsProbes) {
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  DoDnsProbe(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  timer()->Fire();
  EXPECT_EQ(1, reload_count());
}

TEST_F(NetErrorHelperCoreAutoReloadTest, ExponentialBackoffLevelsOff) {
  base::TimeDelta previous = base::TimeDelta::FromMilliseconds(0);
  const int kMaxTries = 50;
  int tries = 0;
  for (tries = 0; tries < kMaxTries; tries++) {
    DoErrorLoad(net::ERR_CONNECTION_RESET);
    EXPECT_TRUE(timer()->IsRunning());
    if (previous == timer()->GetCurrentDelay())
      break;
    previous = timer()->GetCurrentDelay();
    timer()->Fire();
  }

  EXPECT_LT(tries, kMaxTries);
}

TEST_F(NetErrorHelperCoreAutoReloadTest, SlowError) {
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_CONNECTION_RESET),
                           false /* is_failed_post */, &html);
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  EXPECT_FALSE(timer()->IsRunning());
  // Start a new non-error page load.
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);
  EXPECT_FALSE(timer()->IsRunning());
  // Finish the error page load.
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_FALSE(timer()->IsRunning());
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_FALSE(timer()->IsRunning());
}

TEST_F(NetErrorHelperCoreAutoReloadTest, OnlineSlowError) {
  core()->NetworkStateChanged(false);
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_CONNECTION_RESET),
                           false /* is_failed_post */, &html);
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  EXPECT_FALSE(timer()->IsRunning());
  core()->NetworkStateChanged(true);
  EXPECT_FALSE(timer()->IsRunning());
  core()->NetworkStateChanged(false);
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_FALSE(timer()->IsRunning());
  core()->NetworkStateChanged(true);
  EXPECT_TRUE(timer()->IsRunning());
}

TEST_F(NetErrorHelperCoreAutoReloadTest, OnlinePendingError) {
  core()->NetworkStateChanged(false);
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_CONNECTION_RESET),
                           false /* is_failed_post */, &html);
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  EXPECT_FALSE(timer()->IsRunning());
  core()->NetworkStateChanged(true);
  EXPECT_FALSE(timer()->IsRunning());
  core()->NetworkStateChanged(false);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_FALSE(timer()->IsRunning());
  core()->NetworkStateChanged(true);
  EXPECT_TRUE(timer()->IsRunning());
}

TEST_F(NetErrorHelperCoreAutoReloadTest, OnlinePartialErrorReplacement) {
  core()->NetworkStateChanged(false);
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);
  std::string html;
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_CONNECTION_RESET),
                           false /* is_failed_post */, &html);
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::NON_ERROR_PAGE);
  core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                           NetError(net::ERR_CONNECTION_RESET),
                           false /* is_failed_post */, &html);
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  EXPECT_FALSE(timer()->IsRunning());
  core()->NetworkStateChanged(true);
  EXPECT_FALSE(timer()->IsRunning());
}

TEST_F(NetErrorHelperCoreAutoReloadTest, ShouldSuppressNonReloadableErrorPage) {
  DoErrorLoad(net::ERR_ABORTED);
  EXPECT_FALSE(core()->ShouldSuppressErrorPage(
      NetErrorHelperCore::MAIN_FRAME, GURL(kFailedUrl), net::ERR_ABORTED));

  DoErrorLoad(net::ERR_UNKNOWN_URL_SCHEME);
  EXPECT_FALSE(core()->ShouldSuppressErrorPage(NetErrorHelperCore::MAIN_FRAME,
                                               GURL(kFailedUrl),
                                               net::ERR_UNKNOWN_URL_SCHEME));
}

TEST_F(NetErrorHelperCoreAutoReloadTest,
       ShouldSuppressErrorPageForDifferentError) {
  DoErrorLoad(net::ERR_CERT_AUTHORITY_INVALID);
  EXPECT_FALSE(core()->ShouldSuppressErrorPage(
      NetErrorHelperCore::MAIN_FRAME, GURL(kFailedUrl),
      net::ERR_INVALID_AUTH_CREDENTIALS));
}

TEST_F(NetErrorHelperCoreAutoReloadTest, DoesNotReload) {
  DoErrorLoad(net::ERR_ABORTED);
  EXPECT_FALSE(timer()->IsRunning());

  DoErrorLoad(net::ERR_UNKNOWN_URL_SCHEME);
  EXPECT_FALSE(timer()->IsRunning());

  DoErrorLoad(net::ERR_SSL_PROTOCOL_ERROR);
  EXPECT_FALSE(timer()->IsRunning());

  DoErrorLoad(net::ERR_BLOCKED_BY_ADMINISTRATOR);
  EXPECT_FALSE(timer()->IsRunning());

  DoErrorLoad(net::ERR_BAD_SSL_CLIENT_AUTH_CERT);
  EXPECT_FALSE(timer()->IsRunning());

  DoErrorLoadOfURL(net::ERR_ACCESS_DENIED, GURL("data://some-data-here"));
  EXPECT_FALSE(timer()->IsRunning());

  DoErrorLoadOfURL(net::ERR_ACCESS_DENIED, GURL("chrome-extension://foo"));
  EXPECT_FALSE(timer()->IsRunning());
}

TEST_F(NetErrorHelperCoreAutoReloadTest, ShouldSuppressErrorPage) {
  // Set up the environment to test ShouldSuppressErrorPage: auto-reload is
  // enabled, an error page is loaded, and the auto-reload callback is running.
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  timer()->Fire();

  // Sub-frame load.
  EXPECT_FALSE(core()->ShouldSuppressErrorPage(NetErrorHelperCore::SUB_FRAME,
                                               GURL(kFailedUrl),
                                               net::ERR_CONNECTION_RESET));
  EXPECT_TRUE(core()->ShouldSuppressErrorPage(NetErrorHelperCore::MAIN_FRAME,
                                              GURL(kFailedUrl),
                                              net::ERR_CONNECTION_RESET));
  // No auto-reload attempt in flight.
  EXPECT_FALSE(core()->ShouldSuppressErrorPage(NetErrorHelperCore::MAIN_FRAME,
                                               GURL(kFailedUrl),
                                               net::ERR_CONNECTION_RESET));
}

TEST_F(NetErrorHelperCoreAutoReloadTest, HiddenAndShown) {
  SetUpCore(true, true);
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_TRUE(timer()->IsRunning());
  core()->OnWasHidden();
  EXPECT_FALSE(timer()->IsRunning());
  core()->OnWasShown();
  EXPECT_TRUE(timer()->IsRunning());
}

TEST_F(NetErrorHelperCoreAutoReloadTest, HiddenWhileOnline) {
  SetUpCore(true, true);
  core()->NetworkStateChanged(false);
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_FALSE(timer()->IsRunning());
  core()->OnWasHidden();
  core()->NetworkStateChanged(true);
  EXPECT_FALSE(timer()->IsRunning());
  core()->NetworkStateChanged(false);
  core()->OnWasShown();
  EXPECT_FALSE(timer()->IsRunning());
  core()->NetworkStateChanged(true);
  EXPECT_TRUE(timer()->IsRunning());
  core()->NetworkStateChanged(false);
  core()->OnWasHidden();
  EXPECT_FALSE(timer()->IsRunning());
  core()->NetworkStateChanged(true);
  EXPECT_FALSE(timer()->IsRunning());
  core()->OnWasShown();
  EXPECT_TRUE(timer()->IsRunning());
}

TEST_F(NetErrorHelperCoreAutoReloadTest, ShownWhileNotReloading) {
  SetUpCore(true, false);
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_FALSE(timer()->IsRunning());
  core()->OnWasShown();
  EXPECT_TRUE(timer()->IsRunning());
}

TEST_F(NetErrorHelperCoreAutoReloadTest, ManualReloadShowsError) {
  SetUpCore(true, true);
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  core()->OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                      NetErrorHelperCore::ERROR_PAGE);
  EXPECT_FALSE(core()->ShouldSuppressErrorPage(NetErrorHelperCore::MAIN_FRAME,
                                               GURL(kFailedUrl),
                                               net::ERR_CONNECTION_RESET));
}

TEST_F(NetErrorHelperCoreTest, ExplicitReloadSucceeds) {
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_EQ(0, reload_count());
  core()->ExecuteButtonPress(NetErrorHelperCore::RELOAD_BUTTON);
  EXPECT_EQ(1, reload_count());
}

TEST_F(NetErrorHelperCoreTest, CanNotShowNetworkDiagnostics) {
  core()->OnSetCanShowNetworkDiagnosticsDialog(false);
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_FALSE(last_can_show_network_diagnostics_dialog());
}

TEST_F(NetErrorHelperCoreTest, CanShowNetworkDiagnostics) {
  core()->OnSetCanShowNetworkDiagnosticsDialog(true);
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_TRUE(last_can_show_network_diagnostics_dialog());

  core()->ExecuteButtonPress(NetErrorHelperCore::DIAGNOSE_ERROR);
  EXPECT_EQ(1, diagnose_error_count());
  EXPECT_EQ(GURL(kFailedUrl), diagnose_error_url());
}

#if defined(OS_ANDROID)
TEST_F(NetErrorHelperCoreTest, Download) {
  DoErrorLoad(net::ERR_INTERNET_DISCONNECTED);
  EXPECT_EQ(0, download_count());
  core()->ExecuteButtonPress(NetErrorHelperCore::DOWNLOAD_BUTTON);
  EXPECT_EQ(1, download_count());
}

const char kThumbnailDataURI[] = "data:image/png;base64,abc";
const char kFaviconDataURI[] = "data:image/png;base64,def";

// Creates a couple of fake AvailableOfflineContent instances.
std::vector<chrome::mojom::AvailableOfflineContentPtr>
GetFakeAvailableContent() {
  std::vector<chrome::mojom::AvailableOfflineContentPtr> content;
  content.push_back(chrome::mojom::AvailableOfflineContent::New(
      "ID", "name_space", "title", "snippet", "date_modified", "attribution",
      GURL(kThumbnailDataURI), GURL(kFaviconDataURI),
      chrome::mojom::AvailableContentType::kPrefetchedPage));
  content.push_back(chrome::mojom::AvailableOfflineContent::New(
      "ID2", "name_space2", "title2", "snippet2", "date_modified2",
      "attribution2", GURL(kThumbnailDataURI), GURL(kFaviconDataURI),
      chrome::mojom::AvailableContentType::kOtherPage));
  return content;
}

// Builds the expected JSON representation of the AvailableOfflineContent
// instances returned by |GetFakeAvailableContent|.
const std::string GetExpectedAvailableContentAsJson() {
  // About the below data:
  // * |content_type| is an AvailableContentType enum value where
  //   0 = kPrefetchedPage and 3=kOtherPage.
  // * The base64 encoded values represent the encoded versions of the
  //   respective entries returned by |GetFakeAvailableContent|.
  std::string want_json = R"([
    {
      "ID": "ID",
      "attribution_base64": "AGEAdAB0AHIAaQBiAHUAdABpAG8Abg==",
      "content_type": 0,
      "date_modified": "date_modified",
      "favicon_data_uri": "data:image/png;base64,def",
      "name_space": "name_space",
      "snippet_base64": "AHMAbgBpAHAAcABlAHQ=",
      "thumbnail_data_uri": "data:image/png;base64,abc",
      "title_base64": "AHQAaQB0AGwAZQ=="
    },
    {
      "ID": "ID2",
      "attribution_base64": "AGEAdAB0AHIAaQBiAHUAdABpAG8AbgAy",
      "content_type": 3,
      "date_modified": "date_modified2",
      "favicon_data_uri": "data:image/png;base64,def",
      "name_space": "name_space2",
      "snippet_base64": "AHMAbgBpAHAAcABlAHQAMg==",
      "thumbnail_data_uri": "data:image/png;base64,abc",
      "title_base64": "AHQAaQB0AGwAZQAy"
    }
  ])";
  base::ReplaceChars(want_json, base::kWhitespaceASCII, "", &want_json);
  return want_json;
}

class FakeAvailableOfflineContentProvider
    : public chrome::mojom::AvailableOfflineContentProvider {
 public:
  FakeAvailableOfflineContentProvider() = default;

  void List(ListCallback callback) override {
    if (return_content_) {
      std::move(callback).Run(list_visible_by_prefs_,
                              GetFakeAvailableContent());
    } else {
      std::move(callback).Run(list_visible_by_prefs_, {});
    }
  }

  MOCK_METHOD2(LaunchItem,
               void(const std::string& item_ID, const std::string& name_space));
  MOCK_METHOD1(LaunchDownloadsPage, void(bool open_prefetched_articles_tab));
  MOCK_METHOD1(ListVisibilityChanged, void(bool is_visible));

  void AddBinding(
      mojo::PendingReceiver<chrome::mojom::AvailableOfflineContentProvider>
          receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  void set_return_content(bool return_content) {
    return_content_ = return_content;
  }

  void set_list_visible_by_prefs(bool list_visible_by_prefs) {
    list_visible_by_prefs_ = list_visible_by_prefs;
  }

 private:
  bool return_content_ = true;
  bool list_visible_by_prefs_ = true;
  mojo::ReceiverSet<chrome::mojom::AvailableOfflineContentProvider> receivers_;

  DISALLOW_COPY_AND_ASSIGN(FakeAvailableOfflineContentProvider);
};

// Provides set up for testing the 'available offline content' feature.
class NetErrorHelperCoreAvailableOfflineContentTest
    : public NetErrorHelperCoreTest {
 public:
  void SetUp() override {
    NetErrorHelperCoreTest::SetUp();
    AvailableOfflineContentHelper::OverrideBinderForTesting(
        base::BindRepeating(&FakeAvailableOfflineContentProvider::AddBinding,
                            base::Unretained(&fake_provider_)));
  }

  void TearDown() override {
    AvailableOfflineContentHelper::OverrideBinderForTesting(
        base::NullCallback());
  }

 protected:
  FakeAvailableOfflineContentProvider fake_provider_;
  base::HistogramTester histogram_tester_;
};

TEST_F(NetErrorHelperCoreAvailableOfflineContentTest, ListAvailableContent) {
  set_offline_content_feature_enabled(true);
  fake_provider_.set_return_content(true);

  DoErrorLoad(net::ERR_INTERNET_DISCONNECTED);
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(list_visible_by_prefs());
  EXPECT_EQ(GetExpectedAvailableContentAsJson(), offline_content_json());

  histogram_tester_.ExpectTotalCount("Net.ErrorPageCounts.SuggestionPresented",
                                     2);
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts.SuggestionPresented",
      chrome::mojom::AvailableContentType::kPrefetchedPage, 1);
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts.SuggestionPresented",
      chrome::mojom::AvailableContentType::kOtherPage, 1);
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTIONS_SHOWN, 1);
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTIONS_SHOWN_COLLAPSED, 0);

  core()->LaunchOfflineItem("ID", "name_space");
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts.SuggestionPresented",
      chrome::mojom::AvailableContentType::kPrefetchedPage, 1);
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTION_CLICKED, 1);

  core()->LaunchDownloadsPage();
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_DOWNLOADS_PAGE_CLICKED, 1);
}

TEST_F(NetErrorHelperCoreAvailableOfflineContentTest, ListHiddenByPrefs) {
  set_offline_content_feature_enabled(true);
  fake_provider_.set_return_content(true);
  fake_provider_.set_list_visible_by_prefs(false);

  DoErrorLoad(net::ERR_INTERNET_DISCONNECTED);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(list_visible_by_prefs());
  EXPECT_EQ(GetExpectedAvailableContentAsJson(), offline_content_json());

  histogram_tester_.ExpectTotalCount("Net.ErrorPageCounts.SuggestionPresented",
                                     2);
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts.SuggestionPresented",
      chrome::mojom::AvailableContentType::kPrefetchedPage, 1);
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts.SuggestionPresented",
      chrome::mojom::AvailableContentType::kOtherPage, 1);
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTIONS_SHOWN, 0);
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTIONS_SHOWN_COLLAPSED, 1);

  core()->LaunchOfflineItem("ID", "name_space");
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts.SuggestionPresented",
      chrome::mojom::AvailableContentType::kPrefetchedPage, 1);
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTION_CLICKED, 1);

  core()->LaunchDownloadsPage();
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_DOWNLOADS_PAGE_CLICKED, 1);
}

TEST_F(NetErrorHelperCoreAvailableOfflineContentTest, ListNoAvailableContent) {
  set_offline_content_feature_enabled(true);
  fake_provider_.set_return_content(false);

  DoErrorLoad(net::ERR_INTERNET_DISCONNECTED);
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(list_visible_by_prefs());
  EXPECT_EQ("", offline_content_json());
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTIONS_SHOWN, 0);
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTIONS_SHOWN_COLLAPSED, 0);
}

TEST_F(NetErrorHelperCoreAvailableOfflineContentTest, NotAllowed) {
  set_offline_content_feature_enabled(false);
  fake_provider_.set_return_content(true);

  DoErrorLoad(net::ERR_INTERNET_DISCONNECTED);
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(list_visible_by_prefs());
  EXPECT_EQ("", offline_content_json());
  histogram_tester_.ExpectTotalCount("Net.ErrorPageCounts.SuggestionPresented",
                                     0);
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTIONS_SHOWN, 0);
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTIONS_SHOWN_COLLAPSED, 0);
}

class FakeOfflinePageAutoFetcher
    : public chrome::mojom::OfflinePageAutoFetcher {
 public:
  FakeOfflinePageAutoFetcher() = default;

  struct TryScheduleParameters {
    bool user_requested;
    TryScheduleCallback callback;
  };

  void TrySchedule(bool user_requested, TryScheduleCallback callback) override {
    try_schedule_calls_.push_back({user_requested, std::move(callback)});
  }

  void CancelSchedule() override { cancel_calls_++; }

  void AddReceiver(
      mojo::PendingReceiver<chrome::mojom::OfflinePageAutoFetcher> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  int cancel_calls() const { return cancel_calls_; }
  std::vector<TryScheduleParameters> take_try_schedule_calls() {
    std::vector<TryScheduleParameters> result = std::move(try_schedule_calls_);
    try_schedule_calls_.clear();
    return result;
  }

 private:
  mojo::ReceiverSet<chrome::mojom::OfflinePageAutoFetcher> receivers_;
  int cancel_calls_ = 0;
  std::vector<TryScheduleParameters> try_schedule_calls_;

  DISALLOW_COPY_AND_ASSIGN(FakeOfflinePageAutoFetcher);
};
// This uses the real implementation of PageAutoFetcherHelper, but with a
// substituted fetcher.
class TestPageAutoFetcherHelper : public PageAutoFetcherHelper {
 public:
  explicit TestPageAutoFetcherHelper(
      base::RepeatingCallback<
          mojo::PendingRemote<chrome::mojom::OfflinePageAutoFetcher>()> binder)
      : PageAutoFetcherHelper(nullptr), binder_(binder) {}
  bool Bind() override {
    if (!fetcher_)
      fetcher_.Bind(binder_.Run());
    return true;
  }

 private:
  base::RepeatingCallback<
      mojo::PendingRemote<chrome::mojom::OfflinePageAutoFetcher>()>
      binder_;
};

// Provides set up for testing the 'auto fetch on dino' feature.
class NetErrorHelperCoreAutoFetchTest : public NetErrorHelperCoreTest {
 public:
  void SetUp() override {
    NetErrorHelperCoreTest::SetUp();
    auto binder = base::BindLambdaForTesting([&]() {
      mojo::PendingRemote<chrome::mojom::OfflinePageAutoFetcher> fetcher_remote;
      fake_fetcher_.AddReceiver(
          fetcher_remote.InitWithNewPipeAndPassReceiver());
      return fetcher_remote;
    });

    core()->SetPageAutoFetcherHelperForTesting(
        std::make_unique<TestPageAutoFetcherHelper>(binder));
  }

  void TearDown() override {
    AvailableOfflineContentHelper::OverrideBinderForTesting(
        base::NullCallback());
  }

 protected:
  FakeOfflinePageAutoFetcher fake_fetcher_;
};

TEST_F(NetErrorHelperCoreAutoFetchTest, NotAllowed) {
  set_auto_fetch_allowed(false);

  DoErrorLoad(net::ERR_INTERNET_DISCONNECTED);
  task_environment()->RunUntilIdle();

  // When auto fetch is not allowed, OfflinePageAutoFetcher is not called.
  std::vector<FakeOfflinePageAutoFetcher::TryScheduleParameters> calls =
      fake_fetcher_.take_try_schedule_calls();
  EXPECT_EQ(0ul, calls.size());
}

TEST_F(NetErrorHelperCoreAutoFetchTest, AutoFetchTriggered) {
  set_auto_fetch_allowed(true);

  DoErrorLoad(net::ERR_INTERNET_DISCONNECTED);
  task_environment()->RunUntilIdle();

  // Auto fetch is allowed, so OfflinePageAutoFetcher is called once.
  std::vector<FakeOfflinePageAutoFetcher::TryScheduleParameters> calls =
      fake_fetcher_.take_try_schedule_calls();
  EXPECT_EQ(1ul, calls.size());

  // Finalize the call to TrySchedule, and verify the delegate is called.
  std::move(calls[0].callback)
      .Run(chrome::mojom::OfflinePageAutoFetcherScheduleResult::kScheduled);
  task_environment()->RunUntilIdle();

  EXPECT_EQ(chrome::mojom::OfflinePageAutoFetcherScheduleResult::kScheduled,
            auto_fetch_state());
}

#endif  // defined(OS_ANDROID)

}  // namespace
