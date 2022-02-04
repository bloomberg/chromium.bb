// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/touch_to_fill_controller.h"

#include <memory>
#include <tuple>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/types/pass_key.h"
#include "components/device_reauth/biometric_authenticator.h"
#include "components/device_reauth/mock_biometric_authenticator.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using ShowVirtualKeyboard =
    password_manager::PasswordManagerDriver::ShowVirtualKeyboard;
using base::test::RunOnceCallback;
using device_reauth::BiometricAuthRequester;
using device_reauth::BiometricsAvailability;
using device_reauth::MockBiometricAuthenticator;
using password_manager::UiCredential;
using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;
using ::testing::WithArg;
using IsOriginSecure = TouchToFillView::IsOriginSecure;

using IsPublicSuffixMatch = UiCredential::IsPublicSuffixMatch;
using IsAffiliationBasedMatch = UiCredential::IsAffiliationBasedMatch;

constexpr char kExampleCom[] = "https://example.com/";

struct MockPasswordManagerDriver : password_manager::StubPasswordManagerDriver {
  MOCK_METHOD2(FillSuggestion,
               void(const std::u16string&, const std::u16string&));
  MOCK_METHOD1(TouchToFillClosed, void(ShowVirtualKeyboard));
  MOCK_CONST_METHOD0(GetLastCommittedURL, const GURL&());
};

struct MockTouchToFillView : TouchToFillView {
  MOCK_METHOD3(Show,
               void(const GURL&,
                    IsOriginSecure,
                    base::span<const UiCredential>));
  MOCK_METHOD1(OnCredentialSelected, void(const UiCredential&));
  MOCK_METHOD0(OnDismiss, void());
};

struct MakeUiCredentialParams {
  base::StringPiece username;
  base::StringPiece password;
  base::StringPiece origin = kExampleCom;
  bool is_public_suffix_match = false;
  bool is_affiliation_based_match = false;
  base::TimeDelta time_since_last_use;
};

UiCredential MakeUiCredential(MakeUiCredentialParams params) {
  return UiCredential(
      base::UTF8ToUTF16(params.username), base::UTF8ToUTF16(params.password),
      url::Origin::Create(GURL(params.origin)),
      IsPublicSuffixMatch(params.is_public_suffix_match),
      IsAffiliationBasedMatch(params.is_affiliation_based_match),
      base::Time::Now() - params.time_since_last_use);
}

}  // namespace

class TouchToFillControllerTest : public testing::Test {
 protected:
  using UkmBuilder = ukm::builders::TouchToFill_Shown;

  TouchToFillControllerTest() {
    auto mock_view = std::make_unique<MockTouchToFillView>();
    mock_view_ = mock_view.get();
    touch_to_fill_controller_.set_view(std::move(mock_view));

    ON_CALL(driver_, GetLastCommittedURL())
        .WillByDefault(ReturnRefOfCopy(GURL(kExampleCom)));

    scoped_feature_list_.InitAndEnableFeature(
        password_manager::features::kBiometricTouchToFill);
  }

  std::unique_ptr<TouchToFillController> CreateNoAuthController() {
    return std::make_unique<TouchToFillController>(
        base::PassKey<TouchToFillControllerTest>(), nullptr);
  }

  std::unique_ptr<TouchToFillController> CreateAuthController() {
    return std::make_unique<TouchToFillController>(
        base::PassKey<TouchToFillControllerTest>(), authenticator_);
  }

  MockPasswordManagerDriver& driver() { return driver_; }

  MockTouchToFillView& view() { return *mock_view_; }

  MockBiometricAuthenticator* authenticator() { return authenticator_.get(); }

  ukm::TestAutoSetUkmRecorder& test_recorder() { return test_recorder_; }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  TouchToFillController& touch_to_fill_controller() {
    return touch_to_fill_controller_;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  raw_ptr<MockTouchToFillView> mock_view_ = nullptr;
  scoped_refptr<MockBiometricAuthenticator> authenticator_ =
      base::MakeRefCounted<MockBiometricAuthenticator>();
  MockPasswordManagerDriver driver_;
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder test_recorder_;
  TouchToFillController touch_to_fill_controller_{
      base::PassKey<TouchToFillControllerTest>(), authenticator_};
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TouchToFillControllerTest, Show_And_Fill_No_Auth) {
  std::unique_ptr<TouchToFillController> controller_no_auth =
      CreateNoAuthController();
  std::unique_ptr<MockTouchToFillView> mock_view =
      std::make_unique<MockTouchToFillView>();
  MockTouchToFillView* weak_view = mock_view.get();
  controller_no_auth->set_view(std::move(mock_view));

  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(*weak_view, Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                               ElementsAreArray(credentials)));
  controller_no_auth->Show(credentials, driver().AsWeakPtr());

  // Test that we correctly log the absence of an Android credential.
  EXPECT_CALL(driver(), FillSuggestion(std::u16string(u"alice"),
                                       std::u16string(u"p4ssw0rd")));
  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(false)));
  controller_no_auth->OnCredentialSelected(credentials[0]);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.TouchToFill.NumCredentialsShown", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.FilledCredentialWasFromAndroidApp", false, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.TouchToFill.Outcome",
      TouchToFillController::TouchToFillOutcome::kCredentialFilled, 1);

  auto entries = test_recorder().GetEntriesByName(UkmBuilder::kEntryName);
  ASSERT_EQ(1u, entries.size());
  test_recorder().ExpectEntryMetric(
      entries[0], UkmBuilder::kUserActionName,
      static_cast<int64_t>(
          TouchToFillController::UserAction::kSelectedCredential));
}

TEST_F(TouchToFillControllerTest, Show_And_Fill_No_Auth_Available) {
  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials)));
  touch_to_fill_controller().Show(credentials, driver().AsWeakPtr());

  // Test that we correctly log the absence of an Android credential.
  EXPECT_CALL(driver(), FillSuggestion(std::u16string(u"alice"),
                                       std::u16string(u"p4ssw0rd")));
  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(false)));

  EXPECT_CALL(*authenticator(),
              CanAuthenticate(BiometricAuthRequester::kTouchToFill))
      .WillOnce(Return(BiometricsAvailability::kNoHardware));

  touch_to_fill_controller().OnCredentialSelected(credentials[0]);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.TouchToFill.NumCredentialsShown", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.FilledCredentialWasFromAndroidApp", false, 1);

  auto entries = test_recorder().GetEntriesByName(UkmBuilder::kEntryName);
  ASSERT_EQ(1u, entries.size());
  test_recorder().ExpectEntryMetric(
      entries[0], UkmBuilder::kUserActionName,
      static_cast<int64_t>(
          TouchToFillController::UserAction::kSelectedCredential));
}

TEST_F(TouchToFillControllerTest, Show_And_Fill_Auth_Available_Success) {
  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials)));
  touch_to_fill_controller().Show(credentials, driver().AsWeakPtr());

  EXPECT_CALL(driver(), FillSuggestion(std::u16string(u"alice"),
                                       std::u16string(u"p4ssw0rd")));
  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(false)));

  EXPECT_CALL(*authenticator(),
              CanAuthenticate(BiometricAuthRequester::kTouchToFill))
      .WillOnce(Return(BiometricsAvailability::kAvailable));
  EXPECT_CALL(*authenticator(),
              Authenticate(BiometricAuthRequester::kTouchToFill, _))
      .WillOnce(RunOnceCallback<1>(true));
  touch_to_fill_controller().OnCredentialSelected(credentials[0]);
}

TEST_F(TouchToFillControllerTest, Show_And_Fill_Auth_Available_Failure) {
  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials)));
  touch_to_fill_controller().Show(credentials, driver().AsWeakPtr());

  EXPECT_CALL(driver(), FillSuggestion(_, _)).Times(0);
  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(true)));

  EXPECT_CALL(*authenticator(),
              CanAuthenticate(BiometricAuthRequester::kTouchToFill))
      .WillOnce(Return(BiometricsAvailability::kAvailable));
  EXPECT_CALL(*authenticator(),
              Authenticate(BiometricAuthRequester::kTouchToFill, _))
      .WillOnce(RunOnceCallback<1>(false));
  touch_to_fill_controller().OnCredentialSelected(credentials[0]);

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.TouchToFill.Outcome",
      TouchToFillController::TouchToFillOutcome::kReauthenticationFailed, 1);
}

TEST_F(TouchToFillControllerTest, Show_Empty) {
  EXPECT_CALL(view(), Show).Times(0);
  touch_to_fill_controller().Show({}, driver().AsWeakPtr());
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.TouchToFill.NumCredentialsShown", 0, 1);
}

TEST_F(TouchToFillControllerTest, Show_Insecure_Origin) {
  EXPECT_CALL(driver(), GetLastCommittedURL())
      .WillOnce(ReturnRefOfCopy(GURL("http://example.com")));

  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(view(),
              Show(Eq(GURL("http://example.com")), IsOriginSecure(false),
                   ElementsAreArray(credentials)));
  touch_to_fill_controller().Show(credentials, driver().AsWeakPtr());
}

TEST_F(TouchToFillControllerTest, Show_And_Fill_Android_Credential) {
  // Test multiple credentials with one of them being an Android credential.
  UiCredential credentials[] = {
      MakeUiCredential({
          .username = "alice",
          .password = "p4ssw0rd",
          .time_since_last_use = base::Minutes(2),
      }),
      MakeUiCredential({
          .username = "bob",
          .password = "s3cr3t",
          .origin = "",
          .is_affiliation_based_match = true,
          .time_since_last_use = base::Minutes(3),
      }),
  };

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials)));
  touch_to_fill_controller().Show(credentials, driver().AsWeakPtr());

  // Test that we correctly log the presence of an Android credential.
  EXPECT_CALL(driver(), FillSuggestion(std::u16string(u"bob"),
                                       std::u16string(u"s3cr3t")));
  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(false)));
  EXPECT_CALL(*authenticator(),
              CanAuthenticate(BiometricAuthRequester::kTouchToFill))
      .WillOnce(Return(BiometricsAvailability::kNoHardware));
  touch_to_fill_controller().OnCredentialSelected(credentials[1]);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.TouchToFill.NumCredentialsShown", 2, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.FilledCredentialWasFromAndroidApp", true, 1);

  auto entries = test_recorder().GetEntriesByName(UkmBuilder::kEntryName);
  ASSERT_EQ(1u, entries.size());
  test_recorder().ExpectEntryMetric(
      entries[0], UkmBuilder::kUserActionName,
      static_cast<int64_t>(
          TouchToFillController::UserAction::kSelectedCredential));
}

// Verify that the credentials are ordered by their PSL match bit and last
// time used before being passed to the view.
TEST_F(TouchToFillControllerTest, Show_Orders_Credentials) {
  auto alice = MakeUiCredential({
      .username = "alice",
      .password = "p4ssw0rd",
      .time_since_last_use = base::Minutes(3),
  });
  auto bob = MakeUiCredential({
      .username = "bob",
      .password = "s3cr3t",
      .is_public_suffix_match = true,
      .time_since_last_use = base::Minutes(1),
  });
  auto charlie = MakeUiCredential({
      .username = "charlie",
      .password = "very_s3cr3t",
      .time_since_last_use = base::Minutes(2),
  });
  auto david = MakeUiCredential({
      .username = "david",
      .password = "even_more_s3cr3t",
      .is_public_suffix_match = true,
      .time_since_last_use = base::Minutes(4),
  });

  UiCredential credentials[] = {alice, bob, charlie, david};
  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           testing::ElementsAre(charlie, alice, bob, david)));
  touch_to_fill_controller().Show(credentials, driver().AsWeakPtr());
}

TEST_F(TouchToFillControllerTest, Dismiss) {
  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials)));
  touch_to_fill_controller().Show(credentials, driver().AsWeakPtr());

  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(true)));
  touch_to_fill_controller().OnDismiss();

  auto entries = test_recorder().GetEntriesByName(UkmBuilder::kEntryName);
  ASSERT_EQ(1u, entries.size());
  test_recorder().ExpectEntryMetric(
      entries[0], UkmBuilder::kUserActionName,
      static_cast<int64_t>(TouchToFillController::UserAction::kDismissed));
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.TouchToFill.Outcome",
      TouchToFillController::TouchToFillOutcome::kSheetDismissed, 1);
}

TEST_F(TouchToFillControllerTest, DestroyedWhileAuthRunning) {
  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials)));
  touch_to_fill_controller().Show(credentials, driver().AsWeakPtr());

  EXPECT_CALL(*authenticator(),
              CanAuthenticate(BiometricAuthRequester::kTouchToFill))
      .WillOnce(Return(BiometricsAvailability::kAvailable));
  EXPECT_CALL(*authenticator(),
              Authenticate(BiometricAuthRequester::kTouchToFill, _));
  touch_to_fill_controller().OnCredentialSelected(credentials[0]);

  EXPECT_CALL(*authenticator(), Cancel(BiometricAuthRequester::kTouchToFill));
}
