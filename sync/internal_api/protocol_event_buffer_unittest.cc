// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "sync/internal_api/protocol_event_buffer.h"
#include "sync/internal_api/public/events/poll_get_updates_request_event.h"
#include "sync/internal_api/public/events/protocol_event.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class ProtocolEventBufferTest : public ::testing::Test {
 public:
  ProtocolEventBufferTest();
  virtual ~ProtocolEventBufferTest();

  static scoped_ptr<ProtocolEvent> MakeTestEvent(int64 id);
  static bool HasId(const ProtocolEvent& event, int64 id);

 protected:
  ProtocolEventBuffer buffer_;
};

ProtocolEventBufferTest::ProtocolEventBufferTest() {}

ProtocolEventBufferTest::~ProtocolEventBufferTest() {}

scoped_ptr<ProtocolEvent> ProtocolEventBufferTest::MakeTestEvent(int64 id) {
  sync_pb::ClientToServerMessage message;
  return scoped_ptr<ProtocolEvent>(
      new PollGetUpdatesRequestEvent(
          base::Time::FromInternalValue(id),
          message));
}

bool ProtocolEventBufferTest::HasId(const ProtocolEvent& event, int64 id) {
  return event.GetTimestamp() == base::Time::FromInternalValue(id);
}

TEST_F(ProtocolEventBufferTest, AddThenReturnEvents) {
  scoped_ptr<ProtocolEvent> e1(MakeTestEvent(1));
  scoped_ptr<ProtocolEvent> e2(MakeTestEvent(2));

  buffer_.RecordProtocolEvent(*e1);
  buffer_.RecordProtocolEvent(*e2);

  ScopedVector<ProtocolEvent> buffered_events(
      buffer_.GetBufferedProtocolEvents());

  ASSERT_EQ(2U, buffered_events.size());
  EXPECT_TRUE(HasId(*(buffered_events[0]), 1));
  EXPECT_TRUE(HasId(*(buffered_events[1]), 2));
}

TEST_F(ProtocolEventBufferTest, AddThenOverflowThenReturnEvents) {
  for (size_t i = 0; i < ProtocolEventBuffer::kBufferSize+1; ++i) {
    scoped_ptr<ProtocolEvent> e(MakeTestEvent(static_cast<int64>(i)));
    buffer_.RecordProtocolEvent(*e);
  }

  ScopedVector<ProtocolEvent> buffered_events(
      buffer_.GetBufferedProtocolEvents());
  ASSERT_EQ(ProtocolEventBuffer::kBufferSize, buffered_events.size());

  for (size_t i = 1; i < ProtocolEventBuffer::kBufferSize+1; ++i) {
    EXPECT_TRUE(
        HasId(*(buffered_events[i-1]), static_cast<int64>(i)));
  }

}


}  // namespace syncer
