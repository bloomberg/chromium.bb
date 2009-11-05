// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/thread.h"
#include "o3d/gpu_plugin/command_buffer.h"
#include "o3d/gpu_plugin/np_utils/np_browser_mock.h"
#include "o3d/gpu_plugin/np_utils/dynamic_np_object.h"
#include "o3d/gpu_plugin/np_utils/np_object_mock.h"
#include "o3d/gpu_plugin/np_utils/np_object_pointer.h"
#include "o3d/gpu_plugin/system_services/shared_memory_mock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

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
    ring_buffer_ = NPCreateObject<MockSharedMemory>(NULL);

    ON_CALL(*ring_buffer_.Get(), GetSize())
      .WillByDefault(Return(1024));
  }

  MockNPBrowser mock_browser_;
  NPObjectPointer<CommandBuffer> command_buffer_;
  NPObjectPointer<MockSharedMemory> ring_buffer_;
};

TEST_F(CommandBufferTest, NullRingBufferByDefault) {
  EXPECT_EQ(NPObjectPointer<NPObject>(),
            command_buffer_->GetRingBuffer());
}

TEST_F(CommandBufferTest, InitializesCommandBuffer) {
  EXPECT_TRUE(command_buffer_->Initialize(ring_buffer_));
  EXPECT_TRUE(ring_buffer_ == command_buffer_->GetRingBuffer());
  EXPECT_EQ(256, command_buffer_->GetSize());
}

TEST_F(CommandBufferTest, InitializeFailsSecondTime) {
  EXPECT_TRUE(command_buffer_->Initialize(ring_buffer_));
  EXPECT_FALSE(command_buffer_->Initialize(ring_buffer_));
}

TEST_F(CommandBufferTest, InitializeFailsIfSizeIsNegative) {
  ON_CALL(*ring_buffer_.Get(), GetSize())
    .WillByDefault(Return(-1024));

  EXPECT_FALSE(command_buffer_->Initialize(ring_buffer_));
}

TEST_F(CommandBufferTest, InitializeFailsIfRingBufferIsNull) {
  EXPECT_FALSE(command_buffer_->Initialize(NPObjectPointer<NPObject>()));
}

TEST_F(CommandBufferTest, InitializeFailsIfRingBufferDoesNotImplementGetSize) {
  EXPECT_FALSE(command_buffer_->Initialize(
      NPCreateObject<DynamicNPObject>(NULL)));
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
  EXPECT_TRUE(command_buffer_->Initialize(ring_buffer_));

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
  EXPECT_TRUE(NULL == command_buffer_->GetRegisteredObject(0).Get());
}

TEST_F(CommandBufferTest, NegativeHandleMapsToNull) {
  EXPECT_TRUE(NULL == command_buffer_->GetRegisteredObject(-1).Get());
}

TEST_F(CommandBufferTest, OutOfRangeHandleMapsToNull) {
  EXPECT_TRUE(NULL == command_buffer_->GetRegisteredObject(1).Get());
}

TEST_F(CommandBufferTest, RegisteringNullObjectReturnsZero) {
  EXPECT_EQ(0, command_buffer_->RegisterObject(NPObjectPointer<NPObject>()));
}

TEST_F(CommandBufferTest, RegistersDistinctNonZeroHandlesForObject) {
  EXPECT_EQ(1, command_buffer_->RegisterObject(ring_buffer_));
  EXPECT_EQ(ring_buffer_, command_buffer_->GetRegisteredObject(1));
  EXPECT_EQ(2, command_buffer_->RegisterObject(ring_buffer_));
  EXPECT_EQ(ring_buffer_, command_buffer_->GetRegisteredObject(2));
}

TEST_F(CommandBufferTest, RegisterObjectReusesUnregisteredHandles) {
  EXPECT_EQ(1, command_buffer_->RegisterObject(ring_buffer_));
  EXPECT_EQ(ring_buffer_, command_buffer_->GetRegisteredObject(1));
  EXPECT_EQ(2, command_buffer_->RegisterObject(ring_buffer_));
  EXPECT_EQ(ring_buffer_, command_buffer_->GetRegisteredObject(2));
  command_buffer_->UnregisterObject(ring_buffer_, 1);
  EXPECT_EQ(1, command_buffer_->RegisterObject(ring_buffer_));
  EXPECT_EQ(ring_buffer_, command_buffer_->GetRegisteredObject(1));
  EXPECT_EQ(3, command_buffer_->RegisterObject(ring_buffer_));
  EXPECT_EQ(ring_buffer_, command_buffer_->GetRegisteredObject(3));
}

TEST_F(CommandBufferTest, CannotUnregisterHandleZero) {
  command_buffer_->UnregisterObject(ring_buffer_, 0);
  EXPECT_TRUE(NULL == command_buffer_->GetRegisteredObject(0).Get());
  EXPECT_EQ(1, command_buffer_->RegisterObject(ring_buffer_));
  EXPECT_EQ(ring_buffer_, command_buffer_->GetRegisteredObject(1));
}

TEST_F(CommandBufferTest, CannotUnregisterNegativeHandles) {
  command_buffer_->UnregisterObject(ring_buffer_, -1);
  EXPECT_EQ(1, command_buffer_->RegisterObject(ring_buffer_));
  EXPECT_EQ(ring_buffer_, command_buffer_->GetRegisteredObject(1));
}

TEST_F(CommandBufferTest, CannotUnregisterUnregisteredHandles) {
  command_buffer_->UnregisterObject(ring_buffer_, 1);
  EXPECT_EQ(1, command_buffer_->RegisterObject(ring_buffer_));
  EXPECT_EQ(ring_buffer_, command_buffer_->GetRegisteredObject(1));
}

TEST_F(CommandBufferTest,
    CannotUnregisterHandleWithoutDemonstratingAccessToObject) {
  EXPECT_EQ(1, command_buffer_->RegisterObject(ring_buffer_));
  command_buffer_->UnregisterObject(command_buffer_, 1);
  EXPECT_EQ(ring_buffer_, command_buffer_->GetRegisteredObject(1));
  EXPECT_EQ(2, command_buffer_->RegisterObject(ring_buffer_));
}

// Testing this case specifically because there is an optimization that takes
// a different code path in this case.
TEST_F(CommandBufferTest, UnregistersLastRegisteredHandle) {
  EXPECT_EQ(1, command_buffer_->RegisterObject(ring_buffer_));
  EXPECT_EQ(ring_buffer_, command_buffer_->GetRegisteredObject(1));
  command_buffer_->UnregisterObject(ring_buffer_, 1);
  EXPECT_EQ(1, command_buffer_->RegisterObject(ring_buffer_));
  EXPECT_EQ(ring_buffer_, command_buffer_->GetRegisteredObject(1));
}

// Testing this case specifically because there is an optimization that takes
// a different code path in this case.
TEST_F(CommandBufferTest, UnregistersTwoLastRegisteredHandles) {
  EXPECT_EQ(1, command_buffer_->RegisterObject(ring_buffer_));
  EXPECT_EQ(ring_buffer_, command_buffer_->GetRegisteredObject(1));
  EXPECT_EQ(2, command_buffer_->RegisterObject(ring_buffer_));
  EXPECT_EQ(ring_buffer_, command_buffer_->GetRegisteredObject(2));
  command_buffer_->UnregisterObject(ring_buffer_, 2);
  command_buffer_->UnregisterObject(ring_buffer_, 1);
  EXPECT_EQ(1, command_buffer_->RegisterObject(ring_buffer_));
  EXPECT_EQ(ring_buffer_, command_buffer_->GetRegisteredObject(1));
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
