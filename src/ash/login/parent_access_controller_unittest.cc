// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/parent_access_controller.h"

#include "ash/login/mock_login_screen_client.h"
#include "ash/login/ui/login_button.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/pin_request_view.h"
#include "ash/login/ui/pin_request_widget.h"
#include "ash/login/ui/views_utils.h"
#include "base/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/label_button.h"

using ::testing::_;

namespace ash {

namespace {

class ParentAccessControllerTest : public LoginTestBase {
 protected:
  ParentAccessControllerTest()
      : account_id_(AccountId::FromUserEmail("child@gmail.com")) {}
  ~ParentAccessControllerTest() override = default;

  // LoginScreenTest:
  void SetUp() override {
    LoginTestBase::SetUp();
    login_client_ = std::make_unique<MockLoginScreenClient>();
    controller_ = std::make_unique<ParentAccessController>();
  }

  void TearDown() override {
    LoginTestBase::TearDown();

    // If the test did not explicitly dismissed the widget, destroy it now.
    PinRequestWidget* pin_request_widget = PinRequestWidget::Get();
    if (pin_request_widget)
      pin_request_widget->Close(false /* validation success */);
  }

  // Simulates mouse press event on a |button|.
  void SimulateButtonPress(views::Button* button) {
    ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), 0, 0);
    view_->ButtonPressed(button, event);
  }

  // Called when ParentAccessView finished processing.
  void OnFinished(bool access_granted) {
    access_granted ? ++successful_validation_ : ++back_action_;
  }

  void StartParentAccess(ParentAccessRequestReason reason =
                             ParentAccessRequestReason::kUnlockTimeLimits) {
    validation_time_ = base::Time::Now();
    controller_->ShowWidget(
        account_id_,
        base::BindOnce(&ParentAccessControllerTest::OnFinished,
                       base::Unretained(this)),
        reason, false, validation_time_);
    view_ =
        PinRequestWidget::TestApi(PinRequestWidget::Get()).pin_request_view();
  }

  // Verifies expectation that UMA |action| was logged.
  void ExpectUMAActionReported(ParentAccessController::UMAAction action,
                               int bucket_count,
                               int total_count) {
    histogram_tester_.ExpectBucketCount(
        ParentAccessController::kUMAParentAccessCodeAction, action,
        bucket_count);
    histogram_tester_.ExpectTotalCount(
        ParentAccessController::kUMAParentAccessCodeAction, total_count);
  }

  // Simulates entering a code. |success| determines whether the code will be
  // accepted.
  void SimulateValidation(bool success) {
    login_client_->set_validate_parent_access_code_result(success);
    EXPECT_CALL(*login_client_, ValidateParentAccessCode_(account_id_, "012345",
                                                          validation_time_))
        .Times(1);

    ui::test::EventGenerator* generator = GetEventGenerator();
    for (int i = 0; i < 6; ++i) {
      generator->PressKey(ui::KeyboardCode(ui::KeyboardCode::VKEY_0 + i),
                          ui::EF_NONE);
      base::RunLoop().RunUntilIdle();
    }
  }

  std::unique_ptr<ParentAccessController> controller_;

  const AccountId account_id_;
  std::unique_ptr<MockLoginScreenClient> login_client_;

  // Number of times the view was dismissed with back button.
  int back_action_ = 0;

  // Number of times the view was dismissed after successful validation.
  int successful_validation_ = 0;

  // Time that will be used on the code validation.
  base::Time validation_time_;

  base::HistogramTester histogram_tester_;

  PinRequestView* view_ = nullptr;  // Owned by test widget view hierarchy.

 private:
  DISALLOW_COPY_AND_ASSIGN(ParentAccessControllerTest);
};

// Tests parent access dialog showing/hiding and focus behavior for parent
// access.
TEST_F(ParentAccessControllerTest, ParentAccessDialogFocus) {
  EXPECT_FALSE(PinRequestWidget::Get());

  StartParentAccess();
  PinRequestView::TestApi view_test_api = PinRequestView::TestApi(view_);

  ASSERT_TRUE(PinRequestWidget::Get());
  EXPECT_TRUE(login_views_utils::HasFocusInAnyChildView(
      view_test_api.access_code_view()));

  PinRequestWidget::Get()->Close(false /* validation success */);

  EXPECT_FALSE(PinRequestWidget::Get());
}

// Tests correct UMA reporting for parent access.
TEST_F(ParentAccessControllerTest, ParentAccessUMARecording) {
  StartParentAccess(ParentAccessRequestReason::kUnlockTimeLimits);
  histogram_tester_.ExpectBucketCount(
      ParentAccessController::kUMAParentAccessCodeUsage,
      ParentAccessController::UMAUsage::kTimeLimits, 1);
  SimulateButtonPress(PinRequestView::TestApi(view_).back_button());
  ExpectUMAActionReported(ParentAccessController::UMAAction::kCanceledByUser, 1,
                          1);

  StartParentAccess(ParentAccessRequestReason::kChangeTimezone);
  histogram_tester_.ExpectBucketCount(
      ParentAccessController::kUMAParentAccessCodeUsage,
      ParentAccessController::UMAUsage::kTimezoneChange, 1);
  SimulateButtonPress(PinRequestView::TestApi(view_).back_button());
  ExpectUMAActionReported(ParentAccessController::UMAAction::kCanceledByUser, 2,
                          2);

  // The below usage depends on the session state.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  StartParentAccess(ParentAccessRequestReason::kChangeTime);
  histogram_tester_.ExpectBucketCount(
      ParentAccessController::kUMAParentAccessCodeUsage,
      ParentAccessController::UMAUsage::kTimeChangeInSession, 1);
  SimulateButtonPress(PinRequestView::TestApi(view_).back_button());
  ExpectUMAActionReported(ParentAccessController::UMAAction::kCanceledByUser, 3,
                          3);

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  StartParentAccess(ParentAccessRequestReason::kChangeTime);
  histogram_tester_.ExpectBucketCount(
      ParentAccessController::kUMAParentAccessCodeUsage,
      ParentAccessController::UMAUsage::kTimeChangeLoginScreen, 1);
  SimulateButtonPress(PinRequestView::TestApi(view_).back_button());
  ExpectUMAActionReported(ParentAccessController::UMAAction::kCanceledByUser, 4,
                          4);

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  StartParentAccess(ParentAccessRequestReason::kChangeTime);
  histogram_tester_.ExpectBucketCount(
      ParentAccessController::kUMAParentAccessCodeUsage,
      ParentAccessController::UMAUsage::kTimeChangeInSession, 2);
  SimulateButtonPress(PinRequestView::TestApi(view_).back_button());
  ExpectUMAActionReported(ParentAccessController::UMAAction::kCanceledByUser, 5,
                          5);

  histogram_tester_.ExpectTotalCount(
      ParentAccessController::kUMAParentAccessCodeUsage, 5);
  EXPECT_EQ(5, back_action_);
}

// Tests successful parent access validation flow.
TEST_F(ParentAccessControllerTest, ParentAccessSuccessfulValidation) {
  StartParentAccess();
  SimulateValidation(true);

  EXPECT_EQ(1, successful_validation_);
  ExpectUMAActionReported(ParentAccessController::UMAAction::kValidationSuccess,
                          1, 1);
}

// Tests unsuccessful parent access flow, including help button and cancelling
// the request.
TEST_F(ParentAccessControllerTest, ParentAccessUnsuccessfulValidation) {
  StartParentAccess();
  SimulateValidation(false);

  ExpectUMAActionReported(ParentAccessController::UMAAction::kValidationError,
                          1, 1);

  EXPECT_CALL(*login_client_, ShowParentAccessHelpApp(_)).Times(1);
  SimulateButtonPress(PinRequestView::TestApi(view_).help_button());
  ExpectUMAActionReported(ParentAccessController::UMAAction::kGetHelp, 1, 2);

  SimulateButtonPress(PinRequestView::TestApi(view_).back_button());
  ExpectUMAActionReported(ParentAccessController::UMAAction::kCanceledByUser, 1,
                          3);
}

}  // namespace
}  // namespace ash
