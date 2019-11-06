// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MACHINE_LEVEL_USER_CLOUD_POLICY_REGISTER_WATCHER_H_
#define CHROME_BROWSER_POLICY_MACHINE_LEVEL_USER_CLOUD_POLICY_REGISTER_WATCHER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/policy/machine_level_user_cloud_policy_controller.h"
#include "chrome/browser/ui/enterprise_startup_dialog.h"

class MachineLevelUserCloudPolicyRegisterWatcherTest;

namespace policy {

// Watches the status of machine level user cloud policy enrollment.
// Shows the blocking dialog for ongoing enrollment and failed enrollment.
class MachineLevelUserCloudPolicyRegisterWatcher
    : public MachineLevelUserCloudPolicyController::Observer {
 public:
  using DialogCreationCallback =
      base::OnceCallback<std::unique_ptr<EnterpriseStartupDialog>(
          EnterpriseStartupDialog::DialogResultCallback)>;

  explicit MachineLevelUserCloudPolicyRegisterWatcher(
      MachineLevelUserCloudPolicyController* controller);
  ~MachineLevelUserCloudPolicyRegisterWatcher() override;

  // Blocks until the machine level user cloud policy enrollment process
  // finishes. Returns the result of enrollment.
  MachineLevelUserCloudPolicyController::RegisterResult
  WaitUntilCloudPolicyEnrollmentFinished();

  // Returns whether the dialog is being displayed.
  bool IsDialogShowing();

  void SetDialogCreationCallbackForTesting(DialogCreationCallback callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(MachineLevelUserCloudPolicyRegisterWatcherTest,
                           EnrollmentSucceed);
  FRIEND_TEST_ALL_PREFIXES(MachineLevelUserCloudPolicyRegisterWatcherTest,
                           EnrollmentSucceedWithNoErrorMessageSetup);
  FRIEND_TEST_ALL_PREFIXES(MachineLevelUserCloudPolicyRegisterWatcherTest,
                           EnrollmentFailedAndQuit);
  FRIEND_TEST_ALL_PREFIXES(MachineLevelUserCloudPolicyRegisterWatcherTest,
                           EnrollmentFailedAndRestart);
  FRIEND_TEST_ALL_PREFIXES(MachineLevelUserCloudPolicyRegisterWatcherTest,
                           EnrollmentCanceledBeforeFinish);
  FRIEND_TEST_ALL_PREFIXES(
      MachineLevelUserCloudPolicyRegisterWatcherTest,
      EnrollmentCanceledBeforeFinishWithNoErrorMessageSetup);
  FRIEND_TEST_ALL_PREFIXES(MachineLevelUserCloudPolicyRegisterWatcherTest,
                           EnrollmentFailedBeforeDialogDisplay);
  FRIEND_TEST_ALL_PREFIXES(MachineLevelUserCloudPolicyRegisterWatcherTest,
                           EnrollmentFailedWithoutErrorMessage);
  FRIEND_TEST_ALL_PREFIXES(
      MachineLevelUserCloudPolicyRegisterWatcherTest,
      EnrollmentFailedBeforeDialogDisplayWithoutErrorMessage);

  // Enum used with kStartupDialogHistogramName.
  enum class EnrollmentStartupDialog {
    // The enrollment startup dialog was shown.
    kShown = 0,

    // The dialog was closed automatically because enrollment completed
    // successfully.  Chrome startup can continue normally.
    kClosedSuccess = 1,

    // The dialog was closed because enrollment failed.  The user chose to
    // relaunch chrome and try again.
    kClosedRelaunch = 2,

    // The dialog was closed because enrollment failed.  The user chose to
    // close chrome.
    kClosedFail = 3,

    // The dialog was closed because no response from the server was received
    // before the user gave up and closed the dialog.
    kClosedAbort = 4,

    // The dialog was closed automatically because enrollment failed but admin
    // choose to ignore the error and show the browser window.
    kClosedFailAndIgnore = 5,

    kMaxValue = kClosedFailAndIgnore,
  };

  static const char kStartupDialogHistogramName[];

  static void RecordEnrollmentStartDialog(
      EnrollmentStartupDialog dialog_startup);

  // MachineLevelUserCloudPolicyController::Observer
  void OnPolicyRegisterFinished(bool succeeded) override;

  // EnterpriseStartupDialog callback.
  void OnDialogClosed(bool is_accepted, bool can_show_browser_window);

  void DisplayErrorMessage();

  MachineLevelUserCloudPolicyController* controller_;

  base::RunLoop run_loop_;
  std::unique_ptr<EnterpriseStartupDialog> dialog_;

  bool is_restart_needed_ = false;
  base::Optional<bool> register_result_;

  DialogCreationCallback dialog_creation_callback_;

  base::Time visible_start_time_;

  DISALLOW_COPY_AND_ASSIGN(MachineLevelUserCloudPolicyRegisterWatcher);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_MACHINE_LEVEL_USER_CLOUD_POLICY_REGISTER_WATCHER_H_
