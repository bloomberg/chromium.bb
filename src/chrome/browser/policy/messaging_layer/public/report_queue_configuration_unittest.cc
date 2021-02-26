// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_queue_configuration.h"

#include <stdio.h>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/proto/record_constants.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

using policy::DMToken;
using PolicyCheckCallback = ReportQueueConfiguration::PolicyCheckCallback;

constexpr char kDmToken[] = "dm_token";

PolicyCheckCallback GetSuccessfulCallback() {
  return base::BindRepeating([]() { return Status::StatusOK(); });
}

// Tests to ensure that only valid parameters are used to generate a
// ReportQueueConfiguration.
TEST(ReportQueueConfigurationTest, ParametersMustBeValid) {
  // Invalid Parameters
  const DMToken invalid_dm_token = DMToken::CreateInvalidTokenForTesting();
  const Destination invalid_destination = Destination::UNDEFINED_DESTINATION;
  const PolicyCheckCallback invalid_callback;

  // Valid Parameters
  const DMToken valid_dm_token = DMToken::CreateValidTokenForTesting(kDmToken);
  const Destination valid_destination = Destination::UPLOAD_EVENTS;
  const PolicyCheckCallback valid_callback = GetSuccessfulCallback();

  // Test Invalid DMToken.
  EXPECT_FALSE(ReportQueueConfiguration::Create(
                   invalid_dm_token, valid_destination, valid_callback)
                   .ok());

  // Test Invalid Destination.
  EXPECT_FALSE(ReportQueueConfiguration::Create(
                   valid_dm_token, invalid_destination, valid_callback)
                   .ok());

  // Test Invalid Callback.
  EXPECT_FALSE(ReportQueueConfiguration::Create(
                   valid_dm_token, valid_destination, invalid_callback)
                   .ok());

  EXPECT_TRUE(ReportQueueConfiguration::Create(
                  valid_dm_token, valid_destination, GetSuccessfulCallback())
                  .ok());
}

class TestCallbackHandler {
 public:
  TestCallbackHandler() = default;
  MOCK_METHOD(Status, Callback, (), ());
};

TEST(ReportQueueConfigurationTest, UsesProvidedPolicyCheckCallback) {
  const DMToken dm_token = DMToken::CreateValidTokenForTesting(kDmToken);
  const Destination destination = Destination::UPLOAD_EVENTS;

  TestCallbackHandler handler;
  auto config_result = ReportQueueConfiguration::Create(
      dm_token, destination,
      base::BindRepeating(&TestCallbackHandler::Callback,
                          base::Unretained(&handler)));
  EXPECT_TRUE(config_result.ok());

  auto config = std::move(config_result.ValueOrDie());

  EXPECT_CALL(handler, Callback())
      .WillOnce(::testing::Return(Status::StatusOK()));
  EXPECT_OK(config->CheckPolicy());
}

}  // namespace
}  // namespace reporting
