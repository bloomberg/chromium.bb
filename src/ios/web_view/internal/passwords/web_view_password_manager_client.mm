// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/web_view_password_manager_client.h"

#include <memory>
#include <utility>

#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/password_form.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/ios/credential_manager_util.h"
#import "ios/web_view/internal/passwords/web_view_account_password_store_factory.h"
#import "ios/web_view/internal/passwords/web_view_password_manager_log_router_factory.h"
#include "ios/web_view/internal/passwords/web_view_password_store_factory.h"
#include "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/sync/web_view_profile_sync_service_factory.h"
#include "net/cert/cert_status_flags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::PasswordFormManagerForUI;
using password_manager::PasswordManagerMetricsRecorder;
using password_manager::PasswordStore;
using password_manager::SyncState;

namespace ios_web_view {

// static
std::unique_ptr<WebViewPasswordManagerClient>
WebViewPasswordManagerClient::Create(web::WebState* web_state,
                                     WebViewBrowserState* browser_state) {
  syncer::SyncService* sync_service =
      ios_web_view::WebViewProfileSyncServiceFactory::GetForBrowserState(
          browser_state);
  signin::IdentityManager* identity_manager =
      ios_web_view::WebViewIdentityManagerFactory::GetForBrowserState(
          browser_state);
  autofill::LogRouter* logRouter =
      ios_web_view::WebViewPasswordManagerLogRouterFactory::GetForBrowserState(
          browser_state);
  auto log_manager =
      autofill::LogManager::Create(logRouter, base::RepeatingClosure());
  scoped_refptr<password_manager::PasswordStore> profile_store =
      ios_web_view::WebViewPasswordStoreFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);
  scoped_refptr<password_manager::PasswordStore> account_store =
      ios_web_view::WebViewAccountPasswordStoreFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);
  return std::make_unique<ios_web_view::WebViewPasswordManagerClient>(
      web_state, sync_service, browser_state->GetPrefs(), identity_manager,
      std::move(log_manager), profile_store.get(), account_store.get());
}

WebViewPasswordManagerClient::WebViewPasswordManagerClient(
    web::WebState* web_state,
    syncer::SyncService* sync_service,
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    std::unique_ptr<autofill::LogManager> log_manager,
    PasswordStore* profile_store,
    PasswordStore* account_store)
    : web_state_(web_state),
      sync_service_(sync_service),
      pref_service_(pref_service),
      identity_manager_(identity_manager),
      log_manager_(std::move(log_manager)),
      profile_store_(profile_store),
      account_store_(account_store),
      password_feature_manager_(pref_service, sync_service),
      credentials_filter_(
          this,
          base::Bind(&WebViewPasswordManagerClient::GetSyncService,
                     base::Unretained(this))),
      helper_(this) {
  saving_passwords_enabled_.Init(
      password_manager::prefs::kCredentialsEnableService, GetPrefs());
}

WebViewPasswordManagerClient::~WebViewPasswordManagerClient() = default;

SyncState WebViewPasswordManagerClient::GetPasswordSyncState() const {
  return password_manager_util::GetPasswordSyncState(sync_service_);
}

bool WebViewPasswordManagerClient::PromptUserToChooseCredentials(
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
    const GURL& origin,
    const CredentialsCallback& callback) {
  NOTIMPLEMENTED();
  return false;
}

bool WebViewPasswordManagerClient::PromptUserToSaveOrUpdatePassword(
    std::unique_ptr<PasswordFormManagerForUI> form_to_save,
    bool update_password) {
  if (form_to_save->IsBlacklisted()) {
    return false;
  }
  if (!password_feature_manager_.IsOptedInForAccountStorage()) {
    return false;
  }

  if (update_password) {
    [delegate_ showUpdatePasswordInfoBar:std::move(form_to_save)];
  } else {
    [delegate_ showSavePasswordInfoBar:std::move(form_to_save)];
  }

  return true;
}

void WebViewPasswordManagerClient::PromptUserToMovePasswordToAccount(
    std::unique_ptr<PasswordFormManagerForUI> form_to_move) {
  NOTIMPLEMENTED();
}

bool WebViewPasswordManagerClient::ShowOnboarding(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save) {
  return false;
}

void WebViewPasswordManagerClient::ShowManualFallbackForSaving(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    bool has_generated_password,
    bool is_update) {
  NOTIMPLEMENTED();
}

void WebViewPasswordManagerClient::HideManualFallbackForSaving() {
  NOTIMPLEMENTED();
}

void WebViewPasswordManagerClient::FocusedInputChanged(
    password_manager::PasswordManagerDriver* driver,
    autofill::mojom::FocusedFieldType focused_field_type) {
  NOTIMPLEMENTED();
}

void WebViewPasswordManagerClient::AutomaticPasswordSave(
    std::unique_ptr<PasswordFormManagerForUI> saved_form_manager) {
  NOTIMPLEMENTED();
}

void WebViewPasswordManagerClient::PromptUserToEnableAutosignin() {
  // TODO(crbug.com/435048): Implement this method.
  NOTIMPLEMENTED();
}

bool WebViewPasswordManagerClient::IsIncognito() const {
  return web_state_->GetBrowserState()->IsOffTheRecord();
}

const password_manager::PasswordManager*
WebViewPasswordManagerClient::GetPasswordManager() const {
  return delegate_.passwordManager;
}

const password_manager::PasswordFeatureManager*
WebViewPasswordManagerClient::GetPasswordFeatureManager() const {
  return &password_feature_manager_;
}

bool WebViewPasswordManagerClient::IsMainFrameSecure() const {
  return password_manager::WebStateContentIsSecureHtml(web_state_);
}

PrefService* WebViewPasswordManagerClient::GetPrefs() const {
  return pref_service_;
}

PasswordStore* WebViewPasswordManagerClient::GetProfilePasswordStore() const {
  return profile_store_;
}

PasswordStore* WebViewPasswordManagerClient::GetAccountPasswordStore() const {
  return account_store_;
}

void WebViewPasswordManagerClient::NotifyUserAutoSignin(
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
    const GURL& origin) {
  DCHECK(!local_forms.empty());
  helper_.NotifyUserAutoSignin();
  [delegate_ showAutosigninNotification:std::move(local_forms[0])];
}

void WebViewPasswordManagerClient::NotifyUserCouldBeAutoSignedIn(
    std::unique_ptr<autofill::PasswordForm> form) {
  helper_.NotifyUserCouldBeAutoSignedIn(std::move(form));
}

void WebViewPasswordManagerClient::NotifySuccessfulLoginWithExistingPassword(
    std::unique_ptr<password_manager::PasswordFormManagerForUI>
        submitted_manager) {
  helper_.NotifySuccessfulLoginWithExistingPassword(
      std::move(submitted_manager));
}

void WebViewPasswordManagerClient::NotifyStorePasswordCalled() {
  helper_.NotifyStorePasswordCalled();
}

bool WebViewPasswordManagerClient::IsSavingAndFillingEnabled(
    const GURL& url) const {
  return *saving_passwords_enabled_ && !IsIncognito() &&
         !net::IsCertStatusError(GetMainFrameCertStatus()) &&
         IsFillingEnabled(url);
}

const GURL& WebViewPasswordManagerClient::GetLastCommittedEntryURL() const {
  return delegate_.lastCommittedURL;
}

const password_manager::CredentialsFilter*
WebViewPasswordManagerClient::GetStoreResultFilter() const {
  return &credentials_filter_;
}

const autofill::LogManager* WebViewPasswordManagerClient::GetLogManager()
    const {
  return log_manager_.get();
}

ukm::SourceId WebViewPasswordManagerClient::GetUkmSourceId() {
  // We don't collect UKM metrics from //ios/web_view.
  return ukm::kInvalidSourceId;
}

PasswordManagerMetricsRecorder*
WebViewPasswordManagerClient::GetMetricsRecorder() {
  // We don't collect UKM metrics from //ios/web_view.
  return nullptr;
}

signin::IdentityManager* WebViewPasswordManagerClient::GetIdentityManager() {
  return identity_manager_;
}

scoped_refptr<network::SharedURLLoaderFactory>
WebViewPasswordManagerClient::GetURLLoaderFactory() {
  return web_state_->GetBrowserState()->GetSharedURLLoaderFactory();
}

void WebViewPasswordManagerClient::UpdateFormManagers() {
  delegate_.passwordManager->UpdateFormManagers();
}

bool WebViewPasswordManagerClient::IsIsolationForPasswordSitesEnabled() const {
  return false;
}

bool WebViewPasswordManagerClient::IsNewTabPage() const {
  return false;
}

password_manager::FieldInfoManager*
WebViewPasswordManagerClient::GetFieldInfoManager() const {
  return nullptr;
}

const syncer::SyncService* WebViewPasswordManagerClient::GetSyncService() {
  return sync_service_;
}

}  // namespace ios_web_view
