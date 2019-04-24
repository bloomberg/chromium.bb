// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_handler_chromeos.h"

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler.h"
#include "chromeos/account_manager/account_manager.h"
#include "chromeos/account_manager/account_manager_factory.h"
#include "components/signin/core/browser/account_info.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {
namespace {

// A helper class for completing the inline login flow. Primarily, it is
// responsible for exchanging the auth code, obtained after a successful user
// sign in, for OAuth tokens and subsequently populating Chrome OS
// AccountManager with these tokens.
// This object is supposed to be used in a one-shot fashion and it deletes
// itself after its work is complete.
class SigninHelper : public GaiaAuthConsumer {
 public:
  SigninHelper(
      chromeos::AccountManager* account_manager,
      const base::RepeatingClosure& close_dialog_closure,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& gaia_id,
      const std::string& email,
      const std::string& auth_code)
      : account_manager_(account_manager),
        close_dialog_closure_(close_dialog_closure),
        email_(email),
        gaia_auth_fetcher_(this,
                           gaia::GaiaSource::kChrome,
                           url_loader_factory) {
    account_key_ = chromeos::AccountManager::AccountKey{
        gaia_id, chromeos::account_manager::AccountType::ACCOUNT_TYPE_GAIA};

    gaia_auth_fetcher_.StartAuthCodeForOAuth2TokenExchange(auth_code);
  }

  ~SigninHelper() override = default;

  // GaiaAuthConsumer overrides.
  void OnClientOAuthSuccess(const ClientOAuthResult& result) override {
    // Flow of control after this call:
    // |AccountManager::UpsertAccount| updates / inserts the account and calls
    // its |Observer|s, one of which is |ChromeOSOAuth2TokenServiceDelegate|.
    // |ChromeOSOAuth2TokenServiceDelegate::OnTokenUpserted| seeds the Gaia id
    // and email id for this account in |AccountTrackerService| and invokes
    // |FireRefreshTokenAvailable|. This causes the account to propagate
    // throughout the Identity Service chain, including in
    // |AccountFetcherService|. |AccountFetcherService::OnRefreshTokenAvailable|
    // invokes |AccountTrackerService::StartTrackingAccount|, triggers a fetch
    // for the account information from Gaia and updates this information into
    // |AccountTrackerService|. At this point the account will be fully added to
    // the system.
    account_manager_->UpsertAccount(account_key_, email_, result.refresh_token);

    close_dialog_closure_.Run();
    base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }

  void OnClientOAuthFailure(const GoogleServiceAuthError& error) override {
    // TODO(sinhak): Display an error.
    close_dialog_closure_.Run();
    base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }

 private:
  // A non-owning pointer to Chrome OS AccountManager.
  chromeos::AccountManager* const account_manager_;
  // A closure to close the hosting dialog window.
  base::RepeatingClosure close_dialog_closure_;
  // The user's AccountKey for which |this| object has been created.
  chromeos::AccountManager::AccountKey account_key_;
  // The user's email for which |this| object has been created.
  const std::string email_;
  // Used for exchanging auth code for OAuth tokens.
  GaiaAuthFetcher gaia_auth_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(SigninHelper);
};

}  // namespace

InlineLoginHandlerChromeOS::InlineLoginHandlerChromeOS(
    const base::RepeatingClosure& close_dialog_closure)
    : close_dialog_closure_(close_dialog_closure) {}

InlineLoginHandlerChromeOS::~InlineLoginHandlerChromeOS() = default;

void InlineLoginHandlerChromeOS::SetExtraInitParams(
    base::DictionaryValue& params) {
  const GaiaUrls* const gaia_urls = GaiaUrls::GetInstance();
  params.SetKey("isNewGaiaFlow", base::Value(true));
  params.SetKey("clientId", base::Value(gaia_urls->oauth2_chrome_client_id()));

  const GURL& url = gaia_urls->embedded_setup_chromeos_url(2U);
  params.SetKey("gaiaPath", base::Value(url.path().substr(1)));

  params.SetKey("constrained", base::Value("1"));
  params.SetKey("flow", base::Value("crosAddAccount"));
}

void InlineLoginHandlerChromeOS::CompleteLogin(const std::string& email,
                                               const std::string& password,
                                               const std::string& gaia_id,
                                               const std::string& auth_code,
                                               bool skip_for_now,
                                               bool trusted,
                                               bool trusted_found,
                                               bool choose_what_to_sync) {
  CHECK(!auth_code.empty());
  CHECK(!gaia_id.empty());
  CHECK(!email.empty());

  // TODO(sinhak): Do not depend on Profile unnecessarily.
  Profile* profile = Profile::FromWebUI(web_ui());

  // TODO(sinhak): Do not depend on Profile unnecessarily. When multiprofile on
  // Chrome OS is released, get rid of |AccountManagerFactory| and get
  // AccountManager directly from |g_browser_process|.
  chromeos::AccountManager* account_manager =
      g_browser_process->platform_part()
          ->GetAccountManagerFactory()
          ->GetAccountManager(profile->GetPath().value());

  // SigninHelper deletes itself after its work is done.
  new SigninHelper(account_manager, close_dialog_closure_,
                   account_manager->GetUrlLoaderFactory(), gaia_id, email,
                   auth_code);
}

}  // namespace chromeos
