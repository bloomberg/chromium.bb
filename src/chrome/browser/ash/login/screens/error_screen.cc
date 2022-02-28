// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/error_screen.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/ash/app_mode/certificate_manager_dialog.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/login/auth/chrome_login_performer.h"
#include "chrome/browser/ash/login/chrome_restart_request.h"
#include "chrome/browser/ash/login/ui/captive_portal_window_proxy.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/login_display_host_mojo.h"
#include "chrome/browser/ash/login/ui/webui_login_view.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/connectivity_diagnostics_dialog.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "chromeos/network/portal_detector/network_portal_detector_strategy.h"
#include "components/session_manager/core/session_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/gfx/native_widget_types.h"

namespace ash {
namespace {

// TODO(https://crbug.com/1241511): Remove this global.
bool g_offline_login_allowed_ = false;

// Additional flag applied on top of g_offline_login_allowed_ that can block
// visibility of offline login link when a focused pod user has offline login
// timer (set by policy) expired. If that happens flag is flipped to false.
// In all other cases it should be set to a default value of true.
// Even if a user gets to the (hidden) flow, the offline login may be blocked
// by checking the policy value there.
// TODO(https://crbug.com/1241511): Remove this global.
bool g_offline_login_per_user_allowed_ = true;

// Returns the current running kiosk app profile in a kiosk session. Otherwise,
// returns nullptr.
Profile* GetAppProfile() {
  return chrome::IsRunningInForcedAppMode()
             ? ProfileManager::GetActiveUserProfile()
             : nullptr;
}

}  // namespace

constexpr const char ErrorScreen::kUserActionConfigureCertsButtonClicked[] =
    "configure-certs";
constexpr const char ErrorScreen::kUserActionDiagnoseButtonClicked[] =
    "diagnose";
constexpr const char ErrorScreen::kUserActionLaunchOobeGuestSessionClicked[] =
    "launch-oobe-guest";
constexpr const char
    ErrorScreen::kUserActionLocalStateErrorPowerwashButtonClicked[] =
        "local-state-error-powerwash";
constexpr const char ErrorScreen::kUserActionRebootButtonClicked[] = "reboot";
constexpr const char ErrorScreen::kUserActionShowCaptivePortalClicked[] =
    "show-captive-portal";
constexpr const char ErrorScreen::kUserActionNetworkConnected[] =
    "network-connected";
constexpr const char ErrorScreen::kUserActionReloadGaia[] = "reload-gaia";
constexpr const char ErrorScreen::kUserActionCancelReset[] = "cancel-reset";
constexpr const char ErrorScreen::kUserActionCancel[] = "cancel";

ErrorScreen::ErrorScreen(ErrorScreenView* view)
    : BaseScreen(ErrorScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view) {
  network_state_informer_ = new NetworkStateInformer();
  network_state_informer_->Init();
  NetworkHandler::Get()->network_connection_handler()->AddObserver(this);
  if (view_)
    view_->Bind(this);
}

ErrorScreen::~ErrorScreen() {
  NetworkHandler::Get()->network_connection_handler()->RemoveObserver(this);
  if (view_)
    view_->Unbind();
}

void ErrorScreen::AllowGuestSignin(bool allowed) {
  if (view_)
    view_->SetGuestSigninAllowed(allowed);
}

void ErrorScreen::ShowOfflineLoginOption(bool show) {
  if (view_)
    view_->SetOfflineSigninAllowed(show);
}

void ErrorScreen::AllowOfflineLogin(bool allowed) {
  g_offline_login_allowed_ = allowed;
}

void ErrorScreen::AllowOfflineLoginPerUser(bool allowed) {
  g_offline_login_per_user_allowed_ = allowed;
}

void ErrorScreen::FixCaptivePortal() {
  MaybeInitCaptivePortalWindowProxy(
      LoginDisplayHost::default_host()->GetOobeWebContents());
  captive_portal_window_proxy_->ShowIfRedirected();
}

NetworkError::UIState ErrorScreen::GetUIState() const {
  return ui_state_;
}

NetworkError::ErrorState ErrorScreen::GetErrorState() const {
  return error_state_;
}

OobeScreenId ErrorScreen::GetParentScreen() const {
  return parent_screen_;
}

void ErrorScreen::HideCaptivePortal() {
  if (captive_portal_window_proxy_.get())
    captive_portal_window_proxy_->Close();
}

void ErrorScreen::OnViewDestroyed(ErrorScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void ErrorScreen::SetUIState(NetworkError::UIState ui_state) {
  ui_state_ = ui_state;
  if (view_)
    view_->SetUIState(ui_state);
}

void ErrorScreen::SetErrorState(NetworkError::ErrorState error_state,
                                const std::string& network) {
  error_state_ = error_state;
  if (view_) {
    view_->SetErrorStateCode(error_state);
    view_->SetErrorStateNetwork(network);
  }
}

void ErrorScreen::SetParentScreen(OobeScreenId parent_screen) {
  parent_screen_ = parent_screen;
  // Not really used on JS side yet so no need to propagate to screen context.
}

void ErrorScreen::SetHideCallback(base::OnceClosure on_hide) {
  on_hide_callback_ = std::move(on_hide);
}

void ErrorScreen::ShowCaptivePortal() {
  // This call is an explicit user action
  // i.e. clicking on link so force dialog show.
  FixCaptivePortal();
  captive_portal_window_proxy_->Show();
}

void ErrorScreen::ShowConnectingIndicator(bool show) {
  if (view_)
    view_->SetShowConnectingIndicator(show);
}

void ErrorScreen::SetIsPersistentError(bool is_persistent) {
  if (view_)
    view_->SetIsPersistentError(is_persistent);
}

base::CallbackListSubscription ErrorScreen::RegisterConnectRequestCallback(
    base::RepeatingClosure callback) {
  return connect_request_callbacks_.Add(std::move(callback));
}

void ErrorScreen::MaybeInitCaptivePortalWindowProxy(
    content::WebContents* web_contents) {
  if (!captive_portal_window_proxy_.get()) {
    captive_portal_window_proxy_ = std::make_unique<CaptivePortalWindowProxy>(
        network_state_informer_.get(), web_contents);
  }
}

void ErrorScreen::DoShow() {
  LOG(WARNING) << "Network error screen message is shown";
  session_manager::SessionManager::Get()->NotifyNetworkErrorScreenShown();
  network_portal_detector::GetInstance()->SetStrategy(
      PortalDetectorStrategy::STRATEGY_ID_ERROR_SCREEN);
}

void ErrorScreen::DoHide() {
  LOG(WARNING) << "Network error screen message is hidden";
  if (on_hide_callback_) {
    std::move(on_hide_callback_).Run();
    on_hide_callback_ = base::OnceClosure();
  }
  network_portal_detector::GetInstance()->SetStrategy(
      PortalDetectorStrategy::STRATEGY_ID_LOGIN_SCREEN);
}

void ErrorScreen::ShowNetworkErrorMessage(NetworkStateInformer::State state,
                                          NetworkError::ErrorReason reason) {
  const std::string network_path = network_state_informer_->network_path();
  const std::string network_name =
      NetworkStateInformer::GetNetworkName(network_path);

  const bool is_behind_captive_portal =
      NetworkStateInformer::IsBehindCaptivePortal(state, reason);
  const bool is_proxy_error = NetworkStateInformer::IsProxyError(state, reason);
  const bool is_loading_timeout =
      (reason == NetworkError::ERROR_REASON_LOADING_TIMEOUT);

  if (!is_behind_captive_portal)
    HideCaptivePortal();

  if (is_proxy_error) {
    SetErrorState(NetworkError::ERROR_STATE_PROXY, std::string());
  } else if (is_behind_captive_portal) {
    if (GetErrorState() != NetworkError::ERROR_STATE_PORTAL) {
      LoginDisplayHost::default_host()->HandleDisplayCaptivePortal();
    }
    SetErrorState(NetworkError::ERROR_STATE_PORTAL, network_name);
  } else if (is_loading_timeout) {
    SetErrorState(NetworkError::ERROR_STATE_AUTH_EXT_TIMEOUT, network_name);
  } else {
    SetErrorState(NetworkError::ERROR_STATE_OFFLINE, std::string());
  }

  const bool guest_signin_allowed =
      user_manager::UserManager::Get()->IsGuestSessionAllowed();
  AllowGuestSignin(guest_signin_allowed);
  ShowOfflineLoginOption(
      g_offline_login_allowed_ && g_offline_login_per_user_allowed_ &&
      GetErrorState() != NetworkError::ERROR_STATE_AUTH_EXT_TIMEOUT);

  // No need to show the screen again if it is already shown.
  if (is_hidden()) {
    SetUIState(NetworkError::UI_STATE_SIGNIN);
    Show(nullptr /*wizard_context*/);
  }
}

void ErrorScreen::ShowImpl() {
  if (!on_hide_callback_) {
    SetHideCallback(base::BindOnce(&ErrorScreen::DefaultHideCallback,
                                   weak_factory_.GetWeakPtr()));
  }
  if (view_)
    view_->Show();
}

void ErrorScreen::HideImpl() {
  if (view_)
    view_->Hide();
}

void ErrorScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionShowCaptivePortalClicked) {
    ShowCaptivePortal();
  } else if (action_id == kUserActionConfigureCertsButtonClicked) {
    OnConfigureCerts();
  } else if (action_id == kUserActionDiagnoseButtonClicked) {
    OnDiagnoseButtonClicked();
  } else if (action_id == kUserActionLaunchOobeGuestSessionClicked) {
    OnLaunchOobeGuestSession();
  } else if (action_id == kUserActionLocalStateErrorPowerwashButtonClicked) {
    OnLocalStateErrorPowerwashButtonClicked();
  } else if (action_id == kUserActionRebootButtonClicked) {
    OnRebootButtonClicked();
  } else if (action_id == kUserActionCancel) {
    OnCancelButtonClicked();
  } else if (action_id == kUserActionReloadGaia) {
    OnReloadGaiaClicked();
  } else if (action_id == kUserActionNetworkConnected ||
             action_id == kUserActionCancelReset) {
    Hide();
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

void ErrorScreen::OnAuthFailure(const AuthFailure& error) {
  // The only condition leading here is guest mount failure, which should not
  // happen in practice. For now, just log an error so this situation is visible
  // in logs if it ever occurs.
  NOTREACHED() << "Guest login failed.";
  guest_login_performer_.reset();
}

void ErrorScreen::OnAuthSuccess(const UserContext& user_context) {
  LOG(FATAL);
}

void ErrorScreen::OnOffTheRecordAuthSuccess() {
  // Restart Chrome to enter the guest session.
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  base::CommandLine command_line(browser_command_line.GetProgram());
  GetOffTheRecordCommandLine(GURL(), browser_command_line, &command_line);
  RestartChrome(command_line, RestartChromeReason::kGuest);
}

void ErrorScreen::OnPasswordChangeDetected(const UserContext& user_context) {
  LOG(FATAL);
}

void ErrorScreen::AllowlistCheckFailed(const std::string& email) {
  LOG(FATAL);
}

void ErrorScreen::PolicyLoadFailed() {
  LOG(FATAL);
}

void ErrorScreen::DefaultHideCallback() {
  if (parent_screen_ != OobeScreen::SCREEN_UNKNOWN && view_)
    view_->ShowOobeScreen(parent_screen_);

  // TODO(antrim): Due to potential race with GAIA reload and hiding network
  // error UI we can't just reset parent screen to SCREEN_UNKNOWN here.
}

void ErrorScreen::OnConfigureCerts() {
  gfx::NativeWindow native_window =
      LoginDisplayHost::default_host()->GetNativeWindow();
  CertificateManagerDialog* dialog =
      new CertificateManagerDialog(GetAppProfile(), NULL, native_window);
  dialog->Show();
}

void ErrorScreen::OnDiagnoseButtonClicked() {
  chromeos::ConnectivityDiagnosticsDialog::ShowDialog();
}

void ErrorScreen::OnLaunchOobeGuestSession() {
  DeviceSettingsService::Get()->GetOwnershipStatusAsync(
      base::BindOnce(&ErrorScreen::StartGuestSessionAfterOwnershipCheck,
                     weak_factory_.GetWeakPtr()));
}

void ErrorScreen::OnLocalStateErrorPowerwashButtonClicked() {
  SessionManagerClient::Get()->StartDeviceWipe();
}

void ErrorScreen::OnRebootButtonClicked() {
  chromeos::PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_FOR_USER, "login error screen");
}

void ErrorScreen::OnCancelButtonClicked() {
  if (view_)
    view_->OnCancelButtonClicked();
  Hide();
}

void ErrorScreen::OnReloadGaiaClicked() {
  if (view_)
    view_->OnReloadGaiaClicked();
}

void ErrorScreen::ConnectToNetworkRequested(const std::string& service_path) {
  connect_request_callbacks_.Notify();
}

void ErrorScreen::StartGuestSessionAfterOwnershipCheck(
    DeviceSettingsService::OwnershipStatus ownership_status) {
  // Make sure to disallow guest login if it's explicitly disabled.
  CrosSettingsProvider::TrustedStatus trust_status =
      CrosSettings::Get()->PrepareTrustedValues(
          base::BindOnce(&ErrorScreen::StartGuestSessionAfterOwnershipCheck,
                         weak_factory_.GetWeakPtr(), ownership_status));
  switch (trust_status) {
    case CrosSettingsProvider::TEMPORARILY_UNTRUSTED:
      // Wait for a callback.
      return;
    case CrosSettingsProvider::PERMANENTLY_UNTRUSTED:
      // Only allow guest sessions if there is no owner yet.
      if (ownership_status == DeviceSettingsService::OWNERSHIP_NONE)
        break;
      return;
    case CrosSettingsProvider::TRUSTED: {
      // Honor kAccountsPrefAllowGuest.
      bool allow_guest = false;
      CrosSettings::Get()->GetBoolean(kAccountsPrefAllowGuest, &allow_guest);
      if (allow_guest)
        break;
      return;
    }
  }

  if (guest_login_performer_)
    return;

  guest_login_performer_ = std::make_unique<ChromeLoginPerformer>(this);
  guest_login_performer_->LoginOffTheRecord();
}

}  // namespace ash
