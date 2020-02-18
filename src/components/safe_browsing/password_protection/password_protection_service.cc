// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/password_protection/password_protection_service.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/history/core/browser/history_service.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/safe_browsing/common/utils.h"
#include "components/safe_browsing/db/database_manager.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/password_protection/password_protection_navigation_throttle.h"
#include "components/safe_browsing/password_protection/password_protection_request.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_zoom.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"

using content::BrowserThread;
using content::WebContents;
using history::HistoryService;
using password_manager::metrics_util::PasswordType;

namespace safe_browsing {

using PasswordReuseEvent = LoginReputationClientRequest::PasswordReuseEvent;

namespace {

// Keys for storing password protection verdict into a DictionaryValue.
const int kRequestTimeoutMs = 10000;
const char kPasswordProtectionRequestUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/login";

}  // namespace

PasswordProtectionService::PasswordProtectionService(
    const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    HistoryService* history_service)
    : database_manager_(database_manager),
      url_loader_factory_(url_loader_factory),
      history_service_observer_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (history_service)
    history_service_observer_.Add(history_service);
}

PasswordProtectionService::~PasswordProtectionService() {
  tracker_.TryCancelAll();
  CancelPendingRequests();
  history_service_observer_.RemoveAll();
  weak_factory_.InvalidateWeakPtrs();
}

bool PasswordProtectionService::CanGetReputationOfURL(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS() || net::IsLocalhost(url))
    return false;

  const std::string hostname = url.HostNoBrackets();
  return !net::IsHostnameNonUnique(hostname) &&
         hostname.find('.') != std::string::npos;
}

bool PasswordProtectionService::ShouldShowModalWarning(
    LoginReputationClientRequest::TriggerType trigger_type,
    PasswordType password_type,
    const std::string& username,
    LoginReputationClientResponse::VerdictType verdict_type) {
  if (trigger_type != LoginReputationClientRequest::PASSWORD_REUSE_EVENT ||
      !IsSupportedPasswordTypeForModalWarning(password_type)) {
    return false;
  }

  // Accounts are not signed in only in tests.
  if ((password_type == PasswordType::PRIMARY_ACCOUNT_PASSWORD &&
       !IsPrimaryAccountSignedIn()) ||
      (password_type == PasswordType::OTHER_GAIA_PASSWORD &&
       !IsOtherGaiaAccountSignedIn(username))) {
    return false;
  }

  return (verdict_type == LoginReputationClientResponse::PHISHING ||
          verdict_type == LoginReputationClientResponse::LOW_REPUTATION) &&
         IsWarningEnabled();
}

LoginReputationClientResponse::VerdictType
PasswordProtectionService::GetCachedVerdict(
    const GURL& url,
    LoginReputationClientRequest::TriggerType trigger_type,
    PasswordType password_type,
    LoginReputationClientResponse* out_response) {
  return LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED;
}

void PasswordProtectionService::CacheVerdict(
    const GURL& url,
    LoginReputationClientRequest::TriggerType trigger_type,
    PasswordType password_type,
    const LoginReputationClientResponse& verdict,
    const base::Time& receive_time) {}

void PasswordProtectionService::StartRequest(
    WebContents* web_contents,
    const GURL& main_frame_url,
    const GURL& password_form_action,
    const GURL& password_form_frame_url,
    const std::string& username,
    PasswordType password_type,
    const std::vector<std::string>& matching_domains,
    LoginReputationClientRequest::TriggerType trigger_type,
    bool password_field_exists) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<PasswordProtectionRequest> request(
      new PasswordProtectionRequest(
          web_contents, main_frame_url, password_form_action,
          password_form_frame_url, username, password_type, matching_domains,
          trigger_type, password_field_exists, this, GetRequestTimeoutInMS()));
  request->Start();
  pending_requests_.insert(std::move(request));
}

void PasswordProtectionService::MaybeStartPasswordFieldOnFocusRequest(
    WebContents* web_contents,
    const GURL& main_frame_url,
    const GURL& password_form_action,
    const GURL& password_form_frame_url,
    const std::string& hosted_domain) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RequestOutcome reason;
  if (CanSendPing(LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                  main_frame_url, PasswordType::PASSWORD_TYPE_UNKNOWN, "",
                  &reason)) {
    StartRequest(web_contents, main_frame_url, password_form_action,
                 password_form_frame_url, /* username */ "",
                 PasswordType::PASSWORD_TYPE_UNKNOWN,
                 {}, /* matching_domains: not used for this type */
                 LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, true);
  }
}

void PasswordProtectionService::MaybeStartProtectedPasswordEntryRequest(
    WebContents* web_contents,
    const GURL& main_frame_url,
    const std::string& username,
    PasswordType password_type,
    const std::vector<std::string>& matching_domains,
    bool password_field_exists) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!IsSupportedPasswordTypeForPinging(password_type))
    return;

  // Collect metrics about typical page-zoom on login pages.
  double zoom_level =
      zoom::ZoomController::GetZoomLevelForWebContents(web_contents);
  UMA_HISTOGRAM_COUNTS_1000(
      "PasswordProtection.PageZoomFactor",
      static_cast<int>(100 * content::ZoomLevelToZoomFactor(zoom_level)));

  RequestOutcome reason;
  if (CanSendPing(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                  main_frame_url, password_type, username, &reason)) {
    StartRequest(web_contents, main_frame_url, GURL(), GURL(), username,
                 password_type, matching_domains,
                 LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                 password_field_exists);
  } else {
    MaybeLogPasswordReuseLookupEvent(web_contents, reason, nullptr);
  }
  if (CanShowInterstitial(reason, password_type, main_frame_url)) {
    ShowInterstitial(web_contents, password_type);
  }
}

bool PasswordProtectionService::CanSendPing(
    LoginReputationClientRequest::TriggerType trigger_type,
    const GURL& main_frame_url,
    PasswordType password_type,
    const std::string& username,
    RequestOutcome* reason) {
  *reason = RequestOutcome::UNKNOWN;
  bool is_pinging_enabled =
      IsPingingEnabled(trigger_type, password_type, username, reason);
  // Pinging is enabled for password_reuse trigger level; however we need to
  // make sure *reason is set appropriately.
  PasswordProtectionTrigger trigger_level =
      GetPasswordProtectionWarningTriggerPref();
  if (trigger_level == PASSWORD_REUSE) {
    *reason = RequestOutcome::PASSWORD_ALERT_MODE;
  }
  if (is_pinging_enabled &&
      !IsURLWhitelistedForPasswordEntry(main_frame_url, reason)) {
    return true;
  }
  LogNoPingingReason(
      trigger_type, *reason,
      GetPasswordProtectionReusedPasswordAccountType(password_type));
  return false;
}

void PasswordProtectionService::RequestFinished(
    PasswordProtectionRequest* request,
    RequestOutcome outcome,
    std::unique_ptr<LoginReputationClientResponse> response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(request);

  if (response) {
    if (outcome != RequestOutcome::RESPONSE_ALREADY_CACHED) {
      CacheVerdict(request->main_frame_url(), request->trigger_type(),
                   request->password_type(), *response, base::Time::Now());
    }
    if (ShouldShowModalWarning(request->trigger_type(),
                               request->password_type(), request->username(),
                               response->verdict_type())) {
      ShowModalWarning(request->web_contents(), response->verdict_token(),
                       request->password_type());
      request->set_is_modal_warning_showing(true);
    }
  }

  request->HandleDeferredNavigations();

  // If the request is canceled, the PasswordProtectionService is already
  // partially destroyed, and we won't be able to log accurate metrics.
  if (outcome != RequestOutcome::CANCELED) {
    auto verdict =
        response ? response->verdict_type()
                 : LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED;
    auto is_phishing_url = verdict == LoginReputationClientResponse::PHISHING;
    MaybeReportPasswordReuseDetected(request->web_contents(),
                                     request->username(),
                                     request->password_type(), is_phishing_url);
  }

  // Remove request from |pending_requests_| list. If it triggers warning, add
  // it into the !warning_reqeusts_| list.
  for (auto it = pending_requests_.begin(); it != pending_requests_.end();
       it++) {
    if (it->get() == request) {
      if (request->is_modal_warning_showing())
        warning_requests_.insert(std::move(request));
      pending_requests_.erase(it);
      break;
    }
  }
}

void PasswordProtectionService::CancelPendingRequests() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto it = pending_requests_.begin(); it != pending_requests_.end();) {
    PasswordProtectionRequest* request = it->get();
    // These are the requests for whom we're still waiting for verdicts.
    // We need to advance the iterator before we cancel because canceling
    // the request will invalidate it when RequestFinished is called.
    it++;
    request->Cancel(false);
  }
  DCHECK(pending_requests_.empty());
}

int PasswordProtectionService::GetStoredVerdictCount(
    LoginReputationClientRequest::TriggerType trigger_type) {
  return -1;
}

scoped_refptr<SafeBrowsingDatabaseManager>
PasswordProtectionService::database_manager() {
  return database_manager_;
}

GURL PasswordProtectionService::GetPasswordProtectionRequestUrl() {
  GURL url(kPasswordProtectionRequestUrl);
  std::string api_key = google_apis::GetAPIKey();
  DCHECK(!api_key.empty());
  return url.Resolve("?key=" + net::EscapeQueryParamValue(api_key, true));
}

int PasswordProtectionService::GetRequestTimeoutInMS() {
  return kRequestTimeoutMs;
}

void PasswordProtectionService::FillUserPopulation(
    LoginReputationClientRequest::TriggerType trigger_type,
    LoginReputationClientRequest* request_proto) {
  ChromeUserPopulation* user_population = request_proto->mutable_population();
  user_population->set_user_population(
      IsExtendedReporting() ? ChromeUserPopulation::EXTENDED_REPORTING
                            : ChromeUserPopulation::SAFE_BROWSING);
  user_population->set_profile_management_status(
      GetProfileManagementStatus(GetBrowserPolicyConnector()));
  user_population->set_is_history_sync_enabled(IsHistorySyncEnabled());
  user_population->set_is_under_advanced_protection(
      IsUnderAdvancedProtection());
  user_population->set_is_incognito(IsIncognito());
}

void PasswordProtectionService::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindRepeating(&PasswordProtectionService::
                              RemoveUnhandledSyncPasswordReuseOnURLsDeleted,
                          GetWeakPtr(), deletion_info.IsAllHistory(),
                          deletion_info.deleted_rows()));
}

void PasswordProtectionService::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  history_service_observer_.RemoveAll();
}

std::unique_ptr<PasswordProtectionNavigationThrottle>
PasswordProtectionService::MaybeCreateNavigationThrottle(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsRendererInitiated())
    return nullptr;

  content::WebContents* web_contents = navigation_handle->GetWebContents();
  for (scoped_refptr<PasswordProtectionRequest> request : pending_requests_) {
    if (request->web_contents() == web_contents &&
        request->trigger_type() ==
            safe_browsing::LoginReputationClientRequest::PASSWORD_REUSE_EVENT &&
        IsSupportedPasswordTypeForModalWarning(request->password_type())) {
      return std::make_unique<PasswordProtectionNavigationThrottle>(
          navigation_handle, request, /*is_warning_showing=*/false);
    }
  }

  for (scoped_refptr<PasswordProtectionRequest> request : warning_requests_) {
    if (request->web_contents() == web_contents) {
      return std::make_unique<PasswordProtectionNavigationThrottle>(
          navigation_handle, request, /*is_warning_showing=*/true);
    }
  }
  return nullptr;
}

void PasswordProtectionService::RemoveWarningRequestsByWebContents(
    content::WebContents* web_contents) {
  for (auto it = warning_requests_.begin(); it != warning_requests_.end();) {
    if (it->get()->web_contents() == web_contents)
      it = warning_requests_.erase(it);
    else
      ++it;
  }
}

bool PasswordProtectionService::IsModalWarningShowingInWebContents(
    content::WebContents* web_contents) {
  for (const auto& request : warning_requests_) {
    if (request->web_contents() == web_contents)
      return true;
  }
  return false;
}

bool PasswordProtectionService::IsWarningEnabled() {
  return GetPasswordProtectionWarningTriggerPref() == PHISHING_REUSE;
}

bool PasswordProtectionService::IsEventLoggingEnabled() {
  // TODO(bdea): Incorporate non-sync users.
  return !IsIncognito() && IsPrimaryAccountSignedIn();
}

// static
ReusedPasswordType
PasswordProtectionService::GetPasswordProtectionReusedPasswordType(
    password_manager::metrics_util::PasswordType password_type) {
  switch (password_type) {
    case PasswordType::SAVED_PASSWORD:
      return PasswordReuseEvent::SAVED_PASSWORD;
    case PasswordType::PRIMARY_ACCOUNT_PASSWORD:
      return PasswordReuseEvent::SIGN_IN_PASSWORD;
    case PasswordType::OTHER_GAIA_PASSWORD:
      return PasswordReuseEvent::OTHER_GAIA_PASSWORD;
    case PasswordType::ENTERPRISE_PASSWORD:
      return PasswordReuseEvent::ENTERPRISE_PASSWORD;
    case PasswordType::PASSWORD_TYPE_UNKNOWN:
      return PasswordReuseEvent::REUSED_PASSWORD_TYPE_UNKNOWN;
    case PasswordType::PASSWORD_TYPE_COUNT:
      break;
  }
  NOTREACHED();
  return PasswordReuseEvent::REUSED_PASSWORD_TYPE_UNKNOWN;
}

// static
ReusedPasswordAccountType
PasswordProtectionService::GetPasswordProtectionReusedPasswordAccountType(
    password_manager::metrics_util::PasswordType password_type) const {
  ReusedPasswordAccountType reused_password_account_type;
  switch (password_type) {
    case PasswordType::SAVED_PASSWORD:
      reused_password_account_type.set_account_type(
          ReusedPasswordAccountType::SAVED_PASSWORD);
      return reused_password_account_type;
    case PasswordType::ENTERPRISE_PASSWORD:
      reused_password_account_type.set_account_type(
          ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
      return reused_password_account_type;
    case PasswordType::PRIMARY_ACCOUNT_PASSWORD:
    case PasswordType::OTHER_GAIA_PASSWORD:
      if (password_type == PasswordType::PRIMARY_ACCOUNT_PASSWORD)
        reused_password_account_type.set_is_account_syncing(
            IsPrimaryAccountSyncing());
      if (!IsPrimaryAccountSignedIn()) {
        reused_password_account_type.set_account_type(
            ReusedPasswordAccountType::UNKNOWN);
        return reused_password_account_type;
      }
      reused_password_account_type.set_account_type(
          IsPrimaryAccountGmail() ? ReusedPasswordAccountType::GMAIL
                                  : ReusedPasswordAccountType::GSUITE);
      return reused_password_account_type;
    case PasswordType::PASSWORD_TYPE_UNKNOWN:
    case PasswordType::PASSWORD_TYPE_COUNT:
      reused_password_account_type.set_account_type(
          ReusedPasswordAccountType::UNKNOWN);
      return reused_password_account_type;
  }
  NOTREACHED();
  return reused_password_account_type;
}

bool PasswordProtectionService::IsSupportedPasswordTypeForPinging(
    PasswordType password_type) const {
  switch (password_type) {
    case PasswordType::SAVED_PASSWORD:
      return true;
    case PasswordType::PRIMARY_ACCOUNT_PASSWORD:
      return true;
    case PasswordType::ENTERPRISE_PASSWORD:
      return true;
    case PasswordType::OTHER_GAIA_PASSWORD:
      return base::FeatureList::IsEnabled(
          safe_browsing::kPasswordProtectionForSignedInUsers);
    case PasswordType::PASSWORD_TYPE_UNKNOWN:
    case PasswordType::PASSWORD_TYPE_COUNT:
      return false;
  }
  NOTREACHED();
  return false;
}

bool PasswordProtectionService::IsSupportedPasswordTypeForModalWarning(
    PasswordType password_type) const {
  if (password_type == PasswordType::OTHER_GAIA_PASSWORD) {
    return base::FeatureList::IsEnabled(
        safe_browsing::kPasswordProtectionForSignedInUsers);
  }
  return password_type == PasswordType::PRIMARY_ACCOUNT_PASSWORD ||
         password_type == PasswordType::ENTERPRISE_PASSWORD;
}

void PasswordProtectionService::GetPhishingDetector(
    service_manager::InterfaceProvider* provider,
    mojom::PhishingDetectorPtr* phishing_detector) {
  provider->GetInterface(phishing_detector);
}

}  // namespace safe_browsing
