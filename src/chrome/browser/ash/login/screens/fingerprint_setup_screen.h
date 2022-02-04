// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_FINGERPRINT_SETUP_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_FINGERPRINT_SETUP_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/fingerprint_setup_screen_handler.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/fingerprint.mojom.h"

namespace ash {

// Controls fingerprint setup. The screen can be shown during OOBE. It allows
// user to enroll fingerprint on the device.
class FingerprintSetupScreen : public BaseScreen,
                               public device::mojom::FingerprintObserver {
 public:
  using TView = FingerprintSetupScreenView;

  enum class Result { DONE, SKIPPED, NOT_APPLICABLE };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other). Entries should be never modified
  // or deleted. Only additions possible.
  enum class UserAction {
    kSetupDone = 0,
    // kSetupSkipped_obsolete = 1,
    // kDoItLater_obsolete = 2,
    kAddAnotherFinger = 3,
    // kShowSensorLocation_obsolete = 4,
    kSkipButtonClickedOnStart = 5,
    kSkipButtonClickedInFlow = 6,
    kMaxValue = kSkipButtonClickedInFlow
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  FingerprintSetupScreen(FingerprintSetupScreenView* view,
                         const ScreenExitCallback& exit_callback);

  FingerprintSetupScreen(const FingerprintSetupScreen&) = delete;
  FingerprintSetupScreen& operator=(const FingerprintSetupScreen&) = delete;

  ~FingerprintSetupScreen() override;

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

  // device::mojom::FingerprintObserver:
  void OnRestarted() override;
  void OnEnrollScanDone(device::mojom::ScanResult scan_result,
                        bool enroll_session_complete,
                        int percent_complete) override;
  void OnAuthScanDone(
      const device::mojom::FingerprintMessagePtr msg,
      const base::flat_map<std::string, std::vector<std::string>>& matches)
      override;
  void OnSessionFailed() override;

  // BaseScreen:

 protected:
  // BaseScreen:
  bool MaybeSkip(WizardContext* context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

 private:
  void StartAddingFinger();
  void OnCancelCurrentEnrollSession(bool success);

  mojo::Remote<device::mojom::Fingerprint> fp_service_;
  mojo::Receiver<device::mojom::FingerprintObserver> receiver_{this};
  int enrolled_finger_count_ = 0;
  bool enroll_session_started_ = false;

  FingerprintSetupScreenView* const view_;
  ScreenExitCallback exit_callback_;

  base::WeakPtrFactory<FingerprintSetupScreen> weak_ptr_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::FingerprintSetupScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_FINGERPRINT_SETUP_SCREEN_H_
