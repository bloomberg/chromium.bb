// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/console_message.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/url_loader_factory_manager.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "services/network/cross_origin_read_blocking.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

namespace {

enum TestParam {
  // Whether the extension under test is "allowlisted" (see
  // GetExtensionsAllowlist in
  // //extensions/browser/url_loader_factory_manager.cc).
  kAllowlisted = 1 << 0,

  // Whether network::features::kOutOfBlinkCors is enabled.
  kOutOfBlinkCors = 1 << 1,

  // Whether network::features::kCorbAllowlistAlsoAppliesToOorCors is enabled.
  kAllowlistForCors = 1 << 2,

  // Whether network::features::
  // kDeriveOriginFromUrlForNeitherGetNorHeadRequestWhenHavingSpecialAccess is
  // enabled.
  kDeriveOriginFromUrl = 1 << 3,
};

const char kCorsErrorWhenFetching[] = "error: TypeError: Failed to fetch";

// The manifest.json used by tests uses |kExpectedKey| that will result in the
// hash of extension id that is captured in |kExpectedHashedExtensionId|.
// Knowing the hash constant helps with simulating distributing the hash via
// field trial param (e.g. via CorbAllowlistAlsoAppliesToOorCorsParamName).
const char kExtensionKey[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAjzv7dI7Ygyh67VHE1DdidudpYf8PFf"
    "v8iucWvzO+3xpF/"
    "Dm5xNo7aQhPNiEaNfHwJQ7lsp4gc+C+4bbaVewBFspTruoSJhZc5uEfqxwovJwN+v1/"
    "SUFXTXQmQBv6gs0qZB4gBbl4caNQBlqrFwAMNisnu1V6UROna8rOJQ90D7Nv7TCwoVPKBfVshp"
    "FjdDOTeBg4iLctO3S/"
    "06QYqaTDrwVceSyHkVkvzBY6tc6mnYX0RZu78J9iL8bdqwfllOhs69cqoHHgrLdI6JdOyiuh6p"
    "BP6vxMlzSKWJ3YTNjaQTPwfOYaLMuzdl0v+YdzafIzV9zwe4Xiskk+5JNGt8b2rQIDAQAB";
const char kExpectedHashedExtensionId[] =
    "14B587526D9AC6ADCACAA8A4AAE3DB281CA2AB53";

}  // namespace

using CORBAction = network::CrossOriginReadBlocking::Action;

class CorbAndCorsExtensionTestBase : public ExtensionBrowserTest {
 public:
  CorbAndCorsExtensionTestBase() = default;

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
  }

  std::string CreateFetchScript(const GURL& resource) {
    const char kFetchScriptTemplate[] = R"(
      fetch($1)
        .then(response => response.text())
        .then(text => domAutomationController.send(text))
        .catch(err => domAutomationController.send('error: ' + err));
    )";
    return content::JsReplace(kFetchScriptTemplate, resource);
  }

  std::string PopString(content::DOMMessageQueue* message_queue) {
    std::string json;
    EXPECT_TRUE(message_queue->WaitForMessage(&json));
    base::Optional<base::Value> value =
        base::JSONReader::Read(json, base::JSON_ALLOW_TRAILING_COMMAS);
    std::string result;
    EXPECT_TRUE(value->GetAsString(&result));
    return result;
  }

 protected:
  TestExtensionDir dir_;
};

class ServiceWorkerConsoleObserver
    : public content::ServiceWorkerContextObserver {
 public:
  explicit ServiceWorkerConsoleObserver(
      content::BrowserContext* browser_context)
      : scoped_observer_(this) {
    content::StoragePartition* partition =
        content::BrowserContext::GetDefaultStoragePartition(browser_context);
    scoped_observer_.Add(partition->GetServiceWorkerContext());
  }
  ~ServiceWorkerConsoleObserver() override = default;

  ServiceWorkerConsoleObserver(const ServiceWorkerConsoleObserver&) = delete;
  ServiceWorkerConsoleObserver& operator=(const ServiceWorkerConsoleObserver&) =
      delete;

  using Message = content::ConsoleMessage;
  const std::vector<Message>& messages() const { return messages_; }

  void WaitForMessages() { run_loop_.Run(); }

 private:
  // ServiceWorkerContextObserver:
  void OnReportConsoleMessage(int64_t version_id,
                              const Message& message) override {
    messages_.push_back(message);
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  std::vector<Message> messages_;
  ScopedObserver<content::ServiceWorkerContext,
                 content::ServiceWorkerContextObserver>
      scoped_observer_;
};

class CorbAndCorsExtensionBrowserTest
    : public CorbAndCorsExtensionTestBase,
      public ::testing::WithParamInterface<TestParam> {
 public:
  using Base = CorbAndCorsExtensionTestBase;

  CorbAndCorsExtensionBrowserTest() {
    std::vector<base::Feature> disabled_features;
    std::vector<base::test::ScopedFeatureList::FeatureAndParams>
        enabled_features;

    if (IsOutOfBlinkCorsEnabled()) {
      enabled_features.emplace_back(network::features::kOutOfBlinkCors,
                                    base::FieldTrialParams());
    } else {
      disabled_features.push_back(network::features::kOutOfBlinkCors);
    }

    if (DeriveOriginFromUrl()) {
      enabled_features.emplace_back(
          network::features::
              kDeriveOriginFromUrlForNeitherGetNorHeadRequestWhenHavingSpecialAccess,
          base::FieldTrialParams());
    } else {
      disabled_features.push_back(
          network::features::
              kDeriveOriginFromUrlForNeitherGetNorHeadRequestWhenHavingSpecialAccess);
    }

    if (ShouldAllowlistAlsoApplyToOorCors()) {
      base::FieldTrialParams field_trial_params;
      if (IsExtensionAllowlisted()) {
        field_trial_params.emplace(
            network::features::kCorbAllowlistAlsoAppliesToOorCorsParamName,
            kExpectedHashedExtensionId);
      }
      enabled_features.emplace_back(
          network::features::kCorbAllowlistAlsoAppliesToOorCors,
          field_trial_params);
    } else {
      disabled_features.push_back(
          network::features::kCorbAllowlistAlsoAppliesToOorCors);
    }

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }

  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy_provider_.SetAutoRefresh();
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  bool IsExtensionAllowlisted() {
    return (GetParam() & TestParam::kAllowlisted) != 0;
  }

  bool IsOutOfBlinkCorsEnabled() {
    return (GetParam() & TestParam::kOutOfBlinkCors) != 0;
  }

  // This returns true if content scripts are not exempt from CORS.
  bool ShouldAllowlistAlsoApplyToOorCors() {
    return (GetParam() & TestParam::kAllowlistForCors) != 0;
  }

  bool DeriveOriginFromUrl() {
    return (GetParam() & TestParam::kDeriveOriginFromUrl) != 0;
  }

  const Extension* InstallExtension(
      GURL resource_to_fetch_from_declarative_content_script = GURL()) {
    bool use_declarative_content_script =
        resource_to_fetch_from_declarative_content_script.is_valid();
    const char kContentScriptManifestEntry[] = R"(
          "content_scripts": [{
            "all_frames": true,
            "match_about_blank": true,
            "matches": ["*://fetch-initiator.com/*"],
            "js": ["content_script.js"]
          }],
    )";

    // Note that the hardcoded "key" below matches kExpectedHashedExtensionId
    // (used by the test suite for allowlisting the extension as needed).
    const char kManifestTemplate[] = R"(
        {
          "name": "CrossOriginReadBlockingTest - Extension",
          "key": "%s",
          "version": "1.0",
          "manifest_version": 2,
          "permissions": [
              "tabs",
              "*://fetch-initiator.com/*",
              "*://127.0.0.1/*",  // Initiator in AppCache tests.
              "*://cross-site.com/*",
              "*://*.subdomain.com/*",
              "*://other-with-permission.com/*"
              // This list intentionally does NOT include
              // other-without-permission.com.
          ],
          %s
          "background": {"scripts": ["background_script.js"]}
        } )";
    dir_.WriteManifest(base::StringPrintf(
        kManifestTemplate, kExtensionKey,
        use_declarative_content_script ? kContentScriptManifestEntry : ""));

    dir_.WriteFile(FILE_PATH_LITERAL("background_script.js"), "");
    dir_.WriteFile(FILE_PATH_LITERAL("page.html"), "<body>Hello World!</body>");

    if (use_declarative_content_script) {
      dir_.WriteFile(
          FILE_PATH_LITERAL("content_script.js"),
          CreateFetchScript(resource_to_fetch_from_declarative_content_script));
    }
    extension_ = LoadExtension(dir_.UnpackedPath());
    DCHECK(extension_);

    AllowlistExtensionIfNeeded(*extension_);
    return extension_;
  }

  bool AreContentScriptFetchesExpectedToBeBlocked() {
    return !IsExtensionAllowlisted();
  }

  bool IsCorbExpectedToBeTurnedOffAltogether() {
    return IsExtensionAllowlisted();
  }

  void VerifyPassiveUmaForAllowlistForCors(
      const base::HistogramTester& histograms,
      base::Optional<bool> expected_value) {
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    const char* kUmaName =
        "SiteIsolation.XSD.Browser.AllowedByCorbButNotCors.ContentScript";
    bool expect_uma_presence = expected_value.has_value();

    // This logging is to get an initial estimate, and it won't work once we
    // actually turn the new CORS content script behavior on.
    if (IsOutOfBlinkCorsEnabled() && ShouldAllowlistAlsoApplyToOorCors())
      expect_uma_presence = false;

    // If the extension is allowlisted, then CORB is disabled (and therefore the
    // UMA logging code in CrossOriginReadBlocking::ResponseAnalyzer won't run
    // at all for allowlisted extensions).
    if (IsExtensionAllowlisted())
      expect_uma_presence = false;

    // Verify |expect_uma_presence| and |expected_value|.
    if (!expect_uma_presence) {
      histograms.ExpectTotalCount(kUmaName, 0);
    } else {
      histograms.ExpectUniqueSample(kUmaName, static_cast<int>(*expected_value),
                                    1);
    }
  }

  // Verifies that |console_observer| has captured a console message indicating
  // that CORS has blocked a response.
  //
  // |console_observer| can be either
  // - ServiceWorkerConsoleObserver (defined above in this file)
  // or
  // - content::WebContentsConsoleObserver
  template <typename TConsoleObserver>
  void VerifyFetchWasBlockedByCors(const TConsoleObserver& console_observer) {
    using ConsoleMessage = typename TConsoleObserver::Message;
    const std::vector<ConsoleMessage>& console_messages =
        console_observer.messages();

    std::vector<std::string> messages;
    std::transform(console_messages.begin(), console_messages.end(),
                   std::back_inserter(messages),
                   [](const ConsoleMessage& console_message) {
                     return base::UTF16ToUTF8(console_message.message);
                   });

    if (IsOutOfBlinkCorsEnabled()) {
      // Expect exactly 1 CORS error message.
      EXPECT_THAT(messages, testing::ElementsAre(testing::HasSubstr(
                                "has been blocked by CORS policy")));
    } else {
      // We allow more than 1 error message, because in some test cases there
      // might be 2 error messages (one from InBlink CORS and one from
      // FileURLLoaderFactory).  This doesn't seem worth fixing in product code
      // (because InBlink CORS support will go away soon).
      EXPECT_FALSE(messages.empty());
      EXPECT_THAT(
          messages,
          testing::Each(testing::HasSubstr("has been blocked by CORS policy")));
    }
  }

  void VerifyFetchFromContentScriptWasBlockedByCorb(
      const base::HistogramTester& histograms) {
    // Make sure that histograms logged in other processes (e.g. in
    // NetworkService process) get synced.
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    if (IsCorbExpectedToBeTurnedOffAltogether()) {
      EXPECT_EQ(0u,
                histograms.GetTotalCountsForPrefix("SiteIsolation.XSD.Browser")
                    .size());
      return;
    }

    histograms.ExpectBucketCount("SiteIsolation.XSD.Browser.Action",
                                 CORBAction::kResponseStarted, 1);
    histograms.ExpectBucketCount("SiteIsolation.XSD.Browser.Action",
                                 CORBAction::kBlockedWithoutSniffing, 1);

    // If CORB blocks the response, then there is no risk in enabling
    // CorbAllowlistAlsoAppliesToOorCors and we shouldn't log the UMA.
    VerifyPassiveUmaForAllowlistForCors(histograms, base::nullopt);
  }

  void VerifyFetchFromContentScriptWasAllowedByCorb(
      const base::HistogramTester& histograms,
      bool expecting_sniffing = false) {
    // Make sure that histograms logged in other processes (e.g. in
    // NetworkService process) get synced.
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    if (IsCorbExpectedToBeTurnedOffAltogether()) {
      EXPECT_EQ(0u,
                histograms.GetTotalCountsForPrefix("SiteIsolation.XSD.Browser")
                    .size());
      return;
    }

    histograms.ExpectBucketCount("SiteIsolation.XSD.Browser.Action",
                                 CORBAction::kResponseStarted, 1);
    histograms.ExpectBucketCount("SiteIsolation.XSD.Browser.Action",
                                 expecting_sniffing
                                     ? CORBAction::kAllowedAfterSniffing
                                     : CORBAction::kAllowedWithoutSniffing,
                                 1);
  }

  // Verifies results of fetching a CORB-eligible resource from a content
  // script.  Expectations differ depending on the following:
  // 1. Allowlisted extension: Fetches from content scripts
  //                           should not be blocked
  // 2. Other extension: Fetches from content scripts should be blocked by
  //    either: only CORB or CORS+CORB.
  //
  // This verification helper might not work for non-CORB-eligible resources
  // like MIME types not covered by CORB (e.g. application/octet-stream) or
  // same-origin responses.
  void VerifyCorbEligibleFetchFromContentScript(
      const base::HistogramTester& histograms,
      const content::WebContentsConsoleObserver& console_observer,
      const std::string& actual_fetch_result,
      const std::string& expected_fetch_result) {
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    // VerifyCorbEligibleFetchFromContentScript is only called for Content Types
    // covered by CORB and therefore these requests carry no risk for
    // CorbAllowlistAlsoAppliesToOorCors - verify that we didn't log the UMA.
    VerifyPassiveUmaForAllowlistForCors(histograms, base::nullopt);

    if (AreContentScriptFetchesExpectedToBeBlocked()) {
      if (ShouldAllowlistAlsoApplyToOorCors()) {
        // Verify the fetch was blocked by CORS.
        EXPECT_EQ(kCorsErrorWhenFetching, actual_fetch_result);
        VerifyFetchWasBlockedByCors(console_observer);

        // No verification if the request was blocked by CORB, because
        // 1) once request_initiator is trustworthy, CORB should only
        //    apply to no-cors requests
        // 2) some CORS-blocked requests may not reach CORB/response-started
        //    stage at all (e.g. if CORS blocks a redirect).

        // TODO(lukasza): Verify that the request was made in CORS mode (e.g.
        // included an Origin header).
      } else {
        // Verify the fetch was blocked by CORB, but not blocked by CORS.
        EXPECT_EQ(std::string(), actual_fetch_result);
        VerifyFetchFromContentScriptWasBlockedByCorb(histograms);
      }
    } else {
      // Verify the fetch was allowed.
      EXPECT_EQ(expected_fetch_result, actual_fetch_result);
      VerifyFetchFromContentScriptWasAllowedByCorb(histograms);
    }
  }

  void VerifyNonCorbElligibleFetchFromContentScript(
      const base::HistogramTester& histograms,
      const content::WebContentsConsoleObserver& console_observer,
      const std::string& actual_fetch_result,
      const std::string& expected_fetch_result_prefix) {
    // Verify that CORB sniffing allowed the response.
    VerifyFetchFromContentScriptWasAllowedByCorb(histograms,
                                                 true /* expecting_sniffing */);

    if (ShouldAllowlistAlsoApplyToOorCors() &&
        AreContentScriptFetchesExpectedToBeBlocked()) {
      // Verify that the response body was blocked by CORS.
      EXPECT_EQ(kCorsErrorWhenFetching, actual_fetch_result);
      VerifyFetchWasBlockedByCors(console_observer);
    } else {
      // Verify that the response body was not blocked by either CORB nor CORS.
      EXPECT_THAT(actual_fetch_result,
                  ::testing::StartsWith(expected_fetch_result_prefix));
    }

    // This is the kind of response (i.e., cross-origin fetch of a non-CORB
    // type) that could be affected by the planned
    // CorbAllowlistAlsoAppliesToOorCors feature.
    VerifyPassiveUmaForAllowlistForCors(histograms, true);
  }

  content::WebContents* active_web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  const Extension* InstallExtensionWithPermissionToAllUrls() {
    // Note that the hardcoded "key" below matches kExpectedHashedExtensionId
    // (used by the test suite for allowlisting the extension as needed).
    const char kManifestTemplate[] = R"(
        {
          "name": "CrossOriginReadBlockingTest - Extension/AllUrls",
          "key": "%s",
          "version": "1.0",
          "manifest_version": 2,
          "permissions": [ "tabs", "<all_urls>" ],
          "background": {"scripts": ["background_script.js"]}
        } )";
    dir_.WriteManifest(base::StringPrintf(kManifestTemplate, kExtensionKey));
    dir_.WriteFile(FILE_PATH_LITERAL("background_script.js"), "");
    extension_ = LoadExtension(dir_.UnpackedPath());
    DCHECK(extension_);

    AllowlistExtensionIfNeeded(*extension_);
    return extension_;
  }

  bool RegisterServiceWorkerForExtension(
      const std::string& service_worker_script) {
    const char kServiceWorkerPath[] = "service_worker.js";
    dir_.WriteFile(base::FilePath::FromUTF8Unsafe(kServiceWorkerPath).value(),
                   service_worker_script);

    const char kRegistrationScript[] = R"(
        navigator.serviceWorker.register($1).then(function() {
          // Wait until the service worker is active.
          return navigator.serviceWorker.ready;
        }).then(function(r) {
          window.domAutomationController.send('SUCCESS');
        }).catch(function(err) {
          window.domAutomationController.send('ERROR: ' + err.message);
        }); )";
    std::string registration_script =
        content::JsReplace(kRegistrationScript, kServiceWorkerPath);

    std::string result = browsertest_util::ExecuteScriptInBackgroundPage(
        browser()->profile(), extension_->id(), registration_script);
    if (result != "SUCCESS") {
      ADD_FAILURE() << "Failed to register the service worker: " << result;
      return false;
    }
    return !::testing::Test::HasFailure();
  }

  // Injects (into |web_contents|) a content_script that performs a fetch of
  // |url|. Returns the body of the response.
  //
  // The method below uses "programmatic" (rather than "declarative") way to
  // inject a content script, but the behavior and permissions of the conecnt
  // script should be the same in both cases.  See also
  // https://developer.chrome.com/extensions/content_scripts#programmatic.
  std::string FetchViaContentScript(const GURL& url,
                                    content::WebContents* web_contents) {
    return FetchHelper(
        url,
        base::BindOnce(&CorbAndCorsExtensionBrowserTest::ExecuteContentScript,
                       base::Unretained(this), base::Unretained(web_contents)));
  }

  // Performs a fetch of |url| from the background page of the test extension.
  // Returns the body of the response.
  std::string FetchViaBackgroundPage(const GURL& url) {
    return FetchHelper(
        url, base::BindOnce(
                 &browsertest_util::ExecuteScriptInBackgroundPageNoWait,
                 base::Unretained(browser()->profile()), extension_->id()));
  }

  // Performs a fetch of |url| from |web_contents| (directly, without going
  // through content scripts).  Returns the body of the response.
  std::string FetchViaWebContents(const GURL& url,
                                  content::WebContents* web_contents) {
    return FetchHelper(
        url,
        base::BindOnce(&CorbAndCorsExtensionBrowserTest::ExecuteRegularScript,
                       base::Unretained(this), base::Unretained(web_contents)));
  }

  // Performs a fetch of |url| from a srcdoc subframe added to |parent_frame|
  // and executing a script via <script> tag.  Returns the body of the response.
  std::string FetchViaSrcDocFrame(GURL url,
                                  content::RenderFrameHost* parent_frame) {
    return FetchHelper(
        url,
        base::BindOnce(&CorbAndCorsExtensionBrowserTest::ExecuteInSrcDocFrame,
                       base::Unretained(this), base::Unretained(parent_frame)));
  }

  GURL GetExtensionResource(const std::string& relative_path) {
    return extension_->GetResourceURL(relative_path);
  }

  url::Origin GetExtensionOrigin() {
    return url::Origin::Create(extension_->url());
  }

  GURL GetTestPageUrl(const std::string& hostname) {
    // Using the page below avoids a network fetch of /favicon.ico which helps
    // avoid extra synchronization hassles in the tests.
    return embedded_test_server()->GetURL(
        hostname, "/favicon/title1_with_data_uri_icon.html");
  }

  const Extension* extension() { return extension_; }

  // Asks the test |extension_| to inject |content_script| into |web_contents|.
  //
  // This is an implementation of FetchCallback.
  // Returns true if the content script execution started succeessfully.
  bool ExecuteContentScript(content::WebContents* web_contents,
                            const std::string& content_script) {
    int tab_id = ExtensionTabUtil::GetTabId(web_contents);
    std::string background_script = content::JsReplace(
        "chrome.tabs.executeScript($1, { code: $2 });", tab_id, content_script);
    return browsertest_util::ExecuteScriptInBackgroundPageNoWait(
        browser()->profile(), extension_->id(), background_script);
  }

  void AllowlistExtensionIfNeeded(const Extension& extension) {
    // Sanity check that the field trial param (which has to be registered via
    // ScopedFeatureList early) uses the right extension id hash.
    EXPECT_EQ(kExpectedHashedExtensionId, extension.hashed_id().value());

    if (ShouldAllowlistAlsoApplyToOorCors()) {
      // Allowlist has already been populated via field trial param (see the
      // constructor of CrossOriginReadBlockingExtensionAllowlistingTest).
      return;
    }

    // If field trial param cannot be used, fall back to allowlisting via
    // URLLoaderFactoryManager's test support methods.
    if (IsExtensionAllowlisted()) {
      URLLoaderFactoryManager::AddExtensionToAllowlistForTesting(extension);
    } else {
      URLLoaderFactoryManager::RemoveExtensionFromAllowlistForTesting(
          extension);
    }
  }

 protected:
  policy::MockConfigurationPolicyProvider policy_provider_;

 private:
  // Executes |regular_script| in |web_contents|.
  //
  // This is an implementation of FetchCallback.
  // Returns true if the script execution started succeessfully.
  bool ExecuteRegularScript(content::WebContents* web_contents,
                            const std::string& regular_script) {
    content::ExecuteScriptAsync(web_contents, regular_script);

    // Report artificial success to meet FetchCallback's requirements.
    return true;
  }

  // Injects into |parent_frame| an "srcdoc" subframe that contains/executes
  // |script_to_run_in_subframe| via <script> tag.
  //
  // This function is useful to exercise a scenario when a <script> tag may
  // execute before the browser gets a chance to see the a frame/navigation
  // commit is happening.
  //
  // This is an implementation of FetchCallback.
  // Returns true if the script execution started succeessfully.
  bool ExecuteInSrcDocFrame(content::RenderFrameHost* parent_frame,
                            const std::string& script_to_run_in_subframe) {
    static int sequence_id = 0;
    sequence_id++;
    std::string filename =
        base::StringPrintf("srcdoc_script_%d.js", sequence_id);
    dir_.WriteFile(base::FilePath::FromUTF8Unsafe(filename).value(),
                   script_to_run_in_subframe);

    // Using <script src=...></script> instead of <script>...</script> to avoid
    // extensions CSP which forbids inline scripts.
    const char kScriptTemplate[] = R"(
        var subframe = document.createElement('iframe');
        subframe.srcdoc = '<script src=' + $1 + '></script>';
        document.body.appendChild(subframe); )";
    std::string subframe_injection_script =
        content::JsReplace(kScriptTemplate, filename);
    content::ExecuteScriptAsync(parent_frame, subframe_injection_script);

    // Report artificial success to meet FetchCallback's requirements.
    return true;
  }

  // FetchCallback represents a function that executes |fetch_script|.
  //
  // |fetch_script| will include calls to |domAutomationController.send| and
  // therefore instances of FetchCallback should not inject their own calls to
  // |domAutomationController.send| (e.g. this constraint rules out
  // browsertest_util::ExecuteScriptInBackgroundPage and/or
  // content::ExecuteScript).
  //
  // The function should return true if script execution started successfully.
  //
  // Currently used "implementations":
  // - CorbAndCorsExtensionBrowserTest::ExecuteContentScript(web_contents)
  // - CorbAndCorsExtensionBrowserTest::ExecuteRegularScript(web_contents)
  // - browsertest_util::ExecuteScriptInBackgroundPageNoWait(profile, ext_id)
  using FetchCallback =
      base::OnceCallback<bool(const std::string& fetch_script)>;

  // Returns response body of a fetch of |url| initiated via |fetch_callback|.
  std::string FetchHelper(const GURL& url, FetchCallback fetch_callback) {
    content::DOMMessageQueue message_queue;

    // Inject a content script that performs a cross-origin fetch to
    // cross-site.com.
    EXPECT_TRUE(std::move(fetch_callback).Run(CreateFetchScript(url)));

    // Wait until the message comes back and extract result from the message.
    return PopString(&message_queue);
  }

  const Extension* extension_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(CorbAndCorsExtensionBrowserTest);
};

IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest,
                       FromDeclarativeContentScript_NoSniffXml) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL cross_site_resource(
      embedded_test_server()->GetURL("cross-site.com", "/nosniff.xml"));

  ASSERT_TRUE(InstallExtension(cross_site_resource));

  // Test case #1: Declarative script injected after a browser-initiated
  // navigation of the main frame.
  {
    // Monitor CORB behavior + result of the fetch.
    base::HistogramTester histograms;
    content::WebContentsConsoleObserver console_observer(active_web_contents());
    content::DOMMessageQueue message_queue;

    // Navigate to a fetch-initiator.com page - this should trigger execution of
    // the |content_script| declared in the extension manifest.
    GURL page_url = GetTestPageUrl("fetch-initiator.com");
    ui_test_utils::NavigateToURL(browser(), page_url);
    EXPECT_EQ(page_url,
              active_web_contents()->GetMainFrame()->GetLastCommittedURL());
    EXPECT_EQ(url::Origin::Create(page_url),
              active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

    // Extract results of the fetch done in the declarative content script.
    std::string fetch_result = PopString(&message_queue);

    // Verify whether the fetch worked or not (expectations differ depending on
    // various factors - see the body of
    // VerifyCorbEligibleFetchFromContentScript).
    VerifyCorbEligibleFetchFromContentScript(
        histograms, console_observer, fetch_result, "nosniff.xml - body\n");
  }

  // Test case #2: Declarative script injected after a renderer-initiated
  // creation of an about:blank frame.
  {
    // Monitor CORB behavior + result of the fetch.
    base::HistogramTester histograms;
    content::WebContentsConsoleObserver console_observer(active_web_contents());
    content::DOMMessageQueue message_queue;

    // Inject an about:blank subframe - this should trigger execution of the
    // |content_script| declared in the extension manifest.
    const char kBlankSubframeInjectionScript[] = R"(
        var subframe = document.createElement('iframe');
        document.body.appendChild(subframe); )";
    content::ExecuteScriptAsync(active_web_contents(),
                                kBlankSubframeInjectionScript);

    // Extract results of the fetch done in the declarative content script.
    std::string fetch_result = PopString(&message_queue);

    // Verify whether the fetch worked or not (expectations differ depending on
    // various factors - see the body of
    // VerifyCorbEligibleFetchFromContentScript).
    VerifyCorbEligibleFetchFromContentScript(
        histograms, console_observer, fetch_result, "nosniff.xml - body\n");
  }
}

// Test that verifies the current, baked-in (but not necessarily desirable
// behavior) where a content script injected by an extension can bypass
// CORS (and CORB) for any hosts the extension has access to.
// See also https://crbug.com/846346.
IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest,
                       FromProgrammaticContentScript_NoSniffXml) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a cross-origin fetch to
  // cross-site.com.
  base::HistogramTester histograms;
  content::WebContentsConsoleObserver console_observer(active_web_contents());
  GURL cross_site_resource(
      embedded_test_server()->GetURL("cross-site.com", "/nosniff.xml"));
  std::string fetch_result =
      FetchViaContentScript(cross_site_resource, active_web_contents());

  // Verify whether the fetch worked or not (expectations differ depending on
  // various factors - see the body of
  // VerifyCorbEligibleFetchFromContentScript).
  VerifyCorbEligibleFetchFromContentScript(
      histograms, console_observer, fetch_result, "nosniff.xml - body\n");
}

// Tests that extension permission to bypass CORS is revoked after the extension
// is unloaded.  See also https://crbug.com/843381.
IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest,
                       FromProgrammaticContentScript_UnloadedExtension) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const extensions::Extension* extension = InstallExtension();
  ASSERT_TRUE(extension);

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that adds a link that initiates a fetch from
  // cross-site.com.
  GURL cross_site_resource(
      embedded_test_server()->GetURL("cross-site.com", "/nosniff.xml"));
  {
    const char kNewButtonScriptTemplate[] = R"(
        function startFetch() {
            %s
        }

        var link = document.createElement('a');
        link.href = '#foo';
        link.addEventListener('click', function() {
            startFetch();
        });
        link.id = 'fetch-button';
        link.innerText = 'start-fetch';

        document.body.appendChild(link);
        domAutomationController.send('READY');
    )";
    content::DOMMessageQueue queue;
    ASSERT_TRUE(ExecuteContentScript(
        active_web_contents(),
        base::StringPrintf(kNewButtonScriptTemplate,
                           CreateFetchScript(cross_site_resource).c_str())));
    ASSERT_EQ("READY", PopString(&queue));
  }

  // Click the button - the fetch should work if the extension is allowlisted.
  //
  // Clicking the button will execute the 'click' handler belonging to the
  // content script (i.e. the `startFetch` method defined in the
  // kNewButtonScriptTemplate above).  Directly executing the script via
  // content::ExecuteScript would have executed the script in the main world
  // (which is not what we want).
  const char kFetchInitiatingScript[] = R"(
      document.getElementById('fetch-button').click();
    )";
  {
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::HistogramTester histograms;
    content::WebContentsConsoleObserver console_observer(active_web_contents());

    content::DOMMessageQueue queue;
    content::ExecuteScriptAsync(active_web_contents(), kFetchInitiatingScript);
    std::string fetch_result = PopString(&queue);

    VerifyCorbEligibleFetchFromContentScript(
        histograms, console_observer, fetch_result, "nosniff.xml - body\n");
  }

  // Unload the extension and try fetching again.  The content script should
  // still be present and work, but after the extension is unloaded, the fetch
  // should always fail.  See also https://crbug.com/843381.
  extension_service()->DisableExtension(extension->id(),
                                        disable_reason::DISABLE_USER_ACTION);
  EXPECT_FALSE(ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
      extension->id()));
  {
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::HistogramTester histograms;
    content::WebContentsConsoleObserver console_observer(active_web_contents());

    content::DOMMessageQueue queue;
    content::ExecuteScriptAsync(active_web_contents(), kFetchInitiatingScript);
    std::string fetch_result = PopString(&queue);

    if (IsExtensionAllowlisted() && IsOutOfBlinkCorsEnabled()) {
      // TODO(lukasza): https://crbug.com/1062043: Revoking of extension
      // permissions doesn't cover
      // URLLoaderFactoryParams::factory_bound_access_patterns.
      EXPECT_EQ("nosniff.xml - body\n", fetch_result);
      VerifyFetchFromContentScriptWasAllowedByCorb(histograms);
    } else {
      EXPECT_EQ(kCorsErrorWhenFetching, fetch_result);
      VerifyFetchFromContentScriptWasBlockedByCorb(histograms);
      VerifyFetchWasBlockedByCors(console_observer);
    }
  }
}

// Test that <all_urls> permission does not apply to hosts blocked by policy.
IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest,
                       ContentScriptVsHostBlockedByPolicy_NoSniffXml) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtensionWithPermissionToAllUrls());
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://*.example.com");
    pref.AddPolicyAllowedHost("*", "*://public.example.com");
  }

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Test fetch from a host allowed by the policy (and allowed by the extension
  // permissions).
  {
    SCOPED_TRACE(::testing::Message() << "Allowed by policy");
    base::HistogramTester histograms;
    content::WebContentsConsoleObserver console_observer(active_web_contents());
    GURL cross_site_resource(
        embedded_test_server()->GetURL("public.example.com", "/nosniff.xml"));
    std::string fetch_result =
        FetchViaContentScript(cross_site_resource, active_web_contents());

    // Verify whether the fetch worked or not (expectations differ depending on
    // various factors - see the body of
    // VerifyCorbEligibleFetchFromContentScript).
    VerifyCorbEligibleFetchFromContentScript(
        histograms, console_observer, fetch_result, "nosniff.xml - body\n");
  }

  // Test fetch from a host blocked by the policy (and allowed by the extension
  // permissions).
  {
    SCOPED_TRACE(::testing::Message() << "Blocked by policy");
    base::HistogramTester histograms;
    content::WebContentsConsoleObserver console_observer(active_web_contents());
    GURL cross_site_resource(
        embedded_test_server()->GetURL("example.com", "/nosniff.xml"));
    std::string fetch_result =
        FetchViaContentScript(cross_site_resource, active_web_contents());

    // Verify that the fetch was blocked by CORS.
    EXPECT_EQ(kCorsErrorWhenFetching, fetch_result);
    VerifyFetchFromContentScriptWasBlockedByCorb(histograms);
    VerifyFetchWasBlockedByCors(console_observer);
  }
}

// Test that <all_urls> permission does not apply to hosts blocked by policy.
IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest,
                       ContentScriptVsHostBlockedByPolicy_AllowedTextResource) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtensionWithPermissionToAllUrls());
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://*.example.com");
    pref.AddPolicyAllowedHost("*", "*://public.example.com");
  }

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Test fetch from a host allowed by the policy (and allowed by the extension
  // permissions).
  {
    SCOPED_TRACE(::testing::Message() << "Allowed by policy");
    base::HistogramTester histograms;
    content::WebContentsConsoleObserver console_observer(active_web_contents());
    GURL cross_site_resource(embedded_test_server()->GetURL(
        "public.example.com", "/save_page/text.txt"));
    std::string fetch_result =
        FetchViaContentScript(cross_site_resource, active_web_contents());

    // Verify that the fetch was allowed by CORB.  CORS expectations differ
    // depending on exact scenario.
    VerifyNonCorbElligibleFetchFromContentScript(
        histograms, console_observer, fetch_result,
        "text-object.txt: ae52dd09-9746-4b7e-86a6-6ada5e2680c2");
  }

  // Test fetch from a host blocked by the policy (and allowed by the extension
  // permissions).
  {
    SCOPED_TRACE(::testing::Message() << "Blocked by policy");
    base::HistogramTester histograms;
    content::WebContentsConsoleObserver console_observer(active_web_contents());
    GURL cross_site_resource(
        embedded_test_server()->GetURL("example.com", "/save_page/text.txt"));
    std::string fetch_result =
        FetchViaContentScript(cross_site_resource, active_web_contents());

    // Verify that the fetch was blocked by CORS.
    EXPECT_EQ(kCorsErrorWhenFetching, fetch_result);
    VerifyFetchFromContentScriptWasAllowedByCorb(histograms,
                                                 true /* expecting_sniffing */);
    VerifyFetchWasBlockedByCors(console_observer);
  }
}

IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest,
                       FromProgrammaticContentScript_PermissionToAllUrls) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtensionWithPermissionToAllUrls());

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a cross-origin fetch to
  // cross-site.com.
  base::HistogramTester histograms;
  content::WebContentsConsoleObserver console_observer(active_web_contents());
  GURL cross_site_resource(
      embedded_test_server()->GetURL("cross-site.com", "/nosniff.xml"));
  std::string fetch_result =
      FetchViaContentScript(cross_site_resource, active_web_contents());

  // Verify whether the fetch worked or not (expectations differ depending on
  // various factors - see the body of
  // VerifyCorbEligibleFetchFromContentScript).
  VerifyCorbEligibleFetchFromContentScript(
      histograms, console_observer, fetch_result, "nosniff.xml - body\n");
}

// Verification that granting file access to extensions doesn't relax CORS in
// case of requests to file: URLs (even from content scripts of allowlisted
// extensions with <all_urls> permission).  See also
// https://crbug.com/1049604#c14.
IN_PROC_BROWSER_TEST_P(
    CorbAndCorsExtensionBrowserTest,
    FromProgrammaticContentScript_PermissionToAllUrls_FileUrls) {
  // Install the extension and verify that the extension has access to file URLs
  // (<all_urls> permission is not sufficient - the extension has to be
  // additionally granted file access by passing kFlagEnableFileAccess in
  // ExtensionBrowserTest::LoadExtension).
  const Extension* extension = InstallExtensionWithPermissionToAllUrls();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(util::AllowFileAccess(
      extension->id(), active_web_contents()->GetBrowserContext()));

  // Gather the test URLs.
  GURL page_url = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("title1.html")));
  GURL same_dir_resource = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("title2.html")));
  ASSERT_EQ(url::kFileScheme, page_url.scheme());
  ASSERT_EQ(url::kFileScheme, same_dir_resource.scheme());

  // Navigate to a file:// test page.
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  const char kScriptTemplate[] = R"(
      const url = $1;
      const xhr = new XMLHttpRequest();
      xhr.onload = () => domAutomationController.send(xhr.responseText);
      xhr.onerror = () => domAutomationController.send('XHR ERROR');
      xhr.open("GET", url);
      xhr.send()
  )";
  std::string script = content::JsReplace(kScriptTemplate, same_dir_resource);

  // Sanity check: file: -> file: XHR (no extensions involved) should be blocked
  // by CORS equivalent inside FileURLLoaderFactory.  (All file: URLs are
  // treated as an opaque origin, so all such XHRs would be considered
  // cross-origin.)
  {
    base::HistogramTester histograms;
    content::WebContentsConsoleObserver console_observer(active_web_contents());
    content::DOMMessageQueue queue;
    ExecuteScriptAsync(active_web_contents(), script);
    std::string xhr_result = PopString(&queue);

    // Verify that the XHR was blocked by CORS-equivalent in
    // FileURLLoaderFactory.
    EXPECT_EQ("XHR ERROR", xhr_result);
    VerifyFetchWasBlockedByCors(console_observer);

    // CORB is not used from FileURLLoaderFactory - verify that no CORB UMAs
    // have been logged.
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    EXPECT_EQ(
        0u,
        histograms.GetTotalCountsForPrefix("SiteIsolation.XSD.Browser").size());
  }

  // Inject a content script that performs a cross-origin XHR from another file:
  // URL (all file: URLs are treated as an opaque origin, so all such XHRs would
  // be considered cross-origin).
  //
  // The ability to inject content scripts into file: URLs comes from a
  // combination of <all_urls> and granting file access to the extension.
  //
  // The script below uses the XMLHttpRequest API, rather than fetch API,
  // because the fetch API doesn't support file: requests currently
  // (see https://crbug.com/1051594#c9 and https://crbug.com/1051597#c19).
  {
    base::HistogramTester histograms;
    content::WebContentsConsoleObserver console_observer(active_web_contents());
    content::DOMMessageQueue queue;
    ExecuteContentScript(active_web_contents(), script);
    std::string xhr_result = PopString(&queue);

    // Verify that the XHR was blocked by CORS-equivalent in
    // FileURLLoaderFactory (even though the extension has <all_urls> permission
    // and was granted file access).
    EXPECT_EQ("XHR ERROR", xhr_result);
    VerifyFetchWasBlockedByCors(console_observer);

    // CORB is not used from FileURLLoaderFactory - verify that no CORB UMAs
    // have been logged.
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    EXPECT_EQ(
        0u,
        histograms.GetTotalCountsForPrefix("SiteIsolation.XSD.Browser").size());
  }
}

// Coverage of *.subdomain.com extension permissions for CORB-eligible fetches
// (via nosniff.xml).
IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest,
                       FromProgrammaticContentScript_SubdomainPermissions) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Verify behavior for fetching URLs covered by extension permissions.
  GURL kAllowedUrls[] = {
      embedded_test_server()->GetURL("subdomain.com", "/nosniff.xml"),
      embedded_test_server()->GetURL("foo.subdomain.com", "/nosniff.xml"),
  };
  for (const GURL& allowed_url : kAllowedUrls) {
    SCOPED_TRACE(::testing::Message() << "allowed_url = " << allowed_url);

    base::HistogramTester histograms;
    content::WebContentsConsoleObserver console_observer(active_web_contents());
    std::string fetch_result =
        FetchViaContentScript(allowed_url, active_web_contents());

    // Verify whether the fetch worked or not (expectations differ depending on
    // various factors - see the body of
    // VerifyCorbEligibleFetchFromContentScript).
    VerifyCorbEligibleFetchFromContentScript(
        histograms, console_observer, fetch_result, "nosniff.xml - body\n");
  }
}

// Test that verifies the current, baked-in (but not necessarily desirable
// behavior) where a content script injected by an extension can bypass
// CORS (and CORB) for any hosts the extension has access to.
// See also https://crbug.com/1034408 and https://crbug.com/846346.
IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest,
                       FromProgrammaticContentScript_RedirectToNoSniffXml) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a cross-origin fetch to
  // cross-site.com.
  base::HistogramTester histograms;
  content::WebContentsConsoleObserver console_observer(active_web_contents());
  GURL cross_site_resource(
      embedded_test_server()->GetURL("cross-site.com", "/nosniff.xml"));
  GURL redirecting_url(embedded_test_server()->GetURL(
      "other-with-permission.com",
      std::string("/server-redirect?") + cross_site_resource.spec()));
  std::string fetch_result =
      FetchViaContentScript(redirecting_url, active_web_contents());

  // Verify whether the fetch worked or not (expectations differ depending on
  // various factors - see the body of
  // VerifyCorbEligibleFetchFromContentScript).
  VerifyCorbEligibleFetchFromContentScript(
      histograms, console_observer, fetch_result, "nosniff.xml - body\n");
}

// Test that verifies CORS-allowed fetches work for targets that are not
// covered by the extension permissions.
IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest,
                       ContentScript_CorsAllowedByServer_NoPermissionToTarget) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a cross-origin fetch to
  // other-without-permission.com.
  base::HistogramTester histograms;
  GURL cross_site_resource(embedded_test_server()->GetURL(
      "other-without-permission.com", "/cors-ok.txt"));
  std::string fetch_result =
      FetchViaContentScript(cross_site_resource, active_web_contents());

  // Verify that the fetch succeeded (because of the server's
  // Access-Control-Allow-Origin response header).
  EXPECT_EQ("cors-ok.txt - body\n", fetch_result);
  VerifyFetchFromContentScriptWasAllowedByCorb(histograms);
}

// Test that verifies that CORS blocks non-CORB-eligible fetches for targets
// that are not covered by the extension permissions.
IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest,
                       ContentScript_CorsIgnoredByServer_NoPermissionToTarget) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a cross-origin fetch to
  // other-without-permission.com.
  base::HistogramTester histograms;
  content::WebContentsConsoleObserver console_observer(active_web_contents());
  GURL cross_site_resource(embedded_test_server()->GetURL(
      "other-without-permission.com", "/save_page/text.txt"));
  std::string fetch_result =
      FetchViaContentScript(cross_site_resource, active_web_contents());

  // Verify that the fetch was blocked by CORS (because the extension has no
  // permission to the target + server didn't reply with
  // Access-Control-Allow-Origin response header).
  EXPECT_EQ(kCorsErrorWhenFetching, fetch_result);
  VerifyFetchWasBlockedByCors(console_observer);

  // Verify that the fetch was allowed by CORB (because the response sniffed as
  // didn't sniff as html/xml/json).
  VerifyFetchFromContentScriptWasAllowedByCorb(histograms,
                                               true /* expecting_sniffing */);
}

// Tests that same-origin fetches (same-origin relative to the webpage the
// content script is injected into) are allowed.  See also
// https://crbug.com/918660.
IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest,
                       FromProgrammaticContentScript_SameOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a same-origin fetch to
  // fetch-initiator.com.
  base::HistogramTester histograms;
  GURL same_origin_resource(
      embedded_test_server()->GetURL("fetch-initiator.com", "/nosniff.xml"));
  std::string fetch_result =
      FetchViaContentScript(same_origin_resource, active_web_contents());

  // Verify that no blocking occurred.
  EXPECT_THAT(fetch_result, ::testing::StartsWith("nosniff.xml - body"));
  VerifyFetchFromContentScriptWasAllowedByCorb(histograms,
                                               false /* expecting_sniffing */);

  // Same-origin requests are not at risk of being broken.
  VerifyPassiveUmaForAllowlistForCors(histograms, false);
}

// Test that responses that would have been allowed by CORB anyway are not
// reported to LogInitiatorSchemeBypassingDocumentBlocking.
IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest,
                       FromProgrammaticContentScript_AllowedTextResource) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a cross-origin fetch to
  // cross-site.com.
  base::HistogramTester histograms;
  content::WebContentsConsoleObserver console_observer(active_web_contents());
  GURL cross_site_resource(
      embedded_test_server()->GetURL("cross-site.com", "/save_page/text.txt"));
  std::string fetch_result =
      FetchViaContentScript(cross_site_resource, active_web_contents());

  // Verify that the fetch was allowed by CORB.  CORS expectations differ
  // depending on exact scenario.
  VerifyNonCorbElligibleFetchFromContentScript(
      histograms, console_observer, fetch_result,
      "text-object.txt: ae52dd09-9746-4b7e-86a6-6ada5e2680c2");
}

// Coverage of *.subdomain.com extension permissions for non-CORB eligible
// fetches (via save_page/text.txt).
IN_PROC_BROWSER_TEST_P(
    CorbAndCorsExtensionBrowserTest,
    FromProgrammaticContentScript_AllowedTextResource_SubdomainPermissions) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Verify behavior for fetching URLs covered by extension permissions.
  GURL kAllowedUrls[] = {
      embedded_test_server()->GetURL("subdomain.com", "/save_page/text.txt"),
      embedded_test_server()->GetURL("x.subdomain.com", "/save_page/text.txt"),
  };
  for (const GURL& allowed_url : kAllowedUrls) {
    SCOPED_TRACE(::testing::Message() << "allowed_url = " << allowed_url);

    // Inject a content script that performs a cross-origin fetch to
    // cross-site.com.
    base::HistogramTester histograms;
    content::WebContentsConsoleObserver console_observer(active_web_contents());
    std::string fetch_result =
        FetchViaContentScript(allowed_url, active_web_contents());

    // Verify that CORB sniffing allowed the response.
    VerifyNonCorbElligibleFetchFromContentScript(
        histograms, console_observer, fetch_result,
        "text-object.txt: ae52dd09-9746-4b7e-86a6-6ada5e2680c2");
  }
}

// Test that responses that would have been allowed by CORB after sniffing are
// included in the AllowedByCorbButNotCors UMA.
IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest,
                       FromProgrammaticContentScript_AllowedAfterSniffing) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a cross-origin fetch to
  // cross-site.com (to a PNG image that is incorrectly labelled as
  // `Content-Type: text/html`).
  base::HistogramTester histograms;
  content::WebContentsConsoleObserver console_observer(active_web_contents());
  GURL cross_site_resource(embedded_test_server()->GetURL(
      "cross-site.com", "/downloads/image-labeled-as-html.png"));
  std::string fetch_result =
      FetchViaContentScript(cross_site_resource, active_web_contents());

  // Verify that CORB sniffing allowed the response.
  VerifyNonCorbElligibleFetchFromContentScript(histograms, console_observer,
                                               fetch_result, "\xEF\xBF\xBDPNG");
}

// Test that responses are blocked by CORB, but have empty response body are not
// reported to LogInitiatorSchemeBypassingDocumentBlocking.
IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest,
                       FromProgrammaticContentScript_EmptyAndBlocked) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a cross-origin fetch to
  // cross-site.com.
  base::HistogramTester histograms;
  content::WebContentsConsoleObserver console_observer(active_web_contents());
  GURL cross_site_resource(
      embedded_test_server()->GetURL("cross-site.com", "/nosniff.empty"));
  std::string fetch_result =
      FetchViaContentScript(cross_site_resource, active_web_contents());

  // Verify whether the fetch worked or not (expectations differ depending on
  // various factors - see the body of
  // VerifyCorbEligibleFetchFromContentScript).
  VerifyCorbEligibleFetchFromContentScript(histograms, console_observer,
                                           fetch_result,
                                           "" /* expected_response_body */);
}

// Test that LogInitiatorSchemeBypassingDocumentBlocking exits early for
// requests that aren't from content scripts.
IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest,
                       FromBackgroundPage_NoSniffXml) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Performs a cross-origin fetch from the background page.
  base::HistogramTester histograms;
  GURL cross_site_resource(
      embedded_test_server()->GetURL("cross-site.com", "/nosniff.xml"));
  std::string fetch_result = FetchViaBackgroundPage(cross_site_resource);

  // Verify that no blocking occurred.
  EXPECT_EQ("nosniff.xml - body\n", fetch_result);
}

// Test that requests from a extension page hosted in a foreground tab use
// relaxed CORB processing.
IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest,
                       FromForegroundPage_NoSniffXml) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate a tab to an extension page.
  ui_test_utils::NavigateToURL(browser(), GetExtensionResource("page.html"));
  ASSERT_EQ(GetExtensionOrigin(),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Test case #1: Fetch from a chrome-extension://... main frame.
  {
    // Perform a cross-origin fetch from the foreground extension page.
    base::HistogramTester histograms;
    GURL cross_site_resource(
        embedded_test_server()->GetURL("cross-site.com", "/nosniff.xml"));
    std::string fetch_result =
        FetchViaWebContents(cross_site_resource, active_web_contents());

    // Verify that no blocking occurred.
    EXPECT_EQ("nosniff.xml - body\n", fetch_result);
  }

  // Test case #2: Fetch from an about:srcdoc subframe of a
  // chrome-extension://... frame.
  {
    // Perform a cross-origin fetch from the foreground extension page.
    base::HistogramTester histograms;
    GURL cross_site_resource(
        embedded_test_server()->GetURL("cross-site.com", "/nosniff.xml"));
    std::string fetch_result = FetchViaSrcDocFrame(
        cross_site_resource, active_web_contents()->GetMainFrame());

    // Verify that no blocking occurred.
    EXPECT_EQ("nosniff.xml - body\n", fetch_result);
  }
}

// Test that requests from an extension's service worker to the network use
// relaxed CORB processing (both in the case of requests that 1) are initiated
// by the service worker and/or 2) are ignored by the service worker and fall
// back to the network).
IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest,
                       FromRegisteredServiceWorker_NoSniffXml) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Register the service worker which injects "SERVICE WORKER INTERCEPT: "
  // prefix to the body of each response.
  const char kServiceWorkerScript[] = R"(
      self.addEventListener('fetch', function(event) {
          // Intercept all http requests to cross-site.com and inject
          // 'SERVICE WORKER INTERCEPT:' prefix.
          if (event.request.url.startsWith('http://cross-site.com')) {
            event.respondWith(
                // By using the 'fetch' call below, the service worker initiates
                // a network request that will go through the URLLoaderFactory
                // created via CreateFactoryBundle called / posted indirectly
                // from EmbeddedWorkerInstance::StartTask::Start.
                fetch(event.request)
                    .then(response => response.text())
                    .then(text => new Response(
                        'SERVICE WORKER INTERCEPT: >>>' + text + '<<<')));
          }

          // Let the request go directly to the network in all the other cases,
          // like:
          // - loading the extension resources like page.html (avoiding going
          //   through the service worker is required for correctness of test
          //   setup),
          // - handling the cross-origin fetch to other.com in test case #2.
          // Note that these requests will use the URLLoaderFactory owned by
          // ServiceWorkerSubresourceLoader which can be different to the
          // network loader factory owned by the ServiceWorker thread (which is
          // used for fetch intiated by the service worker above).
      }); )";
  ASSERT_TRUE(RegisterServiceWorkerForExtension(kServiceWorkerScript));

  // Navigate a tab to an extension page.
  ui_test_utils::NavigateToURL(browser(), GetExtensionResource("page.html"));
  ASSERT_EQ(GetExtensionOrigin(),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Verify that the service worker controls the fetches.
  bool is_controlled_by_service_worker = false;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents(),
      "domAutomationController.send(!!navigator.serviceWorker.controller)",
      &is_controlled_by_service_worker));
  ASSERT_TRUE(is_controlled_by_service_worker);

  // Test case #1: Network fetch initiated by the service worker.
  //
  // This covers URLLoaderFactory owned by the ServiceWorker thread and created
  // created via CreateFactoryBundle called / posted indirectly from
  // EmbeddedWorkerInstance::StartTask::Start.
  {
    // Perform a cross-origin fetch from the foreground extension page.
    // This should be intercepted by the service worker installed above.
    base::HistogramTester histograms;
    GURL cross_site_resource_intercepted_by_service_worker(
        embedded_test_server()->GetURL("cross-site.com", "/nosniff.xml"));
    std::string fetch_result =
        FetchViaWebContents(cross_site_resource_intercepted_by_service_worker,
                            active_web_contents());

    // Verify that no blocking occurred (and that the response really did go
    // through the service worker).
    EXPECT_EQ("SERVICE WORKER INTERCEPT: >>>nosniff.xml - body\n<<<",
              fetch_result);
  }

  // Test case #2: Network fetch used as a fallback when service worker ignores
  // the 'fetch' event.
  //
  // This covers URLLoaderFactory owned by the ServiceWorkerSubresourceLoader,
  // which can be different to the network loader factory owned by the
  // ServiceWorker thread (which is used in test case #1).
  {
    // Perform a cross-origin fetch from the foreground extension page.
    // This should be intercepted by the service worker installed above.
    base::HistogramTester histograms;
    GURL cross_site_resource_ignored_by_service_worker(
        embedded_test_server()->GetURL("other-with-permission.com",
                                       "/nosniff.xml"));
    std::string fetch_result = FetchViaWebContents(
        cross_site_resource_ignored_by_service_worker, active_web_contents());

    // Verify that no blocking occurred.
    EXPECT_EQ("nosniff.xml - body\n", fetch_result);
  }
}

#if defined(OS_LINUX)
// Flaky on Linux, especially under sanitizers: https://crbug.com/1073052
#define MAYBE_FromBackgroundServiceWorker_NoSniffXml \
  DISABLED_FromBackgroundServiceWorker_NoSniffXml
#else
#define MAYBE_FromBackgroundServiceWorker_NoSniffXml \
  FromBackgroundServiceWorker_NoSniffXml
#endif

IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest,
                       MAYBE_FromBackgroundServiceWorker_NoSniffXml) {
  // Install the extension with a service worker that can be asked to start a
  // fetch to an arbitrary URL.
  const char kManifestTemplate[] = R"(
      {
        "name": "CrossOriginReadBlockingTest - Extension/BgServiceWorker",
        "key": "%s",
        "version": "1.0",
        "manifest_version": 2,
        "permissions": [
            "*://cross-site.com/*"
            // This list intentionally does NOT include
            // other-without-permission.com.
        ],
        "background": {"service_worker": "sw.js"}
      } )";
  const char kServiceWorker[] = R"(
      chrome.runtime.onMessage.addListener(
          function(request, sender, sendResponse) {
            if (request.url) {
              fetch(request.url)
                .then(response => response.text())
                .then(text => sendResponse(text))
                .catch(err => sendResponse('error: ' + err));
              return true;
            }
          });
  )";
  dir_.WriteManifest(base::StringPrintf(kManifestTemplate, kExtensionKey));
  dir_.WriteFile(FILE_PATH_LITERAL("sw.js"), kServiceWorker);
  dir_.WriteFile(FILE_PATH_LITERAL("page.html"), "<body>Hello World!</body>");
  const Extension* extension = LoadExtension(dir_.UnpackedPath());
  ASSERT_TRUE(extension);
  AllowlistExtensionIfNeeded(*extension);

  // Navigate a foreground tab to an extension URL, so that from this tab we can
  // ask the background service worker to initiate test fetches.
  ui_test_utils::NavigateToURL(browser(),
                               extension->GetResourceURL("page.html"));
  const char kFetchTemplate[] = R"(
      chrome.runtime.sendMessage({url: $1}, function(response) {
          domAutomationController.send(response);
      });
  )";
  ASSERT_TRUE(embedded_test_server()->Start());

  // Test a request to a website covered by extension permissions.
  {
    GURL nosniff_xml_with_permission(
        embedded_test_server()->GetURL("cross-site.com", "/nosniff.xml"));
    content::DOMMessageQueue queue;
    base::HistogramTester histograms;
    content::ExecuteScriptAsync(
        active_web_contents(),
        content::JsReplace(kFetchTemplate, nosniff_xml_with_permission));
    std::string fetch_result = PopString(&queue);

    // Verify that no CORB or CORS blocking occurred.
    EXPECT_EQ("nosniff.xml - body\n", fetch_result);

    // CORB should be disabled for extension origins.
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    EXPECT_EQ(
        0u,
        histograms.GetTotalCountsForPrefix("SiteIsolation.XSD.Browser").size());
  }

  // Test a request to a website *not* covered by extension permissions.
  {
    GURL nosniff_xml_with_permission(embedded_test_server()->GetURL(
        "other-without-permission.com", "/nosniff.xml"));
    content::DOMMessageQueue queue;
    base::HistogramTester histograms;
    ServiceWorkerConsoleObserver console_observer(
        active_web_contents()->GetBrowserContext());
    content::ExecuteScriptAsync(
        active_web_contents(),
        content::JsReplace(kFetchTemplate, nosniff_xml_with_permission));
    std::string fetch_result = PopString(&queue);

    // Verify that CORS blocked the response.
    EXPECT_EQ(kCorsErrorWhenFetching, fetch_result);
    console_observer.WaitForMessages();
    VerifyFetchWasBlockedByCors(console_observer);

    // CORB should be disabled for extension origins.
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    EXPECT_EQ(
        0u,
        histograms.GetTotalCountsForPrefix("SiteIsolation.XSD.Browser").size());
  }
}

class ReadyToCommitWaiter : public content::WebContentsObserver {
 public:
  explicit ReadyToCommitWaiter(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  ~ReadyToCommitWaiter() override {}

  void Wait() { run_loop_.Run(); }

  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override {
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ReadyToCommitWaiter);
};

IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest,
                       ProgrammaticContentScriptVsWebUI) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Try to inject a content script just as we are about to commit a WebUI page.
  // This will cause ExecuteCodeInTabFunction::CanExecuteScriptOnPage to execute
  // while RenderFrameHost::GetLastCommittedOrigin() still corresponds to the
  // old page.
  {
    // Initiate navigating a new, blank tab (this avoids process swaps which
    // would otherwise occur when navigating to a WebUI pages from either the
    // NTP or from a web page).  This simulates choosing "Settings" from the
    // main menu.
    GURL web_ui_url("chrome://settings");
    NavigateParams nav_params(
        browser(), web_ui_url,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED));
    nav_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    content::WebContentsAddedObserver new_web_contents_observer;
    Navigate(&nav_params);

    // Capture the new WebContents.
    content::WebContents* new_web_contents =
        new_web_contents_observer.GetWebContents();
    ReadyToCommitWaiter ready_to_commit_waiter(new_web_contents);
    content::TestNavigationObserver navigation_observer(new_web_contents, 1);

    // Repro of https://crbug.com/894766 requires that no cross-process swap
    // takes place - this is what happens when navigating an initial/blank tab.

    // Wait until ReadyToCommit happens.
    //
    // For the repro to happen, content script injection needs to run
    // 1) after RenderFrameHostImpl::CommitNavigation
    //    (which runs in the same revolution of the message pump as
    //    WebContentsObserver::ReadyToCommitNavigation)
    // 2) before RenderFrameHostImpl::DidCommitProvisionalLoad
    //    (task posted from ReadyToCommitNavigation above should execute
    //     before any IPC responses that come after PostTask call).
    ready_to_commit_waiter.Wait();
    DCHECK_NE(web_ui_url,
              active_web_contents()->GetMainFrame()->GetLastCommittedURL());

    // Inject the content script (simulating chrome.tabs.executeScript, but
    // using TabsExecuteScriptFunction directly to ensure the right timing).
    int tab_id = ExtensionTabUtil::GetTabId(active_web_contents());
    const char kArgsTemplate[] = R"(
        [%d, {"code": "
            var p = document.createElement('p');
            p.innerText = 'content script injection succeeded unexpectedly';
            p.id = 'content-script-injection-result';
            document.body.appendChild(p);
        "}] )";
    std::string args = base::StringPrintf(kArgsTemplate, tab_id);
    auto function = base::MakeRefCounted<TabsExecuteScriptFunction>();
    function->set_extension(extension());
    std::string actual_error =
        extension_function_test_utils::RunFunctionAndReturnError(
            function.get(), args, browser());
    std::string expected_error =
        "Cannot access contents of url \"chrome://settings/\". "
        "Extension manifest must request permission to access this host.";
    EXPECT_EQ(expected_error, actual_error);

    // Wait until the navigation completes.
    navigation_observer.Wait();
    EXPECT_EQ(web_ui_url,
              active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  }

  // Check if the injection above succeeded (it shouldn't have, because of
  // renderer-side checks).
  const char kInjectionVerificationScript[] = R"(
      domAutomationController.send(
          !!document.getElementById('content-script-injection-result')); )";
  bool has_content_script_run = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(active_web_contents(),
                                                   kInjectionVerificationScript,
                                                   &has_content_script_run));
  EXPECT_FALSE(has_content_script_run);

  // Try to fetch a WebUI resource (i.e. verify that the unsucessful content
  // script injection above didn't clobber the WebUI-specific URLLoaderFactory).
  const char kScript[] = R"(
      var img = document.createElement('img');
      img.src = 'chrome://resources/images/arrow_down.svg';
      img.onload = () => domAutomationController.send('LOADED');
      img.onerror = e => domAutomationController.send('ERROR: ' + e);
  )";
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(active_web_contents(),
                                                     kScript, &result));
  EXPECT_EQ("LOADED", result);
}

IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest,
                       ProgrammaticContentScriptVsAppCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Set up http server serving files from content/test/data (which conveniently
  // already contains appcache-related test files, unlike chrome/test/data).
  net::EmbeddedTestServer content_test_data_server;
  content_test_data_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(content_test_data_server.Start());

  // Load the main page twice. The second navigation should have AppCache
  // initialized for the page.
  //
  // Note that localhost / 127.0.0.1 need to be used, because Application Cache
  // is restricted to secure contexts.
  GURL main_url = content_test_data_server.GetURL(
      "127.0.0.1", "/appcache/simple_page_with_manifest.html");
  ui_test_utils::NavigateToURL(browser(), main_url);
  base::string16 expected_title = base::ASCIIToUTF16("AppCache updated");
  content::TitleWatcher title_watcher(active_web_contents(), expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  ui_test_utils::NavigateToURL(browser(), main_url);

  // Turn off the server and sanity check that the resource is still available.
  const char kScriptTemplate[] = R"(
      new Promise(function (resolve, reject) {
          var img = document.createElement('img');
          img.src = '/appcache/' + $1;
          img.onload = _ => resolve('IMG LOADED');
          img.onerror = reject;
      })
  )";
  ASSERT_TRUE(content_test_data_server.ShutdownAndWaitUntilComplete());
  EXPECT_EQ("IMG LOADED",
            content::EvalJs(active_web_contents(),
                            content::JsReplace(kScriptTemplate, "logo.png")));

  // Inject a content script and verify that this doesn't negatively impact
  // AppCache (i.e. verify that
  // RenderFrameHostImpl::MarkInitiatorsAsRequiringSeparateURLLoaderFactory
  // does not clobber the default URLLoaderFactory).
  {
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::HistogramTester histograms;
    content::WebContentsConsoleObserver console_observer(active_web_contents());
    GURL cross_site_resource(
        embedded_test_server()->GetURL("cross-site.com", "/nosniff.xml"));
    std::string fetch_result =
        FetchViaContentScript(cross_site_resource, active_web_contents());

    // Verify whether the fetch worked or not (expectations differ depending on
    // various factors - see the body of
    // VerifyCorbEligibleFetchFromContentScript).
    VerifyCorbEligibleFetchFromContentScript(
        histograms, console_observer, fetch_result, "nosniff.xml - body\n");
  }
  // Using a different image, to bypass renderer-side caching.
  EXPECT_EQ("IMG LOADED",
            content::EvalJs(active_web_contents(),
                            content::JsReplace(kScriptTemplate, "logo2.png")));

  // Crash the network service and wait for things to come back up.  This (and
  // the remaining part of the test) only makes sense if 1) the network service
  // is enabled and running in a separate process and 2) the frame has at least
  // one network-bound URLLoaderFactory (i.e. the test extension is
  // allowlisted).
  if (!content::IsOutOfProcessNetworkService() || !IsExtensionAllowlisted())
    return;
  SimulateNetworkServiceCrash();
  active_web_contents()
      ->GetMainFrame()
      ->FlushNetworkAndNavigationInterfacesForTesting();

  // Make sure that both requests still work - the code should have recovered
  // from the crash by 1) refreshing the URLLoaderFactory for the content script
  // and 2) without cloberring the default factory for the AppCache.
  {
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::HistogramTester histograms;
    content::WebContentsConsoleObserver console_observer(active_web_contents());
    GURL cross_site_resource(
        embedded_test_server()->GetURL("cross-site.com", "/nosniff.xml"));
    std::string fetch_result =
        FetchViaContentScript(cross_site_resource, active_web_contents());

    // Verify whether the fetch worked or not (expectations differ depending on
    // various factors - see the body of
    // VerifyCorbEligibleFetchFromContentScript).
    VerifyCorbEligibleFetchFromContentScript(
        histograms, console_observer, fetch_result, "nosniff.xml - body\n");
  }
  // Using a different image, to bypass renderer-side caching.
  EXPECT_EQ("IMG LOADED",
            content::EvalJs(active_web_contents(),
                            content::JsReplace(kScriptTemplate, "logo3.png")));
}

using CorbAndCorsAppBrowserTest = CorbAndCorsExtensionTestBase;

IN_PROC_BROWSER_TEST_F(CorbAndCorsAppBrowserTest, WebViewContentScript) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load the test app.
  const char kManifest[] = R"(
      {
        "name": "CrossOriginReadBlockingTest - App",
        "version": "1.0",
        "manifest_version": 2,
        "permissions": ["*://*/*", "webview"],
        "app": {
          "background": {
            "scripts": ["background_script.js"]
          }
        }
      } )";
  dir_.WriteManifest(kManifest);
  const char kBackgroungScript[] = R"(
      chrome.app.runtime.onLaunched.addListener(function() {
        chrome.app.window.create('page.html', {}, function () {});
      });
  )";
  dir_.WriteFile(FILE_PATH_LITERAL("background_script.js"), kBackgroungScript);
  const char kPage[] = R"(
      <div id="webview-tag-container"></div>
  )";
  dir_.WriteFile(FILE_PATH_LITERAL("page.html"), kPage);
  const Extension* app = LoadExtension(dir_.UnpackedPath());
  ASSERT_TRUE(app);

  // Launch the test app and grab its WebContents.
  content::WebContents* app_contents = nullptr;
  {
    content::WebContentsAddedObserver new_contents_observer;
    apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
        ->BrowserAppLauncher()
        .LaunchAppWithParams(apps::AppLaunchParams(
            app->id(), LaunchContainer::kLaunchContainerNone,
            WindowOpenDisposition::NEW_WINDOW,
            apps::mojom::AppLaunchSource::kSourceTest));
    app_contents = new_contents_observer.GetWebContents();
  }
  ASSERT_TRUE(content::WaitForLoadStop(app_contents));

  // Inject a <webview> script and declare desire to inject
  // cross-origin-fetching content scripts into the guest.
  const char kWebViewInjectionScriptTemplate[] = R"(
      document.querySelector('#webview-tag-container').innerHTML =
          '<webview style="width: 100px; height: 100px;"></webview>';
      var webview = document.querySelector('webview');
      webview.addContentScripts([{
          name: 'rule',
          matches: ['*://*/*'],
          js: { code: $1 },
          run_at: 'document_start'}]);
  )";
  GURL cross_site_resource(
      embedded_test_server()->GetURL("cross-site.com", "/nosniff.xml"));
  std::string web_view_injection_script = content::JsReplace(
      kWebViewInjectionScriptTemplate, CreateFetchScript(cross_site_resource));
  ASSERT_TRUE(ExecuteScript(app_contents, web_view_injection_script));

  // Navigate <webview>, which should trigger content script execution.
  GURL guest_url(
      embedded_test_server()->GetURL("fetch-initiator.com", "/title1.html"));
  const char kWebViewNavigationScriptTemplate[] = R"(
      var webview = document.querySelector('webview');
      webview.src = $1;
  )";
  std::string web_view_navigation_script =
      content::JsReplace(kWebViewNavigationScriptTemplate, guest_url);
  {
    content::DOMMessageQueue queue;
    base::HistogramTester histograms;
    content::ExecuteScriptAsync(app_contents, web_view_navigation_script);
    std::string fetch_result = PopString(&queue);

    // Verify that no CORB blocking occurred.
    EXPECT_EQ("nosniff.xml - body\n", fetch_result);
  }
}

using OriginHeaderExtensionBrowserTest = CorbAndCorsExtensionBrowserTest;

IN_PROC_BROWSER_TEST_P(OriginHeaderExtensionBrowserTest,
                       OriginHeaderInCrossOriginGetRequest) {
  const char kResourcePath[] = "/simulated-resource";
  net::test_server::ControllableHttpResponse http_request(
      embedded_test_server(), kResourcePath);
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a cross-origin GET fetch to
  // cross-site.com.
  GURL cross_site_resource(
      embedded_test_server()->GetURL("cross-site.com", kResourcePath));
  const char* kScriptTemplate = R"(
      fetch($1, {method: 'GET', mode:'cors'})
          .then(response => response.text())
          .then(text => domAutomationController.send(text))
          .catch(err => domAutomationController.send('ERROR: ' + err));
  )";
  ExecuteContentScript(
      active_web_contents(),
      content::JsReplace(kScriptTemplate, cross_site_resource));

  // Extract the Origin header.
  http_request.WaitForRequest();
  std::string actual_origin_header = "<none>";
  const auto& headers_map = http_request.http_request()->headers;
  auto it = headers_map.find("Origin");
  if (it != headers_map.end())
    actual_origin_header = it->second;

  if (AreContentScriptFetchesExpectedToBeBlocked() &&
      ShouldAllowlistAlsoApplyToOorCors()) {
    // Verify the Origin header uses the page's origin (not the extension
    // origin).
    EXPECT_EQ(url::Origin::Create(page_url).Serialize(), actual_origin_header);
  } else {
    // Verify the Origin header is missing.
    EXPECT_EQ("<none>", actual_origin_header);
  }

  // Regression test against https://crbug.com/944704.
  EXPECT_THAT(actual_origin_header,
              ::testing::Not(::testing::HasSubstr("chrome-extension")));
}

IN_PROC_BROWSER_TEST_P(OriginHeaderExtensionBrowserTest,
                       OriginHeaderInCrossOriginPostRequest) {
  const char kResourcePath[] = "/simulated-resource";
  net::test_server::ControllableHttpResponse http_request(
      embedded_test_server(), kResourcePath);
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a cross-origin POST fetch to
  // cross-site.com.
  GURL cross_site_resource(
      embedded_test_server()->GetURL("cross-site.com", kResourcePath));
  const char* kScriptTemplate = R"(
      fetch($1, {method: 'POST', mode:'cors'})
          .then(response => response.text())
          .then(text => domAutomationController.send(text))
          .catch(err => domAutomationController.send('ERROR: ' + err));
  )";
  ExecuteContentScript(
      active_web_contents(),
      content::JsReplace(kScriptTemplate, cross_site_resource));

  // Extract the Origin header.
  http_request.WaitForRequest();
  std::string actual_origin_header = "<none>";
  const auto& headers_map = http_request.http_request()->headers;
  auto it = headers_map.find("Origin");
  if (it != headers_map.end())
    actual_origin_header = it->second;

  // Verify the Origin header uses the page's origin (not the extension
  // origin).
  EXPECT_EQ(url::Origin::Create(page_url).Serialize(), actual_origin_header);

  // Regression test against https://crbug.com/944704.
  EXPECT_THAT(actual_origin_header,
              ::testing::Not(::testing::HasSubstr("chrome-extension")));
}

IN_PROC_BROWSER_TEST_P(OriginHeaderExtensionBrowserTest,
                       OriginHeaderInSameOriginPostRequest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to a fetch-initiator.com page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a same-origin POST fetch to
  // fetch-initiator.com.
  GURL same_origin_resource(
      embedded_test_server()->GetURL("fetch-initiator.com", "/echoall"));
  const char* kScriptTemplate = R"(
      fetch($1, {method: 'POST', mode:'cors'})
          .then(response => response.text())
          .then(text => domAutomationController.send(text))
          .catch(err => domAutomationController.send('ERROR: ' + err));
  )";
  content::DOMMessageQueue message_queue;
  ExecuteContentScript(
      active_web_contents(),
      content::JsReplace(kScriptTemplate, same_origin_resource));
  std::string fetch_result = PopString(&message_queue);

  // Verify the Origin header.
  //
  // According to the Fetch spec, POST should always set the Origin header (even
  // for same-origin requests).
  EXPECT_THAT(fetch_result,
              ::testing::HasSubstr("Origin: http://fetch-initiator.com"));

  // Regression test against https://crbug.com/944704.
  EXPECT_THAT(fetch_result,
              ::testing::Not(::testing::HasSubstr("Origin: chrome-extension")));
}

IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest,
                       RequestHeaders_InSameOriginFetch_FromContentScript) {
  // Sec-Fetch-Site only works on secure origins - setting up a https test
  // server to help with this.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  net::test_server::ControllableHttpResponse subresource_request(
      &https_server, "/subresource");
  ASSERT_TRUE(https_server.Start());

  // Load the test extension.
  ASSERT_TRUE(InstallExtension());

  // Navigate to https test page.
  GURL page_url = https_server.GetURL("/title1.html");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a same-origin GET fetch.
  GURL same_origin_resource(https_server.GetURL("/subresource"));
  EXPECT_EQ(url::Origin::Create(page_url),
            url::Origin::Create(same_origin_resource));
  const char* kScriptTemplate = R"(
      fetch($1, {method: 'GET', mode: 'no-cors'}) )";
  ExecuteContentScript(
      active_web_contents(),
      content::JsReplace(kScriptTemplate, same_origin_resource));

  // Verify the Referrer and Sec-Fetch-* header values.
  subresource_request.WaitForRequest();
  EXPECT_THAT(
      subresource_request.http_request()->headers,
      testing::IsSupersetOf({testing::Pair("Referer", page_url.spec().c_str()),
                             testing::Pair("Sec-Fetch-Mode", "no-cors"),
                             testing::Pair("Sec-Fetch-Site", "same-origin")}));
}

IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest,
                       RequestHeaders_InSameOriginXhr_FromContentScript) {
  // Sec-Fetch-Site only works on secure origins - setting up a https test
  // server to help with this.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  net::test_server::ControllableHttpResponse subresource_request(
      &https_server, "/subresource");
  ASSERT_TRUE(https_server.Start());

  // Load the test extension.
  ASSERT_TRUE(InstallExtension());

  // Navigate to https test page.
  GURL page_url = https_server.GetURL("/title1.html");
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(url::Origin::Create(page_url),
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a same-origin GET XHR.
  GURL same_origin_resource(https_server.GetURL("/subresource"));
  EXPECT_EQ(url::Origin::Create(page_url),
            url::Origin::Create(same_origin_resource));
  const char* kScriptTemplate = R"(
    var req = new XMLHttpRequest();
    req.open('GET', $1, true);
    req.send(null); )";
  ExecuteContentScript(
      active_web_contents(),
      content::JsReplace(kScriptTemplate, same_origin_resource));

  // Verify the Referrer and Sec-Fetch-* header values.
  subresource_request.WaitForRequest();
  EXPECT_THAT(
      subresource_request.http_request()->headers,
      testing::IsSupersetOf({testing::Pair("Referer", page_url.spec().c_str()),
                             testing::Pair("Sec-Fetch-Mode", "cors"),
                             testing::Pair("Sec-Fetch-Site", "same-origin")}));
}

IN_PROC_BROWSER_TEST_P(CorbAndCorsExtensionBrowserTest, CorsFromContentScript) {
  std::string cors_resource_path = "/cors-subresource-to-intercept";
  net::test_server::ControllableHttpResponse cors_request(
      embedded_test_server(), cors_resource_path);
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(InstallExtension());

  // Navigate to test page.
  GURL page_url = GetTestPageUrl("fetch-initiator.com");
  url::Origin page_origin = url::Origin::Create(page_url);
  std::string page_origin_string = page_origin.Serialize();
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_EQ(page_url,
            active_web_contents()->GetMainFrame()->GetLastCommittedURL());
  ASSERT_EQ(page_origin,
            active_web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a cross-origin GET fetch.
  content::DOMMessageQueue message_queue;
  GURL cors_resource_url(
      embedded_test_server()->GetURL("cross-site.com", cors_resource_path));
  EXPECT_TRUE(ExecuteContentScript(active_web_contents(),
                                   CreateFetchScript(cors_resource_url)));

  // Verify the request headers (e.g. Origin and Sec-Fetch-Site headers).
  cors_request.WaitForRequest();
  if (IsExtensionAllowlisted() || !ShouldAllowlistAlsoApplyToOorCors()) {
    // Content scripts of allowlisted extensions should be exempted from CORS,
    // based on the websites the extension has permission for, via extension
    // manifest.  Therefore, there should be no "Origin" header.
    EXPECT_THAT(
        cors_request.http_request()->headers,
        testing::Not(testing::Contains(testing::Pair("Origin", testing::_))));
  } else {
    // Content scripts of non-allowlisted extensions should participate in
    // regular CORS, just as if the request was issued from the webpage that the
    // content script got injected into.  Therefore we should expect the Origin
    // header to be present and have the right value.
    EXPECT_THAT(
        cors_request.http_request()->headers,
        testing::Contains(testing::Pair("Origin", page_origin_string.c_str())));
  }

  // Respond with Access-Control-Allow-Origin that matches the origin of the web
  // page.
  cors_request.Send("HTTP/1.1 200 OK\r\n");
  cors_request.Send("Content-Type: text/xml; charset=utf-8\r\n");
  cors_request.Send("X-Content-Type-Options: nosniff\r\n");
  cors_request.Send("Access-Control-Allow-Origin: " + page_origin_string +
                    "\r\n");
  cors_request.Send("\r\n");
  cors_request.Send("cors-allowed-body");
  cors_request.Done();

  // Verify that no CORB blocking occurred.
  //
  // CORB blocks responses based on Access-Control-Allow-Origin, oblivious to
  // whether the Origin request header was present (and/or if the extension is
  // exempted from CORS).  The Access-Control-Allow-Origin header is compared
  // with the request_initiator of the fetch (the origin of |page_url|) and the
  // test responds with "*" which matches all origins.
  std::string fetch_result = PopString(&message_queue);
  EXPECT_EQ("cors-allowed-body", fetch_result);
}

INSTANTIATE_TEST_SUITE_P(Allowlisted_AllowlistForCors,
                         CorbAndCorsExtensionBrowserTest,
                         ::testing::Values(TestParam::kAllowlisted |
                                           TestParam::kOutOfBlinkCors |
                                           TestParam::kAllowlistForCors));
INSTANTIATE_TEST_SUITE_P(NotAllowlisted_AllowlistForCors,
                         CorbAndCorsExtensionBrowserTest,
                         ::testing::Values(TestParam::kOutOfBlinkCors |
                                           TestParam::kAllowlistForCors));
INSTANTIATE_TEST_SUITE_P(Allowlisted_OorCors,
                         CorbAndCorsExtensionBrowserTest,
                         ::testing::Values(TestParam::kAllowlisted |
                                           TestParam::kOutOfBlinkCors));
INSTANTIATE_TEST_SUITE_P(NotAllowlisted_OorCors,
                         CorbAndCorsExtensionBrowserTest,
                         ::testing::Values(TestParam::kOutOfBlinkCors));
INSTANTIATE_TEST_SUITE_P(Allowlisted_InBlinkCors,
                         CorbAndCorsExtensionBrowserTest,
                         ::testing::Values(TestParam::kAllowlisted));
INSTANTIATE_TEST_SUITE_P(NotAllowlisted_InBlinkCors,
                         CorbAndCorsExtensionBrowserTest,
                         ::testing::Values(0));

INSTANTIATE_TEST_SUITE_P(
    Allowlisted_LegacyOriginHeaderBehavior_AllowlistForCors,
    OriginHeaderExtensionBrowserTest,
    ::testing::Values(TestParam::kAllowlisted | TestParam::kAllowlistForCors |
                      TestParam::kOutOfBlinkCors));
INSTANTIATE_TEST_SUITE_P(Allowlisted_NewOriginHeaderBehavior_AllowlistForCors,
                         OriginHeaderExtensionBrowserTest,
                         ::testing::Values(TestParam::kAllowlisted |
                                           TestParam::kAllowlistForCors |
                                           TestParam::kOutOfBlinkCors |
                                           TestParam::kDeriveOriginFromUrl));
INSTANTIATE_TEST_SUITE_P(
    NotAllowlisted_LegacyOriginHeaderBehavior_AllowlistForCors,
    OriginHeaderExtensionBrowserTest,
    ::testing::Values(TestParam::kOutOfBlinkCors |
                      TestParam::kAllowlistForCors));
INSTANTIATE_TEST_SUITE_P(
    NotAllowlisted_NewOriginHeaderBehavior_AllowlistForCors,
    OriginHeaderExtensionBrowserTest,
    ::testing::Values(TestParam::kOutOfBlinkCors |
                      TestParam::kAllowlistForCors |
                      TestParam::kDeriveOriginFromUrl));
INSTANTIATE_TEST_SUITE_P(Allowlisted_LegacyOriginHeaderBehavior,
                         OriginHeaderExtensionBrowserTest,
                         ::testing::Values(TestParam::kAllowlisted |
                                           TestParam::kOutOfBlinkCors));
INSTANTIATE_TEST_SUITE_P(Allowlisted_NewOriginHeaderBehavior,
                         OriginHeaderExtensionBrowserTest,
                         ::testing::Values(TestParam::kAllowlisted |
                                           TestParam::kOutOfBlinkCors |
                                           TestParam::kDeriveOriginFromUrl));
INSTANTIATE_TEST_SUITE_P(NotAllowlisted_LegacyOriginHeaderBehavior,
                         OriginHeaderExtensionBrowserTest,
                         ::testing::Values(TestParam::kOutOfBlinkCors));
INSTANTIATE_TEST_SUITE_P(NotAllowlisted_NewOriginHeaderBehavior,
                         OriginHeaderExtensionBrowserTest,
                         ::testing::Values(TestParam::kOutOfBlinkCors |
                                           TestParam::kDeriveOriginFromUrl));

}  // namespace extensions
