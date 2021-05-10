// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_view.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/accelerator_table.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/profiles/profile_customization_bubble_sync_controller.h"
#include "chrome/browser/ui/views/profiles/profile_customization_bubble_view.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_sync_delegate.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "chrome/browser/ui/webui/signin/profile_picker_ui.h"
#include "chrome/browser/ui/webui/signin/signin_web_dialog_ui.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/google_chrome_strings.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/vector_icons/vector_icons.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if defined(OS_WIN)
#include "chrome/browser/shell_integration_win.h"
#include "ui/base/win/shell.h"
#include "ui/views/win/hwnd_util.h"
#endif

#if defined(OS_MAC)
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#endif

namespace {

ProfilePickerView* g_profile_picker_view = nullptr;
base::OnceClosure* g_profile_picker_opened_callback_for_testing = nullptr;

constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 758;
constexpr float kMaxRatioOfWorkArea = 0.9;

// Padding of elements in the simple toolbar.
constexpr gfx::Insets kToolbarPadding = gfx::Insets(8);

constexpr base::TimeDelta kExtendedAccountInfoTimeout =
    base::TimeDelta::FromSeconds(10);

constexpr int kSupportedAcceleratorCommands[] = {
    IDC_CLOSE_TAB,       IDC_CLOSE_WINDOW, IDC_EXIT,  IDC_FULLSCREEN,
    IDC_MINIMIZE_WINDOW, IDC_BACK,         IDC_RELOAD};

void ShowCustomizationBubble(SkColor new_profile_color, Browser* browser) {
  views::View* anchor_view = BrowserView::GetBrowserViewForBrowser(browser)
                                 ->toolbar_button_provider()
                                 ->GetAvatarToolbarButton();
  DCHECK(anchor_view);

  if (ProfileCustomizationBubbleSyncController::CanThemeSyncStart(
          browser->profile())) {
    // For sync users, their profile color has not been applied yet. Call a
    // helper class that applies the color and shows the bubble only if there is
    // no conflict with a synced theme / color.
    ProfileCustomizationBubbleSyncController::
        ApplyColorAndShowBubbleWhenNoValueSynced(
            browser->profile(), anchor_view,
            /*suggested_profile_color=*/new_profile_color);
  } else {
    // For non syncing users, simply show the bubble.
    ProfileCustomizationBubbleView::CreateBubble(browser->profile(),
                                                 anchor_view);
  }
}

GURL CreateURLForEntryPoint(ProfilePicker::EntryPoint entry_point) {
  GURL base_url = GURL(chrome::kChromeUIProfilePickerUrl);
  switch (entry_point) {
    case ProfilePicker::EntryPoint::kOnStartup: {
      GURL::Replacements replacements;
      replacements.SetQueryStr(chrome::kChromeUIProfilePickerStartupQuery);
      return base_url.ReplaceComponents(replacements);
    }
    case ProfilePicker::EntryPoint::kProfileMenuManageProfiles:
    case ProfilePicker::EntryPoint::kOpenNewWindowAfterProfileDeletion:
    case ProfilePicker::EntryPoint::kNewSessionOnExistingProcess:
    case ProfilePicker::EntryPoint::kProfileLocked:
    case ProfilePicker::EntryPoint::kUnableToCreateBrowser:
    case ProfilePicker::EntryPoint::kBackgroundModeManager:
      return base_url;
    case ProfilePicker::EntryPoint::kProfileMenuAddNewProfile:
      return base_url.Resolve("new-profile");
  }
}

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
  if (gaia::IsGaiaSignonRealm(url.GetOrigin()))
    return false;
  return true;
}

void ContinueSAMLSignin(std::unique_ptr<content::WebContents> saml_wc,
                        Browser* browser) {
  DCHECK(browser);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  // Attach DiceTabHelper to `saml_wc` so that sync consent dialog appears after
  // a successful sign-in.
  DiceTabHelper::CreateForWebContents(saml_wc.get());
  DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(saml_wc.get());
  // Use |redirect_url| and not |continue_url|, so that the DiceTabHelper can
  // redirect to chrome:// URLs such as the NTP.
  tab_helper->InitializeSigninFlow(
      GetSigninURL(browser_view->GetNativeTheme()->ShouldUseDarkColors()),
      signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
      signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
      GURL(chrome::kChromeUINewTabURL));

  browser->tab_strip_model()->ReplaceWebContentsAt(0, std::move(saml_wc));

  ProfileMetrics::LogProfileAddSignInFlowOutcome(
      ProfileMetrics::ProfileAddSignInFlowOutcome::kSAML);
}

class ProfilePickerWidget : public views::Widget {
 public:
  explicit ProfilePickerWidget(ProfilePickerView* profile_picker_view)
      : profile_picker_view_(profile_picker_view) {
    views::Widget::InitParams params;
    params.delegate = profile_picker_view_;
    Init(std::move(params));
  }
  ~ProfilePickerWidget() override = default;

  // views::Widget:
  const ui::ThemeProvider* GetThemeProvider() const override {
    return profile_picker_view_->GetThemeProviderForProfileBeingCreated();
  }

 private:
  ProfilePickerView* const profile_picker_view_;
};

class SimpleBackButton : public ToolbarButton {
 public:
  explicit SimpleBackButton(PressedCallback callback)
      : ToolbarButton(std::move(callback)) {
    SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                             ui::EF_MIDDLE_MOUSE_BUTTON);
    SetTooltipText(
        l10n_util::GetStringUTF16(IDS_PROFILE_PICKER_BACK_BUTTON_LABEL));
    // Unlike toolbar buttons, this one should be focusable to make it
    // consistent with other screens of the flow where the back button is part
    // of the page.
    SetFocusBehavior(FocusBehavior::ALWAYS);
  }
  SimpleBackButton(const SimpleBackButton&) = delete;
  SimpleBackButton& operator=(const SimpleBackButton&) = delete;
  ~SimpleBackButton() override = default;

  // ToolbarButton:
  void UpdateIcon() override {
    const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
    const gfx::VectorIcon* image =
        touch_ui ? &kBackArrowTouchIcon : &vector_icons::kBackArrowIcon;
    UpdateIconsWithStandardColors(*image);
  }
};

}  // namespace

// static
void ProfilePicker::Show(EntryPoint entry_point,
                         const GURL& on_select_profile_target_url) {
  if (!g_profile_picker_view)
    g_profile_picker_view = new ProfilePickerView(on_select_profile_target_url);

  g_profile_picker_view->Display(entry_point);
}

// static
GURL ProfilePicker::GetOnSelectProfileTargetUrl() {
  if (g_profile_picker_view) {
    return g_profile_picker_view->GetOnSelectProfileTargetUrl();
  }
  return GURL();
}

// static
void ProfilePicker::SwitchToSignIn(
    SkColor profile_color,
    base::OnceCallback<void(bool)> switch_finished_callback) {
  if (g_profile_picker_view) {
    g_profile_picker_view->SwitchToSignIn(profile_color,
                                          std::move(switch_finished_callback));
  }
}

// static
void ProfilePicker::CancelSignIn() {
  if (g_profile_picker_view) {
    g_profile_picker_view->CancelSignIn();
  }
}

// static
void ProfilePicker::SwitchToSyncConfirmation() {
  if (g_profile_picker_view) {
    g_profile_picker_view->SwitchToSyncConfirmation();
  }
}

// static
void ProfilePicker::ShowDialog(content::BrowserContext* browser_context,
                               const GURL& url,
                               const base::FilePath& profile_path) {
  if (g_profile_picker_view) {
    g_profile_picker_view->ShowDialog(browser_context, url, profile_path);
  }
}

// static
void ProfilePicker::HideDialog() {
  if (g_profile_picker_view) {
    g_profile_picker_view->HideDialog();
  }
}

// static
base::FilePath ProfilePicker::GetForceSigninProfilePath() {
  if (g_profile_picker_view) {
    return g_profile_picker_view->GetForceSigninProfilePath();
  }

  return base::FilePath();
}

// static
void ProfilePicker::Hide() {
  if (g_profile_picker_view)
    g_profile_picker_view->Clear();
}

// static
bool ProfilePicker::IsOpen() {
  return g_profile_picker_view;
}

bool ProfilePicker::IsActive() {
  if (!IsOpen())
    return false;

#if defined(OS_MAC)
  return g_profile_picker_view->GetWidget()->IsVisible();
#else
  return g_profile_picker_view->GetWidget()->IsActive();
#endif
}

// static
views::WebView* ProfilePicker::GetWebViewForTesting() {
  if (!g_profile_picker_view)
    return nullptr;
  return g_profile_picker_view->web_view_;
}

// static
views::View* ProfilePicker::GetViewForTesting() {
  return g_profile_picker_view;
}

// static
views::View* ProfilePicker::GetToolbarForTesting() {
  if (!g_profile_picker_view)
    return nullptr;
  return g_profile_picker_view->toolbar_;
}

// static
void ProfilePicker::AddOnProfilePickerOpenedCallbackForTesting(
    base::OnceClosure callback) {
  DCHECK(!g_profile_picker_opened_callback_for_testing);
  DCHECK(!callback.is_null());
  g_profile_picker_opened_callback_for_testing =
      new base::OnceClosure(std::move(callback));
}

// static
void ProfilePicker::SetExtendedAccountInfoTimeoutForTesting(
    base::TimeDelta timeout) {
  if (g_profile_picker_view) {
    g_profile_picker_view->SetExtendedAccountInfoTimeoutForTesting(  // IN-TEST
        timeout);
  }
}

// ProfilePickerForceSigninDialog
// -------------------------------------------------------------

// static
void ProfilePickerForceSigninDialog::ShowUnlockDialog(
    content::BrowserContext* browser_context,
    const std::string& email) {
  ShowUnlockDialogWithProfilePath(browser_context, email, base::FilePath());
}

// static
void ProfilePickerForceSigninDialog::ShowUnlockDialogWithProfilePath(
    content::BrowserContext* browser_context,
    const std::string& email,
    const base::FilePath& profile_path) {
  // This method should only be called if the profile picker is already showing.
  if (!ProfilePicker::IsActive())
    return;
  // Load the re-auth URL, prepopulated with the user's email address.
  // Add the index of the profile to the URL so that the inline login page
  // knows which profile to load and update the credentials.
  GURL url = signin::GetEmbeddedReauthURLWithEmail(
      signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
      signin_metrics::Reason::REASON_UNLOCK, email);
  ProfilePicker::ShowDialog(browser_context, url, profile_path);
}

// static
void ProfilePickerForceSigninDialog::ShowForceSigninDialog(
    content::BrowserContext* browser_context,
    const base::FilePath& profile_path) {
  if (!ProfilePicker::IsActive())
    return;

  GURL url = signin::GetEmbeddedPromoURL(
      signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
      signin_metrics::Reason::REASON_FORCED_SIGNIN_PRIMARY_ACCOUNT, true);

  ProfilePicker::ShowDialog(browser_context, url, profile_path);
}

void ProfilePickerForceSigninDialog::ShowDialogAndDisplayErrorMessage(
    content::BrowserContext* browser_context) {
  if (!ProfilePicker::IsActive())
    return;

  GURL url(chrome::kChromeUISigninErrorURL);
  ProfilePicker::ShowDialog(browser_context, url, base::FilePath());
  return;
}

// static
void ProfilePickerForceSigninDialog::DisplayErrorMessage() {
  if (g_profile_picker_view) {
    g_profile_picker_view->DisplayErrorMessage();
  }
}

// static
void ProfilePickerForceSigninDialog::HideDialog() {
  ProfilePicker::HideDialog();
}

// ProfilePickerView ----------------------------------------------------------

const ui::ThemeProvider*
ProfilePickerView::GetThemeProviderForProfileBeingCreated() const {
  if (!sign_in_)
    return nullptr;
  return &ThemeService::GetThemeProviderForProfile(sign_in_->profile);
}

ProfilePickerView::ProfilePickerView(const GURL& on_select_profile_target_url)
    : keep_alive_(KeepAliveOrigin::USER_MANAGER_VIEW,
                  KeepAliveRestartOption::DISABLED),
      extended_account_info_timeout_(kExtendedAccountInfoTimeout),
      on_select_profile_target_url_(on_select_profile_target_url) {
  // Setup the WidgetDelegate.
  SetHasWindowSizeControls(true);
  SetTitle(IDS_PRODUCT_NAME);

  ConfigureAccelerators();
  // TODO(crbug.com/1063856): Add |RecordDialogCreation|.
}

ProfilePickerView::~ProfilePickerView() {
  if (system_profile_contents_)
    system_profile_contents_->SetDelegate(nullptr);

  // Log profile creation flow abortion.
  if (sign_in_ && state_ != kFinalizing) {
    if (sign_in_->name_for_signed_in_profile.empty()) {
      ProfileMetrics::LogProfileAddSignInFlowOutcome(
          ProfileMetrics::ProfileAddSignInFlowOutcome::kAbortedBeforeSignIn);
    } else {
      ProfileMetrics::LogProfileAddSignInFlowOutcome(
          ProfileMetrics::ProfileAddSignInFlowOutcome::kAbortedAfterSignIn);
    }
  }
}

ProfilePickerView::SignInFlow::SignInFlow(ProfilePickerView* observer,
                                          Profile* profile,
                                          SkColor profile_color)
    : profile(profile),
      profile_color(profile_color),
      identity_manager_observation(observer) {
  // Listen for sign-in getting completed.
  identity_manager_observation.Observe(
      IdentityManagerFactory::GetForProfile(profile));

  contents =
      content::WebContents::Create(content::WebContents::CreateParams(profile));
  contents->SetDelegate(observer);

  // Create a manager that supports modal dialogs, such as for webauthn.
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      contents.get());
  web_modal::WebContentsModalDialogManager::FromWebContents(contents.get())
      ->SetDelegate(observer);
}

ProfilePickerView::SignInFlow::~SignInFlow() {
  if (contents)
    contents->SetDelegate(nullptr);
}

void ProfilePickerView::Display(ProfilePicker::EntryPoint entry_point) {
  // Record creation metrics.
  base::UmaHistogramEnumeration("ProfilePicker.Shown", entry_point);
  if (entry_point == ProfilePicker::EntryPoint::kOnStartup) {
    DCHECK(creation_time_on_startup_.is_null());
    // Display() is called right after the creation of this object.
    creation_time_on_startup_ = base::TimeTicks::Now();
    base::UmaHistogramTimes("ProfilePicker.StartupTime.BeforeCreation",
                            creation_time_on_startup_ -
                                startup_metric_utils::MainEntryPointTicks());
  }

  if (state_ == kNotStarted) {
    state_ = kInitializing;
    entry_point_ = entry_point;
    // Build the layout synchronously before creating the system profile to
    // simplify tests.
    BuildLayout();

    g_browser_process->profile_manager()->CreateProfileAsync(
        ProfileManager::GetSystemProfilePath(),
        base::BindRepeating(&ProfilePickerView::OnSystemProfileCreated,
                            weak_ptr_factory_.GetWeakPtr()),
        /*name=*/base::string16(), /*icon_url=*/std::string());
    return;
  }

  if (state_ == kInitializing)
    return;

  GetWidget()->Activate();
}

void ProfilePickerView::Clear() {
  if (state_ == kReady || state_ == kFinalizing) {
    GetWidget()->Close();
    return;
  }

  WindowClosing();
  DeleteDelegate();
}

void ProfilePickerView::OnSystemProfileCreated(
    Profile* system_profile,
    Profile::CreateStatus status) {
  DCHECK_NE(status, Profile::CREATE_STATUS_LOCAL_FAIL);
  if (status != Profile::CREATE_STATUS_INITIALIZED)
    return;

  Init(system_profile);
}

void ProfilePickerView::Init(Profile* system_profile) {
  DCHECK_EQ(state_, kInitializing);
  system_profile_contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(system_profile));
  system_profile_contents_->SetDelegate(this);
  // To record metrics using javascript, extensions are needed.
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      system_profile_contents_.get());

  // The widget is owned by the native widget.
  new ProfilePickerWidget(this);

#if defined(OS_WIN)
  // Set the app id for the user manager to the app id of its parent.
  ui::win::SetAppIdForWindow(
      shell_integration::win::GetAppUserModelIdForBrowser(
          system_profile->GetPath()),
      views::HWNDForWidget(GetWidget()));
#endif

  ShowScreen(system_profile_contents_.get(),
             CreateURLForEntryPoint(entry_point_), /*show_toolbar=*/false);
  GetWidget()->Show();
  state_ = kReady;

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kBrowserProfilePickerShown, true);

  if (entry_point_ == ProfilePicker::EntryPoint::kOnStartup) {
    DCHECK(!creation_time_on_startup_.is_null());
    base::UmaHistogramTimes("ProfilePicker.StartupTime.WebViewCreated",
                            base::TimeTicks::Now() - creation_time_on_startup_);
  }

  if (g_profile_picker_opened_callback_for_testing) {
    std::move(*g_profile_picker_opened_callback_for_testing).Run();
    delete g_profile_picker_opened_callback_for_testing;
    g_profile_picker_opened_callback_for_testing = nullptr;
  }
}

void ProfilePickerView::SwitchToSignIn(
    SkColor profile_color,
    base::OnceCallback<void(bool)> switch_finished_callback) {
  if (sign_in_) {
    // The profile is already created (the user went back and forth again). No
    // need to create it again.
    std::move(switch_finished_callback).Run(true);
    // Do not load any url because the desired sign-in screen is still loaded in
    // `sign_in_->contents`.
    ShowScreen(sign_in_->contents.get(), GURL(),
               /*show_toolbar=*/true);
    return;
  }

  size_t icon_index = profiles::GetPlaceholderAvatarIndex();
  // Silently create the new profile for browsing on GAIA (so that the sign-in
  // cookies are stored in the right profile).
  ProfileManager::CreateMultiProfileAsync(
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .ChooseNameForNewProfile(icon_index),
      profiles::GetDefaultAvatarIconUrl(icon_index),
      base::BindRepeating(&ProfilePickerView::OnProfileForSigninCreated,
                          weak_ptr_factory_.GetWeakPtr(), profile_color,
                          base::AdaptCallbackForRepeating(
                              std::move(switch_finished_callback))));
}

void ProfilePickerView::CancelSignIn() {
  DCHECK(sign_in_);
  DCHECK_EQ(web_view_->GetWebContents(), sign_in_->contents.get());

  g_browser_process->profile_manager()->ScheduleProfileForDeletion(
      sign_in_->profile->GetPath(), base::DoNothing());

  switch (entry_point_) {
    case ProfilePicker::EntryPoint::kOnStartup:
    case ProfilePicker::EntryPoint::kProfileMenuManageProfiles:
    case ProfilePicker::EntryPoint::kOpenNewWindowAfterProfileDeletion:
    case ProfilePicker::EntryPoint::kNewSessionOnExistingProcess:
    case ProfilePicker::EntryPoint::kProfileLocked:
    case ProfilePicker::EntryPoint::kUnableToCreateBrowser:
    case ProfilePicker::EntryPoint::kBackgroundModeManager: {
      // Navigate to the very beginning which is guaranteed to be the profile
      // picker.
      system_profile_contents_->GetController().GoToIndex(0);
      ShowScreen(system_profile_contents_.get(), GURL(),
                 /*show_toolbar=*/false);
      // Reset the sign-in flow.
      sign_in_.reset();
      toolbar_->RemoveAllChildViews(/*delete_children=*/true);
      return;
    }
    case ProfilePicker::EntryPoint::kProfileMenuAddNewProfile: {
      Clear();
      return;
    }
  }
}

void ProfilePickerView::OnProfileForSigninCreated(
    SkColor profile_color,
    base::RepeatingCallback<void(bool)> switch_finished_callback,
    Profile* profile,
    Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_LOCAL_FAIL) {
    std::move(switch_finished_callback).Run(false);
    return;
  } else if (status != Profile::CREATE_STATUS_INITIALIZED) {
    return;
  }

  DCHECK(profile);
  std::move(switch_finished_callback).Run(true);

  // Apply the default theme to get consistent colors for toolbars (this matters
  // for linux where the 'system' theme is used for new profiles).
  auto* theme_service = ThemeServiceFactory::GetForProfile(profile);
  theme_service->UseDefaultTheme();
  if (signin_util::IsForceSigninEnabled()) {
    // Show the embedded sign-in flow if the force signin is enabled.
    ProfilePickerForceSigninDialog::ShowForceSigninDialog(
        web_view_->GetWebContents()->GetBrowserContext(), profile->GetPath());
    return;
  }

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (!entry) {
    NOTREACHED();
    return;
  }

  // Mark this profile ephemeral so that it is deleted upon next startup if the
  // browser crashes before finishing the flow.
  entry->SetIsEphemeral(true);
  // Mark this profile as omitted so that it is not displayed in the list of
  // profiles.
  entry->SetIsOmitted(true);

  // Record that the sign in process starts (its end is recorded automatically
  // by the instance of DiceTurnSyncOnHelper constructed later on).
  signin_metrics::RecordSigninUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
  signin_metrics::LogSigninAccessPointStarted(
      signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);

  sign_in_ = std::make_unique<SignInFlow>(this, profile, profile_color);

  // Build the toolbar. Do it as late as here because the button depends on the
  // ThemeProvider which is available only by `sign_in_`.
  auto back_button = std::make_unique<SimpleBackButton>(base::BindRepeating(
      &ProfilePickerView::BackButtonPressed, base::Unretained(this)));
  toolbar_->AddChildView(std::move(back_button));

  // Make sure the web contents used for sign-in has proper background to match
  // the toolbar (for dark mode).
  SkColor background_color =
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR);
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      sign_in_->contents.get(), background_color);
  // On Mac, the WebContents is initially transparent. Set the color for the
  // main view as well.
  SetBackground(views::CreateSolidBackground(background_color));

  UpdateToolbarColor();

  ShowScreen(sign_in_->contents.get(),
             GetSigninURL(GetNativeTheme()->ShouldUseDarkColors()),
             /*show_toolbar=*/true);
}

void ProfilePickerView::SwitchToSyncConfirmation() {
  // The sync confirmation screen cannot render in the system profile web
  // contents and thus `sign_in_->contents` is used for this. As there is no
  // back button on the confirmation screen, the performance of going back to
  // the signin screen is no concern any more.
  ShowScreen(sign_in_->contents.get(),
             GURL(chrome::kChromeUISyncConfirmationURL),
             /*show_toolbar=*/false,
             /*enable_navigating_back=*/false);

  SyncConfirmationUI* sync_confirmation_ui = static_cast<SyncConfirmationUI*>(
      sign_in_->contents->GetWebUI()->GetController());
  sync_confirmation_ui->InitializeMessageHandlerForCreationFlow(
      sign_in_->profile_color);
}

void ProfilePickerView::WindowClosing() {
  // Now that the window is closed, we can allow a new one to be opened.
  // (WindowClosing comes in asynchronously from the call to Close() and we
  // may have already opened a new instance).
  if (g_profile_picker_view == this)
    g_profile_picker_view = nullptr;
}

views::ClientView* ProfilePickerView::CreateClientView(views::Widget* widget) {
  return new views::ClientView(widget, TransferOwnershipOfContentsView());
}

views::View* ProfilePickerView::GetContentsView() {
  return this;
}

base::string16 ProfilePickerView::GetAccessibleWindowTitle() const {
  if (!web_view_ || !web_view_->GetWebContents() ||
      web_view_->GetWebContents()->GetTitle().empty()) {
    return l10n_util::GetStringUTF16(IDS_PROFILE_PICKER_MAIN_VIEW_TITLE);
  }
  return web_view_->GetWebContents()->GetTitle();
}

gfx::Size ProfilePickerView::CalculatePreferredSize() const {
  gfx::Size preferred_size = gfx::Size(kWindowWidth, kWindowHeight);
  gfx::Size work_area_size = GetWidget()->GetWorkAreaBoundsInScreen().size();
  // Keep the window smaller then |work_area_size| so that it feels more like a
  // dialog then like the actual Chrome window.
  gfx::Size max_dialog_size = ScaleToFlooredSize(
      work_area_size, kMaxRatioOfWorkArea, kMaxRatioOfWorkArea);
  preferred_size.SetToMin(max_dialog_size);
  return preferred_size;
}

gfx::Size ProfilePickerView::GetMinimumSize() const {
  // On small screens, the preferred size may be smaller than the picker
  // minimum size. In that case there will be scrollbars on the picker.
  gfx::Size minimum_size = GetPreferredSize();
  minimum_size.SetToMin(ProfilePickerUI::GetMinimumSize());
  return minimum_size;
}

bool ProfilePickerView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  // Ignore presses of the Escape key. The profile picker may be Chrome's only
  // top-level window, in which case we don't want presses of Esc to maybe quit
  // the entire browser. This has higher priority than the default dialog Esc
  // accelerator (which would otherwise close the window).
  if (accelerator.key_code() == ui::VKEY_ESCAPE &&
      accelerator.modifiers() == ui::EF_NONE) {
    return true;
  }

  const auto& iter = accelerator_table_.find(accelerator);
  DCHECK(iter != accelerator_table_.end());
  int command_id = iter->second;
  switch (command_id) {
    case IDC_CLOSE_TAB:
    case IDC_CLOSE_WINDOW:
      // kEscKeyPressed is used although that shortcut is disabled (this is
      // Ctrl-Shift-W instead).
      GetWidget()->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
      break;
    case IDC_EXIT:
      chrome::AttemptUserExit();
      break;
    case IDC_FULLSCREEN:
      GetWidget()->SetFullscreen(!GetWidget()->IsFullscreen());
      break;
    case IDC_MINIMIZE_WINDOW:
      GetWidget()->Minimize();
      break;
    case IDC_BACK: {
      NavigateBack();
      break;
    }
    // Always reload bypassing cache.
    case IDC_RELOAD:
    case IDC_RELOAD_BYPASSING_CACHE:
    case IDC_RELOAD_CLEARING_CACHE: {
      // Sign-in may fail due to connectivity issues, allow reloading.
      if (GetSigningIn()) {
        sign_in_->contents->GetController().Reload(
            content::ReloadType::BYPASSING_CACHE, true);
      }
      break;
    }
    default:
      NOTREACHED() << "Unexpected command_id: " << command_id;
      break;
  }

  return true;
}

void ProfilePickerView::OnThemeChanged() {
  views::WidgetDelegateView::OnThemeChanged();
  if (!GetSigningIn())
    return;
  UpdateToolbarColor();
}

bool ProfilePickerView::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // Ignores context menu.
  return true;
}

bool ProfilePickerView::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  // Forward the keyboard event to AcceleratorPressed() through the
  // FocusManager.
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

void ProfilePickerView::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture,
    bool* was_blocked) {
  DCHECK(sign_in_) << "Opening new tabs should only happen within GAIA signin";
  NavigateParams params(sign_in_->profile, target_url,
                        ui::PAGE_TRANSITION_LINK);
  // Open all links as new popups.
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.contents_to_insert = std::move(new_contents);
  params.window_bounds = initial_rect;
  Navigate(&params);
}

void ProfilePickerView::NavigationStateChanged(
    content::WebContents* source,
    content::InvalidateTypes changed_flags) {
  if (GetSigningIn() && IsExternalURL(sign_in_->contents->GetVisibleURL()))
    FinishSignedInCreationFlowForSAML();
}

void ProfilePickerView::BuildLayout() {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                   views::MaximumFlexSizeRule::kUnbounded));

  auto toolbar = std::make_unique<views::View>();
  toolbar->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCollapseMargins(true)
      .SetInteriorMargin(kToolbarPadding);
  toolbar->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred));
  // It is important for tests that the toolbar starts visible (being empty).
  toolbar_ = AddChildView(std::move(toolbar));

  auto web_view = std::make_unique<views::WebView>();
  web_view->set_allow_accelerators(true);
  web_view_ = AddChildView(std::move(web_view));
}

void ProfilePickerView::UpdateToolbarColor() {
  DCHECK(sign_in_->contents);
  toolbar_->SetBackground(views::CreateSolidBackground(
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR)));
}

void ProfilePickerView::ShowScreen(content::WebContents* contents,
                                   const GURL& url,
                                   bool show_toolbar,
                                   bool enable_navigating_back) {
  web_view_->SetWebContents(contents);
  web_view_->RequestFocus();

  // Change visibility of the toolbar after swapping wc in `web_view_` to make
  // it easier for tests to detect changing of the screen.
  toolbar_->SetVisible(show_toolbar);

  enable_navigating_back_ = enable_navigating_back;

  if (!url.is_empty()) {
    // Make sure to load the url as the last step so that the UI state is
    // coherent upon the NavigationStateChanged notification.
    contents->GetController().LoadURL(url, content::Referrer(),
                                      ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                      std::string());
  }
}

void ProfilePickerView::BackButtonPressed(const ui::Event& event) {
  NavigateBack();
}

void ProfilePickerView::NavigateBack() {
  if (!enable_navigating_back_)
    return;

  if (web_view_->GetWebContents()->GetController().CanGoBack()) {
    web_view_->GetWebContents()->GetController().GoBack();
    return;
  }

  // Move from sign-in back to the previous screen of profile creation.
  // Do not load any url because the desired screen is still loaded in
  // `system_profile_contents_`.
  if (GetSigningIn())
    ShowScreen(system_profile_contents_.get(), GURL(), /*show_toolbar=*/false);
}

bool ProfilePickerView::GetSigningIn() const {
  return (state_ == kReady || state_ == kFinalizing) && toolbar_->GetVisible();
}

void ProfilePickerView::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  DCHECK(!account_info.IsEmpty());

  base::OnceClosure sync_consent_completed_closure = base::BindOnce(
      &ProfilePickerView::FinishSignedInCreationFlow,
      weak_ptr_factory_.GetWeakPtr(),
      base::BindOnce(&ShowCustomizationBubble, sign_in_->profile_color),
      /*enterprise_sync_consent_needed=*/false);

  // Stop with the sign-in navigation, it is not needed any more and this avoids
  // any glitches of the redirect page getting displayed. This is needed because
  // in some cases (such as managed signed-in), there are further delays before
  // any follow-up UI is shown.
  ShowScreen(sign_in_->contents.get(), GURL(url::kAboutBlankURL),
             /*show_toolbar=*/true, /*enable_navigating_back=*/false);

  // Set up a timeout for extended account info (which cancels any existing
  // timeout closure).
  sign_in_->extended_account_info_timeout_closure.Reset(
      base::BindOnce(&ProfilePickerView::OnExtendedAccountInfoTimeout,
                     weak_ptr_factory_.GetWeakPtr(), account_info));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, sign_in_->extended_account_info_timeout_closure.callback(),
      extended_account_info_timeout_);

  // DiceTurnSyncOnHelper deletes itself once done.
  new DiceTurnSyncOnHelper(
      sign_in_->profile, signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
      signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT,
      account_info.account_id,
      DiceTurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
      std::make_unique<ProfilePickerViewSyncDelegate>(
          sign_in_->profile,
          base::BindOnce(&ProfilePickerView::FinishSignedInCreationFlow,
                         weak_ptr_factory_.GetWeakPtr())),
      std::move(sync_consent_completed_closure));
}

void ProfilePickerView::OnExtendedAccountInfoUpdated(
    const AccountInfo& account_info) {
  if (!account_info.IsValid())
    return;
  sign_in_->name_for_signed_in_profile =
      profiles::GetDefaultNameForNewSignedInProfile(account_info);
  OnProfileNameAvailable();
  // Extended info arrived on time, no need for the timeout callback any more.
  sign_in_->extended_account_info_timeout_closure.Cancel();
}

web_modal::WebContentsModalDialogHost*
ProfilePickerView::GetWebContentsModalDialogHost() {
  return this;
}

gfx::NativeView ProfilePickerView::GetHostView() const {
  return GetWidget()->GetNativeView();
}

gfx::Point ProfilePickerView::GetDialogPosition(const gfx::Size& size) {
  gfx::Size widget_size = GetWidget()->GetWindowBoundsInScreen().size();
  return gfx::Point(std::max(0, (widget_size.width() - size.width()) / 2), 0);
}

gfx::Size ProfilePickerView::GetMaximumDialogSize() {
  return GetWidget()->GetWindowBoundsInScreen().size();
}

void ProfilePickerView::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {}

void ProfilePickerView::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {}

void ProfilePickerView::SetExtendedAccountInfoTimeoutForTesting(
    base::TimeDelta timeout) {
  extended_account_info_timeout_ = timeout;
}

void ProfilePickerView::OnExtendedAccountInfoTimeout(
    const CoreAccountInfo& account) {
  sign_in_->name_for_signed_in_profile =
      profiles::GetDefaultNameForNewSignedInProfileWithIncompleteInfo(account);
  OnProfileNameAvailable();
}

void ProfilePickerView::OnProfileNameAvailable() {
  // Stop listening to further changes.
  DCHECK(sign_in_->identity_manager_observation.IsObservingSource(
      IdentityManagerFactory::GetForProfile(sign_in_->profile)));
  sign_in_->identity_manager_observation.Reset();

  if (sign_in_->on_profile_name_available)
    std::move(sign_in_->on_profile_name_available).Run();
}

void ProfilePickerView::FinishSignedInCreationFlow(
    BrowserOpenedCallback callback,
    bool enterprise_sync_consent_needed) {
  // This can get called first time from a special case handling (such as the
  // Settings link) and than second time when the consent flow finishes. We need
  // to make sure only the first call gets handled.
  if (state_ == kFinalizing)
    return;
  state_ = kFinalizing;

  if (sign_in_->name_for_signed_in_profile.empty()) {
    sign_in_->on_profile_name_available =
        base::BindOnce(&ProfilePickerView::FinishSignedInCreationFlowImpl,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       enterprise_sync_consent_needed);
    return;
  }

  FinishSignedInCreationFlowImpl(std::move(callback),
                                 enterprise_sync_consent_needed);
}

void ProfilePickerView::FinishSignedInCreationFlowForSAML() {
  DCHECK_NE(state_, kFinalizing);
  state_ = kFinalizing;
  DCHECK(sign_in_->name_for_signed_in_profile.empty());

  sign_in_->name_for_signed_in_profile =
      profiles::GetDefaultNameForNewEnterpriseProfile();

  // Free up `sign_in_->contents` to be moved to a new browser window.
  sign_in_->contents->SetDelegate(nullptr);
  ShowScreen(system_profile_contents_.get(), GURL(url::kAboutBlankURL),
             /*show_toolbar=*/false, /*enable_navigating_back=*/false);
  FinishSignedInCreationFlowImpl(
      base::BindOnce(&ContinueSAMLSignin, std::move(sign_in_->contents)),
      /*enterprise_sync_consent_needed=*/true);
}

void ProfilePickerView::FinishSignedInCreationFlowImpl(
    BrowserOpenedCallback callback,
    bool enterprise_sync_consent_needed) {
  DCHECK(!sign_in_->name_for_signed_in_profile.empty());

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(sign_in_->profile->GetPath());
  if (!entry) {
    NOTREACHED();
    return;
  }

  entry->SetIsOmitted(false);
  if (!sign_in_->profile->GetPrefs()->GetBoolean(
          prefs::kForceEphemeralProfiles)) {
    // Unmark this profile ephemeral so that it isn't deleted upon next startup.
    // Profiles should never be made non-ephemeral if ephemeral mode is forced
    // by policy.
    entry->SetIsEphemeral(false);
  }
  entry->SetLocalProfileName(sign_in_->name_for_signed_in_profile,
                             /*is_default_name=*/false);
  ProfileMetrics::LogProfileAddNewUser(
      ProfileMetrics::ADD_NEW_PROFILE_PICKER_SIGNED_IN);

  // If sync is not enabled (and will not likely be enabled with an enterprise
  // consent), apply a new color to the profile (otherwise, a more complicated
  // logic gets triggered in ShowCustomizationBubble()).
  if (!enterprise_sync_consent_needed &&
      !ProfileCustomizationBubbleSyncController::CanThemeSyncStart(
          sign_in_->profile)) {
    auto* theme_service = ThemeServiceFactory::GetForProfile(sign_in_->profile);
    theme_service->BuildAutogeneratedThemeFromColor(sign_in_->profile_color);
  }

  // Skip the FRE for this profile as it's replaced by profile creation flow.
  sign_in_->profile->GetPrefs()->SetBoolean(prefs::kHasSeenWelcomePage, true);

  // TODO(crbug.com/1126913): Change the callback of
  // profiles::OpenBrowserWindowForProfile() to be a OnceCallback as it is only
  // called once.
  profiles::OpenBrowserWindowForProfile(
      base::AdaptCallbackForRepeating(
          base::BindOnce(&ProfilePickerView::OnBrowserOpened,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback))),
      /*always_create=*/false,   // Don't create a window if one already exists.
      /*is_new_profile=*/false,  // Don't create a first run window.
      /*unblock_extensions=*/false,  // There is no need to unblock all
                                     // extensions because we only open browser
                                     // window if the Profile is not locked.
                                     // Hence there is no extension blocked.
      sign_in_->profile, Profile::CREATE_STATUS_INITIALIZED);
}

void ProfilePickerView::OnBrowserOpened(
    BrowserOpenedCallback finish_flow_callback,
    Profile* profile,
    Profile::CreateStatus profile_create_status) {
  DCHECK_EQ(profile, sign_in_->profile);

  // Hide the flow window. This posts a task on the message loop to destroy the
  // window incl. this view.
  Clear();

  if (!finish_flow_callback)
    return;

  Browser* browser = chrome::FindLastActiveWithProfile(sign_in_->profile);
  DCHECK(browser);
  std::move(finish_flow_callback).Run(browser);
}

void ProfilePickerView::ConfigureAccelerators() {
  // By default, dialog views close when pressing escape. Override this
  // behavior as the profile picker should not close in that case.
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  const std::vector<AcceleratorMapping> accelerator_list(GetAcceleratorList());
  for (const auto& entry : accelerator_list) {
    if (!base::Contains(kSupportedAcceleratorCommands, entry.command_id))
      continue;
    ui::Accelerator accelerator(entry.keycode, entry.modifiers);
    accelerator_table_[accelerator] = entry.command_id;
    AddAccelerator(accelerator);
  }

#if defined(OS_MAC)
  // Check Mac-specific accelerators. Note: Chrome does not support dynamic or
  // user-configured accelerators on Mac. Default static accelerators are used
  // instead.
  for (int command_id : kSupportedAcceleratorCommands) {
    ui::Accelerator accelerator;
    bool mac_accelerator_found =
        GetDefaultMacAcceleratorForCommandId(command_id, &accelerator);
    if (mac_accelerator_found) {
      accelerator_table_[accelerator] = command_id;
      AddAccelerator(accelerator);
    }
  }
#endif  // OS_MAC
}

void ProfilePickerView::ShowDialog(content::BrowserContext* browser_context,
                                   const GURL& url,
                                   const base::FilePath& profile_path) {
  gfx::NativeView parent = GetWidget()->GetNativeView();
  dialog_host_.ShowDialog(browser_context, url, profile_path, parent);
}

void ProfilePickerView::HideDialog() {
  dialog_host_.HideDialog();
}

void ProfilePickerView::DisplayErrorMessage() {
  dialog_host_.DisplayErrorMessage();
}

base::FilePath ProfilePickerView::GetForceSigninProfilePath() const {
  return dialog_host_.GetForceSigninProfilePath();
}

GURL ProfilePickerView::GetOnSelectProfileTargetUrl() const {
  return on_select_profile_target_url_;
}

BEGIN_METADATA(ProfilePickerView, views::WidgetDelegateView)
ADD_READONLY_PROPERTY_METADATA(bool, SigningIn)
ADD_READONLY_PROPERTY_METADATA(base::FilePath, ForceSigninProfilePath)
END_METADATA
