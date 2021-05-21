// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/chrome_web_client.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#import "base/ios/ios_util.h"
#import "base/ios/ns_error_util.h"
#include "base/mac/bundle_locations.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_about_rewriter.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/ios_chrome_main_parts.h"
#import "ios/chrome/browser/reading_list/offline_page_tab_helper.h"
#import "ios/chrome/browser/safe_browsing/password_protection_java_script_feature.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_error.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_unsafe_resource_container.h"
#include "ios/chrome/browser/ssl/ios_ssl_error_handler.h"
#import "ios/chrome/browser/ui/elements/windowed_container_view.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/web/error_page_controller_bridge.h"
#import "ios/chrome/browser/web/error_page_util.h"
#include "ios/chrome/browser/web/features.h"
#import "ios/chrome/browser/web/java_script_console/java_script_console_feature.h"
#import "ios/chrome/browser/web/java_script_console/java_script_console_feature_factory.h"
#include "ios/chrome/browser/web/print/print_java_script_feature.h"
#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#import "ios/components/security_interstitials/legacy_tls/legacy_tls_blocking_page.h"
#import "ios/components/security_interstitials/legacy_tls/legacy_tls_controller_client.h"
#import "ios/components/security_interstitials/legacy_tls/legacy_tls_tab_allow_list.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_blocking_page.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_container.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_controller_client.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_error.h"
#include "ios/components/webui/web_ui_url_constants.h"
#import "ios/net/protocol_handler_util.h"
#include "ios/public/provider/chrome/browser/browser_url_rewriter_provider.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/font_size_java_script_feature.h"
#include "ios/public/provider/chrome/browser/voice/audio_session_controller.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_provider.h"
#include "ios/web/common/features.h"
#include "ios/web/common/user_agent.h"
#include "ios/web/public/navigation/browser_url_rewriter.h"
#include "ios/web/public/navigation/navigation_manager.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The tag describing the product name with a placeholder for the version.
const char kProductTagWithPlaceholder[] = "CriOS/%s";

// Returns an autoreleased string containing the JavaScript loaded from a
// bundled resource file with the given name (excluding extension).
NSString* GetPageScript(NSString* script_file_name) {
  DCHECK(script_file_name);
  NSString* path =
      [base::mac::FrameworkBundle() pathForResource:script_file_name
                                             ofType:@"js"];
  DCHECK(path) << "Script file not found: "
               << base::SysNSStringToUTF8(script_file_name) << ".js";
  NSError* error = nil;
  NSString* content = [NSString stringWithContentsOfFile:path
                                                encoding:NSUTF8StringEncoding
                                                   error:&error];
  DCHECK(!error) << "Error fetching script: "
                 << base::SysNSStringToUTF8(error.description);
  DCHECK(content);
  return content;
}
// Returns the safe browsing error page HTML.
NSString* GetSafeBrowsingErrorPageHTML(web::WebState* web_state,
                                       int64_t navigation_id) {
  // Fetch the unsafe resource causing this error page from the WebState's
  // container.
  SafeBrowsingUnsafeResourceContainer* container =
      SafeBrowsingUnsafeResourceContainer::FromWebState(web_state);
  const security_interstitials::UnsafeResource* resource =
      container->GetMainFrameUnsafeResource()
          ?: container->GetSubFrameUnsafeResource(
                 web_state->GetNavigationManager()->GetLastCommittedItem());

  // Construct the blocking page and associate it with the WebState.
  std::unique_ptr<security_interstitials::IOSSecurityInterstitialPage> page =
      SafeBrowsingBlockingPage::Create(*resource);
  std::string error_page_content = page->GetHtmlContents();
  security_interstitials::IOSBlockingPageTabHelper::FromWebState(web_state)
      ->AssociateBlockingPage(navigation_id, std::move(page));

  return base::SysUTF8ToNSString(error_page_content);
}

// Returns the lookalike error page HTML.
NSString* GetLookalikeUrlErrorPageHtml(web::WebState* web_state,
                                       int64_t navigation_id) {
  // Fetch the lookalike URL info from the WebState's container.
  LookalikeUrlContainer* container =
      LookalikeUrlContainer::FromWebState(web_state);
  std::unique_ptr<LookalikeUrlContainer::LookalikeUrlInfo> lookalike_info =
      container->ReleaseLookalikeUrlInfo();

  // Construct the blocking page and associate it with the WebState.
  std::unique_ptr<security_interstitials::IOSSecurityInterstitialPage> page =
      std::make_unique<LookalikeUrlBlockingPage>(
          web_state, lookalike_info->safe_url, lookalike_info->request_url,
          ukm::ConvertToSourceId(navigation_id,
                                 ukm::SourceIdType::NAVIGATION_ID),
          lookalike_info->match_type,
          std::make_unique<LookalikeUrlControllerClient>(
              web_state, lookalike_info->safe_url, lookalike_info->request_url,
              GetApplicationContext()->GetApplicationLocale()));
  std::string error_page_content = page->GetHtmlContents();
  security_interstitials::IOSBlockingPageTabHelper::FromWebState(web_state)
      ->AssociateBlockingPage(navigation_id, std::move(page));

  return base::SysUTF8ToNSString(error_page_content);
}

// Returns the legacy TLS error page HTML.
NSString* GetLegacyTLSErrorPageHTML(web::WebState* web_state,
                                    int64_t navigation_id) {
  std::string error_page_content;
  security_interstitials::IOSBlockingPageTabHelper* blocking_page_tab_helper =
      security_interstitials::IOSBlockingPageTabHelper::FromWebState(web_state);

  // WebStates that are not in the WebStateList (e.g., WebStates used for
  // reading list sync) do not have an IOSBlockingPageTabHelper. Since such
  // WebStates are not used for displaying web contents to a user, it is not
  // necessary to produce an actual error page, and instead an empty string is
  // used.
  if (blocking_page_tab_helper) {
    // Construct the blocking page and associate it with the WebState.
    std::unique_ptr<security_interstitials::IOSSecurityInterstitialPage> page =
        std::make_unique<LegacyTLSBlockingPage>(
            web_state, web_state->GetVisibleURL() /*request_url*/,
            std::make_unique<LegacyTLSControllerClient>(
                web_state, web_state->GetVisibleURL(),
                GetApplicationContext()->GetApplicationLocale()));
    error_page_content = page->GetHtmlContents();
    blocking_page_tab_helper->AssociateBlockingPage(navigation_id,
                                                    std::move(page));
  }

  return base::SysUTF8ToNSString(error_page_content);
}

// Returns a string describing the product name and version, of the
// form "productname/version". Used as part of the user agent string.
std::string GetMobileProduct() {
  return base::StringPrintf(kProductTagWithPlaceholder,
                            version_info::GetVersionNumber().c_str());
}

// Returns a string describing the product name and version, of the
// form "productname/version". Used as part of the user agent string.
// The Desktop UserAgent is only using the major version to reduce the surface
// for fingerprinting. The Mobile one is using the full version for legacy
// reasons.
std::string GetDesktopProduct() {
  return base::StringPrintf(kProductTagWithPlaceholder,
                            version_info::GetMajorVersionNumber().c_str());
}

}  // namespace

ChromeWebClient::ChromeWebClient() {}

ChromeWebClient::~ChromeWebClient() {}

std::unique_ptr<web::WebMainParts> ChromeWebClient::CreateWebMainParts() {
  return std::make_unique<IOSChromeMainParts>(
      *base::CommandLine::ForCurrentProcess());
}

void ChromeWebClient::PreWebViewCreation() const {
  // TODO(crbug.com/1082371): Confirm that this code is no longer needed and
  // remove it entirely. Until then, prevent this from running on iOS 13.4+, as
  // it occasionally triggers a permissions prompt.
  if (!base::ios::IsRunningOnOrLater(13, 4, 0)) {
    // Initialize the audio session to allow a web page's audio to continue
    // playing after the app is backgrounded.
    VoiceSearchProvider* voice_provider =
        ios::GetChromeBrowserProvider()->GetVoiceSearchProvider();
    if (voice_provider) {
      AudioSessionController* audio_controller =
          voice_provider->GetAudioSessionController();
      if (audio_controller) {
        audio_controller->InitializeSessionIfNecessary();
      }
    }
  }
}

void ChromeWebClient::AddAdditionalSchemes(Schemes* schemes) const {
  schemes->standard_schemes.push_back(kChromeUIScheme);
  schemes->secure_schemes.push_back(kChromeUIScheme);
}

std::string ChromeWebClient::GetApplicationLocale() const {
  DCHECK(GetApplicationContext());
  return GetApplicationContext()->GetApplicationLocale();
}

bool ChromeWebClient::IsAppSpecificURL(const GURL& url) const {
  return url.SchemeIs(kChromeUIScheme);
}

void ChromeWebClient::AddSerializableData(
    web::SerializableUserDataManager* user_data_manager,
    web::WebState* web_state) {
  return ios::GetChromeBrowserProvider()->AddSerializableData(user_data_manager,
                                                              web_state);
}

std::u16string ChromeWebClient::GetPluginNotSupportedText() const {
  return l10n_util::GetStringUTF16(IDS_PLUGIN_NOT_SUPPORTED);
}

std::string ChromeWebClient::GetUserAgent(web::UserAgentType type) const {
  // The user agent should not be requested for app-specific URLs.
  DCHECK_NE(type, web::UserAgentType::NONE);

  // Using desktop user agent overrides a command-line user agent, so that
  // request desktop site can still work when using an overridden UA.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (type != web::UserAgentType::DESKTOP &&
      command_line->HasSwitch(switches::kUserAgent)) {
    std::string user_agent =
        command_line->GetSwitchValueASCII(switches::kUserAgent);
    if (net::HttpUtil::IsValidHeaderValue(user_agent))
      return user_agent;
    LOG(WARNING) << "Ignored invalid value for flag --" << switches::kUserAgent;
  }

  if (type == web::UserAgentType::DESKTOP)
    return web::BuildDesktopUserAgent(GetDesktopProduct());
  return web::BuildMobileUserAgent(GetMobileProduct());
}

std::u16string ChromeWebClient::GetLocalizedString(int message_id) const {
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece ChromeWebClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* ChromeWebClient::GetDataResourceBytes(
    int resource_id) const {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

void ChromeWebClient::GetAdditionalWebUISchemes(
    std::vector<std::string>* additional_schemes) {
  additional_schemes->push_back(dom_distiller::kDomDistillerScheme);
}

void ChromeWebClient::PostBrowserURLRewriterCreation(
    web::BrowserURLRewriter* rewriter) {
  rewriter->AddURLRewriter(&WillHandleWebBrowserAboutURL);
  BrowserURLRewriterProvider* provider =
      ios::GetChromeBrowserProvider()->GetBrowserURLRewriterProvider();
  if (provider)
    provider->AddProviderRewriters(rewriter);
}

std::vector<web::JavaScriptFeature*> ChromeWebClient::GetJavaScriptFeatures(
    web::BrowserState* browser_state) const {
  static base::NoDestructor<PrintJavaScriptFeature> print_feature;
  std::vector<web::JavaScriptFeature*> features;
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordReuseDetectionEnabled) &&
      base::ios::IsRunningOnIOS14OrLater()) {
    features.push_back(PasswordProtectionJavaScriptFeature::GetInstance());
  }

  JavaScriptConsoleFeature* java_script_console_feature =
      JavaScriptConsoleFeatureFactory::GetInstance()->GetForBrowserState(
          browser_state);
  features.push_back(java_script_console_feature);

  features.push_back(print_feature.get());

  features.push_back(autofill::AutofillJavaScriptFeature::GetInstance());
  features.push_back(FontSizeJavaScriptFeature::GetInstance());

  return features;
}

NSString* ChromeWebClient::GetDocumentStartScriptForAllFrames(
    web::BrowserState* browser_state) const {
  return GetPageScript(@"chrome_bundle_all_frames");
}

NSString* ChromeWebClient::GetDocumentStartScriptForMainFrame(
    web::BrowserState* browser_state) const {
  NSMutableArray* scripts = [NSMutableArray array];
  [scripts addObject:GetPageScript(@"chrome_bundle_main_frame")];

  return [scripts componentsJoinedByString:@";"];
}

bool ChromeWebClient::IsLegacyTLSAllowedForHost(web::WebState* web_state,
                                                const std::string& hostname) {
  auto* allowlist = LegacyTLSTabAllowList::FromWebState(web_state);
  if (!allowlist)
    return false;
  return allowlist->IsDomainAllowed(hostname);
}

void ChromeWebClient::PrepareErrorPage(
    web::WebState* web_state,
    const GURL& url,
    NSError* error,
    bool is_post,
    bool is_off_the_record,
    const base::Optional<net::SSLInfo>& info,
    int64_t navigation_id,
    base::OnceCallback<void(NSString*)> callback) {
  OfflinePageTabHelper* offline_page_tab_helper =
      OfflinePageTabHelper::FromWebState(web_state);
  // WebState that are not attached to a tab may not have an
  // OfflinePageTabHelper.
  if (offline_page_tab_helper &&
      (offline_page_tab_helper->CanHandleErrorLoadingURL(url))) {
    // An offline version of the page will be displayed to replace this error
    // page. Loading an error page here can cause a race between the
    // navigation to load the error page and the navigation to display the
    // offline version of the page. If the latter navigation interrupts the
    // former and causes it to fail, this can incorrectly appear to be a
    // navigation back to the previous committed URL. To avoid this race,
    // return a nil error page here to avoid an error page load. See
    // crbug.com/980912.
    std::move(callback).Run(nil);
    return;
  }
  DCHECK(error);
  __block NSString* error_html = nil;
  __block base::OnceCallback<void(NSString*)> error_html_callback =
      std::move(callback);
  NSError* final_underlying_error =
      base::ios::GetFinalUnderlyingErrorFromError(error);
  if ([final_underlying_error.domain isEqual:kSafeBrowsingErrorDomain]) {
    // Only kUnsafeResourceErrorCode is supported.
    DCHECK_EQ(kUnsafeResourceErrorCode, final_underlying_error.code);
    std::move(error_html_callback)
        .Run(GetSafeBrowsingErrorPageHTML(web_state, navigation_id));
  } else if ([final_underlying_error.domain isEqual:kLookalikeUrlErrorDomain]) {
    // Only kLookalikeUrlErrorCode is supported.
    DCHECK_EQ(kLookalikeUrlErrorCode, final_underlying_error.code);
    std::move(error_html_callback)
        .Run(GetLookalikeUrlErrorPageHtml(web_state, navigation_id));
  } else if ([final_underlying_error.domain isEqual:net::kNSErrorDomain] &&
             final_underlying_error.code == net::ERR_SSL_OBSOLETE_VERSION) {
    std::move(error_html_callback)
        .Run(GetLegacyTLSErrorPageHTML(web_state, navigation_id));
  } else if (info.has_value()) {
    base::OnceCallback<void(bool)> proceed_callback;
    base::OnceCallback<void(NSString*)> blocking_page_callback =
        base::BindOnce(^(NSString* blocking_page_html) {
          error_html = blocking_page_html;
          std::move(error_html_callback).Run(error_html);
        });
    IOSSSLErrorHandler::HandleSSLError(
        web_state, net::MapCertStatusToNetError(info.value().cert_status),
        info.value(), url, info.value().is_fatal_cert_error, navigation_id,
        std::move(proceed_callback), std::move(blocking_page_callback));
  } else {
    std::move(error_html_callback)
        .Run(GetErrorPage(url, error, is_post, is_off_the_record));
    ErrorPageControllerBridge* error_page_controller =
        ErrorPageControllerBridge::FromWebState(web_state);
    if (error_page_controller) {
      // ErrorPageControllerBridge may not be created for web_state not attached
      // to a tab.
      error_page_controller->StartHandlingJavascriptCommands();
    }
  }
}

UIView* ChromeWebClient::GetWindowedContainer() {
  if (!windowed_container_) {
    windowed_container_ = [[WindowedContainerView alloc] init];
  }
  return windowed_container_;
}

bool ChromeWebClient::EnableLongPressAndForceTouchHandling() const {
  return !web::features::UseWebViewNativeContextMenuWeb();
}

bool ChromeWebClient::EnableLongPressUIContextMenu() const {
  return web::features::UseWebViewNativeContextMenuSystem();
}

bool ChromeWebClient::ForceMobileVersionByDefault(const GURL& url) {
  DCHECK(web::features::UseWebClientDefaultUserAgent());
  if (base::FeatureList::IsEnabled(web::kMobileGoogleSRP)) {
    return google_util::IsGoogleSearchUrl(url);
  }
  return false;
}

web::UserAgentType ChromeWebClient::GetDefaultUserAgent(
    id<UITraitEnvironment> web_view,
    const GURL& url) {
  DCHECK(web::features::UseWebClientDefaultUserAgent());
  if (ForceMobileVersionByDefault(url))
    return web::UserAgentType::MOBILE;
  BOOL isRegularRegular = web_view.traitCollection.horizontalSizeClass ==
                              UIUserInterfaceSizeClassRegular &&
                          web_view.traitCollection.verticalSizeClass ==
                              UIUserInterfaceSizeClassRegular;
  return isRegularRegular ? web::UserAgentType::DESKTOP
                          : web::UserAgentType::MOBILE;
}
