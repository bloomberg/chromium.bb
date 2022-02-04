// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_dice_sign_in_provider.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_encryption_keys_tab_helper.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/views/profiles/profile_picker_dice_sign_in_toolbar.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "ui/base/theme_provider.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"

namespace {

GURL GetSigninURL(bool dark_mode) {
  GURL signin_url = GaiaUrls::GetInstance()->signin_chrome_sync_dice();
  if (dark_mode)
    signin_url = net::AppendQueryParameter(signin_url, "color_scheme", "dark");
  return signin_url;
}

bool IsExternalURL(const GURL& url) {
  // Empty URL is used initially, about:blank is used to stop navigation after
  // sign-in succeeds.
  if (url.is_empty() || url == GURL(url::kAboutBlankURL))
    return false;
  if (gaia::IsGaiaSignonRealm(url.DeprecatedGetOriginAsURL()))
    return false;
  return true;
}

}  // namespace

ProfilePickerDiceSignInProvider::ProfilePickerDiceSignInProvider(
    ProfilePickerWebContentsHost* host,
    ProfilePickerDiceSignInToolbar* toolbar)
    : host_(host), toolbar_(toolbar) {}

ProfilePickerDiceSignInProvider::~ProfilePickerDiceSignInProvider() {
  // Handle unfinished signed-in profile creation (i.e. when callback was not
  // called yet).
  if (callback_) {
    if (IsInitialized()) {
      contents()->SetDelegate(nullptr);

      // Schedule the profile for deletion, it's not needed any more.
      g_browser_process->profile_manager()->ScheduleEphemeralProfileForDeletion(
          profile_->GetPath());
    }

    ProfileMetrics::LogProfileAddSignInFlowOutcome(
        ProfileMetrics::ProfileAddSignInFlowOutcome::kAbortedBeforeSignIn);
  }
}

void ProfilePickerDiceSignInProvider::SwitchToSignIn(
    base::OnceCallback<void(bool)> switch_finished_callback,
    SignedInCallback signin_finished_callback) {
  // Update the callback even if the profile is already initialized (to respect
  // that the callback may be different).
  callback_ = std::move(signin_finished_callback);

  if (IsInitialized()) {
    std::move(switch_finished_callback).Run(true);
    // Do not load any url because the desired sign-in screen is still loaded in
    // `contents()`.
    host_->ShowScreen(contents(), GURL());
    toolbar_->SetVisible(true);
    return;
  }

  size_t icon_index = profiles::GetPlaceholderAvatarIndex();
  // Silently create the new profile for browsing on GAIA (so that the sign-in
  // cookies are stored in the right profile).
  ProfileManager::CreateMultiProfileAsync(
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .ChooseNameForNewProfile(icon_index),
      icon_index, /*is_hidden=*/true,
      base::BindRepeating(&ProfilePickerDiceSignInProvider::OnProfileCreated,
                          weak_ptr_factory_.GetWeakPtr(),
                          base::OwnedRef(std::move(switch_finished_callback))));
}

void ProfilePickerDiceSignInProvider::ReloadSignInPage() {
  if (IsInitialized() && contents()) {
    contents()->GetController().Reload(content::ReloadType::BYPASSING_CACHE,
                                       true);
  }
}

void ProfilePickerDiceSignInProvider::NavigateBack() {
  if (!IsInitialized() || !contents())
    return;

  if (contents()->GetController().CanGoBack()) {
    contents()->GetController().GoBack();
    return;
  }

  // Move from sign-in back to the previous screen of profile creation.
  // Do not load any url because the desired screen is still loaded in the
  // picker contents.
  host_->ShowScreenInPickerContents(GURL());
  toolbar_->SetVisible(false);
}

const ui::ThemeProvider* ProfilePickerDiceSignInProvider::GetThemeProvider()
    const {
  if (!IsInitialized())
    return nullptr;
  return &ThemeService::GetThemeProviderForProfile(profile_);
}

ui::ColorProviderManager::InitializerSupplier*
ProfilePickerDiceSignInProvider::GetCustomTheme() const {
  if (!IsInitialized())
    return nullptr;
  return ThemeService::GetThemeSupplierForProfile(profile_);
}

bool ProfilePickerDiceSignInProvider::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Ignores context menu.
  return true;
}

void ProfilePickerDiceSignInProvider::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture,
    bool* was_blocked) {
  NavigateParams params(profile_, target_url, ui::PAGE_TRANSITION_LINK);
  // Open all links as new popups.
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.contents_to_insert = std::move(new_contents);
  params.window_bounds = initial_rect;
  Navigate(&params);
}

bool ProfilePickerDiceSignInProvider::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return host_->HandleKeyboardEvent(source, event);
}

void ProfilePickerDiceSignInProvider::NavigationStateChanged(
    content::WebContents* source,
    content::InvalidateTypes changed_flags) {
  if (source == contents_.get() && IsExternalURL(contents_->GetVisibleURL())) {
    // Attach DiceTabHelper to `contents_` so that sync consent dialog appears
    // after a successful sign-in.
    DiceTabHelper::CreateForWebContents(contents_.get());
    DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(contents_.get());
    // Use |redirect_url| and not |continue_url|, so that the DiceTabHelper can
    // redirect to chrome:// URLs such as the NTP.
    tab_helper->InitializeSigninFlow(
        GetSigninURL(host_->ShouldUseDarkColors()),
        signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
        signin_metrics::Reason::kSigninPrimaryAccount,
        signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
        GURL(chrome::kChromeUINewTabURL));

    // The rest of the SAML flow logic is handled by the signed-in flow
    // controller.
    FinishFlow(/*is_saml=*/true);
  }
}

web_modal::WebContentsModalDialogHost*
ProfilePickerDiceSignInProvider::GetWebContentsModalDialogHost() {
  return host_;
}

void ProfilePickerDiceSignInProvider::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  DCHECK(IsInitialized());
  refresh_token_updated_ = true;
  FinishFlowIfSignedIn();
}

void ProfilePickerDiceSignInProvider::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  DCHECK(IsInitialized());
  FinishFlowIfSignedIn();
}

void ProfilePickerDiceSignInProvider::FinishFlowIfSignedIn() {
  DCHECK(IsInitialized());

  if (IdentityManagerFactory::GetForProfile(profile_)
          ->HasPrimaryAccountWithRefreshToken(signin::ConsentLevel::kSignin) &&
      refresh_token_updated_) {
    FinishFlow(/*is_saml=*/false);
  }
}

void ProfilePickerDiceSignInProvider::OnProfileCreated(
    base::OnceCallback<void(bool)>& switch_finished_callback,
    Profile* new_profile,
    Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_LOCAL_FAIL) {
    std::move(switch_finished_callback).Run(false);
    return;
  }
  if (status != Profile::CREATE_STATUS_INITIALIZED) {
    return;
  }

  DCHECK(new_profile);
  DCHECK(!profile_);
  DCHECK(!contents());
  std::move(switch_finished_callback).Run(true);

  profile_ = new_profile;
  profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
      profile_, ProfileKeepAliveOrigin::kProfileCreationFlow);

  contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(profile_));
  contents()->SetDelegate(this);

  // Create a manager that supports modal dialogs, such as for webauthn.
  web_modal::WebContentsModalDialogManager::CreateForWebContents(contents());
  web_modal::WebContentsModalDialogManager::FromWebContents(contents())
      ->SetDelegate(this);

  // To allow passing encryption keys during interactions with the page,
  // instantiate SyncEncryptionKeysTabHelper.
  SyncEncryptionKeysTabHelper::CreateForWebContents(contents());

  // Listen for sign-in getting completed.
  identity_manager_observation_.Observe(
      IdentityManagerFactory::GetForProfile(profile_));

  // Record that the sign in process starts (its end is recorded automatically
  // by the instance of DiceTurnSyncOnHelper constructed later on in
  // ProfilePickerSignedInFlowController).
  signin_metrics::RecordSigninUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
  signin_metrics::LogSigninAccessPointStarted(
      signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);

  // Apply the default theme to get consistent colors for toolbars (this matters
  // for linux where the 'system' theme is used for new profiles).
  auto* theme_service = ThemeServiceFactory::GetForProfile(profile_);
  theme_service->UseDefaultTheme();

  // Make sure the web contents used for sign-in has proper background to match
  // the toolbar (for dark mode).
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      contents(), GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR));

  toolbar_->BuildToolbar(base::BindRepeating(
      &ProfilePickerDiceSignInProvider::NavigateBack, base::Unretained(this)));

  host_->ShowScreen(
      contents(), GetSigninURL(host_->ShouldUseDarkColors()),
      base::BindOnce(&ProfilePickerDiceSignInToolbar::SetVisible,
                     // Unretained is enough as the callback is
                     // called by the owner of the toolbar.
                     base::Unretained(toolbar_), /*visible=*/true));
}

bool ProfilePickerDiceSignInProvider::IsInitialized() const {
  return profile_ != nullptr;
}

void ProfilePickerDiceSignInProvider::FinishFlow(bool is_saml) {
  DCHECK(IsInitialized());
  contents()->SetDelegate(nullptr);
  // Stop the sign-in: hide and clear the toolbar.
  toolbar_->ClearToolbar();
  toolbar_->SetVisible(false);
  std::move(callback_).Run(profile_.get(), std::move(contents_), is_saml);
}
