// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_rotation_launcher.h"

#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/mock_key_rotation_command.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/scoped_key_rotation_command_factory.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {

namespace {
const char kNonce[] = "nonce";
const char kFakeDMToken[] = "fake-browser-dm-token";
const char kFakeClientId[] = "fake-client-id";
const char kExpectedDmServerUrl[] =
    "https://example.com/"
    "management_service?retry=false&agent=Chrome+1.2.3(456)&apptype=Chrome&"
    "critical=true&deviceid=fake-client-id&devicetype=2&platform=Test%7CUnit%"
    "7C1.2.3&request=browser_public_key_upload";
}  // namespace

class KeyRotationLauncherTest : public testing::Test {
 protected:
  void SetUp() override {
    auto mock_command =
        std::make_unique<testing::StrictMock<test::MockKeyRotationCommand>>();
    mock_command_ = mock_command.get();
    scoped_command_factory_.SetMock(std::move(mock_command));
  }

  std::unique_ptr<KeyRotationLauncher> CreateLauncher() {
    return KeyRotationLauncher::Create(&fake_dm_token_storage_,
                                       &fake_device_management_service_);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  raw_ptr<testing::StrictMock<test::MockKeyRotationCommand>> mock_command_;
  ScopedKeyRotationCommandFactory scoped_command_factory_;
  policy::FakeBrowserDMTokenStorage fake_dm_token_storage_;
  testing::StrictMock<policy::MockJobCreationHandler> job_creation_handler_;
  policy::FakeDeviceManagementService fake_device_management_service_{
      &job_creation_handler_};
};

TEST_F(KeyRotationLauncherTest, LaunchKeyRotation) {
  // Set valid values.
  fake_dm_token_storage_.SetDMToken(kFakeDMToken);
  fake_dm_token_storage_.SetClientId(kFakeClientId);

  absl::optional<KeyRotationCommand::Params> params;
  EXPECT_CALL(*mock_command_, Trigger(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&params](const KeyRotationCommand::Params given_params,
                    KeyRotationCommand::Callback callback) {
            params = given_params;
            std::move(callback).Run(KeyRotationCommand::Status::SUCCEEDED);
          }));

  auto launcher = CreateLauncher();
  launcher->LaunchKeyRotation(kNonce, base::DoNothing());

  ASSERT_TRUE(params.has_value());
  EXPECT_EQ(kNonce, params->nonce);
  EXPECT_EQ(kFakeDMToken, params->dm_token);
  EXPECT_EQ(kExpectedDmServerUrl, params->dm_server_url);
}

TEST_F(KeyRotationLauncherTest, LaunchKeyRotation_InvalidDMToken) {
  // Set the DM token to an invalid value (i.e. empty string).
  fake_dm_token_storage_.SetDMToken("");

  auto launcher = CreateLauncher();
  bool callback_called;
  launcher->LaunchKeyRotation(
      kNonce, base::BindLambdaForTesting(
                  [&callback_called](KeyRotationCommand::Status status) {
                    EXPECT_EQ(KeyRotationCommand::Status::FAILED, status);
                    callback_called = true;
                  }));
  EXPECT_TRUE(callback_called);
}

}  // namespace enterprise_connectors
