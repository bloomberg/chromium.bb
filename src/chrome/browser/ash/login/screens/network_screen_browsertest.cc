// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/network_screen.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/mock_network_state_helper.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/views/controls/button/button.h"

namespace ash {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::ElementsAre;
using ::testing::Return;
using ::testing::ReturnRef;
using ::views::Button;

class NetworkScreenTest : public OobeBaseTest {
 public:
  NetworkScreenTest() {
    feature_list_.InitWithFeatures(
        {
            features::kEnableOobeNetworkScreenSkip,
        },
        {});
  }

  NetworkScreenTest(const NetworkScreenTest&) = delete;
  NetworkScreenTest& operator=(const NetworkScreenTest&) = delete;

  ~NetworkScreenTest() override = default;

  void SetUpOnMainThread() override {
    network_screen_ = static_cast<NetworkScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            NetworkScreenView::kScreenId));
    network_screen_->set_exit_callback_for_testing(base::BindRepeating(
        &NetworkScreenTest::HandleScreenExit, base::Unretained(this)));
    ASSERT_TRUE(network_screen_->view_ != nullptr);

    mock_network_state_helper_ = new login::MockNetworkStateHelper;
    SetDefaultNetworkStateHelperExpectations();
    network_screen_->SetNetworkStateHelperForTest(mock_network_state_helper_);
    OobeBaseTest::SetUpOnMainThread();
  }

  void ShowNetworkScreen() {
    WizardController::default_controller()->AdvanceToScreen(
        NetworkScreenView::kScreenId);
  }

  void EmulateContinueButtonExit(NetworkScreen* network_screen) {
    EXPECT_CALL(*network_state_helper(), IsConnected()).WillOnce(Return(true));
    network_screen->OnContinueButtonClicked();
    base::RunLoop().RunUntilIdle();

    ASSERT_TRUE(last_screen_result_.has_value());
    EXPECT_EQ(NetworkScreen::Result::CONNECTED_REGULAR,
              last_screen_result_.value());
  }

  void SetDefaultNetworkStateHelperExpectations() {
    EXPECT_CALL(*network_state_helper(), GetCurrentNetworkName())
        .Times(AnyNumber())
        .WillRepeatedly((Return(std::u16string())));
    EXPECT_CALL(*network_state_helper(), IsConnected())
        .Times(AnyNumber())
        .WillRepeatedly((Return(false)));
    EXPECT_CALL(*network_state_helper(), IsConnecting())
        .Times(AnyNumber())
        .WillRepeatedly((Return(false)));
    EXPECT_CALL(*network_state_helper(), IsConnectedToEthernet())
        .Times(AnyNumber())
        .WillRepeatedly((Return(false)));
  }

  void WaitForScreenShown() {
    OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
  }

  void WaitForScreenExit() {
    if (screen_exited_)
      return;
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void CheckResult(NetworkScreen::Result result) {
    ASSERT_TRUE(last_screen_result_.has_value());
    EXPECT_EQ(last_screen_result_.value(), result);
  }

  login::MockNetworkStateHelper* network_state_helper() {
    return mock_network_state_helper_;
  }
  NetworkScreen* network_screen() { return network_screen_; }

  base::HistogramTester histogram_tester_;

 private:
  void HandleScreenExit(NetworkScreen::Result result) {
    screen_exited_ = true;
    last_screen_result_ = result;
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  login::MockNetworkStateHelper* mock_network_state_helper_;
  NetworkScreen* network_screen_;
  bool screen_exited_ = false;
  base::test::ScopedFeatureList feature_list_;
  absl::optional<NetworkScreen::Result> last_screen_result_;
  base::RepeatingClosure screen_exit_callback_;
};

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, CanConnect) {
  ShowNetworkScreen();
  EXPECT_CALL(*network_state_helper(), IsConnecting()).WillOnce((Return(true)));
  // EXPECT_FALSE(view_->IsContinueEnabled());
  network_screen()->UpdateStatus();

  EXPECT_CALL(*network_state_helper(), IsConnected())
      .Times(2)
      .WillRepeatedly(Return(true));
  // TODO(nkostylev): Add integration with WebUI view http://crosbug.com/22570
  // EXPECT_FALSE(view_->IsContinueEnabled());
  // EXPECT_FALSE(view_->IsConnecting());
  network_screen()->UpdateStatus();

  // EXPECT_TRUE(view_->IsContinueEnabled());
  EmulateContinueButtonExit(network_screen());
}

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, Timeout) {
  ShowNetworkScreen();
  EXPECT_CALL(*network_state_helper(), IsConnecting()).WillOnce((Return(true)));
  // EXPECT_FALSE(view_->IsContinueEnabled());
  network_screen()->UpdateStatus();

  EXPECT_CALL(*network_state_helper(), IsConnected())
      .Times(2)
      .WillRepeatedly(Return(false));
  // TODO(nkostylev): Add integration with WebUI view http://crosbug.com/22570
  // EXPECT_FALSE(view_->IsContinueEnabled());
  // EXPECT_FALSE(view_->IsConnecting());
  network_screen()->OnConnectionTimeout();

  // Close infobubble with error message - it makes the test stable.
  // EXPECT_FALSE(view_->IsContinueEnabled());
  // EXPECT_FALSE(view_->IsConnecting());
  // view_->ClearErrors();
}

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, SkippedEthernetConnected) {
  EXPECT_CALL(*network_state_helper(), IsConnectedToEthernet())
      .Times(AnyNumber())
      .WillRepeatedly((Return(true)));
  ShowNetworkScreen();
  WaitForScreenExit();
  CheckResult(NetworkScreen::Result::NOT_APPLICABLE);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Network-selection.Connected", 0);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Network-selection.OfflineDemoSetup",
      0);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Network-selection.Back", 0);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTime.Network-selection", 0);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("OOBE.StepShownStatus.Network-selection"),
      ElementsAre(base::Bucket(
          static_cast<int>(WizardController::ScreenShownStatus::kSkipped), 1)));
  // Showing screen again to test skip doesn't work now.
  ShowNetworkScreen();
  WaitForScreenShown();
}

}  // namespace ash
