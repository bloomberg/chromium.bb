// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin_view_controller.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/signin_reauth_popup_delegate.h"
#include "chrome/browser/ui/signin_view_controller_delegate.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/core_account_id.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/logout_tab_helper.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/google_api_keys.h"
#include "url/url_constants.h"
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

namespace {

const base::Feature kSigninReauthPrompt = {"SigninReauthPrompt",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

const base::Feature kOpenSignoutTab = {"OpenSignoutTab",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// Returns the sign-in reason for |mode|.
signin_metrics::Reason GetSigninReasonFromMode(profiles::BubbleViewMode mode) {
  DCHECK(SigninViewController::ShouldShowSigninForMode(mode));
  switch (mode) {
    case profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN:
      return signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT;
    case profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT:
      return signin_metrics::Reason::REASON_ADD_SECONDARY_ACCOUNT;
    case profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH:
      return signin_metrics::Reason::REASON_REAUTHENTICATION;
    default:
      NOTREACHED();
      return signin_metrics::Reason::REASON_UNKNOWN_REASON;
  }
}

// Opens a new tab on |url| or reuses the current tab if it is the NTP.
void ShowTabOverwritingNTP(Browser* browser, const GURL& url) {
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.user_gesture = true;
  params.tabstrip_add_types |= TabStripModel::ADD_INHERIT_OPENER;

  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (contents) {
    const GURL& contents_url = contents->GetVisibleURL();
    if (contents_url == chrome::kChromeUINewTabURL ||
        search::IsInstantNTP(contents) || contents_url == url::kAboutBlankURL) {
      params.disposition = WindowOpenDisposition::CURRENT_TAB;
    }
  }

  Navigate(&params);
}

// Returns the index of an existing re-usable Dice signin tab, or -1.
int FindDiceSigninTab(TabStripModel* tab_strip, const GURL& signin_url) {
  int tab_count = tab_strip->count();
  for (int tab_index = 0; tab_index < tab_count; ++tab_index) {
    content::WebContents* web_contents = tab_strip->GetWebContentsAt(tab_index);
    DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(web_contents);
    if (tab_helper && tab_helper->signin_url() == signin_url &&
        tab_helper->IsChromeSigninPage()) {
      return tab_index;
    }
  }
  return -1;
}

// Returns the promo action to be used when signing with a new account.
signin_metrics::PromoAction GetPromoActionForNewAccount(
    signin::IdentityManager* identity_manager) {
  return !identity_manager->GetAccountsWithRefreshTokens().empty()
             ? signin_metrics::PromoAction::
                   PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT
             : signin_metrics::PromoAction::
                   PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT;
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// If this is destroyed before SignalReauthDone is called, will call
// |close_modal_signin_callback_| to stop the ongoing reauth.
class ReauthAbortHandleImpl : public SigninViewController::ReauthAbortHandle {
 public:
  explicit ReauthAbortHandleImpl(base::OnceClosure close_modal_signin_callback);
  ReauthAbortHandleImpl(const ReauthAbortHandleImpl&) = delete;
  ReauthAbortHandleImpl operator=(const ReauthAbortHandleImpl&) = delete;
  ~ReauthAbortHandleImpl() override;

  // Nullifies |close_modal_signin_callback_|.
  void SignalReauthDone();

 private:
  base::OnceClosure close_modal_signin_callback_;
};

ReauthAbortHandleImpl::ReauthAbortHandleImpl(
    base::OnceClosure close_modal_signin_callback)
    : close_modal_signin_callback_(std::move(close_modal_signin_callback)) {
  DCHECK(close_modal_signin_callback_);
}

ReauthAbortHandleImpl::~ReauthAbortHandleImpl() {
  if (close_modal_signin_callback_) {
    std::move(close_modal_signin_callback_).Run();
  }
}

void ReauthAbortHandleImpl::SignalReauthDone() {
  close_modal_signin_callback_.Reset();
}

}  // namespace

SigninViewController::SigninViewController(Browser* browser)
    : browser_(browser) {}

SigninViewController::~SigninViewController() {
  CloseModalSignin();
}

// static
bool SigninViewController::ShouldShowSigninForMode(
    profiles::BubbleViewMode mode) {
  return mode == profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN ||
         mode == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT ||
         mode == profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH;
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void SigninViewController::ShowSignin(profiles::BubbleViewMode mode,
                                      signin_metrics::AccessPoint access_point,
                                      const GURL& redirect_url) {
  DCHECK(ShouldShowSigninForMode(mode));

  Profile* profile = browser_->profile();
  std::string email;
  signin_metrics::Reason signin_reason = GetSigninReasonFromMode(mode);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (signin_reason == signin_metrics::Reason::REASON_REAUTHENTICATION) {
    email = identity_manager->GetPrimaryAccountInfo().email;
  }
  signin_metrics::PromoAction promo_action =
      GetPromoActionForNewAccount(identity_manager);
  ShowDiceSigninTab(signin_reason, access_point, promo_action, email,
                    redirect_url);
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

void SigninViewController::ShowModalSyncConfirmationDialog() {
  CloseModalSignin();
  // The delegate will delete itself on request of the UI code when the widget
  // is closed.
  delegate_ =
      SigninViewControllerDelegate::CreateSyncConfirmationDelegate(browser_);
  delegate_observer_.Add(delegate_);
  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::SIGN_IN_SYNC_CONFIRMATION);
}

void SigninViewController::ShowModalSigninErrorDialog() {
  CloseModalSignin();
  // The delegate will delete itself on request of the UI code when the widget
  // is closed.
  delegate_ = SigninViewControllerDelegate::CreateSigninErrorDelegate(browser_);
  delegate_observer_.Add(delegate_);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::SIGN_IN_ERROR);
}

std::unique_ptr<SigninViewController::ReauthAbortHandle>
SigninViewController::ShowReauthPrompt(
    const CoreAccountId& account_id,
    base::OnceCallback<void(signin::ReauthResult)> reauth_callback) {
  CloseModalSignin();

  auto abort_handle = std::make_unique<ReauthAbortHandleImpl>(base::BindOnce(
      &SigninViewController::CloseModalSignin, weak_ptr_factory_.GetWeakPtr()));

  // Wrap |reauth_callback| so that it also signals to |reauth_abort_handle|
  // when executed. The handle outlives the callback because it calls
  // CloseModalSignin on destruction, and this runs the callback (with a
  // "cancelled" result). So base::Unretained can be used.
  auto wrapped_reauth_callback = base::BindOnce(
      [](ReauthAbortHandleImpl* handle,
         base::OnceCallback<void(signin::ReauthResult)> cb,
         signin::ReauthResult result) {
        handle->SignalReauthDone();
        std::move(cb).Run(result);
      },
      base::Unretained(abort_handle.get()), std::move(reauth_callback));

  // The delegate will delete itself on request of the UI code when the widget
  // is closed.
  if (!base::FeatureList::IsEnabled(kSigninReauthPrompt)) {
    // This currently displays a fake dialog for development purposes. Should
    // not be called in production.
    delegate_ = SigninViewControllerDelegate::CreateFakeReauthDelegate(
        browser_, account_id, std::move(wrapped_reauth_callback));
    delegate_observer_.Add(delegate_);
    return abort_handle;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser_->profile());
  base::Optional<AccountInfo> account_info =
      identity_manager
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
              account_id);

  // For now, Reauth is restricted to the primary account only.
  CoreAccountId primary_account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kNotRequired);

  if (!account_info || account_id != primary_account_id) {
    std::move(wrapped_reauth_callback)
        .Run(signin::ReauthResult::kAccountNotSignedIn);
    return abort_handle;
  }

  if (account_info->hosted_domain != kNoHostedDomainFound &&
      account_info->hosted_domain != "google.com") {
    // Display a popup for Dasher users. Ideally it should only be shown for
    // SAML users but there is no way to distinguish them.
    delegate_ = new SigninReauthPopupDelegate(
        browser_, account_id, std::move(wrapped_reauth_callback));
    delegate_observer_.Add(delegate_);
  } else {
    delegate_ = SigninViewControllerDelegate::CreateReauthDelegate(
        browser_, account_id, std::move(wrapped_reauth_callback));
    delegate_observer_.Add(delegate_);
  }
  chrome::RecordDialogCreation(chrome::DialogIdentifier::SIGNIN_REAUTH);
  return abort_handle;
}

bool SigninViewController::ShowsModalDialog() {
  return delegate_ != nullptr;
}

void SigninViewController::CloseModalSignin() {
  if (delegate_)
    delegate_->CloseModalSignin();

  DCHECK(!delegate_);
}

void SigninViewController::SetModalSigninHeight(int height) {
  if (delegate_)
    delegate_->ResizeNativeView(height);
}

void SigninViewController::OnModalSigninClosed() {
  delegate_observer_.Remove(delegate_);
  delegate_ = nullptr;
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void SigninViewController::ShowDiceSigninTab(
    signin_metrics::Reason signin_reason,
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action,
    const std::string& email_hint,
    const GURL& redirect_url) {
#if DCHECK_IS_ON()
  if (!AccountConsistencyModeManager::IsDiceEnabledForProfile(
          browser_->profile())) {
    // Developers often fall into the trap of not configuring the OAuth client
    // ID and client secret and then attempt to sign in to Chromium, which
    // fail as the account consistency is disabled. Explicitly check that the
    // OAuth client ID are configured when developers attempt to sign in to
    // Chromium.
    DCHECK(google_apis::HasOAuthClientConfigured())
        << "You must configure the OAuth client ID and client secret in order "
           "to sign in to Chromium. See instruction at "
           "https://www.chromium.org/developers/how-tos/api-keys";

    // Account consistency mode does not support signing in to Chrome due to
    // some other unexpected reason. Signing in to Chrome is not supported.
    NOTREACHED()
        << "OAuth client ID and client secret is configured, but "
           "the account consistency mode does not support signing in to "
           "Chromium.";
  }
#endif

  // If redirect_url is empty, we would like to redirect to the NTP, but it's
  // not possible through the continue_url, because Gaia cannot redirect to
  // chrome:// URLs. Use the google base URL instead here, and the DiceTabHelper
  // may do the redirect to the NTP later.
  // Note: Gaia rejects some continue URLs as invalid and responds with HTTP
  // error 400. This seems to happen in particular if the continue URL is not a
  // Google-owned domain. Chrome cannot enforce that only valid URLs are used,
  // because the set of valid URLs is not specified.
  std::string continue_url =
      (redirect_url.is_empty() || !redirect_url.SchemeIsHTTPOrHTTPS())
          ? UIThreadSearchTermsData().GoogleBaseURLValue()
          : redirect_url.spec();

  GURL signin_url =
      signin_reason == signin_metrics::Reason::REASON_ADD_SECONDARY_ACCOUNT
          ? signin::GetAddAccountURLForDice(email_hint, continue_url)
          : signin::GetChromeSyncURLForDice(email_hint, continue_url);

  content::WebContents* active_contents = nullptr;
  if (access_point == signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE) {
    active_contents = browser_->tab_strip_model()->GetActiveWebContents();
    content::OpenURLParams params(signin_url, content::Referrer(),
                                  WindowOpenDisposition::CURRENT_TAB,
                                  ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
    active_contents->OpenURL(params);
  } else {
    // Check if there is already a signin-tab open.
    TabStripModel* tab_strip = browser_->tab_strip_model();
    int dice_tab_index = FindDiceSigninTab(tab_strip, signin_url);
    if (dice_tab_index != -1) {
      if (access_point !=
          signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS) {
        // Extensions do not activate the tab to prevent misbehaving
        // extensions to keep focusing the signin tab.
        tab_strip->ActivateTabAt(dice_tab_index,
                                 {TabStripModel::GestureType::kOther});
      }
      // Do not create a new signin tab, because there is already one.
      return;
    }

    ShowTabOverwritingNTP(browser_, signin_url);
    active_contents = browser_->tab_strip_model()->GetActiveWebContents();
  }

  DCHECK(active_contents);
  DCHECK_EQ(signin_url, active_contents->GetVisibleURL());
  DiceTabHelper::CreateForWebContents(active_contents);
  DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(active_contents);

  // Use |redirect_url| and not |continue_url|, so that the DiceTabHelper can
  // redirect to chrome:// URLs such as the NTP.
  tab_helper->InitializeSigninFlow(signin_url, access_point, signin_reason,
                                   promo_action, redirect_url);
}

void SigninViewController::ShowDiceEnableSyncTab(
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action,
    const std::string& email_hint) {
  signin_metrics::Reason reason =
      signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT;
  std::string email_to_use = email_hint;
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser_->profile());
  if (identity_manager->HasPrimaryAccount()) {
    reason = signin_metrics::Reason::REASON_REAUTHENTICATION;
    email_to_use = identity_manager->GetPrimaryAccountInfo().email;
    DCHECK(email_hint.empty() || gaia::AreEmailsSame(email_hint, email_to_use));
  }
  ShowDiceSigninTab(reason, access_point, promo_action, email_to_use);
}

void SigninViewController::ShowDiceAddAccountTab(
    signin_metrics::AccessPoint access_point,
    const std::string& email_hint) {
  ShowDiceSigninTab(
      signin_metrics::Reason::REASON_ADD_SECONDARY_ACCOUNT, access_point,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO, email_hint);
}

void SigninViewController::ShowGaiaLogoutTab(
    signin_metrics::SourceForRefreshTokenOperation source) {
  if (!base::FeatureList::IsEnabled(kOpenSignoutTab)) {
    IdentityManagerFactory::GetForProfile(browser_->profile())
        ->GetAccountsMutator()
        ->RemoveAllAccounts(source);
    return;
  }

  // Since the user may be triggering navigation from another UI element such as
  // a menu, ensure the web contents (and therefore the page that is about to be
  // shown) is focused. (See crbug/926492 for motivation.)
  auto* const contents = browser_->tab_strip_model()->GetActiveWebContents();
  if (contents)
    contents->Focus();

  // Do not use a singleton tab. A new tab should be opened even if there is
  // already a logout tab.
  ShowTabOverwritingNTP(browser_,
                        GaiaUrls::GetInstance()->service_logout_url());

  // Monitor the logout and fallback to local signout if it fails. The
  // LogoutTabHelper deletes itself.
  content::WebContents* logout_tab_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  DCHECK(logout_tab_contents);
  LogoutTabHelper::CreateForWebContents(logout_tab_contents);
}

void SigninViewController::ShowModalSigninEmailConfirmationDialog(
    const std::string& last_email,
    const std::string& email,
    base::OnceCallback<void(SigninEmailConfirmationDialog::Action)> callback) {
  CloseModalSignin();
  content::WebContents* active_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  // The delegate will delete itself on request of the UI code when the widget
  // is closed.
  delegate_ = SigninEmailConfirmationDialog::AskForConfirmation(
      active_contents, browser_->profile(), last_email, email,
      std::move(callback));
  delegate_observer_.Add(delegate_);
  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::SIGN_IN_EMAIL_CONFIRMATION);
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

content::WebContents*
SigninViewController::GetModalDialogWebContentsForTesting() {
  DCHECK(delegate_);
  return delegate_->GetWebContents();
}
