// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/traffic_recorder.h"

#include "sync/protocol/sync.pb.h"
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

}  // namespace syncer
