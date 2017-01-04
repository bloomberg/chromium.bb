// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/validating_authenticator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

namespace {

using testing::_;
using testing::Return;

typedef ValidatingAuthenticator::Result ValidationResult;

const char kRemoteTestJid[] = "ficticious_jid_for_testing";

// testing::InvokeArgument<N> does not work with base::Callback, fortunately
// gmock makes it simple to create action templates that do for the various
// possible numbers of arguments.
ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_0_VALUE_PARAMS()) {
  ::std::tr1::get<k>(args).Run();
}

}  // namespace

class ValidatingAuthenticatorTest : public testing::Test {
 public:
  ValidatingAuthenticatorTest();
  ~ValidatingAuthenticatorTest() override;

  void ValidateCallback(
      const std::string& remote_jid,
      const ValidatingAuthenticator::ResultCallback& callback);

 protected:
  // testing::Test overrides.
  void SetUp() override;

  // Calls ProcessMessage() on |validating_authenticator_| and blocks until
  // the result callback is called.
  void SendMessageAndWaitForCallback();

  // Used to set up our mock behaviors on the MockAuthenticator object passed
  // to |validating_authenticator_|.  Lifetime of the object is controlled by
  // |validating_authenticator_| so this pointer is no longer valid once
  // the owner is destroyed.
  MockAuthenticator* mock_authenticator_ = nullptr;

  // This member is used to drive behavior in |validating_authenticator_| when
  // it's validation complete callback is run.
  ValidationResult validation_result_ = ValidationResult::SUCCESS;

  // Tracks whether our ValidateCallback has been called or not.
  bool validate_complete_called_ = false;

  // The object under test.
  std::unique_ptr<ValidatingAuthenticator> validating_authenticator_;

 private:
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ValidatingAuthenticatorTest);
};

ValidatingAuthenticatorTest::ValidatingAuthenticatorTest() {}

ValidatingAuthenticatorTest::~ValidatingAuthenticatorTest() {}

void ValidatingAuthenticatorTest::ValidateCallback(
    const std::string& remote_jid,
    const ValidatingAuthenticator::ResultCallback& callback) {
  validate_complete_called_ = true;
  callback.Run(validation_result_);
}

void ValidatingAuthenticatorTest::SetUp() {
  mock_authenticator_ = new MockAuthenticator();
  std::unique_ptr<Authenticator> authenticator(mock_authenticator_);

  validating_authenticator_.reset(new ValidatingAuthenticator(
      kRemoteTestJid, base::Bind(&ValidatingAuthenticatorTest::ValidateCallback,
                                 base::Unretained(this)),
      std::move(authenticator)));
}

void ValidatingAuthenticatorTest::SendMessageAndWaitForCallback() {
  base::RunLoop run_loop;
  std::unique_ptr<buzz::XmlElement> first_message(
      Authenticator::CreateEmptyAuthenticatorMessage());
  validating_authenticator_->ProcessMessage(first_message.get(),
                                            run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(ValidatingAuthenticatorTest, ValidConnection_SingleMessage) {
  EXPECT_CALL(*mock_authenticator_, ProcessMessage(_, _))
      .Times(1)
      .WillOnce(InvokeCallbackArgument<1>());

  ON_CALL(*mock_authenticator_, state())
      .WillByDefault(Return(Authenticator::ACCEPTED));

  SendMessageAndWaitForCallback();
  ASSERT_TRUE(validate_complete_called_);
  ASSERT_EQ(validating_authenticator_->state(), Authenticator::ACCEPTED);
}

TEST_F(ValidatingAuthenticatorTest, ValidConnection_TwoMessages) {
  // Send the first message to the authenticator, set the mock up to act
  // like it is waiting for a second message.
  EXPECT_CALL(*mock_authenticator_, ProcessMessage(_, _))
      .Times(2)
      .WillRepeatedly(InvokeCallbackArgument<1>());

  EXPECT_CALL(*mock_authenticator_, state())
      .WillRepeatedly(Return(Authenticator::MESSAGE_READY));

  SendMessageAndWaitForCallback();
  ASSERT_TRUE(validate_complete_called_);
  ASSERT_EQ(validating_authenticator_->state(), Authenticator::MESSAGE_READY);

  // Now 'retrieve' the message for the client which resets the state.
  EXPECT_CALL(*mock_authenticator_, state())
      .WillRepeatedly(Return(Authenticator::WAITING_MESSAGE));

  // This dance is needed because GMock doesn't handle unique_ptrs very well.
  // The mock method receives a raw pointer which it wraps and returns when
  // GetNextMessage() is called.
  std::unique_ptr<buzz::XmlElement> next_message(
      Authenticator::CreateEmptyAuthenticatorMessage());
  EXPECT_CALL(*mock_authenticator_, GetNextMessagePtr())
      .Times(1)
      .WillOnce(Return(next_message.release()));

  validating_authenticator_->GetNextMessage();
  ASSERT_EQ(validating_authenticator_->state(), Authenticator::WAITING_MESSAGE);

  // Now send the second message for processing.
  EXPECT_CALL(*mock_authenticator_, state())
      .WillRepeatedly(Return(Authenticator::ACCEPTED));

  // Reset the callback state, we don't expect the validate function to be
  // called for the second message.
  validate_complete_called_ = false;
  SendMessageAndWaitForCallback();
  ASSERT_FALSE(validate_complete_called_);
  ASSERT_EQ(validating_authenticator_->state(), Authenticator::ACCEPTED);
}

TEST_F(ValidatingAuthenticatorTest, InvalidConnection_RejectedByUser) {
  EXPECT_CALL(*mock_authenticator_, ProcessMessage(_, _)).Times(0);
  EXPECT_CALL(*mock_authenticator_, state()).Times(0);
  EXPECT_CALL(*mock_authenticator_, rejection_reason()).Times(0);

  validation_result_ = ValidationResult::ERROR_REJECTED_BY_USER;

  SendMessageAndWaitForCallback();
  ASSERT_TRUE(validate_complete_called_);
  ASSERT_EQ(validating_authenticator_->state(), Authenticator::REJECTED);
  ASSERT_EQ(validating_authenticator_->rejection_reason(),
            Authenticator::REJECTED_BY_USER);
}

TEST_F(ValidatingAuthenticatorTest, InvalidConnection_InvalidCredentials) {
  EXPECT_CALL(*mock_authenticator_, ProcessMessage(_, _)).Times(0);
  EXPECT_CALL(*mock_authenticator_, state()).Times(0);
  EXPECT_CALL(*mock_authenticator_, rejection_reason()).Times(0);

  validation_result_ = ValidationResult::ERROR_INVALID_CREDENTIALS;

  SendMessageAndWaitForCallback();
  ASSERT_TRUE(validate_complete_called_);
  ASSERT_EQ(validating_authenticator_->state(), Authenticator::REJECTED);
  ASSERT_EQ(validating_authenticator_->rejection_reason(),
            Authenticator::INVALID_CREDENTIALS);
}

TEST_F(ValidatingAuthenticatorTest, InvalidConnection_InvalidAccount) {
  EXPECT_CALL(*mock_authenticator_, ProcessMessage(_, _)).Times(0);
  EXPECT_CALL(*mock_authenticator_, state()).Times(0);
  EXPECT_CALL(*mock_authenticator_, rejection_reason()).Times(0);

  validation_result_ = ValidationResult::ERROR_INVALID_ACCOUNT;

  SendMessageAndWaitForCallback();
  ASSERT_TRUE(validate_complete_called_);
  ASSERT_EQ(validating_authenticator_->state(), Authenticator::REJECTED);
  ASSERT_EQ(validating_authenticator_->rejection_reason(),
            Authenticator::INVALID_ACCOUNT);
}

TEST_F(ValidatingAuthenticatorTest,
       WrappedAuthenticatorRejectsConnection_InvalidCredentials) {
  EXPECT_CALL(*mock_authenticator_, ProcessMessage(_, _))
      .Times(1)
      .WillOnce(InvokeCallbackArgument<1>());

  ON_CALL(*mock_authenticator_, state())
      .WillByDefault(Return(Authenticator::REJECTED));

  ON_CALL(*mock_authenticator_, rejection_reason())
      .WillByDefault(Return(Authenticator::REJECTED_BY_USER));

  SendMessageAndWaitForCallback();
  ASSERT_TRUE(validate_complete_called_);
  ASSERT_EQ(validating_authenticator_->state(), Authenticator::REJECTED);
  ASSERT_EQ(validating_authenticator_->rejection_reason(),
            Authenticator::REJECTED_BY_USER);
}

TEST_F(ValidatingAuthenticatorTest,
       WrappedAuthenticatorRejectsConnection_InvalidAccount) {
  EXPECT_CALL(*mock_authenticator_, ProcessMessage(_, _))
      .Times(1)
      .WillOnce(InvokeCallbackArgument<1>());

  ON_CALL(*mock_authenticator_, state())
      .WillByDefault(Return(Authenticator::REJECTED));

  ON_CALL(*mock_authenticator_, rejection_reason())
      .WillByDefault(Return(Authenticator::INVALID_CREDENTIALS));

  SendMessageAndWaitForCallback();
  ASSERT_TRUE(validate_complete_called_);
  ASSERT_EQ(validating_authenticator_->state(), Authenticator::REJECTED);
  ASSERT_EQ(validating_authenticator_->rejection_reason(),
            Authenticator::INVALID_CREDENTIALS);
}

TEST_F(ValidatingAuthenticatorTest,
       WrappedAuthenticatorRejectsConnection_PROTOCOL_ERROR) {
  EXPECT_CALL(*mock_authenticator_, ProcessMessage(_, _))
      .Times(1)
      .WillOnce(InvokeCallbackArgument<1>());

  ON_CALL(*mock_authenticator_, state())
      .WillByDefault(Return(Authenticator::REJECTED));

  ON_CALL(*mock_authenticator_, rejection_reason())
      .WillByDefault(Return(Authenticator::PROTOCOL_ERROR));

  SendMessageAndWaitForCallback();
  ASSERT_TRUE(validate_complete_called_);
  ASSERT_EQ(validating_authenticator_->state(), Authenticator::REJECTED);
  ASSERT_EQ(validating_authenticator_->rejection_reason(),
            Authenticator::PROTOCOL_ERROR);
}

}  // namespace protocol
}  // namespace remoting
