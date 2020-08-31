// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/interstitials/interstitial_ui.h"

#include <memory>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lookalikes/lookalike_url_blocking_page.h"
#include "chrome/browser/lookalikes/lookalike_url_controller_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_blocking_page_quiet.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/ssl/chrome_security_blocking_page_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/url_constants.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/grit/dev_ui_components_resources.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "components/security_interstitials/content/bad_clock_blocking_page.h"
#include "components/security_interstitials/content/blocked_interception_blocking_page.h"
#include "components/security_interstitials/content/legacy_tls_blocking_page.h"
#include "components/security_interstitials/content/mitm_software_blocking_page.h"
#include "components/security_interstitials/content/origin_policy_ui.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/unsafe_resource_util.h"
#include "components/security_interstitials/core/ssl_error_options_mask.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "crypto/rsa_private_key.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/ssl/ssl_info.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/origin_policy.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "components/security_interstitials/content/captive_portal_blocking_page.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_error_page/supervised_user_error_page.h"
#include "chrome/browser/supervised_user/supervised_user_interstitial.h"
#endif

using security_interstitials::TestSafeBrowsingBlockingPageQuiet;

namespace {

// NSS requires that serial numbers be unique even for the same issuer;
// as all fake certificates will contain the same issuer name, it's
// necessary to ensure the serial number is unique, as otherwise
// NSS will fail to parse.
base::AtomicSequenceNumber g_serial_number;

scoped_refptr<net::X509Certificate> CreateFakeCert() {
  std::unique_ptr<crypto::RSAPrivateKey> unused_key;
  std::string cert_der;
  if (!net::x509_util::CreateKeyAndSelfSignedCert(
          "CN=Error", static_cast<uint32_t>(g_serial_number.GetNext()),
          base::Time::Now() - base::TimeDelta::FromMinutes(5),
          base::Time::Now() + base::TimeDelta::FromMinutes(5), &unused_key,
          &cert_der)) {
    return nullptr;
  }

  return net::X509Certificate::CreateFromBytes(cert_der.data(),
                                               cert_der.size());
}

// Implementation of chrome://interstitials demonstration pages. This code is
// not used in displaying any real interstitials.
class InterstitialHTMLSource : public content::URLDataSource {
 public:
  InterstitialHTMLSource() = default;
  ~InterstitialHTMLSource() override = default;

  // content::URLDataSource:
  std::string GetMimeType(const std::string& mime_type) override;
  std::string GetSource() override;
  std::string GetContentSecurityPolicyScriptSrc() override;
  std::string GetContentSecurityPolicyStyleSrc() override;
  std::string GetContentSecurityPolicyImgSrc() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;

 private:
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  std::string GetSupervisedUserInterstitialHTML(const std::string& path);
#endif

  DISALLOW_COPY_AND_ASSIGN(InterstitialHTMLSource);
};

std::unique_ptr<SSLBlockingPage> CreateSslBlockingPage(
    content::WebContents* web_contents) {
  // Random parameters for SSL blocking page.
  int cert_error = net::ERR_CERT_CONTAINS_ERRORS;
  GURL request_url("https://example.com");
  bool overridable = false;
  bool strict_enforcement = false;
  base::Time time_triggered_ = base::Time::NowFromSystemTime();
  std::string url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "url", &url_param)) {
    if (GURL(url_param).is_valid()) {
      request_url = GURL(url_param);
    }
  }
  std::string overridable_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "overridable",
                                 &overridable_param)) {
    overridable = overridable_param == "1";
  }
  std::string strict_enforcement_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "strict_enforcement",
                                 &strict_enforcement_param)) {
    strict_enforcement = strict_enforcement_param == "1";
  }
  std::string type_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "type", &type_param)) {
    if (type_param == "hpkp_failure") {
      cert_error = net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN;
    } else if (type_param == "ct_failure") {
      cert_error = net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED;
    }
  }
  net::SSLInfo ssl_info;
  ssl_info.cert = ssl_info.unverified_cert = CreateFakeCert();
  // This delegate doesn't create an interstitial.
  int options_mask = 0;
  if (overridable)
    options_mask |=
        security_interstitials::SSLErrorOptionsMask::SOFT_OVERRIDE_ENABLED;
  if (strict_enforcement)
    options_mask |=
        security_interstitials::SSLErrorOptionsMask::STRICT_ENFORCEMENT;
  ChromeSecurityBlockingPageFactory blocking_page_factory;
  return blocking_page_factory.CreateSSLPage(web_contents, cert_error, ssl_info,
                                             request_url, options_mask,
                                             time_triggered_, GURL(), nullptr);
}

std::unique_ptr<MITMSoftwareBlockingPage> CreateMITMSoftwareBlockingPage(
    content::WebContents* web_contents) {
  const int cert_error = net::ERR_CERT_AUTHORITY_INVALID;
  const GURL request_url("https://example.com");
  const std::string mitm_software_name = "Misconfigured Antivirus";
  bool is_enterprise_managed = false;

  std::string is_enterprise_managed_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "enterprise",
                                 &is_enterprise_managed_param)) {
    is_enterprise_managed = is_enterprise_managed_param == "1";
  }

  ChromeSecurityBlockingPageFactory blocking_page_factory;
  blocking_page_factory.SetEnterpriseManagedForTesting(is_enterprise_managed);

  net::SSLInfo ssl_info;
  ssl_info.cert = ssl_info.unverified_cert = CreateFakeCert();
  return blocking_page_factory.CreateMITMSoftwareBlockingPage(
      web_contents, cert_error, request_url, nullptr, ssl_info,
      mitm_software_name);
}

std::unique_ptr<BlockedInterceptionBlockingPage>
CreateBlockedInterceptionBlockingPage(content::WebContents* web_contents) {
  const int cert_error = net::ERR_CERT_AUTHORITY_INVALID;
  const GURL request_url("https://example.com");

  net::SSLInfo ssl_info;
  ssl_info.cert = ssl_info.unverified_cert = CreateFakeCert();
  ChromeSecurityBlockingPageFactory blocking_page_factory;
  return blocking_page_factory.CreateBlockedInterceptionBlockingPage(
      web_contents, cert_error, request_url, nullptr, ssl_info);
}

std::unique_ptr<LegacyTLSBlockingPage> CreateLegacyTLSBlockingPage(
    content::WebContents* web_contents) {
  const int cert_error = net::ERR_SSL_OBSOLETE_VERSION;
  const GURL request_url("https://example.com");

  net::SSLInfo ssl_info;
  ssl_info.cert = ssl_info.unverified_cert = CreateFakeCert();
  ChromeSecurityBlockingPageFactory blocking_page_factory;
  return blocking_page_factory.CreateLegacyTLSBlockingPage(
      web_contents, cert_error, request_url, nullptr, ssl_info);
}

std::unique_ptr<BadClockBlockingPage> CreateBadClockBlockingPage(
    content::WebContents* web_contents) {
  // Set up a fake clock error.
  int cert_error = net::ERR_CERT_DATE_INVALID;
  GURL request_url("https://example.com");
  bool overridable = false;
  bool strict_enforcement = false;
  std::string url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "url", &url_param) &&
      GURL(url_param).is_valid()) {
    request_url = GURL(url_param);
  }
  std::string overridable_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "overridable",
                                 &overridable_param)) {
    overridable = overridable_param == "1";
  }
  std::string strict_enforcement_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "strict_enforcement",
                                 &strict_enforcement_param)) {
    strict_enforcement = strict_enforcement_param == "1";
  }

  // Determine whether to change the clock to be ahead or behind.
  std::string clock_manipulation_param;
  ssl_errors::ClockState clock_state = ssl_errors::CLOCK_STATE_PAST;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "clock_manipulation",
                                 &clock_manipulation_param)) {
    int time_offset;
    if (base::StringToInt(clock_manipulation_param, &time_offset)) {
      clock_state = time_offset > 0 ? ssl_errors::CLOCK_STATE_FUTURE
                                    : ssl_errors::CLOCK_STATE_PAST;
    }
  }

  net::SSLInfo ssl_info;
  ssl_info.cert = ssl_info.unverified_cert = CreateFakeCert();
  // This delegate doesn't create an interstitial.
  int options_mask = 0;
  if (overridable)
    options_mask |=
        security_interstitials::SSLErrorOptionsMask::SOFT_OVERRIDE_ENABLED;
  if (strict_enforcement)
    options_mask |=
        security_interstitials::SSLErrorOptionsMask::STRICT_ENFORCEMENT;
  ChromeSecurityBlockingPageFactory blocking_page_factory;
  return blocking_page_factory.CreateBadClockBlockingPage(
      web_contents, cert_error, ssl_info, request_url, base::Time::Now(),
      clock_state, nullptr);
}

std::unique_ptr<LookalikeUrlBlockingPage> CreateLookalikeInterstitialPage(
    content::WebContents* web_contents) {
  GURL request_url("https://example.net");
  GURL safe_url("https://example.com");

  return std::make_unique<LookalikeUrlBlockingPage>(
      web_contents, safe_url, ukm::kInvalidSourceId,
      LookalikeUrlMatchType::kNone,
      std::make_unique<LookalikeUrlControllerClient>(web_contents, request_url,
                                                     safe_url));
}

std::unique_ptr<safe_browsing::SafeBrowsingBlockingPage>
CreateSafeBrowsingBlockingPage(content::WebContents* web_contents) {
  safe_browsing::SBThreatType threat_type =
      safe_browsing::SB_THREAT_TYPE_URL_MALWARE;
  GURL request_url("http://example.com");
  std::string url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "url", &url_param)) {
    if (GURL(url_param).is_valid()) {
      request_url = GURL(url_param);
    }
  }
  GURL main_frame_url(request_url);
  // TODO(mattm): add flag to change main_frame_url or add dedicated flag to
  // test subresource interstitials.
  std::string type_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "type", &type_param)) {
    if (type_param == "malware") {
      threat_type = safe_browsing::SB_THREAT_TYPE_URL_MALWARE;
    } else if (type_param == "phishing") {
      threat_type = safe_browsing::SB_THREAT_TYPE_URL_PHISHING;
    } else if (type_param == "unwanted") {
      threat_type = safe_browsing::SB_THREAT_TYPE_URL_UNWANTED;
    } else if (type_param == "clientside_malware") {
      threat_type = safe_browsing::SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE;
    } else if (type_param == "clientside_phishing") {
      threat_type = safe_browsing::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING;
    } else if (type_param == "billing") {
      threat_type = safe_browsing::SB_THREAT_TYPE_BILLING;
    }
  }
  safe_browsing::SafeBrowsingBlockingPage::UnsafeResource resource;
  resource.url = request_url;
  resource.is_subresource = request_url != main_frame_url;
  resource.is_subframe = false;
  resource.threat_type = threat_type;
  resource.web_contents_getter = security_interstitials::GetWebContentsGetter(
      web_contents->GetMainFrame()->GetProcess()->GetID(),
      web_contents->GetMainFrame()->GetRoutingID());
  resource.threat_source = g_browser_process->safe_browsing_service()
                               ->database_manager()
                               ->GetThreatSource();

  // Normally safebrowsing interstitial types which block the main page load
  // (SB_THREAT_TYPE_URL_MALWARE, SB_THREAT_TYPE_URL_PHISHING, and
  // SB_THREAT_TYPE_URL_UNWANTED on main-frame loads) would expect there to be a
  // pending navigation when the SafeBrowsingBlockingPage is created. This demo
  // creates a SafeBrowsingBlockingPage but does not actually show a real
  // interstitial. Instead it extracts the html and displays it manually, so the
  // parts which depend on the NavigationEntry are not hit.
  return base::WrapUnique<safe_browsing::SafeBrowsingBlockingPage>(
      safe_browsing::SafeBrowsingBlockingPage::CreateBlockingPage(
          g_browser_process->safe_browsing_service()->ui_manager().get(),
          web_contents, main_frame_url, resource, true));
}

std::unique_ptr<TestSafeBrowsingBlockingPageQuiet>
CreateSafeBrowsingQuietBlockingPage(content::WebContents* web_contents) {
  safe_browsing::SBThreatType threat_type =
      safe_browsing::SB_THREAT_TYPE_URL_MALWARE;
  GURL request_url("http://example.com");
  std::string url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "url", &url_param)) {
    if (GURL(url_param).is_valid())
      request_url = GURL(url_param);
  }
  GURL main_frame_url(request_url);
  std::string type_param;
  bool is_giant_webview = false;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "type", &type_param)) {
    if (type_param == "malware") {
      threat_type = safe_browsing::SB_THREAT_TYPE_URL_MALWARE;
    } else if (type_param == "phishing") {
      threat_type = safe_browsing::SB_THREAT_TYPE_URL_PHISHING;
    } else if (type_param == "unwanted") {
      threat_type = safe_browsing::SB_THREAT_TYPE_URL_UNWANTED;
    } else if (type_param == "billing") {
      threat_type = safe_browsing::SB_THREAT_TYPE_BILLING;
    } else if (type_param == "giant") {
      threat_type = safe_browsing::SB_THREAT_TYPE_URL_MALWARE;
      is_giant_webview = true;
    }
  }
  safe_browsing::SafeBrowsingBlockingPage::UnsafeResource resource;
  resource.url = request_url;
  resource.is_subresource = request_url != main_frame_url;
  resource.is_subframe = false;
  resource.threat_type = threat_type;
  resource.web_contents_getter = security_interstitials::GetWebContentsGetter(
      web_contents->GetMainFrame()->GetProcess()->GetID(),
      web_contents->GetMainFrame()->GetRoutingID());
  resource.threat_source = g_browser_process->safe_browsing_service()
                               ->database_manager()
                               ->GetThreatSource();

  // Normally safebrowsing interstitial types which block the main page load
  // (SB_THREAT_TYPE_URL_MALWARE, SB_THREAT_TYPE_URL_PHISHING, and
  // SB_THREAT_TYPE_URL_UNWANTED on main-frame loads) would expect there to be a
  // pending navigation when the SafeBrowsingBlockingPage is created. This demo
  // creates a SafeBrowsingBlockingPage but does not actually show a real
  // interstitial. Instead it extracts the html and displays it manually, so the
  // parts which depend on the NavigationEntry are not hit.
  return base::WrapUnique<TestSafeBrowsingBlockingPageQuiet>(
      TestSafeBrowsingBlockingPageQuiet::CreateBlockingPage(
          g_browser_process->safe_browsing_service()->ui_manager().get(),
          web_contents, main_frame_url, resource, is_giant_webview));
}

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
std::unique_ptr<CaptivePortalBlockingPage> CreateCaptivePortalBlockingPage(
    content::WebContents* web_contents) {
  bool is_wifi_connection = false;
  GURL landing_url("https://captive.portal/login");
  GURL request_url("https://google.com");
  // Not initialized to a default value, since non-empty wifi_ssid is
  // considered a wifi connection, even if is_wifi_connection is false.
  std::string wifi_ssid;

  std::string request_url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "url",
                                 &request_url_param)) {
    if (GURL(request_url_param).is_valid())
      request_url = GURL(request_url_param);
  }
  std::string landing_url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "landing_page",
                                 &landing_url_param)) {
    if (GURL(landing_url_param).is_valid())
      landing_url = GURL(landing_url_param);
  }
  std::string wifi_connection_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "is_wifi",
                                 &wifi_connection_param)) {
    is_wifi_connection = wifi_connection_param == "1";
  }
  std::string wifi_ssid_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "wifi_name",
                                 &wifi_ssid_param)) {
    wifi_ssid = wifi_ssid_param;
  }
  net::SSLInfo ssl_info;
  ssl_info.cert = ssl_info.unverified_cert = CreateFakeCert();
  ChromeSecurityBlockingPageFactory blocking_page_factory;
  std::unique_ptr<CaptivePortalBlockingPage> blocking_page =
      blocking_page_factory.CreateCaptivePortalBlockingPage(
          web_contents, request_url, landing_url, nullptr, ssl_info,
          net::ERR_CERT_COMMON_NAME_INVALID);
  blocking_page->OverrideWifiInfoForTesting(is_wifi_connection, wifi_ssid);
  return blocking_page;
}
#endif

std::unique_ptr<security_interstitials::SecurityInterstitialPage>
CreateOriginPolicyInterstitialPage(content::WebContents* web_contents) {
  return base::WrapUnique<security_interstitials::SecurityInterstitialPage>(
      security_interstitials::OriginPolicyUI::GetBlockingPage(
          network::OriginPolicyState::kCannotLoadPolicy, web_contents,
          GURL("https://example.com/broken/origin/policy")));
}

}  //  namespace

InterstitialUI::InterstitialUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  content::URLDataSource::Add(Profile::FromWebUI(web_ui),
                              std::make_unique<InterstitialHTMLSource>());
}

InterstitialUI::~InterstitialUI() = default;

// InterstitialHTMLSource

std::string InterstitialHTMLSource::GetMimeType(const std::string& mime_type) {
  return "text/html";
}

std::string InterstitialHTMLSource::GetSource() {
  return chrome::kChromeUIInterstitialHost;
}

std::string InterstitialHTMLSource::GetContentSecurityPolicyScriptSrc() {
  // 'unsafe-inline' is added to script-src.
  return "script-src chrome://resources 'self' 'unsafe-inline';";
}

std::string InterstitialHTMLSource::GetContentSecurityPolicyStyleSrc() {
  return "style-src 'self' 'unsafe-inline';";
}

std::string InterstitialHTMLSource::GetContentSecurityPolicyImgSrc() {
  return "img-src data:;";
}

void InterstitialHTMLSource::StartDataRequest(
    const GURL& request_url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  // TODO(crbug/1009127): Simplify usages of |path| since |request_url| is
  // available.
  const std::string path =
      content::URLDataSource::URLToRequestPath(request_url);
  content::WebContents* web_contents = wc_getter.Run();
  if (!web_contents) {
    // When browser-side navigation is enabled, web_contents can be null if
    // the tab is closing. Nothing to do in this case.
    return;
  }
  std::unique_ptr<security_interstitials::SecurityInterstitialPage>
      interstitial_delegate;
  std::string html;
  // Using this form of the path so we can do exact matching, while ignoring the
  // query (everything after the ? character).
  GURL url =
      GURL(chrome::kChromeUIInterstitialURL).GetWithEmptyPath().Resolve(path);
  std::string path_without_query = url.path();
  if (path_without_query == "/ssl") {
    interstitial_delegate = CreateSslBlockingPage(web_contents);
  } else if (path_without_query == "/mitm-software-ssl") {
    interstitial_delegate = CreateMITMSoftwareBlockingPage(web_contents);
  } else if (path_without_query == "/blocked-interception") {
    interstitial_delegate = CreateBlockedInterceptionBlockingPage(web_contents);
  } else if (path_without_query == "/legacy-tls") {
    interstitial_delegate = CreateLegacyTLSBlockingPage(web_contents);
  } else if (path_without_query == "/safebrowsing") {
    interstitial_delegate = CreateSafeBrowsingBlockingPage(web_contents);
  } else if (path_without_query == "/clock") {
    interstitial_delegate = CreateBadClockBlockingPage(web_contents);
  } else if (path_without_query == "/lookalike") {
    interstitial_delegate = CreateLookalikeInterstitialPage(web_contents);
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  } else if (path_without_query == "/captiveportal") {
    interstitial_delegate = CreateCaptivePortalBlockingPage(web_contents);
#endif
  } else if (path_without_query == "/origin_policy") {
    interstitial_delegate = CreateOriginPolicyInterstitialPage(web_contents);
  }

  if (path_without_query == "/quietsafebrowsing") {
    std::unique_ptr<TestSafeBrowsingBlockingPageQuiet> blocking_page =
        CreateSafeBrowsingQuietBlockingPage(web_contents);
    html = blocking_page->GetHTML();
    interstitial_delegate = std::move(blocking_page);
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  } else if (path_without_query == "/supervised_user") {
    html = GetSupervisedUserInterstitialHTML(path);
#endif
  } else if (interstitial_delegate.get()) {
    html = interstitial_delegate.get()->GetHTMLContents();
  } else {
    html = ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
        IDR_SECURITY_INTERSTITIAL_UI_HTML);
  }
  scoped_refptr<base::RefCountedString> html_bytes = new base::RefCountedString;
  html_bytes->data().assign(html.begin(), html.end());
  std::move(callback).Run(html_bytes.get());
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
std::string InterstitialHTMLSource::GetSupervisedUserInterstitialHTML(
    const std::string& path) {
  GURL url("https://localhost/" + path);

  bool allow_access_requests = true;
  std::string allow_access_requests_string;
  if (net::GetValueForKeyInQuery(url, "allow_access_requests",
                                 &allow_access_requests_string)) {
    allow_access_requests = allow_access_requests_string == "1";
  }

  bool is_child_account = false;
  std::string is_child_account_string;
  if (net::GetValueForKeyInQuery(url, "is_child_account",
                                 &is_child_account_string)) {
    is_child_account = is_child_account_string == "1";
  }

  bool is_deprecated = false;
  std::string is_deprecated_string;
  if (net::GetValueForKeyInQuery(url, "is_deprecated", &is_deprecated_string)) {
    is_deprecated = is_deprecated_string == "1" && !is_child_account;
  }

  std::string custodian;
  net::GetValueForKeyInQuery(url, "custodian", &custodian);
  std::string second_custodian;
  net::GetValueForKeyInQuery(url, "second_custodian", &second_custodian);
  std::string custodian_email;
  net::GetValueForKeyInQuery(url, "custodian_email", &custodian_email);
  std::string second_custodian_email;
  net::GetValueForKeyInQuery(url, "second_custodian_email",
                             &second_custodian_email);
  std::string profile_image_url;
  net::GetValueForKeyInQuery(url, "profile_image_url", &profile_image_url);
  std::string profile_image_url2;
  net::GetValueForKeyInQuery(url, "profile_image_url2", &profile_image_url2);

  supervised_user_error_page::FilteringBehaviorReason reason =
      supervised_user_error_page::DEFAULT;
  std::string reason_string;
  if (net::GetValueForKeyInQuery(url, "reason", &reason_string)) {
    if (reason_string == "safe_sites" && is_child_account) {
      reason = supervised_user_error_page::ASYNC_CHECKER;
    } else if (reason_string == "manual") {
      reason = supervised_user_error_page::MANUAL;
    } else if (reason_string == "not_signed_in") {
      reason = supervised_user_error_page::NOT_SIGNED_IN;
    }
  }

  return supervised_user_error_page::BuildHtml(
      allow_access_requests, profile_image_url, profile_image_url2, custodian,
      custodian_email, second_custodian, second_custodian_email,
      is_child_account, is_deprecated, reason,
      g_browser_process->GetApplicationLocale());
}
#endif
