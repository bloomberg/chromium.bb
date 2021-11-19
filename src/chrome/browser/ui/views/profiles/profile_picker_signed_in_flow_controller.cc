// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_signed_in_flow_controller.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/profiles/profile_customization_bubble_sync_controller.h"
#include "chrome/browser/ui/views/profiles/profile_customization_bubble_view.h"
#include "chrome/browser/ui/views/profiles/profile_picker_turn_sync_on_delegate.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/theme_provider.h"

namespace {

// Shows the customization bubble if possible. The bubble won't be shown if the
// color is enforced by policy or downloaded through Sync or the default theme
// should be used. An IPH is shown after the bubble, or right away if the bubble
// cannot be shown.
void ShowCustomizationBubble(absl::optional<SkColor> new_profile_color,
                             Browser* browser) {
  DCHECK(browser);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view || !browser_view->toolbar_button_provider())
    return;
  views::View* anchor_view =
      browser_view->toolbar_button_provider()->GetAvatarToolbarButton();
  CHECK(anchor_view);

  if (ProfileCustomizationBubbleSyncController::CanThemeSyncStart(
          browser->profile())) {
    // For sync users, their profile color has not been applied yet. Call a
    // helper class that applies the color and shows the bubble only if there is
    // no conflict with a synced theme / color.
    ProfileCustomizationBubbleSyncController::
        ApplyColorAndShowBubbleWhenNoValueSynced(
            browser->profile(), anchor_view,
            /*suggested_profile_color=*/new_profile_color.value());
  } else {
    // For non syncing users, simply show the bubble.
    ProfileCustomizationBubbleView::CreateBubble(browser->profile(),
                                                 anchor_view);
  }
}

void MaybeShowProfileSwitchIPH(Browser* browser) {
  DCHECK(browser);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view)
    return;
  browser_view->MaybeShowProfileSwitchIPH();
}

GURL GetSyncConfirmationLoadingURL() {
  return GURL(chrome::kChromeUISyncConfirmationURL)
      .Resolve(chrome::kChromeUISyncConfirmationLoadingPath);
}

void ContinueSAMLSignin(std::unique_ptr<content::WebContents> saml_wc,
                        Browser* browser) {
  DCHECK(browser);
  browser->tab_strip_model()->ReplaceWebContentsAt(0, std::move(saml_wc));

  ProfileMetrics::LogProfileAddSignInFlowOutcome(
      ProfileMetrics::ProfileAddSignInFlowOutcome::kSAML);
}

}  // namespace

ProfilePickerSignedInFlowController::ProfilePickerSignedInFlowController(
    ProfilePickerWebContentsHost* host,
    Profile* profile,
    std::unique_ptr<content::WebContents> contents,
    absl::optional<SkColor> profile_color,
    base::TimeDelta extended_account_info_timeout)
    : host_(host),
      profile_(profile),
      contents_(std::move(contents)),
      profile_color_(profile_color),
      extended_account_info_timeout_(extended_account_info_timeout) {
  DCHECK(profile_);
  DCHECK(contents_);
}

ProfilePickerSignedInFlowController::~ProfilePickerSignedInFlowController() {
  if (contents())
    contents()->SetDelegate(nullptr);

  // Record unfinished signed-in profile creation.
  if (!is_finished_) {
    // TODO(crbug.com/1227699): Schedule the profile for deletion here, it's not
    // needed any more. This triggers a crash if the browser is shutting down
    // completely. Figure a way how to delete the profile only if that does not
    // compete with a shutdown.

    ProfileMetrics::LogProfileAddSignInFlowOutcome(
        ProfileMetrics::ProfileAddSignInFlowOutcome::kAbortedAfterSignIn);
  }
}

void ProfilePickerSignedInFlowController::Cancel() {
  DCHECK(IsInitialized());
  if (is_finished_)
    return;

  is_finished_ = true;

  // TODO(crbug.com/1227699): Consider moving this into the destructor so that
  // unfinished (and unaborted) flows also get the profile deleted right away.
  g_browser_process->profile_manager()->ScheduleProfileForDeletion(
      profile_->GetPath(), base::DoNothing());
}

void ProfilePickerSignedInFlowController::FinishAndOpenBrowser(
    BrowserOpenedCallback callback) {
  DCHECK(IsInitialized());
  // Do nothing if the sign-in flow is aborted or if this has already been
  // called. Note that this can get called first time from a special case
  // handling (such as the Settings link) and than second time when the
  // DiceTurnSyncOnHelper finishes.
  if (is_finished_)
    return;
  is_finished_ = true;

  if (name_for_signed_in_profile_.empty()) {
    on_profile_name_available_ = base::BindOnce(
        &ProfilePickerSignedInFlowController::FinishAndOpenBrowserImpl,
        weak_ptr_factory_.GetWeakPtr(), std::move(callback));
    return;
  }

  FinishAndOpenBrowserImpl(std::move(callback));
}

void ProfilePickerSignedInFlowController::SwitchToSyncConfirmation() {
  DCHECK(IsInitialized());
  host_->ShowScreen(contents(), GURL(chrome::kChromeUISyncConfirmationURL),
                    /*navigation_finished_closure=*/
                    base::BindOnce(&ProfilePickerSignedInFlowController::
                                       SwitchToSyncConfirmationFinished,
                                   // Unretained is enough as the callback is
                                   // called by the owner of this instance.
                                   base::Unretained(this)));
}

void ProfilePickerSignedInFlowController::SwitchToEnterpriseProfileWelcome(
    EnterpriseProfileWelcomeUI::ScreenType type,
    base::OnceCallback<void(bool)> proceed_callback) {
  DCHECK(IsInitialized());
  host_->ShowScreen(contents(),
                    GURL(chrome::kChromeUIEnterpriseProfileWelcomeURL),
                    /*navigation_finished_closure=*/
                    base::BindOnce(&ProfilePickerSignedInFlowController::
                                       SwitchToEnterpriseProfileWelcomeFinished,
                                   // Unretained is enough as the callback is
                                   // called by the owner of this instance.
                                   base::Unretained(this), type,
                                   std::move(proceed_callback)));
}

void ProfilePickerSignedInFlowController::SwitchToProfileSwitch(
    const base::FilePath& profile_path) {
  DCHECK(IsInitialized());
  // The sign-in flow is finished, no profile window should be shown in the end.
  Cancel();

  switch_profile_path_ = profile_path;
  host_->ShowScreenInSystemContents(
      GURL(chrome::kChromeUIProfilePickerUrl).Resolve("profile-switch"));
}

bool ProfilePickerSignedInFlowController::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Ignores context menu.
  return true;
}

void ProfilePickerSignedInFlowController::Init(bool is_saml) {
  profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
      profile_, ProfileKeepAliveOrigin::kProfileCreationFlow);

  if (is_saml) {
    FinishAndOpenBrowserForSAML();
    return;
  }

  contents_->SetDelegate(this);

  // Listen for extended account info getting fetched.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  identity_manager_observation_.Observe(identity_manager);

  const CoreAccountInfo& account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  DCHECK(!account_info.IsEmpty()) << "A profile with valid (unconsented) "
                                     "primary account must be passed in.";
  email_ = account_info.email;

  base::OnceClosure sync_consent_completed_closure =
      base::BindOnce(&ProfilePickerSignedInFlowController::FinishAndOpenBrowser,
                     weak_ptr_factory_.GetWeakPtr(), BrowserOpenedCallback());

  // Stop with the sign-in navigation and show a spinner instead. The spinner
  // will be shown until DiceTurnSyncOnHelper (below) figures out whether it's a
  // managed account and whether sync is disabled by policies (which in some
  // cases involves fetching policies and can take a couple of seconds).
  host_->ShowScreen(contents(), GetSyncConfirmationLoadingURL());

  // Set up a timeout for extended account info (which cancels any existing
  // timeout closure).
  extended_account_info_timeout_closure_.Reset(base::BindOnce(
      &ProfilePickerSignedInFlowController::OnExtendedAccountInfoTimeout,
      weak_ptr_factory_.GetWeakPtr(), account_info));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, extended_account_info_timeout_closure_.callback(),
      extended_account_info_timeout_);

  // DiceTurnSyncOnHelper deletes itself once done.
  new DiceTurnSyncOnHelper(
      profile_, signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
      signin_metrics::Reason::kSigninPrimaryAccount, account_info.account_id,
      DiceTurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
      std::make_unique<ProfilePickerTurnSyncOnDelegate>(
          weak_ptr_factory_.GetWeakPtr(), profile_),
      std::move(sync_consent_completed_closure));
}

void ProfilePickerSignedInFlowController::OnExtendedAccountInfoUpdated(
    const AccountInfo& account_info) {
  DCHECK(IsInitialized());
  if (!account_info.IsValid())
    return;
  name_for_signed_in_profile_ =
      profiles::GetDefaultNameForNewSignedInProfile(account_info);
  OnProfileNameAvailable();
  // Extended info arrived on time, no need for the timeout callback any more.
  extended_account_info_timeout_closure_.Cancel();
}

void ProfilePickerSignedInFlowController::OnExtendedAccountInfoTimeout(
    const CoreAccountInfo& account) {
  DCHECK(IsInitialized());
  name_for_signed_in_profile_ =
      profiles::GetDefaultNameForNewSignedInProfileWithIncompleteInfo(account);
  OnProfileNameAvailable();
}

void ProfilePickerSignedInFlowController::OnProfileNameAvailable() {
  DCHECK(IsInitialized());
  // Stop listening to further changes.
  DCHECK(identity_manager_observation_.IsObservingSource(
      IdentityManagerFactory::GetForProfile(profile_)));
  identity_manager_observation_.Reset();

  if (on_profile_name_available_)
    std::move(on_profile_name_available_).Run();
}

void ProfilePickerSignedInFlowController::SwitchToSyncConfirmationFinished() {
  DCHECK(IsInitialized());
  // Initialize the WebUI page once we know it's committed.
  SyncConfirmationUI* sync_confirmation_ui =
      static_cast<SyncConfirmationUI*>(contents()->GetWebUI()->GetController());

  sync_confirmation_ui->InitializeMessageHandlerForCreationFlow(
      GetProfileColor());
}

void ProfilePickerSignedInFlowController::
    SwitchToEnterpriseProfileWelcomeFinished(
        EnterpriseProfileWelcomeUI::ScreenType type,
        base::OnceCallback<void(bool)> proceed_callback) {
  DCHECK(IsInitialized());
  // Initialize the WebUI page once we know it's committed.
  EnterpriseProfileWelcomeUI* enterprise_profile_welcome_ui =
      contents()
          ->GetWebUI()
          ->GetController()
          ->GetAs<EnterpriseProfileWelcomeUI>();

  enterprise_profile_welcome_ui->Initialize(
      /*browser=*/nullptr, type,
      IdentityManagerFactory::GetForProfile(profile_)
          ->FindExtendedAccountInfoByEmailAddress(email_),
      GetProfileColor(), std::move(proceed_callback));
}

bool ProfilePickerSignedInFlowController::IsInitialized() const {
  // This is initialized if the `profile_keep_alive_` object exists.
  return static_cast<bool>(profile_keep_alive_);
}

absl::optional<SkColor> ProfilePickerSignedInFlowController::GetProfileColor()
    const {
  // The new profile theme may be overridden by an existing policy theme. This
  // check ensures the correct theme is applied to the sync confirmation window.
  auto* theme_service = ThemeServiceFactory::GetForProfile(profile_);
  if (theme_service->UsingPolicyTheme())
    return theme_service->GetPolicyThemeColor();
  return profile_color_;
}

void ProfilePickerSignedInFlowController::FinishAndOpenBrowserImpl(
    BrowserOpenedCallback callback) {
  DCHECK(IsInitialized());
  DCHECK(!name_for_signed_in_profile_.empty());

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_->GetPath());
  if (!entry) {
    NOTREACHED();
    return;
  }

  entry->SetIsOmitted(false);
  if (!profile_->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles)) {
    // Unmark this profile ephemeral so that it isn't deleted upon next startup.
    // Profiles should never be made non-ephemeral if ephemeral mode is forced
    // by policy.
    entry->SetIsEphemeral(false);
  }
  entry->SetLocalProfileName(name_for_signed_in_profile_,
                             /*is_default_name=*/false);
  ProfileMetrics::LogProfileAddNewUser(
      ProfileMetrics::ADD_NEW_PROFILE_PICKER_SIGNED_IN);

  // If there's no custom callback specified (that overrides profile
  // customization bubble), Chrome should show the customization bubble.
  if (!callback) {
    // If there's no color to apply to the profile, skip the customization
    // bubble and trigger an IPH, instead.
    if (ThemeServiceFactory::GetForProfile(profile_)->UsingPolicyTheme() ||
        !profile_color_.has_value()) {
      callback = base::BindOnce(&MaybeShowProfileSwitchIPH);
    } else {
      callback = base::BindOnce(&ShowCustomizationBubble, profile_color_);

      // If sync cannot start, we apply `profile_color_` right away before
      // opening a browser window to avoid flicker. Otherwise, it's applied
      // later by code triggered from ShowCustomizationBubble().
      if (!ProfileCustomizationBubbleSyncController::CanThemeSyncStart(
              profile_)) {
        auto* theme_service = ThemeServiceFactory::GetForProfile(profile_);
        theme_service->BuildAutogeneratedThemeFromColor(profile_color_.value());
      }
    }
  }

  // Skip the FRE for this profile as it's replaced by profile creation flow.
  profile_->GetPrefs()->SetBoolean(prefs::kHasSeenWelcomePage, true);

  // TODO(crbug.com/1126913): Change the callback of
  // profiles::OpenBrowserWindowForProfile() to be a OnceCallback as it is only
  // called once.
  profiles::OpenBrowserWindowForProfile(
      base::BindOnce(&ProfilePickerSignedInFlowController::OnBrowserOpened,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      /*always_create=*/false,   // Don't create a window if one already exists.
      /*is_new_profile=*/false,  // Don't create a first run window.
      /*unblock_extensions=*/false,  // There is no need to unblock all
                                     // extensions because we only open browser
                                     // window if the Profile is not locked.
                                     // Hence there is no extension blocked.
      profile_, Profile::CREATE_STATUS_INITIALIZED);
}

void ProfilePickerSignedInFlowController::FinishAndOpenBrowserForSAML() {
  DCHECK(IsInitialized());
  // First, free up `contents()` to be moved to a new browser window.
  host_->ShowScreenInSystemContents(
      GURL(url::kAboutBlankURL),
      /*navigation_finished_closure=*/
      base::BindOnce(
          &ProfilePickerSignedInFlowController::OnSignInContentsFreedUp,
          // Unretained is enough as the callback is called by a
          // member of `host_` that outlives `this`.
          base::Unretained(this)));
}

void ProfilePickerSignedInFlowController::OnSignInContentsFreedUp() {
  DCHECK(IsInitialized());
  DCHECK(!is_finished_);
  is_finished_ = true;

  DCHECK(name_for_signed_in_profile_.empty());
  name_for_signed_in_profile_ =
      profiles::GetDefaultNameForNewEnterpriseProfile();
  contents_->SetDelegate(nullptr);
  FinishAndOpenBrowserImpl(
      base::BindOnce(&ContinueSAMLSignin, std::move(contents_)));
}

void ProfilePickerSignedInFlowController::OnBrowserOpened(
    BrowserOpenedCallback finish_flow_callback,
    Profile* profile,
    Profile::CreateStatus profile_create_status) {
  DCHECK(IsInitialized());
  CHECK_EQ(profile, profile_);

  // Hide the flow window. This posts a task on the message loop to destroy the
  // window incl. this view.
  host_->Clear();

  if (!finish_flow_callback)
    return;

  Browser* browser = chrome::FindLastActiveWithProfile(profile);
  CHECK(browser);
  std::move(finish_flow_callback).Run(browser);
}
