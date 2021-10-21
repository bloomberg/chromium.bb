// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_OFFLINE_LOGIN_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_OFFLINE_LOGIN_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/idle_detector.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/offline_login_screen_handler.h"

namespace ash {

// This class represents offline login screen: that handles login in offline
// mode with provided username and password checked against cryptohome.
class OfflineLoginScreen
    : public BaseScreen,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  using TView = OfflineLoginView;

  enum class Result {
    BACK,
    RELOAD_ONLINE_LOGIN,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  OfflineLoginScreen(OfflineLoginView* view,
                     const ScreenExitCallback& exit_callback);
  ~OfflineLoginScreen() override;

  // Called when the associated View is being destroyed. This screen should call
  // Unbind() on the associated View if this class is destroyed before that.
  void OnViewDestroyed(OfflineLoginView* view);

  void LoadOffline();

  void HandleCompleteAuth(const std::string& username,
                          const std::string& password);

  void HandleEmailSubmitted(const std::string& username);

  // NetworkStateInformer::NetworkStateInformerObserver:
  void OnNetworkReady() override;
  void UpdateState(NetworkError::ErrorReason reason) override;

 private:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

  void StartIdleDetection();
  void OnIdle();

  void HandleTryLoadOnlineLogin();

  OfflineLoginView* view_ = nullptr;

  // True when network is available.
  bool is_network_available_ = false;

  // Network state informer used to keep signin screen up.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  std::unique_ptr<base::ScopedObservation<NetworkStateInformer,
                                          NetworkStateInformerObserver>>
      scoped_observer_;

  ScreenExitCallback exit_callback_;

  // Will monitor if the user is idle for a long period of time and we can try
  // to get back to Online Gaia.
  std::unique_ptr<IdleDetector> idle_detector_;

  base::WeakPtrFactory<OfflineLoginScreen> weak_ptr_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::OfflineLoginScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_OFFLINE_LOGIN_SCREEN_H_
