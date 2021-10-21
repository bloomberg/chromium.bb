// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/settings_controller.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager_internal.h"
#include "chromeos/services/libassistant/test_support/fake_assistant_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace chromeos {
namespace libassistant {

namespace {

#define EXPECT_NO_CALLS(args...) EXPECT_CALL(args).Times(0);
#define IGNORE_CALLS(args...) EXPECT_CALL(args).Times(testing::AnyNumber());
// The auth tokens are pairs of <user, token>
using AuthTokens = std::vector<std::pair<std::string, std::string>>;

std::vector<mojom::AuthenticationTokenPtr> ToVector(
    mojom::AuthenticationTokenPtr token) {
  std::vector<mojom::AuthenticationTokenPtr> result;
  result.push_back(std::move(token));
  return result;
}

class AssistantClientMock : public FakeAssistantClient {
 public:
  AssistantClientMock(
      std::unique_ptr<assistant::FakeAssistantManager> assistant_manager,
      assistant::FakeAssistantManagerInternal* assistant_manager_internal)
      : FakeAssistantClient(std::move(assistant_manager),
                            assistant_manager_internal) {}
  ~AssistantClientMock() override = default;

  // FakeAssistantClient:
  MOCK_METHOD(
      void,
      UpdateAssistantSettings,
      (const ::assistant::ui::SettingsUiUpdate& settings,
       const std::string& user_id,
       base::OnceCallback<void(
           const ::assistant::api::UpdateAssistantSettingsResponse&)> on_done));
  MOCK_METHOD(
      void,
      GetAssistantSettings,
      (const ::assistant::ui::SettingsUiSelector& selector,
       const std::string& user_id,
       base::OnceCallback<void(
           const ::assistant::api::GetAssistantSettingsResponse&)> on_done));
  MOCK_METHOD(void, SetLocaleOverride, (const std::string& locale));
  MOCK_METHOD(void,
              SetInternalOptions,
              (const std::string& locale, bool spoken_feedback_enabled));
  MOCK_METHOD(void, SetDeviceAttributes, (bool dark_mode_enabled));
  MOCK_METHOD(void, EnableListening, (bool listening_enabled));
  MOCK_METHOD(void, SetAuthenticationInfo, (const AuthTokens& tokens));
};

}  // namespace

class AssistantSettingsControllerTest : public testing::Test {
 public:
  AssistantSettingsControllerTest() { Init(); }

  AssistantSettingsControllerTest(const AssistantSettingsControllerTest&) =
      delete;
  AssistantSettingsControllerTest& operator=(
      const AssistantSettingsControllerTest&) = delete;
  ~AssistantSettingsControllerTest() override = default;

  SettingsController& controller() { return controller_; }

  void CreateLibassistant() {
    controller().OnAssistantClientCreated(assistant_client_.get());
  }

  void StartLibassistant() {
    controller().OnAssistantClientStarted(assistant_client_.get());
  }

  void DestroyLibassistant() {
    controller().OnDestroyingAssistantClient(assistant_client_.get());

    Init();
  }

  void CreateAndStartLibassistant() {
    CreateLibassistant();
    StartLibassistant();
  }

  void Init() {
    assistant_client_ = nullptr;
    assistant_manager_internal_ = nullptr;

    auto assistant_manager =
        std::make_unique<assistant::FakeAssistantManager>();
    assistant_manager_internal_ = std::make_unique<
        testing::StrictMock<assistant::FakeAssistantManagerInternal>>();
    assistant_client_ = std::make_unique<AssistantClientMock>(
        std::move(assistant_manager), assistant_manager_internal_.get());
  }

  AssistantClientMock& assistant_client_mock() { return *assistant_client_; }

 private:
  base::test::SingleThreadTaskEnvironment environment_;

  SettingsController controller_;
  std::unique_ptr<assistant::FakeAssistantManagerInternal>
      assistant_manager_internal_;
  std::unique_ptr<AssistantClientMock> assistant_client_;
};

TEST_F(AssistantSettingsControllerTest,
       ShouldNotCrashIfLibassistantIsNotCreated) {
  controller().SetAuthenticationTokens({});
  controller().SetHotwordEnabled(true);
  controller().SetListeningEnabled(true);
  controller().SetLocale("locale");
  controller().SetSpokenFeedbackEnabled(true);
  controller().SetDarkModeEnabled(false);
}

TEST_F(AssistantSettingsControllerTest,
       ShouldNotCrashAfterDestroyingLibassistant) {
  CreateAndStartLibassistant();
  DestroyLibassistant();

  controller().SetAuthenticationTokens({});
  controller().SetHotwordEnabled(true);
  controller().SetListeningEnabled(true);
  controller().SetLocale("locale");
  controller().SetSpokenFeedbackEnabled(true);
  controller().SetDarkModeEnabled(false);
}

TEST_F(AssistantSettingsControllerTest,
       ShouldResetAllValuesWhenLibassistantIsDestroyed) {
  controller().SetAuthenticationTokens({});
  controller().SetHotwordEnabled(true);
  controller().SetLocale("locale");
  controller().SetSpokenFeedbackEnabled(true);
  controller().SetDarkModeEnabled(true);

  DestroyLibassistant();

  // After destroying Libassistant, the settings should be cleared.
  // We test this by ensuring they are not applied when Libassistant starts.
  EXPECT_NO_CALLS(assistant_client_mock(), SetLocaleOverride);
  EXPECT_NO_CALLS(assistant_client_mock(), SetInternalOptions);
  EXPECT_NO_CALLS(assistant_client_mock(), UpdateAssistantSettings);
  EXPECT_NO_CALLS(assistant_client_mock(), SetAuthenticationInfo);
  CreateAndStartLibassistant();
}

TEST_F(AssistantSettingsControllerTest, ShouldSetLocale) {
  CreateLibassistant();

  EXPECT_CALL(assistant_client_mock(), SetLocaleOverride("locale"));

  controller().SetLocale("locale");
}

TEST_F(AssistantSettingsControllerTest,
       ShouldUseDefaultLocaleIfSettingToEmptyString) {
  const std::string default_locale = icu::Locale::getDefault().getName();
  CreateLibassistant();

  EXPECT_CALL(assistant_client_mock(), SetLocaleOverride(default_locale));

  controller().SetLocale("");
}

TEST_F(AssistantSettingsControllerTest,
       ShouldNotSetInternalOptionsWhenLocaleIsNotSet) {
  CreateLibassistant();

  EXPECT_NO_CALLS(assistant_client_mock(), SetInternalOptions);
  EXPECT_NO_CALLS(assistant_client_mock(), SetDeviceAttributes);

  controller().SetSpokenFeedbackEnabled(true);
  controller().SetDarkModeEnabled(false);
}

TEST_F(AssistantSettingsControllerTest,
       ShouldNotSetInternalOptionsWhenSpokenFeedbackEnabledIsNotSet) {
  IGNORE_CALLS(assistant_client_mock(), SetLocaleOverride);
  CreateLibassistant();

  EXPECT_NO_CALLS(assistant_client_mock(), SetInternalOptions);
  EXPECT_NO_CALLS(assistant_client_mock(), SetDeviceAttributes);

  controller().SetLocale("locale");
  controller().SetDarkModeEnabled(false);
}

TEST_F(AssistantSettingsControllerTest,
       ShouldNotSetInternalOptionsWhenDarkModeEnabledIsNotSet) {
  IGNORE_CALLS(assistant_client_mock(), SetLocaleOverride);
  CreateLibassistant();

  EXPECT_NO_CALLS(assistant_client_mock(), SetInternalOptions);
  EXPECT_NO_CALLS(assistant_client_mock(), SetDeviceAttributes);

  controller().SetLocale("locale");
  controller().SetSpokenFeedbackEnabled(true);
}

TEST_F(AssistantSettingsControllerTest,
       ShouldSetInternalOptionsWhenLocaleIsUpdated) {
  IGNORE_CALLS(assistant_client_mock(), SetLocaleOverride);
  controller().SetSpokenFeedbackEnabled(true);
  controller().SetDarkModeEnabled(false);
  CreateLibassistant();

  EXPECT_CALL(assistant_client_mock(), SetInternalOptions);

  controller().SetLocale("locale");
}

TEST_F(AssistantSettingsControllerTest,
       ShouldSetInternalOptionsWhenSpokenFeedbackEnabledIsUpdated) {
  IGNORE_CALLS(assistant_client_mock(), SetLocaleOverride);
  controller().SetLocale("locale");
  controller().SetDarkModeEnabled(false);
  CreateLibassistant();

  EXPECT_CALL(assistant_client_mock(), SetInternalOptions);

  controller().SetSpokenFeedbackEnabled(true);
}

TEST_F(AssistantSettingsControllerTest,
       ShouldSetInternalOptionsWhenDarkModeEnabledIsUpdated) {
  IGNORE_CALLS(assistant_client_mock(), SetLocaleOverride);
  controller().SetLocale("locale");
  controller().SetSpokenFeedbackEnabled(true);
  CreateLibassistant();

  EXPECT_CALL(assistant_client_mock(), SetInternalOptions);
  EXPECT_CALL(assistant_client_mock(), SetDeviceAttributes);

  controller().SetDarkModeEnabled(false);
}

TEST_F(AssistantSettingsControllerTest,
       ShouldSetInternalOptionsAndLocaleWhenLibassistantIsCreated) {
  controller().SetLocale("locale");
  controller().SetSpokenFeedbackEnabled(true);
  controller().SetDarkModeEnabled(false);

  EXPECT_CALL(assistant_client_mock(), SetLocaleOverride);
  EXPECT_CALL(assistant_client_mock(), SetInternalOptions);

  CreateLibassistant();
}

TEST_F(AssistantSettingsControllerTest,
       ShouldNotSetDeviceOptionsWhenLocaleIsNotSet) {
  IGNORE_CALLS(assistant_client_mock(), SetLocaleOverride);
  CreateAndStartLibassistant();

  EXPECT_NO_CALLS(assistant_client_mock(), UpdateAssistantSettings);

  controller().SetHotwordEnabled(true);
}

TEST_F(AssistantSettingsControllerTest,
       ShouldNotSetDeviceOptionsWhenHotwordEnabledIsNotSet) {
  IGNORE_CALLS(assistant_client_mock(), SetLocaleOverride);
  CreateAndStartLibassistant();

  EXPECT_NO_CALLS(assistant_client_mock(), UpdateAssistantSettings);

  controller().SetLocale("locale");
}

TEST_F(AssistantSettingsControllerTest,
       ShouldSetDeviceOptionsWhenLocaleIsUpdated) {
  IGNORE_CALLS(assistant_client_mock(), SetLocaleOverride);
  CreateAndStartLibassistant();
  controller().SetHotwordEnabled(true);

  EXPECT_CALL(assistant_client_mock(), UpdateAssistantSettings);

  controller().SetLocale("locale");
}

TEST_F(AssistantSettingsControllerTest,
       ShouldSetDeviceOptionsWhenHotwordEnabledIsUpdated) {
  IGNORE_CALLS(assistant_client_mock(), SetLocaleOverride);
  CreateAndStartLibassistant();
  controller().SetLocale("locale");

  EXPECT_CALL(assistant_client_mock(), UpdateAssistantSettings);

  controller().SetHotwordEnabled(true);
}

TEST_F(AssistantSettingsControllerTest,
       ShouldSetDeviceOptionsWhenLibassistantIsStarted) {
  IGNORE_CALLS(assistant_client_mock(), SetLocaleOverride);
  CreateLibassistant();

  controller().SetLocale("locale");
  controller().SetHotwordEnabled(true);

  EXPECT_CALL(assistant_client_mock(), UpdateAssistantSettings);

  StartLibassistant();
}

TEST_F(AssistantSettingsControllerTest, ShouldSetAuthenticationTokens) {
  const AuthTokens expected = {{"user", "token"}};

  CreateLibassistant();

  EXPECT_CALL(assistant_client_mock(), SetAuthenticationInfo(expected));

  controller().SetAuthenticationTokens(
      ToVector(mojom::AuthenticationToken::New("user", "token")));
}

TEST_F(AssistantSettingsControllerTest,
       ShouldSetAuthenticationTokensWhenLibassistantIsCreated) {
  const AuthTokens expected = {{"user", "token"}};

  controller().SetAuthenticationTokens(
      ToVector(mojom::AuthenticationToken::New("user", "token")));

  EXPECT_CALL(assistant_client_mock(), SetAuthenticationInfo(expected));

  CreateLibassistant();
}

TEST_F(AssistantSettingsControllerTest,
       ShouldSupportEmptyAuthenticationTokenList) {
  CreateLibassistant();

  const AuthTokens expected = {};
  EXPECT_CALL(assistant_client_mock(), SetAuthenticationInfo(expected));

  controller().SetAuthenticationTokens({});
}

TEST_F(AssistantSettingsControllerTest, ShouldSetListeningEnabled) {
  CreateLibassistant();

  EXPECT_CALL(assistant_client_mock(), EnableListening(true));

  controller().SetListeningEnabled(true);
}

TEST_F(AssistantSettingsControllerTest,
       ShouldSetListeningEnabledWhenLibassistantIsCreated) {
  controller().SetListeningEnabled(false);

  EXPECT_CALL(assistant_client_mock(), EnableListening(false));

  CreateLibassistant();
}

TEST_F(AssistantSettingsControllerTest,
       GetSettingsShouldCallCallbackEvenIfLibassistantIsNotStarted) {
  base::MockCallback<SettingsController::GetSettingsCallback> callback;

  EXPECT_CALL(callback, Run(std::string{}));

  controller().GetSettings("selector", /*include_header=*/false, callback.Get());
}

TEST_F(AssistantSettingsControllerTest,
       GetSettingsShouldCallCallbackIfLibassistantIsStopped) {
  CreateAndStartLibassistant();

  base::MockCallback<SettingsController::GetSettingsCallback> callback;
  controller().GetSettings("selector", /*include_header=*/false, callback.Get());

  EXPECT_CALL(callback, Run(std::string{}));
  DestroyLibassistant();
}

TEST_F(AssistantSettingsControllerTest,
       UpdateSettingsShouldCallCallbackEvenIfLibassistantIsNotStarted) {
  base::MockCallback<SettingsController::UpdateSettingsCallback> callback;

  EXPECT_CALL(callback, Run(std::string{}));

  controller().UpdateSettings("selector", callback.Get());
}

TEST_F(AssistantSettingsControllerTest,
       UpdateSettingsShouldCallCallbackIfLibassistantIsStopped) {
  IGNORE_CALLS(assistant_client_mock(), UpdateAssistantSettings);
  CreateAndStartLibassistant();

  base::MockCallback<SettingsController::UpdateSettingsCallback> callback;
  controller().UpdateSettings("selector", callback.Get());

  EXPECT_CALL(callback, Run(std::string{}));
  DestroyLibassistant();
}

TEST_F(AssistantSettingsControllerTest,
       ShouldInvokeGetAssistantSettingsWhenGetSettingsCalled) {
  CreateAndStartLibassistant();

  EXPECT_CALL(assistant_client_mock(), GetAssistantSettings);

  controller().GetSettings("selector", /*include_header=*/false,
                           base::DoNothing());

  DestroyLibassistant();
}

TEST_F(AssistantSettingsControllerTest,
       ShouldInvokeUpdateAssistantSettingsWhenUpdateSettingsCalled) {
  CreateAndStartLibassistant();

  EXPECT_CALL(assistant_client_mock(), UpdateAssistantSettings);

  controller().UpdateSettings("selector", base::DoNothing());

  DestroyLibassistant();
}

}  // namespace libassistant
}  // namespace chromeos
