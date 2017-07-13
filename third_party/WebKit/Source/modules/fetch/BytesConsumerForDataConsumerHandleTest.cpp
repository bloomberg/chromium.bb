// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/BytesConsumerForDataConsumerHandle.h"

#include "core/testing/DummyPageHolder.h"
#include "modules/fetch/BytesConsumer.h"
#include "modules/fetch/DataConsumerHandleTestUtil.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

using DataConsumerCommand = DataConsumerHandleTestUtil::Command;
using Checkpoint = ::testing::StrictMock<::testing::MockFunction<void(int)>>;
using ReplayingHandle = DataConsumerHandleTestUtil::ReplayingHandle;
using Result = BytesConsumer::Result;
using ::testing::ByMove;
using ::testing::InSequence;
using ::testing::Return;

class BytesConsumerForDataConsumerHandleTest : public ::testing::Test {
 public:
  Document* GetDocument() { return &page_->GetDocument(); }

 protected:
  BytesConsumerForDataConsumerHandleTest() : page_(DummyPageHolder::Create()) {}
  ~BytesConsumerForDataConsumerHandleTest() {
    ThreadState::Current()->CollectAllGarbage();
  }
  std::unique_ptr<DummyPageHolder> page_;
};

class MockBytesConsumerClient
    : public GarbageCollectedFinalized<MockBytesConsumerClient>,
      public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(MockBytesConsumerClient);

 public:
  static MockBytesConsumerClient* Create() {
    return new ::testing::StrictMock<MockBytesConsumerClient>();
  }
  MOCK_METHOD0(OnStateChange, void());

 protected:
  MockBytesConsumerClient() {}
};

class MockDataConsumerHandle final : public WebDataConsumerHandle {
 public:
  class MockReaderProxy : public GarbageCollectedFinalized<MockReaderProxy> {
   public:
    MOCK_METHOD3(BeginRead,
                 WebDataConsumerHandle::Result(const void**,
                                               WebDataConsumerHandle::Flags,
                                               size_t*));
    MOCK_METHOD1(EndRead, WebDataConsumerHandle::Result(size_t));

    DEFINE_INLINE_TRACE() {}
  };

  MockDataConsumerHandle() : proxy_(new MockReaderProxy) {}
  MockReaderProxy* Proxy() { return proxy_; }
  const char* DebugName() const { return "MockDataConsumerHandle"; }

 private:
  class Reader final : public WebDataConsumerHandle::Reader {
   public:
    explicit Reader(MockReaderProxy* proxy) : proxy_(proxy) {}
    Result BeginRead(const void** buffer,
                     Flags flags,
                     size_t* available) override {
      return proxy_->BeginRead(buffer, flags, available);
    }
    Result EndRead(size_t read_size) override {
      return proxy_->EndRead(read_size);
    }

   private:
    Persistent<MockReaderProxy> proxy_;
  };

  std::unique_ptr<WebDataConsumerHandle::Reader> ObtainReader(
      Client*) override {
    return WTF::MakeUnique<Reader>(proxy_);
  }
  Persistent<MockReaderProxy> proxy_;
};

TEST_F(BytesConsumerForDataConsumerHandleTest, Create) {
  std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::Create();
  handle->Add(DataConsumerCommand(DataConsumerCommand::kData, "hello"));
  handle->Add(DataConsumerCommand(DataConsumerCommand::kDone));
  Persistent<BytesConsumer> consumer =
      new BytesConsumerForDataConsumerHandle(GetDocument(), std::move(handle));
}

TEST_F(BytesConsumerForDataConsumerHandleTest, BecomeReadable) {
  Checkpoint checkpoint;
  Persistent<MockBytesConsumerClient> client =
      MockBytesConsumerClient::Create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(2));

  std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::Create();
  handle->Add(DataConsumerCommand(DataConsumerCommand::kData, "hello"));
  Persistent<BytesConsumer> consumer =
      new BytesConsumerForDataConsumerHandle(GetDocument(), std::move(handle));
  consumer->SetClient(client);
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            consumer->GetPublicState());

  checkpoint.Call(1);
  testing::RunPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            consumer->GetPublicState());
}

TEST_F(BytesConsumerForDataConsumerHandleTest, BecomeClosed) {
  Checkpoint checkpoint;
  Persistent<MockBytesConsumerClient> client =
      MockBytesConsumerClient::Create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(2));

  std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::Create();
  handle->Add(DataConsumerCommand(DataConsumerCommand::kDone));
  Persistent<BytesConsumer> consumer =
      new BytesConsumerForDataConsumerHandle(GetDocument(), std::move(handle));
  consumer->SetClient(client);
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            consumer->GetPublicState());

  checkpoint.Call(1);
  testing::RunPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
}

TEST_F(BytesConsumerForDataConsumerHandleTest, BecomeErrored) {
  Checkpoint checkpoint;
  Persistent<MockBytesConsumerClient> client =
      MockBytesConsumerClient::Create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(2));

  std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::Create();
  handle->Add(DataConsumerCommand(DataConsumerCommand::kError));
  Persistent<BytesConsumer> consumer =
      new BytesConsumerForDataConsumerHandle(GetDocument(), std::move(handle));
  consumer->SetClient(client);
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            consumer->GetPublicState());

  checkpoint.Call(1);
  testing::RunPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(BytesConsumer::PublicState::kErrored, consumer->GetPublicState());
}

TEST_F(BytesConsumerForDataConsumerHandleTest, ClearClient) {
  Checkpoint checkpoint;
  Persistent<MockBytesConsumerClient> client =
      MockBytesConsumerClient::Create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));

  std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::Create();
  handle->Add(DataConsumerCommand(DataConsumerCommand::kError));
  Persistent<BytesConsumer> consumer =
      new BytesConsumerForDataConsumerHandle(GetDocument(), std::move(handle));
  consumer->SetClient(client);
  consumer->ClearClient();

  checkpoint.Call(1);
  testing::RunPendingTasks();
  checkpoint.Call(2);
}

TEST_F(BytesConsumerForDataConsumerHandleTest, TwoPhaseReadWhenReadable) {
  std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::Create();
  handle->Add(DataConsumerCommand(DataConsumerCommand::kData, "hello"));
  Persistent<BytesConsumer> consumer =
      new BytesConsumerForDataConsumerHandle(GetDocument(), std::move(handle));
  consumer->SetClient(MockBytesConsumerClient::Create());

  const char* buffer = nullptr;
  size_t available = 0;
  ASSERT_EQ(Result::kOk, consumer->BeginRead(&buffer, &available));
  EXPECT_EQ("hello", String(buffer, available));

  ASSERT_EQ(Result::kOk, consumer->EndRead(1));
  ASSERT_EQ(Result::kOk, consumer->BeginRead(&buffer, &available));
  EXPECT_EQ("ello", String(buffer, available));

  ASSERT_EQ(Result::kOk, consumer->EndRead(4));
  ASSERT_EQ(Result::kShouldWait, consumer->BeginRead(&buffer, &available));
}

TEST_F(BytesConsumerForDataConsumerHandleTest, TwoPhaseReadWhenWaiting) {
  std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::Create();
  Persistent<BytesConsumer> consumer =
      new BytesConsumerForDataConsumerHandle(GetDocument(), std::move(handle));
  consumer->SetClient(MockBytesConsumerClient::Create());
  const char* buffer = nullptr;
  size_t available = 0;
  ASSERT_EQ(Result::kShouldWait, consumer->BeginRead(&buffer, &available));
}

TEST_F(BytesConsumerForDataConsumerHandleTest, TwoPhaseReadWhenClosed) {
  std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::Create();
  handle->Add(DataConsumerCommand(DataConsumerCommand::kDone));
  Persistent<BytesConsumer> consumer =
      new BytesConsumerForDataConsumerHandle(GetDocument(), std::move(handle));
  consumer->SetClient(MockBytesConsumerClient::Create());
  const char* buffer = nullptr;
  size_t available = 0;
  ASSERT_EQ(Result::kDone, consumer->BeginRead(&buffer, &available));
}

TEST_F(BytesConsumerForDataConsumerHandleTest, TwoPhaseReadWhenErrored) {
  std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::Create();
  handle->Add(DataConsumerCommand(DataConsumerCommand::kError));
  Persistent<BytesConsumer> consumer =
      new BytesConsumerForDataConsumerHandle(GetDocument(), std::move(handle));
  consumer->SetClient(MockBytesConsumerClient::Create());
  const char* buffer = nullptr;
  size_t available = 0;
  ASSERT_EQ(Result::kError, consumer->BeginRead(&buffer, &available));
  EXPECT_EQ(BytesConsumer::Error("error"), consumer->GetError());
}

TEST_F(BytesConsumerForDataConsumerHandleTest, Cancel) {
  std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::Create();
  Persistent<BytesConsumer> consumer =
      new BytesConsumerForDataConsumerHandle(GetDocument(), std::move(handle));
  consumer->SetClient(MockBytesConsumerClient::Create());
  consumer->Cancel();
  const char* buffer = nullptr;
  size_t available = 0;
  ASSERT_EQ(Result::kDone, consumer->BeginRead(&buffer, &available));
}

TEST_F(BytesConsumerForDataConsumerHandleTest, drainAsBlobDataHandle) {
  // WebDataConsumerHandle::Reader::drainAsBlobDataHandle should return
  // nullptr from the second time, but we don't care that here.
  std::unique_ptr<MockDataConsumerHandle> handle =
      WTF::WrapUnique(new MockDataConsumerHandle);
  Persistent<MockDataConsumerHandle::MockReaderProxy> proxy = handle->Proxy();
  Persistent<BytesConsumer> consumer =
      new BytesConsumerForDataConsumerHandle(GetDocument(), std::move(handle));
  consumer->SetClient(MockBytesConsumerClient::Create());

  Checkpoint checkpoint;
  InSequence s;

  EXPECT_FALSE(consumer->DrainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::kDisallowBlobWithInvalidSize));
  EXPECT_FALSE(consumer->DrainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::kAllowBlobWithInvalidSize));
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            consumer->GetPublicState());
}

TEST_F(BytesConsumerForDataConsumerHandleTest, drainAsFormData) {
  std::unique_ptr<MockDataConsumerHandle> handle =
      WTF::WrapUnique(new MockDataConsumerHandle);
  Persistent<MockDataConsumerHandle::MockReaderProxy> proxy = handle->Proxy();
  Persistent<BytesConsumer> consumer =
      new BytesConsumerForDataConsumerHandle(GetDocument(), std::move(handle));
  consumer->SetClient(MockBytesConsumerClient::Create());

  Checkpoint checkpoint;
  InSequence s;

  EXPECT_FALSE(consumer->DrainAsFormData());
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            consumer->GetPublicState());
}

}  // namespace

}  // namespace blink
