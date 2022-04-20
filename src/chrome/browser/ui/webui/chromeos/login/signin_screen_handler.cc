// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/login/auth/key.h"
#include "ash/components/login/auth/user_context.h"
#include "ash/components/proximity_auth/screenlock_bridge.h"
#include "ash/constants/ash_features.h"
#include "ash/public/mojom/tray_action.mojom.h"
#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/language_preferences.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/ash/login/error_screens_histogram_helper.h"
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ash/login/hwid_checker.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/lock_screen_utils.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chrome/browser/ash/login/screens/gaia_screen.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/login_display_host_webui.h"
#include "chrome/browser/ash/login/ui/login_display_webui.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/login/users/multi_profile_user_controller.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/system/system_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/ash/session_controller_client_impl.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/l10n_util.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chrome/browser/ui/webui/chromeos/login/offline_login_screen_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/session_manager/core/session_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"

namespace {

// Timeout to delay first notification about offline state for a
// current network.
constexpr base::TimeDelta kOfflineTimeout = base::Seconds(1);

// Timeout to delay first notification about offline state when authenticating
// to a proxy.
constexpr base::TimeDelta kProxyAuthTimeout = base::Seconds(5);

// Timeout used to prevent infinite connecting to a flaky network.
constexpr base::TimeDelta kConnectingTimeout = base::Seconds(60);

// Max number of Gaia Reload to Show Proxy Auth Dialog.
const int kMaxGaiaReloadForProxyAuthDialog = 3;

class CallOnReturn {
 public:
  explicit CallOnReturn(base::OnceClosure callback)
      : callback_(std::move(callback)), call_scheduled_(false) {}

  CallOnReturn(const CallOnReturn&) = delete;
  CallOnReturn& operator=(const CallOnReturn&) = delete;

  ~CallOnReturn() {
    if (call_scheduled_ && !callback_.is_null())
      std::move(callback_).Run();
  }

  void CancelScheduledCall() { call_scheduled_ = false; }
  void ScheduleCall() { call_scheduled_ = true; }

 private:
  base::OnceClosure callback_;
  bool call_scheduled_;
};

}  // namespace

namespace chromeos {

namespace {

bool IsOnline(NetworkStateInformer::State state,
              NetworkError::ErrorReason reason) {
  return state == NetworkStateInformer::ONLINE &&
         reason != NetworkError::ERROR_REASON_PORTAL_DETECTED &&
         reason != NetworkError::ERROR_REASON_LOADING_TIMEOUT;
}

bool IsBehindCaptivePortal(NetworkStateInformer::State state,
                           NetworkError::ErrorReason reason) {
  return state == NetworkStateInformer::CAPTIVE_PORTAL ||
         reason == NetworkError::ERROR_REASON_PORTAL_DETECTED;
}

bool IsProxyError(NetworkStateInformer::State state,
                  NetworkError::ErrorReason reason,
                  net::Error frame_error) {
  return state == NetworkStateInformer::PROXY_AUTH_REQUIRED ||
         reason == NetworkError::ERROR_REASON_PROXY_AUTH_CANCELLED ||
         reason == NetworkError::ERROR_REASON_PROXY_CONNECTION_FAILED ||
         (reason == NetworkError::ERROR_REASON_FRAME_ERROR &&
          (frame_error == net::ERR_PROXY_CONNECTION_FAILED ||
           frame_error == net::ERR_TUNNEL_CONNECTION_FAILED));
}

bool IsSigninScreen(const OobeScreenId screen) {
  return screen == GaiaView::kScreenId;
}

}  // namespace

// SigninScreenHandler implementation ------------------------------------------

SigninScreenHandler::SigninScreenHandler(
    const scoped_refptr<NetworkStateInformer>& network_state_informer,
    ErrorScreen* error_screen,
    GaiaScreenHandler* gaia_screen_handler)
    : network_state_informer_(network_state_informer),
      error_screen_(error_screen),
      proxy_auth_dialog_reload_times_(kMaxGaiaReloadForProxyAuthDialog),
      gaia_screen_handler_(gaia_screen_handler),
      histogram_helper_(
          std::make_unique<ErrorScreensHistogramHelper>("Signin")) {
  DCHECK(network_state_informer_.get());
  DCHECK(error_screen_);
  gaia_screen_handler_->set_signin_screen_handler(this);
  network_state_informer_->AddObserver(this);

  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTH_NEEDED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTH_SUPPLIED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTH_CANCELLED,
                 content::NotificationService::AllSources());
}

SigninScreenHandler::~SigninScreenHandler() {
  weak_factory_.InvalidateWeakPtrs();
  network_state_informer_->RemoveObserver(this);
  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(nullptr);
  proximity_auth::ScreenlockBridge::Get()->SetFocusedUser(EmptyAccountId());
}

void SigninScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

void SigninScreenHandler::RegisterMessages() {
  AddCallback("launchIncognito", &SigninScreenHandler::HandleLaunchIncognito);
  AddCallback("offlineLogin", &SigninScreenHandler::HandleOfflineLogin);

  AddCallback("showLoadingTimeoutError",
              &SigninScreenHandler::HandleShowLoadingTimeoutError);
}

void SigninScreenHandler::Show() {
  CHECK(delegate_);
  histogram_helper_->OnScreenShow();
}

void SigninScreenHandler::SetDelegate(SigninScreenHandlerDelegate* delegate) {
  delegate_ = delegate;
}

void SigninScreenHandler::UpdateState(NetworkError::ErrorReason reason) {
  // ERROR_REASON_FRAME_ERROR is an explicit signal from GAIA frame so it shoud
  // force network error UI update.
  bool force_update = reason == NetworkError::ERROR_REASON_FRAME_ERROR;
  UpdateStateInternal(reason, force_update);
}

void SigninScreenHandler::SetOfflineTimeoutForTesting(
    base::TimeDelta offline_timeout) {
  is_offline_timeout_for_test_set_ = true;
  offline_timeout_for_test_ = offline_timeout;
}

// SigninScreenHandler, private: -----------------------------------------------

// TODO(antrim@): split this method into small parts.
// TODO(antrim@): move this logic to GaiaScreenHandler.
void SigninScreenHandler::UpdateStateInternal(NetworkError::ErrorReason reason,
                                              bool force_update) {
  // Do nothing once user has signed in or sign in is in progress.
  // TODO(antrim): We will end up here when processing network state
  // notification but no ShowSigninScreen() was called so delegate_ will be
  // nullptr. Network state processing logic does not belong here.
  if (delegate_ &&
      (delegate_->IsUserSigninCompleted() || delegate_->IsSigninInProgress())) {
    return;
  }

  NetworkStateInformer::State state = network_state_informer_->state();
  const std::string network_path = network_state_informer_->network_path();
  const std::string network_name =
      NetworkStateInformer::GetNetworkName(network_path);

  // Skip "update" notification about OFFLINE state from
  // NetworkStateInformer if previous notification already was
  // delayed.
  if ((state == NetworkStateInformer::OFFLINE ||
       network_state_ignored_until_proxy_auth_) &&
      !force_update && !update_state_callback_.IsCancelled()) {
    return;
  }

  update_state_callback_.Cancel();

  if ((state == NetworkStateInformer::OFFLINE && !force_update) ||
      network_state_ignored_until_proxy_auth_) {
    update_state_callback_.Reset(
        base::BindOnce(&SigninScreenHandler::UpdateStateInternal,
                       weak_factory_.GetWeakPtr(), reason, true));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, update_state_callback_.callback(),
        is_offline_timeout_for_test_set_ ? offline_timeout_for_test_
                                         : kOfflineTimeout);
    return;
  }

  // Don't show or hide error screen if we're in connecting state.
  if (state == NetworkStateInformer::CONNECTING && !force_update) {
    if (connecting_callback_.IsCancelled()) {
      // First notification about CONNECTING state.
      connecting_callback_.Reset(
          base::BindOnce(&SigninScreenHandler::UpdateStateInternal,
                         weak_factory_.GetWeakPtr(), reason, true));
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, connecting_callback_.callback(), kConnectingTimeout);
    }
    return;
  }
  connecting_callback_.Cancel();

  const bool is_online = IsOnline(state, reason);
  const bool is_behind_captive_portal = IsBehindCaptivePortal(state, reason);
  const bool is_gaia_loading_timeout =
      (reason == NetworkError::ERROR_REASON_LOADING_TIMEOUT);
  const bool is_gaia_error =
      FrameError() != net::OK && FrameError() != net::ERR_NETWORK_CHANGED;
  const bool is_gaia_signin = IsGaiaVisible() || IsGaiaHiddenByError();
  const bool error_screen_should_overlay = IsGaiaVisible();
  const bool from_not_online_to_online_transition =
      is_online && last_network_state_ != NetworkStateInformer::ONLINE;
  last_network_state_ = state;
  proxy_auth_dialog_need_reload_ =
      (reason == NetworkError::ERROR_REASON_NETWORK_STATE_CHANGED) &&
      (state == NetworkStateInformer::PROXY_AUTH_REQUIRED) &&
      (proxy_auth_dialog_reload_times_ > 0);

  CallOnReturn reload_gaia(base::BindOnce(&SigninScreenHandler::ReloadGaia,
                                          weak_factory_.GetWeakPtr(), true));

  if (is_online || !is_behind_captive_portal)
    error_screen_->HideCaptivePortal();

  // Hide offline message (if needed) and return if current screen is
  // not a Gaia frame.
  if (!is_gaia_signin) {
    if (!IsSigninScreenHiddenByError())
      HideOfflineMessage(state, reason);
    return;
  }

  // Reload frame if network state is changed from {!ONLINE} -> ONLINE state.
  if (reason == NetworkError::ERROR_REASON_NETWORK_STATE_CHANGED &&
      from_not_online_to_online_transition) {
    // Schedules a immediate retry.
    LOG(WARNING) << "Retry frame load since network has been changed.";
    gaia_reload_reason_ = reason;
    reload_gaia.ScheduleCall();
  }

  if (reason == NetworkError::ERROR_REASON_PROXY_CONFIG_CHANGED &&
      error_screen_should_overlay) {
    // Schedules a immediate retry.
    LOG(WARNING) << "Retry frameload since proxy settings has been changed.";
    gaia_reload_reason_ = reason;
    reload_gaia.ScheduleCall();
  }

  if (reason == NetworkError::ERROR_REASON_FRAME_ERROR &&
      reason != gaia_reload_reason_ &&
      !IsProxyError(state, reason, FrameError())) {
    LOG(WARNING) << "Retry frame load due to reason: "
                 << NetworkError::ErrorReasonString(reason);
    gaia_reload_reason_ = reason;
    reload_gaia.ScheduleCall();
  }

  if (is_gaia_loading_timeout) {
    LOG(WARNING) << "Retry frame load due to loading timeout.";
    reload_gaia.ScheduleCall();
  }

  if (proxy_auth_dialog_need_reload_) {
    --proxy_auth_dialog_reload_times_;
    LOG(WARNING) << "Retry frame load to show proxy auth dialog";
    reload_gaia.ScheduleCall();
  }

  if (!is_online || is_gaia_loading_timeout || is_gaia_error) {
    if (GetCurrentScreen() != ErrorScreenView::kScreenId) {
      error_screen_->SetParentScreen(GaiaView::kScreenId);
      error_screen_->ShowNetworkErrorMessage(state, reason);
      histogram_helper_->OnErrorShow(error_screen_->GetErrorState());
    }
  } else {
    HideOfflineMessage(state, reason);

    // Cancel scheduled GAIA reload (if any) to prevent double reloads.
    reload_gaia.CancelScheduledCall();
  }
}

void SigninScreenHandler::HideOfflineMessage(NetworkStateInformer::State state,
                                             NetworkError::ErrorReason reason) {
  if (!IsSigninScreenHiddenByError())
    return;

  gaia_reload_reason_ = NetworkError::ERROR_REASON_NONE;

  error_screen_->Hide();
  histogram_helper_->OnErrorHide();

  // Forces a reload for Gaia screen on hiding error message.
  if (IsGaiaVisible() || IsGaiaHiddenByError())
    ReloadGaia(reason == NetworkError::ERROR_REASON_NETWORK_STATE_CHANGED);
}

void SigninScreenHandler::ReloadGaia(bool force_reload) {
  gaia_screen_handler_->ReloadGaia(force_reload);
}

void SigninScreenHandler::RegisterPrefs(PrefRegistrySimple* registry) {
  // The pref is deprecated. Remove around 09/2022 (https://crbug.com/1297407)
  registry->RegisterDictionaryPref(prefs::kUsersLastInputMethod);
}

void SigninScreenHandler::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_AUTH_NEEDED: {
      network_state_ignored_until_proxy_auth_ = true;
      break;
    }
    case chrome::NOTIFICATION_AUTH_SUPPLIED: {
      if (IsGaiaHiddenByError()) {
        // Start listening to network state notifications immediately, hoping
        // that the network will switch to ONLINE soon.
        update_state_callback_.Cancel();
        ReenableNetworkStateUpdatesAfterProxyAuth();
      } else {
        // Gaia is not hidden behind an error yet. Discard last cached network
        // state notification and wait for `kProxyAuthTimeout` before
        // considering network update notifications again (hoping the network
        // will become ONLINE by then).
        update_state_callback_.Cancel();
        base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(
                &SigninScreenHandler::ReenableNetworkStateUpdatesAfterProxyAuth,
                weak_factory_.GetWeakPtr()),
            kProxyAuthTimeout);
      }
      break;
    }
    case chrome::NOTIFICATION_AUTH_CANCELLED: {
      update_state_callback_.Cancel();
      ReenableNetworkStateUpdatesAfterProxyAuth();
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
}

void SigninScreenHandler::ReenableNetworkStateUpdatesAfterProxyAuth() {
  network_state_ignored_until_proxy_auth_ = false;
}

void SigninScreenHandler::HandleLaunchIncognito() {
  UserContext context(user_manager::USER_TYPE_GUEST, EmptyAccountId());
  if (delegate_)
    delegate_->Login(context, SigninSpecifics());
}

void SigninScreenHandler::HandleOfflineLogin() {
  if (!delegate_) {
    NOTREACHED();
    return;
  }

  auto* offline_login_screen =
      WizardController::default_controller()->GetScreen<OfflineLoginScreen>();
  offline_login_screen->LoadOffline();
  HideOfflineMessage(NetworkStateInformer::OFFLINE,
                     NetworkError::ERROR_REASON_NONE);
  LoginDisplayHost::default_host()->StartWizard(OfflineLoginView::kScreenId);
}

void SigninScreenHandler::HandleShowLoadingTimeoutError() {
  UpdateState(NetworkError::ERROR_REASON_LOADING_TIMEOUT);
}

bool SigninScreenHandler::IsGaiaVisible() {
  return IsSigninScreen(GetCurrentScreen());
}

bool SigninScreenHandler::IsGaiaHiddenByError() {
  return IsSigninScreenHiddenByError();
}

bool SigninScreenHandler::IsSigninScreenHiddenByError() {
  return (GetCurrentScreen() == ErrorScreenView::kScreenId) &&
         (IsSigninScreen(error_screen_->GetParentScreen()));
}

net::Error SigninScreenHandler::FrameError() const {
  return gaia_screen_handler_->frame_error();
}

}  // namespace chromeos
