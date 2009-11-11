// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/thread.h"
#include "o3d/gpu_plugin/command_buffer.h"
#include "o3d/gpu_plugin/np_utils/np_browser_mock.h"
#include "o3d/gpu_plugin/np_utils/dynamic_np_object.h"
#include "o3d/gpu_plugin/np_utils/np_object_mock.h"
#include "o3d/gpu_plugin/np_utils/np_object_pointer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::SharedMemory;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

namespace gpu_plugin {

class CommandBufferTest : public testing::Test {
 protected:
  virtual void SetUp() {
    command_buffer_ = NPCreateObject<CommandBuffer>(NULL);
  }

  MockNPBrowser mock_browser_;
  NPObjectPointer<CommandBuffer> command_buffer_;
};

TEST_F(CommandBufferTest, NullRingBufferByDefault) {
  EXPECT_TRUE(NULL == command_buffer_->GetRingBuffer());
}

TEST_F(CommandBufferTest, InitializesCommandBuffer) {
  SharedMemory* ring_buffer = new SharedMemory;
  EXPECT_TRUE(ring_buffer->Create(std::wstring(), false, false, 1024));
  EXPECT_TRUE(command_buffer_->Initialize(ring_buffer));
  EXPECT_TRUE(ring_buffer == command_buffer_->GetRingBuffer());
  EXPECT_EQ(256, command_buffer_->GetSize());
}

TEST_F(CommandBufferTest, InitializeFailsSecondTime) {
  SharedMemory* ring_buffer = new SharedMemory;
  EXPECT_TRUE(command_buffer_->Initialize(ring_buffer));
  EXPECT_FALSE(command_buffer_->Initialize(ring_buffer));
}

TEST_F(CommandBufferTest, GetAndPutOffsetsDefaultToZero) {
  EXPECT_EQ(0, command_buffer_->GetGetOffset());
  EXPECT_EQ(0, command_buffer_->GetPutOffset());
}

class MockCallback : public CallbackRunner<Tuple0> {
 public:
  MOCK_METHOD1(RunWithParams, void(const Tuple0&));
};

TEST_F(CommandBufferTest, CanSyncGetAndPutOffset) {
  SharedMemory* ring_buffer = new SharedMemory;
  ring_buffer->Create(std::wstring(), false, false, 1024);

  EXPECT_TRUE(command_buffer_->Initialize(ring_buffer));

  StrictMock<MockCallback>* put_offset_change_callback =
      new StrictMock<MockCallback>;
  command_buffer_->SetPutOffsetChangeCallback(put_offset_change_callback);

  EXPECT_CALL(*put_offset_change_callback, RunWithParams(_));
  EXPECT_EQ(0, command_buffer_->SyncOffsets(2));
  EXPECT_EQ(2, command_buffer_->GetPutOffset());

  EXPECT_CALL(*put_offset_change_callback, RunWithParams(_));
  EXPECT_EQ(0, command_buffer_->SyncOffsets(4));
  EXPECT_EQ(4, command_buffer_->GetPutOffset());

  command_buffer_->SetGetOffset(2);
  EXPECT_EQ(2, command_buffer_->GetGetOffset());
  EXPECT_CALL(*put_offset_change_callback, RunWithParams(_));
  EXPECT_EQ(2, command_buffer_->SyncOffsets(6));

  EXPECT_EQ(-1, command_buffer_->SyncOffsets(-1));
  EXPECT_EQ(-1, command_buffer_->SyncOffsets(1024));
}

TEST_F(CommandBufferTest, ZeroHandleMapsToNull) {
  EXPECT_TRUE(NULL == command_buffer_->GetTransferBuffer(0));
}

TEST_F(CommandBufferTest, NegativeHandleMapsToNull) {
  EXPECT_TRUE(NULL == command_buffer_->GetTransferBuffer(-1));
}

TEST_F(CommandBufferTest, OutOfRangeHandleMapsToNull) {
  EXPECT_TRUE(NULL == command_buffer_->GetTransferBuffer(1));
}

TEST_F(CommandBufferTest, CanCreateTransferBuffers) {
  int32 handle = command_buffer_->CreateTransferBuffer(1024);
  EXPECT_EQ(1, handle);
  SharedMemory* buffer = command_buffer_->GetTransferBuffer(handle);
  ASSERT_TRUE(NULL != buffer);
  EXPECT_EQ(1024, buffer->max_size());
}

TEST_F(CommandBufferTest, CreateTransferBufferReturnsDistinctHandles) {
  EXPECT_EQ(1, command_buffer_->CreateTransferBuffer(1024));
}

TEST_F(CommandBufferTest, CreateTransferBufferReusesUnregisteredHandles) {
  EXPECT_EQ(1, command_buffer_->CreateTransferBuffer(1024));
  EXPECT_EQ(2, command_buffer_->CreateTransferBuffer(1024));
  command_buffer_->DestroyTransferBuffer(1);
  EXPECT_EQ(1, command_buffer_->CreateTransferBuffer(1024));
  EXPECT_EQ(3, command_buffer_->CreateTransferBuffer(1024));
}

TEST_F(CommandBufferTest, CannotUnregisterHandleZero) {
  command_buffer_->DestroyTransferBuffer(0);
  EXPECT_TRUE(NULL == command_buffer_->GetTransferBuffer(0));
  EXPECT_EQ(1, command_buffer_->CreateTransferBuffer(1024));
}

TEST_F(CommandBufferTest, CannotUnregisterNegativeHandles) {
  command_buffer_->DestroyTransferBuffer(-1);
  EXPECT_EQ(1, command_buffer_->CreateTransferBuffer(1024));
}

TEST_F(CommandBufferTest, CannotUnregisterUnregisteredHandles) {
  command_buffer_->DestroyTransferBuffer(1);
  EXPECT_EQ(1, command_buffer_->CreateTransferBuffer(1024));
}

// Testing this case specifically because there is an optimization that takes
// a different code path in this case.
TEST_F(CommandBufferTest, UnregistersLastRegisteredHandle) {
  EXPECT_EQ(1, command_buffer_->CreateTransferBuffer(1024));
  command_buffer_->DestroyTransferBuffer(1);
  EXPECT_EQ(1, command_buffer_->CreateTransferBuffer(1024));
}

// Testing this case specifically because there is an optimization that takes
// a different code path in this case.
TEST_F(CommandBufferTest, UnregistersTwoLastRegisteredHandles) {
  EXPECT_EQ(1, command_buffer_->CreateTransferBuffer(1024));
  EXPECT_EQ(2, command_buffer_->CreateTransferBuffer(1024));
  command_buffer_->DestroyTransferBuffer(2);
  command_buffer_->DestroyTransferBuffer(1);
  EXPECT_EQ(1, command_buffer_->CreateTransferBuffer(1024));
}

TEST_F(CommandBufferTest, DefaultTokenIsZero) {
  EXPECT_EQ(0, command_buffer_->GetToken());
}

TEST_F(CommandBufferTest, CanSetToken) {
  command_buffer_->SetToken(7);
  EXPECT_EQ(7, command_buffer_->GetToken());
}

TEST_F(CommandBufferTest, DefaultParseErrorIsNoError) {
  EXPECT_EQ(0, command_buffer_->ResetParseError());
}

TEST_F(CommandBufferTest, CanSetAndResetParseError) {
  command_buffer_->SetParseError(1);
  EXPECT_EQ(1, command_buffer_->ResetParseError());
  EXPECT_EQ(0, command_buffer_->ResetParseError());
}

TEST_F(CommandBufferTest, DefaultErrorStatusIsFalse) {
  EXPECT_FALSE(command_buffer_->GetErrorStatus());
}

TEST_F(CommandBufferTest, CanRaiseErrorStatus) {
  command_buffer_->RaiseErrorStatus();
  EXPECT_TRUE(command_buffer_->GetErrorStatus());
}

}  // namespace gpu_plugin
