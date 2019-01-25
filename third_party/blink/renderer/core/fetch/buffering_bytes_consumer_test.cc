// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/buffering_bytes_consumer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/fetch/bytes_consumer_test_util.h"
#include "third_party/blink/renderer/platform/loader/testing/bytes_consumer_test_reader.h"
#include "third_party/blink/renderer/platform/loader/testing/replaying_bytes_consumer.h"

namespace blink {
namespace {

class BufferingBytesConsumerTest : public testing::Test {
 public:
  using Command = ReplayingBytesConsumer::Command;
  using Result = BytesConsumer::Result;
  using PublicState = BytesConsumer::PublicState;

  static String CharVectorToString(const Vector<char>& x) {
    return BytesConsumerTestUtil::CharVectorToString(x);
  }
};

TEST_F(BufferingBytesConsumerTest, Read) {
  V8TestingScope scope;
  auto* replaying_bytes_consumer = MakeGarbageCollected<ReplayingBytesConsumer>(
      scope.GetDocument().GetTaskRunner(TaskType::kNetworking));

  replaying_bytes_consumer->Add(Command(Command::kWait));
  replaying_bytes_consumer->Add(Command(Command::kData, "1"));
  replaying_bytes_consumer->Add(Command(Command::kWait));
  replaying_bytes_consumer->Add(Command(Command::kWait));
  replaying_bytes_consumer->Add(Command(Command::kData, "23"));
  replaying_bytes_consumer->Add(Command(Command::kData, "4"));
  replaying_bytes_consumer->Add(Command(Command::kData, "567"));
  replaying_bytes_consumer->Add(Command(Command::kData, "8"));
  replaying_bytes_consumer->Add(Command(Command::kDone));

  auto* bytes_consumer =
      MakeGarbageCollected<BufferingBytesConsumer>(replaying_bytes_consumer);

  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());
  auto* reader = MakeGarbageCollected<BytesConsumerTestReader>(bytes_consumer);
  auto result = reader->Run();

  EXPECT_EQ(PublicState::kClosed, bytes_consumer->GetPublicState());
  ASSERT_EQ(result.first, Result::kDone);
  EXPECT_EQ("12345678", CharVectorToString(result.second));
}

}  // namespace
}  // namespace blink
