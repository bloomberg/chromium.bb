// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_REAUTH_DIALOGS_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_REAUTH_DIALOGS_H_

#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/base_lock_dialog.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

class LockScreenCaptivePortalDialog;
class LockScreenNetworkDialog;

class LockScreenStartReauthDialog
    : public BaseLockDialog,
      public NetworkStateInformer::NetworkStateInformerObserver,
      public ChromeWebModalDialogManagerDelegate,
      public web_modal::WebContentsModalDialogHost {
 public:
  LockScreenStartReauthDialog();
  LockScreenStartReauthDialog(LockScreenStartReauthDialog const&) = delete;
  ~LockScreenStartReauthDialog() override;

  void Show();
  void Dismiss();
  bool IsRunning();
  int GetDialogWidth();

  void DismissLockScreenNetworkDialog();
  void DismissLockScreenCaptivePortalDialog();
  void ShowLockScreenNetworkDialog();
  void ShowLockScreenCaptivePortalDialog();
  static gfx::Size CalculateLockScreenReauthDialogSize(
      bool is_new_layout_enabled);

  // Used for waiting for the corresponding dialogs in tests.
  // Similar methods exist for the main dialog in InSessionPasswordSyncManager.
  bool IsNetworkDialogLoadedForTesting(base::OnceClosure callback);
  bool IsCaptivePortalDialogLoadedForTesting(base::OnceClosure callback);
  void OnNetworkDialogReadyForTesting();

  LockScreenNetworkDialog* get_network_dialog_for_testing() {
    return lock_screen_network_dialog_.get();
  }

  LockScreenCaptivePortalDialog* get_captive_portal_dialog_for_testing() {
    return captive_portal_dialog_.get();
  }

  bool is_network_dialog_visible_for_testing() {
    return is_network_dialog_visible_;
  }

 private:
  class ModalDialogManagerCleanup;

  void OnProfileCreated(Profile* profile, Profile::CreateStatus status);
  void DeleteLockScreenNetworkDialog();

  // BaseLockDialog:
  void OnDialogShown(content::WebUI* webui) override;
  void OnDialogClosed(const std::string& json_retval) override;

  // NetworkStateInformer::NetworkStateInformerObserver:
  void UpdateState(NetworkError::ErrorReason reason) override;

  // ChromeWebModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // web_modal::WebContentsModalDialogHost:
  gfx::Size GetMaximumDialogSize() override;
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

  void OnCaptivePortalDialogReadyForTesting();

  scoped_refptr<chromeos::NetworkStateInformer> network_state_informer_;
  bool is_network_dialog_visible_ = false;

  base::ScopedObservation<NetworkStateInformer, NetworkStateInformerObserver>
      scoped_observation_{this};

  std::unique_ptr<LockScreenNetworkDialog> lock_screen_network_dialog_;
  Profile* profile_ = nullptr;

  std::unique_ptr<LockScreenCaptivePortalDialog> captive_portal_dialog_;

  // Callbacks that are used to notify tests that the corresponding dialog is
  // loaded.
  base::OnceClosure on_network_dialog_loaded_callback_for_testing_;
  base::OnceClosure on_captive_portal_dialog_loaded_callback_for_testing_;
  bool is_network_dialog_loaded_for_testing_ = false;
  bool is_captive_portal_dialog_loaded_for_testing_ = false;

  base::ObserverList<web_modal::ModalDialogHostObserver>::Unchecked
      modal_dialog_host_observer_list_;
  std::unique_ptr<ModalDialogManagerCleanup> modal_dialog_manager_cleanup_;

  base::WeakPtrFactory<LockScreenStartReauthDialog> weak_factory_{this};
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::LockScreenStartReauthDialog;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_REAUTH_DIALOGS_H_
