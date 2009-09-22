// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/thread.h"
#include "o3d/gpu_plugin/command_buffer.h"
#include "o3d/gpu_plugin/np_utils/dynamic_np_object.h"
#include "o3d/gpu_plugin/np_utils/np_browser_mock.h"
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

namespace o3d {
namespace gpu_plugin {

class MockSystemNPObject : public DefaultNPObject<NPObject> {
 public:
  explicit MockSystemNPObject(NPP npp) {
  }

  MOCK_METHOD1(CreateSharedMemory, NPObjectPointer<NPObject>(int32));

  NP_UTILS_BEGIN_DISPATCHER_CHAIN(MockSystemNPObject, DefaultNPObject<NPObject>)
    NP_UTILS_DISPATCHER(CreateSharedMemory, NPObjectPointer<NPObject>(int32))
  NP_UTILS_END_DISPATCHER_CHAIN

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSystemNPObject);
};

class CommandBufferTest : public testing::Test {
 protected:
  virtual void SetUp() {
    command_buffer_ = NPCreateObject<CommandBuffer>(NULL);

    window_object_ = NPCreateObject<DynamicNPObject>(NULL);

    chromium_object_ = NPCreateObject<DynamicNPObject>(NULL);
    NPSetProperty(NULL, window_object_, "chromium", chromium_object_);

    system_object_ = NPCreateObject<StrictMock<MockSystemNPObject> >(NULL);
    NPSetProperty(NULL, chromium_object_, "system", system_object_);
  }

  MockNPBrowser mock_browser_;
  NPObjectPointer<CommandBuffer> command_buffer_;
  NPObjectPointer<DynamicNPObject> window_object_;
  NPObjectPointer<DynamicNPObject> chromium_object_;
  NPObjectPointer<MockSystemNPObject> system_object_;
};

TEST_F(CommandBufferTest, NullRingBufferByDefault) {
  EXPECT_EQ(NPObjectPointer<NPObject>(),
            command_buffer_->GetRingBuffer());
}

TEST_F(CommandBufferTest, InitializesCommandBuffer) {
  EXPECT_CALL(mock_browser_, GetWindowNPObject(NULL))
    .WillOnce(Return(window_object_.ToReturned()));

  NPObjectPointer<MockSharedMemory> expected_shared_memory =
      NPCreateObject<StrictMock<MockSharedMemory> >(NULL);

  EXPECT_CALL(*system_object_.Get(), CreateSharedMemory(1024))
    .WillOnce(Return(expected_shared_memory));

  EXPECT_CALL(*expected_shared_memory.Get(), Map())
    .WillOnce(Return(true));

  EXPECT_TRUE(command_buffer_->Initialize(1024));
  EXPECT_EQ(expected_shared_memory, command_buffer_->GetRingBuffer());

  // Cannot reinitialize.
  EXPECT_FALSE(command_buffer_->Initialize(1024));
  EXPECT_EQ(expected_shared_memory, command_buffer_->GetRingBuffer());
}

TEST_F(CommandBufferTest, InitializeFailsIfCannotCreateSharedMemory) {
  EXPECT_CALL(mock_browser_, GetWindowNPObject(NULL))
    .WillOnce(Return(window_object_.ToReturned()));

  EXPECT_CALL(*system_object_.Get(), CreateSharedMemory(1024))
    .WillOnce(Return(NPObjectPointer<NPObject>()));

  EXPECT_FALSE(command_buffer_->Initialize(1024));
  EXPECT_EQ(NPObjectPointer<NPObject>(),
            command_buffer_->GetRingBuffer());
}

TEST_F(CommandBufferTest, InitializeFailsIfCannotMapSharedMemory) {
  EXPECT_CALL(mock_browser_, GetWindowNPObject(NULL))
    .WillOnce(Return(window_object_.ToReturned()));

  NPObjectPointer<MockSharedMemory> expected_shared_memory =
      NPCreateObject<StrictMock<MockSharedMemory> >(NULL);

  EXPECT_CALL(*system_object_.Get(), CreateSharedMemory(1024))
    .WillOnce(Return(expected_shared_memory));

  EXPECT_CALL(*expected_shared_memory.Get(), Map())
    .WillOnce(Return(false));

  EXPECT_FALSE(command_buffer_->Initialize(1024));
  EXPECT_EQ(NPObjectPointer<NPObject>(),
            command_buffer_->GetRingBuffer());
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
  EXPECT_CALL(mock_browser_, GetWindowNPObject(NULL))
    .WillOnce(Return(window_object_.ToReturned()));

  NPObjectPointer<MockSharedMemory> expected_shared_memory =
      NPCreateObject<StrictMock<MockSharedMemory> >(NULL);

  EXPECT_CALL(*system_object_.Get(), CreateSharedMemory(1024))
    .WillOnce(Return(expected_shared_memory));

  EXPECT_CALL(*expected_shared_memory.Get(), Map())
    .WillOnce(Return(true));

  EXPECT_TRUE(command_buffer_->Initialize(1024));

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

TEST_F(CommandBufferTest, RegisteringNullObjectReturnsZero) {
  EXPECT_EQ(0, command_buffer_->RegisterObject(NPObjectPointer<NPObject>()));
}

TEST_F(CommandBufferTest, RegistersDistinctNonZeroHandlesForObject) {
  EXPECT_EQ(1, command_buffer_->RegisterObject(window_object_));
  EXPECT_EQ(window_object_, command_buffer_->GetRegisteredObject(1));
  EXPECT_EQ(2, command_buffer_->RegisterObject(window_object_));
  EXPECT_EQ(window_object_, command_buffer_->GetRegisteredObject(2));
}

TEST_F(CommandBufferTest, RegisterObjectReusesUnregisteredHandles) {
  EXPECT_EQ(1, command_buffer_->RegisterObject(window_object_));
  EXPECT_EQ(window_object_, command_buffer_->GetRegisteredObject(1));
  EXPECT_EQ(2, command_buffer_->RegisterObject(window_object_));
  EXPECT_EQ(window_object_, command_buffer_->GetRegisteredObject(2));
  command_buffer_->UnregisterObject(window_object_, 1);
  EXPECT_EQ(1, command_buffer_->RegisterObject(window_object_));
  EXPECT_EQ(window_object_, command_buffer_->GetRegisteredObject(1));
  EXPECT_EQ(3, command_buffer_->RegisterObject(window_object_));
  EXPECT_EQ(window_object_, command_buffer_->GetRegisteredObject(3));
}

TEST_F(CommandBufferTest, CannotUnregisterHandleZero) {
  command_buffer_->UnregisterObject(window_object_, 0);
  EXPECT_TRUE(NULL == command_buffer_->GetRegisteredObject(0).Get());
  EXPECT_EQ(1, command_buffer_->RegisterObject(window_object_));
  EXPECT_EQ(window_object_, command_buffer_->GetRegisteredObject(1));
}

TEST_F(CommandBufferTest, CannotUnregisterNegativeHandles) {
  command_buffer_->UnregisterObject(window_object_, -1);
  EXPECT_EQ(1, command_buffer_->RegisterObject(window_object_));
  EXPECT_EQ(window_object_, command_buffer_->GetRegisteredObject(1));
}

TEST_F(CommandBufferTest, CannotUnregisterUnregisteredHandles) {
  command_buffer_->UnregisterObject(window_object_, 1);
  EXPECT_EQ(1, command_buffer_->RegisterObject(window_object_));
  EXPECT_EQ(window_object_, command_buffer_->GetRegisteredObject(1));
}

TEST_F(CommandBufferTest,
    CannotUnregisterHandleWithoutDemonstratingAccessToObject) {
  EXPECT_EQ(1, command_buffer_->RegisterObject(window_object_));
  command_buffer_->UnregisterObject(chromium_object_, 1);
  EXPECT_EQ(window_object_, command_buffer_->GetRegisteredObject(1));
  EXPECT_EQ(2, command_buffer_->RegisterObject(window_object_));
}

// Testing this case specifically because there is an optimization that takes
// a different code path in this case.
TEST_F(CommandBufferTest, UnregistersLastRegisteredHandle) {
  EXPECT_EQ(1, command_buffer_->RegisterObject(window_object_));
  EXPECT_EQ(window_object_, command_buffer_->GetRegisteredObject(1));
  command_buffer_->UnregisterObject(window_object_, 1);
  EXPECT_EQ(1, command_buffer_->RegisterObject(window_object_));
  EXPECT_EQ(window_object_, command_buffer_->GetRegisteredObject(1));
}

// Testing this case specifically because there is an optimization that takes
// a different code path in this case.
TEST_F(CommandBufferTest, UnregistersTwoLastRegisteredHandles) {
  EXPECT_EQ(1, command_buffer_->RegisterObject(window_object_));
  EXPECT_EQ(window_object_, command_buffer_->GetRegisteredObject(1));
  EXPECT_EQ(2, command_buffer_->RegisterObject(window_object_));
  EXPECT_EQ(window_object_, command_buffer_->GetRegisteredObject(2));
  command_buffer_->UnregisterObject(window_object_, 2);
  command_buffer_->UnregisterObject(window_object_, 1);
  EXPECT_EQ(1, command_buffer_->RegisterObject(window_object_));
  EXPECT_EQ(window_object_, command_buffer_->GetRegisteredObject(1));
}

}  // namespace gpu_plugin
}  // namespace o3d
