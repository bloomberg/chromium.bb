// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/enrollment_screen.h"

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/ash/login/enrollment/enterprise_enrollment_helper_mock.h"
#include "chrome/browser/ash/login/enrollment/mock_enrollment_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/policy/enrollment_status.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;

class EnrollmentScreenUnitTest : public testing::Test {
 public:
  EnrollmentScreenUnitTest() = default;

  EnrollmentScreenUnitTest(const EnrollmentScreenUnitTest&) = delete;
  EnrollmentScreenUnitTest& operator=(const EnrollmentScreenUnitTest&) = delete;

  // Creates the EnrollmentScreen and sets required parameters.
  virtual void SetUpEnrollmentScreen() {
    wizard_context_ = std::make_unique<WizardContext>();

    enrollment_screen_ = std::make_unique<EnrollmentScreen>(
        &mock_view_,
        base::BindRepeating(&EnrollmentScreenUnitTest::HandleScreenExit,
                            base::Unretained(this)));

    enrollment_screen_->SetEnrollmentConfig(enrollment_config_);
  }

  // Fast forwards time by the specified amount.
  void FastForwardTime(base::TimeDelta time) {
    runner_.task_runner()->FastForwardBy(time);
  }

  MockEnrollmentScreenView* GetMockScreenView() { return &mock_view_; }

  // testing::Test:
  void SetUp() override {
    RegisterLocalState(pref_service_.registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(&pref_service_);
    chromeos::system::StatisticsProvider::SetTestProvider(
        &statistics_provider_);
    DBusThreadManager::Initialize();
    policy::EnrollmentRequisitionManager::Initialize();
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetShuttingDown(true);
    DBusThreadManager::Shutdown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

 protected:
  std::unique_ptr<EnrollmentScreen> enrollment_screen_;
  std::unique_ptr<WizardContext> wizard_context_;

  // The last result reported by `enrollment_screen_`.
  absl::optional<EnrollmentScreen::Result> last_screen_result_;

  policy::EnrollmentConfig enrollment_config_;

 private:
  void HandleScreenExit(EnrollmentScreen::Result screen_result) {
    EXPECT_FALSE(last_screen_result_.has_value());
    last_screen_result_ = screen_result;
  }

  base::test::TaskEnvironment task_environment_;
  // Replace main thread's task runner with a mock for duration of test.
  base::ScopedMockTimeMessageLoopTaskRunner runner_;

  ScopedStubInstallAttributes test_install_attributes_;

  TestingPrefServiceSimple pref_service_;

  chromeos::system::FakeStatisticsProvider statistics_provider_;

  // Objects required by the EnrollmentScreen that can be re-used.
  MockEnrollmentScreenView mock_view_;
};

class ZeroTouchEnrollmentScreenUnitTest : public EnrollmentScreenUnitTest {
 public:
  ZeroTouchEnrollmentScreenUnitTest() = default;

  ZeroTouchEnrollmentScreenUnitTest(const ZeroTouchEnrollmentScreenUnitTest&) =
      delete;
  ZeroTouchEnrollmentScreenUnitTest& operator=(
      const ZeroTouchEnrollmentScreenUnitTest&) = delete;

  enum AttestationEnrollmentStatus {
    SUCCESS,
    DEVICE_NOT_SETUP_FOR_ZERO_TOUCH,
    DMSERVER_ERROR
  };

  // Closure passed to EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock
  // which creates the EnterpriseEnrollmentHelperMock object that will
  // eventually be tied to the EnrollmentScreen. It also sets up the
  // appropriate expectations for testing with the Google Mock framework.
  // The template parameter should_enroll indicates whether or not
  // the EnterpriseEnrollmentHelper should be mocked to successfully enroll.
  void SetupMockEnrollmentHelper(AttestationEnrollmentStatus status) {
    std::unique_ptr<EnterpriseEnrollmentHelperMock> mock =
        std::make_unique<EnterpriseEnrollmentHelperMock>();
    EnterpriseEnrollmentHelperMock* mock_ptr = mock.get();
    if (status == SUCCESS) {
      // Define behavior of EnrollUsingAttestation to successfully enroll.
      EXPECT_CALL(*mock, EnrollUsingAttestation())
          .Times(AnyNumber())
          .WillRepeatedly(Invoke([mock_ptr]() {
            static_cast<EnrollmentScreen*>(mock_ptr->status_consumer())
                ->ShowEnrollmentStatusOnSuccess();
          }));
    } else {
      // Define behavior of EnrollUsingAttestation to fail to enroll.
      const policy::EnrollmentStatus enrollment_status =
          policy::EnrollmentStatus::ForRegistrationError(
              status == DEVICE_NOT_SETUP_FOR_ZERO_TOUCH
                  ? policy::DeviceManagementStatus::
                        DM_STATUS_SERVICE_DEVICE_NOT_FOUND
                  : policy::DeviceManagementStatus::
                        DM_STATUS_TEMPORARY_UNAVAILABLE);
      EXPECT_CALL(*mock, EnrollUsingAttestation())
          .Times(AnyNumber())
          .WillRepeatedly(Invoke([mock_ptr, enrollment_status]() {
            mock_ptr->status_consumer()->OnEnrollmentError(enrollment_status);
          }));
    }
    // Define behavior of ClearAuth to only run the callback it is given.
    EXPECT_CALL(*mock, ClearAuth(_))
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(
            [](base::OnceClosure callback) { std::move(callback).Run(); }));

    EnterpriseEnrollmentHelper::SetEnrollmentHelperMock(std::move(mock));
  }

  void SetUpEnrollmentScreen() override {
    enrollment_config_.mode =
        policy::EnrollmentConfig::MODE_ATTESTATION_LOCAL_FORCED;
    enrollment_config_.auth_mechanism =
        policy::EnrollmentConfig::AUTH_MECHANISM_ATTESTATION;
    EnrollmentScreenUnitTest::SetUpEnrollmentScreen();
  }

  virtual void SetUpEnrollmentScreenForFallback() {
    enrollment_config_.mode = policy::EnrollmentConfig::MODE_ATTESTATION;
    enrollment_config_.auth_mechanism =
        policy::EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE;
    EnrollmentScreenUnitTest::SetUpEnrollmentScreen();
  }

  // testing::Test:
  void SetUp() override {
    EnrollmentScreenUnitTest::SetUp();

    // Configure the browser to use Hands-Off Enrollment.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnterpriseEnableZeroTouchEnrollment, "hands-off");
  }

  void TestRetry() {
    // Define behavior of EnterpriseEnrollmentHelperMock to always fail
    // enrollment.
    SetupMockEnrollmentHelper(DMSERVER_ERROR);

    SetUpEnrollmentScreen();

    // Remove jitter to enable deterministic testing.
    enrollment_screen_->retry_policy_.jitter_factor = 0;

    // Start zero-touch enrollment.
    enrollment_screen_->Show(wizard_context_.get());

    // Fast forward time by 1 minute.
    FastForwardTime(base::Minutes(1));

    // Check that we have retried 4 times.
    EXPECT_EQ(enrollment_screen_->num_retries_, 4);
  }

  void TestFinishEnrollmentFlow() {
    // Define behavior of EnterpriseEnrollmentHelperMock to successfully enroll.
    SetupMockEnrollmentHelper(SUCCESS);

    SetUpEnrollmentScreen();

    // Start zero-touch enrollment.
    enrollment_screen_->Show(wizard_context_.get());

    // Verify that enrollment flow finished and exited cleanly.
    ASSERT_TRUE(last_screen_result_.has_value());
    EXPECT_EQ(EnrollmentScreen::Result::COMPLETED, last_screen_result_.value());
  }

  void TestFallback() {
    // Define behavior of EnterpriseEnrollmentHelperMock to fail
    // attestation-based enrollment.
    SetupMockEnrollmentHelper(DEVICE_NOT_SETUP_FOR_ZERO_TOUCH);

    SetUpEnrollmentScreenForFallback();

    // Once we fallback we show a sign in screen for manual enrollment.
    EXPECT_CALL(*GetMockScreenView(), Show()).Times(2);

    // Start enrollment.
    enrollment_screen_->Show(wizard_context_.get());
  }
};

TEST_F(ZeroTouchEnrollmentScreenUnitTest, FinishEnrollmentFlow) {
  TestFinishEnrollmentFlow();
}

TEST_F(ZeroTouchEnrollmentScreenUnitTest, Fallback) {
  TestFallback();
}

TEST_F(ZeroTouchEnrollmentScreenUnitTest, Retry) {
  TestRetry();
}

TEST_F(ZeroTouchEnrollmentScreenUnitTest, DoNotRetryOnTopOfUser) {
  // Define behavior of EnterpriseEnrollmentHelperMock to always fail
  // enrollment.
  SetupMockEnrollmentHelper(DMSERVER_ERROR);

  SetUpEnrollmentScreen();

  // Remove jitter to enable deterministic testing.
  enrollment_screen_->retry_policy_.jitter_factor = 0;

  // Start zero-touch enrollment.
  enrollment_screen_->Show(wizard_context_.get());

  // Schedule user retry button click after 30 sec.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&EnrollmentScreen::OnRetry,
                     enrollment_screen_->weak_ptr_factory_.GetWeakPtr()),
      base::Seconds(30));

  // Fast forward time by 1 minute.
  FastForwardTime(base::Minutes(1));

  // Check that the number of retries is still 4.
  EXPECT_EQ(enrollment_screen_->num_retries_, 4);
}

TEST_F(ZeroTouchEnrollmentScreenUnitTest, DoNotRetryAfterSuccess) {
  // Define behavior of EnterpriseEnrollmentHelperMock to successfully enroll.
  SetupMockEnrollmentHelper(SUCCESS);

  SetUpEnrollmentScreen();

  // Start zero-touch enrollment.
  enrollment_screen_->Show(wizard_context_.get());

  // Fast forward time by 1 minute.
  FastForwardTime(base::Minutes(1));

  // Check that we do not retry.
  EXPECT_EQ(enrollment_screen_->num_retries_, 0);
}

/*
 * We base these tests off ZeroTouchEnrollmenScreenUnitTest for two reasons:
 *   1. We want to check that some same tests pass in both classes
 *   2. We want to leverage Zero-Touch Hands Off to test for proper completions
 */
class AutomaticEnrollmentScreenUnitTest
    : public ZeroTouchEnrollmentScreenUnitTest,
      public ::testing::WithParamInterface<policy::EnrollmentConfig::Mode> {
 public:
  AutomaticEnrollmentScreenUnitTest() = default;

  AutomaticEnrollmentScreenUnitTest(const AutomaticEnrollmentScreenUnitTest&) =
      delete;
  AutomaticEnrollmentScreenUnitTest& operator=(
      const AutomaticEnrollmentScreenUnitTest&) = delete;

  void SetUpEnrollmentScreen() override {
    enrollment_config_.mode = GetParam();
    enrollment_config_.auth_mechanism =
        policy::EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE;
    EnrollmentScreenUnitTest::SetUpEnrollmentScreen();
  }

  void SetUpEnrollmentScreenForFallback() override {
    // Automatic re-enrollment is always setup for fallback.
    SetUpEnrollmentScreen();
  }
};

TEST_P(AutomaticEnrollmentScreenUnitTest, ShowErrorPanel) {
  // We use Zero-Touch's test for retries as a way to know that there was
  // an error pane with a Retry button displayed to the user when we encounter
  // a DMServer error that is not that the device isn't setup for Auto RE.
  TestRetry();
}

TEST_P(AutomaticEnrollmentScreenUnitTest, FinishEnrollmentFlow) {
  TestFinishEnrollmentFlow();
}

TEST_P(AutomaticEnrollmentScreenUnitTest, Fallback) {
  TestFallback();
}

INSTANTIATE_TEST_SUITE_P(
    P,
    AutomaticEnrollmentScreenUnitTest,
    ::testing::Values(
        policy::EnrollmentConfig::MODE_ATTESTATION_INITIAL_SERVER_FORCED,
        policy::EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED));

}  // namespace ash
