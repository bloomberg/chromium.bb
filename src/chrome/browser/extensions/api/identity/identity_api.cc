// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_api.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/identity/identity_constants.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/extensions/api/identity.h"
#include "chrome/common/url_constants.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/manifest_handlers/oauth2_manifest_handler.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/gurl.h"

namespace extensions {

namespace {
const char kIdentityGaiaIdPref[] = "identity_gaia_id";
}

IdentityTokenCacheValue::IdentityTokenCacheValue() = default;
IdentityTokenCacheValue::IdentityTokenCacheValue(
    const IdentityTokenCacheValue& other) = default;
IdentityTokenCacheValue::~IdentityTokenCacheValue() = default;

// static
IdentityTokenCacheValue IdentityTokenCacheValue::CreateIssueAdvice(
    const IssueAdviceInfo& issue_advice) {
  IdentityTokenCacheValue cache_value;
  cache_value.status_ = CACHE_STATUS_ADVICE;
  cache_value.issue_advice_ = issue_advice;
  cache_value.expiration_time_ =
      base::Time::Now() + base::TimeDelta::FromSeconds(
                              identity_constants::kCachedIssueAdviceTTLSeconds);
  return cache_value;
}

// static
IdentityTokenCacheValue IdentityTokenCacheValue::CreateRemoteConsent(
    const RemoteConsentResolutionData& resolution_data) {
  IdentityTokenCacheValue cache_value;
  cache_value.status_ = CACHE_STATUS_REMOTE_CONSENT;
  cache_value.resolution_data_ = resolution_data;
  cache_value.expiration_time_ =
      base::Time::Now() + base::TimeDelta::FromSeconds(
                              identity_constants::kCachedIssueAdviceTTLSeconds);
  return cache_value;
}

// static
IdentityTokenCacheValue IdentityTokenCacheValue::CreateRemoteConsentApproved(
    const std::string& consent_result) {
  IdentityTokenCacheValue cache_value;
  cache_value.status_ = CACHE_STATUS_REMOTE_CONSENT_APPROVED;
  cache_value.consent_result_ = consent_result;
  cache_value.expiration_time_ =
      base::Time::Now() + base::TimeDelta::FromSeconds(
                              identity_constants::kCachedIssueAdviceTTLSeconds);
  return cache_value;
}

// static
IdentityTokenCacheValue IdentityTokenCacheValue::CreateToken(
    const std::string& token,
    base::TimeDelta time_to_live) {
  IdentityTokenCacheValue cache_value;
  cache_value.status_ = CACHE_STATUS_TOKEN;
  cache_value.token_ = token;

  // Remove 20 minutes from the ttl so cached tokens will have some time
  // to live any time they are returned.
  time_to_live -= base::TimeDelta::FromMinutes(20);

  base::TimeDelta zero_delta;
  if (time_to_live < zero_delta)
    time_to_live = zero_delta;

  cache_value.expiration_time_ = base::Time::Now() + time_to_live;
  return cache_value;
}

IdentityTokenCacheValue::CacheValueStatus IdentityTokenCacheValue::status()
    const {
  if (is_expired())
    return IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND;
  else
    return status_;
}

const IssueAdviceInfo& IdentityTokenCacheValue::issue_advice() const {
  return issue_advice_;
}

const RemoteConsentResolutionData& IdentityTokenCacheValue::resolution_data()
    const {
  return resolution_data_;
}

const std::string& IdentityTokenCacheValue::consent_result() const {
  return consent_result_;
}

const std::string& IdentityTokenCacheValue::token() const { return token_; }

bool IdentityTokenCacheValue::is_expired() const {
  return status_ == CACHE_STATUS_NOTFOUND ||
         expiration_time_ < base::Time::Now();
}

const base::Time& IdentityTokenCacheValue::expiration_time() const {
  return expiration_time_;
}

IdentityAPI::IdentityAPI(content::BrowserContext* context)
    : IdentityAPI(Profile::FromBrowserContext(context),
                  IdentityManagerFactory::GetForProfile(
                      Profile::FromBrowserContext(context)),
                  ExtensionPrefs::Get(context),
                  EventRouter::Get(context)) {}

IdentityAPI::~IdentityAPI() {}

IdentityMintRequestQueue* IdentityAPI::mint_queue() { return &mint_queue_; }

void IdentityAPI::SetCachedToken(const ExtensionTokenKey& key,
                                 const IdentityTokenCacheValue& token_data) {
  auto it = token_cache_.find(key);
  if (it != token_cache_.end() && it->second.status() <= token_data.status())
    token_cache_.erase(it);

  token_cache_.insert(std::make_pair(key, token_data));
}

void IdentityAPI::EraseCachedToken(const std::string& extension_id,
                                   const std::string& token) {
  CachedTokens::iterator it;
  for (it = token_cache_.begin(); it != token_cache_.end(); ++it) {
    if (it->first.extension_id == extension_id &&
        it->second.status() == IdentityTokenCacheValue::CACHE_STATUS_TOKEN &&
        it->second.token() == token) {
      token_cache_.erase(it);
      break;
    }
  }
}

void IdentityAPI::EraseAllCachedTokens() { token_cache_.clear(); }

const IdentityTokenCacheValue& IdentityAPI::GetCachedToken(
    const ExtensionTokenKey& key) {
  return token_cache_[key];
}

const IdentityAPI::CachedTokens& IdentityAPI::GetAllCachedTokens() {
  return token_cache_;
}

void IdentityAPI::SetGaiaIdForExtension(const std::string& extension_id,
                                        const std::string& gaia_id) {
  DCHECK(!gaia_id.empty());
  extension_prefs_->UpdateExtensionPref(extension_id, kIdentityGaiaIdPref,
                                        std::make_unique<base::Value>(gaia_id));
}

base::Optional<std::string> IdentityAPI::GetGaiaIdForExtension(
    const std::string& extension_id) {
  std::string gaia_id;
  if (!extension_prefs_->ReadPrefAsString(extension_id, kIdentityGaiaIdPref,
                                          &gaia_id)) {
    return base::nullopt;
  }
  return gaia_id;
}

void IdentityAPI::EraseGaiaIdForExtension(const std::string& extension_id) {
  extension_prefs_->UpdateExtensionPref(extension_id, kIdentityGaiaIdPref,
                                        nullptr);
}

void IdentityAPI::EraseStaleGaiaIdsForAllExtensions() {
  // Refresh tokens haven't been loaded yet. Wait for OnRefreshTokensLoaded() to
  // fire.
  if (!identity_manager_->AreRefreshTokensLoaded())
    return;
  std::vector<CoreAccountInfo> accounts =
      identity_manager_->GetAccountsWithRefreshTokens();
  extensions::ExtensionIdList extensions;
  extension_prefs_->GetExtensions(&extensions);
  for (const ExtensionId& extension_id : extensions) {
    base::Optional<std::string> gaia_id = GetGaiaIdForExtension(extension_id);
    if (!gaia_id)
      continue;
    auto account_it = std::find_if(accounts.begin(), accounts.end(),
                                   [&](const CoreAccountInfo& account) {
                                     return account.gaia == *gaia_id;
                                   });
    if (account_it == accounts.end()) {
      EraseGaiaIdForExtension(extension_id);
    }
  }
}

void IdentityAPI::SetConsentResult(const std::string& result,
                                   const std::string& window_id) {
  on_set_consent_result_callback_list_.Notify(result, window_id);
}

std::unique_ptr<
    base::CallbackList<IdentityAPI::OnSetConsentResultSignature>::Subscription>
IdentityAPI::RegisterOnSetConsentResultCallback(
    const base::RepeatingCallback<OnSetConsentResultSignature>& callback) {
  return on_set_consent_result_callback_list_.Add(callback);
}

void IdentityAPI::Shutdown() {
  on_shutdown_callback_list_.Notify();
  identity_manager_->RemoveObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<IdentityAPI>>::
    DestructorAtExit g_identity_api_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<IdentityAPI>* IdentityAPI::GetFactoryInstance() {
  return g_identity_api_factory.Pointer();
}

std::unique_ptr<base::CallbackList<void()>::Subscription>
IdentityAPI::RegisterOnShutdownCallback(const base::Closure& cb) {
  return on_shutdown_callback_list_.Add(cb);
}

bool IdentityAPI::AreExtensionsRestrictedToPrimaryAccount() {
  return !AccountConsistencyModeManager::IsDiceEnabledForProfile(profile_) &&
         !AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile_);
}

IdentityAPI::IdentityAPI(Profile* profile,
                         signin::IdentityManager* identity_manager,
                         ExtensionPrefs* extension_prefs,
                         EventRouter* event_router)
    : profile_(profile),
      identity_manager_(identity_manager),
      extension_prefs_(extension_prefs),
      event_router_(event_router) {
  identity_manager_->AddObserver(this);
  EraseStaleGaiaIdsForAllExtensions();
}

void IdentityAPI::OnRefreshTokensLoaded() {
  EraseStaleGaiaIdsForAllExtensions();
}

void IdentityAPI::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  // Refresh tokens are sometimes made available in contexts where
  // AccountTrackerService is not tracking the account in question. Bail out in
  // these cases.
  if (account_info.gaia.empty())
    return;

  FireOnAccountSignInChanged(account_info.gaia, true);
}

void IdentityAPI::OnExtendedAccountInfoRemoved(
    const AccountInfo& account_info) {
  DCHECK(!account_info.gaia.empty());
  EraseStaleGaiaIdsForAllExtensions();
  FireOnAccountSignInChanged(account_info.gaia, false);
}

void IdentityAPI::FireOnAccountSignInChanged(const std::string& gaia_id,
                                             bool is_signed_in) {
  DCHECK(!gaia_id.empty());
  api::identity::AccountInfo api_account_info;
  api_account_info.id = gaia_id;

  std::unique_ptr<base::ListValue> args =
      api::identity::OnSignInChanged::Create(api_account_info, is_signed_in);
  std::unique_ptr<Event> event(new Event(
      events::IDENTITY_ON_SIGN_IN_CHANGED,
      api::identity::OnSignInChanged::kEventName, std::move(args), profile_));

  if (on_signin_changed_callback_for_testing_)
    on_signin_changed_callback_for_testing_.Run(event.get());

  event_router_->BroadcastEvent(std::move(event));
}

template <>
void BrowserContextKeyedAPIFactory<IdentityAPI>::DeclareFactoryDependencies() {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());

  DependsOn(ChromeSigninClientFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

}  // namespace extensions
