// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/save_generated_password_action.h"

#include <string>
#include <utility>

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/mock_website_login_manager.h"
#include "components/autofill_assistant/browser/value_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {
const char kMemoryKeyForGeneratedPassword[] = "memory-key-for-generation";
const char kGeneratedPassword[] = "mX.12pq";
}  // namespace

namespace autofill_assistant {
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;

class SaveGeneratedPasswordActionTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(mock_action_delegate_, GetWebsiteLoginManager)
        .WillByDefault(Return(&mock_website_login_manager_));

    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));

    ON_CALL(mock_action_delegate_, WriteUserData)
        .WillByDefault(
            RunOnceCallback<0>(&user_data_, /* field_change = */ nullptr));
  }

 protected:
  MockActionDelegate mock_action_delegate_;
  MockWebsiteLoginManager mock_website_login_manager_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ActionProto proto_;
  UserData user_data_;
};

TEST_F(SaveGeneratedPasswordActionTest, SavedPassword) {
  SaveGeneratedPasswordProto* save_password_proto =
      proto_.mutable_save_generated_password();
  save_password_proto->set_memory_key(kMemoryKeyForGeneratedPassword);

  user_data_.additional_values_[kMemoryKeyForGeneratedPassword] =
      SimpleValue(std::string(kGeneratedPassword));

  ON_CALL(mock_website_login_manager_, OnReadyToCommitGeneratedPassword)
      .WillByDefault(Return(true));

  SaveGeneratedPasswordAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  EXPECT_CALL(mock_website_login_manager_, OnCommitGeneratedPassword).Times(1);

  action.ProcessAction(callback_.Get());

  EXPECT_FALSE(user_data_.has_additional_value(kMemoryKeyForGeneratedPassword));
}

TEST_F(SaveGeneratedPasswordActionTest, MissingMemoryKeyPreconditionFails) {
  SaveGeneratedPasswordProto* save_password_proto =
      proto_.mutable_save_generated_password();
  save_password_proto->set_memory_key(kMemoryKeyForGeneratedPassword);
  SaveGeneratedPasswordAction action(&mock_action_delegate_, proto_);
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PRECONDITION_FAILED))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SaveGeneratedPasswordActionTest, PresaveNotCalledPreconditionFails) {
  SaveGeneratedPasswordProto* save_password_proto =
      proto_.mutable_save_generated_password();
  save_password_proto->set_memory_key(kMemoryKeyForGeneratedPassword);

  user_data_.additional_values_[kMemoryKeyForGeneratedPassword] =
      SimpleValue(std::string(kGeneratedPassword));

  ON_CALL(mock_website_login_manager_, OnReadyToCommitGeneratedPassword)
      .WillByDefault(Return(false));

  SaveGeneratedPasswordAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PRECONDITION_FAILED))));

  EXPECT_CALL(mock_website_login_manager_, OnReadyToCommitGeneratedPassword)
      .Times(1);
  EXPECT_CALL(mock_website_login_manager_, OnCommitGeneratedPassword).Times(0);

  action.ProcessAction(callback_.Get());
}

}  // namespace autofill_assistant
