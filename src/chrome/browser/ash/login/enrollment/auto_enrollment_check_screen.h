// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_AUTO_ENROLLMENT_CHECK_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_AUTO_ENROLLMENT_CHECK_SCREEN_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/enrollment/auto_enrollment_check_screen_view.h"
#include "chrome/browser/ash/login/enrollment/auto_enrollment_controller.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ash/login/error_screens_histogram_helper.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"

namespace ash {

// Handles the control flow after OOBE auto-update completes to wait for the
// enterprise auto-enrollment check that happens as part of OOBE. This includes
// keeping track of current auto-enrollment state and displaying and updating
// the error screen upon failures. Similar to a screen controller, but it
// doesn't actually drive a dedicated screen.
class AutoEnrollmentCheckScreen
    : public AutoEnrollmentCheckScreenView::Delegate,
      public BaseScreen,
      public NetworkPortalDetector::Observer {
 public:
  using TView = AutoEnrollmentCheckScreenView;

  AutoEnrollmentCheckScreen(AutoEnrollmentCheckScreenView* view,
                            ErrorScreen* error_screen,
                            const base::RepeatingClosure& exit_callback);

  AutoEnrollmentCheckScreen(const AutoEnrollmentCheckScreen&) = delete;
  AutoEnrollmentCheckScreen& operator=(const AutoEnrollmentCheckScreen&) =
      delete;

  ~AutoEnrollmentCheckScreen() override;

  // Clears the cached state causing the forced enrollment check to be retried.
  void ClearState();

  void set_auto_enrollment_controller(
      AutoEnrollmentController* auto_enrollment_controller) {
    auto_enrollment_controller_ = auto_enrollment_controller;
  }

  void set_exit_callback_for_testing(const base::RepeatingClosure& callback) {
    exit_callback_ = callback;
  }

  // AutoEnrollmentCheckScreenView::Delegate implementation:
  void OnViewDestroyed(AutoEnrollmentCheckScreenView* view) override;

  // NetworkPortalDetector::Observer implementation:
  void OnPortalDetectionCompleted(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalStatus status) override;

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;

  // Runs `exit_callback_` - used to prevent `exit_callback_` from running after
  // `this` has been destroyed (by wrapping it with a callback bound to a weak
  // ptr).
  void RunExitCallback() { exit_callback_.Run(); }

 private:
  // Handles update notifications regarding the auto-enrollment check.
  void OnAutoEnrollmentCheckProgressed(policy::AutoEnrollmentState state);

  // Handles a state update, updating the UI and saving the state.
  void UpdateState();

  // Configures the UI to reflect `new_captive_portal_status`. Returns true if
  // and only if a UI change has been made.
  bool UpdateCaptivePortalStatus(
      NetworkPortalDetector::CaptivePortalStatus new_captive_portal_status);

  // Configures the UI to reflect `new_auto_enrollment_state`. Returns true if
  // and only if a UI change has been made.
  bool UpdateAutoEnrollmentState(
      policy::AutoEnrollmentState new_auto_enrollment_state);

  // Configures the error screen.
  void ShowErrorScreen(NetworkError::ErrorState error_state);

  // Passed as a callback to the error screen when it's shown. Called when the
  // error screen gets hidden.
  void OnErrorScreenHidden();

  // Asynchronously signals completion. The owner might destroy `this` in
  // response, so no code should be run after the completion of a message loop
  // task, in which this function was called.
  void SignalCompletion();

  // Returns whether enrollment check was completed and decision was made.
  bool IsCompleted() const;

  // The user requested a connection attempt to be performed.
  void OnConnectRequested();

  // Returns true if an error response from the server should cause a network
  // error screen to be displayed and block the wizard from continuing. If false
  // is returned, an error response from the server is treated as "no enrollment
  // necessary".
  bool ShouldBlockOnServerError() const;

  AutoEnrollmentCheckScreenView* view_;
  ErrorScreen* error_screen_;
  base::RepeatingClosure exit_callback_;
  AutoEnrollmentController* auto_enrollment_controller_;

  base::CallbackListSubscription auto_enrollment_progress_subscription_;

  NetworkPortalDetector::CaptivePortalStatus captive_portal_status_;
  policy::AutoEnrollmentState auto_enrollment_state_;

  std::unique_ptr<ErrorScreensHistogramHelper> histogram_helper_;

  base::CallbackListSubscription connect_request_subscription_;

  base::WeakPtrFactory<AutoEnrollmentCheckScreen> weak_ptr_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::AutoEnrollmentCheckScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_AUTO_ENROLLMENT_CHECK_SCREEN_H_
