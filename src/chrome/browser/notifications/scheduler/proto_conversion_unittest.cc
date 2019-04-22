// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/proto_conversion.h"

#include "base/logging.h"
#include "chrome/browser/notifications/scheduler/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using IconProto = notifications::proto::Icon;

namespace notifications {
namespace {

const char kUuid[] = "123";
const char kData[] = "bitmapdata";

void TestClientStateConversion(ClientState* client_state) {
  DCHECK(client_state);
  notifications::proto::ClientState proto;
  ClientState expected;
  ClientStateToProto(client_state, &proto);
  ClientStateFromProto(&proto, &expected);
  EXPECT_EQ(*client_state, expected)
      << " \n Output: \n " << client_state->DebugPrint() << " \n Expected: \n"
      << expected.DebugPrint();
}

TEST(ProtoConversionTest, IconEntryFromProto) {
  IconProto proto;
  proto.set_uuid(kUuid);
  proto.set_icon(kData);
  IconEntry entry;

  IconEntryFromProto(&proto, &entry);

  // Verify entry data.
  EXPECT_EQ(entry.uuid, kUuid);
  EXPECT_EQ(entry.data, kData);
}

TEST(ProtoConversionTest, IconEntryToProto) {
  IconEntry entry;
  entry.data = kData;
  entry.uuid = kUuid;
  IconProto proto;

  IconEntryToProto(&entry, &proto);

  // Verify proto data.
  EXPECT_EQ(proto.icon(), kData);
  EXPECT_EQ(proto.uuid(), kUuid);
}

// Verifies client state proto conversion.
TEST(ProtoConversionTest, ClientStateProtoConversion) {
  // Verify basic fields.
  ClientState client_state;
  test::ImpressionTestData test_data{
      SchedulerClientType::kTest1, 3, {}, base::nullopt};
  test::AddImpressionTestData(test_data, &client_state);
  TestClientStateConversion(&client_state);

  // Verify suppression info.
  base::Time last_trigger_time;
  bool success =
      base::Time::FromString("04/25/20 01:00:00 AM", &last_trigger_time);
  DCHECK(success);
  auto duration = base::TimeDelta::FromDays(7);
  auto suppression = SuppressionInfo(last_trigger_time, duration);
  suppression.recover_goal = 5;
  client_state.suppression_info = std::move(suppression);
  TestClientStateConversion(&client_state);
}

// Verifies impression proto conversion.
TEST(ProtoConversionTest, ImpressionProtoConversion) {
  ClientState client_state;
  base::Time create_time;
  bool success = base::Time::FromString("03/25/19 00:00:00 AM", &create_time);
  DCHECK(success);
  Impression impression{create_time, UserFeedback::kHelpful,
                        ImpressionResult::kPositive, true,
                        SchedulerTaskTime::kMorning};
  client_state.impressions.emplace_back(impression);
  TestClientStateConversion(&client_state);

  auto& first_impression = *client_state.impressions.begin();

  // Verify all feedback types.
  std::vector<UserFeedback> feedback_types{
      UserFeedback::kNoFeedback, UserFeedback::kHelpful,
      UserFeedback::kNotHelpful, UserFeedback::kClick,
      UserFeedback::kDismiss,    UserFeedback::kIgnore};
  for (const auto feedback_type : feedback_types) {
    first_impression.feedback = feedback_type;
    TestClientStateConversion(&client_state);
  }

  // Verify all impression result types.
  std::vector<ImpressionResult> impression_results{
      ImpressionResult::kInvalid, ImpressionResult::kPositive,
      ImpressionResult::kNegative, ImpressionResult::kNeutral};
  for (const auto impression_result : impression_results) {
    first_impression.impression = impression_result;
    TestClientStateConversion(&client_state);
  }

  // Verify all scheduler task time types.
  std::vector<SchedulerTaskTime> task_times{SchedulerTaskTime::kUnknown,
                                            SchedulerTaskTime::kMorning,
                                            SchedulerTaskTime::kEvening};
  for (const auto task_start_time : task_times) {
    first_impression.task_start_time = task_start_time;
    TestClientStateConversion(&client_state);
  }
}

// Verifies multiple impressions are serialized correctly.
TEST(ProtoConversionTest, MultipleImpressionConversion) {
  ClientState client_state;
  base::Time create_time;
  bool success = base::Time::FromString("04/25/20 01:00:00 AM", &create_time);
  DCHECK(success);

  Impression impression{create_time, UserFeedback::kHelpful,
                        ImpressionResult::kPositive, true,
                        SchedulerTaskTime::kMorning};
  Impression other_impression{create_time, UserFeedback::kNoFeedback,
                              ImpressionResult::kNegative, false,
                              SchedulerTaskTime::kEvening};
  client_state.impressions.emplace_back(std::move(impression));
  client_state.impressions.emplace_back(std::move(other_impression));
  TestClientStateConversion(&client_state);
}

}  // namespace
}  // namespace notifications
