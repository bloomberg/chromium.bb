// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_KIOSK_ENABLE_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_KIOSK_ENABLE_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/kiosk_enable_screen_handler.h"

namespace ash {

// Representation independent class that controls screen for enabling
// consumer kiosk mode.
class KioskEnableScreen : public BaseScreen {
 public:
  KioskEnableScreen(KioskEnableScreenView* view,
                    const base::RepeatingClosure& exit_callback);

  KioskEnableScreen(const KioskEnableScreen&) = delete;
  KioskEnableScreen& operator=(const KioskEnableScreen&) = delete;

  ~KioskEnableScreen() override;

  // This method is called, when view is being destroyed. Note, if Screen
  // is destroyed earlier then it has to call SetScreen(nullptr).
  void OnViewDestroyed(KioskEnableScreenView* view);

 private:
  // BaseScreen implementation:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

  void HandleClose();
  void HandleEnable();

  // Callback for KioskAppManager::EnableConsumerModeKiosk().
  void OnEnableConsumerKioskAutoLaunch(bool success);

  // Callback for KioskAppManager::GetConsumerKioskModeStatus().
  void OnGetConsumerKioskAutoLaunchStatus(
      KioskAppManager::ConsumerKioskAutoLaunchStatus status);

  KioskEnableScreenView* view_;
  base::RepeatingClosure exit_callback_;

  // True if machine's consumer kiosk mode is in a configurable state.
  bool is_configurable_ = false;

  base::WeakPtrFactory<KioskEnableScreen> weak_ptr_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::KioskEnableScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_KIOSK_ENABLE_SCREEN_H_
