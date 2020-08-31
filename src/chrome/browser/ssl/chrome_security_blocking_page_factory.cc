// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_security_blocking_page_factory.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ssl/ssl_error_controller_client.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/common/channel_info.h"
#include "components/security_interstitials/content/content_metrics_helper.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"

#if defined(OS_WIN)
#include "base/enterprise_util.h"
#elif defined(OS_CHROMEOS)
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "components/security_interstitials/content/captive_portal_helper_android.h"
#include "content/public/common/referrer.h"
#include "net/android/network_library.h"
#include "ui/base/window_open_disposition.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_config.h"
#endif

namespace {

enum EnterpriseManaged {
  ENTERPRISE_MANAGED_STATUS_NOT_SET,
  ENTERPRISE_MANAGED_STATUS_TRUE,
  ENTERPRISE_MANAGED_STATUS_FALSE
};

EnterpriseManaged g_is_enterprise_managed_for_testing =
    ENTERPRISE_MANAGED_STATUS_NOT_SET;

bool IsEnterpriseManaged() {
  // Return the value of the testing flag if it's set.
  if (g_is_enterprise_managed_for_testing == ENTERPRISE_MANAGED_STATUS_TRUE) {
    return true;
  }

  if (g_is_enterprise_managed_for_testing == ENTERPRISE_MANAGED_STATUS_FALSE) {
    return false;
  }

#if defined(OS_WIN)
  if (base::IsMachineExternallyManaged()) {
    return true;
  }
#elif defined(OS_CHROMEOS)
  if (g_browser_process->platform_part()->browser_policy_connector_chromeos()) {
    return true;
  }
#endif  // #if defined OS_WIN

  return false;
}

// Opens the login page for a captive portal. Passed in to
// CaptivePortalBlockingPage to be invoked when the user has pressed the
// connect button.
void OpenLoginPage(content::WebContents* web_contents) {
#if !BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  // OpenLoginTabForWebContents() is not available on Android (the only
  // platform on which captive portal detection is not enabled). Simply open
  // the platform's portal detection URL in a new tab.
  const std::string url = security_interstitials::GetCaptivePortalServerUrl(
      base::android::AttachCurrentThread());
  content::OpenURLParams params(GURL(url), content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  web_contents->OpenURL(params);
#else
  ChromeSecurityBlockingPageFactory::OpenLoginTabForWebContents(web_contents,
                                                                true);
#endif  // !BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
}

std::unique_ptr<ContentMetricsHelper> CreateMetricsHelperAndStartRecording(
    content::WebContents* web_contents,
    const GURL& request_url,
    const std::string& metric_prefix,
    bool overridable) {
  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  reporting_info.metric_prefix = metric_prefix;
  std::unique_ptr<ContentMetricsHelper> metrics_helper =
      std::make_unique<ContentMetricsHelper>(
          HistoryServiceFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext()),
              ServiceAccessType::EXPLICIT_ACCESS),
          request_url, reporting_info);
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  metrics_helper.get()->StartRecordingCaptivePortalMetrics(
      CaptivePortalServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      overridable);
#endif
  return metrics_helper;
}

}  // namespace

std::unique_ptr<SSLBlockingPage>
ChromeSecurityBlockingPageFactory::CreateSSLPage(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    int options_mask,
    const base::Time& time_triggered,
    const GURL& support_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter) {
  bool overridable = SSLBlockingPage::IsOverridable(options_mask);
  std::unique_ptr<ContentMetricsHelper> metrics_helper(
      CreateMetricsHelperAndStartRecording(
          web_contents, request_url,
          overridable ? "ssl_overridable" : "ssl_nonoverridable", overridable));

  StatefulSSLHostStateDelegate* state =
      StatefulSSLHostStateDelegateFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  state->DidDisplayErrorPage(cert_error);
  bool is_recurrent_error = state->HasSeenRecurrentErrors(cert_error);
  if (overridable) {
    UMA_HISTOGRAM_BOOLEAN("interstitial.ssl_overridable.is_recurrent_error",
                          is_recurrent_error);
    if (cert_error == net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED) {
      UMA_HISTOGRAM_BOOLEAN(
          "interstitial.ssl_overridable.is_recurrent_error.ct_error",
          is_recurrent_error);
    }
  } else {
    UMA_HISTOGRAM_BOOLEAN("interstitial.ssl_nonoverridable.is_recurrent_error",
                          is_recurrent_error);
    if (cert_error == net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED) {
      UMA_HISTOGRAM_BOOLEAN(
          "interstitial.ssl_nonoverridable.is_recurrent_error.ct_error",
          is_recurrent_error);
    }
  }

  auto controller_client = std::make_unique<SSLErrorControllerClient>(
      web_contents, ssl_info, cert_error, request_url,
      std::move(metrics_helper));

  std::unique_ptr<SSLBlockingPage> page;

  page = std::make_unique<SSLBlockingPage>(
      web_contents, cert_error, ssl_info, request_url, options_mask,
      time_triggered, support_url, std::move(ssl_cert_reporter), overridable,
      std::move(controller_client));

  DoChromeSpecificSetup(page.get());
  return page;
}

std::unique_ptr<CaptivePortalBlockingPage>
ChromeSecurityBlockingPageFactory::CreateCaptivePortalBlockingPage(
    content::WebContents* web_contents,
    const GURL& request_url,
    const GURL& login_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    const net::SSLInfo& ssl_info,
    int cert_error) {
  auto page = std::make_unique<CaptivePortalBlockingPage>(
      web_contents, request_url, login_url, std::move(ssl_cert_reporter),
      ssl_info,
      std::make_unique<SSLErrorControllerClient>(
          web_contents, ssl_info, cert_error, request_url,
          CreateMetricsHelperAndStartRecording(web_contents, request_url,
                                               "captive_portal", false)),
      base::BindRepeating(&OpenLoginPage));

  DoChromeSpecificSetup(page.get());
  return page;
}

std::unique_ptr<BadClockBlockingPage>
ChromeSecurityBlockingPageFactory::CreateBadClockBlockingPage(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    const base::Time& time_triggered,
    ssl_errors::ClockState clock_state,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter) {
  auto page = std::make_unique<BadClockBlockingPage>(
      web_contents, cert_error, ssl_info, request_url, time_triggered,
      clock_state, std::move(ssl_cert_reporter),
      std::make_unique<SSLErrorControllerClient>(
          web_contents, ssl_info, cert_error, request_url,
          CreateMetricsHelperAndStartRecording(web_contents, request_url,
                                               "bad_clock", false)));

  ChromeSecurityBlockingPageFactory::DoChromeSpecificSetup(page.get());
  return page;
}

std::unique_ptr<LegacyTLSBlockingPage>
ChromeSecurityBlockingPageFactory::CreateLegacyTLSBlockingPage(
    content::WebContents* web_contents,
    int cert_error,
    const GURL& request_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    const net::SSLInfo& ssl_info) {
  auto page = std::make_unique<LegacyTLSBlockingPage>(
      web_contents, cert_error, request_url, std::move(ssl_cert_reporter),
      ssl_info,
      std::make_unique<SSLErrorControllerClient>(
          web_contents, ssl_info, cert_error, request_url,
          CreateMetricsHelperAndStartRecording(web_contents, request_url,
                                               "legacy_tls", false)));

  DoChromeSpecificSetup(page.get());
  return page;
}

std::unique_ptr<MITMSoftwareBlockingPage>
ChromeSecurityBlockingPageFactory::CreateMITMSoftwareBlockingPage(
    content::WebContents* web_contents,
    int cert_error,
    const GURL& request_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    const net::SSLInfo& ssl_info,
    const std::string& mitm_software_name) {
  auto page = std::make_unique<MITMSoftwareBlockingPage>(
      web_contents, cert_error, request_url, std::move(ssl_cert_reporter),
      ssl_info, mitm_software_name, IsEnterpriseManaged(),
      std::make_unique<SSLErrorControllerClient>(
          web_contents, ssl_info, cert_error, request_url,
          CreateMetricsHelperAndStartRecording(web_contents, request_url,
                                               "mitm_software", false)));

  DoChromeSpecificSetup(page.get());
  return page;
}

std::unique_ptr<BlockedInterceptionBlockingPage>
ChromeSecurityBlockingPageFactory::CreateBlockedInterceptionBlockingPage(
    content::WebContents* web_contents,
    int cert_error,
    const GURL& request_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    const net::SSLInfo& ssl_info) {
  auto page = std::make_unique<BlockedInterceptionBlockingPage>(
      web_contents, cert_error, request_url, std::move(ssl_cert_reporter),
      ssl_info,
      std::make_unique<SSLErrorControllerClient>(
          web_contents, ssl_info, cert_error, request_url,
          CreateMetricsHelperAndStartRecording(web_contents, request_url,
                                               "blocked_interception", false)));

  ChromeSecurityBlockingPageFactory::DoChromeSpecificSetup(page.get());
  return page;
}

// static
void ChromeSecurityBlockingPageFactory::DoChromeSpecificSetup(
    SSLBlockingPageBase* page) {
  page->cert_report_helper()->set_client_details_callback(
      base::BindRepeating([](CertificateErrorReport* report) {
        report->AddChromeChannel(chrome::GetChannel());

#if defined(OS_WIN)
        report->SetIsEnterpriseManaged(base::IsMachineExternallyManaged());
#elif defined(OS_CHROMEOS)
        report->SetIsEnterpriseManaged(g_browser_process->platform_part()
                                           ->browser_policy_connector_chromeos()
                                           ->IsEnterpriseManaged());
#endif

        // TODO(estade): this one is probably necessary for all clients, and
        // should be enforced (e.g. via a pure virtual method) rather than
        // optional.
        report->AddNetworkTimeInfo(g_browser_process->network_time_tracker());
      }));
}

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
// static
void ChromeSecurityBlockingPageFactory::OpenLoginTabForWebContents(
    content::WebContents* web_contents,
    bool focus) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);

  // If the Profile doesn't have a tabbed browser window open, do nothing.
  if (!browser)
    return;

  SecureDnsConfig secure_dns_config =
      SystemNetworkContextManager::GetStubResolverConfigReader()
          ->GetSecureDnsConfiguration(
              false /* force_check_parental_controls */);

  // If the DNS mode is SECURE, captive portal login tabs should be opened in
  // new popup windows where secure DNS will be disabled.
  if (secure_dns_config.mode() == net::DnsConfig::SecureDnsMode::SECURE) {
    // If there is already a captive portal popup window, do not create another.
    for (auto* contents : AllTabContentses()) {
      captive_portal::CaptivePortalTabHelper* captive_portal_tab_helper =
          captive_portal::CaptivePortalTabHelper::FromWebContents(contents);
      if (captive_portal_tab_helper->IsLoginTab()) {
        Browser* browser_with_login_tab =
            chrome::FindBrowserWithWebContents(contents);
        browser_with_login_tab->window()->Show();
        browser_with_login_tab->tab_strip_model()->ActivateTabAt(
            browser_with_login_tab->tab_strip_model()->GetIndexOfWebContents(
                contents));
        return;
      }
    }

    // Otherwise, create a captive portal popup window.
    NavigateParams params(
        browser,
        CaptivePortalServiceFactory::GetForProfile(browser->profile())
            ->test_url(),
        ui::PAGE_TRANSITION_TYPED);
    params.disposition = WindowOpenDisposition::NEW_POPUP;
    params.is_captive_portal_popup = true;
    Navigate(&params);
    content::WebContents* new_contents = params.navigated_or_inserted_contents;
    captive_portal::CaptivePortalTabHelper* captive_portal_tab_helper =
        captive_portal::CaptivePortalTabHelper::FromWebContents(new_contents);
    captive_portal_tab_helper->SetIsLoginTab();
    return;
  }

  // Check if the Profile's topmost browser window already has a login tab.
  // If so, do nothing.
  // TODO(mmenke):  Consider focusing that tab, at least if this is the tab
  //                helper for the currently active tab for the profile.
  for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
    content::WebContents* contents =
        browser->tab_strip_model()->GetWebContentsAt(i);
    captive_portal::CaptivePortalTabHelper* captive_portal_tab_helper =
        captive_portal::CaptivePortalTabHelper::FromWebContents(contents);
    if (captive_portal_tab_helper->IsLoginTab()) {
      if (focus)
        browser->tab_strip_model()->ActivateTabAt(i);
      return;
    }
  }

  // Otherwise, open a login tab.  Only end up here when a captive portal result
  // was received, so it's safe to assume profile has a
  // captive_portal::CaptivePortalService.
  content::WebContents* new_contents = chrome::AddSelectedTabWithURL(
      browser,
      CaptivePortalServiceFactory::GetForProfile(browser->profile())
          ->test_url(),
      ui::PAGE_TRANSITION_TYPED);
  captive_portal::CaptivePortalTabHelper* captive_portal_tab_helper =
      captive_portal::CaptivePortalTabHelper::FromWebContents(new_contents);
  captive_portal_tab_helper->SetIsLoginTab();
}
#endif  // BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)

void ChromeSecurityBlockingPageFactory::SetEnterpriseManagedForTesting(
    bool enterprise_managed) {
  if (enterprise_managed) {
    g_is_enterprise_managed_for_testing = ENTERPRISE_MANAGED_STATUS_TRUE;
  } else {
    g_is_enterprise_managed_for_testing = ENTERPRISE_MANAGED_STATUS_FALSE;
  }
}
