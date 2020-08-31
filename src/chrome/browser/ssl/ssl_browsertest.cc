// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/interstitials/security_interstitial_idn_test.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ssl/certificate_reporting_test_utils.h"
#include "chrome/browser/ssl/chrome_security_blocking_page_factory.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ssl/ssl_browsertest_util.h"
#include "chrome/browser/ssl/ssl_error_controller_client.h"
#include "chrome/browser/ssl/tls_deprecation_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/certificate_transparency/pref_names.h"
#include "components/content_settings/browser/tab_specific_content_settings.h"
#include "components/content_settings/common/content_settings_agent.mojom.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/network_time/network_time_test_utils.h"
#include "components/network_time/network_time_tracker.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/bad_clock_blocking_page.h"
#include "components/security_interstitials/content/captive_portal_blocking_page.h"
#include "components/security_interstitials/content/cert_report_helper.h"
#include "components/security_interstitials/content/common_name_mismatch_handler.h"
#include "components/security_interstitials/content/mitm_software_blocking_page.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "components/security_interstitials/content/ssl_error_assistant.h"
#include "components/security_interstitials/content/ssl_error_assistant.pb.h"
#include "components/security_interstitials/content/ssl_error_handler.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_state/core/features.h"
#include "components/security_state/core/security_state.h"
#include "components/ssl_errors/error_classification.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_params_manager.h"
#include "components/variations/variations_switches.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/page_state.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "crypto/sha2.h"
#include "extensions/browser/event_router.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/base/escape.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_database.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/http/transport_security_state_test_util.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_info.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(USE_NSS_CERTS)
#include "chrome/browser/certificate_manager_model.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/net/nss_context.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_util_nss.h"
#endif  // defined(USE_NSS_CERTS)

#if defined(OS_CHROMEOS)
#include "base/path_service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/session_manager/core/session_manager.h"
#endif  // defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

using content::WebContents;
namespace AuthState = ssl_test_util::AuthState;
namespace CertError = ssl_test_util::CertError;

namespace {

const int kLargeVersionId = 0xFFFFFF;

const char kHstsTestHostName[] = "hsts-example.test";

constexpr char kPreloadedPKPHost[] = "with-report-uri-pkp.preloaded.test";
constexpr char kPreloadedReportHost[] = "report-uri.preloaded.test";

enum ProceedDecision {
  SSL_INTERSTITIAL_PROCEED,
  SSL_INTERSTITIAL_DO_NOT_PROCEED
};

security_state::SecurityLevel GetPassiveMixedContentSecurityLevel() {
  if (base::FeatureList::IsEnabled(
          security_state::features::kPassiveMixedContentWarning)) {
    return security_state::WARNING;
  }
  return security_state::NONE;
}

// This observer waits for the SSLErrorHandler to start an interstitial timer
// for the given web contents.
class SSLInterstitialTimerObserver {
 public:
  explicit SSLInterstitialTimerObserver(WebContents* web_contents)
      : web_contents_(web_contents), message_loop_runner_(new base::RunLoop) {
    callback_ = base::Bind(&SSLInterstitialTimerObserver::OnTimerStarted,
                           base::Unretained(this));
    SSLErrorHandler::SetInterstitialTimerStartedCallbackForTesting(&callback_);
  }

  ~SSLInterstitialTimerObserver() {
    SSLErrorHandler::SetInterstitialTimerStartedCallbackForTesting(nullptr);
  }

  // Waits until the interstitial delay timer in SSLErrorHandler is started.
  void WaitForTimerStarted() { message_loop_runner_->Run(); }

  // Returns true if the interstitial delay timer has been started.
  bool timer_started() const { return timer_started_; }

 private:
  void OnTimerStarted(WebContents* web_contents) {
    timer_started_ = true;
    if (web_contents_ == web_contents)
      message_loop_runner_->Quit();
  }

  bool timer_started_ = false;
  const WebContents* web_contents_;
  SSLErrorHandler::TimerStartedCallback callback_;

  std::unique_ptr<base::RunLoop> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(SSLInterstitialTimerObserver);
};

class ChromeContentBrowserClientForMixedContentTest
    : public ChromeContentBrowserClient {
 public:
  ChromeContentBrowserClientForMixedContentTest() {}
  void OverrideWebkitPrefs(content::RenderViewHost* rvh,
                           content::WebPreferences* web_prefs) override {
    web_prefs->allow_running_insecure_content = allow_running_insecure_content_;
    web_prefs->strict_mixed_content_checking = strict_mixed_content_checking_;
    web_prefs->strictly_block_blockable_mixed_content =
        strictly_block_blockable_mixed_content_;
  }
  void SetMixedContentSettings(bool allow_running_insecure_content,
                               bool strict_mixed_content_checking,
                               bool strictly_block_blockable_mixed_content) {
    allow_running_insecure_content_ = allow_running_insecure_content;
    strict_mixed_content_checking_ = strict_mixed_content_checking;
    strictly_block_blockable_mixed_content_ =
        strictly_block_blockable_mixed_content;
  }

 private:
  bool allow_running_insecure_content_ = false;
  bool strict_mixed_content_checking_ = false;
  bool strictly_block_blockable_mixed_content_ = false;

  DISALLOW_COPY_AND_ASSIGN(ChromeContentBrowserClientForMixedContentTest);
};

std::string EncodeQuery(const std::string& query) {
  url::RawCanonOutputT<char> buffer;
  url::EncodeURIComponent(query.data(), query.size(), &buffer);
  return std::string(buffer.data(), buffer.length());
}

// Returns the Sha256 hash of the SPKI of |cert|.
net::HashValue GetSPKIHash(const CRYPTO_BUFFER* cert) {
  base::StringPiece spki_bytes;
  EXPECT_TRUE(net::asn1::ExtractSPKIFromDERCert(
      net::x509_util::CryptoBufferAsStringPiece(cert), &spki_bytes));
  net::HashValue sha256(net::HASH_VALUE_SHA256);
  crypto::SHA256HashString(spki_bytes, sha256.data(), crypto::kSHA256Length);
  return sha256;
}

// Compares two SSLStatuses to check if they match up before and after an
// interstitial. To match up, they should have the same connection information
// properties, such as certificate, connection status, connection security,
// etc. Content status and user data are not compared. Returns true if the
// statuses match and false otherwise.
bool ComparePreAndPostInterstitialSSLStatuses(const content::SSLStatus& one,
                                              const content::SSLStatus& two) {
  // TODO(mattm): It feels like this should use
  // certificate->EqualsIncludingChain, but that fails on some platforms. Find
  // out why and document or fix.
  return one.initialized == two.initialized &&
         !!one.certificate == !!two.certificate &&
         (one.certificate
              ? one.certificate->EqualsExcludingChain(two.certificate.get())
              : true) &&
         one.cert_status == two.cert_status &&
         one.key_exchange_group == two.key_exchange_group &&
         // Skip comparing the peer_signature_algorithm, because it is not
         // filled in by the time of an interstitial.
         one.connection_status == two.connection_status &&
         one.pkp_bypassed == two.pkp_bypassed;
}

// Waits until an interstitial is showing.
//
// TODO(crbug.com/752372): This should not be needed for committed
// interstitials. Replace all call sites directly with the assert.
void WaitForInterstitial(WebContents* tab) {
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetMainFrame()));
}

void ExpectInterstitialElementHidden(WebContents* tab,
                                     const std::string& element_id,
                                     bool expect_hidden) {
  content::RenderFrameHost* frame = tab->GetMainFrame();
  // Send CMD_TEXT_FOUND to indicate that the 'hidden' class is found, and
  // CMD_TEXT_NOT_FOUND if not.
  std::string command = base::StringPrintf(
      "window.domAutomationController.send($('%s').classList.contains('hidden')"
      " ? %d : %d);",
      element_id.c_str(), security_interstitials::CMD_TEXT_FOUND,
      security_interstitials::CMD_TEXT_NOT_FOUND);
  int result = 0;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(frame, command, &result));
  EXPECT_EQ(expect_hidden ? security_interstitials::CMD_TEXT_FOUND
                          : security_interstitials::CMD_TEXT_NOT_FOUND,
            result);
}

// Runs |quit_callback| on the UI thread once a URL request has been seen.
// If |hung_response| is true, returns a request that hangs.
std::unique_ptr<net::test_server::HttpResponse> WaitForJsonRequest(
    const base::RepeatingClosure& quit_closure,
    bool hung_response,
    const net::test_server::HttpRequest& request) {
  // Basic sanity checks on the request.
  EXPECT_EQ("/pkp", request.relative_url);
  EXPECT_EQ("POST", request.method_string);
  base::Optional<base::Value> value = base::JSONReader::Read(request.content);
  EXPECT_TRUE(value);

  base::PostTask(FROM_HERE, {content::BrowserThread::UI}, quit_closure);

  if (hung_response)
    return std::make_unique<net::test_server::HungResponse>();
  return nullptr;
}

}  // namespace

class SSLUITestBase : public InProcessBrowserTest,
                      public network::mojom::SSLConfigClient {
 public:
  SSLUITestBase()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        https_server_expired_(net::EmbeddedTestServer::TYPE_HTTPS),
        https_server_mismatched_(net::EmbeddedTestServer::TYPE_HTTPS),
        https_server_sha1_(net::EmbeddedTestServer::TYPE_HTTPS),
        https_server_common_name_only_(net::EmbeddedTestServer::TYPE_HTTPS),
        https_server_ocsp_ok_(net::EmbeddedTestServer::TYPE_HTTPS),
        https_server_ocsp_revoked_(net::EmbeddedTestServer::TYPE_HTTPS),
        wss_server_expired_(net::SpawnedTestServer::TYPE_WSS,
                            SSLOptions(SSLOptions::CERT_EXPIRED),
                            net::GetWebSocketTestDataDirectory()),
        wss_server_mismatched_(net::SpawnedTestServer::TYPE_WSS,
                               SSLOptions(SSLOptions::CERT_MISMATCHED_NAME),
                               net::GetWebSocketTestDataDirectory()) {
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    https_server_expired_.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
    https_server_expired_.AddDefaultHandlers(GetChromeTestDataDir());

    https_server_mismatched_.SetSSLConfig(
        net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
    https_server_mismatched_.AddDefaultHandlers(GetChromeTestDataDir());

    https_server_sha1_.SetSSLConfig(net::EmbeddedTestServer::CERT_SHA1_LEAF);
    https_server_sha1_.AddDefaultHandlers(GetChromeTestDataDir());

    https_server_common_name_only_.SetSSLConfig(
        net::EmbeddedTestServer::CERT_COMMON_NAME_ONLY);
    https_server_common_name_only_.AddDefaultHandlers(GetChromeTestDataDir());

    net::EmbeddedTestServer::ServerCertificateConfig ok_cert_config;
    ok_cert_config.ocsp_config = net::EmbeddedTestServer::OCSPConfig(
        {{net::OCSPRevocationStatus::GOOD,
          net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});
    https_server_ocsp_ok_.SetSSLConfig(ok_cert_config);
    https_server_ocsp_ok_.AddDefaultHandlers(GetChromeTestDataDir());

    net::EmbeddedTestServer::ServerCertificateConfig revoked_cert_config;
    revoked_cert_config.ocsp_config = net::EmbeddedTestServer::OCSPConfig(
        {{net::OCSPRevocationStatus::REVOKED,
          net::EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});
    https_server_ocsp_revoked_.SetSSLConfig(revoked_cert_config);
    https_server_ocsp_revoked_.AddDefaultHandlers(GetChromeTestDataDir());
  }

  void SetUp() override {
    EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    InProcessBrowserTest::SetUp();
    SSLErrorHandler::ResetConfigForTesting();
  }

  void TearDown() override {
    SSLErrorHandler::ResetConfigForTesting();
    InProcessBrowserTest::TearDown();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Browser will both run and display insecure content.
    command_line->AppendSwitch(switches::kAllowRunningInsecureContent);
    // Use process-per-site so that navigating to a same-site page in a
    // new tab will use the same process.
    command_line->AppendSwitch(switches::kProcessPerSite);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    network::mojom::NetworkContextParamsPtr context_params =
        CreateDefaultNetworkContextParams();
    last_ssl_config_ = *context_params->initial_ssl_config;
    receiver_.Bind(std::move(context_params->ssl_config_client_receiver));
  }

  void TearDownOnMainThread() override { receiver_.reset(); }

  void ProceedThroughInterstitial(WebContents* tab) {
    content::TestNavigationObserver nav_observer(tab, 1);
    SendInterstitialCommand(tab, security_interstitials::CMD_PROCEED);
    nav_observer.Wait();
  }

  virtual void DontProceedThroughInterstitial(WebContents* tab) {
    SendInterstitialCommand(tab, security_interstitials::CMD_DONT_PROCEED);
    WaitForInterstitialDetach(tab);
  }

  void SendInterstitialCommand(
      WebContents* tab,
      security_interstitials::SecurityInterstitialCommand command) {
    std::string javascript;
    switch (command) {
      case security_interstitials::CMD_DONT_PROCEED: {
        javascript = "window.certificateErrorPageController.dontProceed();";
        break;
      }
      case security_interstitials::CMD_PROCEED: {
        javascript = "window.certificateErrorPageController.proceed();";
        break;
      }
      case security_interstitials::CMD_SHOW_MORE_SECTION: {
        javascript = "window.certificateErrorPageController.showMoreSection();";
        break;
      }
      case security_interstitials::CMD_OPEN_HELP_CENTER: {
        javascript = "window.certificateErrorPageController.openHelpCenter();";
        break;
      }
      case security_interstitials::CMD_OPEN_DIAGNOSTIC: {
        javascript = "window.certificateErrorPageController.openDiagnostic();";
        break;
      }
      case security_interstitials::CMD_RELOAD: {
        javascript = "window.certificateErrorPageController.reload();";
        break;
      }
      case security_interstitials::CMD_OPEN_DATE_SETTINGS: {
        javascript =
            "window.certificateErrorPageController.openDateSettings();";
        break;
      }
      case security_interstitials::CMD_OPEN_LOGIN: {
        javascript = "window.certificateErrorPageController.openLogin();";
        break;
      }
      case security_interstitials::CMD_DO_REPORT: {
        javascript = "window.certificateErrorPageController.doReport();";
        break;
      }
      case security_interstitials::CMD_DONT_REPORT: {
        javascript = "window.certificateErrorPageController.dontReport();";
        break;
      }
      case security_interstitials::CMD_OPEN_REPORTING_PRIVACY: {
        javascript =
            "window.certificateErrorPageController.openReportingPrivacy();";
        break;
      }
      case security_interstitials::CMD_OPEN_WHITEPAPER: {
        javascript = "window.certificateErrorPageController.openWhitepaper();";
        break;
      }
      case security_interstitials::CMD_REPORT_PHISHING_ERROR: {
        javascript =
            "window.certificateErrorPageController.reportPhishingError();";
        break;
      }
      default: {
        // Other values in the enum are not used by these tests, and don't
        // have a Javascript equivalent that can be called here.
        NOTREACHED();
      }
    }
    ASSERT_TRUE(content::ExecuteScript(tab, javascript));
    return;
  }

  network::mojom::NetworkContextParamsPtr CreateDefaultNetworkContextParams() {
    return g_browser_process->system_network_context_manager()
        ->CreateDefaultNetworkContextParams();
  }

  static std::string GetFilePathWithHostAndPortReplacement(
      const std::string& original_file_path,
      const net::HostPortPair& host_port_pair) {
    base::StringPairs replacement_text;
    replacement_text.push_back(
        make_pair("REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()));
    return net::test_server::GetFilePathWithReplacements(original_file_path,
                                                         replacement_text);
  }

  static std::string GetTopFramePath(
      const net::EmbeddedTestServer& http_server,
      const net::EmbeddedTestServer& good_https_server,
      const net::EmbeddedTestServer& bad_https_server) {
    // The "frame_left.html" page contained in the top_frame.html page contains
    // <a href>'s to three different servers. This sets up all of the
    // replacement text to work with test servers which listen on ephemeral
    // ports.
    GURL http_url = http_server.GetURL("/ssl/google.html");
    GURL good_https_url = good_https_server.GetURL("/ssl/google.html");
    GURL bad_https_url = bad_https_server.GetURL("/ssl/bad_iframe.html");

    base::StringPairs replacement_text_frame_left;
    replacement_text_frame_left.push_back(
        make_pair("REPLACE_WITH_HTTP_PORT", http_url.port()));
    replacement_text_frame_left.push_back(
        make_pair("REPLACE_WITH_GOOD_HTTPS_PAGE", good_https_url.spec()));
    replacement_text_frame_left.push_back(
        make_pair("REPLACE_WITH_BAD_HTTPS_PAGE", bad_https_url.spec()));
    std::string frame_left_path = net::test_server::GetFilePathWithReplacements(
        "frame_left.html", replacement_text_frame_left);

    // Substitute the generated frame_left URL into the top_frame page.
    base::StringPairs replacement_text_top_frame;
    replacement_text_top_frame.push_back(
        make_pair("REPLACE_WITH_FRAME_LEFT_PATH", frame_left_path));
    return net::test_server::GetFilePathWithReplacements(
        "/ssl/top_frame.html", replacement_text_top_frame);
  }

  security_interstitials::SecurityInterstitialPage* GetInterstitialPage(
      WebContents* tab) {
    security_interstitials::SecurityInterstitialTabHelper* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            tab);
    if (!helper)
      return nullptr;
    return helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting();
  }

  // Helper function for testing invalid certificate chain reporting.
  void TestBrokenHTTPSReporting(
      certificate_reporting_test_utils::OptIn opt_in,
      ProceedDecision proceed,
      certificate_reporting_test_utils::ExpectReport expect_report,
      Browser* browser) {
    ASSERT_TRUE(https_server_expired_.Start());

    base::RunLoop run_loop;
    certificate_reporting_test_utils::SSLCertReporterCallback reporter_callback(
        &run_loop);

    // Opt in to sending reports for invalid certificate chains.
    certificate_reporting_test_utils::SetCertReportingOptIn(browser, opt_in);

    ui_test_utils::NavigateToURL(browser,
                                 https_server_expired_.GetURL("/title1.html"));

    WebContents* tab = browser->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(tab != nullptr);
    ssl_test_util::CheckAuthenticationBrokenState(
        tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

    std::unique_ptr<SSLCertReporter> ssl_cert_reporter =
        certificate_reporting_test_utils::CreateMockSSLCertReporter(
            base::Bind(&certificate_reporting_test_utils::
                           SSLCertReporterCallback::ReportSent,
                       base::Unretained(&reporter_callback)),
            expect_report);

    SSLBlockingPage* interstitial_page =
        static_cast<SSLBlockingPage*>(GetInterstitialPage(tab));
    ASSERT_TRUE(interstitial_page);
    interstitial_page->SetSSLCertReporterForTesting(
        std::move(ssl_cert_reporter));

    EXPECT_EQ(std::string(), reporter_callback.GetLatestHostnameReported());
    EXPECT_EQ(chrome_browser_ssl::CertLoggerRequest::CHROME_CHANNEL_NONE,
              reporter_callback.GetLatestChromeChannelReported());

    // Leave the interstitial (either by proceeding or going back)
    if (proceed == SSL_INTERSTITIAL_PROCEED) {
      ProceedThroughInterstitial(tab);
    } else {
      DontProceedThroughInterstitial(tab);
    }

    if (expect_report ==
        certificate_reporting_test_utils::CERT_REPORT_EXPECTED) {
      // Check that the mock reporter received a request to send a report.
      run_loop.Run();
      EXPECT_EQ(https_server_expired_.GetURL("/title1.html").host(),
                reporter_callback.GetLatestHostnameReported());
      EXPECT_NE(chrome_browser_ssl::CertLoggerRequest::CHROME_CHANNEL_NONE,
                reporter_callback.GetLatestChromeChannelReported());
    } else {
      base::RunLoop().RunUntilIdle();
      EXPECT_EQ(std::string(), reporter_callback.GetLatestHostnameReported());
      EXPECT_EQ(chrome_browser_ssl::CertLoggerRequest::CHROME_CHANNEL_NONE,
                reporter_callback.GetLatestChromeChannelReported());
    }
  }

  // Helper function for testing invalid certificate chain reporting with the
  // bad clock interstitial.
  void TestBadClockReporting(
      certificate_reporting_test_utils::OptIn opt_in,
      certificate_reporting_test_utils::ExpectReport expect_report,
      Browser* browser) {
    ASSERT_TRUE(https_server_expired_.Start());

    base::RunLoop run_loop;
    certificate_reporting_test_utils::SSLCertReporterCallback reporter_callback(
        &run_loop);

    // Set network time back ten minutes, which is sufficient to
    // trigger the reporting.
    g_browser_process->network_time_tracker()->UpdateNetworkTime(
        base::Time::Now() - base::TimeDelta::FromMinutes(10),
        base::TimeDelta::FromMilliseconds(1),   /* resolution */
        base::TimeDelta::FromMilliseconds(500), /* latency */
        base::TimeTicks::Now() /* posting time of this update */);

    // Opt in to sending reports for invalid certificate chains.
    certificate_reporting_test_utils::SetCertReportingOptIn(browser, opt_in);

    ui_test_utils::NavigateToURL(browser,
                                 https_server_expired_.GetURL("/title1.html"));
    WebContents* tab = browser->tab_strip_model()->GetActiveWebContents();
    WaitForInterstitial(tab);

    ssl_test_util::CheckAuthenticationBrokenState(
        tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

    std::unique_ptr<SSLCertReporter> ssl_cert_reporter =
        certificate_reporting_test_utils::CreateMockSSLCertReporter(
            base::Bind(&certificate_reporting_test_utils::
                           SSLCertReporterCallback::ReportSent,
                       base::Unretained(&reporter_callback)),
            expect_report);

    ASSERT_TRUE(
        chrome_browser_interstitials::IsShowingBadClockInterstitial(tab));
    BadClockBlockingPage* clock_page =
        static_cast<BadClockBlockingPage*>(GetInterstitialPage(tab));
    clock_page->SetSSLCertReporterForTesting(std::move(ssl_cert_reporter));

    EXPECT_EQ(std::string(), reporter_callback.GetLatestHostnameReported());
    DontProceedThroughInterstitial(tab);

    if (expect_report ==
        certificate_reporting_test_utils::CERT_REPORT_EXPECTED) {
      // Check that the mock reporter received a request to send a report.
      run_loop.Run();
      EXPECT_EQ(https_server_expired_.GetURL("/title1.html").host(),
                reporter_callback.GetLatestHostnameReported());
    } else {
      base::RunLoop().RunUntilIdle();
      EXPECT_EQ(std::string(), reporter_callback.GetLatestHostnameReported());
    }
  }

  // Sets the policy identified by |policy_name| to be true, ensuring
  // that the corresponding boolean pref |pref_name| is updated to match.
  void EnablePolicy(PrefService* pref_service,
                    const char* policy_name,
                    const char* pref_name) {
    policy::PolicyMap policy_map;
    policy_map.Set(policy_name, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                   std::make_unique<base::Value>(true), nullptr);

    EXPECT_NO_FATAL_FAILURE(UpdateChromePolicy(policy_map));

    EXPECT_TRUE(pref_service->GetBoolean(pref_name));
    EXPECT_TRUE(pref_service->IsManagedPreference(pref_name));

    // Wait for the updated SSL configuration to be sent to the network service,
    // to avoid a race.
    g_browser_process->system_network_context_manager()
        ->FlushSSLConfigManagerForTesting();
  }

  // Sets the policy identified by |policy_name| to |policy_value|.
  void SetPolicy(const char* policy_name,
                 std::unique_ptr<base::Value> policy_value) {
    policy::PolicyMap policy_map;
    policy_map.Set(policy_name, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                   std::move(policy_value), nullptr);

    EXPECT_NO_FATAL_FAILURE(UpdateChromePolicy(policy_map));

    // Wait for the updated SSL configuration to be sent to the network service,
    // to avoid a race.
    g_browser_process->system_network_context_manager()
        ->FlushSSLConfigManagerForTesting();
  }

  // Helper function for TestInterstitialLinksOpenInNewTab. Implemented as a
  // test fixture method because the whole test fixture class is friended by
  // SSLBlockingPage.
  security_interstitials::SecurityInterstitialControllerClient*
  GetControllerClientFromSSLBlockingPage(SSLBlockingPage* ssl_interstitial) {
    return ssl_interstitial->controller();
  }

  // Helper function that checks that after proceeding through an interstitial,
  // the app window is closed, a new tab with the app URL is opened, and there
  // is no interstitial.
  void ProceedThroughInterstitialInAppAndCheckNewTabOpened(
      Browser* app_browser,
      const GURL& app_url) {
    Profile* profile = browser()->profile();

    size_t num_browsers = chrome::GetBrowserCount(profile);
    EXPECT_EQ(app_browser, chrome::FindLastActive());
    int num_tabs = browser()->tab_strip_model()->count();

    ProceedThroughInterstitial(
        app_browser->tab_strip_model()->GetActiveWebContents());

    EXPECT_EQ(--num_browsers, chrome::GetBrowserCount(profile));
    EXPECT_EQ(browser(), chrome::FindLastActive());
    EXPECT_EQ(++num_tabs, browser()->tab_strip_model()->count());

    WebContents* new_tab = browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(new_tab));

    ssl_test_util::CheckAuthenticationBrokenState(
        new_tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
    EXPECT_EQ(app_url, new_tab->GetVisibleURL());
  }

  void set_ssl_config_updated_callback(
      const base::RepeatingClosure& ssl_config_updated_callback) {
    ssl_config_updated_callback_ = std::move(ssl_config_updated_callback);
  }

  // network::mojom::SSLConfigClient implementation.
  void OnSSLConfigUpdated(network::mojom::SSLConfigPtr ssl_config) override {
    last_ssl_config_ = *ssl_config;
    if (ssl_config_updated_callback_)
      ssl_config_updated_callback_.Run();
  }

 protected:
  typedef net::SpawnedTestServer::SSLOptions SSLOptions;

  // Navigates to an interstitial and clicks through the certificate
  // error; then navigates to a page at |path| that loads unsafe content.
  void SetUpUnsafeContentsWithUserException(const std::string& path) {
    ASSERT_TRUE(https_server_.Start());
    // Note that it is necessary to user https_server_mismatched_ here over the
    // other invalid cert servers. This is because the test relies on the two
    // servers having different hosts since SSL exceptions are per-host, not per
    // origin, and https_server_mismatched_ uses 'localhost' rather than
    // '127.0.0.1'.
    ASSERT_TRUE(https_server_mismatched_.Start());

    // Navigate to an unsafe site. Proceed with interstitial page to indicate
    // the user approves the bad certificate.
    ui_test_utils::NavigateToURL(
        browser(), https_server_mismatched_.GetURL("/ssl/blank_page.html"));
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    ssl_test_util::CheckAuthenticationBrokenState(
        tab, net::CERT_STATUS_COMMON_NAME_INVALID,
        AuthState::SHOWING_INTERSTITIAL);
    ProceedThroughInterstitial(tab);
    ssl_test_util::CheckAuthenticationBrokenState(
        tab, net::CERT_STATUS_COMMON_NAME_INVALID, AuthState::NONE);

    std::string replacement_path = GetFilePathWithHostAndPortReplacement(
        path, https_server_mismatched_.host_port_pair());
    ui_test_utils::NavigateToURL(browser(),
                                 https_server_.GetURL(replacement_path));
  }

  Browser* InstallAndOpenTestWebApp(const GURL& app_url) {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = app_url;
    web_app_info->scope = app_url.GetWithoutFilename();
    web_app_info->title = base::UTF8ToUTF16("Test app");
    web_app_info->description = base::UTF8ToUTF16("Test description");

    Profile* profile = browser()->profile();

    web_app::AppId app_id =
        web_app::InstallWebApp(profile, std::move(web_app_info));

    Browser* app_browser = web_app::LaunchWebAppBrowserAndWait(profile, app_id);
    return app_browser;
  }

  void UpdateChromePolicy(const policy::PolicyMap& policies) {
    policy_provider_.UpdateChromePolicy(policies);
    ASSERT_TRUE(base::MessageLoopCurrent::Get());

    base::RunLoop().RunUntilIdle();

    content::FlushNetworkServiceInstanceForTesting();
  }

  void RunOnIOThreadBlocking(base::OnceClosure task) {
    base::RunLoop run_loop;
    base::PostTaskAndReply(FROM_HERE, {content::BrowserThread::IO},
                           std::move(task), run_loop.QuitClosure());
    run_loop.Run();
  }

  net::EmbeddedTestServer https_server_;
  net::EmbeddedTestServer https_server_expired_;
  net::EmbeddedTestServer https_server_mismatched_;
  net::EmbeddedTestServer https_server_sha1_;
  net::EmbeddedTestServer https_server_common_name_only_;
  net::EmbeddedTestServer https_server_ocsp_ok_;
  net::EmbeddedTestServer https_server_ocsp_revoked_;

  net::SpawnedTestServer wss_server_expired_;
  net::SpawnedTestServer wss_server_mismatched_;

  policy::MockConfigurationPolicyProvider policy_provider_;

  base::RepeatingClosure ssl_config_updated_callback_;
  network::mojom::SSLConfig last_ssl_config_;
  mojo::Receiver<network::mojom::SSLConfigClient> receiver_{this};

 private:
  DISALLOW_COPY_AND_ASSIGN(SSLUITestBase);
};

class SSLUITest : public SSLUITestBase {
 public:
  SSLUITest() : SSLUITestBase() {
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kMixedContentAutoupgrade);
  }

 protected:
  void DontProceedThroughInterstitial(WebContents* tab) override {
    content::TestNavigationObserver nav_observer(tab, 1);
    SendInterstitialCommand(tab, security_interstitials::CMD_DONT_PROCEED);
    nav_observer.Wait();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(SSLUITest);
};

class SSLUITestBlock : public SSLUITest {
 public:
  SSLUITestBlock() : SSLUITest() {}

  // Browser will not run insecure content.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // By overriding SSLUITest, we won't apply the flag that allows running
    // insecure content.
  }
};

class SSLUITestIgnoreCertErrors : public SSLUITest {
 public:
  SSLUITestIgnoreCertErrors() : SSLUITest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SSLUITest::SetUpCommandLine(command_line);
    // Browser will ignore certificate errors.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }
};

static std::string MakeCertSPKIFingerprint(net::X509Certificate* cert) {
  net::HashValue hash = GetSPKIHash(cert->cert_buffer());
  std::string hash_base64;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(hash.data()),
                        hash.size()),
      &hash_base64);
  return hash_base64;
}

class SSLUITestIgnoreCertErrorsBySPKIHTTPS : public SSLUITest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SSLUITest::SetUpCommandLine(command_line);
    std::string whitelist_flag = MakeCertSPKIFingerprint(
        https_server_mismatched_.GetCertificate().get());
    // Browser will ignore certificate errors for chains matching one of the
    // public keys from the list.
    command_line->AppendSwitchASCII(
        network::switches::kIgnoreCertificateErrorsSPKIList, whitelist_flag);
  }
};

class SSLUITestIgnoreCertErrorsBySPKIWSS : public SSLUITest {
 public:
  SSLUITestIgnoreCertErrorsBySPKIWSS() : SSLUITest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SSLUITest::SetUpCommandLine(command_line);
    std::string whitelist_flag =
        MakeCertSPKIFingerprint(wss_server_expired_.GetCertificate().get());
    // Browser will ignore certificate errors for chains matching one of the
    // public keys from the list.
    command_line->AppendSwitchASCII(
        network::switches::kIgnoreCertificateErrorsSPKIList, whitelist_flag);
  }
};

class SSLUITestIgnoreLocalhostCertErrors : public SSLUITest {
 public:
  SSLUITestIgnoreLocalhostCertErrors() : SSLUITest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SSLUITest::SetUpCommandLine(command_line);
    // Browser will ignore certificate errors on localhost.
    command_line->AppendSwitch(switches::kAllowInsecureLocalhost);
  }
};

class SSLUITestWithExtendedReporting : public SSLUITest {
 public:
  SSLUITestWithExtendedReporting() : SSLUITest() {
    // Certificate reports are only sent from official builds, unless this has
    // been called.
    CertReportHelper::SetFakeOfficialBuildForTesting();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SSLUITest::SetUpCommandLine(command_line);

    // CertReportHelper::ShouldReportCertificateError checks the value of this
    // variation. Ensure reporting is enabled.
    variations::testing::VariationParamsManager::AppendVariationParams(
        "ReportCertificateErrors", "ShowAndPossiblySend",
        {{"sendingThreshold", "1.0"}}, command_line);
  }
};

class SSLUITestHSTS : public SSLUITest {
 public:
  void SetUpOnMainThread() override {
    SSLUITest::SetUpOnMainThread();
    ssl_test_util::SetHSTSForHostName(browser()->profile(), kHstsTestHostName);
  }
};

// Visits a regular page over http.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTP) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html"));

  ssl_test_util::CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);
}

// Visits a page over http which includes broken https resources (status should
// be OK).
// TODO(jcampan): test that bad HTTPS content is blocked (otherwise we'll give
//                the secure cookies away!).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPWithBrokenHTTPSResource) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_unsafe_contents.html",
      https_server_expired_.host_port_pair());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(replacement_path));

  ssl_test_util::CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);
}

// Tests that after loading mixed content and then making a same-document
// navigation, the mixed content security indicator remains. See
// https://crbug.com/959571.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestMixedContentWithSamePageNavigation) {
  ASSERT_TRUE(https_server_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to a secure page (no mixed content).
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  // Add a mixed form after a same-document navigation.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html#foo"));
  ssl_test_util::SecurityStateWebContentsObserver observer(tab);
  ASSERT_NE(false, content::EvalJs(tab,
                                   "var f = document.createElement('form');"
                                   "f.action = 'http://foo.test';"
                                   "document.body.appendChild(f)"));
  observer.WaitForDidChangeVisibleSecurityState();
  ssl_test_util::CheckSecurityState(
      tab, CertError::NONE, security_state::NONE,
      AuthState::DISPLAYED_FORM_WITH_INSECURE_ACTION);

  // Go back (which should also be a same-document navigation) and test that the
  // security indicator is still downgraded because of the mixed form.
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(tab);
  ssl_test_util::CheckSecurityState(
      tab, CertError::NONE, security_state::NONE,
      AuthState::DISPLAYED_FORM_WITH_INSECURE_ACTION);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestBrokenHTTPSWithInsecureContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_expired_.GetURL(replacement_path));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID,
      AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Tests that the NavigationEntry gets marked as active mixed content,
// even if there is a certificate error. Regression test for
// https://crbug.com/593950.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestBrokenHTTPSWithActiveInsecureContent) {
  ASSERT_TRUE(https_server_expired_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  // Navigate to a page with a certificate error and click through the
  // interstitial.
  ui_test_utils::NavigateToURL(
      browser(),
      https_server_expired_.GetURL("/ssl/page_runs_insecure_content.html"));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);
  ProceedThroughInterstitial(tab);

  // Now check that the page is marked as having run insecure content.
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::RAN_INSECURE_CONTENT);
}

namespace {

// A WebContentsObserver that allows the user to wait for a same-document
// navigation. Tests using this observer will fail if a non-same-document
// navigation completes after calling WaitForSameDocumentNavigation.
class SameDocumentNavigationObserver : public content::WebContentsObserver {
 public:
  explicit SameDocumentNavigationObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~SameDocumentNavigationObserver() override {}

  void WaitForSameDocumentNavigation() { run_loop_.Run(); }

  // WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    ASSERT_TRUE(navigation_handle->IsSameDocument());
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
};

}  // namespace

// Tests that the mixed content flags are reset when going back to an existing
// navigation entry that had mixed content. Regression test for
// https://crbug.com/750649.
IN_PROC_BROWSER_TEST_F(SSLUITest, GoBackToMixedContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  // Navigate to a URL and dynamically load mixed content.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
  ssl_test_util::SecurityStateWebContentsObserver observer(tab);
  ASSERT_TRUE(content::ExecuteScript(tab,
                                     "var i = document.createElement('img');"
                                     "i.src = 'http://example.test';"
                                     "document.body.appendChild(i);"));
  observer.WaitForDidChangeVisibleSecurityState();
  ssl_test_util::CheckSecurityState(tab, CertError::NONE,
                                    GetPassiveMixedContentSecurityLevel(),
                                    AuthState::DISPLAYED_INSECURE_CONTENT);

  // Now navigate somewhere else, and then back to the page that dynamically
  // loaded mixed content.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html"));
  ssl_test_util::CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(tab);
  // After going back, the mixed content indicator should no longer be present.
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// Tests that the mixed content flags are not reset for an in-page navigation.
IN_PROC_BROWSER_TEST_F(SSLUITest, MixedContentWithSameDocumentNavigation) {
  ASSERT_TRUE(https_server_.Start());

  // Navigate to a URL and dynamically load mixed content.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
  ssl_test_util::SecurityStateWebContentsObserver security_state_observer(tab);
  ASSERT_TRUE(content::ExecuteScript(tab,
                                     "var i = document.createElement('img');"
                                     "i.src = 'http://example.test';"
                                     "document.body.appendChild(i);"));
  security_state_observer.WaitForDidChangeVisibleSecurityState();
  ssl_test_util::CheckSecurityState(tab, CertError::NONE,
                                    GetPassiveMixedContentSecurityLevel(),
                                    AuthState::DISPLAYED_INSECURE_CONTENT);

  // Initiate a same-document navigation and check that the page is still
  // marked as having displayed mixed content.
  SameDocumentNavigationObserver navigation_observer(tab);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html#foo"));
  navigation_observer.WaitForSameDocumentNavigation();
  ssl_test_util::CheckSecurityState(tab, CertError::NONE,
                                    GetPassiveMixedContentSecurityLevel(),
                                    AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Tests that the WebContents's flag for displaying content with cert
// errors get cleared upon navigation.
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       DisplayedContentWithCertErrorsClearedOnNavigation) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  // Navigate to a page with a certificate error and click through the
  // interstitial.
  ui_test_utils::NavigateToURL(
      browser(),
      https_server_expired_.GetURL("/ssl/page_with_subresource.html"));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);
  ProceedThroughInterstitial(tab);

  content::NavigationEntry* entry = tab->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_TRUE(entry->GetSSL().content_status &
              content::SSLStatus::DISPLAYED_CONTENT_WITH_CERT_ERRORS);

  // Navigate away to a different page, and check that the flag gets cleared.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  entry = tab->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_FALSE(entry->GetSSL().content_status &
               content::SSLStatus::DISPLAYED_CONTENT_WITH_CERT_ERRORS);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestBrokenHTTPSMetricsReporting_Proceed) {
  ASSERT_TRUE(https_server_expired_.Start());
  base::HistogramTester histograms;
  const std::string decision_histogram =
      "interstitial.ssl_overridable.decision";
  const std::string interaction_histogram =
      "interstitial.ssl_overridable.interaction";

  // Histograms should start off empty.
  histograms.ExpectTotalCount(decision_histogram, 0);
  histograms.ExpectTotalCount(interaction_histogram, 0);

  // After navigating to the page, the totals should be set.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_expired_.GetURL("/title1.html"));
  WaitForInterstitial(browser()->tab_strip_model()->GetActiveWebContents());
  histograms.ExpectTotalCount(decision_histogram, 1);
  histograms.ExpectBucketCount(decision_histogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectTotalCount(interaction_histogram, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);

  // Decision should be recorded.
  SendInterstitialCommand(browser()->tab_strip_model()->GetActiveWebContents(),
                          security_interstitials::CMD_PROCEED);
  histograms.ExpectTotalCount(decision_histogram, 2);
  histograms.ExpectBucketCount(
      decision_histogram, security_interstitials::MetricsHelper::PROCEED, 1);
  histograms.ExpectTotalCount(interaction_histogram, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestBrokenHTTPSMetricsReporting_DontProceed) {
  ASSERT_TRUE(https_server_expired_.Start());
  base::HistogramTester histograms;
  const std::string decision_histogram =
      "interstitial.ssl_overridable.decision";
  const std::string interaction_histogram =
      "interstitial.ssl_overridable.interaction";

  // Histograms should start off empty.
  histograms.ExpectTotalCount(decision_histogram, 0);
  histograms.ExpectTotalCount(interaction_histogram, 0);

  // After navigating to the page, the totals should be set.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_expired_.GetURL("/title1.html"));
  WaitForInterstitial(browser()->tab_strip_model()->GetActiveWebContents());
  histograms.ExpectTotalCount(decision_histogram, 1);
  histograms.ExpectBucketCount(decision_histogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectTotalCount(interaction_histogram, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);

  // Decision should be recorded.
  SendInterstitialCommand(browser()->tab_strip_model()->GetActiveWebContents(),
                          security_interstitials::CMD_DONT_PROCEED);
  histograms.ExpectTotalCount(decision_histogram, 2);
  histograms.ExpectBucketCount(
      decision_histogram, security_interstitials::MetricsHelper::DONT_PROCEED,
      1);
  histograms.ExpectTotalCount(interaction_histogram, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);
}

// Visits a page over OK https:
IN_PROC_BROWSER_TEST_F(SSLUITest, TestOKHTTPS) {
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));

  ssl_test_util::CheckAuthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);
}

// Visits a page with https error and proceed:
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSExpiredCertAndProceed) {
  ASSERT_TRUE(https_server_expired_.Start());

  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitial(tab);
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
  EXPECT_EQ(https_server_expired_.GetURL("/ssl/google.html"),
            tab->GetVisibleURL());
}

// Visits a page in an app window with https error and proceed:
IN_PROC_BROWSER_TEST_F(SSLUITest, InAppTestHTTPSExpiredCertAndProceed) {
  ASSERT_TRUE(https_server_expired_.Start());

  const GURL app_url = https_server_expired_.GetURL("/ssl/google.html");
  Browser* app_browser = InstallAndOpenTestWebApp(app_url);

  WebContents* app_tab = app_browser->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitial(app_tab);
  ssl_test_util::CheckAuthenticationBrokenState(
      app_tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitialInAppAndCheckNewTabOpened(app_browser, app_url);
}

// Visits a page with https error and proceed. Then open the app and proceed.
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       InAppTestHTTPSExpiredCertAndPreviouslyProceeded) {
  ASSERT_TRUE(https_server_expired_.Start());

  const GURL app_url = https_server_expired_.GetURL("/ssl/google.html");

  // Go through the interstitial in a regular browser tab.
  ui_test_utils::NavigateToURL(browser(), app_url);

  WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitial(initial_tab);
  ssl_test_util::CheckAuthenticationBrokenState(
      initial_tab, net::CERT_STATUS_DATE_INVALID,
      AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(initial_tab);
  ssl_test_util::CheckAuthenticationBrokenState(
      initial_tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);

  Browser* app_browser = InstallAndOpenTestWebApp(app_url);

  // Apps are not allowed to have SSL errors, so the interstitial should be
  // showing even though the user proceeded through it in a regular tab.
  WebContents* app_tab = app_browser->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitial(app_tab);
  ssl_test_util::CheckAuthenticationBrokenState(
      app_tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitialInAppAndCheckNewTabOpened(app_browser, app_url);
}

// Visits a page with https error and don't proceed (and ensure we can still
// navigate at that point):
IN_PROC_BROWSER_TEST_F(SSLUITest, TestInterstitialCrossSiteNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_mismatched_.Start());

  // First navigate to an OK page.
  GURL initial_url = https_server_.GetURL("/ssl/google.html");
  ASSERT_EQ("127.0.0.1", initial_url.host());
  ui_test_utils::NavigateToURL(browser(), initial_url);
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate from 127.0.0.1 to localhost so it triggers a
  // cross-site navigation to make sure http://crbug.com/5800 is gone.
  GURL cross_site_url = https_server_mismatched_.GetURL("/ssl/google.html");
  ASSERT_EQ("localhost", cross_site_url.host());
  ui_test_utils::NavigateToURL(browser(), cross_site_url);
  // An interstitial should be showing.
  WaitForInterstitial(tab);
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_COMMON_NAME_INVALID,
      AuthState::SHOWING_INTERSTITIAL);
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));

  // Simulate user clicking "Take me back".
  DontProceedThroughInterstitial(tab);

  // We should be back to the original good page.
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  // Navigate to a new page to make sure bug 5800 is fixed.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html"));
  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);
}

// Test that localhost pages don't show an interstitial.
IN_PROC_BROWSER_TEST_F(SSLUITestIgnoreLocalhostCertErrors,
                       TestNoInterstitialOnLocalhost) {
  ASSERT_TRUE(https_server_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to a localhost page.
  GURL url = https_server_.GetURL("/ssl/page_with_subresource.html");
  GURL::Replacements replacements;
  std::string new_host("localhost");
  replacements.SetHostStr(new_host);
  url = url.ReplaceComponents(replacements);

  ui_test_utils::NavigateToURL(browser(), url);

  // We should see no interstitial, but we should have an error
  // (red-crossed-out-https) in the URL bar.
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_COMMON_NAME_INVALID, AuthState::NONE);

  // We should see that the script tag in the page loaded and ran (and
  // wasn't blocked by the certificate error).
  base::string16 title;
  base::string16 expected_title = base::ASCIIToUTF16("This script has loaded");
  ui_test_utils::GetCurrentTabTitle(browser(), &title);
  EXPECT_EQ(title, expected_title);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSErrorCausedByClockUsingBuildTime) {
  ASSERT_TRUE(https_server_expired_.Start());

  // Set up the build and current clock times to be more than a year apart.
  std::unique_ptr<base::SimpleTestClock> mock_clock(
      new base::SimpleTestClock());
  mock_clock->SetNow(base::Time::NowFromSystemTime());
  mock_clock->Advance(base::TimeDelta::FromDays(367));
  SSLErrorHandler::SetClockForTesting(mock_clock.get());
  ssl_errors::SetBuildTimeForTesting(base::Time::NowFromSystemTime());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_expired_.GetURL("/title1.html"));
  WebContents* clock_tab = browser()->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitial(clock_tab);
  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingBadClockInterstitial(clock_tab));
  ssl_test_util::CheckSecurityState(clock_tab, net::CERT_STATUS_DATE_INVALID,
                                    security_state::DANGEROUS,
                                    AuthState::SHOWING_INTERSTITIAL);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSErrorCausedByClockUsingNetwork) {
  ASSERT_TRUE(https_server_expired_.Start());

  // Set network forward ten minutes, which is sufficient to trigger
  // the interstitial.
  g_browser_process->network_time_tracker()->UpdateNetworkTime(
      base::Time::Now() + base::TimeDelta::FromMinutes(10),
      base::TimeDelta::FromMilliseconds(1),   /* resolution */
      base::TimeDelta::FromMilliseconds(500), /* latency */
      base::TimeTicks::Now() /* posting time of this update */);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_expired_.GetURL("/title1.html"));
  WebContents* clock_tab = browser()->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitial(clock_tab);
  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingBadClockInterstitial(clock_tab));
  ssl_test_util::CheckSecurityState(clock_tab, net::CERT_STATUS_DATE_INVALID,
                                    security_state::DANGEROUS,
                                    AuthState::SHOWING_INTERSTITIAL);
}

// Visits a page with https error and then goes back using Browser::GoBack.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSExpiredCertAndGoBackViaButton) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // First navigate to an HTTP page.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Now go to a bad HTTPS page that shows an interstitial.
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  // Simulate user clicking on back button (crbug.com/39248).
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(tab);

  // We should be back at the original good page.
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(tab));
  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);
}

// Visits a page with https error and then goes back using GoToOffset.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSExpiredCertAndGoBackViaMenu) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // First navigate to an HTTP page.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Now go to a bad HTTPS page that shows an interstitial.
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  // Simulate user clicking and holding on back button (crbug.com/37215). With
  // committed interstitials enabled, this triggers a navigation.
  content::TestNavigationObserver nav_observer(tab);
  tab->GetController().GoToOffset(-1);
  nav_observer.Wait();

  // We should be back at the original good page.
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(tab));
  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);
}

// Visits a page with https error and then goes back using the DONT_PROCEED
// interstitial command.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSExpiredCertGoBackUsingCommand) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // First navigate to an HTTP page.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Now go to a bad HTTPS page that shows an interstitial.
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(&tab->GetController()));
  SendInterstitialCommand(tab, security_interstitials::CMD_DONT_PROCEED);
  observer.Wait();

  // We should be back at the original good page.
  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);
}

// Visits a page with revocation checking enabled and a valid OCSP response.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSOCSPOk) {
  // OCSP checking is disabled by default.
  EXPECT_FALSE(last_ssl_config_.rev_checking_enabled);
  EXPECT_FALSE(CreateDefaultNetworkContextParams()
                   ->initial_ssl_config->rev_checking_enabled);

  // Enable, and make sure the default network context params reflect the
  // change.
  base::RunLoop run_loop;
  set_ssl_config_updated_callback(run_loop.QuitClosure());
  ASSERT_NO_FATAL_FAILURE(
      EnablePolicy(g_browser_process->local_state(),
                   policy::key::kEnableOnlineRevocationChecks,
                   prefs::kCertRevocationCheckingEnabled));
  run_loop.Run();
  EXPECT_TRUE(last_ssl_config_.rev_checking_enabled);
  EXPECT_TRUE(CreateDefaultNetworkContextParams()
                  ->initial_ssl_config->rev_checking_enabled);

  ASSERT_TRUE(https_server_ocsp_ok_.Start());

  ui_test_utils::NavigateToURL(
      browser(), https_server_ocsp_ok_.GetURL("/ssl/google.html"));

  ssl_test_util::CheckAuthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);

  content::NavigationEntry* entry = browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetController()
                                        .GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_TRUE(entry->GetSSL().cert_status &
              net::CERT_STATUS_REV_CHECKING_ENABLED);
}

// Visits a page with revocation checking enabled and a revoked OCSP response.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSOCSPRevoked) {
  // OCSP checking is disabled by default.
  EXPECT_FALSE(last_ssl_config_.rev_checking_enabled);
  EXPECT_FALSE(CreateDefaultNetworkContextParams()
                   ->initial_ssl_config->rev_checking_enabled);

  // Enable, and make sure the default network context params reflect the
  // change.
  base::RunLoop run_loop;
  set_ssl_config_updated_callback(run_loop.QuitClosure());
  ASSERT_NO_FATAL_FAILURE(
      EnablePolicy(g_browser_process->local_state(),
                   policy::key::kEnableOnlineRevocationChecks,
                   prefs::kCertRevocationCheckingEnabled));
  run_loop.Run();
  EXPECT_TRUE(last_ssl_config_.rev_checking_enabled);
  EXPECT_TRUE(CreateDefaultNetworkContextParams()
                  ->initial_ssl_config->rev_checking_enabled);

  ASSERT_TRUE(https_server_ocsp_revoked_.Start());

  ui_test_utils::NavigateToURL(
      browser(), https_server_ocsp_revoked_.GetURL("/ssl/google.html"));

  ssl_test_util::CheckAuthenticationBrokenState(
      browser()->tab_strip_model()->GetActiveWebContents(),
      net::CERT_STATUS_REVOKED, AuthState::SHOWING_INTERSTITIAL);
}

// Visits a page with revocation checking set to the default value (disabled)
// and a revoked OCSP response.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSOCSPRevokedButNotChecked) {
  // OCSP checking is disabled by default.
  EXPECT_FALSE(last_ssl_config_.rev_checking_enabled);
  EXPECT_FALSE(CreateDefaultNetworkContextParams()
                   ->initial_ssl_config->rev_checking_enabled);

  ASSERT_TRUE(https_server_ocsp_revoked_.Start());

  ui_test_utils::NavigateToURL(
      browser(), https_server_ocsp_revoked_.GetURL("/ssl/google.html"));

  ssl_test_util::CheckAuthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);

  content::NavigationEntry* entry = browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetController()
                                        .GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_FALSE(entry->GetSSL().cert_status &
               net::CERT_STATUS_REV_CHECKING_ENABLED);
}

// Visits a page that uses a SHA-1 leaf certificate, which should be rejected
// by default.
IN_PROC_BROWSER_TEST_F(SSLUITest, SHA1IsDefaultDisabled) {
  EXPECT_FALSE(last_ssl_config_.sha1_local_anchors_enabled);
  EXPECT_FALSE(CreateDefaultNetworkContextParams()
                   ->initial_ssl_config->sha1_local_anchors_enabled);

  ASSERT_TRUE(https_server_sha1_.Start());
  ui_test_utils::NavigateToURL(browser(),
                               https_server_sha1_.GetURL("/ssl/google.html"));

  int expected_error = net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM;

#if defined(OS_MACOSX)
  // On macOS 10.15 (and presumably later) SHA1 certs are considered prima
  // facie invalid by the system verifier.
  // TODO(https://crbug.com/977767): Reconsider this when the built-in verifier
  // is used on Mac.
  if (base::mac::IsAtLeastOS10_15())
    expected_error |= net::CERT_STATUS_INVALID;
#endif

  ssl_test_util::CheckAuthenticationBrokenState(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_error,
      AuthState::SHOWING_INTERSTITIAL);
}

// By default, trust in Symantec's Legacy PKI should be disabled. Unfortunately,
// there is currently no way to simulate navigation to a page that will
// meaningfully test that Symantec enforcement is actually applied to the
// request.
IN_PROC_BROWSER_TEST_F(SSLUITest, SymantecEnforcementIsNotDisabled) {
  EXPECT_FALSE(last_ssl_config_.symantec_enforcement_disabled);
  EXPECT_FALSE(CreateDefaultNetworkContextParams()
                   ->initial_ssl_config->symantec_enforcement_disabled);
}

class CertificateTransparencySSLUITest : public CertVerifierBrowserTest {
 public:
  CertificateTransparencySSLUITest()
      : CertVerifierBrowserTest(),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        true);
  }
  ~CertificateTransparencySSLUITest() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        base::nullopt);
  }

  void SetUpOnMainThread() override {
    CertVerifierBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
  }

  void SetUp() override {
    EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    CertVerifierBrowserTest::SetUp();
  }

  // Sets the policy identified by |policy_name| to the value specified by
  // |list_values|, ensuring that the corresponding list pref |pref_name| is
  // updated to match. |policy_name| must specify a policy that is a list of
  // string values.
  void ConfigureStringListPolicy(PrefService* pref_service,
                                 const char* policy_name,
                                 const char* pref_name,
                                 const std::vector<std::string>& list_values) {
    std::unique_ptr<base::ListValue> policy_value =
        std::make_unique<base::ListValue>();
    for (const auto& value : list_values) {
      policy_value->Append(value);
    }
    policy::PolicyMap policy_map;
    policy_map.Set(policy_name, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                   std::move(policy_value), nullptr);

    EXPECT_NO_FATAL_FAILURE(UpdateChromePolicy(policy_map));

    const base::ListValue* pref_value = pref_service->GetList(pref_name);
    ASSERT_TRUE(pref_value);
    std::vector<std::string> pref_values;
    for (const auto& value : pref_value->GetList()) {
      ASSERT_TRUE(value.is_string());
      pref_values.push_back(value.GetString());
    }
    EXPECT_THAT(pref_values, testing::UnorderedElementsAreArray(list_values));
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  void UpdateChromePolicy(const policy::PolicyMap& policies) {
    policy_provider_.UpdateChromePolicy(policies);
    ASSERT_TRUE(base::MessageLoopCurrent::Get());

    base::RunLoop().RunUntilIdle();

    content::FlushNetworkServiceInstanceForTesting();
  }

  net::EmbeddedTestServer https_server_;

  policy::MockConfigurationPolicyProvider policy_provider_;

  DISALLOW_COPY_AND_ASSIGN(CertificateTransparencySSLUITest);
};

// Visit an HTTPS page that has a publicly trusted certificate issued after
// the Certificate Transparency requirement date of April 2018. The connection
// should be blocked, as the server will not be providing CT details, and the
// Chrome CT Policy should be being enforced.
IN_PROC_BROWSER_TEST_F(CertificateTransparencySSLUITest,
                       EnforcedAfterApril2018) {
  ASSERT_TRUE(https_server()->Start());

  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "may_2018.pem");
  ASSERT_TRUE(verify_result.verified_cert);
  verify_result.is_issued_by_known_root = true;
  verify_result.public_key_hashes.push_back(
      GetSPKIHash(verify_result.verified_cert->cert_buffer()));

  mock_cert_verifier()->AddResultForCert(https_server()->GetCertificate().get(),
                                         verify_result, net::OK);

  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("/ssl/google.html"));

  ssl_test_util::CheckSecurityState(
      browser()->tab_strip_model()->GetActiveWebContents(),
      net::CERT_STATUS_CERTIFICATE_TRANSPARENCY_REQUIRED,
      security_state::DANGEROUS, AuthState::SHOWING_INTERSTITIAL);
}

// Visit an HTTPS page that has a publicly trusted certificate issued after
// the Certificate Transparency requirement date of April 2018. The connection
// would normally be blocked, as the server will not be providing CT details,
// and the Chrome CT Policy should be being enforced; however, because a policy
// configuration exists that disables CT enforcement for that cert, the
// connection should succeed.
IN_PROC_BROWSER_TEST_F(CertificateTransparencySSLUITest,
                       EnforcedAfterApril2018UnlessPoliciesSet) {
  ASSERT_TRUE(https_server()->Start());

  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "may_2018.pem");
  ASSERT_TRUE(verify_result.verified_cert);
  verify_result.is_issued_by_known_root = true;
  verify_result.public_key_hashes.push_back(
      GetSPKIHash(verify_result.verified_cert->cert_buffer()));

  mock_cert_verifier()->AddResultForCert(https_server()->GetCertificate().get(),
                                         verify_result, net::OK);

  ASSERT_NO_FATAL_FAILURE(ConfigureStringListPolicy(
      browser()->profile()->GetPrefs(),
      policy::key::kCertificateTransparencyEnforcementDisabledForCas,
      certificate_transparency::prefs::kCTExcludedSPKIs,
      {verify_result.public_key_hashes.back().ToString()}));

  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("/ssl/google.html"));
  ssl_test_util::CheckSecurityState(
      browser()->tab_strip_model()->GetActiveWebContents(), CertError::NONE,
      security_state::SECURE, AuthState::NONE);
}

// Visit an HTTPS page that has a certificate issued by a certificate authority
// that is trusted in a root store that Chrome does not consider consistently
// secure. In the case where the certificate was issued after the Certificate
// Transparency requirement date of April 2018 the connection would normally be
// blocked, as the server will not be providing CT details, and the Chrome CT
// Policy should be being enforced; however, because a policy configuration
// exists that disables CT enforcement for that Legacy cert, the connection
// should succeed. For more detail, see /net/docs/certificate-transparency.md
IN_PROC_BROWSER_TEST_F(CertificateTransparencySSLUITest,
                       LegacyEnforcedAfterApril2018UnlessPoliciesSet) {
  ASSERT_TRUE(https_server()->Start());

  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "may_2018.pem");
  ASSERT_TRUE(verify_result.verified_cert);
  verify_result.is_issued_by_known_root = true;

  // We'll use a SPKI hash corresponding to the Federal Common Policy CA as
  // captured at https://fpki.idmanagement.gov/announcements/mspkichanges/
  const net::SHA256HashValue legacy_spki_hash = {
      0x8e, 0x8b, 0x56, 0xf5, 0x91, 0x8a, 0x25, 0xbd, 0x85, 0xdc, 0xe7,
      0x66, 0x63, 0xfd, 0x94, 0xcc, 0x23, 0x69, 0x0f, 0x10, 0xea, 0x95,
      0x86, 0x61, 0x31, 0x71, 0xc6, 0xf8, 0x37, 0x88, 0x90, 0xd5};
  verify_result.public_key_hashes.push_back(net::HashValue(legacy_spki_hash));

  mock_cert_verifier()->AddResultForCert(https_server()->GetCertificate().get(),
                                         verify_result, net::OK);

  ASSERT_NO_FATAL_FAILURE(ConfigureStringListPolicy(
      browser()->profile()->GetPrefs(),
      policy::key::kCertificateTransparencyEnforcementDisabledForLegacyCas,
      certificate_transparency::prefs::kCTExcludedLegacySPKIs,
      {verify_result.public_key_hashes.back().ToString()}));

  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("/ssl/google.html"));
  ssl_test_util::CheckSecurityState(
      browser()->tab_strip_model()->GetActiveWebContents(), CertError::NONE,
      security_state::SECURE, AuthState::NONE);
}

// Visit a HTTP page which request WSS connection to a server providing invalid
// certificate. Close the page while WSS connection waits for SSLManager's
// response from UI thread.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestWSSInvalidCertAndClose) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(wss_server_expired_.Start());

  // Setup page title observer.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher watcher(tab, base::ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(base::ASCIIToUTF16("FAIL"));

  // Create GURLs to test pages.
  std::string master_url_path = base::StringPrintf(
      "%s?%d",
      embedded_test_server()->GetURL("/ssl/wss_close.html").spec().c_str(),
      wss_server_expired_.host_port_pair().port());
  GURL master_url(master_url_path);
  std::string slave_url_path =
      base::StringPrintf("%s?%d",
                         embedded_test_server()
                             ->GetURL("/ssl/wss_close_slave.html")
                             .spec()
                             .c_str(),
                         wss_server_expired_.host_port_pair().port());
  GURL slave_url(slave_url_path);

  // Create tabs and visit pages which keep on creating wss connections.
  WebContents* tabs[16];
  for (int i = 0; i < 16; ++i) {
    tabs[i] = chrome::AddSelectedTabWithURL(browser(), slave_url,
                                            ui::PAGE_TRANSITION_LINK);
  }
  chrome::SelectNextTab(browser());

  // Visit a page which waits for one TLS handshake failure.
  // The title will be changed to 'PASS'.
  ui_test_utils::NavigateToURL(browser(), master_url);
  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(base::LowerCaseEqualsASCII(result, "pass"));

  // Close tabs which contains the test page.
  for (int i = 0; i < 16; ++i)
    chrome::CloseWebContents(browser(), tabs[i], false);
  chrome::CloseWebContents(browser(), tab, false);
}

// Visit a HTTPS page and proceeds despite an invalid certificate. The page
// requests WSS connection to the same origin host to check if WSS connection
// share certificates policy with HTTPS correcly.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestWSSInvalidCert) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(wss_server_expired_.Start());

  // Setup page title observer.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher watcher(tab, base::ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(base::ASCIIToUTF16("FAIL"));

  // Visit bad HTTPS page.
  GURL::Replacements replacements;
  replacements.SetSchemeStr("https");
  ui_test_utils::NavigateToURL(browser(),
                               wss_server_expired_.GetURL("connect_check.html")
                                   .ReplaceComponents(replacements));
  WaitForInterstitial(tab);
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  // Proceed anyway.
  ProceedThroughInterstitial(tab);

  // Test page run a WebSocket wss connection test. The result will be shown
  // as page title.
  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(base::LowerCaseEqualsASCII(result, "pass"));
}

class SSLUITestWithHttpDangerous : public SSLUITest {
 public:
  SSLUITestWithHttpDangerous() {
    feature_list_.InitAndEnableFeatureWithParameters(
        security_state::features::kMarkHttpAsFeature,
        {{security_state::features::kMarkHttpAsFeatureParameterName,
          security_state::features::kMarkHttpAsParameterDangerous}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Ensure that non-standard origins are marked as neutral when the
// MarkNonSecureAs Dangerous flag is enabled.
IN_PROC_BROWSER_TEST_F(SSLUITestWithHttpDangerous, MarkFileAsNonSecure) {
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ui_test_utils::NavigateToURL(browser(), GURL("file:///"));
  EXPECT_EQ(security_state::NONE, helper->GetSecurityLevel());
}

// Ensure that about-protocol origins are marked as neutral when the
// MarkNonSecureAs Dangerous flag is enabled.
IN_PROC_BROWSER_TEST_F(SSLUITestWithHttpDangerous, MarkAboutAsNonSecure) {
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  EXPECT_EQ(security_state::NONE, helper->GetSecurityLevel());
}

// Data URLs should always be marked as non-secure.
IN_PROC_BROWSER_TEST_F(SSLUITest, MarkDataAsNonSecure) {
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ui_test_utils::NavigateToURL(browser(), GURL("data:text/plain,hello"));
  EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());
}

// Ensure that HTTP-protocol origins are marked as Dangerous when the
// MarkNonSecureAs Dangerous flag is enabled.
IN_PROC_BROWSER_TEST_F(SSLUITestWithHttpDangerous, MarkHTTPAsDangerous) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a non-local HTTP page.
  GURL url = embedded_test_server()->GetURL("/ssl/google.html");
  GURL::Replacements http_url_replacements;
  http_url_replacements.SetHostStr("example.test");
  url = url.ReplaceComponents(http_url_replacements);
  ui_test_utils::NavigateToURL(browser(), url);
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStateTabHelper* helper = SecurityStateTabHelper::FromWebContents(tab);
  ASSERT_TRUE(helper);

  EXPECT_EQ(security_state::DANGEROUS, helper->GetSecurityLevel());
}

// Ensure that blob-protocol origins are marked as neutral when the
// MarkNonSecureAs Dangerous flag is enabled.
IN_PROC_BROWSER_TEST_F(SSLUITestWithHttpDangerous, MarkBlobAsNonSecure) {
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ui_test_utils::NavigateToURL(
      browser(),
      GURL("blob:chrome://newtab/49a463bb-fac8-476c-97bf-5d7076c3ea1a"));
  EXPECT_EQ(security_state::NONE, helper->GetSecurityLevel());
}

#if defined(USE_NSS_CERTS)
class SSLUITestWithClientCert : public SSLUITestBase {
 public:
  SSLUITestWithClientCert() : cert_db_(NULL) {}

  void SetUpOnMainThread() override {
    SSLUITestBase::SetUpOnMainThread();

    base::RunLoop loop;
    GetNSSCertDatabaseForProfile(
        browser()->profile(),
        base::Bind(&SSLUITestWithClientCert::DidGetCertDatabase,
                   base::Unretained(this), &loop));
    loop.Run();
  }

 protected:
  void DidGetCertDatabase(base::RunLoop* loop, net::NSSCertDatabase* cert_db) {
    cert_db_ = cert_db;
    loop->Quit();
  }

  net::NSSCertDatabase* cert_db_;
};

// SSL client certificate tests are only enabled when using NSS for private key
// storage, as only NSS can avoid modifying global machine state when testing.
// See http://crbug.com/51132

// Visit a HTTPS page which requires client cert authentication. The client
// cert will be selected automatically, then a test which uses WebSocket runs.
IN_PROC_BROWSER_TEST_F(SSLUITestWithClientCert, TestWSSClientCert) {
  // Import a client cert for test.
  crypto::ScopedPK11Slot public_slot = cert_db_->GetPublicSlot();
  std::string pkcs12_data;
  base::FilePath cert_path = net::GetTestCertsDirectory().Append(
      FILE_PATH_LITERAL("websocket_client_cert.p12"));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::ReadFileToString(cert_path, &pkcs12_data));
  }
  EXPECT_EQ(net::OK,
            cert_db_->ImportFromPKCS12(public_slot.get(), pkcs12_data,
                                       base::string16(), true, nullptr));

  // Start WebSocket test server with TLS and client cert authentication.
  net::SpawnedTestServer::SSLOptions options(
      net::SpawnedTestServer::SSLOptions::CERT_OK);
  options.request_client_certificate = true;
  base::FilePath ca_path = net::GetTestCertsDirectory().Append(
      FILE_PATH_LITERAL("websocket_cacert.pem"));
  options.client_authorities.push_back(ca_path);
  net::SpawnedTestServer wss_server(net::SpawnedTestServer::TYPE_WSS, options,
                                    net::GetWebSocketTestDataDirectory());
  ASSERT_TRUE(wss_server.Start());
  GURL::Replacements replacements;
  replacements.SetSchemeStr("https");
  GURL url =
      wss_server.GetURL("connect_check.html").ReplaceComponents(replacements);

  // Setup page title observer.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher watcher(tab, base::ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(base::ASCIIToUTF16("FAIL"));

  // Add an entry into AutoSelectCertificateForUrls policy for automatic client
  // cert selection.
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  DCHECK(profile);
  std::unique_ptr<base::DictionaryValue> setting =
      std::make_unique<base::DictionaryValue>();
  base::Value* filters = setting->SetKey("filters", base::ListValue());
  base::DictionaryValue filter = base::DictionaryValue();
  filter.SetString("ISSUER.CN", "pywebsocket");
  filters->Append(std::move(filter));
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetWebsiteSettingDefaultScope(
          url, GURL(), ContentSettingsType::AUTO_SELECT_CERTIFICATE,
          std::string(), std::move(setting));

  // Visit a HTTPS page which requires client certs.
  ui_test_utils::NavigateToURL(browser(), url);
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  // Test page runs a WebSocket wss connection test. The result will be shown
  // as page title.
  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(base::LowerCaseEqualsASCII(result, "pass"));
}
#endif  // defined(USE_NSS_CERTS)

// A stub ClientCertStore that returns a FakeClientCertIdentity.
class ClientCertStoreStub : public net::ClientCertStore {
 public:
  explicit ClientCertStoreStub(net::ClientCertIdentityList list)
      : list_(std::move(list)) {}

  ~ClientCertStoreStub() override {}

  // net::ClientCertStore:
  void GetClientCerts(const net::SSLCertRequestInfo& cert_request_info,
                      ClientCertListCallback callback) override {
    std::move(callback).Run(std::move(list_));
  }

 private:
  net::ClientCertIdentityList list_;
};

std::unique_ptr<net::ClientCertStore> CreateCertStore() {
  base::FilePath certs_dir = net::GetTestCertsDirectory();

  net::ClientCertIdentityList cert_identity_list;

  {
    base::ScopedAllowBlockingForTesting allow_blocking;

    std::unique_ptr<net::FakeClientCertIdentity> cert_identity =
        net::FakeClientCertIdentity::CreateFromCertAndKeyFiles(
            certs_dir, "client_1.pem", "client_1.pk8");
    EXPECT_TRUE(cert_identity.get());
    if (cert_identity)
      cert_identity_list.push_back(std::move(cert_identity));
  }

  return std::unique_ptr<net::ClientCertStore>(
      new ClientCertStoreStub(std::move(cert_identity_list)));
}

std::unique_ptr<net::ClientCertStore> CreateFailSigningCertStore() {
  base::FilePath certs_dir = net::GetTestCertsDirectory();

  net::ClientCertIdentityList cert_identity_list;

  {
    base::ScopedAllowBlockingForTesting allow_blocking;

    std::unique_ptr<net::FakeClientCertIdentity> cert_identity =
        net::FakeClientCertIdentity::CreateFromCertAndFailSigning(
            certs_dir, "client_1.pem");
    EXPECT_TRUE(cert_identity.get());
    if (cert_identity)
      cert_identity_list.push_back(std::move(cert_identity));
  }

  return std::unique_ptr<net::ClientCertStore>(
      new ClientCertStoreStub(std::move(cert_identity_list)));
}

std::unique_ptr<net::ClientCertStore> CreateEmptyCertStore() {
  return std::unique_ptr<net::ClientCertStore>(new ClientCertStoreStub({}));
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestBrowserUseClientCertStore) {
  // Make the browser use the ClientCertStoreStub instead of the regular one.
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->set_client_cert_store_factory_for_testing(
          base::BindRepeating(&CreateCertStore));

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  GURL https_url =
      https_server.GetURL("/ssl/browser_use_client_cert_store.html");

  // Add an entry into AutoSelectCertificateForUrls policy for automatic client
  // cert selection.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  DCHECK(profile);
  std::unique_ptr<base::DictionaryValue> setting =
      std::make_unique<base::DictionaryValue>();
  base::Value* filters = setting->SetKey("filters", base::ListValue());
  filters->Append(base::DictionaryValue());
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetWebsiteSettingDefaultScope(
          https_url, GURL(), ContentSettingsType::AUTO_SELECT_CERTIFICATE,
          std::string(), std::move(setting));

  // Visit a HTTPS page which requires client certs.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                            https_url, 1);
  EXPECT_EQ("pass", tab->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestClientAuthSigningFails) {
  // Make the browser use the ClientCertStoreStub instead of the regular one.
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->set_client_cert_store_factory_for_testing(
          base::BindRepeating(&CreateFailSigningCertStore));

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  GURL https_url =
      https_server.GetURL("/ssl/browser_use_client_cert_store.html");

  // Add an entry into AutoSelectCertificateForUrls policy for automatic client
  // cert selection.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  DCHECK(profile);
  std::unique_ptr<base::DictionaryValue> setting =
      std::make_unique<base::DictionaryValue>();
  base::Value* filters = setting->SetKey("filters", base::ListValue());
  filters->Append(base::DictionaryValue());
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetWebsiteSettingDefaultScope(
          https_url, GURL(), ContentSettingsType::AUTO_SELECT_CERTIFICATE,
          std::string(), std::move(setting));

  // Visit a HTTPS page which requires client certs.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                            https_url, 1);
  // Page should not load successfully.
  EXPECT_EQ("", tab->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestClientAuthContinueWithoutCert) {
  // Make the browser use a ClientCertStoreStub that returns no certs.
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->set_client_cert_store_factory_for_testing(
          base::BindRepeating(&CreateEmptyCertStore));

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  GURL https_url =
      https_server.GetURL("/ssl/browser_use_client_cert_store.html");

  // Visit a HTTPS page which requires client certs.
  // The browser should automatically continue to the site without a client
  // cert, since the ClientCertStore returns no certs.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                            https_url, 1);
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  // Page should not load successfully.
  EXPECT_EQ("", tab->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestCertDBChangedFlushesClientAuthCache) {
  // Make the browser use the ClientCertStoreStub instead of the regular one.
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->set_client_cert_store_factory_for_testing(
          base::BindRepeating(&CreateCertStore));

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  GURL https_url =
      https_server.GetURL("/ssl/browser_use_client_cert_store.html");

  // Add an entry into AutoSelectCertificateForUrls policy for automatic client
  // cert selection.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  DCHECK(profile);
  std::unique_ptr<base::DictionaryValue> setting =
      std::make_unique<base::DictionaryValue>();
  base::Value* filters = setting->SetKey("filters", base::ListValue());
  filters->Append(base::DictionaryValue());
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetWebsiteSettingDefaultScope(
          https_url, GURL(), ContentSettingsType::AUTO_SELECT_CERTIFICATE,
          std::string(), std::move(setting));

  // Visit a HTTPS page which requires client certs.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                            https_url, 1);
  EXPECT_EQ("pass", tab->GetLastCommittedURL().ref());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("about:blank"), 1);
  EXPECT_EQ("", tab->GetLastCommittedURL().ref());

  // Now use a ClientCertStoreStub that always returns an empty list.
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->set_client_cert_store_factory_for_testing(
          base::BindRepeating(&CreateEmptyCertStore));

  // Visiting the page which requires client certs should still work (either
  // due to the socket still being open, or due to the SSL client auth cache).
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                            https_url, 1);
  EXPECT_EQ("pass", tab->GetLastCommittedURL().ref());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("about:blank"), 1);
  EXPECT_EQ("", tab->GetLastCommittedURL().ref());

  // Send a CertDBChanged notification.
  net::CertDatabase::GetInstance()->NotifyObserversCertDBChanged();

  // Visiting the page which requires client certs should fail, as the socket
  // pool has been flushed and SSL client auth cache has been cleared due to
  // the CertDBChanged observer.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                            https_url, 1);
  EXPECT_EQ("", tab->GetLastCommittedURL().ref());
}

// Open a page with a HTTPS error in a tab with no prior navigation (through a
// link with a blank target).  This is to test that the lack of navigation entry
// does not cause any problems (it was causing a crasher, see
// http://crbug.com/19941).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSErrorWithNoNavEntry) {
  ASSERT_TRUE(https_server_expired_.Start());

  const GURL url = https_server_expired_.GetURL("/ssl/google.htm");
  WebContents* tab2 =
      chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_TYPED);
  content::WaitForLoadStop(tab2);

  // Verify our assumption that there was no prior navigation.
  EXPECT_FALSE(chrome::CanGoBack(browser()));

  // We should have an interstitial page showing.
  WaitForInterstitial(tab2);
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab2));
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestBadHTTPSDownload) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());
  GURL url_non_dangerous = embedded_test_server()->GetURL("/title1.html");
  GURL url_dangerous =
      https_server_expired_.GetURL("/downloads/dangerous/dangerous.exe");

  // Visit a non-dangerous page.
  ui_test_utils::NavigateToURL(browser(), url_non_dangerous);

  // Now, start a transition to dangerous download.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    NavigateParams navigate_params(browser(), url_dangerous,
                                   ui::PAGE_TRANSITION_TYPED);
    Navigate(&navigate_params);
    observer.Wait();
  }

  // Proceed through the SSL interstitial. This doesn't use
  // ProceedThroughInterstitial() since no page load will commit.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  WaitForInterstitial(tab);
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
  {
    // Wait for the download to complete after proceeding with the download.
    // This serves to verify the download was initiated, and to let the
    // test successfully shut down and cleanup. Exiting the browser with a
    // download still in-progress can lead to test failues.
    content::DownloadTestObserverTerminal dangerous_download_observer(
        content::BrowserContext::GetDownloadManager(browser()->profile()), 1,
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT);
    SendInterstitialCommand(tab, security_interstitials::CMD_PROCEED);
    dangerous_download_observer.WaitForFinished();
  }

  // There should still be an interstitial at this point. Press the
  // back button on the browser. Note that this doesn't wait for a
  // NAV_ENTRY_COMMITTED notification because going back with an
  // active interstitial simply hides the interstitial.
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
  EXPECT_TRUE(chrome::CanGoBack(browser()));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
}

//
// Insecure content
//

// Visits a page that displays insecure content.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestDisplaysInsecureContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair());

  // Load a page that displays insecure content.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));

  ssl_test_util::CheckSecurityState(
      browser()->tab_strip_model()->GetActiveWebContents(), CertError::NONE,
      GetPassiveMixedContentSecurityLevel(),
      AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Visits a page that displays an insecure form.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestDisplaysInsecureForm) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_form.html",
      embedded_test_server()->host_port_pair());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));

  ssl_test_util::CheckSecurityState(
      browser()->tab_strip_model()->GetActiveWebContents(), CertError::NONE,
      security_state::NONE, AuthState::DISPLAYED_FORM_WITH_INSECURE_ACTION);
}

// Verifies that an SSL interstitial generates SafeBrowsing extension api
// events.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestExtensionEvents) {
  class ExtensionEventObserver : public extensions::EventRouter::TestObserver {
   public:
    ExtensionEventObserver() = default;
    ~ExtensionEventObserver() override = default;

    // extensions::EventRouter::TestObserver:
    void OnWillDispatchEvent(const extensions::Event& event) override {
      event_names_.push_back(event.event_name);
    }

    void OnDidDispatchEventToProcess(const extensions::Event& event) override {}

    const std::vector<std::string>& event_names() const { return event_names_; }

   private:
    std::vector<std::string> event_names_;

    DISALLOW_COPY_AND_ASSIGN(ExtensionEventObserver);
  };

  ExtensionEventObserver observer;
  extensions::EventRouter::Get(browser()->profile())
      ->AddObserverForTesting(&observer);

  ASSERT_TRUE(https_server_expired_.Start());

  GURL request_url = https_server_expired_.GetURL("/title1.html");
  ui_test_utils::NavigateToURL(browser(), request_url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab != nullptr);
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  // Verifies that security interstitial shown event is observed.
  EXPECT_TRUE(base::Contains(observer.event_names(),
                             extensions::api::safe_browsing_private::
                                 OnSecurityInterstitialShown::kEventName));

  ProceedThroughInterstitial(tab);

  // Verifies that security interstitial proceeded event is observed.
  EXPECT_TRUE(base::Contains(observer.event_names(),
                             extensions::api::safe_browsing_private::
                                 OnSecurityInterstitialProceeded::kEventName));

  extensions::EventRouter::Get(browser()->profile())
      ->RemoveObserverForTesting(&observer);
}

// Test that a report is sent if the user closes the tab on an interstitial
// before making a decision to proceed or go back.
IN_PROC_BROWSER_TEST_F(SSLUITestWithExtendedReporting,
                       TestBrokenHTTPSReportingCloseTab) {
  ASSERT_TRUE(https_server_expired_.Start());

  base::RunLoop run_loop;
  certificate_reporting_test_utils::SSLCertReporterCallback reporter_callback(
      &run_loop);

  // Opt in to sending reports for invalid certificate chains.
  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(), certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_expired_.GetURL("/title1.html"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab != nullptr);
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  std::unique_ptr<SSLCertReporter> ssl_cert_reporter =
      certificate_reporting_test_utils::CreateMockSSLCertReporter(
          base::Bind(&certificate_reporting_test_utils::
                         SSLCertReporterCallback::ReportSent,
                     base::Unretained(&reporter_callback)),
          certificate_reporting_test_utils::CERT_REPORT_EXPECTED);

  SSLBlockingPage* interstitial_page =
      static_cast<SSLBlockingPage*>(GetInterstitialPage(tab));
  ASSERT_TRUE(interstitial_page);
  interstitial_page->SetSSLCertReporterForTesting(std::move(ssl_cert_reporter));

  EXPECT_EQ(std::string(), reporter_callback.GetLatestHostnameReported());

  // Leave the interstitial by closing the tab.
  chrome::CloseWebContents(browser(), tab, false);
  // Check that the mock reporter received a request to send a report.
  run_loop.Run();
  EXPECT_EQ(https_server_expired_.GetURL("/title1.html").host(),
            reporter_callback.GetLatestHostnameReported());
}

// Test that if the user proceeds and the checkbox is checked, a report
// is sent or not sent depending on the Finch config.
IN_PROC_BROWSER_TEST_F(SSLUITestWithExtendedReporting,
                       TestBrokenHTTPSProceedReporting) {
  certificate_reporting_test_utils::ExpectReport expect_report =
      certificate_reporting_test_utils::GetReportExpectedFromFinch();
  TestBrokenHTTPSReporting(
      certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN,
      SSL_INTERSTITIAL_PROCEED, expect_report, browser());
}

// Test that if the user goes back and the checkbox is checked, a report
// is sent or not sent depending on the Finch config.
IN_PROC_BROWSER_TEST_F(SSLUITestWithExtendedReporting,
                       TestBrokenHTTPSGoBackReporting) {
  certificate_reporting_test_utils::ExpectReport expect_report =
      certificate_reporting_test_utils::GetReportExpectedFromFinch();
  TestBrokenHTTPSReporting(
      certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN,
      SSL_INTERSTITIAL_DO_NOT_PROCEED, expect_report, browser());
}

// User proceeds, checkbox is shown but unchecked. Reports should never
// be sent, regardless of Finch config.
IN_PROC_BROWSER_TEST_F(SSLUITestWithExtendedReporting,
                       TestBrokenHTTPSProceedReportingWithNoOptIn) {
  TestBrokenHTTPSReporting(
      certificate_reporting_test_utils::EXTENDED_REPORTING_DO_NOT_OPT_IN,
      SSL_INTERSTITIAL_PROCEED,
      certificate_reporting_test_utils::CERT_REPORT_NOT_EXPECTED, browser());
}

// User goes back, checkbox is shown but unchecked. Reports should never
// be sent, regardless of Finch config.
IN_PROC_BROWSER_TEST_F(SSLUITestWithExtendedReporting,
                       TestBrokenHTTPSGoBackShowYesCheckNoParamYesReportNo) {
  TestBrokenHTTPSReporting(
      certificate_reporting_test_utils::EXTENDED_REPORTING_DO_NOT_OPT_IN,
      SSL_INTERSTITIAL_DO_NOT_PROCEED,
      certificate_reporting_test_utils::CERT_REPORT_NOT_EXPECTED, browser());
}

// User proceeds, checkbox is not shown but checked -> we expect no
// report.
IN_PROC_BROWSER_TEST_F(SSLUITestWithExtendedReporting,
                       TestBrokenHTTPSProceedShowNoCheckYesReportNo) {
  if (base::FieldTrialList::FindFullName(
          CertReportHelper::kFinchExperimentName) ==
      CertReportHelper::kFinchGroupDontShowDontSend) {
    TestBrokenHTTPSReporting(
        certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN,
        SSL_INTERSTITIAL_PROCEED,
        certificate_reporting_test_utils::CERT_REPORT_NOT_EXPECTED, browser());
  }
}

// Browser is incognito, user proceeds, checkbox has previously opted in
// -> no report, regardless of Finch config.
IN_PROC_BROWSER_TEST_F(SSLUITestWithExtendedReporting,
                       TestBrokenHTTPSInIncognitoReportNo) {
  TestBrokenHTTPSReporting(
      certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN,
      SSL_INTERSTITIAL_PROCEED,
      certificate_reporting_test_utils::CERT_REPORT_NOT_EXPECTED,
      CreateIncognitoBrowser());
}

// Test that reports don't get sent when extended reporting opt-in is
// disabled by policy.
IN_PROC_BROWSER_TEST_F(SSLUITestWithExtendedReporting,
                       TestBrokenHTTPSNoReportingWhenDisallowed) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingOptInAllowed, false);
  TestBrokenHTTPSReporting(
      certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN,
      SSL_INTERSTITIAL_PROCEED,
      certificate_reporting_test_utils::CERT_REPORT_NOT_EXPECTED, browser());
}

// Checkbox is shown but unchecked. Reports should never be sent, regardless of
// Finch config.
IN_PROC_BROWSER_TEST_F(SSLUITestWithExtendedReporting,
                       TestBadClockReportingWithNoOptIn) {
  TestBadClockReporting(
      certificate_reporting_test_utils::EXTENDED_REPORTING_DO_NOT_OPT_IN,
      certificate_reporting_test_utils::CERT_REPORT_NOT_EXPECTED, browser());
}

// Test that when the interstitial closes and the checkbox is checked, a report
// is sent or not sent depending on the Finch config.
IN_PROC_BROWSER_TEST_F(SSLUITestWithExtendedReporting,
                       TestBadClockReportingWithOptIn) {
  certificate_reporting_test_utils::ExpectReport expect_report =
      certificate_reporting_test_utils::GetReportExpectedFromFinch();
  TestBadClockReporting(
      certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN,
      expect_report, browser());
}

// Visits a page that runs insecure content and tries to suppress the insecure
// content warnings by randomizing location.hash.
// Based on http://crbug.com/8706
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRunsInsecuredContentRandomizeHash) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/page_runs_insecure_content.html"));

  ssl_test_util::CheckAuthenticationBrokenState(
      browser()->tab_strip_model()->GetActiveWebContents(), CertError::NONE,
      AuthState::RAN_INSECURE_CONTENT);
}

// Visits an SSL page twice, once with subresources served over good SSL and
// once over bad SSL.
// - For the good SSL case, the iframe and images should be properly displayed.
// - For the bad SSL case, the iframe contents shouldn't be displayed and images
//   and scripts should be filtered out entirely.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestUnsafeContents) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());
  // Enable popups without user gesture.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::POPUPS,
                                 CONTENT_SETTING_ALLOW);
  {
    // First visit the page with its iframe and subresources served over good
    // SSL. This is a sanity check to make sure these resources aren't already
    // broken in the good case.
    std::string replacement_path = GetFilePathWithHostAndPortReplacement(
        "/ssl/page_with_unsafe_contents.html", https_server_.host_port_pair());
    ui_test_utils::NavigateToURL(browser(),
                                 https_server_.GetURL(replacement_path));
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    // The state is expected to be authenticated.
    ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
    // The iframe should be able to open a popup.
    EXPECT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
    // In order to check that the image was loaded, check its width.
    // The actual image (Google logo) is 276 pixels wide.
    int img_width = 0;
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        tab, "window.domAutomationController.send(ImageWidth());", &img_width));
    EXPECT_EQ(img_width, 276);
    // Check that variable |foo| is set.
    bool js_result = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        tab, "window.domAutomationController.send(IsFooSet());", &js_result));
    EXPECT_TRUE(js_result);
  }
  {
    // Now visit the page with its iframe and subresources served over bad
    // SSL. Iframes, images, and scripts should all be blocked.
    std::string replacement_path = GetFilePathWithHostAndPortReplacement(
        "/ssl/page_with_unsafe_contents.html",
        https_server_expired_.host_port_pair());
    ui_test_utils::NavigateToURL(browser(),
                                 https_server_.GetURL(replacement_path));
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    // When the bad content is filtered, the state is expected to be
    // authenticated.
    ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
    // The iframe attempts to open a popup window, but it shouldn't be able to.
    // Previous popup is still open.
    EXPECT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
    // The broken image width is zero.
    int img_width = 99;
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        tab, "window.domAutomationController.send(ImageWidth());", &img_width));
    EXPECT_EQ(img_width, 16);
    // Check that variable |foo| is not set.
    bool js_result = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        tab, "window.domAutomationController.send(IsFooSet());", &js_result));
    EXPECT_FALSE(js_result);
  }
}

// Visits a page with insecure content loaded by JS (after the initial page
// load).
#if defined(OS_LINUX)
// flaky http://crbug.com/396462
#define MAYBE_TestDisplaysInsecureContentLoadedFromJS \
  DISABLED_TestDisplaysInsecureContentLoadedFromJS
#else
#define MAYBE_TestDisplaysInsecureContentLoadedFromJS \
  TestDisplaysInsecureContentLoadedFromJS
#endif
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       MAYBE_TestDisplaysInsecureContentLoadedFromJS) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  net::HostPortPair replacement_pair = embedded_test_server()->host_port_pair();
  replacement_pair.set_host("example.test");

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_dynamic_insecure_content.html", replacement_pair);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  // Load the insecure image.
  bool js_result = false;
  EXPECT_TRUE(
      content::ExecuteScriptAndExtractBool(tab, "loadBadImage();", &js_result));
  EXPECT_TRUE(js_result);

  // We should now have insecure content.
  ssl_test_util::CheckSecurityState(tab, CertError::NONE,
                                    GetPassiveMixedContentSecurityLevel(),
                                    AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Visits two pages from the same origin: one that displays insecure content and
// one that doesn't.  The test checks that we do not propagate the insecure
// content state from one to the other.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestDisplaysInsecureContentTwoTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/blank_page.html"));

  WebContents* tab1 = browser()->tab_strip_model()->GetActiveWebContents();

  // This tab should be fine.
  ssl_test_util::CheckAuthenticatedState(tab1, AuthState::NONE);

  // Create a new tab.
  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair());

  GURL url = https_server_.GetURL(replacement_path);
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.tabstrip_index = 0;
  params.source_contents = tab1;
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  Navigate(&params);
  WebContents* tab2 = params.navigated_or_inserted_contents;
  observer.Wait();

  // The new tab has insecure content.
  ssl_test_util::CheckSecurityState(tab2, CertError::NONE,
                                    GetPassiveMixedContentSecurityLevel(),
                                    AuthState::DISPLAYED_INSECURE_CONTENT);

  // The original tab should not be contaminated.
  ssl_test_util::CheckAuthenticatedState(tab1, AuthState::NONE);
}

// Visits two pages from the same origin: one that runs insecure content and one
// that doesn't.  The test checks that we propagate the insecure content state
// from one to the other.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRunsInsecureContentTwoTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/blank_page.html"));

  WebContents* tab1 = browser()->tab_strip_model()->GetActiveWebContents();

  // This tab should be fine.
  ssl_test_util::CheckAuthenticatedState(tab1, AuthState::NONE);

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content.html",
      embedded_test_server()->host_port_pair());

  // Create a new tab in the same process.  Using a NEW_FOREGROUND_TAB
  // disposition won't usually stay in the same process, but this works
  // because we are using process-per-site in SetUpCommandLine.
  GURL url = https_server_.GetURL(replacement_path);
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.source_contents = tab1;
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  Navigate(&params);
  WebContents* tab2 = params.navigated_or_inserted_contents;
  observer.Wait();

  // Both tabs should have the same process.
  EXPECT_EQ(tab1->GetMainFrame()->GetProcess(),
            tab2->GetMainFrame()->GetProcess());

  // The new tab has insecure content.
  ssl_test_util::CheckAuthenticationBrokenState(
      tab2, CertError::NONE, AuthState::RAN_INSECURE_CONTENT);

  // Which means the origin for the first tab has also been contaminated with
  // insecure content.
  ssl_test_util::CheckAuthenticationBrokenState(
      tab1, CertError::NONE, AuthState::RAN_INSECURE_CONTENT);
}

// Visits a page with an image over http.  Visits another page over https
// referencing that same image over http (hoping it is coming from the webcore
// memory cache).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestDisplaysCachedInsecureContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair());

  // Load original page over HTTP.
  const GURL url_http = embedded_test_server()->GetURL(replacement_path);
  ui_test_utils::NavigateToURL(browser(), url_http);
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);

  // Load again but over SSL.  It should be marked as displaying insecure
  // content (even though the image comes from the WebCore memory cache).
  const GURL url_https = https_server_.GetURL(replacement_path);
  ui_test_utils::NavigateToURL(browser(), url_https);
  ssl_test_util::CheckSecurityState(tab, CertError::NONE,
                                    GetPassiveMixedContentSecurityLevel(),
                                    AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Visits a page with script over http.  Visits another page over https
// referencing that same script over http (hoping it is coming from the webcore
// memory cache).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRunsCachedInsecureContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content.html",
      embedded_test_server()->host_port_pair());

  // Load original page over HTTP.
  const GURL url_http = embedded_test_server()->GetURL(replacement_path);
  ui_test_utils::NavigateToURL(browser(), url_http);
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);

  // Load again but over SSL.  It should be marked as displaying insecure
  // content (even though the image comes from the WebCore memory cache).
  const GURL url_https = https_server_.GetURL(replacement_path);
  ui_test_utils::NavigateToURL(browser(), url_https);
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, CertError::NONE, AuthState::RAN_INSECURE_CONTENT);
}

// This test ensures the CN invalid status does not 'stick' to a certificate
// (see bug #1044942) and that it depends on the host-name.
// Test if disabled due to flakiness http://crbug.com/368280 .
IN_PROC_BROWSER_TEST_F(SSLUITest, DISABLED_TestCNInvalidStickiness) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_mismatched_.Start());

  // First we hit the server with hostname, this generates an invalid policy
  // error.
  ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/google.html"));

  // We get an interstitial page as a result.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_COMMON_NAME_INVALID,
      AuthState::SHOWING_INTERSTITIAL);
  ProceedThroughInterstitial(tab);
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_COMMON_NAME_INVALID, AuthState::NONE);

  // Now we try again with the right host name this time.
  GURL url(https_server_.GetURL("/ssl/google.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  // Security state should be OK.
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  // Now try again the broken one to make sure it is still broken.
  ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/google.html"));

  // Since we OKed the interstitial last time, we get right to the page.
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_COMMON_NAME_INVALID, AuthState::NONE);
}

// Test that navigating to a #ref does not change a bad security state.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRefNavigation) {
  ASSERT_TRUE(https_server_expired_.Start());

  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/page_with_refs.html"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
  // Now navigate to a ref in the page, the security state should not have
  // changed.
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/page_with_refs.html#jp"));

  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
}

// Tests that closing a page that opened a pop-up with an interstitial does not
// crash the browser (crbug.com/1966).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestCloseTabWithUnsafePopup) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // Enable popups without user gesture.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::POPUPS,
                                 CONTENT_SETTING_ALLOW);

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_unsafe_popup.html",
      https_server_expired_.host_port_pair());
  WebContents* tab1 = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(
      https_server_expired_.GetURL("/ssl/bad_iframe.html"));
  nav_observer.StartWatchingNewWebContents();
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(replacement_path));
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));

  // Last activated browser should be the popup.
  Browser* popup_browser = chrome::FindBrowserWithProfile(browser()->profile());
  WebContents* popup = popup_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(popup, tab1);
  nav_observer.Wait();
  WaitForInterstitial(popup);
  ASSERT_TRUE(popup->GetController().GetVisibleEntry());
  EXPECT_EQ(https_server_expired_.GetURL("/ssl/bad_iframe.html"),
            popup->GetController().GetVisibleEntry()->GetURL());
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(popup));

  // Add another tab to make sure the browser does not exit when we close
  // the first tab.
  GURL url = embedded_test_server()->GetURL("/ssl/google.html");
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_TYPED);
  observer.Wait();

  // Close the first tab.
  chrome::CloseWebContents(browser(), tab1, false);
}

// Visit a page over bad https that is a redirect to a page with good https.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectBadToGoodHTTPS) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  GURL url1 = https_server_expired_.GetURL("/server-redirect?");
  GURL url2 = https_server_.GetURL("/ssl/google.html");

  ui_test_utils::NavigateToURL(browser(), GURL(url1.spec() + url2.spec()));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  // We have been redirected to the good page.
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// Visit a page over good https that is a redirect to a page with bad https.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectGoodToBadHTTPS) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  GURL url1 = https_server_.GetURL("/server-redirect?");
  GURL url2 = https_server_expired_.GetURL("/ssl/google.html");
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(), GURL(url1.spec() + url2.spec()));
  WaitForInterstitial(tab);

  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
}

// Visit a page over http that is a redirect to a page with good HTTPS.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectHTTPToGoodHTTPS) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // HTTP redirects to good HTTPS.
  GURL http_url = embedded_test_server()->GetURL("/server-redirect?");
  GURL good_https_url = https_server_.GetURL("/ssl/google.html");

  ui_test_utils::NavigateToURL(browser(),
                               GURL(http_url.spec() + good_https_url.spec()));
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// Visit a page over http that is a redirect to a page with bad HTTPS.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectHTTPToBadHTTPS) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  const GURL http_url = embedded_test_server()->GetURL("/server-redirect?");
  const GURL bad_https_url = https_server_expired_.GetURL("/ssl/google.html");
  ui_test_utils::NavigateToURL(browser(),
                               GURL(http_url.spec() + bad_https_url.spec()));
  WaitForInterstitial(tab);
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
}

// Visit a page over https that is a redirect to a page with http (to make sure
// we don't keep the secure state).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectHTTPSToHTTP) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  GURL https_url = https_server_.GetURL("/server-redirect?");
  GURL http_url = embedded_test_server()->GetURL("/ssl/google.html");

  ui_test_utils::NavigateToURL(browser(),
                               GURL(https_url.spec() + http_url.spec()));
  ssl_test_util::CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);
}

class SSLUITestWaitForDOMNotification : public SSLUITestIgnoreCertErrors,
                                        public content::NotificationObserver {
 public:
  SSLUITestWaitForDOMNotification()
      : SSLUITestIgnoreCertErrors(), run_loop_(nullptr) {}

  ~SSLUITestWaitForDOMNotification() override { registrar_.RemoveAll(); }

  void SetUpOnMainThread() override {
    SSLUITestIgnoreCertErrors::SetUpOnMainThread();
    registrar_.Add(this, content::NOTIFICATION_DOM_OPERATION_RESPONSE,
                   content::NotificationService::AllSources());
  }

  void set_expected_notification(const std::string& expected_notification) {
    expected_notification_ = expected_notification;
  }

  void set_run_loop(base::RunLoop* run_loop) { run_loop_ = run_loop; }

  // content::NotificationObserver
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK(run_loop_);
    if (type == content::NOTIFICATION_DOM_OPERATION_RESPONSE) {
      content::Details<std::string> dom_op_result(details);
      if (*dom_op_result.ptr() == expected_notification_) {
        run_loop_->Quit();
      }
    }
  }

 private:
  content::NotificationRegistrar registrar_;
  std::string expected_notification_;
  base::RunLoop* run_loop_;

  DISALLOW_COPY_AND_ASSIGN(SSLUITestWaitForDOMNotification);
};

// Tests that a mixed resource which includes HTTP in the redirect chain
// is marked as mixed content, even if the end result is HTTPS.
IN_PROC_BROWSER_TEST_F(SSLUITestWaitForDOMNotification,
                       TestMixedContentWithHTTPInRedirectChain) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/blank_page.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  // Construct a URL which will be dynamically added to the page as an
  // image. The URL redirects through HTTP, though it ends up at an
  // HTTPS resource.
  GURL http_url = embedded_test_server()->GetURL("/server-redirect?");
  GURL::Replacements http_url_replacements;
  // Be sure to use a non-localhost name for the mixed content request,
  // since local hostnames are not considered mixed content.
  http_url_replacements.SetHostStr("example.test");
  std::string http_url_query =
      EncodeQuery(https_server_.GetURL("/ssl/google_files/logo.gif").spec());
  http_url_replacements.SetQueryStr(http_url_query);
  http_url = http_url.ReplaceComponents(http_url_replacements);

  GURL https_url = https_server_.GetURL("/server-redirect?");
  GURL::Replacements https_url_replacements;
  std::string https_url_query = EncodeQuery(http_url.spec());
  https_url_replacements.SetQueryStr(https_url_query);
  https_url = https_url.ReplaceComponents(https_url_replacements);

  base::RunLoop run_loop;

  // Load the image. It starts at |https_server_|, which redirects to an
  // embedded_test_server() HTTP URL, which redirects back to
  // |https_server_| for the final HTTPS image. Because the redirect
  // chain passes through HTTP, the page should be marked as mixed
  // content.
  set_expected_notification("\"mixed-image-loaded\"");
  set_run_loop(&run_loop);
  ASSERT_TRUE(content::ExecuteScript(
      tab,
      "var loaded = function () {"
      "  window.domAutomationController.send('mixed-image-loaded');"
      "};"
      "var img = document.createElement('img');"
      "img.onload = loaded;"
      "img.src = '" +
          https_url.spec() +
          "';"
          "document.body.appendChild(img);"));

  run_loop.Run();
  ssl_test_util::CheckSecurityState(tab, CertError::NONE,
                                    GetPassiveMixedContentSecurityLevel(),
                                    AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Visits a page to which we could not connect (bad port) over http and https
// and make sure the security style is correct.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestConnectToBadPort) {
  ui_test_utils::NavigateToURL(browser(), GURL("http://localhost:17"));
  ssl_test_util::CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(),
      AuthState::SHOWING_ERROR);

  // Same thing over HTTPS.
  ui_test_utils::NavigateToURL(browser(), GURL("https://localhost:17"));
  ssl_test_util::CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(),
      AuthState::SHOWING_ERROR);
}

//
// Frame navigation
//

// From a good HTTPS top frame:
// - navigate to an OK HTTPS frame
// - navigate to a bad HTTPS (expect unsafe content and filtered frame), then
//   back
// - navigate to HTTP (expect insecure content), then back
IN_PROC_BROWSER_TEST_F(SSLUITest, TestGoodFrameNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // SetUpOnMainThread adds this hostname to the resolver so that it's not
  // blocked (browser_test_base.cc has a resolver that blocks all non-local
  // hostnames by default to ensure tests don't hit the network). This is
  // critical to do because the request would otherwise get cancelled in the
  // browser before the renderer sees it.

  std::string top_frame_path = GetTopFramePath(
      *embedded_test_server(), https_server_, https_server_expired_);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL(top_frame_path));

  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  bool success = false;
  // Now navigate inside the frame.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<content::NavigationController>(&tab->GetController()));
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        tab, "window.domAutomationController.send(clickLink('goodHTTPSLink'));",
        &success));
    ASSERT_TRUE(success);
    observer.Wait();
  }

  // We should still be fine.
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  // Now let's hit a bad page.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<content::NavigationController>(&tab->GetController()));
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        tab, "window.domAutomationController.send(clickLink('badHTTPSLink'));",
        &success));
    ASSERT_TRUE(success);
    observer.Wait();
  }

  // The security style should still be secure.
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  // And the frame should be blocked.
  bool is_content_evil = true;
  content::RenderFrameHost* content_frame = content::FrameMatchingPredicate(
      tab, base::Bind(&content::FrameMatchesName, "contentFrame"));
  std::string is_evil_js(
      "window.domAutomationController.send("
      "document.getElementById('evilDiv') != null);");
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(content_frame, is_evil_js,
                                                   &is_content_evil));
  EXPECT_FALSE(is_content_evil);

  // Now go back, our state should still be OK.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<content::NavigationController>(&tab->GetController()));
    tab->GetController().GoBack();
    observer.Wait();
  }
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  // Navigate to a page served over HTTP.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<content::NavigationController>(&tab->GetController()));
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        tab, "window.domAutomationController.send(clickLink('HTTPLink'));",
        &success));
    ASSERT_TRUE(success);
    observer.Wait();
  }

  // Our state should be unauthenticated (in the ran mixed script sense). Note
  // this also displays images from the http page (google.com).
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, CertError::NONE,
      AuthState::RAN_INSECURE_CONTENT | AuthState::DISPLAYED_INSECURE_CONTENT |
          AuthState::DISPLAYED_FORM_WITH_INSECURE_ACTION);

  // Go back, our state should be unchanged.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<content::NavigationController>(&tab->GetController()));
    tab->GetController().GoBack();
    observer.Wait();
  }

  ssl_test_util::CheckAuthenticationBrokenState(
      tab, CertError::NONE,
      AuthState::RAN_INSECURE_CONTENT | AuthState::DISPLAYED_INSECURE_CONTENT |
          AuthState::DISPLAYED_FORM_WITH_INSECURE_ACTION);
}

// From a bad HTTPS top frame:
// - navigate to an OK HTTPS frame (expected to be still authentication broken).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestBadFrameNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string top_frame_path = GetTopFramePath(
      *embedded_test_server(), https_server_, https_server_expired_);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               https_server_expired_.GetURL(top_frame_path));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  // Navigate to a good frame.
  bool success = false;
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(&tab->GetController()));
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      tab, "window.domAutomationController.send(clickLink('goodHTTPSLink'));",
      &success));
  ASSERT_TRUE(success);
  observer.Wait();

  // We should still be authentication broken.
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
}

// From an HTTP top frame, navigate to good and bad HTTPS (security state should
// stay unauthenticated).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestUnauthenticatedFrameNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string top_frame_path = GetTopFramePath(
      *embedded_test_server(), https_server_, https_server_expired_);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(top_frame_path));
  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);

  // Now navigate inside the frame to a secure HTTPS frame.
  {
    bool success = false;
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<content::NavigationController>(&tab->GetController()));
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        tab, "window.domAutomationController.send(clickLink('goodHTTPSLink'));",
        &success));
    ASSERT_TRUE(success);
    observer.Wait();
  }

  // We should still be unauthenticated.
  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);

  // Now navigate to a bad HTTPS frame.
  {
    bool success = false;
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<content::NavigationController>(&tab->GetController()));
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        tab, "window.domAutomationController.send(clickLink('badHTTPSLink'));",
        &success));
    ASSERT_TRUE(success);
    observer.Wait();
  }

  // State should not have changed.
  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);

  // And the frame should have been blocked (see bug #2316).
  bool is_content_evil = true;
  content::RenderFrameHost* content_frame = content::FrameMatchingPredicate(
      tab, base::Bind(&content::FrameMatchesName, "contentFrame"));
  std::string is_evil_js(
      "window.domAutomationController.send("
      "document.getElementById('evilDiv') != null);");
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(content_frame, is_evil_js,
                                                   &is_content_evil));
  EXPECT_FALSE(is_content_evil);
}

enum class OffMainThreadFetchMode { kEnabled, kDisabled };
enum class SSLUIWorkerFetchTestType { kUseFetch, kUseImportScripts };

class SSLUIWorkerFetchTest
    : public testing::WithParamInterface<SSLUIWorkerFetchTestType>,
      public SSLUITestBase {
 public:
  SSLUIWorkerFetchTest() {
    EXPECT_TRUE(tmp_dir_.CreateUniqueTempDir());
  }

  ~SSLUIWorkerFetchTest() override {}

  void SetUpOnMainThread() override {
    SSLUITestBase::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SSLUITestBase::SetUpCommandLine(command_line);
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kMixedContentAutoupgrade);
  }

 protected:
  void WriteFile(const base::FilePath::StringType& filename,
                 base::StringPiece contents) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::WriteFile(tmp_dir_.GetPath().Append(filename), contents));
  }

  void WriteTestFiles(const net::EmbeddedTestServer& remote_server,
                      const std::string& hostname) {
    WriteFile(FILE_PATH_LITERAL("worker_test.html"),
              "<script>"
              "var worker = new Worker('worker.js');"
              "worker.addEventListener("
              "    'message',"
              "    event => { document.title = event.data; });"
              "</script>");
    switch (GetParam()) {
      case SSLUIWorkerFetchTestType::kUseFetch:
        WriteFile(FILE_PATH_LITERAL("worker_test_data.txt.mock-http-headers"),
                  "HTTP/1.1 200 OK\n"
                  "Content-Type: text/plain\n"
                  "Access-Control-Allow-Origin: *");
        WriteFile(FILE_PATH_LITERAL("worker_test_data.txt"), "LOADED");
        WriteFile(FILE_PATH_LITERAL("worker.js"),
                  base::StringPrintf(
                      "fetch('%s')"
                      "  .then(res => res.text())"
                      "  .then(text => postMessage(text))"
                      "  .catch(_ => postMessage('FAILED'))",
                      remote_server.GetURL(hostname, "/worker_test_data.txt")
                          .spec()
                          .c_str()));
        break;
      case SSLUIWorkerFetchTestType::kUseImportScripts: {
        WriteFile(FILE_PATH_LITERAL("imported.js"), "data = 'LOADED';");
        WriteFile(
            FILE_PATH_LITERAL("worker.js"),
            base::StringPrintf(
                "var data = 'FAILED';"
                "try {"
                "  importScripts('%s')"
                "} catch(e) {}"
                "postMessage(data);",
                remote_server.GetURL(hostname, "/imported.js").spec().c_str()));
      } break;
    }
  }

  void RunMixedContentSettingsTest(
      ChromeContentBrowserClientForMixedContentTest* browser_client,
      bool allow_running_insecure_content,
      bool strict_mixed_content_checking,
      bool strictly_block_blockable_mixed_content,
      bool expected_load,
      bool expected_show_blocked,
      bool expected_show_dangerous,
      bool expected_load_after_allow,
      bool expected_show_blocked_after_allow,
      bool expected_show_dangerous_after_allow) {
    SCOPED_TRACE(
        ::testing::Message()
        << "RunMixedContentSettingsTest :"
        << "allow_running_insecure_content="
        << (allow_running_insecure_content ? "true " : "false ")
        << "strict_mixed_content_checking="
        << (strict_mixed_content_checking ? "true " : "false ")
        << "strictly_block_blockable_mixed_content="
        << (strictly_block_blockable_mixed_content ? "true " : "false "));

    // Run the tests in a new tab. This forces each call of
    // RunMixedContentSettingsTest in a single test case to use different tabs
    // and thus different processes, bypassing a subtle race condition where
    // processes can get re-used under Site Isolation and retain their mixed
    // content status (see crbug.com/890372). This ensures all error state is
    // cleared.
    chrome::NewTab(browser());
    CheckErrorStateIsCleared();

    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

    browser_client->SetMixedContentSettings(
        allow_running_insecure_content, strict_mixed_content_checking,
        strictly_block_blockable_mixed_content);
    tab->GetRenderViewHost()->OnWebkitPreferencesChanged();
    CheckMixedContentSettings(allow_running_insecure_content,
                              strict_mixed_content_checking,
                              strictly_block_blockable_mixed_content);

    const base::string16 loaded_title = base::ASCIIToUTF16("LOADED");
    const base::string16 failed_title = base::ASCIIToUTF16("FAILED");

    {
      // First load.
      content::TitleWatcher watcher(tab, loaded_title);
      watcher.AlsoWaitForTitle(failed_title);
      ui_test_utils::NavigateToURL(browser(),
                                   https_server_.GetURL("/worker_test.html"));
      EXPECT_EQ(expected_load ? loaded_title : failed_title,
                watcher.WaitAndGetTitle());
    }

    EXPECT_EQ(expected_show_blocked,
              content_settings::TabSpecificContentSettings::FromWebContents(tab)
                  ->IsContentBlocked(ContentSettingsType::MIXEDSCRIPT));
    ssl_test_util::CheckSecurityState(
        tab, CertError::NONE,
        expected_show_dangerous ? security_state::DANGEROUS
                                : security_state::SECURE,
        expected_show_dangerous ? AuthState::RAN_INSECURE_CONTENT
                                : AuthState::NONE);
    // Clears title.
    ASSERT_TRUE(
        content::ExecuteScript(tab->GetMainFrame(), "document.title = \"\";"));

    {
      // SetAllowRunningInsecureContent will reload the page.
      content::TitleWatcher watcher(tab, loaded_title);
      watcher.AlsoWaitForTitle(failed_title);
      SetAllowRunningInsecureContent();
      tab->GetRenderViewHost()->OnWebkitPreferencesChanged();
      EXPECT_EQ(expected_load_after_allow ? loaded_title : failed_title,
                watcher.WaitAndGetTitle());
    }

    EXPECT_EQ(expected_show_blocked_after_allow,
              content_settings::TabSpecificContentSettings::FromWebContents(tab)
                  ->IsContentBlocked(ContentSettingsType::MIXEDSCRIPT));
    ssl_test_util::CheckSecurityState(
        tab, CertError::NONE,
        expected_show_dangerous_after_allow ? security_state::DANGEROUS
                                            : security_state::SECURE,
        expected_show_dangerous_after_allow ? AuthState::RAN_INSECURE_CONTENT
                                            : AuthState::NONE);

    chrome::CloseTab(browser());
  }

  base::ScopedTempDir tmp_dir_;

 private:
  void SetAllowRunningInsecureContent() {
    content::RenderFrameHost* render_frame_host =
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
    mojo::AssociatedRemote<content_settings::mojom::ContentSettingsAgent> agent;
    render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&agent);
    agent->SetAllowRunningInsecureContent();
  }

  void CheckErrorStateIsCleared() {
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_FALSE(
        content_settings::TabSpecificContentSettings::FromWebContents(tab)
            ->IsContentBlocked(ContentSettingsType::MIXEDSCRIPT));
    ssl_test_util::CheckSecurityState(tab, CertError::NONE,
                                      security_state::NONE, AuthState::NONE);
    EXPECT_FALSE(SecurityStateTabHelper::FromWebContents(tab)
                     ->GetVisibleSecurityState()
                     ->ran_mixed_content);
  }

  void CheckMixedContentSettings(bool allow_running_insecure_content,
                                 bool strict_mixed_content_checking,
                                 bool strictly_block_blockable_mixed_content) {
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    const content::WebPreferences& prefs =
        tab->GetMainFrame()->GetRenderViewHost()->GetWebkitPreferences();
    ASSERT_EQ(prefs.strictly_block_blockable_mixed_content,
              strictly_block_blockable_mixed_content);
    ASSERT_EQ(prefs.allow_running_insecure_content,
              allow_running_insecure_content);
    ASSERT_EQ(prefs.strict_mixed_content_checking,
              strict_mixed_content_checking);
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SSLUIWorkerFetchTest);
};

IN_PROC_BROWSER_TEST_P(SSLUIWorkerFetchTest,
                       TestUnsafeContentsInWorkerFiltered) {
  https_server_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  https_server_expired_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());
  WriteTestFiles(https_server_expired_, "localhost");

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  const base::string16 loaded_title = base::ASCIIToUTF16("LOADED");
  const base::string16 failed_title = base::ASCIIToUTF16("FAILED");
  content::TitleWatcher watcher(tab, loaded_title);
  watcher.AlsoWaitForTitle(failed_title);

  // This page will spawn a Worker which will try to load content from
  // BadCertServer.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/worker_test.html"));
  // Expect Worker not to load insecure content.
  EXPECT_EQ(failed_title, watcher.WaitAndGetTitle());
  // The bad content is filtered, expect the state to be authenticated.
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// This test, and the related test TestUnsafeContentsWithUserException, verify
// that if unsafe content is loaded but the host of that unsafe content has a
// user exception, the content runs and the security style is downgraded.
IN_PROC_BROWSER_TEST_P(SSLUIWorkerFetchTest,
                       TestUnsafeContentsInWorkerWithUserException) {
  https_server_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  https_server_mismatched_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  ASSERT_TRUE(https_server_.Start());
  // Note that it is necessary to user https_server_mismatched_ here over the
  // other invalid cert servers. This is because the test relies on the two
  // servers having different hosts since SSL exceptions are per-host, not per
  // origin, and https_server_mismatched_ uses 'localhost' rather than
  // '127.0.0.1'.
  ASSERT_TRUE(https_server_mismatched_.Start());

  WriteTestFiles(https_server_mismatched_, "localhost");

  // Navigate to an unsafe site. Proceed with interstitial page to indicate
  // the user approves the bad certificate.
  ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/blank_page.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_COMMON_NAME_INVALID,
      AuthState::SHOWING_INTERSTITIAL);
  content::TestNavigationObserver nav_observer(tab, 1);
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
      ->CommandReceived(
          base::NumberToString(security_interstitials::CMD_PROCEED));
  nav_observer.Wait();

  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_COMMON_NAME_INVALID, AuthState::NONE);

  SecurityStateTabHelper* tab_helper =
      SecurityStateTabHelper::FromWebContents(tab);
  ASSERT_TRUE(tab_helper);
  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      tab_helper->GetVisibleSecurityState();
  EXPECT_FALSE(visible_security_state->ran_mixed_content);
  EXPECT_FALSE(visible_security_state->displayed_mixed_content);
  EXPECT_FALSE(visible_security_state->ran_content_with_cert_errors);
  EXPECT_FALSE(visible_security_state->displayed_content_with_cert_errors);

  const base::string16 loaded_title = base::ASCIIToUTF16("LOADED");
  const base::string16 failed_title = base::ASCIIToUTF16("FAILED");
  content::TitleWatcher watcher(tab, loaded_title);
  watcher.AlsoWaitForTitle(failed_title);

  // Navigate to safe page that has Worker loading unsafe content.
  // Expect content to load but be marked as auth broken due to running insecure
  // content.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/worker_test.html"));
  // Worker loads insecure content
  EXPECT_EQ(loaded_title, watcher.WaitAndGetTitle());
  ssl_test_util::CheckAuthenticationBrokenState(tab, CertError::NONE,
                                                AuthState::NONE);

  visible_security_state = tab_helper->GetVisibleSecurityState();
  EXPECT_FALSE(visible_security_state->ran_mixed_content);
  EXPECT_FALSE(visible_security_state->displayed_mixed_content);
  EXPECT_TRUE(visible_security_state->ran_content_with_cert_errors);
  EXPECT_FALSE(visible_security_state->displayed_content_with_cert_errors);
}

// This test checks the behavior of mixed content blocking for the requests
// from a dedicated worker by changing the settings in WebPreferences
// with allow_running_insecure_content = true.
IN_PROC_BROWSER_TEST_P(SSLUIWorkerFetchTest,
                       MixedContentSettings_AllowRunningInsecureContent) {
  ChromeContentBrowserClientForMixedContentTest browser_client;
  content::ContentBrowserClient* old_browser_client =
      content::SetBrowserClientForTesting(&browser_client);

  https_server_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  embedded_test_server()->ServeFilesFromDirectory(tmp_dir_.GetPath());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  WriteTestFiles(*embedded_test_server(), "example.com");
  for (bool strict_mixed_content_checking : {true, false}) {
    for (bool strictly_block_blockable_mixed_content : {true, false}) {
      if (strict_mixed_content_checking) {
        RunMixedContentSettingsTest(
            &browser_client, true /* allow_running_insecure_content */,
            strict_mixed_content_checking,
            strictly_block_blockable_mixed_content, false /* expected_load */,
            false /* expected_show_blocked */,
            false /* expected_show_dangerous */,
            false /* expected_load_after_allow */,
            false /* expected_show_blocked_after_allow */,
            false /* expected_show_dangerous_after_allow */);
      } else {
        RunMixedContentSettingsTest(
            &browser_client, true /* allow_running_insecure_content */,
            strict_mixed_content_checking,
            strictly_block_blockable_mixed_content, true /* expected_load */,
            false /* expected_show_blocked */,
            true /* expected_show_dangerous */,
            true /* expected_load_after_allow */,
            false /* expected_show_blocked_after_allow */,
            true /* expected_show_dangerous_after_allow */);
      }
    }
  }

  content::SetBrowserClientForTesting(old_browser_client);
}

// This test checks the behavior of mixed content blocking for the requests
// from a dedicated worker by changing the settings in WebPreferences
// with allow_running_insecure_content = false.
IN_PROC_BROWSER_TEST_P(SSLUIWorkerFetchTest,
                       MixedContentSettings_DisallowRunningInsecureContent) {
  ChromeContentBrowserClientForMixedContentTest browser_client;
  content::ContentBrowserClient* old_browser_client =
      content::SetBrowserClientForTesting(&browser_client);

  https_server_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  embedded_test_server()->ServeFilesFromDirectory(tmp_dir_.GetPath());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  WriteTestFiles(*embedded_test_server(), "example.com");
  for (bool strict_mixed_content_checking : {true, false}) {
    for (bool strictly_block_blockable_mixed_content : {true, false}) {
      if (strict_mixed_content_checking) {
        RunMixedContentSettingsTest(
            &browser_client, false /* allow_running_insecure_content */,
            strict_mixed_content_checking,
            strictly_block_blockable_mixed_content, false /* expected_load */,
            false /* expected_show_blocked */,
            false /* expected_show_dangerous */,
            false /* expected_load_after_allow */,
            false /* expected_show_blocked_after_allow */,
            false /* expected_show_dangerous_after_allow */);
      } else if (strictly_block_blockable_mixed_content) {
        RunMixedContentSettingsTest(
            &browser_client, false /* allow_running_insecure_content */,
            strict_mixed_content_checking,
            strictly_block_blockable_mixed_content, false /* expected_load */,
            false /* expected_show_blocked */,
            false /* expected_show_dangerous */,
            false /* expected_load_after_allow */,
            false /* expected_show_blocked_after_allow */,
            false /* expected_show_dangerous_after_allow */);
      } else {
        RunMixedContentSettingsTest(
            &browser_client, false /* allow_running_insecure_content */,
            strict_mixed_content_checking,
            strictly_block_blockable_mixed_content, false /* expected_load */,
            true /* expected_show_blocked */,
            false /* expected_show_dangerous */,
            true /* expected_load_after_allow */,
            false /* expected_show_blocked_after_allow */,
            true /* expected_show_dangerous_after_allow */);
      }
    }
  }

  content::SetBrowserClientForTesting(old_browser_client);
}

// This test checks that all mixed content requests from a dedicated worker are
// blocked regardless of the settings in WebPreferences when
// block-all-mixed-content CSP is set with allow_running_insecure_content=true.
IN_PROC_BROWSER_TEST_P(
    SSLUIWorkerFetchTest,
    MixedContentSettingsWithBlockingCSP_AllowRunningInsecureContent) {
  ChromeContentBrowserClientForMixedContentTest browser_client;
  content::ContentBrowserClient* old_browser_client =
      content::SetBrowserClientForTesting(&browser_client);

  https_server_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  embedded_test_server()->ServeFilesFromDirectory(tmp_dir_.GetPath());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  WriteTestFiles(*embedded_test_server(), "example.com");
  WriteFile(FILE_PATH_LITERAL("worker_test.html.mock-http-headers"),
            "HTTP/1.1 200 OK\n"
            "Content-Type: text/html\n"
            "Content-Security-Policy: block-all-mixed-content;");
  for (bool strict_mixed_content_checking : {true, false}) {
    for (bool strictly_block_blockable_mixed_content : {true, false}) {
      RunMixedContentSettingsTest(
          &browser_client, true /* allow_running_insecure_content */,
          strict_mixed_content_checking, strictly_block_blockable_mixed_content,
          false /* expected_load */, false /* expected_show_blocked */,
          false /* expected_show_dangerous */,
          false /* expected_load_after_allow */,
          false /* expected_show_blocked_after_allow */,
          false /* expected_show_dangerous_after_allow */);
    }
  }
  content::SetBrowserClientForTesting(old_browser_client);
}

// This test checks that all mixed content requests from a dedicated worker are
// blocked regardless of the settings in WebPreferences when
// block-all-mixed-content CSP is set with allow_running_insecure_content=false.
IN_PROC_BROWSER_TEST_P(
    SSLUIWorkerFetchTest,
    MixedContentSettingsWithBlockingCSP_DisallowRunningInsecureContent) {
  ChromeContentBrowserClientForMixedContentTest browser_client;
  content::ContentBrowserClient* old_browser_client =
      content::SetBrowserClientForTesting(&browser_client);

  https_server_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  embedded_test_server()->ServeFilesFromDirectory(tmp_dir_.GetPath());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  WriteTestFiles(*embedded_test_server(), "example.com");
  WriteFile(FILE_PATH_LITERAL("worker_test.html.mock-http-headers"),
            "HTTP/1.1 200 OK\n"
            "Content-Type: text/html\n"
            "Content-Security-Policy: block-all-mixed-content;");
  for (bool strict_mixed_content_checking : {true, false}) {
    for (bool strictly_block_blockable_mixed_content : {true, false}) {
      RunMixedContentSettingsTest(
          &browser_client, false /* allow_running_insecure_content */,
          strict_mixed_content_checking, strictly_block_blockable_mixed_content,
          false /* expected_load */, false /* expected_show_blocked */,
          false /* expected_show_dangerous */,
          false /* expected_load_after_allow */,
          false /* expected_show_blocked_after_allow */,
          false /* expected_show_dangerous_after_allow */);
    }
  }
  content::SetBrowserClientForTesting(old_browser_client);
}

// This test checks that all mixed content requests from a dedicated worker
// which is started from a subframe are blocked if
// allow_running_insecure_content setting is false or
// strict_mixed_content_checking setting is true.
// TODO(carlosil): Re-enable to check if this triggers flakiness due to
// committed interstitials.
IN_PROC_BROWSER_TEST_P(SSLUIWorkerFetchTest, DISABLED_MixedContentSubFrame) {
  ChromeContentBrowserClientForMixedContentTest browser_client;
  content::ContentBrowserClient* old_browser_client =
      content::SetBrowserClientForTesting(&browser_client);

  https_server_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  embedded_test_server()->ServeFilesFromDirectory(tmp_dir_.GetPath());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  WriteTestFiles(*embedded_test_server(), "example.com");
  WriteFile(FILE_PATH_LITERAL("worker_iframe.html"),
            "<script>"
            "var worker = new Worker('worker.js');"
            "worker.addEventListener("
            "    'message',"
            "    event => { parent.postMessage(event.data, '*'); });"
            "</script>");
  WriteFile(FILE_PATH_LITERAL("worker_test.html"),
            "<script>"
            "window.addEventListener("
            "    'message',"
            "    event => { document.title = event.data; });"
            "</script>"
            "<iframe src=\"./worker_iframe.html\" />");

  for (bool allow_running_insecure_content : {true, false}) {
    for (bool strict_mixed_content_checking : {true, false}) {
      for (bool strictly_block_blockable_mixed_content : {true, false}) {
        if (allow_running_insecure_content && !strict_mixed_content_checking) {
          RunMixedContentSettingsTest(
              &browser_client, allow_running_insecure_content,
              strict_mixed_content_checking,
              strictly_block_blockable_mixed_content, true /* expected_load */,
              false /* expected_show_blocked */,
              true /* expected_show_dangerous */,
              true /* expected_load_after_allow */,
              false /* expected_show_blocked_after_allow */,
              true /* expected_show_dangerous_after_allow */);
        } else {
          RunMixedContentSettingsTest(
              &browser_client, allow_running_insecure_content,
              strict_mixed_content_checking,
              strictly_block_blockable_mixed_content, false /* expected_load */,
              false /* expected_show_blocked */,
              false /* expected_show_dangerous */,
              false /* expected_load_after_allow */,
              false /* expected_show_blocked_after_allow */,
              false /* expected_show_dangerous_after_allow */);
        }
      }
    }
  }

  content::SetBrowserClientForTesting(old_browser_client);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    SSLUIWorkerFetchTest,
    ::testing::Values(SSLUIWorkerFetchTestType::kUseFetch,
                      SSLUIWorkerFetchTestType::kUseImportScripts));

// Visits a page with unsafe content and makes sure that if a user exception
// to the certificate error is present, the image is loaded and script
// executes.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestUnsafeContentsWithUserException) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NO_FATAL_FAILURE(SetUpUnsafeContentsWithUserException(
      "/ssl/page_with_unsafe_contents.html"));
  ssl_test_util::CheckAuthenticationBrokenState(tab, CertError::NONE,
                                                AuthState::NONE);

  SecurityStateTabHelper* helper = SecurityStateTabHelper::FromWebContents(tab);
  ASSERT_TRUE(helper);
  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      helper->GetVisibleSecurityState();
  EXPECT_FALSE(visible_security_state->ran_mixed_content);
  EXPECT_FALSE(visible_security_state->displayed_mixed_content);
  EXPECT_TRUE(visible_security_state->ran_content_with_cert_errors);
  EXPECT_TRUE(visible_security_state->displayed_content_with_cert_errors);

  int img_width;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      tab, "window.domAutomationController.send(ImageWidth());", &img_width));
  // In order to check that the image was loaded, we check its width.
  // The actual image (Google logo) is 114 pixels wide, so we assume a good
  // image is greater than 100.
  EXPECT_GT(img_width, 100);

  bool js_result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      tab, "window.domAutomationController.send(IsFooSet());", &js_result));
  EXPECT_TRUE(js_result);

  // Test that active subresources with the same certificate errors as
  // the main resources also get noted in |content_with_cert_errors_status|.
  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_unsafe_contents.html",
      https_server_mismatched_.host_port_pair());
  ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL(replacement_path));
  js_result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      tab, "window.domAutomationController.send(IsFooSet());", &js_result));
  EXPECT_TRUE(js_result);
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_COMMON_NAME_INVALID, AuthState::NONE);

  visible_security_state = helper->GetVisibleSecurityState();
  EXPECT_FALSE(visible_security_state->ran_mixed_content);
  EXPECT_FALSE(visible_security_state->displayed_mixed_content);
  EXPECT_TRUE(visible_security_state->ran_content_with_cert_errors);
  EXPECT_TRUE(visible_security_state->displayed_content_with_cert_errors);
}

// Like the test above, but only displaying inactive content (an image).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestUnsafeImageWithUserException) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NO_FATAL_FAILURE(
      SetUpUnsafeContentsWithUserException("/ssl/page_with_unsafe_image.html"));

  SecurityStateTabHelper* helper = SecurityStateTabHelper::FromWebContents(tab);
  ASSERT_TRUE(helper);
  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      helper->GetVisibleSecurityState();
  EXPECT_FALSE(visible_security_state->ran_mixed_content);
  EXPECT_FALSE(visible_security_state->displayed_mixed_content);
  EXPECT_FALSE(visible_security_state->ran_content_with_cert_errors);
  EXPECT_TRUE(visible_security_state->displayed_content_with_cert_errors);
  EXPECT_EQ(0u, visible_security_state->cert_status);

  int img_width;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      tab, "window.domAutomationController.send(ImageWidth());", &img_width));
  // In order to check that the image was loaded, we check its width.
  // The actual image (Google logo) is 114 pixels wide, so we assume a good
  // image is greater than 100.
  EXPECT_GT(img_width, 100);
}

// Test that when the browser blocks displaying insecure content (iframes),
// the indicator shows a secure page, because the blocking made the otherwise
// unsafe page safe (the notification of this state is handled by other means)
IN_PROC_BROWSER_TEST_F(SSLUITestBlock, TestBlockDisplayingInsecureIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_iframe.html",
      embedded_test_server()->host_port_pair());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));

  ssl_test_util::CheckAuthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);
}

// Test that when the browser blocks running insecure content, the
// indicator shows a secure page, because the blocking made the otherwise
// unsafe page safe (the notification of this state is handled by other
// means).
IN_PROC_BROWSER_TEST_F(SSLUITestBlock, TestBlockRunningInsecureContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content.html",
      embedded_test_server()->host_port_pair());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));

  ssl_test_util::CheckAuthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);
}

// Visit a page and establish a WebSocket connection over bad https with
// --ignore-certificate-errors. The connection should be established without
// interstitial page showing.
IN_PROC_BROWSER_TEST_F(SSLUITestIgnoreCertErrors, TestWSS) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(wss_server_expired_.Start());

  // Setup page title observer.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher watcher(tab, base::ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(base::ASCIIToUTF16("FAIL"));

  // Visit bad HTTPS page.
  GURL::Replacements replacements;
  replacements.SetSchemeStr("https");
  ui_test_utils::NavigateToURL(browser(),
                               wss_server_expired_.GetURL("connect_check.html")
                                   .ReplaceComponents(replacements));

  // We shouldn't have an interstitial page showing here.

  // Test page run a WebSocket wss connection test. The result will be shown
  // as page title.
  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(base::LowerCaseEqualsASCII(result, "pass"));
}

// Visit a page and establish a WebSocket connection over bad https with
// --ignore-certificate-errors-spki-list. The connection should be established
// without interstitial page showing.
#if !defined(OS_CHROMEOS)  // Chrome OS does not support the flag.
IN_PROC_BROWSER_TEST_F(SSLUITestIgnoreCertErrorsBySPKIWSS, TestWSSExpired) {
  ASSERT_TRUE(wss_server_expired_.Start());

  // Setup page title observer.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher watcher(tab, base::ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(base::ASCIIToUTF16("FAIL"));

  // Visit bad HTTPS page.
  GURL::Replacements replacements;
  replacements.SetSchemeStr("https");
  ui_test_utils::NavigateToURL(browser(),
                               wss_server_expired_.GetURL("connect_check.html")
                                   .ReplaceComponents(replacements));

  // We shouldn't have an interstitial page showing here.

  // Test page run a WebSocket wss connection test. The result will be shown
  // as page title.
  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(base::LowerCaseEqualsASCII(result, "pass"));
}
#endif  // !defined(OS_CHROMEOS)

// Test that HTTPS pages with a bad certificate don't show an interstitial if
// the public key matches a value from --ignore-certificate-errors-spki-list.
#if !defined(OS_CHROMEOS)  // Chrome OS does not support the flag.
IN_PROC_BROWSER_TEST_F(SSLUITestIgnoreCertErrorsBySPKIHTTPS, TestHTTPS) {
  ASSERT_TRUE(https_server_mismatched_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  ui_test_utils::NavigateToURL(
      browser(),
      https_server_mismatched_.GetURL("/ssl/page_with_subresource.html"));

  // We should see no interstitial. The script tag in the page should have
  // loaded and ran (and wasn't blocked by the certificate error).
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
  base::string16 title;
  ui_test_utils::GetCurrentTabTitle(browser(), &title);
  EXPECT_EQ(title, base::ASCIIToUTF16("This script has loaded"));
}
#endif  // !defined(OS_CHROMEOS)

// Test subresources from an origin with a bad certificate are loaded if the
// public key matches a value from --ignore-certificate-errors-spki-list.
#if !defined(OS_CHROMEOS)  // Chrome OS does not support the flag.
IN_PROC_BROWSER_TEST_F(SSLUITestIgnoreCertErrorsBySPKIHTTPS,
                       TestInsecureSubresource) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_mismatched_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_unsafe_image.html",
      https_server_mismatched_.host_port_pair());
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));

  // We should see no interstitial.
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
  // In order to check that the image was loaded, check its width.
  // The actual image (Google logo) is 276 pixels wide.
  int img_width = 0;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      tab, "window.domAutomationController.send(ImageWidth());", &img_width));
  EXPECT_GT(img_width, 200);
}
#endif  // !defined(OS_CHROMEOS)

// Verifies that the interstitial can proceed, even if JavaScript is disabled.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestInterstitialJavaScriptProceeds) {
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT,
                                 CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  WaitForInterstitial(tab);
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(&tab->GetController()));
  const std::string javascript =
      "window.certificateErrorPageController.proceed();";
  EXPECT_TRUE(content::ExecuteScript(tab, javascript));
  observer.Wait();
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
}

// Verifies that the interstitial can go back, even if JavaScript is disabled.
// http://crbug.com/322948
IN_PROC_BROWSER_TEST_F(SSLUITest, TestInterstitialJavaScriptGoesBack) {
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT,
                                 CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);
  WaitForInterstitial(tab);
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(&tab->GetController()));
  const std::string javascript =
      "window.certificateErrorPageController.dontProceed();";
  EXPECT_TRUE(content::ExecuteScript(tab, javascript));
  observer.Wait();
  EXPECT_EQ("about:blank", tab->GetVisibleURL().spec());
}

// Verifies that an overridable interstitial has a proceed link.
IN_PROC_BROWSER_TEST_F(SSLUITest, ProceedLinkOverridable) {
  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));
  WaitForInterstitial(tab);
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));

  EXPECT_TRUE(chrome_browser_interstitials::InterstitialHasProceedLink(
      tab->GetMainFrame()));
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestLearnMoreLinkContainsErrorCode) {
  ASSERT_TRUE(https_server_expired_.Start());

  // Navigate to a site that causes an interstitial.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_expired_.GetURL("/title1.html"));
  WaitForInterstitial(browser()->tab_strip_model()->GetActiveWebContents());

  // Simulate clicking the learn more link.
  SendInterstitialCommand(browser()->tab_strip_model()->GetActiveWebContents(),
                          security_interstitials::CMD_OPEN_HELP_CENTER);
  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL()
                .ref(),
            base::NumberToString(net::ERR_CERT_DATE_INVALID));
}

// Checks that interstitials are not used for subframe SSL errors. Regression
// test for https://crbug.com/808797.
IN_PROC_BROWSER_TEST_F(SSLUITest, SubframeCertError) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));

  // Insert a broken-HTTPS iframe on the page and check that a generic net
  // error, not a certificate error page, is shown.
  content::TestNavigationObserver observer(tab, 1);
  std::string insert_frame = base::StringPrintf(
      "var i = document.createElement('iframe');"
      "i.src = '%s';"
      "document.body.appendChild(i);",
      https_server_expired_.GetURL("/ssl/google.html").spec().c_str());
  EXPECT_TRUE(content::ExecuteScript(tab, insert_frame));
  observer.Wait();

  content::RenderFrameHost* child =
      content::ChildFrameAt(tab->GetMainFrame(), 0);
  ASSERT_TRUE(child);
  int result = security_interstitials::CMD_ERROR;
  const std::string javascript = base::StringPrintf(
      "domAutomationController.send("
      "(document.querySelector(\"#proceed-link\") === null) "
      "? (%d) : (%d))",
      security_interstitials::CMD_TEXT_NOT_FOUND,
      security_interstitials::CMD_TEXT_FOUND);
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(child, javascript, &result));
  EXPECT_EQ(security_interstitials::CMD_TEXT_NOT_FOUND, result);
}

// Verifies that a non-overridable interstitial does not have a proceed link.
IN_PROC_BROWSER_TEST_F(SSLUITestHSTS, TestInterstitialOptionsNonOverridable) {
  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL::Replacements replacements;
  replacements.SetHostStr(kHstsTestHostName);
  GURL url = https_server_expired_.GetURL("/ssl/google.html")
                 .ReplaceComponents(replacements);

  ui_test_utils::NavigateToURL(browser(), url);
  WaitForInterstitial(tab);
  // Since we are connecting to a different domain than the test server default,
  // we also expect CERT_STATUS_COMMON_NAME_INVALID.
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID | net::CERT_STATUS_COMMON_NAME_INVALID,
      AuthState::SHOWING_INTERSTITIAL);

  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));

  int result = security_interstitials::CMD_ERROR;
  const std::string javascript = base::StringPrintf(
      "domAutomationController.send("
      "(document.querySelector(\"#proceed-link\") === null) "
      "? (%d) : (%d))",
      security_interstitials::CMD_TEXT_NOT_FOUND,
      security_interstitials::CMD_TEXT_FOUND);
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(tab->GetMainFrame(),
                                                  javascript, &result));
  EXPECT_EQ(security_interstitials::CMD_TEXT_NOT_FOUND, result);
}

// Verifies that links in the interstitial open in a new tab.
// https://crbug.com/717616
IN_PROC_BROWSER_TEST_F(SSLUITest, TestInterstitialLinksOpenInNewTab) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  WebContents* interstitial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));
  WaitForInterstitial(browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingSSLInterstitial(interstitial_tab));
  ssl_test_util::CheckAuthenticationBrokenState(
      interstitial_tab, net::CERT_STATUS_DATE_INVALID,
      AuthState::SHOWING_INTERSTITIAL);

  content::TestNavigationObserver nav_observer(nullptr);
  nav_observer.StartWatchingNewWebContents();

  SSLBlockingPage* ssl_interstitial =
      static_cast<SSLBlockingPage*>(GetInterstitialPage(interstitial_tab));
  security_interstitials::SecurityInterstitialControllerClient* client =
      GetControllerClientFromSSLBlockingPage(ssl_interstitial);

  // Mock out the help center URL so that our test will hit the test server
  // instead of a real server.
  // NOTE: The CMD_OPEN_HELP_CENTER code in
  // components/security_interstitials/core/ssl_error_ui.cc ends up appending
  // a path to whatever URL is passed to it. Since that path doesn't exist on
  // our test server, this results in a 404. This is expected behavior, and
  // things are still working as expected so long as the test passes!
  const GURL mock_help_center_url = https_server_.GetURL("/title1.html");
  client->SetBaseHelpCenterUrlForTesting(mock_help_center_url);

  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  SendInterstitialCommand(interstitial_tab,
                          security_interstitials::CMD_OPEN_HELP_CENTER);

  nav_observer.Wait();

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  WebContents* new_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(mock_help_center_url.host(), new_tab->GetURL().host());
}

// Verifies that switching tabs, while showing interstitial page, will not
// affect the visibility of the interstitial.
// https://crbug.com/381439
IN_PROC_BROWSER_TEST_F(SSLUITest, InterstitialNotAffectedByHideShow) {
  ASSERT_TRUE(https_server_expired_.Start());
  ASSERT_TRUE(https_server_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);
  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());

  AddTabAtIndex(0, https_server_.GetURL("/ssl/google.html"),
                ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(tab, browser()->tab_strip_model()->GetWebContentsAt(1));
  EXPECT_FALSE(tab->GetRenderWidgetHostView()->IsShowing());

  browser()->tab_strip_model()->ActivateTabAt(
      1, {TabStripModel::GestureType::kOther});
  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
}

// Verifies that if a bad certificate is seen for a host and the user proceeds
// through the interstitial, the decision to proceed is initially remembered.
// However, if this is followed by another visit, and a good certificate
// is seen for the same host, the original exception is forgotten.
IN_PROC_BROWSER_TEST_F(SSLUITest, BadCertFollowedByGoodCert) {
  // It is necessary to use |https_server_expired_| rather than
  // |https_server_mismatched| because the former shares a host with
  // |https_server_| and cert exceptions are per host.
  ASSERT_TRUE(https_server_expired_.Start());
  ASSERT_TRUE(https_server_.Start());

  std::string https_server_expired_host =
      https_server_expired_.GetURL("/ssl/google.html").host();
  std::string https_server_host =
      https_server_.GetURL("/ssl/google.html").host();
  ASSERT_EQ(https_server_expired_host, https_server_host);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  StatefulSSLHostStateDelegate* state =
      reinterpret_cast<StatefulSSLHostStateDelegate*>(
          profile->GetSSLHostStateDelegate());

  // First check that frame requests revoke the decision.
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));

  ProceedThroughInterstitial(tab);
  EXPECT_TRUE(state->HasAllowException(https_server_host, tab));

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  EXPECT_FALSE(state->HasAllowException(https_server_host, tab));

  // Now check that subresource requests revoke the decision.
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));

  ProceedThroughInterstitial(tab);
  EXPECT_TRUE(state->HasAllowException(https_server_host, tab));

  GURL image = https_server_.GetURL("/ssl/google_files/logo.gif");
  bool result = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      tab,
      std::string("var img = document.createElement('img');img.src ='") +
          image.spec() +
          "';img.onload=function() { "
          "window.domAutomationController.send(true); };"
          "document.body.appendChild(img);",
      &result));
  EXPECT_TRUE(result);
  EXPECT_FALSE(state->HasAllowException(https_server_host, tab));
}

// Verifies that if a bad certificate is seen for a host and the user proceeds
// through the interstitial, the decision to proceed is not forgotten once blob
// URLs are loaded (blob loads never have certificate errors).  This is a
// regression test for https://crbug.com/1049625.
IN_PROC_BROWSER_TEST_F(SSLUITest, BadCertFollowedByBlobUrl) {
  ASSERT_TRUE(https_server_expired_.Start());
  std::string https_server_host =
      https_server_expired_.GetURL("/ssl/google.html").host();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  StatefulSSLHostStateDelegate* state =
      reinterpret_cast<StatefulSSLHostStateDelegate*>(
          profile->GetSSLHostStateDelegate());

  // Proceed through the interstitial, accepting the broken cert.
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));
  ProceedThroughInterstitial(tab);
  ASSERT_TRUE(state->HasAllowException(https_server_host, tab));

  // Load a blob URL.
  content::WebContentsConsoleObserver console_observer(tab);
  console_observer.SetPattern("hello from blob");
  const char kScript[] = R"(
      new Promise(function (resolvePromise, rejectPromise) {
          var blob = new Blob(['console.log("hello from blob")'],
                              {type : 'application/javascript'});
          script = document.createElement('script');
          script.onerror = rejectPromise;
          script.onload = () => resolvePromise('success');
          script.src = URL.createObjectURL(blob);
          document.body.appendChild(script);
      });
  )";
  ASSERT_EQ("success", content::EvalJs(tab, kScript));

  // Verify that the script from the blob has successfully run.
  console_observer.Wait();

  // Verify that the decision to accept the broken cert has not been revoked
  // (this is a regression test for https://crbug.com/1049625).
  EXPECT_TRUE(state->HasAllowException(https_server_host, tab));
}

// Tests that the SSLStatus of a navigation entry for an SSL
// interstitial matches the navigation entry once the interstitial is
// clicked through. https://crbug.com/529456
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       SSLStatusMatchesOnInterstitialAndAfterProceed) {
  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));
  WaitForInterstitial(tab);
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));

  content::NavigationEntry* entry = tab->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  content::SSLStatus interstitial_ssl_status = entry->GetSSL();

  ProceedThroughInterstitial(tab);
  EXPECT_FALSE(tab->ShowingInterstitialPage());
  entry = tab->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);

  content::SSLStatus after_interstitial_ssl_status = entry->GetSSL();
  EXPECT_TRUE(ComparePreAndPostInterstitialSSLStatuses(
      interstitial_ssl_status, after_interstitial_ssl_status));
}

// As above, but for a bad clock interstitial. Tests that a clock
// interstitial's SSLStatus matches the SSLStatus of the HTTPS page
// after proceeding through a normal SSL interstitial.
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       SSLStatusMatchesonClockInterstitialAndAfterProceed) {
  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  // Set up the build and current clock times to be more than a year apart.
  base::SimpleTestClock mock_clock;
  mock_clock.SetNow(base::Time::NowFromSystemTime());
  mock_clock.Advance(base::TimeDelta::FromDays(367));
  SSLErrorHandler::SetClockForTesting(&mock_clock);
  ssl_errors::SetBuildTimeForTesting(base::Time::NowFromSystemTime());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_expired_.GetURL("/title1.html"));
  WaitForInterstitial(tab);
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingBadClockInterstitial(tab));

  // Grab the SSLStatus on the clock interstitial.
  content::NavigationEntry* entry = tab->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  content::SSLStatus clock_interstitial_ssl_status = entry->GetSSL();

  // Put the clock back to normal, trigger a normal SSL interstitial,
  // and proceed through it.
  mock_clock.SetNow(base::Time::NowFromSystemTime());
  ui_test_utils::NavigateToURL(browser(),
                               https_server_expired_.GetURL("/title1.html"));
  WaitForInterstitial(tab);
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
  ProceedThroughInterstitial(tab);
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(tab));

  // Grab the SSLStatus from the page and check that it is the same as
  // on the clock interstitial.
  entry = tab->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  content::SSLStatus after_interstitial_ssl_status = entry->GetSSL();
  EXPECT_TRUE(ComparePreAndPostInterstitialSSLStatuses(
      clock_interstitial_ssl_status, after_interstitial_ssl_status));
}

// A fixture for testing on-demand network time queries on SSL
// certificate date errors. It can simulate a delayed network time
// request, and it allows the user to configure the experimental
// parameters of the NetworkTimeTracker. Expects only one network time
// request to be issued during the test.
class SSLNetworkTimeBrowserTest : public SSLUITest {
 public:
  SSLNetworkTimeBrowserTest() : SSLUITest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        network_time::kNetworkTimeServiceQuerying,
        {{"FetchBehavior", "on-demand-only"}});
  }

  ~SSLNetworkTimeBrowserTest() override = default;

  void SetUpOnMainThread() override {
    SSLUITest::SetUpOnMainThread();
    controllable_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/", true);
    ASSERT_TRUE(embedded_test_server()->Start());
    g_browser_process->network_time_tracker()->SetTimeServerURLForTesting(
        embedded_test_server()->GetURL("/"));
  }

 protected:
  void TriggerTimeResponse() {
    std::string response = "HTTP/1.1 200 OK\nContent-type: text/plain\n";
    response += base::StringPrintf(
        "Content-Length: %1d\n",
        static_cast<int>(strlen(network_time::kGoodTimeResponseBody[0])));
    response +=
        "x-cup-server-proof: " +
        std::string(network_time::kGoodTimeResponseServerProofHeader[0]);
    response += "\n\n";
    response += std::string(network_time::kGoodTimeResponseBody[0]);
    controllable_response_->WaitForRequest();
    controllable_response_->Send(response);
  }

  // Asserts that the first time request to the server is currently pending.
  void CheckTimeQueryPending() {
    base::Time unused_time;
    base::TimeDelta unused_uncertainty;
    ASSERT_EQ(network_time::NetworkTimeTracker::NETWORK_TIME_FIRST_SYNC_PENDING,
              g_browser_process->network_time_tracker()->GetNetworkTime(
                  &unused_time, &unused_uncertainty));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      controllable_response_;

  DISALLOW_COPY_AND_ASSIGN(SSLNetworkTimeBrowserTest);
};

// Tests that if an on-demand network time fetch returns that the clock
// is okay, a normal SSL interstitial is shown.
IN_PROC_BROWSER_TEST_F(SSLNetworkTimeBrowserTest, OnDemandFetchClockOk) {
  ASSERT_TRUE(https_server_expired_.Start());
  // Use a testing clock set to the time that GoodTimeResponseHandler
  // returns, to simulate the system clock matching the network time.
  base::SimpleTestClock testing_clock;
  SSLErrorHandler::SetClockForTesting(&testing_clock);
  testing_clock.SetNow(
      base::Time::FromJsTime(network_time::kGoodTimeResponseHandlerJsTime[0]));
  // Set the build time to match the testing clock, to ensure that the
  // build time heuristic doesn't fire.
  ssl_errors::SetBuildTimeForTesting(testing_clock.Now());

  // Set a long timeout to ensure that the on-demand time fetch completes.
  SSLErrorHandler::SetInterstitialDelayForTesting(
      base::TimeDelta::FromHours(1));

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_expired_.GetURL("/"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);

  // Once |interstitial_timer_observer| has fired, the request has been
  // sent. Override the nonce that NetworkTimeTracker expects so that
  // when the response comes back, it will validate. The nonce can only
  // be overriden for the current in-flight request, so the test must
  // call OverrideNonceForTesting() after the request has been sent and
  // before the response has been received.
  interstitial_timer_observer.WaitForTimerStarted();
  g_browser_process->network_time_tracker()->OverrideNonceForTesting(123123123);
  TriggerTimeResponse();

  EXPECT_TRUE(contents->IsLoading());
  observer.Wait();
  WaitForInterstitial(contents);

  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(contents));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(contents));
}

// Tests that if an on-demand network time fetch returns that the clock
// is wrong, a bad clock interstitial is shown.
IN_PROC_BROWSER_TEST_F(SSLNetworkTimeBrowserTest, OnDemandFetchClockWrong) {
  ASSERT_TRUE(https_server_expired_.Start());
  // Use a testing clock set to a time that is different from what
  // GoodTimeResponseHandler returns, simulating a system clock that is
  // 30 days ahead of the network time.
  base::SimpleTestClock testing_clock;
  SSLErrorHandler::SetClockForTesting(&testing_clock);
  testing_clock.SetNow(
      base::Time::FromJsTime(network_time::kGoodTimeResponseHandlerJsTime[0]));
  testing_clock.Advance(base::TimeDelta::FromDays(30));
  // Set the build time to match the testing clock, to ensure that the
  // build time heuristic doesn't fire.
  ssl_errors::SetBuildTimeForTesting(testing_clock.Now());

  // Set a long timeout to ensure that the on-demand time fetch completes.
  SSLErrorHandler::SetInterstitialDelayForTesting(
      base::TimeDelta::FromHours(1));

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_expired_.GetURL("/"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);

  // Once |interstitial_timer_observer| has fired, the request has been
  // sent. Override the nonce that NetworkTimeTracker expects so that
  // when the response comes back, it will validate. The nonce can only
  // be overriden for the current in-flight request, so the test must
  // call OverrideNonceForTesting() after the request has been sent and
  // before the response has been received.
  interstitial_timer_observer.WaitForTimerStarted();
  g_browser_process->network_time_tracker()->OverrideNonceForTesting(123123123);
  TriggerTimeResponse();

  EXPECT_TRUE(contents->IsLoading());
  observer.Wait();
  WaitForInterstitial(contents);

  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingBadClockInterstitial(contents));
}

// Tests that if the timeout expires before the network time fetch
// returns, then a normal SSL interstitial is shown.
IN_PROC_BROWSER_TEST_F(SSLNetworkTimeBrowserTest,
                       TimeoutExpiresBeforeFetchCompletes) {
  ASSERT_TRUE(https_server_expired_.Start());
  // Set the timer to fire immediately.
  SSLErrorHandler::SetInterstitialDelayForTesting(base::TimeDelta());

  ui_test_utils::NavigateToURL(browser(), https_server_expired_.GetURL("/"));
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  WaitForInterstitial(contents);

  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(contents));

  // Navigate away, and then trigger the network time response; no crash should
  // occur.
  ASSERT_TRUE(https_server_.Start());
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL("/"));
  ASSERT_NO_FATAL_FAILURE(CheckTimeQueryPending());
  TriggerTimeResponse();
}

// Tests that if the user stops the page load before either the network
// time fetch completes or the timeout expires, then there is no interstitial.
IN_PROC_BROWSER_TEST_F(SSLNetworkTimeBrowserTest, StopBeforeTimeoutExpires) {
  ASSERT_TRUE(https_server_expired_.Start());
  // Set the timer to a long delay.
  SSLErrorHandler::SetInterstitialDelayForTesting(
      base::TimeDelta::FromHours(1));

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_expired_.GetURL("/"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);
  interstitial_timer_observer.WaitForTimerStarted();

  EXPECT_TRUE(contents->IsLoading());
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  contents->Stop();
  observer.Wait();

  // Make sure that the |SSLErrorHandler| is deleted.
  EXPECT_FALSE(SSLErrorHandler::FromWebContents(contents));
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));
  EXPECT_FALSE(contents->IsLoading());

  // Navigate away, and then trigger the network time response; no crash should
  // occur.
  ASSERT_TRUE(https_server_.Start());
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL("/title1.html"));
  ASSERT_NO_FATAL_FAILURE(CheckTimeQueryPending());
  TriggerTimeResponse();
}

// Tests that if the user reloads the page before either the network
// time fetch completes or the timeout expires, then there is no interstitial.
IN_PROC_BROWSER_TEST_F(SSLNetworkTimeBrowserTest, ReloadBeforeTimeoutExpires) {
  ASSERT_TRUE(https_server_expired_.Start());
  // Set the timer to a long delay.
  SSLErrorHandler::SetInterstitialDelayForTesting(
      base::TimeDelta::FromHours(1));

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_expired_.GetURL("/"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);
  interstitial_timer_observer.WaitForTimerStarted();

  EXPECT_TRUE(contents->IsLoading());
  content::TestNavigationObserver observer(contents);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();

  // Make sure that the |SSLErrorHandler| is deleted.
  EXPECT_FALSE(SSLErrorHandler::FromWebContents(contents));
  EXPECT_FALSE(contents->ShowingInterstitialPage());
  EXPECT_FALSE(contents->IsLoading());

  // Navigate away, and then trigger the network time response and wait
  // for the response; no crash should occur.
  ASSERT_TRUE(https_server_.Start());
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL("/"));
  ASSERT_NO_FATAL_FAILURE(CheckTimeQueryPending());
  TriggerTimeResponse();
}

// Tests that if the user navigates away before either the network time
// fetch completes or the timeout expires, then there is no
// interstitial.
IN_PROC_BROWSER_TEST_F(SSLNetworkTimeBrowserTest,
                       NavigateAwayBeforeTimeoutExpires) {
  ASSERT_TRUE(https_server_expired_.Start());
  ASSERT_TRUE(https_server_.Start());
  // Set the timer to a long delay.
  SSLErrorHandler::SetInterstitialDelayForTesting(
      base::TimeDelta::FromHours(1));

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_expired_.GetURL("/"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);
  interstitial_timer_observer.WaitForTimerStarted();

  EXPECT_TRUE(contents->IsLoading());
  content::TestNavigationObserver observer(contents, 1);
  browser()->OpenURL(content::OpenURLParams(
      https_server_.GetURL("/"), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
  observer.Wait();

  // Make sure that the |SSLErrorHandler| is deleted.
  EXPECT_FALSE(SSLErrorHandler::FromWebContents(contents));
  EXPECT_FALSE(contents->ShowingInterstitialPage());
  EXPECT_FALSE(contents->IsLoading());

  // Navigate away, and then trigger the network time response and wait
  // for the response; no crash should occur.
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL("/"));
  ASSERT_NO_FATAL_FAILURE(CheckTimeQueryPending());
  TriggerTimeResponse();
}

// Tests that if the user closes the tab before the network time fetch
// completes, it doesn't cause a crash.
IN_PROC_BROWSER_TEST_F(SSLNetworkTimeBrowserTest,
                       CloseTabBeforeNetworkFetchCompletes) {
  ASSERT_TRUE(https_server_expired_.Start());
  // Set the timer to fire immediately.
  SSLErrorHandler::SetInterstitialDelayForTesting(base::TimeDelta());

  ui_test_utils::NavigateToURL(browser(), https_server_expired_.GetURL("/"));
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  WaitForInterstitial(contents);

  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(contents));

  // Open a second tab, close the first, and then trigger the network time
  // response and wait for the response; no crash should occur.
  ASSERT_TRUE(https_server_.Start());
  AddTabAtIndex(1, https_server_.GetURL("/"), ui::PAGE_TRANSITION_TYPED);
  chrome::CloseWebContents(browser(), contents, false);
  ASSERT_NO_FATAL_FAILURE(CheckTimeQueryPending());
  TriggerTimeResponse();
}

class CommonNameMismatchBrowserTest : public CertVerifierBrowserTest {
 public:
  CommonNameMismatchBrowserTest() : CertVerifierBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    CertVerifierBrowserTest::SetUpCommandLine(command_line);
    // Enable finch experiment for SSL common name mismatch handling.
    command_line->AppendSwitchASCII(switches::kForceFieldTrials,
                                    "SSLCommonNameMismatchHandling/Enabled/");
  }

  void SetUpOnMainThread() override {
    CertVerifierBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void TearDownOnMainThread() override {
    CertVerifierBrowserTest::TearDownOnMainThread();
  }
};

// Visit the URL www.mail.example.com on a server that presents a valid
// certificate for mail.example.com. Verify that the page navigates to
// mail.example.com.
IN_PROC_BROWSER_TEST_F(CommonNameMismatchBrowserTest,
                       ShouldShowWWWSubdomainMismatchInterstitial) {
  net::EmbeddedTestServer https_server_example_domain(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_example_domain.ServeFilesFromSourceDirectory(
      GetChromeTestDataDir());
  ASSERT_TRUE(https_server_example_domain.Start());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain.GetCertificate();

  // Use the "spdy_pooling.pem" cert which has "mail.example.com"
  // as one of its SANs.
  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

  // Request to "www.mail.example.com" should result in
  // |net::ERR_CERT_COMMON_NAME_INVALID| error.
  mock_cert_verifier()->AddResultForCertAndHost(
      cert.get(), "www.mail.example.com", verify_result,
      net::ERR_CERT_COMMON_NAME_INVALID);

  net::CertVerifyResult verify_result_valid;
  verify_result_valid.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  // Request to "www.mail.example.com" should not result in any error.
  mock_cert_verifier()->AddResultForCertAndHost(cert.get(), "mail.example.com",
                                                verify_result_valid, net::OK);

  // Use a complex URL to ensure the path, etc., are preserved. The path itself
  // does not matter.
  const GURL https_server_url =
      https_server_example_domain.GetURL("/ssl/google.html?a=b#anchor");
  GURL::Replacements replacements;
  replacements.SetHostStr("www.mail.example.com");
  const GURL https_server_mismatched_url =
      https_server_url.ReplaceComponents(replacements);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(contents, 1);
  ui_test_utils::NavigateToURL(browser(), https_server_mismatched_url);
  observer.Wait();

  ssl_test_util::CheckSecurityState(contents, CertError::NONE,
                                    security_state::SECURE, AuthState::NONE);
  replacements.SetHostStr("mail.example.com");
  GURL https_server_new_url = https_server_url.ReplaceComponents(replacements);
  // Verify that the current URL is the suggested URL.
  EXPECT_EQ(https_server_new_url.spec(),
            contents->GetLastCommittedURL().spec());
}

// Visit the URL www.mail.example.com on a server that presents an invalid
// certificate for mail.example.com. Verify that the page shows an interstitial
// for www.mail.example.com with no crash.
IN_PROC_BROWSER_TEST_F(CommonNameMismatchBrowserTest,
                       NoCrashIfBothSubdomainsHaveCommonNameErrors) {
  net::EmbeddedTestServer https_server_example_domain(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_example_domain.ServeFilesFromSourceDirectory(
      GetChromeTestDataDir());
  ASSERT_TRUE(https_server_example_domain.Start());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain.GetCertificate();

  // Use the "spdy_pooling.pem" cert which has "mail.example.com"
  // as one of its SANs.
  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

  // Request to "www.mail.example.com" should result in
  // |net::ERR_CERT_COMMON_NAME_INVALID| error.
  mock_cert_verifier()->AddResultForCertAndHost(
      cert.get(), "www.mail.example.com", verify_result,
      net::ERR_CERT_COMMON_NAME_INVALID);

  // Request to "mail.example.com" should also result in
  // |net::ERR_CERT_COMMON_NAME_INVALID| error.
  mock_cert_verifier()->AddResultForCertAndHost(
      cert.get(), "mail.example.com", verify_result,
      net::ERR_CERT_COMMON_NAME_INVALID);

  // Use a complex URL to ensure the path, etc., are preserved. The path itself
  // does not matter.
  const GURL https_server_url =
      https_server_example_domain.GetURL("/ssl/google.html?a=b#anchor");
  GURL::Replacements replacements;
  replacements.SetHostStr("www.mail.example.com");
  const GURL https_server_mismatched_url =
      https_server_url.ReplaceComponents(replacements);

  // Should simply show an interstitial, because both subdomains have common
  // name errors.
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(), https_server_mismatched_url);
  WaitForInterstitial(contents);

  ssl_test_util::CheckSecurityState(
      contents, net::CERT_STATUS_COMMON_NAME_INVALID, security_state::DANGEROUS,
      AuthState::SHOWING_INTERSTITIAL);
}

// Visit the URL example.org on a server that presents a valid certificate
// for www.example.org. Verify that the page redirects to www.example.org.
IN_PROC_BROWSER_TEST_F(CommonNameMismatchBrowserTest,
                       CheckWWWSubdomainMismatchInverse) {
  net::EmbeddedTestServer https_server_example_domain(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_example_domain.ServeFilesFromSourceDirectory(
      GetChromeTestDataDir());
  ASSERT_TRUE(https_server_example_domain.Start());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain.GetCertificate();

  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

  mock_cert_verifier()->AddResultForCertAndHost(
      cert.get(), "example.org", verify_result,
      net::ERR_CERT_COMMON_NAME_INVALID);

  net::CertVerifyResult verify_result_valid;
  verify_result_valid.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  mock_cert_verifier()->AddResultForCertAndHost(cert.get(), "www.example.org",
                                                verify_result_valid, net::OK);

  const GURL https_server_url =
      https_server_example_domain.GetURL("/ssl/google.html?a=b");
  GURL::Replacements replacements;
  replacements.SetHostStr("example.org");
  const GURL https_server_mismatched_url =
      https_server_url.ReplaceComponents(replacements);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(contents, 1);
  ui_test_utils::NavigateToURL(browser(), https_server_mismatched_url);
  observer.Wait();

  ssl_test_util::CheckSecurityState(contents, CertError::NONE,
                                    security_state::SECURE, AuthState::NONE);
}

namespace {
// Redirects incoming request to http://example.org.
std::unique_ptr<net::test_server::HttpResponse> HTTPSToHTTPRedirectHandler(
    const net::EmbeddedTestServer* test_server,
    const net::test_server::HttpRequest& request) {
  GURL::Replacements replacements;
  replacements.SetHostStr("example.org");
  replacements.SetSchemeStr("http");
  const GURL redirect_url =
      test_server->base_url().ReplaceComponents(replacements);

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", redirect_url.spec());
  return std::move(http_response);
}
}  // namespace

// Common name mismatch handling feature should ignore redirects when pinging
// the suggested hostname. Visit the URL example.org on a server that presents a
// valid certificate for www.example.org. In this case, www.example.org
// redirects to http://example.org, and the SSL error should not be redirected
// to this URL.
IN_PROC_BROWSER_TEST_F(CommonNameMismatchBrowserTest,
                       WWWSubdomainMismatch_StopOnRedirects) {
  net::EmbeddedTestServer https_server_example_domain(
      net::EmbeddedTestServer::TYPE_HTTPS);

  // Redirect all URLs to http://example.org. Since this test will trigger only
  // one request to check the suggested URL, redirecting all requests is OK.
  // We would normally use content::SetupCrossSiteRedirector here, but that
  // function does not support https to http redirects.
  // This must be done before ServeFilesFromSourceDirectory(), otherwise the
  // test server will serve files instead of redirecting requests to them.
  https_server_example_domain.RegisterRequestHandler(
      base::Bind(&HTTPSToHTTPRedirectHandler, &https_server_example_domain));

  https_server_example_domain.ServeFilesFromSourceDirectory(
      GetChromeTestDataDir());

  ASSERT_TRUE(https_server_example_domain.Start());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain.GetCertificate();

  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

  mock_cert_verifier()->AddResultForCertAndHost(
      cert.get(), "example.org", verify_result,
      net::ERR_CERT_COMMON_NAME_INVALID);

  net::CertVerifyResult verify_result_valid;
  verify_result_valid.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  mock_cert_verifier()->AddResultForCertAndHost(cert.get(), "www.example.org",
                                                verify_result_valid, net::OK);

  // The user will visit https://example.org:port/ssl/blank.html.
  GURL::Replacements replacements;
  replacements.SetHostStr("example.org");
  const GURL https_server_mismatched_url =
      https_server_example_domain.GetURL("/ssl/blank.html")
          .ReplaceComponents(replacements);

  // Should simply show an interstitial, because the suggested URL
  // (https://www.example.org) redirected to http://example.org.
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(), https_server_mismatched_url);
  WaitForInterstitial(contents);

  ssl_test_util::CheckSecurityState(
      contents, net::CERT_STATUS_COMMON_NAME_INVALID, security_state::DANGEROUS,
      AuthState::SHOWING_INTERSTITIAL);
}

// Tests this scenario:
// - |CommonNameMismatchHandler| does not give a callback as it's set into the
//   state |IGNORE_REQUESTS_FOR_TESTING|. So no suggested URL check result can
//   arrive.
// - A cert error triggers an interstitial timer with a very long timeout.
// - No suggested URL check results arrive, causing the tab to appear as loading
//   indefinitely (also because the timer has a long timeout).
// - Stopping the page load shouldn't result in any interstitials.
IN_PROC_BROWSER_TEST_F(CommonNameMismatchBrowserTest,
                       InterstitialStopNavigationWhileLoading) {
  net::EmbeddedTestServer https_server_example_domain(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_example_domain.ServeFilesFromSourceDirectory(
      GetChromeTestDataDir());
  ASSERT_TRUE(https_server_example_domain.Start());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain.GetCertificate();

  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

  mock_cert_verifier()->AddResultForCertAndHost(
      cert.get(), "www.mail.example.com", verify_result,
      net::ERR_CERT_COMMON_NAME_INVALID);

  net::CertVerifyResult verify_result_valid;
  verify_result_valid.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  mock_cert_verifier()->AddResultForCertAndHost(cert.get(), "mail.example.com",
                                                verify_result_valid, net::OK);

  const GURL https_server_url =
      https_server_example_domain.GetURL("/ssl/google.html?a=b");
  GURL::Replacements replacements;
  replacements.SetHostStr("www.mail.example.com");
  const GURL https_server_mismatched_url =
      https_server_url.ReplaceComponents(replacements);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  CommonNameMismatchHandler::set_state_for_testing(
      CommonNameMismatchHandler::IGNORE_REQUESTS_FOR_TESTING);
  // Set delay long enough so that the page appears loading.
  SSLErrorHandler::SetInterstitialDelayForTesting(
      base::TimeDelta::FromHours(1));
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_mismatched_url,
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);
  interstitial_timer_observer.WaitForTimerStarted();

  EXPECT_TRUE(contents->IsLoading());
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  contents->Stop();
  observer.Wait();

  SSLErrorHandler* ssl_error_handler =
      SSLErrorHandler::FromWebContents(contents);
  // Make sure that the |SSLErrorHandler| is deleted.
  EXPECT_FALSE(ssl_error_handler);
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));
  EXPECT_FALSE(contents->IsLoading());
}

// Same as above, but instead of stopping, the loading page is reloaded. The end
// result is the same. (i.e. page load stops, no interstitials shown)
IN_PROC_BROWSER_TEST_F(CommonNameMismatchBrowserTest,
                       InterstitialReloadNavigationWhileLoading) {
  net::EmbeddedTestServer https_server_example_domain(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_example_domain.ServeFilesFromSourceDirectory(
      GetChromeTestDataDir());
  ASSERT_TRUE(https_server_example_domain.Start());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain.GetCertificate();

  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

  mock_cert_verifier()->AddResultForCertAndHost(
      cert.get(), "www.mail.example.com", verify_result,
      net::ERR_CERT_COMMON_NAME_INVALID);

  net::CertVerifyResult verify_result_valid;
  verify_result_valid.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  mock_cert_verifier()->AddResultForCertAndHost(cert.get(), "mail.example.com",
                                                verify_result_valid, net::OK);

  const GURL https_server_url =
      https_server_example_domain.GetURL("/ssl/google.html?a=b");
  GURL::Replacements replacements;
  replacements.SetHostStr("www.mail.example.com");
  const GURL https_server_mismatched_url =
      https_server_url.ReplaceComponents(replacements);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  CommonNameMismatchHandler::set_state_for_testing(
      CommonNameMismatchHandler::IGNORE_REQUESTS_FOR_TESTING);
  // Set delay long enough so that the page appears loading.
  SSLErrorHandler::SetInterstitialDelayForTesting(
      base::TimeDelta::FromHours(1));
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_mismatched_url,
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);
  interstitial_timer_observer.WaitForTimerStarted();

  EXPECT_TRUE(contents->IsLoading());
  content::TestNavigationObserver observer(contents, 1);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();

  SSLErrorHandler* ssl_error_handler =
      SSLErrorHandler::FromWebContents(contents);
  // Make sure that the |SSLErrorHandler| is deleted.
  EXPECT_FALSE(ssl_error_handler);
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));
  EXPECT_FALSE(contents->IsLoading());
}

// Same as above, but instead of reloading, the page is navigated away. The
// new page should load, and no interstitials should be shown.
IN_PROC_BROWSER_TEST_F(CommonNameMismatchBrowserTest,
                       InterstitialNavigateAwayWhileLoading) {
  net::EmbeddedTestServer https_server_example_domain(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_example_domain.ServeFilesFromSourceDirectory(
      GetChromeTestDataDir());
  ASSERT_TRUE(https_server_example_domain.Start());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain.GetCertificate();

  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

  mock_cert_verifier()->AddResultForCertAndHost(
      cert.get(), "www.mail.example.com", verify_result,
      net::ERR_CERT_COMMON_NAME_INVALID);

  net::CertVerifyResult verify_result_valid;
  verify_result_valid.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  mock_cert_verifier()->AddResultForCertAndHost(cert.get(), "mail.example.com",
                                                verify_result_valid, net::OK);

  const GURL https_server_url =
      https_server_example_domain.GetURL("/ssl/google.html?a=b");
  GURL::Replacements replacements;
  replacements.SetHostStr("www.mail.example.com");
  const GURL https_server_mismatched_url =
      https_server_url.ReplaceComponents(replacements);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  CommonNameMismatchHandler::set_state_for_testing(
      CommonNameMismatchHandler::IGNORE_REQUESTS_FOR_TESTING);
  // Set delay long enough so that the page appears loading.
  SSLErrorHandler::SetInterstitialDelayForTesting(
      base::TimeDelta::FromHours(1));
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_mismatched_url,
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);
  interstitial_timer_observer.WaitForTimerStarted();

  EXPECT_TRUE(contents->IsLoading());
  content::TestNavigationObserver observer(contents, 1);
  browser()->OpenURL(content::OpenURLParams(
      GURL("https://google.com"), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
  observer.Wait();

  SSLErrorHandler* ssl_error_handler =
      SSLErrorHandler::FromWebContents(contents);
  // Make sure that the |SSLErrorHandler| is deleted.
  EXPECT_FALSE(ssl_error_handler);
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));
  EXPECT_FALSE(contents->IsLoading());
}

class SSLBlockingPageIDNTest
    : public chrome_browser_interstitials::SecurityInterstitialIDNTest {
 protected:
  // chrome_browser_interstitials::SecurityInterstitialIDNTest:
  security_interstitials::SecurityInterstitialPage* CreateInterstitial(
      WebContents* contents,
      const GURL& request_url) const override {
    net::SSLInfo ssl_info;
    ssl_info.cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ChromeSecurityBlockingPageFactory blocking_page_factory;
    return blocking_page_factory
        .CreateSSLPage(contents, net::ERR_CERT_CONTAINS_ERRORS, ssl_info,
                       request_url, 0, base::Time::NowFromSystemTime(), GURL(),
                       nullptr)
        .release();
  }
};

// Flaky on mac OS and Windows: https://crbug.com/689846
#if defined(OS_MACOSX) || defined(OS_WIN)
#define MAYBE_SSLBlockingPageDecodesIDN DISABLED_SSLBlockingPageDecodesIDN
#else
#define MAYBE_SSLBlockingPageDecodesIDN SSLBlockingPageDecodesIDN
#endif

IN_PROC_BROWSER_TEST_F(SSLBlockingPageIDNTest,
                       MAYBE_SSLBlockingPageDecodesIDN) {
  EXPECT_TRUE(VerifyIDNDecoded());
}

IN_PROC_BROWSER_TEST_F(CertVerifierBrowserTest, MockCertVerifierSmokeTest) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  mock_cert_verifier()->set_default_result(
      net::ERR_CERT_NAME_CONSTRAINT_VIOLATION);

  ui_test_utils::NavigateToURL(browser(),
                               https_server.GetURL("/ssl/google.html"));

  ssl_test_util::CheckSecurityState(
      browser()->tab_strip_model()->GetActiveWebContents(),
      net::CERT_STATUS_NAME_CONSTRAINT_VIOLATION, security_state::DANGEROUS,
      AuthState::SHOWING_INTERSTITIAL);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, RestoreHasSSLState) {
  ASSERT_TRUE(https_server_.Start());
  GURL url(https_server_.GetURL("/ssl/google.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  content::NavigationEntry* entry =
      tab->GetController().GetLastCommittedEntry();
  std::unique_ptr<content::NavigationEntry> restored_entry =
      content::NavigationController::CreateNavigationEntry(
          url, content::Referrer(), base::nullopt, ui::PAGE_TRANSITION_RELOAD,
          false, std::string(), tab->GetBrowserContext(),
          nullptr /* blob_url_loader_factory */);
  restored_entry->SetPageState(entry->GetPageState());

  WebContents::CreateParams params(tab->GetBrowserContext());
  std::unique_ptr<WebContents> tab2 = WebContents::Create(params);
  WebContents* raw_tab2 = tab2.get();
  tab->GetDelegate()->AddNewContents(nullptr, std::move(tab2), url,
                                     WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                     gfx::Rect(), false, nullptr);
  std::vector<std::unique_ptr<content::NavigationEntry>> entries;
  entries.push_back(std::move(restored_entry));
  content::TestNavigationObserver observer(raw_tab2);
  raw_tab2->GetController().Restore(
      entries.size() - 1, content::RestoreType::LAST_SESSION_EXITED_CLEANLY,
      &entries);
  raw_tab2->GetController().LoadIfNecessary();
  observer.Wait();
  ssl_test_util::CheckAuthenticatedState(raw_tab2, AuthState::NONE);
}

void SetupRestoredTabWithNavigation(
    net::test_server::EmbeddedTestServer* https_server,
    Browser* browser) {
  ASSERT_TRUE(https_server->Start());
  GURL url(https_server->GetURL("/ssl/google.html"));
  ui_test_utils::NavigateToURL(browser, url);
  WebContents* tab = browser->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationObserver observer(tab);
  EXPECT_TRUE(ExecuteScript(tab, "history.pushState({}, '', '');"));
  observer.Wait();

  ui_test_utils::NavigateToURLWithDisposition(
      browser, GURL("about:blank"), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  chrome::CloseTab(browser);

  WebContents* blank_tab = browser->tab_strip_model()->GetActiveWebContents();

  // Restore the tab.
  ui_test_utils::TabAddedWaiter tab_added_waiter(browser);
  content::WindowedNotificationObserver tab_loaded_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::RestoreTab(browser);
  tab_added_waiter.Wait();
  tab_loaded_observer.Wait();

  tab = browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(tab, blank_tab);
}

// Simulate a browser-initiated in-page navigation in a restored tab.
// https://crbug.com/662267
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       BrowserInitiatedExistingPageAfterRestoreHasSSLState) {
  SetupRestoredTabWithNavigation(&https_server_, browser());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(tab);
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// Simulate a renderer-initiated in-page navigation in a restored tab.
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       RendererInitiatedExistingPageAfterRestoreHasSSLState) {
  SetupRestoredTabWithNavigation(&https_server_, browser());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  content::TestNavigationObserver observer(tab);
  ASSERT_TRUE(content::ExecuteScript(
      tab, "location.replace(window.location.href + '#1')"));
  observer.Wait();
  content::WaitForLoadStop(tab);
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

namespace {

// A handler which changes the response. The first time it's called for
// |relative_url| it'll give an empty response. The second time it'll
// redirect to |redirect_url|.
std::unique_ptr<net::test_server::HttpResponse> ChangingHandler(
    int* count,
    const std::string& relative_url,
    const GURL& redirect_url,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != relative_url)
    return nullptr;

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  if ((*count)++) {
    http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location", redirect_url.spec());
  }
  return std::move(http_response);
}

}  // namespace

// Check that SSL state isn't stale when navigating to an existing page that
// gives a different response. This covers the case of going from http to
// https. http://crbug.com/792221
IN_PROC_BROWSER_TEST_F(SSLUITest, ExistingPageHTTPToHTTPSSSLState) {
  ASSERT_TRUE(https_server_.Start());
  int count = 0;
  std::string relative_url = "/foo";
  GURL redirect_url = https_server_.GetURL("/simple.html");
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(ChangingHandler, &count, relative_url, redirect_url));
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL(relative_url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(), url);
  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);

  content::TestNavigationObserver observer(tab, 1);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();

  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// Check that SSL state isn't stale when navigating to an existing page that
// gives a different response. This covers the case of going from https to
// http URL. http://crbug.com/792221
IN_PROC_BROWSER_TEST_F(SSLUITest, ExistingPageHTTPSToHTTPSSLState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  int count = 0;
  std::string relative_url = "/foo";
  GURL redirect_url = embedded_test_server()->GetURL("/simple.html");
  https_server_.RegisterRequestHandler(
      base::BindRepeating(ChangingHandler, &count, relative_url, redirect_url));
  ASSERT_TRUE(https_server_.Start());
  const GURL url = https_server_.GetURL(relative_url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(), url);
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  content::TestNavigationObserver observer(tab, 1);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();

  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);

  // We also manually check the cert on the NavigationEntry, since in the case
  // of http URLs GetSecurityLevelForRequest will return SecurityLevel::NONE for
  // http URLs.
  content::NavigationEntry* entry = tab->GetController().GetVisibleEntry();
  ASSERT_FALSE(!!entry->GetSSL().certificate);
}

// Checks that a restore followed immediately by a history navigation doesn't
// lose SSL state.
// Disabled since this is a test for bug 738177.
IN_PROC_BROWSER_TEST_F(SSLUITest, DISABLED_RestoreThenNavigateHasSSLState) {
  ASSERT_TRUE(https_server_.Start());
  GURL url1(https_server_.GetURL("/ssl/google.html"));
  GURL url2(https_server_.GetURL("/ssl/page_with_refs.html"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURL(browser(), url2);
  chrome::CloseTab(browser());

  ui_test_utils::TabAddedWaiter tab_added_waiter(browser());
  chrome::RestoreTab(browser());
  tab_added_waiter.Wait();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationManager observer(tab, url1);
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.WaitForNavigationFinished();
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// Simulate the URL changing when the user presses enter in the omnibox. This
// could happen when the user's login is expired and the server redirects them
// to a login page. This will be considered a same document navigation but we
// do want to update the SSL state.
IN_PROC_BROWSER_TEST_F(SSLUITest, SameDocumentHasSSLState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to a simple page and then perform an in-page navigation.
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(browser(), start_url);

  GURL fragment_change_url(embedded_test_server()->GetURL("/title1.html#foo"));
  ui_test_utils::NavigateToURL(browser(), fragment_change_url);
  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);

  // Replace the URL of the current NavigationEntry with one that will cause
  // a server redirect when loaded.
  {
    GURL redirect_dest_url(https_server_.GetURL("/ssl/google.html"));
    content::TestNavigationObserver observer(tab);
    std::string script = "history.replaceState({}, '', '/server-redirect?" +
                         redirect_dest_url.spec() + "')";
    EXPECT_TRUE(ExecuteScript(tab, script));
    observer.Wait();
  }

  // Simulate the user hitting Enter in the omnibox without changing the URL.
  {
    content::TestNavigationObserver observer(tab);
    tab->GetController().LoadURL(tab->GetLastCommittedURL(),
                                 content::Referrer(), ui::PAGE_TRANSITION_LINK,
                                 std::string());
    observer.Wait();
  }

  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// Simulate the user revisiting a page without triggering a reload (e.g., when
// clicking a bookmark with an anchor hash twice).  As this is a same document
// navigation, the SSL state should be left intact despite not triggering a
// network request. Regression test for https://crbug.com/877618.
IN_PROC_BROWSER_TEST_F(SSLUITest, SameDocumentHasSSLStateNoLoad) {
  ASSERT_TRUE(https_server_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL start_url(https_server_.GetURL("/ssl/google.html#foo"));
  ui_test_utils::NavigateToURL(browser(), start_url);

  // Simulate clicking on a bookmark.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    NavigateParams navigate_params(browser(), start_url,
                                   ui::PAGE_TRANSITION_AUTO_BOOKMARK);
    Navigate(&navigate_params);
    observer.Wait();
  }

  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// Checks that if a client redirect occurs while the page is loading, the SSL
// state reflects the final URL.
IN_PROC_BROWSER_TEST_F(SSLUITest, ClientRedirectSSLState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  GURL https_url = https_server_.GetURL("/ssl/redirect.html?");
  GURL http_url = embedded_test_server()->GetURL("/ssl/google.html");
  GURL url(https_url.spec() + http_url.spec());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationManager navigation_observer_https(tab, url);
  content::TestNavigationManager navigation_observer_http(tab, http_url);

  tab->GetController().LoadURL(url, content::Referrer(),
                               ui::PAGE_TRANSITION_LINK, std::string());

  navigation_observer_https.WaitForNavigationFinished();
  navigation_observer_http.WaitForNavigationFinished();
  content::WaitForLoadStop(tab);

  ssl_test_util::CheckUnauthenticatedState(tab, AuthState::NONE);
}

// Checks that if a redirect occurs while the page is loading from a mixed
// content to a valid HTTPS page, the SSL state reflects the final URL.
IN_PROC_BROWSER_TEST_F(SSLUITest, ClientRedirectFromMixedContentSSLState) {
  ASSERT_TRUE(https_server_.Start());

  GURL url = GURL(
      https_server_.GetURL("/ssl/redirect_with_mixed_content.html").spec() +
      "?" + https_server_.GetURL("/ssl/google.html").spec());

  // Load a page that displays insecure content.
  ui_test_utils::NavigateToURL(browser(), url);
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// Checks that if a redirect occurs while the page is loading from a valid HTTPS
// page to a mixed content page, the SSL state reflects the final URL.
IN_PROC_BROWSER_TEST_F(SSLUITest, ClientRedirectToMixedContentSSLState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  GURL redirect(https_server_.GetURL("/ssl/redirect.html"));
  GURL final_url(
      https_server_.GetURL("/ssl/page_displays_insecure_content.html"));
  GURL url = GURL(redirect.spec() + "?" + final_url.spec());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationManager navigation_manager_redirect(tab, url);
  content::TestNavigationManager navigation_manager_final_url(tab, final_url);

  tab->GetController().LoadURL(url, content::Referrer(),
                               ui::PAGE_TRANSITION_LINK, std::string());

  navigation_manager_redirect.WaitForNavigationFinished();
  navigation_manager_final_url.WaitForNavigationFinished();
  content::WaitForLoadStop(tab);

  ssl_test_util::CheckSecurityState(tab, CertError::NONE,
                                    GetPassiveMixedContentSecurityLevel(),
                                    AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Checks that same-document navigations during page load preserve SSL state.
IN_PROC_BROWSER_TEST_F(SSLUITest, SameDocumentNavigationDuringLoadSSLState) {
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(
      browser(),
      https_server_.GetURL("/ssl/same_document_navigation_during_load.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// Checks that same-document navigations after the page load preserve SSL
// state.
IN_PROC_BROWSER_TEST_F(SSLUITest, SameDocumentNavigationAfterLoadSSLState) {
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(tab, "location.hash = Math.random()"));
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

// Checks that navigations after pushState maintain the SSL status.
// Flaky, see https://crbug.com/872029 and https://crbug.com/872030.
IN_PROC_BROWSER_TEST_F(SSLUITest, DISABLED_PushStateSSLState) {
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  content::TestNavigationObserver observer(tab);
  EXPECT_TRUE(ExecuteScript(tab, "history.pushState({}, '', '');"));
  observer.Wait();
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);

  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(tab);
  ssl_test_util::CheckAuthenticatedState(tab, AuthState::NONE);
}

#if defined(OS_CHROMEOS)

class SSLUITestNoCert : public SSLUITest,
                        public CertificateManagerModel::Observer {
 public:
  SSLUITestNoCert() = default;
  ~SSLUITestNoCert() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableTestCerts);
    SSLUITest::SetUpCommandLine(command_line);
  }

  // CertificateManagerModel::Observer implementation:
  void CertificatesRefreshed() override {}
};

// Checks that a newly-added certificate authority is usable immediately.
IN_PROC_BROWSER_TEST_F(SSLUITestNoCert, NewCertificateAuthority) {
  if (!content::IsOutOfProcessNetworkService())
    return;

  ASSERT_TRUE(https_server_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));

  std::unique_ptr<CertificateManagerModel> model;
  base::RunLoop run_loop;
  CertificateManagerModel::Create(
      browser()->profile(), this,
      base::BindLambdaForTesting(
          [&](std::unique_ptr<CertificateManagerModel> model2) {
            model = std::move(model2);
            run_loop.Quit();
          }));
  run_loop.Run();

  scoped_refptr<net::X509Certificate> cert;
  net::ScopedCERTCertificateList nss_certs;

  {
    base::ScopedAllowBlockingForTesting allow_io;
    cert = net::CreateCertificateChainFromFile(
        net::GetTestCertsDirectory(), "root_ca_cert.pem",
        net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);

    nss_certs = net::x509_util::CreateCERTCertificateListFromX509Certificate(
        cert.get());
  }

  net::NSSCertDatabase::ImportCertFailureList not_imported;
  EXPECT_TRUE(model->ImportCACerts(nss_certs, net::NSSCertDatabase::TRUSTED_SSL,
                                   &not_imported));
  EXPECT_TRUE(not_imported.empty());

  content::FlushNetworkServiceInstanceForTesting();

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(tab));
}

// A test class which prepares two profiles and allows importing certificates
// into their NSS databases.
class SSLUITestCustomCACerts : public SSLUITestNoCert {
 public:
  SSLUITestCustomCACerts() = default;
  ~SSLUITestCustomCACerts() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SSLUITestNoCert::SetUpCommandLine(command_line);
    // Don't require policy for our sessions - this is required so the policy
    // code knows not to expect cached policy for the secondary profile.
    command_line->AppendSwitchASCII(chromeos::switches::kProfileRequiresPolicy,
                                    "false");
  }

  void SetUpOnMainThread() override {
    SSLUITestNoCert::SetUpOnMainThread();

    profile_1_ = browser()->profile();

    // Create a second profile.
    {
      static const char kSecondProfileAccount[] = "profile2@test.com";
      static const char kSecondProfileGaiaId[] = "9876543210";
      static const char kSecondProfileHash[] = "testProfile2";

      EXPECT_CALL(policy_for_profile_2_, IsInitializationComplete(testing::_))
          .WillRepeatedly(testing::Return(true));
      policy::PushProfilePolicyConnectorProviderForTesting(
          &policy_for_profile_2_);

      base::FilePath user_data_directory;
      base::PathService::Get(chrome::DIR_USER_DATA, &user_data_directory);
      session_manager::SessionManager::Get()->CreateSession(
          AccountId::FromUserEmailGaiaId(kSecondProfileAccount,
                                         kSecondProfileGaiaId),
          kSecondProfileHash, false);
      // Set up the secondary profile.
      base::FilePath profile_dir = user_data_directory.Append(
          chromeos::ProfileHelper::GetUserProfileDir(kSecondProfileHash)
              .BaseName());
      profile_2_ =
          g_browser_process->profile_manager()->GetProfile(profile_dir);
    }

    // Get cert databases for both profiles.
    {
      base::RunLoop loop;
      GetNSSCertDatabaseForProfile(
          profile_1_,
          base::Bind(&SSLUITestCustomCACerts::DidGetCertDatabase,
                     base::Unretained(this), &loop, &profile_1_cert_db_));
      loop.Run();
    }

    {
      base::RunLoop loop;
      GetNSSCertDatabaseForProfile(
          profile_2_,
          base::Bind(&SSLUITestCustomCACerts::DidGetCertDatabase,
                     base::Unretained(this), &loop, &profile_2_cert_db_));
      loop.Run();
    }

    // Double-check that the profile initialization was correct and the two
    // profiles have distinct NSS databases with distinc NSS public slots.
    EXPECT_NE(profile_1_cert_db_, profile_2_cert_db_);
    EXPECT_NE(profile_1_cert_db_->GetPublicSlot().get(),
              profile_2_cert_db_->GetPublicSlot().get());
  }

 protected:
  void ImportCACertAsTrusted(const std::string& cert_file_name,
                             net::NSSCertDatabase* cert_db) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    net::ScopedCERTCertificateList ca_cert_list =
        net::CreateCERTCertificateListFromFile(
            net::GetTestCertsDirectory(), cert_file_name,
            net::X509Certificate::FORMAT_AUTO);
    ASSERT_FALSE(ca_cert_list.empty());
    net::NSSCertDatabase::ImportCertFailureList failures;
    ASSERT_TRUE(cert_db->ImportCACerts(
        ca_cert_list, net::NSSCertDatabase::TRUSTED_SSL, &failures));
    ASSERT_TRUE(failures.empty());
  }

  // The first profile.
  Profile* profile_1_;
  // The second profile.
  Profile* profile_2_;

  // The NSSCertDatabase for |profile_1_|.
  net::NSSCertDatabase* profile_1_cert_db_;

  // The NSSCertDatabase for |profile_2_|.
  net::NSSCertDatabase* profile_2_cert_db_;

  // Policy provider for |profile_2_|. Overrides any other policy providers.
  policy::MockConfigurationPolicyProvider policy_for_profile_2_;

 private:
  void DidGetCertDatabase(base::RunLoop* loop,
                          net::NSSCertDatabase** out_cert_db,
                          net::NSSCertDatabase* cert_db) {
    *out_cert_db = cert_db;
    loop->Quit();
  }

  DISALLOW_COPY_AND_ASSIGN(SSLUITestCustomCACerts);
};

// Imports a trusted CA certiifcate into a profile's NSS database.
// Verifies that the certificate is trusted in the context of the profile it was
// imported for.
// Verifies that the certificate is *not* trusted in the context of a different
// profile.
IN_PROC_BROWSER_TEST_F(SSLUITestCustomCACerts,
                       TrustedCertOnlyRespectedInProfileThatOwnsIt) {
  ASSERT_TRUE(https_server_.Start());

  ASSERT_NO_FATAL_FAILURE(
      ImportCACertAsTrusted("root_ca_cert.pem", profile_2_cert_db_));

  // Flush the network service instance so persistent NSS Database changes are
  // reflected in the network service.
  content::FlushNetworkServiceInstanceForTesting();

  // The certificate that is trusted in |profile_2_| should not be respected in
  // browsers that belong to |profile_1_|.
  Browser* browser_for_profile_1 = CreateBrowser(profile_1_);
  ui_test_utils::NavigateToURL(browser_for_profile_1,
                               https_server_.GetURL("/ssl/google.html"));
  WebContents* tab_for_profile_1 =
      browser_for_profile_1->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitial(tab_for_profile_1);
  ssl_test_util::CheckAuthenticationBrokenState(
      tab_for_profile_1, net::CERT_STATUS_AUTHORITY_INVALID,
      AuthState::SHOWING_INTERSTITIAL);

  // The certificate that is trusted in |profile_2_| should be respected in
  // browsers that belong to |profile_2_|.
  Browser* browser_for_profile_2 = CreateBrowser(profile_2_);
  ui_test_utils::NavigateToURL(browser_for_profile_2,
                               https_server_.GetURL("/ssl/google.html"));
  WebContents* tab_for_profile_2 =
      browser_for_profile_2->tab_strip_model()->GetActiveWebContents();
  ssl_test_util::CheckAuthenticatedState(tab_for_profile_2, AuthState::NONE);
}

#endif  // defined(OS_CHROMEOS)

// Regression test for http://crbug.com/635833 (crash when a window with no
// NavigationEntry commits).
IN_PROC_BROWSER_TEST_F(SSLUITestIgnoreLocalhostCertErrors,
                       NoCrashOnLoadWithNoNavigationEntry) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(tab, "window.open()"));
}

class SSLUICaptivePortalListEnabledTest : public SSLUITest {
 public:
  SSLUICaptivePortalListEnabledTest() {
    feature_list_.InitWithFeatures(
        {kCaptivePortalCertificateList} /* enabled */, {} /* disabled */);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class SSLUICaptivePortalListDisabledTest : public SSLUITest {
 public:
  SSLUICaptivePortalListDisabledTest() {
    feature_list_.InitWithFeatures(
        {} /* enabled */, {kCaptivePortalCertificateList} /* disabled */);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig>
MakeCaptivePortalConfig(int version_id,
                        const std::set<std::string>& spki_hashes) {
  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(version_id);
  for (const std::string& hash : spki_hashes) {
    config_proto->add_captive_portal_cert()->set_sha256_hash(hash);
  }
  return config_proto;
}

// Tests that the captive portal certificate list is not used when the feature
// is disabled via Finch. The list is passed to SSLErrorHandler via a proto.
IN_PROC_BROWSER_TEST_F(SSLUICaptivePortalListDisabledTest, Disabled) {
  ASSERT_TRUE(https_server_mismatched_.Start());
  base::HistogramTester histograms;

  // Mark the server's cert as a captive portal cert.
  const net::HashValue server_spki_hash =
      GetSPKIHash(https_server_mismatched_.GetCertificate()->cert_buffer());
  SSLErrorHandler::SetErrorAssistantProto(MakeCaptivePortalConfig(
      kLargeVersionId, std::set<std::string>{server_spki_hash.ToString()}));
  ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() > 0);

  // Navigate to an unsafe page on the server. A normal SSL interstitial should
  // be displayed since CaptivePortalCertificateList feature is disabled.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/blank_page.html"));
  WaitForInterstitial(tab);

  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
  EXPECT_TRUE(interstitial_timer_observer.timer_started());

  // Check that the histogram for the SSL interstitial was recorded.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::CAPTIVE_PORTAL_CERT_FOUND, 0);
}

// Tests that the captive portal certificate list is used when the feature
// is enabled via Finch. The list is passed to SSLErrorHandler via a proto.
IN_PROC_BROWSER_TEST_F(SSLUICaptivePortalListEnabledTest, Enabled_FromProto) {
  ASSERT_TRUE(https_server_mismatched_.Start());
  base::HistogramTester histograms;

  // Mark the server's cert as a captive portal cert.
  const net::HashValue server_spki_hash =
      GetSPKIHash(https_server_mismatched_.GetCertificate()->cert_buffer());
  SSLErrorHandler::SetErrorAssistantProto(MakeCaptivePortalConfig(
      kLargeVersionId, std::set<std::string>{server_spki_hash.ToString()}));
  ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() > 0);

  // Navigate to an unsafe page on the server. The captive portal interstitial
  // should be displayed since CaptivePortalCertificateList feature is enabled.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/blank_page.html"));
  WaitForInterstitial(tab);

  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingCaptivePortalInterstitial(tab));
  EXPECT_FALSE(interstitial_timer_observer.timer_started());

  // Check that the histogram for the captive portal cert was recorded.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 3);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::CAPTIVE_PORTAL_CERT_FOUND, 1);
}

// Tests the scenario where the OS reports a captive portal. A captive portal
// interstitial should be displayed. The test then switches OS captive portal
// status to false and reloads the page. This time, a normal SSL interstitial
// will be displayed.
IN_PROC_BROWSER_TEST_F(SSLUITest, OSReportsCaptivePortal) {
  ASSERT_TRUE(https_server_mismatched_.Start());
  base::HistogramTester histograms;
  bool netwok_connectivity_reported = false;
  SSLErrorHandler::SetOSReportsCaptivePortalForTesting(true);
  SSLErrorHandler::SetReportNetworkConnectivityCallbackForTesting(
      base::BindLambdaForTesting([&]() {
        SSLErrorHandler::SetOSReportsCaptivePortalForTesting(false);
        netwok_connectivity_reported = true;
      }));

  // Navigate to an unsafe page on the server.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/blank_page.html"));
  WaitForInterstitial(tab);

  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingCaptivePortalInterstitial(tab));
  EXPECT_FALSE(interstitial_timer_observer.timer_started());

  // Check that the histogram for the captive portal cert was recorded.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 3);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::OS_REPORTS_CAPTIVE_PORTAL, 1);

  // Reload the URL. This time the OS should not report a captive portal.
  ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/blank_page.html"));
  WaitForInterstitial(tab);
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
  EXPECT_TRUE(netwok_connectivity_reported);
}

class SSLUITestWithCaptivePortalInterstitialDisabled : public SSLUITest {
 public:
  SSLUITestWithCaptivePortalInterstitialDisabled() {
    feature_list_.InitWithFeatures({} /* enabled */,
                                   {kCaptivePortalInterstitial} /* disabled */);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests the scenario where the OS reports a captive portal but captive portal
// interstitial feature is disabled. A captive portal interstitial should not be
// displayed.
IN_PROC_BROWSER_TEST_F(SSLUITestWithCaptivePortalInterstitialDisabled,
                       OSReportsCaptivePortal_FeatureDisabled) {
  ASSERT_TRUE(https_server_mismatched_.Start());
  base::HistogramTester histograms;

  SSLErrorHandler::SetOSReportsCaptivePortalForTesting(true);

  // Navigate to an unsafe page on the server.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/blank_page.html"));
  WaitForInterstitial(tab);

  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
  EXPECT_FALSE(interstitial_timer_observer.timer_started());

  // Check that the histogram for the SSL interstitial was recorded.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE, 0);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::OS_REPORTS_CAPTIVE_PORTAL, 0);
}

// Tests that the committed interstitial flag triggers the code path to show an
// error PageType instead of an interstitial PageType.
IN_PROC_BROWSER_TEST_F(SSLUITest, ErrorPage) {
  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));

  ssl_test_util::CheckSecurityState(tab, net::CERT_STATUS_DATE_INVALID,
                                    security_state::DANGEROUS,
                                    AuthState::SHOWING_ERROR);

  content::NavigationEntry* entry = tab->GetController().GetVisibleEntry();
  EXPECT_EQ(content::PAGE_TYPE_ERROR, entry->GetPageType());
}

namespace {

// SPKI hash to captive-portal.badssl.com leaf certificate. This
// doesn't match the actual cert (ok_cert.pem) but is good enough for testing.
const char kCaptivePortalSPKI[] =
    "sha256/fjZPHewEHTrMDX3I1ecEIeoy3WFxHyGplOLv28kIbtI=";

// Test class that mimics a URL request with a certificate whose SPKI hash is in
// ssl_error_assistant.asciipb resource. A better way of testing the SPKI hashes
// inside the resource bundle would be to serve the actual certificate from the
// embedded test server, but the test server can only serve a limited number of
// predefined certificates.
class SSLUICaptivePortalListResourceBundleTest
    : public CertVerifierBrowserTest {
 public:
  SSLUICaptivePortalListResourceBundleTest()
      : CertVerifierBrowserTest(),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitWithFeatures(
        {kCaptivePortalCertificateList} /* enabled */, {} /* disabled */);
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  }

  void TearDown() override {
    SSLErrorHandler::ResetConfigForTesting();
    CertVerifierBrowserTest::TearDown();
  }

 protected:
  // Checks that a captive portal interstitial isn't displayed, even though the
  // server's certificate is marked as a captive portal certificate.
  void TestNoCaptivePortalInterstitial(net::CertStatus cert_status,
                                       int net_error) {
    ASSERT_TRUE(https_server()->Start());
    base::HistogramTester histograms;

    // Mark the server's cert as a captive portal cert.
    SetUpCertVerifier(cert_status, net_error, kCaptivePortalSPKI);

    // Navigate to an unsafe page on the server. CaptivePortalCertificateList
    // feature is enabled but either the error is not name-mismatch, or it's not
    // the only error, so a generic SSL interstitial should be displayed.
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    SSLInterstitialTimerObserver interstitial_timer_observer(tab);
    ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));
    WaitForInterstitial(tab);

    ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
    EXPECT_TRUE(interstitial_timer_observer.timer_started());

    // Check that the histogram for the captive portal cert was recorded.
    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                2);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
  }

  void SetUpCertVerifier(net::CertStatus cert_status,
                         int net_result,
                         const std::string& spki_hash) {
    scoped_refptr<net::X509Certificate> cert(https_server_.GetCertificate());
    net::CertVerifyResult verify_result;
    verify_result.is_issued_by_known_root =
        (net_result != net::ERR_CERT_AUTHORITY_INVALID);
    verify_result.verified_cert = cert;
    verify_result.cert_status = cert_status;

    // Set the SPKI hash to captive-portal.badssl.com leaf certificate.
    if (!spki_hash.empty()) {
      net::HashValue hash;
      ASSERT_TRUE(hash.FromString(spki_hash));
      verify_result.public_key_hashes.push_back(hash);
    }
    mock_cert_verifier()->AddResultForCert(cert, verify_result, net_result);
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_;
};

}  // namespace

// Same as CaptivePortalCertificateList_Enabled_FromProto, but this time the
// cert's SPKI hash is listed in ssl_error_assistant.asciipb.
IN_PROC_BROWSER_TEST_F(SSLUICaptivePortalListResourceBundleTest, Enabled) {
  ASSERT_TRUE(https_server()->Start());
  base::HistogramTester histograms;

  // Mark the server's cert as a captive portal cert.
  SetUpCertVerifier(net::CERT_STATUS_COMMON_NAME_INVALID,
                    net::ERR_CERT_COMMON_NAME_INVALID, kCaptivePortalSPKI);

  // Navigate to an unsafe page on the server. The captive portal interstitial
  // should be displayed since CaptivePortalCertificateList feature is enabled.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));
  WaitForInterstitial(tab);

  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingCaptivePortalInterstitial(tab));
  EXPECT_FALSE(interstitial_timer_observer.timer_started());

  // Check that the histogram for the captive portal cert was recorded.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 3);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::CAPTIVE_PORTAL_CERT_FOUND, 1);
}

// Same as SSLUICaptivePortalListResourceBundleTest. Enabled, but this time the
// proto is dynamically updated (e.g. by the component updater). The dynamic
// update should always override the proto loaded from the resource bundle.
IN_PROC_BROWSER_TEST_F(SSLUICaptivePortalListResourceBundleTest,
                       Enabled_DynamicUpdate) {
  ASSERT_TRUE(https_server()->Start());

  // Mark the server's cert as a captive portal cert.
  SetUpCertVerifier(net::CERT_STATUS_COMMON_NAME_INVALID,
                    net::ERR_CERT_COMMON_NAME_INVALID, kCaptivePortalSPKI);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    // Dynamically update the SSL error assistant config, do not include the
    // captive portal SPKI hash.
    SSLErrorHandler::SetErrorAssistantProto(MakeCaptivePortalConfig(
        kLargeVersionId,
        std::set<std::string>{"sha256/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                              "sha256/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"}));
    ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() >
                0);

    // Navigate to an unsafe page on the server. A generic SSL interstitial
    // should be displayed because the dynamic update doesn't contain the hash
    // of the captive portal certificate.
    base::HistogramTester histograms;
    SSLInterstitialTimerObserver interstitial_timer_observer(tab);
    ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));
    WaitForInterstitial(tab);

    ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
    EXPECT_TRUE(interstitial_timer_observer.timer_started());

    // Check that the histogram was recorded for an SSL interstitial.
    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                2);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
  }
  {
    // Dynamically update the error assistant config and add the captive portal
    // SPKI hash.
    SSLErrorHandler::SetErrorAssistantProto(MakeCaptivePortalConfig(
        kLargeVersionId + 1,
        std::set<std::string>{"sha256/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                              "sha256/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
                              kCaptivePortalSPKI}));
    ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() >
                0);

    // Navigate to the unsafe page again. This time, a captive portal
    // interstitial should be displayed.
    base::HistogramTester histograms;
    SSLInterstitialTimerObserver interstitial_timer_observer(tab);
    ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));
    WaitForInterstitial(tab);

    ASSERT_TRUE(
        chrome_browser_interstitials::IsShowingCaptivePortalInterstitial(tab));
    EXPECT_FALSE(interstitial_timer_observer.timer_started());

    // Check that the histogram was recorded for a captive portal interstitial.
    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                3);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE, 1);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::CAPTIVE_PORTAL_CERT_FOUND, 1);
  }
  {
    // Try dynamically updating the error assistant config with an empty config
    // with the same version number. The update should be ignored, and a captive
    // portal interstitial should still be displayed.
    SSLErrorHandler::SetErrorAssistantProto(
        MakeCaptivePortalConfig(kLargeVersionId + 1, std::set<std::string>()));
    ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() >
                0);

    // Navigate to the unsafe page again. This time, a captive portal
    // interstitial should be displayed.
    base::HistogramTester histograms;
    SSLInterstitialTimerObserver interstitial_timer_observer(tab);
    ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));
    WaitForInterstitial(tab);

    ASSERT_TRUE(
        chrome_browser_interstitials::IsShowingCaptivePortalInterstitial(tab));
    EXPECT_FALSE(interstitial_timer_observer.timer_started());

    // Check that the histogram was recorded for a captive portal interstitial.
    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                3);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE, 1);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::CAPTIVE_PORTAL_CERT_FOUND, 1);
  }
}

// Same as SSLUICaptivePortalNameMismatchTest, but this time the error is
// authority-invalid. Captive portal interstitial should not be shown.
IN_PROC_BROWSER_TEST_F(SSLUICaptivePortalListResourceBundleTest,
                       Enabled_AuthorityInvalid) {
  TestNoCaptivePortalInterstitial(net::CERT_STATUS_AUTHORITY_INVALID,
                                  net::ERR_CERT_AUTHORITY_INVALID);
}

// Same as SSLUICaptivePortalListResourceBundleTest.Enabled_AuthorityInvalid,
// but this time there are two errors (name mismatch + weak key). Captive portal
// interstitial should not be shown when name mismatch isn't the only error.
IN_PROC_BROWSER_TEST_F(SSLUICaptivePortalListResourceBundleTest,
                       Enabled_NameMismatchAndWeakKey) {
  const net::CertStatus cert_status =
      net::CERT_STATUS_COMMON_NAME_INVALID | net::CERT_STATUS_WEAK_KEY;
  // Sanity check that COMMON_NAME_INVALID is seen as the net error, since the
  // test is designed to verify that SSLErrorHandler notices other errors in the
  // CertStatus even when COMMON_NAME_INVALID is the net error.
  ASSERT_EQ(net::ERR_CERT_COMMON_NAME_INVALID,
            net::MapCertStatusToNetError(cert_status));
  TestNoCaptivePortalInterstitial(cert_status,
                                  net::ERR_CERT_COMMON_NAME_INVALID);
}

namespace {

char kTestMITMSoftwareName[] = "Misconfigured Firewall";

class SSLUIMITMSoftwareTest : public CertVerifierBrowserTest {
 public:
  SSLUIMITMSoftwareTest()
      : CertVerifierBrowserTest(),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~SSLUIMITMSoftwareTest() override {}

  void SetUpOnMainThread() override {
    CertVerifierBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ssl_test_util::SetHSTSForHostName(browser()->profile(), kHstsTestHostName);
  }

  // Set up the cert verifier to return the error passed in as the cert_error
  // parameter.
  void SetUpCertVerifier(net::CertStatus cert_error) {
    net::CertVerifyResult verify_result;
    verify_result.verified_cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ASSERT_TRUE(verify_result.verified_cert);

    verify_result.cert_status = cert_error;
    mock_cert_verifier()->AddResultForCert(
        https_server()->GetCertificate().get(), verify_result,
        net::MapCertStatusToNetError(cert_error));
  }

  // Sets up an SSLErrorAssistantProto that lists |https_server_|'s default
  // certificate as a MITM software certificate.
  void SetUpMITMSoftwareCertList(uint32_t version_id) {
    auto config_proto =
        std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
    config_proto->set_version_id(version_id);

    chrome_browser_ssl::MITMSoftware* mitm_software =
        config_proto->add_mitm_software();
    mitm_software->set_name(kTestMITMSoftwareName);
    mitm_software->set_issuer_common_name_regex(
        https_server()->GetCertificate().get()->issuer().common_name);
    mitm_software->set_issuer_organization_regex(
        https_server()->GetCertificate().get()->issuer().organization_names[0]);
    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
    ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() >
                0);
  }

  // Returns a URL which triggers an interstitial with the host name that has
  // HSTS set.
  GURL GetHSTSTestURL() const {
    GURL::Replacements replacements;
    replacements.SetHostStr(kHstsTestHostName);
    return https_server()
        ->GetURL("/ssl/blank_page.html")
        .ReplaceComponents(replacements);
  }

  void TestMITMSoftwareInterstitial() {
    base::HistogramTester histograms;
    ASSERT_TRUE(https_server()->Start());
    ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() >
                0);

    // Navigate to an unsafe page on the server. Mock out the URL host name to
    // equal the one set for HSTS.
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    SSLInterstitialTimerObserver interstitial_timer_observer(tab);
    ui_test_utils::NavigateToURL(browser(), GetHSTSTestURL());
    WaitForInterstitial(tab);
    ASSERT_TRUE(chrome_browser_interstitials::IsShowingMITMInterstitial(tab));
    EXPECT_FALSE(interstitial_timer_observer.timer_started());

    // Check that the histograms for the MITM software interstitial were
    // recorded.
    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                2);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 0);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL, 1);
  }

  void TestNoMITMSoftwareInterstitial() {
    base::HistogramTester histograms;
    ASSERT_TRUE(https_server()->Start());
    ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() >
                0);

    // Navigate to an unsafe page on the server. Mock out the URL host name to
    // equal the one set for HSTS.
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    SSLInterstitialTimerObserver interstitial_timer_observer(tab);
    ui_test_utils::NavigateToURL(browser(), GetHSTSTestURL());
    WaitForInterstitial(tab);
    ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
    EXPECT_TRUE(interstitial_timer_observer.timer_started());

    // Check that a MITM software interstitial was not recorded in histogram.
    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                2);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 0);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL, 0);
  }

  // Returns the https server. Guaranteed to be non-NULL.
  const net::EmbeddedTestServer* https_server() const { return &https_server_; }
  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  net::EmbeddedTestServer https_server_;
  DISALLOW_COPY_AND_ASSIGN(SSLUIMITMSoftwareTest);
};

// The SSLUIMITMSoftwareEnabled and Disabled test classes exist so that the
// scoped feature list can be instantiated in the set up method of the class
// rather than in the test itself. Bug crbug.com/713390 was causing some of the
// tests in SSLUIMITMSoftwareTest to be flaky. Refactoring these tests so that
// the scoped feature list initialization is done in the set up method fixes
// this flakiness.

class SSLUIMITMSoftwareEnabledTest : public SSLUIMITMSoftwareTest {
 public:
  SSLUIMITMSoftwareEnabledTest() {
    scoped_feature_list_.InitWithFeatures(
        {kMITMSoftwareInterstitial} /* enabled */, {} /* disabled */);
  }

  ~SSLUIMITMSoftwareEnabledTest() override {}

  void SetUpOnMainThread() override {
    SSLUIMITMSoftwareTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SSLUIMITMSoftwareEnabledTest);
};

class SSLUIMITMSoftwareDisabledTest : public SSLUIMITMSoftwareTest {
 public:
  SSLUIMITMSoftwareDisabledTest() {
    scoped_feature_list_.InitWithFeatures(
        {} /* enabled */, {kMITMSoftwareInterstitial} /* disabled */);
  }

  ~SSLUIMITMSoftwareDisabledTest() override {}

  void SetUpOnMainThread() override {
    SSLUIMITMSoftwareTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SSLUIMITMSoftwareDisabledTest);
};

}  // namespace

// Tests that the MITM software interstitial is not displayed when the feature
// is disabled by Finch.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareDisabledTest, DisabledWithFinch) {
  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  SetUpMITMSoftwareCertList(kLargeVersionId);
  TestNoMITMSoftwareInterstitial();
}

// Tests that the MITM software interstitial is displayed when the feature is
// enabled by Finch.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest, EnabledWithFinch) {
  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  SetUpMITMSoftwareCertList(kLargeVersionId);
  TestMITMSoftwareInterstitial();
}

// Tests that if a certificates matches the common name of a known MITM software
// cert on the list but not the organization name, the MITM software
// interstitial will not be displayed.
IN_PROC_BROWSER_TEST_F(
    SSLUIMITMSoftwareEnabledTest,
    CertificateCommonNameMatchOnly_NoMITMSoftwareInterstitial) {
  base::HistogramTester histograms;

  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  ASSERT_TRUE(https_server()->Start());

  // Set up an error assistant proto with a list of MITM software regexed that
  // the certificate issued by our server won't match.
  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);
  chrome_browser_ssl::MITMSoftware* mitm_software =
      config_proto->add_mitm_software();
  mitm_software->set_name(kTestMITMSoftwareName);
  mitm_software->set_issuer_common_name_regex(
      https_server()->GetCertificate().get()->issuer().common_name);
  mitm_software->set_issuer_organization_regex(
      "pattern-that-does-not-match-anything");
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
  ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
            kLargeVersionId);

  // Navigate to an unsafe page on the server. Mock out the URL host name to
  // equal the one set for HSTS.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ui_test_utils::NavigateToURL(browser(), GetHSTSTestURL());
  WaitForInterstitial(tab);
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
  EXPECT_TRUE(interstitial_timer_observer.timer_started());

  // Check that a MITM software interstitial was not recorded in histogram.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL,
                               0);
}

// Tests that if a certificates matches the organization name of a known MITM
// software cert on the list but not the common name, the MITM software
// interstitial will not be displayed.
IN_PROC_BROWSER_TEST_F(
    SSLUIMITMSoftwareEnabledTest,
    CertificateOrganizationMatchOnly_NoMITMSoftwareInterstitial) {
  base::HistogramTester histograms;

  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  ASSERT_TRUE(https_server()->Start());

  // Set up an error assistant proto with a list of MITM software regexed that
  // the certificate issued by our server won't match.
  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);
  chrome_browser_ssl::MITMSoftware* mitm_software =
      config_proto->add_mitm_software();
  mitm_software->set_name(kTestMITMSoftwareName);
  mitm_software->set_issuer_common_name_regex(
      "pattern-that-does-not-match-anything");
  mitm_software->set_issuer_organization_regex(
      https_server()->GetCertificate().get()->issuer().organization_names[0]);
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
  ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
            kLargeVersionId);

  // Navigate to an unsafe page on the server. Mock out the URL host name to
  // equal the one set for HSTS.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ui_test_utils::NavigateToURL(browser(), GetHSTSTestURL());
  WaitForInterstitial(tab);
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
  EXPECT_TRUE(interstitial_timer_observer.timer_started());

  // Check that a MITM software interstitial was not recorded in histogram.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL,
                               0);
}

// Tests that if the certificate does not match any entry on the list of known
// MITM software, the MITM software interstitial will not be displayed.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest,
                       NonMatchingCertificate_NoMITMSoftwareInterstitial) {
  base::HistogramTester histograms;

  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  ASSERT_TRUE(https_server()->Start());

  // Set up an error assistant proto with a list of MITM software regexes that
  // the certificate issued by our server won't match.
  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);
  chrome_browser_ssl::MITMSoftware* mitm_software =
      config_proto->add_mitm_software();
  mitm_software->set_name("Non-Matching MITM Software");
  mitm_software->set_issuer_common_name_regex(
      "pattern-that-does-not-match-anything");
  mitm_software->set_issuer_organization_regex(
      "pattern-that-does-not-match-anything");
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
  ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
            kLargeVersionId);

  // Navigate to an unsafe page on the server. Mock out the URL host name to
  // equal the one set for HSTS.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ui_test_utils::NavigateToURL(browser(), GetHSTSTestURL());
  WaitForInterstitial(tab);
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
  EXPECT_TRUE(interstitial_timer_observer.timer_started());

  // Check that a MITM software interstitial was not recorded in histogram.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL,
                               0);
}

// Tests that if there is more than one error on the certificate the MITM
// software interstitial will not be displayed.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest,
                       TwoCertErrors_NoMITMSoftwareInterstitial) {
  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID |
                    net::CERT_STATUS_COMMON_NAME_INVALID);
  SetUpMITMSoftwareCertList(kLargeVersionId);
  TestNoMITMSoftwareInterstitial();
}

// Tests that a certificate error other than |CERT_STATUS_AUTHORITY_INVALID|
// will not trigger the MITM software interstitial.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest,
                       WrongCertError_NoMITMSoftwareInterstitial) {
  SetUpCertVerifier(net::CERT_STATUS_COMMON_NAME_INVALID);
  SetUpMITMSoftwareCertList(kLargeVersionId);
  TestNoMITMSoftwareInterstitial();
}

// Tests that if the error on the certificate served is overridable the MITM
// software interstitial will not be displayed.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest,
                       OverridableError_NoMITMSoftwareInterstitial) {
  base::HistogramTester histograms;

  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);

  ASSERT_TRUE(https_server()->Start());
  SetUpMITMSoftwareCertList(kLargeVersionId);

  // Navigate to an unsafe page to trigger an interstitial, but don't replace
  // the host name with the one set for HSTS.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("/ssl/blank_page.html"));
  WaitForInterstitial(tab);
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(tab));
  EXPECT_TRUE(interstitial_timer_observer.timer_started());

  // Check that the histogram for an overridable SSL interstitial was
  // recorded.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL,
                               0);
}

// Tests that the correct strings are displayed on the interstitial in the
// enterprise managed case.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest, EnterpriseManaged) {
  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  ChromeSecurityBlockingPageFactory::SetEnterpriseManagedForTesting(true);

  SetUpMITMSoftwareCertList(kLargeVersionId);
  TestMITMSoftwareInterstitial();

  const std::string expected_primary_paragraph = l10n_util::GetStringFUTF8(
      IDS_MITM_SOFTWARE_PRIMARY_PARAGRAPH_ENTERPRISE,
      net::EscapeForHTML(base::UTF8ToUTF16(kTestMITMSoftwareName)));
  const std::string expected_explanation = l10n_util::GetStringFUTF8(
      IDS_MITM_SOFTWARE_EXPLANATION_ENTERPRISE,
      net::EscapeForHTML(base::UTF8ToUTF16(kTestMITMSoftwareName)),
      l10n_util::GetStringUTF16(IDS_MITM_SOFTWARE_EXPLANATION));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      tab->GetMainFrame(), expected_explanation));
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      tab->GetMainFrame(), expected_primary_paragraph));
}

// Tests that the correct strings are displayed on the interstitial in the
// non-enterprise managed case.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest, NotEnterpriseManaged) {
  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  ChromeSecurityBlockingPageFactory::SetEnterpriseManagedForTesting(false);

  SetUpMITMSoftwareCertList(kLargeVersionId);
  TestMITMSoftwareInterstitial();

  // Don't check the primary paragraph in the non-enterprise case, because it
  // has escaped HTML characters which throw an error.
  const std::string expected_explanation = l10n_util::GetStringFUTF8(
      IDS_MITM_SOFTWARE_EXPLANATION_NONENTERPRISE,
      net::EscapeForHTML(base::UTF8ToUTF16(kTestMITMSoftwareName)),
      l10n_util::GetStringUTF16(IDS_MITM_SOFTWARE_EXPLANATION));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      tab->GetMainFrame(), expected_explanation));
}

// Initialize MITMSoftware certificate list but set the version_id to zero. This
// less than the version_id of the local resource bundle, so the dynamic
// update will be ignored and a non-MITM interstitial will be shown.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest,
                       IgnoreDynamicUpdateWithSmallVersionId) {
  auto config_proto =
      SSLErrorAssistant::GetErrorAssistantProtoFromResourceBundle();
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));

  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  ChromeSecurityBlockingPageFactory::SetEnterpriseManagedForTesting(false);

  SetUpMITMSoftwareCertList(0u);
  TestNoMITMSoftwareInterstitial();
}

class TLSLegacyVersionSSLUITest : public SSLUITest {
 public:
  TLSLegacyVersionSSLUITest() = default;
  ~TLSLegacyVersionSSLUITest() override = default;

  void SetUpOnMainThread() override {
    SSLUITest::SetUpOnMainThread();
    mock_cert_verifier()->set_default_result(net::OK);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  content::ContentMockCertVerifier::CertVerifier* mock_cert_verifier() {
    return mock_cert_verifier_.mock_cert_verifier();
  }

 protected:
  net::EmbeddedTestServer* https_server() { return &https_server_; }

  void SetTLSVersion(uint16_t version) {
    net::SSLServerConfig config;
    config.version_max = version;
    config.version_min = version;
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_OK, config);
  }

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;

  DISALLOW_COPY_AND_ASSIGN(TLSLegacyVersionSSLUITest);
};

// TLS 1.2 should not trigger a warning.
IN_PROC_BROWSER_TEST_F(TLSLegacyVersionSSLUITest, NoWarningTLS12) {
  SetTLSVersion(net::SSL_PROTOCOL_VERSION_TLS1_2);
  ASSERT_TRUE(https_server()->Start());

  GURL url(https_server()->GetURL("/ssl/google.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::ConsoleObserverDelegate console_observer(
      tab, base::StringPrintf(
               "*The connection used to load resources from https://%s:%s*",
               url.host().c_str(), url.port().c_str()));
  tab->SetDelegate(&console_observer);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(std::string(), console_observer.message());
}

// TLS 1.1 should trigger a warning.
IN_PROC_BROWSER_TEST_F(TLSLegacyVersionSSLUITest, WarningTLS11) {
  SetTLSVersion(net::SSL_PROTOCOL_VERSION_TLS1_1);
  ASSERT_TRUE(https_server()->Start());

  GURL url(https_server()->GetURL("/ssl/google.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::ConsoleObserverDelegate console_observer(
      tab, base::StringPrintf(
               "*The connection used to load resources from https://%s:%s*",
               url.host().c_str(), url.port().c_str()));
  tab->SetDelegate(&console_observer);
  ui_test_utils::NavigateToURL(browser(), url);
  console_observer.Wait();
  EXPECT_TRUE(base::MatchPattern(console_observer.message(),
                                 "*will be disabled in the future*"));
  EXPECT_TRUE(base::MatchPattern(console_observer.message(),
                                 "*should enable TLS 1.2 or later*"));
}

// TLS 1.0 should trigger a warning.
IN_PROC_BROWSER_TEST_F(TLSLegacyVersionSSLUITest, WarningTLS1) {
  SetTLSVersion(net::SSL_PROTOCOL_VERSION_TLS1);
  ASSERT_TRUE(https_server()->Start());

  GURL url(https_server()->GetURL("/ssl/google.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::ConsoleObserverDelegate console_observer(
      tab, base::StringPrintf(
               "*The connection used to load resources from https://%s:%s*",
               url.host().c_str(), url.port().c_str()));
  tab->SetDelegate(&console_observer);
  ui_test_utils::NavigateToURL(browser(), url);
  console_observer.Wait();
  EXPECT_TRUE(base::MatchPattern(console_observer.message(),
                                 "*will be disabled in the future*"));
  EXPECT_TRUE(base::MatchPattern(console_observer.message(),
                                 "*should enable TLS 1.2 or later*"));
}

// Warnings should show for subresources, but cap after a limit.
IN_PROC_BROWSER_TEST_F(TLSLegacyVersionSSLUITest, ManySubresources) {
  SetTLSVersion(net::SSL_PROTOCOL_VERSION_TLS1);
  content::SetupCrossSiteRedirector(https_server());
  ASSERT_TRUE(https_server()->Start());
  GURL url(https_server()->GetURL("/ssl/page_with_many_subresources.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Observe the message for a cross-site subresource.
  {
    content::ConsoleObserverDelegate console_observer(tab, "*https://a.test*");
    tab->SetDelegate(&console_observer);
    ui_test_utils::NavigateToURL(browser(), url);
    console_observer.Wait();
    EXPECT_TRUE(base::MatchPattern(console_observer.message(),
                                   "*will be disabled in the future*"));
    EXPECT_TRUE(base::MatchPattern(console_observer.message(),
                                   "*should enable TLS 1.2 or later*"));
  }
  // Observe that the message caps out after some number of subresources.
  {
    content::ConsoleObserverDelegate console_observer(tab,
                                                      "*Additional resources*");
    tab->SetDelegate(&console_observer);
    ui_test_utils::NavigateToURL(browser(), url);
    console_observer.Wait();
    EXPECT_TRUE(base::MatchPattern(console_observer.message(),
                                   "*will be disabled in the future*"));
    EXPECT_TRUE(base::MatchPattern(console_observer.message(),
                                   "*should enable TLS 1.2 or later*"));
  }
}

class LegacyTLSInterstitialTest : public TLSLegacyVersionSSLUITest {
 public:
  LegacyTLSInterstitialTest() {
    feature_list_.InitAndEnableFeature(net::features::kLegacyTLSEnforced);
  }

  void SetUpOnMainThread() override {
    TLSLegacyVersionSSLUITest::SetUpOnMainThread();

    // Set up browser and network service configs to be empty by default.
    InitializeEmptyLegacyTLSConfig();
    base::RunLoop run_loop;
    InitializeEmptyLegacyTLSConfigNetworkService(&run_loop);
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(LegacyTLSInterstitialTest);
};

// When kLegacyTLSEnforcement is enabled, visiting a legacy TLS page should
// show the legacy TLS specific interstitial.
IN_PROC_BROWSER_TEST_F(LegacyTLSInterstitialTest, ShowsInterstitial) {
  SetTLSVersion(net::SSL_PROTOCOL_VERSION_TLS1);
  ASSERT_TRUE(https_server()->Start());
  base::HistogramTester histograms;

  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("/ssl/google.html"));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitial(tab);
  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingLegacyTLSInterstitial(tab));

  // Check that the histogram for the legacy TLS interstitial was recorded.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::SHOW_LEGACY_TLS_INTERSTITIAL,
                               1);

  // Click through the interstitial.
  ProceedThroughInterstitial(tab);

  // Navigating to the same page now shouldn't cause an interstitial.
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("/ssl/google.html"));
  tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingLegacyTLSInterstitial(tab));

  // Check that no new interstitial metrics were recorded from this navigation.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount("interstitial.legacy_tls.decision",
                               security_interstitials::MetricsHelper::PROCEED,
                               1);
}

// When kLegacyTLSEnforcement is enabled but the SSLVersionMin enterprise policy
// is set, the SSLVersionMin policy should also override the version we show the
// legacy TLS interstitial on.
IN_PROC_BROWSER_TEST_F(LegacyTLSInterstitialTest, PolicyOverridesInterstitial) {
  // Set the SSLVersionMin policy and make sure that the network service has
  // received the update.
  std::unique_ptr<base::Value> policy_value =
      std::make_unique<base::Value>("tls1");  // TLS 1.0
  SetPolicy(policy::key::kSSLVersionMin, std::move(policy_value));

  SetTLSVersion(net::SSL_PROTOCOL_VERSION_TLS1);
  ASSERT_TRUE(https_server()->Start());

  base::HistogramTester histograms;

  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("/ssl/google.html"));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingLegacyTLSInterstitial(tab));

  // Interstitial metrics should not have been recorded from this navigation.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 0);
}

// Check that if we have bypassed the legacy TLS error previously and then the
// server responded with TLS 1.2, we drop the error exception.
IN_PROC_BROWSER_TEST_F(LegacyTLSInterstitialTest, FixedServerDropsBypass) {
  int port;  // Save the port used so the different servers can be "identical".

  // Connect over TLS 1.0 and proceed through the interstitial to set an error
  // bypass.
  SetTLSVersion(net::SSL_PROTOCOL_VERSION_TLS1);
  ASSERT_TRUE(https_server()->Start());
  port = https_server()->port();
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("/ssl/google.html"));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitial(tab);
  ProceedThroughInterstitial(tab);
  ASSERT_TRUE(https_server()->ShutdownAndWaitUntilComplete());

  // Connect over a "fixed" TLS 1.2 connection.
  net::EmbeddedTestServer tls12_server(net::EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(tls12_server.Start(port));
  ui_test_utils::NavigateToURL(browser(),
                               tls12_server.GetURL("/ssl/google.html"));
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingLegacyTLSInterstitial(tab));
  ASSERT_TRUE(tls12_server.ShutdownAndWaitUntilComplete());

  // Go back to connecting over TLS 1.0. Visiting should once again show the
  // legacy TLS interstitial
  net::EmbeddedTestServer tls1_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig config;
  config.version_max = net::SSL_PROTOCOL_VERSION_TLS1;
  config.version_min = net::SSL_PROTOCOL_VERSION_TLS1;
  tls1_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, config);
  ASSERT_TRUE(tls1_server.Start(port));
  ui_test_utils::NavigateToURL(browser(),
                               tls1_server.GetURL("/ssl/google.html"));
  WaitForInterstitial(tab);
  EXPECT_TRUE(
      chrome_browser_interstitials::IsShowingLegacyTLSInterstitial(tab));
}

// Check that cliking the "back to safety" button works for the legacy TLS
// interstitial.
IN_PROC_BROWSER_TEST_F(LegacyTLSInterstitialTest, BackToSafety) {
  base::HistogramTester histograms;

  // Connect over TLS 1.0 and don't proceed through the interstitial.
  SetTLSVersion(net::SSL_PROTOCOL_VERSION_TLS1);
  ASSERT_TRUE(https_server()->Start());
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("/ssl/google.html"));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitial(tab);
  DontProceedThroughInterstitial(tab);
  EXPECT_NE(tab->GetVisibleURL(), https_server()->GetURL("/ssl/google.html"));

  // Check that the histogram for the legacy TLS interstitial was recorded.
  histograms.ExpectBucketCount(
      "interstitial.legacy_tls.decision",
      security_interstitials::MetricsHelper::DONT_PROCEED, 1);
}

// Check that we don't show the interstitial for sites in the control set.
IN_PROC_BROWSER_TEST_F(LegacyTLSInterstitialTest, ControlSiteNoInterstitial) {
  InitializeLegacyTLSConfigWithControl();
  base::RunLoop run_loop;
  InitializeLegacyTLSConfigWithControlNetworkService(&run_loop);

  SetTLSVersion(net::SSL_PROTOCOL_VERSION_TLS1);
  ASSERT_TRUE(https_server()->Start());

  base::HistogramTester histograms;

  ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(kLegacyTLSHost, "/ssl/google.html"));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingLegacyTLSInterstitial(tab));

  // Interstitial metrics should not have been recorded from this navigation.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 0);
}

// A WebContentsObserver that allows the user to wait for a navigation, and
// check whether the resource for the navigation was loaded from cache or not.
class CacheNavigationObserver : public content::WebContentsObserver {
 public:
  explicit CacheNavigationObserver(WebContents* web_contents,
                                   bool expect_cached)
      : WebContentsObserver(web_contents), expect_cached_(expect_cached) {}
  ~CacheNavigationObserver() override = default;

  void WaitForNavigation() { run_loop_.Run(); }

  // WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    ASSERT_EQ(expect_cached_, navigation_handle->WasResponseCached());
    run_loop_.Quit();
  }

 private:
  bool expect_cached_;
  base::RunLoop run_loop_;
};

// Tests that resources loaded over legacy TLS should not be cached.
IN_PROC_BROWSER_TEST_F(LegacyTLSInterstitialTest, LegacyTLSPagesNotCached) {
  // Connect over TLS 1.0 and proceed through the interstitial to set an error
  // bypass.
  SetTLSVersion(net::SSL_PROTOCOL_VERSION_TLS1);
  ASSERT_TRUE(https_server()->Start());
  ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/cachetime"));
  auto* tab1 = browser()->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitial(tab1);
  ProceedThroughInterstitial(tab1);

  // Open a new tab and navigate so that resources are fetched via the disk
  // cache. Navigating to the same URL in the same tab triggers a refresh which
  // will not check the disk cache.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  auto* tab2 = browser()->tab_strip_model()->GetActiveWebContents();

  CacheNavigationObserver observer(tab2, false);
  ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/cachetime"));
  observer.WaitForNavigation();  // Will fail if resource is loaded from cache.
}

// Tests that resources from control sites are cached, even if they are loaded
// over legacy TLS.
IN_PROC_BROWSER_TEST_F(LegacyTLSInterstitialTest, ControlSitePagesCached) {
  InitializeLegacyTLSConfigWithControl();
  base::RunLoop run_loop;
  InitializeLegacyTLSConfigWithControlNetworkService(&run_loop);

  // Connect over TLS 1.0 to a site in the control list.
  SetTLSVersion(net::SSL_PROTOCOL_VERSION_TLS1);
  ASSERT_TRUE(https_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(kLegacyTLSHost, "/cachetime"));
  auto* tab1 = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingLegacyTLSInterstitial(tab1));

  // Open a new tab and navigate so that resources are fetched via the disk
  // cache. Navigating to the same URL in the same tab triggers a refresh which
  // will not check the disk cache.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  auto* tab2 = browser()->tab_strip_model()->GetActiveWebContents();

  CacheNavigationObserver observer(tab2, true);
  ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(kLegacyTLSHost, "/cachetime"));
  observer.WaitForNavigation();  // Will fail if not loaded from cache.
}

// Tests that resources loaded over legacy TLS but are control sites should be
// cached.
IN_PROC_BROWSER_TEST_F(LegacyTLSInterstitialTest, NormalPagesCached) {
  // Connect to a non-legacy TLS site.
  ASSERT_TRUE(https_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(kLegacyTLSHost, "/cachetime"));
  auto* tab1 = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingLegacyTLSInterstitial(tab1));

  // Open a new tab and navigate so that resources are fetched via the disk
  // cache. Navigating to the same URL in the same tab triggers a refresh which
  // will not check the disk cache.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  auto* tab2 = browser()->tab_strip_model()->GetActiveWebContents();

  CacheNavigationObserver observer(tab2, true);
  ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(kLegacyTLSHost, "/cachetime"));
  observer.WaitForNavigation();  // Will fail if not loaded from cache.
}

// Tests that a page with legacy TLS and HSTS shows a bypassable interstitial
// rather than a hard non-bypassable HSTS warning.
IN_PROC_BROWSER_TEST_F(LegacyTLSInterstitialTest, LegacyTLSNotFatal) {
  // Set HSTS for the test page.
  ssl_test_util::SetHSTSForHostName(browser()->profile(), kHstsTestHostName);

  // Connect over TLS 1.0 and proceed through the interstitial.
  SetTLSVersion(net::SSL_PROTOCOL_VERSION_TLS1);
  ASSERT_TRUE(https_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(kHstsTestHostName, "/ssl/google.html"));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitial(tab);

  // Verify that there is a proceed link in the interstitial.
  int result = security_interstitials::CMD_ERROR;
  const std::string javascript = base::StringPrintf(
      "domAutomationController.send("
      "(document.querySelector(\"#proceed-link\") === null) "
      "? (%d) : (%d))",
      security_interstitials::CMD_TEXT_NOT_FOUND,
      security_interstitials::CMD_TEXT_FOUND);
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(tab->GetMainFrame(),
                                                  javascript, &result));
  EXPECT_EQ(security_interstitials::CMD_TEXT_FOUND, result);
}

// Tests that the legacy TLS control config applies to subdomains if the
// registrable domain is in the control config.
IN_PROC_BROWSER_TEST_F(LegacyTLSInterstitialTest,
                       ControlConfigIncludesSubdomains) {
  InitializeLegacyTLSConfigWithControl();
  base::RunLoop run_loop;
  InitializeLegacyTLSConfigWithControlNetworkService(&run_loop);

  SetTLSVersion(net::SSL_PROTOCOL_VERSION_TLS1);
  ASSERT_TRUE(https_server()->Start());

  base::HistogramTester histograms;

  ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(std::string("www.") + kLegacyTLSHost,
                                        "/ssl/google.html"));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingLegacyTLSInterstitial(tab));

  // Interstitial metrics should not have been recorded from this navigation.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 0);
}

// Checks that SimpleURLLoader, which uses services/network/url_loader.cc, goes
// through the new NetworkServiceClient interface to deliver cert error
// notifications to the browser which then overrides the certificate error.
IN_PROC_BROWSER_TEST_F(SSLUITest, SimpleURLLoaderCertError) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NO_FATAL_FAILURE(SetUpUnsafeContentsWithUserException(
      "/ssl/page_with_unsafe_contents.html"));
  ssl_test_util::CheckAuthenticationBrokenState(tab, CertError::NONE,
                                                AuthState::NONE);

  content::StoragePartition* partition =
      content::BrowserContext::GetDefaultStoragePartition(browser()->profile());

  auto* frame = tab->GetMainFrame();
  EXPECT_EQ(net::OK,
            content::LoadBasicRequest(
                partition->GetNetworkContext(),
                https_server_mismatched_.GetURL("/anchor_download_test.png"),
                frame->GetProcess()->GetID(), frame->GetRoutingID()));
}

IN_PROC_BROWSER_TEST_F(SSLUITest, NetworkErrorDoesntRevokeExemptions) {
  ASSERT_TRUE(https_server_expired_.Start());
  GURL expired_url = https_server_expired_.GetURL("/title1.html");
  int server_port = expired_url.IntPort();

  // Navigate to the expired cert URL, make sure we get an interstitial.
  ui_test_utils::NavigateToURL(browser(), expired_url);
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));

  // Click through the interstitial.
  ProceedThroughInterstitial(tab);

  // Shut down the server and navigate again to cause a network error.
  ASSERT_TRUE(https_server_expired_.ShutdownAndWaitUntilComplete());
  ui_test_utils::NavigateToURL(browser(), expired_url);

  // Create a new server in the same url (including port), the certificate
  // should still be invalid.
  net::EmbeddedTestServer new_https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  new_https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  new_https_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(new_https_server.Start(server_port));

  ui_test_utils::NavigateToURL(browser(), expired_url);

  // We shouldn't get an interstitial this time.
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(tab));
}

// Checks we don't attempt to show an interstitial (or crash) when visiting an
// SSL error related page in chrome://network-errors. Regression test for
// crbug.com/953812
IN_PROC_BROWSER_TEST_F(SSLUITest, NoInterstitialOnNetworkErrorPage) {
  GURL invalid_cert_url(content::kChromeUINetworkErrorURL);
  GURL::Replacements replacements;
  replacements.SetPathStr("-207");
  invalid_cert_url = invalid_cert_url.ReplaceComponents(replacements);
  ui_test_utils::NavigateToURL(browser(), invalid_cert_url);
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));
}

// This SPKI hash is from a self signed certificate generated using the
// following openssl command:
//  openssl req -new -x509 -keyout server.pem -out server.pem -days 365 -nodes
//  openssl x509 -noout -in certificate.pem -pubkey | \
//  openssl asn1parse -noout -inform pem -out public.key;
//  openssl dgst -sha256 -binary public.key | openssl enc -base64
// The actual value of the hash doesn't matter as long it's a valid SPKI hash.
const char kMatchingDynamicInterstitialCert[] =
    "sha256/eFi0afYJLdI0YsZFu4U8ra2B5/5ynzfKkI88M94iVFA=";

namespace {

class SSLUIDynamicInterstitialTest : public CertVerifierBrowserTest {
 public:
  SSLUIDynamicInterstitialTest()
      : CertVerifierBrowserTest(),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~SSLUIDynamicInterstitialTest() override {}

  void SetUpCertVerifier() {
    scoped_refptr<net::X509Certificate> cert(https_server_.GetCertificate());
    net::CertVerifyResult verify_result;
    verify_result.verified_cert = cert;
    verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

    net::HashValue hash;
    ASSERT_TRUE(hash.FromString(kMatchingDynamicInterstitialCert));
    verify_result.public_key_hashes.push_back(hash);

    mock_cert_verifier()->AddResultForCert(cert, verify_result,
                                           net::ERR_CERT_COMMON_NAME_INVALID);
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  // Creates and returns a SSLErrorAssistantConfig object.
  std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig>
  CreateSSLErrorAssistantConfig() {
    auto config_proto =
        std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
    config_proto->set_version_id(kLargeVersionId);
    return config_proto;
  }

  // Adds a dynamic interstitial to |config_proto|. All of the dynamic
  // interstitial's fields mismatch with |https_server_|'s SSL info.
  void AddMismatchDynamicInterstitial(
      chrome_browser_ssl::SSLErrorAssistantConfig* config_proto) {
    chrome_browser_ssl::DynamicInterstitial* filter =
        config_proto->add_dynamic_interstitial();
    filter->set_interstitial_type(
        chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL);
    filter->set_cert_error(
        chrome_browser_ssl::DynamicInterstitial::ERR_CERT_DATE_INVALID);

    filter->add_sha256_hash("sha256/killdeer");
    filter->add_sha256_hash("sha256/thickkne");

    filter->set_issuer_common_name_regex("beeeater");
    filter->set_issuer_organization_regex("honeycreeper");
    filter->set_mitm_software_name(kTestMITMSoftwareName);
  }

  // Adds a dynamic interstitial to |config_proto| and returns it. All of the
  // fields in the dynamic intersitial matches with |https_server_|'s
  // SSL info. Optionally set the flag for triggering dynamic interstitials
  // only on non-overridable errors.
  chrome_browser_ssl::DynamicInterstitial* AddMatchingDynamicInterstitial(
      chrome_browser_ssl::SSLErrorAssistantConfig* config_proto,
      bool show_only_for_nonoverridable_errors = false) {
    chrome_browser_ssl::DynamicInterstitial* filter =
        config_proto->add_dynamic_interstitial();
    filter->set_interstitial_type(chrome_browser_ssl::DynamicInterstitial::
                                      INTERSTITIAL_PAGE_CAPTIVE_PORTAL);
    filter->set_cert_error(
        chrome_browser_ssl::DynamicInterstitial::ERR_CERT_COMMON_NAME_INVALID);

    filter->add_sha256_hash("sha256/kingfisher");
    filter->add_sha256_hash(kMatchingDynamicInterstitialCert);
    filter->add_sha256_hash("sha256/flycatcher");

    scoped_refptr<net::X509Certificate> cert = https_server_.GetCertificate();
    filter->set_issuer_common_name_regex(cert.get()->issuer().common_name);

    if (!cert.get()->issuer().organization_names.empty()) {
      filter->set_issuer_organization_regex(
          cert.get()->issuer().organization_names[0]);
    }

    filter->set_mitm_software_name(kTestMITMSoftwareName);

    filter->set_support_url("https://google.com");

    filter->set_show_only_for_nonoverridable_errors(
        show_only_for_nonoverridable_errors);

    return filter;
  }

  security_interstitials::SecurityInterstitialPage* GetInterstitialDelegate(
      WebContents* tab) {
    security_interstitials::SecurityInterstitialTabHelper* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            tab);
    if (!helper)
      return nullptr;
    return helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting();
  }

 private:
  net::EmbeddedTestServer https_server_;

  DISALLOW_COPY_AND_ASSIGN(SSLUIDynamicInterstitialTest);
};

}  // namespace

// Tests that the dynamic interstitial list is used when the feature is
// enabled via Finch. The list is passed to SSLErrorHandler via a proto.
IN_PROC_BROWSER_TEST_F(SSLUIDynamicInterstitialTest, Match) {
  ASSERT_TRUE(https_server()->Start());

  SetUpCertVerifier();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> config_proto =
        CreateSSLErrorAssistantConfig();
    config_proto->set_version_id(kLargeVersionId);
    AddMismatchDynamicInterstitial(config_proto.get());
    AddMatchingDynamicInterstitial(config_proto.get());

    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
    ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
              kLargeVersionId);

    ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));
    WaitForInterstitial(tab);

    security_interstitials::SecurityInterstitialPage* interstitial_page =
        GetInterstitialDelegate(tab);
    ASSERT_TRUE(interstitial_page);
    ASSERT_EQ(CaptivePortalBlockingPage::kTypeForTesting,
              interstitial_page->GetTypeForTesting());
  }
}

IN_PROC_BROWSER_TEST_F(SSLUIDynamicInterstitialTest, MatchUnknownCertError) {
  ASSERT_TRUE(https_server()->Start());

  SetUpCertVerifier();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> config_proto =
        CreateSSLErrorAssistantConfig();
    config_proto->set_version_id(kLargeVersionId);
    AddMismatchDynamicInterstitial(config_proto.get());

    // Add a matching dynamic interstitial with the UNKNOWN_CERT_ERROR status.
    chrome_browser_ssl::DynamicInterstitial* match =
        AddMatchingDynamicInterstitial(config_proto.get());
    match->set_cert_error(
        chrome_browser_ssl::DynamicInterstitial::UNKNOWN_CERT_ERROR);

    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
    ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
              kLargeVersionId);

    ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));
    WaitForInterstitial(tab);

    security_interstitials::SecurityInterstitialPage* interstitial_page =
        GetInterstitialDelegate(tab);
    ASSERT_TRUE(interstitial_page);
    ASSERT_EQ(CaptivePortalBlockingPage::kTypeForTesting,
              interstitial_page->GetTypeForTesting());
  }
}

IN_PROC_BROWSER_TEST_F(SSLUIDynamicInterstitialTest,
                       MatchEmptyCommonNameRegex) {
  ASSERT_TRUE(https_server()->Start());

  SetUpCertVerifier();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> config_proto =
        CreateSSLErrorAssistantConfig();
    config_proto->set_version_id(kLargeVersionId);
    AddMismatchDynamicInterstitial(config_proto.get());

    // Add a matching dynamic interstitial with an empty issuer common name
    // regex.
    chrome_browser_ssl::DynamicInterstitial* match =
        AddMatchingDynamicInterstitial(config_proto.get());
    match->set_issuer_common_name_regex(std::string());

    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
    ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
              kLargeVersionId);

    ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));
    WaitForInterstitial(tab);

    security_interstitials::SecurityInterstitialPage* interstitial_page =
        GetInterstitialDelegate(tab);
    ASSERT_TRUE(interstitial_page);
    ASSERT_EQ(CaptivePortalBlockingPage::kTypeForTesting,
              interstitial_page->GetTypeForTesting());
  }
}

IN_PROC_BROWSER_TEST_F(SSLUIDynamicInterstitialTest,
                       MatchEmptyOrganizationRegex) {
  ASSERT_TRUE(https_server()->Start());

  SetUpCertVerifier();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> config_proto =
        CreateSSLErrorAssistantConfig();
    config_proto->set_version_id(kLargeVersionId);
    AddMismatchDynamicInterstitial(config_proto.get());

    // Add a matching dynamic interstitial with an empty issuer organization
    // name regex.
    chrome_browser_ssl::DynamicInterstitial* match =
        AddMatchingDynamicInterstitial(config_proto.get());
    match->set_issuer_organization_regex(std::string());

    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
    ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
              kLargeVersionId);

    ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));
    WaitForInterstitial(tab);

    security_interstitials::SecurityInterstitialPage* interstitial_page =
        GetInterstitialDelegate(tab);
    ASSERT_TRUE(interstitial_page);
    ASSERT_EQ(CaptivePortalBlockingPage::kTypeForTesting,
              interstitial_page->GetTypeForTesting());
  }
}

IN_PROC_BROWSER_TEST_F(SSLUIDynamicInterstitialTest, MismatchHash) {
  ASSERT_TRUE(https_server()->Start());

  SetUpCertVerifier();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> config_proto =
        CreateSSLErrorAssistantConfig();
    config_proto->set_version_id(kLargeVersionId);
    AddMismatchDynamicInterstitial(config_proto.get());

    // Add a dynamic interstitial with matching fields, except for the
    // certificate hashes.
    chrome_browser_ssl::DynamicInterstitial* match =
        AddMatchingDynamicInterstitial(config_proto.get());
    match->clear_sha256_hash();
    match->add_sha256_hash("sha256/sapsucker");
    match->add_sha256_hash("sha256/flowerpiercer");

    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
    ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
              kLargeVersionId);

    ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));
    WaitForInterstitial(tab);

    security_interstitials::SecurityInterstitialPage* interstitial_page =
        GetInterstitialDelegate(tab);
    ASSERT_TRUE(interstitial_page);
    EXPECT_NE(CaptivePortalBlockingPage::kTypeForTesting,
              interstitial_page->GetTypeForTesting());
  }
}

IN_PROC_BROWSER_TEST_F(SSLUIDynamicInterstitialTest, MismatchCertError) {
  ASSERT_TRUE(https_server()->Start());

  SetUpCertVerifier();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> config_proto =
        CreateSSLErrorAssistantConfig();
    config_proto->set_version_id(kLargeVersionId);
    AddMismatchDynamicInterstitial(config_proto.get());

    // Add a dynamic interstitial with matching fields, except for the
    // cert error field.
    chrome_browser_ssl::DynamicInterstitial* match =
        AddMatchingDynamicInterstitial(config_proto.get());
    match->set_cert_error(
        chrome_browser_ssl::DynamicInterstitial::ERR_CERT_DATE_INVALID);

    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
    ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
              kLargeVersionId);

    ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));
    WaitForInterstitial(tab);

    security_interstitials::SecurityInterstitialPage* interstitial_page =
        GetInterstitialDelegate(tab);
    ASSERT_TRUE(interstitial_page);
    EXPECT_NE(CaptivePortalBlockingPage::kTypeForTesting,
              interstitial_page->GetTypeForTesting());
  }
}

IN_PROC_BROWSER_TEST_F(SSLUIDynamicInterstitialTest, MismatchCommonNameRegex) {
  ASSERT_TRUE(https_server()->Start());

  SetUpCertVerifier();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> config_proto =
        CreateSSLErrorAssistantConfig();
    config_proto->set_version_id(kLargeVersionId);
    AddMismatchDynamicInterstitial(config_proto.get());

    // Add a dynamic interstitial with matching fields, except for the
    // issuer common name regex field.
    chrome_browser_ssl::DynamicInterstitial* match =
        AddMatchingDynamicInterstitial(config_proto.get());
    match->set_issuer_common_name_regex("beeeater");

    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
    ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
              kLargeVersionId);

    ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));
    WaitForInterstitial(tab);

    security_interstitials::SecurityInterstitialPage* interstitial_page =
        GetInterstitialDelegate(tab);
    ASSERT_TRUE(interstitial_page);
    EXPECT_NE(CaptivePortalBlockingPage::kTypeForTesting,
              interstitial_page->GetTypeForTesting());
  }
}

IN_PROC_BROWSER_TEST_F(SSLUIDynamicInterstitialTest,
                       MismatchOrganizationRegex) {
  ASSERT_TRUE(https_server()->Start());

  SetUpCertVerifier();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> config_proto =
        CreateSSLErrorAssistantConfig();
    config_proto->set_version_id(kLargeVersionId);
    AddMismatchDynamicInterstitial(config_proto.get());

    // Add a dynamic interstitial with matching fields, except for the
    // issuer organization name regex field.
    chrome_browser_ssl::DynamicInterstitial* match =
        AddMatchingDynamicInterstitial(config_proto.get());
    match->set_issuer_organization_regex("honeycreeper");

    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
    ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
              kLargeVersionId);

    ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));
    WaitForInterstitial(tab);

    security_interstitials::SecurityInterstitialPage* interstitial_page =
        GetInterstitialDelegate(tab);
    ASSERT_TRUE(interstitial_page);
    EXPECT_NE(CaptivePortalBlockingPage::kTypeForTesting,
              interstitial_page->GetTypeForTesting());
  }
}

IN_PROC_BROWSER_TEST_F(SSLUIDynamicInterstitialTest, MismatchWhenOverridable) {
  ASSERT_TRUE(https_server()->Start());

  SetUpCertVerifier();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> config_proto =
        CreateSSLErrorAssistantConfig();
    config_proto->set_version_id(kLargeVersionId);
    AddMismatchDynamicInterstitial(config_proto.get());

    // Add a matching dynamic interstitial, except for the
    // show_only_for_nonoverridable_errors flag is set to true.
    chrome_browser_ssl::DynamicInterstitial* match =
        AddMatchingDynamicInterstitial(config_proto.get(), true);
    match->set_cert_error(
        chrome_browser_ssl::DynamicInterstitial::UNKNOWN_CERT_ERROR);

    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
    ASSERT_EQ(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting(),
              kLargeVersionId);

    ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));
    WaitForInterstitial(tab);

    security_interstitials::SecurityInterstitialPage* interstitial_page =
        GetInterstitialDelegate(tab);
    ASSERT_TRUE(interstitial_page);
    EXPECT_NE(CaptivePortalBlockingPage::kTypeForTesting,
              interstitial_page->GetTypeForTesting());
  }
}

class SSLPKPBrowserTest : public CertVerifierBrowserTest {
 public:
  void SetUpOnMainThread() override {
    CertVerifierBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule(kPreloadedPKPHost, "127.0.0.1");
    host_resolver()->AddRule(kPreloadedReportHost, "127.0.0.1");
  }

  void TearDownOnMainThread() override {
    if (content::IsOutOfProcessNetworkService()) {
      mojo::ScopedAllowSyncCallForTesting allow_sync_call;

      mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
      content::GetNetworkService()->BindTestInterface(
          network_service_test.BindNewPipeAndPassReceiver());
      network_service_test->SetTransportSecurityStateSource(0);
    } else {
      RunOnIOThreadBlocking(base::BindOnce(
          &SSLPKPBrowserTest::CleanUpOnIOThread, base::Unretained(this)));
    }
    CertVerifierBrowserTest::TearDownOnMainThread();
  }

  void EnableStaticPins(int reporting_port) {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    content::StoragePartition* partition =
        content::BrowserContext::GetDefaultStoragePartition(
            browser()->profile());
    partition->GetNetworkContext()->EnableStaticKeyPinningForTesting();
    partition->FlushNetworkInterfaceForTesting();

    if (content::IsOutOfProcessNetworkService()) {
      mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
      content::GetNetworkService()->BindTestInterface(
          network_service_test.BindNewPipeAndPassReceiver());
      network_service_test->SetTransportSecurityStateSource(reporting_port);
    } else {
      // TODO(https://crbug.com/1008175):  This code is not threadsafe, as the
      // network stack does not run on the IO thread. Ideally, the
      // NetworkServiceTest object would be set up in-process on the network
      // service's thread, and this path would be removed.
      RunOnIOThreadBlocking(base::BindOnce(
          &SSLPKPBrowserTest::SetTransportSecurityStateSourceOnIO,
          base::Unretained(this), reporting_port));
    }
  }

 private:
  void RunOnIOThreadBlocking(base::OnceClosure task) {
    base::RunLoop run_loop;
    base::PostTaskAndReply(FROM_HERE, {content::BrowserThread::IO},
                           std::move(task), run_loop.QuitClosure());
    run_loop.Run();
  }

  void SetTransportSecurityStateSourceOnIO(int reporting_port) {
    transport_security_state_source_ =
        std::make_unique<net::ScopedTransportSecurityStateSource>(
            reporting_port);
  }

  void CleanUpOnIOThread() { transport_security_state_source_.reset(); }

  // Only used when NetworkService is disabled. Accessed on IO thread.
  std::unique_ptr<net::ScopedTransportSecurityStateSource>
      transport_security_state_source_;
};

// Test case where a PKP report is sent.
IN_PROC_BROWSER_TEST_F(SSLPKPBrowserTest, SendPKPReport) {
  base::RunLoop wait_for_report_loop;

  // Server that PKP reports are sent to.
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &WaitForJsonRequest, wait_for_report_loop.QuitClosure(), false));
  ASSERT_TRUE(embedded_test_server()->Start());
  EnableStaticPins(embedded_test_server()->port());

  // Server with static key pinning and a report-URI for pin validation
  // failures.
  net::EmbeddedTestServer pkp_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  pkp_test_server.SetSSLConfig(
      net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
  ASSERT_TRUE(pkp_test_server.Start());

  // Set the cert verifier to cause the PKP check to fail.
  net::CertVerifyResult verify_result;
  verify_result.verified_cert = pkp_test_server.GetCertificate();
  net::SHA256HashValue hash = {{0x00, 0x01}};
  verify_result.public_key_hashes.push_back(net::HashValue(hash));
  verify_result.is_issued_by_known_root = true;
  mock_cert_verifier()->AddResultForCertAndHost(
      pkp_test_server.GetCertificate(), kPreloadedPKPHost, verify_result,
      net::OK);

  ui_test_utils::NavigateToURL(browser(),
                               pkp_test_server.GetURL(kPreloadedPKPHost, "/"));
  wait_for_report_loop.Run();

  // Shut down the test server, to make it unlikely this will end up in the same
  // situation as the next test, though it's still theoretically possible.
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
}

// Test case where a PKP report is sent, and the server hasn't replied by the
// time the profile is torn down. Test will crash if the URLRequestContext is
// torn down before the request is torn down.
IN_PROC_BROWSER_TEST_F(SSLPKPBrowserTest, SendPKPReportServerHangs) {
  base::RunLoop wait_for_report_loop;

  // Server that PKP reports are sent to. Have to use a class member to make
  // sure that the test server outlives the IO thread.
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &WaitForJsonRequest, wait_for_report_loop.QuitClosure(), true));
  ASSERT_TRUE(embedded_test_server()->Start());
  EnableStaticPins(embedded_test_server()->port());

  // Server with static key pinning and a report-URI for pin validation
  // failures.
  net::EmbeddedTestServer pkp_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  pkp_test_server.SetSSLConfig(
      net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
  ASSERT_TRUE(pkp_test_server.Start());

  // Set the cert verifier to cause the PKP check to fail.
  net::CertVerifyResult verify_result;
  verify_result.verified_cert = pkp_test_server.GetCertificate();
  net::SHA256HashValue hash = {{0x00, 0x01}};
  verify_result.public_key_hashes.push_back(net::HashValue(hash));
  verify_result.is_issued_by_known_root = true;
  mock_cert_verifier()->AddResultForCertAndHost(
      pkp_test_server.GetCertificate(), kPreloadedPKPHost, verify_result,
      net::OK);

  ui_test_utils::NavigateToURL(browser(),
                               pkp_test_server.GetURL(kPreloadedPKPHost, "/"));
  wait_for_report_loop.Run();
}

class RecurrentInterstitialBrowserTest : public CertVerifierBrowserTest {
 public:
  RecurrentInterstitialBrowserTest() : CertVerifierBrowserTest() {}

  void SetUpOnMainThread() override {
    CertVerifierBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// Tests that a message is added to the interstitial when an error code recurs
// multiple times.
IN_PROC_BROWSER_TEST_F(RecurrentInterstitialBrowserTest,
                       RecurrentInterstitial) {
  const char kRecurrentInterstitialHistogram[] =
      "interstitial.ssl_overridable.is_recurrent_error";
  const char kRecurrentInterstitialActionHistogram[] =
      "interstitial.ssl_recurrent_error.action";

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(https_server.Start());
  mock_cert_verifier()->set_default_result(
      net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED);

  StatefulSSLHostStateDelegate* state =
      reinterpret_cast<StatefulSSLHostStateDelegate*>(
          browser()->profile()->GetSSLHostStateDelegate());
  state->ResetRecurrentErrorCountForTesting();

  base::HistogramTester histograms;
  state->SetRecurrentInterstitialThresholdForTesting(2);

  // Use different hostnames for the two test cases to avoid the clickthrough
  // from one interfering with the other.
  GURL url = https_server.GetURL("show_error_message.test", "/");
  ui_test_utils::NavigateToURL(browser(), url);
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitial(tab);
  ExpectInterstitialElementHidden(tab, "recurrent-error-message",
                                  true /* expect_hidden */);
  histograms.ExpectUniqueSample(kRecurrentInterstitialHistogram, false, 1);

  ui_test_utils::NavigateToURL(browser(), url);
  WaitForInterstitial(tab);
  ExpectInterstitialElementHidden(tab, "recurrent-error-message",
                                  false /* expect_hidden */);
  histograms.ExpectBucketCount(kRecurrentInterstitialHistogram, true, 1);
  histograms.ExpectUniqueSample(
      kRecurrentInterstitialActionHistogram,
      SSLErrorControllerClient::RecurrentErrorAction::kShow, 1);

  // Proceed through the interstitial and observe that the histogram is
  // recorded correctly.
  content::TestNavigationObserver nav_observer(tab, 1);
  ASSERT_TRUE(content::ExecuteScript(
      tab, "window.certificateErrorPageController.proceed();"));
  nav_observer.Wait();
  histograms.ExpectBucketCount(
      kRecurrentInterstitialActionHistogram,
      SSLErrorControllerClient::RecurrentErrorAction::kProceed, 1);
}

// Tests that mixed content is tracked by origin, not by URL. This is tested by
// checking that mixed content flags are set appropriately for about:blank URLs
// (who inherit the origin of their opener).
//
// Note: we test that mixed content flags are propagated from an opener page to
// about:blank, but not the other way around. This is because there is no way
// for a mixed content flag to propagate from about:blank to a different
// tab. Passive mixed content flags are not propagated from one tab to another,
// and for active mixed content, there's no way to bypass mixed content blocking
// on about:blank pages, so there's no way that the origin would get flagged for
// active mixed content from an about:blank page. (There's no way to bypass
// mixed content blocking on about:blank pages because the bypass is implemented
// as a content setting, which doesn't apply to about:blank.)
IN_PROC_BROWSER_TEST_F(SSLUITest, ActiveMixedContentTrackedByOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content.html",
      embedded_test_server()->host_port_pair());

  // The insecure script is allowed to load because SSLUITestBase sets the
  // --allow-running-insecure-content flag.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  ssl_test_util::CheckAuthenticationBrokenState(
      tab, CertError::NONE, AuthState::RAN_INSECURE_CONTENT);

  // Open a new tab from the current page. After an initial navigation,
  // navigate it to about:blank and check that the about:blank page is
  // downgraded, because it shares an origin with |tab| which ran mixed
  // content.
  //
  // Note that the security indicator is not downgraded on the initial
  // about:blank navigation in the new tab. Initial about:blank navigations
  // don't have navigation entries (yet), so there is no way to track the mixed
  // content state for these navigations. See https://crbug.com/1038765.
  ui_test_utils::TabAddedWaiter tab_waiter(browser());
  ASSERT_TRUE(content::ExecJs(tab, "w = window.open()"));
  tab_waiter.Wait();
  WebContents* opened_tab = browser()->tab_strip_model()->GetWebContentsAt(1);
  content::TestNavigationObserver first_navigation(opened_tab);
  ASSERT_TRUE(content::ExecJs(
      tab, content::JsReplace("w.location.href = $1",
                              embedded_test_server()->GetURL("/title1.html"))));
  first_navigation.Wait();
  ssl_test_util::CheckUnauthenticatedState(opened_tab, AuthState::NONE);
  content::TestNavigationObserver about_blank_navigation(opened_tab);
  ASSERT_TRUE(content::ExecJs(tab, "w.location.href = 'about:blank'"));
  about_blank_navigation.Wait();
  ssl_test_util::CheckAuthenticationBrokenState(
      opened_tab, CertError::NONE, AuthState::RAN_INSECURE_CONTENT);
}

// TODO(jcampan): more tests to do below.

// Visit a page over https that contains a frame with a redirect.

// XMLHttpRequest insecure content in synchronous mode.

// XMLHttpRequest insecure content in asynchronous mode.

// XMLHttpRequest over bad ssl in synchronous mode.

// XMLHttpRequest over OK ssl in synchronous mode.
