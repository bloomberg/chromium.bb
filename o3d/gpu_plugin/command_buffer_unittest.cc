// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    command_buffer_object_ = NPCreateObject<CommandBuffer>(NULL);

    window_object_ = NPCreateObject<DynamicNPObject>(NULL);

    chromium_object_ = NPCreateObject<DynamicNPObject>(NULL);
    NPSetProperty(NULL, window_object_, "chromium", chromium_object_);

    system_object_ = NPCreateObject<StrictMock<MockSystemNPObject> >(NULL);
    NPSetProperty(NULL, chromium_object_, "system", system_object_);
  }

  MockNPBrowser mock_browser_;
  NPObjectPointer<CommandBuffer> command_buffer_object_;
  NPObjectPointer<DynamicNPObject> window_object_;
  NPObjectPointer<DynamicNPObject> chromium_object_;
  NPObjectPointer<MockSystemNPObject> system_object_;
};

TEST_F(CommandBufferTest, TestBehaviorWhileUninitialized) {
  EXPECT_EQ(NPObjectPointer<NPObject>(),
            command_buffer_object_->GetSharedMemory());
  EXPECT_EQ(0, command_buffer_object_->GetGetOffset());
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

  EXPECT_TRUE(command_buffer_object_->Initialize(1024));
  EXPECT_EQ(expected_shared_memory, command_buffer_object_->GetSharedMemory());

  // Cannot reinitialize.
  EXPECT_FALSE(command_buffer_object_->Initialize(1024));
  EXPECT_EQ(expected_shared_memory, command_buffer_object_->GetSharedMemory());
}

TEST_F(CommandBufferTest, InitializeFailsIfCannotCreateSharedMemory) {
  EXPECT_CALL(mock_browser_, GetWindowNPObject(NULL))
    .WillOnce(Return(window_object_.ToReturned()));

  EXPECT_CALL(*system_object_.Get(), CreateSharedMemory(1024))
    .WillOnce(Return(NPObjectPointer<NPObject>()));

  EXPECT_FALSE(command_buffer_object_->Initialize(1024));
  EXPECT_EQ(NPObjectPointer<NPObject>(),
            command_buffer_object_->GetSharedMemory());
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

  EXPECT_FALSE(command_buffer_object_->Initialize(1024));
  EXPECT_EQ(NPObjectPointer<NPObject>(),
            command_buffer_object_->GetSharedMemory());
}
}  // namespace gpu_plugin
}  // namespace o3d
