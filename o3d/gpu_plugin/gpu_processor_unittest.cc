// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/message_loop.h"
#include "o3d/gpu_plugin/command_buffer_mock.h"
#include "o3d/gpu_plugin/gpu_processor.h"
#include "o3d/gpu_plugin/np_utils/np_browser_stub.h"
#include "o3d/gpu_plugin/np_utils/np_object_pointer.h"
#include "o3d/gpu_plugin/system_services/shared_memory_mock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

namespace o3d {
namespace gpu_plugin {

class GPUProcessorTest : public testing::Test {
 protected:
  virtual void SetUp() {
    shared_memory_ = NPCreateObject<NiceMock<MockSharedMemory> >(NULL);
    shared_memory_->ptr = buffer_;
    shared_memory_->size = sizeof(buffer_);
    memset(buffer_, 0, sizeof(buffer_));

    command_buffer_ = NPCreateObject<MockCommandBuffer>(NULL);
    ON_CALL(*command_buffer_.Get(), GetRingBuffer())
      .WillByDefault(Return(shared_memory_));

    processor_ = new GPUProcessor(command_buffer_);
  }

  virtual void TearDown() {
    // Ensure that any unexpected tasks posted by the GPU processor are executed
    // in order to fail the test.
    MessageLoop::current()->RunAllPending();
  }

  base::AtExitManager at_exit_manager;
  MessageLoop message_loop;
  StubNPBrowser stub_browser_;
  NPObjectPointer<MockCommandBuffer> command_buffer_;
  NPObjectPointer<NiceMock<MockSharedMemory> > shared_memory_;
  int32 buffer_[1024 / sizeof(int32)];
  scoped_refptr<GPUProcessor> processor_;
};

TEST_F(GPUProcessorTest, ProcessorDoesNothingIfNoSharedMemory) {
  EXPECT_CALL(*command_buffer_, GetGetOffset())
    .WillOnce(Return(0));
  EXPECT_CALL(*command_buffer_, GetPutOffset())
    .WillOnce(Return(0));
  EXPECT_CALL(*command_buffer_, GetSize())
    .WillOnce(Return(1024));
  ON_CALL(*command_buffer_.Get(), GetRingBuffer())
    .WillByDefault(Return(NPObjectPointer<CHRSharedMemory>()));

  processor_->ProcessCommands();
}

TEST_F(GPUProcessorTest, ProcessorDoesNothingIfSharedMemoryIsUnmapped) {
  EXPECT_CALL(*command_buffer_, GetGetOffset())
    .WillOnce(Return(0));
  EXPECT_CALL(*command_buffer_, GetPutOffset())
    .WillOnce(Return(0));
  EXPECT_CALL(*command_buffer_, GetSize())
    .WillOnce(Return(1024));
  shared_memory_->ptr = NULL;

  processor_->ProcessCommands();
}

TEST_F(GPUProcessorTest, ProcessorDoesNothingIfCommandBufferIsZeroSize) {
  EXPECT_CALL(*command_buffer_, GetGetOffset())
    .WillOnce(Return(0));
  EXPECT_CALL(*command_buffer_, GetPutOffset())
    .WillOnce(Return(0));
  EXPECT_CALL(*command_buffer_, GetSize())
    .WillOnce(Return(0));

  processor_->ProcessCommands();
}

TEST_F(GPUProcessorTest, ProcessorDoesNothingIfCommandBufferIsEmpty) {
  EXPECT_CALL(*command_buffer_, GetGetOffset())
    .WillOnce(Return(0));
  EXPECT_CALL(*command_buffer_, GetPutOffset())
    .WillOnce(Return(0));
  EXPECT_CALL(*command_buffer_, GetSize())
    .WillOnce(Return(sizeof(buffer_)));

  processor_->ProcessCommands();
}

TEST_F(GPUProcessorTest, ProcessorHandlesSingleNopCommand) {
  EXPECT_CALL(*command_buffer_, GetGetOffset())
    .WillOnce(Return(0));
  EXPECT_CALL(*command_buffer_, GetPutOffset())
    .WillOnce(Return(4));
  EXPECT_CALL(*command_buffer_, GetSize())
    .WillOnce(Return(sizeof(buffer_)));
  EXPECT_CALL(*command_buffer_, SetGetOffset(4));

  processor_->ProcessCommands();
}

TEST_F(GPUProcessorTest, ProcessorWrapsAroundToBeginningOfCommandBuffer) {
  EXPECT_CALL(*command_buffer_, GetGetOffset())
    .WillOnce(Return(4));
  EXPECT_CALL(*command_buffer_, GetPutOffset())
    .WillOnce(Return(0));
  EXPECT_CALL(*command_buffer_, GetSize())
    .WillOnce(Return(8));
  EXPECT_CALL(*command_buffer_, SetGetOffset(0));

  processor_->ProcessCommands();
}

TEST_F(GPUProcessorTest,
    ProcessorHandlesSingleCommandAndPostsTaskToProcessAdditionalCommands) {
  EXPECT_CALL(*command_buffer_, GetGetOffset())
    .WillOnce(Return(0))
    .WillOnce(Return(4));
  EXPECT_CALL(*command_buffer_, GetPutOffset())
    .WillOnce(Return(8))
    .WillOnce(Return(8));
  EXPECT_CALL(*command_buffer_, GetSize())
    .WillOnce(Return(sizeof(buffer_)))
    .WillOnce(Return(sizeof(buffer_)));
  EXPECT_CALL(*command_buffer_, SetGetOffset(4));

  processor_->ProcessCommands();

  EXPECT_CALL(*command_buffer_, SetGetOffset(8));
  MessageLoop::current()->RunAllPending();
}

TEST_F(GPUProcessorTest, ProcessorDoesNotImmediatelyHandlePartialCommands) {
  EXPECT_CALL(*command_buffer_, GetGetOffset())
    .WillOnce(Return(0))
    .WillOnce(Return(0));
  EXPECT_CALL(*command_buffer_, GetPutOffset())
    .WillOnce(Return(2))
    .WillOnce(Return(4));
  EXPECT_CALL(*command_buffer_, GetSize())
    .WillOnce(Return(sizeof(buffer_)))
    .WillOnce(Return(sizeof(buffer_)));

  processor_->ProcessCommands();

  EXPECT_CALL(*command_buffer_, SetGetOffset(4));
  MessageLoop::current()->RunAllPending();
}

}  // namespace gpu_plugin
}  // namespace o3d
