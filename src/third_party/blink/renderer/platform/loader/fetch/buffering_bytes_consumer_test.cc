// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/buffering_bytes_consumer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/loader/testing/bytes_consumer_test_reader.h"
#include "third_party/blink/renderer/platform/loader/testing/replaying_bytes_consumer.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"

namespace blink {
namespace {

class BufferingBytesConsumerTest : public testing::Test {
 public:
  using Command = ReplayingBytesConsumer::Command;
  using Result = BytesConsumer::Result;
  using PublicState = BytesConsumer::PublicState;
};

TEST_F(BufferingBytesConsumerTest, Read) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* replaying_bytes_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);

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
  auto result = reader->Run(task_runner.get());

  EXPECT_EQ(PublicState::kClosed, bytes_consumer->GetPublicState());
  ASSERT_EQ(result.first, Result::kDone);
  EXPECT_EQ("12345678", String(result.second.data(), result.second.size()));
}

TEST_F(BufferingBytesConsumerTest, Buffering) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* replaying_bytes_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);

  replaying_bytes_consumer->Add(Command(Command::kWait));
  replaying_bytes_consumer->Add(Command(Command::kData, "1"));
  replaying_bytes_consumer->Add(Command(Command::kData, "23"));
  replaying_bytes_consumer->Add(Command(Command::kData, "4"));
  replaying_bytes_consumer->Add(Command(Command::kWait));
  replaying_bytes_consumer->Add(Command(Command::kData, "567"));
  replaying_bytes_consumer->Add(Command(Command::kData, "8"));
  replaying_bytes_consumer->Add(Command(Command::kDone));

  auto* bytes_consumer =
      MakeGarbageCollected<BufferingBytesConsumer>(replaying_bytes_consumer);

  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());
  EXPECT_EQ(PublicState::kReadableOrWaiting,
            replaying_bytes_consumer->GetPublicState());

  task_runner->RunUntilIdle();

  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());
  EXPECT_EQ(PublicState::kClosed, replaying_bytes_consumer->GetPublicState());

  auto* reader = MakeGarbageCollected<BytesConsumerTestReader>(bytes_consumer);
  auto result = reader->Run(task_runner.get());

  EXPECT_EQ(PublicState::kClosed, bytes_consumer->GetPublicState());
  ASSERT_EQ(result.first, Result::kDone);
  EXPECT_EQ("12345678", String(result.second.data(), result.second.size()));
}

TEST_F(BufferingBytesConsumerTest, StopBuffering) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* replaying_bytes_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);

  replaying_bytes_consumer->Add(Command(Command::kWait));
  replaying_bytes_consumer->Add(Command(Command::kData, "1"));
  replaying_bytes_consumer->Add(Command(Command::kData, "23"));
  replaying_bytes_consumer->Add(Command(Command::kData, "4"));
  replaying_bytes_consumer->Add(Command(Command::kWait));
  replaying_bytes_consumer->Add(Command(Command::kData, "567"));
  replaying_bytes_consumer->Add(Command(Command::kData, "8"));
  replaying_bytes_consumer->Add(Command(Command::kDone));

  auto* bytes_consumer =
      MakeGarbageCollected<BufferingBytesConsumer>(replaying_bytes_consumer);
  bytes_consumer->StopBuffering();

  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());
  EXPECT_EQ(PublicState::kReadableOrWaiting,
            replaying_bytes_consumer->GetPublicState());

  task_runner->RunUntilIdle();

  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());
  EXPECT_EQ(PublicState::kReadableOrWaiting,
            replaying_bytes_consumer->GetPublicState());

  auto* reader = MakeGarbageCollected<BytesConsumerTestReader>(bytes_consumer);
  auto result = reader->Run(task_runner.get());

  EXPECT_EQ(PublicState::kClosed, bytes_consumer->GetPublicState());
  ASSERT_EQ(result.first, Result::kDone);
  EXPECT_EQ("12345678", String(result.second.data(), result.second.size()));
}

}  // namespace
}  // namespace blink
