// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/welcome_screen.h"

#include <utility>
#include <vector>

#include "ash/public/interfaces/constants.mojom.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/screens/welcome_view.h"
#include "chrome/browser/chromeos/login/ui/input_events_blocker.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

constexpr char kUserActionContinueButtonClicked[] = "continue";
constexpr char kUserActionConnectDebuggingFeaturesClicked[] =
    "connect-debugging-features";

}  // namespace

namespace chromeos {

///////////////////////////////////////////////////////////////////////////////
// WelcomeScreen, public:

// static
WelcomeScreen* WelcomeScreen::Get(ScreenManager* manager) {
  return static_cast<WelcomeScreen*>(
      manager->GetScreen(OobeScreen::SCREEN_OOBE_WELCOME));
}

WelcomeScreen::WelcomeScreen(BaseScreenDelegate* base_screen_delegate,
                             Delegate* delegate,
                             WelcomeView* view,
                             const base::RepeatingClosure& exit_callback)
    : BaseScreen(base_screen_delegate, OobeScreen::SCREEN_OOBE_WELCOME),
      view_(view),
      delegate_(delegate),
      exit_callback_(exit_callback),
      weak_factory_(this) {
  if (view_)
    view_->Bind(this);

  input_method::InputMethodManager::Get()->AddObserver(this);
  InitializeTimezoneObserver();
  OnSystemTimezoneChanged();
  UpdateLanguageList();
}

WelcomeScreen::~WelcomeScreen() {
  if (view_)
    view_->Unbind();

  input_method::InputMethodManager::Get()->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// WelcomeScreen, public API, setters and getters for input method and timezone.

void WelcomeScreen::OnViewDestroyed(WelcomeView* view) {
  if (view_ == view) {
    view_ = nullptr;
    timezone_subscription_.reset();
  }
}

void WelcomeScreen::UpdateLanguageList() {
  ScheduleResolveLanguageList(
      std::unique_ptr<locale_util::LanguageSwitchResult>());
}

void WelcomeScreen::SetApplicationLocaleAndInputMethod(
    const std::string& locale,
    const std::string& input_method) {
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  if (app_locale == locale || locale.empty()) {
    // If the locale doesn't change, set input method directly.
    SetInputMethod(input_method);
    return;
  }

  // Block UI while resource bundle is being reloaded.
  // (InputEventsBlocker will live until callback is finished.)
  locale_util::SwitchLanguageCallback callback(base::Bind(
      &WelcomeScreen::OnLanguageChangedCallback, weak_factory_.GetWeakPtr(),
      base::Owned(new chromeos::InputEventsBlocker), input_method));
  locale_util::SwitchLanguage(locale, true /* enableLocaleKeyboardLayouts */,
                              true /* login_layouts_only */, callback,
                              ProfileManager::GetActiveUserProfile());
}

std::string WelcomeScreen::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

std::string WelcomeScreen::GetInputMethod() const {
  return input_method_;
}

void WelcomeScreen::SetApplicationLocale(const std::string& locale) {
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  if (app_locale == locale || locale.empty())
    return;

  // Block UI while resource bundle is being reloaded.
  // (InputEventsBlocker will live until callback is finished.)
  locale_util::SwitchLanguageCallback callback(base::Bind(
      &WelcomeScreen::OnLanguageChangedCallback, weak_factory_.GetWeakPtr(),
      base::Owned(new chromeos::InputEventsBlocker), std::string()));
  locale_util::SwitchLanguage(locale, true /* enableLocaleKeyboardLayouts */,
                              true /* login_layouts_only */, callback,
                              ProfileManager::GetActiveUserProfile());
}

void WelcomeScreen::SetInputMethod(const std::string& input_method) {
  const std::vector<std::string>& input_methods =
      input_method::InputMethodManager::Get()
          ->GetActiveIMEState()
          ->GetActiveInputMethodIds();
  if (input_method.empty() ||
      !base::ContainsValue(input_methods, input_method)) {
    LOG(WARNING) << "The input method is empty or ineligible!";
    return;
  }

  if (input_method_ == input_method)
    return;

  input_method_ = input_method;
  input_method::InputMethodManager::Get()
      ->GetActiveIMEState()
      ->ChangeInputMethod(input_method_, false /* show_message */);
}

void WelcomeScreen::SetTimezone(const std::string& timezone_id) {
  if (timezone_id.empty())
    return;

  timezone_ = timezone_id;
  chromeos::system::SetSystemAndSigninScreenTimezone(timezone_id);
}

std::string WelcomeScreen::GetTimezone() const {
  return timezone_;
}

void WelcomeScreen::AddObserver(Observer* observer) {
  if (observer)
    observers_.AddObserver(observer);
}

void WelcomeScreen::RemoveObserver(Observer* observer) {
  if (observer)
    observers_.RemoveObserver(observer);
}

////////////////////////////////////////////////////////////////////////////////
// BaseScreen implementation:

void WelcomeScreen::Show() {
  // Here we should handle default locales, for which we do not have UI
  // resources. This would load fallback, but properly show "selected" locale
  // in the UI.
  if (selected_language_code_.empty()) {
    const StartupCustomizationDocument* startup_manifest =
        StartupCustomizationDocument::GetInstance();
    SetApplicationLocale(startup_manifest->initial_locale_default());
  }

  if (!timezone_subscription_)
    InitializeTimezoneObserver();

  // Automatically continue if we are using hands-off enrollment.
  if (WizardController::UsingHandsOffEnrollment()) {
    OnUserAction(kUserActionContinueButtonClicked);
  } else if (view_) {
    view_->Show();
  }
}

void WelcomeScreen::Hide() {
  timezone_subscription_.reset();
  if (view_)
    view_->Hide();
}

void WelcomeScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionContinueButtonClicked) {
    OnContinueButtonPressed();
  } else if (action_id == kUserActionConnectDebuggingFeaturesClicked) {
    if (delegate_)
      delegate_->OnEnableDebuggingScreenRequested();
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

////////////////////////////////////////////////////////////////////////////////
// WelcomeScreen, InputMethodManager::Observer implementation:

void WelcomeScreen::InputMethodChanged(
    input_method::InputMethodManager* manager,
    Profile* /* proflie */,
    bool /* show_message */) {
  if (view_) {
    view_->SetInputMethodId(
        manager->GetActiveIMEState()->GetCurrentInputMethod().id());
  }
}

////////////////////////////////////////////////////////////////////////////////
// WelcomeScreen, private:

void WelcomeScreen::InitializeTimezoneObserver() {
  timezone_subscription_ = CrosSettings::Get()->AddSettingsObserver(
      kSystemTimezone, base::Bind(&WelcomeScreen::OnSystemTimezoneChanged,
                                  base::Unretained(this)));
}

void WelcomeScreen::OnContinueButtonPressed() {
  if (view_) {
    view_->StopDemoModeDetection();
  }
  exit_callback_.Run();
}

void WelcomeScreen::OnLanguageChangedCallback(
    const InputEventsBlocker* /* input_events_blocker */,
    const std::string& input_method,
    const locale_util::LanguageSwitchResult& result) {
  if (!selected_language_code_.empty()) {
    // We still do not have device owner, so owner settings are not applied.
    // But Guest session can be started before owner is created, so we need to
    // save locale settings directly here.
    g_browser_process->local_state()->SetString(
        language::prefs::kApplicationLocale, selected_language_code_);
  }
  ScheduleResolveLanguageList(
      std::make_unique<locale_util::LanguageSwitchResult>(result));

  AccessibilityManager::Get()->OnLocaleChanged();
  SetInputMethod(input_method);
  NotifyLocaleChange();
}

void WelcomeScreen::ScheduleResolveLanguageList(
    std::unique_ptr<locale_util::LanguageSwitchResult> language_switch_result) {
  UILanguageListResolvedCallback callback = base::Bind(
      &WelcomeScreen::OnLanguageListResolved, weak_factory_.GetWeakPtr());
  ResolveUILanguageList(std::move(language_switch_result), callback);
}

void WelcomeScreen::OnLanguageListResolved(
    std::unique_ptr<base::ListValue> new_language_list,
    const std::string& new_language_list_locale,
    const std::string& new_selected_language) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  language_list_ = std::move(new_language_list);
  language_list_locale_ = new_language_list_locale;
  selected_language_code_ = new_selected_language;

  g_browser_process->local_state()->SetString(
      language::prefs::kApplicationLocale, selected_language_code_);
  if (view_)
    view_->ReloadLocalizedContent();
  for (auto& observer : observers_)
    observer.OnLanguageListReloaded();
}

void WelcomeScreen::OnSystemTimezoneChanged() {
  if (view_) {
    std::string current_timezone_id;
    CrosSettings::Get()->GetString(kSystemTimezone, &current_timezone_id);
    view_->SetTimezoneId(current_timezone_id);
  }
}

void WelcomeScreen::ConnectToLocaleUpdateController() {
  content::ServiceManagerConnection* connection =
      content::ServiceManagerConnection::GetForProcess();
  service_manager::Connector* connector =
      connection ? connection->GetConnector() : nullptr;
  // Unit tests may not have a connector.
  if (!connector)
    return;

  connector->BindInterface(ash::mojom::kServiceName,
                           &locale_update_controller_);
}

void WelcomeScreen::NotifyLocaleChange() {
  if (!locale_update_controller_)
    ConnectToLocaleUpdateController();

  DCHECK(locale_update_controller_);
  locale_update_controller_->OnLocaleChanged(
      std::string(), std::string(), std::string(),
      base::DoNothing::Once<ash::mojom::LocaleNotificationResult>());
}

}  // namespace chromeos
