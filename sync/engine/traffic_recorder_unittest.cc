// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/traffic_recorder.h"

#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/values.h"
#include "sync/protocol/sync.pb.h"
#include "sync/util/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

const unsigned int kMaxMessages = 10;
const unsigned int kMaxMessageSize = 5 * 1024;

// Ensure the number of records don't exceed |kMaxMessages|.
TEST(TrafficRecorderTest, MaxRecordsTest) {
  TrafficRecorder recorder(kMaxMessages, kMaxMessageSize);
  sync_pb::ClientToServerResponse response;

  for (unsigned int i = 0; i < 2*kMaxMessages; ++i)
    recorder.RecordClientToServerResponse(response);

  EXPECT_EQ(recorder.records().size(), kMaxMessages);
}

// Ensure records with size greater than |kMaxMessageSize| are truncated.
TEST(TrafficRecorderTest, MaxMessageSizeTest) {
  sync_pb::ClientToServerResponse response;

  sync_pb::ClientToServerResponse::Error* error = response.mutable_error();
  std::string error_description(kMaxMessageSize * 2, 'a');
  error->set_error_description(error_description);

  TrafficRecorder recorder(kMaxMessages, kMaxMessageSize);
  recorder.RecordClientToServerResponse(response);

  TrafficRecorder::TrafficRecord record = recorder.records().front();
  EXPECT_TRUE(record.truncated);
  EXPECT_TRUE(record.message.empty());
}

// Test implementation of TrafficRecorder.
class TestTrafficRecorder : public TrafficRecorder {
 public:
  TestTrafficRecorder(unsigned int max_messages, unsigned int max_message_size)
      : TrafficRecorder(max_messages, max_message_size) {
    set_time(0);
  }
  virtual ~TestTrafficRecorder() {}

  virtual base::Time GetTime() OVERRIDE {
    return time_;
  }

  void set_time(int64 time)  {
    time_ = ProtoTimeToTime(time);
  }

  void set_time(base::Time time) {
    time_ = time;
  }

 private:
  base::Time time_;
};

// Ensure that timestamp is recorded correctly in traffic record.
TEST(TrafficRecorderTest, TimestampTest) {
  sync_pb::ClientToServerResponse response;

  TestTrafficRecorder recorder(kMaxMessages, kMaxMessageSize);
  recorder.set_time(3);
  recorder.RecordClientToServerResponse(response);

  base::Time expect_time = ProtoTimeToTime(3);
  TrafficRecorder::TrafficRecord record = recorder.records().front();
  EXPECT_EQ(expect_time, record.timestamp);
}

// Ensure that timestamps are recorded correctly in traffic records.
TEST(TrafficRecorderTest, MultipleTimestampTest) {
  sync_pb::ClientToServerResponse response;
  base::Time sample_time_1 = ProtoTimeToTime(GG_INT64_C(1359484676659));
  base::Time sample_time_2 = ProtoTimeToTime(GG_INT64_C(135948467665932));

  TestTrafficRecorder recorder(kMaxMessages, kMaxMessageSize);
  recorder.set_time(sample_time_1);
  recorder.RecordClientToServerResponse(response);
  recorder.set_time(sample_time_2);
  recorder.RecordClientToServerResponse(response);

  TrafficRecorder::TrafficRecord record_1 = recorder.records().front();
  TrafficRecorder::TrafficRecord record_2 = recorder.records().back();
  EXPECT_EQ(sample_time_1, record_1.timestamp);
  EXPECT_EQ(sample_time_2, record_2.timestamp);
}

// Ensure that timestamp is added to ListValue of DictionaryValues in ToValue().
TEST(TrafficRecorderTest, ToValueTimestampTest) {
  sync_pb::ClientToServerResponse response;
  base::Time sample_time = ProtoTimeToTime(GG_INT64_C(135948467665932));
  std::string expect_time_str = GetTimeDebugString(sample_time);

  TestTrafficRecorder recorder(kMaxMessages, kMaxMessageSize);
  recorder.set_time(sample_time);
  recorder.RecordClientToServerResponse(response);

  scoped_ptr<ListValue> value;
  value.reset(recorder.ToValue());

  DictionaryValue* record_value;
  std::string time_str;

  ASSERT_TRUE(value->GetDictionary(0, &record_value));
  EXPECT_TRUE(record_value->GetString("timestamp", &time_str));
  EXPECT_EQ(expect_time_str, time_str);
}

}  // namespace syncer
