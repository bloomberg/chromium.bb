/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the tests for the FencedAllocator class.

#include "tests/common/win/testing_common.h"
#include "base/message_loop.h"
#include "command_buffer/client/cross/cmd_buffer_helper.h"
#include "command_buffer/client/cross/fenced_allocator.h"
#include "command_buffer/service/cross/cmd_buffer_engine.h"
#include "command_buffer/service/cross/mocks.h"
#include "gpu_plugin/command_buffer.h"
#include "gpu_plugin/gpu_processor.h"
#include "gpu_plugin/np_utils/np_object_pointer.h"
#include "gpu_plugin/system_services/shared_memory.h"

namespace o3d {
namespace command_buffer {

using gpu_plugin::CommandBuffer;
using gpu_plugin::GPUProcessor;
using gpu_plugin::NPCreateObject;
using gpu_plugin::NPObjectPointer;
using testing::Return;
using testing::Mock;
using testing::Truly;
using testing::Sequence;
using testing::DoAll;
using testing::Invoke;
using testing::_;

class BaseFencedAllocatorTest : public testing::Test {
 protected:
  static const unsigned int kBufferSize = 1024;

  virtual void SetUp() {
    api_mock_.reset(new AsyncAPIMock);
    // ignore noops in the mock - we don't want to inspect the internals of the
    // helper.
    EXPECT_CALL(*api_mock_, DoCommand(kNoop, 0, _))
        .WillRepeatedly(Return(parse_error::kParseNoError));
    // Forward the SetToken calls to the engine
    EXPECT_CALL(*api_mock_.get(), DoCommand(kSetToken, 1, _))
        .WillRepeatedly(DoAll(Invoke(api_mock_.get(), &AsyncAPIMock::SetToken),
                              Return(parse_error::kParseNoError)));

    NPObjectPointer<gpu_plugin::SharedMemory> ring_buffer =
        NPCreateObject<gpu_plugin::SharedMemory>(NULL);
    ring_buffer->Initialize(1024);

    size_t size_bytes;
    void* buffer = NPN_MapMemory(NULL, ring_buffer.Get(), &size_bytes);

    command_buffer_ = NPCreateObject<CommandBuffer>(NULL);
    command_buffer_->Initialize(ring_buffer);

    parser_ = new command_buffer::CommandParser(buffer,
                                                kBufferSize,
                                                0,
                                                kBufferSize,
                                                0,
                                                api_mock_.get());

    scoped_refptr<GPUProcessor> gpu_processor(new GPUProcessor(
        NULL, command_buffer_.Get(), NULL, NULL, parser_, INT_MAX));
    command_buffer_->SetPutOffsetChangeCallback(NewCallback(
        gpu_processor.get(), &GPUProcessor::ProcessCommands));

    api_mock_->set_engine(gpu_processor.get());

    helper_.reset(new CommandBufferHelper(NULL, command_buffer_));
    helper_->Initialize();
  }

  virtual void TearDown() {
    helper_.release();
  }

  scoped_ptr<AsyncAPIMock> api_mock_;
  NPObjectPointer<CommandBuffer> command_buffer_;
  command_buffer::CommandParser* parser_;
  scoped_ptr<CommandBufferHelper> helper_;
};

#ifndef COMPILER_MSVC
const unsigned int BaseFencedAllocatorTest::kBufferSize;
#endif

// Test fixture for FencedAllocator test - Creates a FencedAllocator, using a
// CommandBufferHelper with a mock AsyncAPIInterface for its interface (calling
// it directly, not through the RPC mechanism), making sure Noops are ignored
// and SetToken are properly forwarded to the engine.
class FencedAllocatorTest : public BaseFencedAllocatorTest {
 protected:
  virtual void SetUp() {
    BaseFencedAllocatorTest::SetUp();
    allocator_.reset(new FencedAllocator(kBufferSize, helper_.get()));
  }

  virtual void TearDown() {
    // If the GPUProcessor posts any tasks, this forces them to run.
    MessageLoop::current()->RunAllPending();

    EXPECT_TRUE(allocator_->CheckConsistency());
    allocator_.release();

    BaseFencedAllocatorTest::TearDown();
  }

  scoped_ptr<FencedAllocator> allocator_;
};

// Checks basic alloc and free.
TEST_F(FencedAllocatorTest, TestBasic) {
  allocator_->CheckConsistency();

  const unsigned int kSize = 16;
  FencedAllocator::Offset offset = allocator_->Alloc(kSize);
  EXPECT_NE(FencedAllocator::kInvalidOffset, offset);
  EXPECT_GE(kBufferSize, offset+kSize);
  EXPECT_TRUE(allocator_->CheckConsistency());

  allocator_->Free(offset);
  EXPECT_TRUE(allocator_->CheckConsistency());
}

// Checks out-of-memory condition.
TEST_F(FencedAllocatorTest, TestOutOfMemory) {
  EXPECT_TRUE(allocator_->CheckConsistency());

  const unsigned int kSize = 16;
  const unsigned int kAllocCount = kBufferSize / kSize;
  CHECK(kAllocCount * kSize == kBufferSize);

  // Allocate several buffers to fill in the memory.
  FencedAllocator::Offset offsets[kAllocCount];
  for (unsigned int i = 0; i < kAllocCount; ++i) {
    offsets[i] = allocator_->Alloc(kSize);
    EXPECT_NE(FencedAllocator::kInvalidOffset, offsets[i]);
    EXPECT_GE(kBufferSize, offsets[i]+kSize);
    EXPECT_TRUE(allocator_->CheckConsistency());
  }

  // This allocation should fail.
  FencedAllocator::Offset offset_failed = allocator_->Alloc(kSize);
  EXPECT_EQ(FencedAllocator::kInvalidOffset, offset_failed);
  EXPECT_TRUE(allocator_->CheckConsistency());

  // Free one successful allocation, reallocate with half the size
  allocator_->Free(offsets[0]);
  EXPECT_TRUE(allocator_->CheckConsistency());
  offsets[0] = allocator_->Alloc(kSize/2);
  EXPECT_NE(FencedAllocator::kInvalidOffset, offsets[0]);
  EXPECT_GE(kBufferSize, offsets[0]+kSize);
  EXPECT_TRUE(allocator_->CheckConsistency());

  // This allocation should fail as well.
  offset_failed = allocator_->Alloc(kSize);
  EXPECT_EQ(FencedAllocator::kInvalidOffset, offset_failed);
  EXPECT_TRUE(allocator_->CheckConsistency());

  // Free up everything.
  for (unsigned int i = 0; i < kAllocCount; ++i) {
    allocator_->Free(offsets[i]);
    EXPECT_TRUE(allocator_->CheckConsistency());
  }
}

// Checks the free-pending-token mechanism.
TEST_F(FencedAllocatorTest, TestFreePendingToken) {
  EXPECT_TRUE(allocator_->CheckConsistency());

  const unsigned int kSize = 16;
  const unsigned int kAllocCount = kBufferSize / kSize;
  CHECK(kAllocCount * kSize == kBufferSize);

  // Allocate several buffers to fill in the memory.
  FencedAllocator::Offset offsets[kAllocCount];
  for (unsigned int i = 0; i < kAllocCount; ++i) {
    offsets[i] = allocator_->Alloc(kSize);
    EXPECT_NE(FencedAllocator::kInvalidOffset, offsets[i]);
    EXPECT_GE(kBufferSize, offsets[i]+kSize);
    EXPECT_TRUE(allocator_->CheckConsistency());
  }

  // This allocation should fail.
  FencedAllocator::Offset offset_failed = allocator_->Alloc(kSize);
  EXPECT_EQ(FencedAllocator::kInvalidOffset, offset_failed);
  EXPECT_TRUE(allocator_->CheckConsistency());

  // Free one successful allocation, pending fence.
  int32 token = helper_.get()->InsertToken();
  allocator_->FreePendingToken(offsets[0], token);
  EXPECT_TRUE(allocator_->CheckConsistency());

  // The way we hooked up the helper and engine, it won't process commands
  // until it has to wait for something. Which means the token shouldn't have
  // passed yet at this point.
  EXPECT_GT(token, command_buffer_->GetToken());

  // This allocation will need to reclaim the space freed above, so that should
  // process the commands until the token is passed.
  offsets[0] = allocator_->Alloc(kSize);
  EXPECT_NE(FencedAllocator::kInvalidOffset, offsets[0]);
  EXPECT_GE(kBufferSize, offsets[0]+kSize);
  EXPECT_TRUE(allocator_->CheckConsistency());
  // Check that the token has indeed passed.
  EXPECT_LE(token, command_buffer_->GetToken());

  // Free up everything.
  for (unsigned int i = 0; i < kAllocCount; ++i) {
    allocator_->Free(offsets[i]);
    EXPECT_TRUE(allocator_->CheckConsistency());
  }
}

// Tests GetLargestFreeSize
TEST_F(FencedAllocatorTest, TestGetLargestFreeSize) {
  EXPECT_TRUE(allocator_->CheckConsistency());
  EXPECT_EQ(kBufferSize, allocator_->GetLargestFreeSize());

  FencedAllocator::Offset offset = allocator_->Alloc(kBufferSize);
  ASSERT_NE(FencedAllocator::kInvalidOffset, offset);
  EXPECT_EQ(0u, allocator_->GetLargestFreeSize());
  allocator_->Free(offset);
  EXPECT_EQ(kBufferSize, allocator_->GetLargestFreeSize());

  const unsigned int kSize = 16;
  offset = allocator_->Alloc(kSize);
  ASSERT_NE(FencedAllocator::kInvalidOffset, offset);
  // The following checks that the buffer is allocated "smartly" - which is
  // dependent on the implementation. But both first-fit or best-fit would
  // ensure that.
  EXPECT_EQ(kBufferSize - kSize, allocator_->GetLargestFreeSize());

  // Allocate 2 more buffers (now 3), and then free the first two. This is to
  // ensure a hole. Note that this is dependent on the first-fit current
  // implementation.
  FencedAllocator::Offset offset1 = allocator_->Alloc(kSize);
  ASSERT_NE(FencedAllocator::kInvalidOffset, offset1);
  FencedAllocator::Offset offset2 = allocator_->Alloc(kSize);
  ASSERT_NE(FencedAllocator::kInvalidOffset, offset2);
  allocator_->Free(offset);
  allocator_->Free(offset1);
  EXPECT_EQ(kBufferSize - 3 * kSize, allocator_->GetLargestFreeSize());

  offset = allocator_->Alloc(kBufferSize - 3 * kSize);
  ASSERT_NE(FencedAllocator::kInvalidOffset, offset);
  EXPECT_EQ(2 * kSize, allocator_->GetLargestFreeSize());

  offset1 = allocator_->Alloc(2 * kSize);
  ASSERT_NE(FencedAllocator::kInvalidOffset, offset1);
  EXPECT_EQ(0u, allocator_->GetLargestFreeSize());

  allocator_->Free(offset);
  allocator_->Free(offset1);
  allocator_->Free(offset2);
}

// Tests GetLargestFreeOrPendingSize
TEST_F(FencedAllocatorTest, TestGetLargestFreeOrPendingSize) {
  EXPECT_TRUE(allocator_->CheckConsistency());
  EXPECT_EQ(kBufferSize, allocator_->GetLargestFreeOrPendingSize());

  FencedAllocator::Offset offset = allocator_->Alloc(kBufferSize);
  ASSERT_NE(FencedAllocator::kInvalidOffset, offset);
  EXPECT_EQ(0u, allocator_->GetLargestFreeOrPendingSize());
  allocator_->Free(offset);
  EXPECT_EQ(kBufferSize, allocator_->GetLargestFreeOrPendingSize());

  const unsigned int kSize = 16;
  offset = allocator_->Alloc(kSize);
  ASSERT_NE(FencedAllocator::kInvalidOffset, offset);
  // The following checks that the buffer is allocates "smartly" - which is
  // dependent on the implementation. But both first-fit or best-fit would
  // ensure that.
  EXPECT_EQ(kBufferSize - kSize, allocator_->GetLargestFreeOrPendingSize());

  // Allocate 2 more buffers (now 3), and then free the first two. This is to
  // ensure a hole. Note that this is dependent on the first-fit current
  // implementation.
  FencedAllocator::Offset offset1 = allocator_->Alloc(kSize);
  ASSERT_NE(FencedAllocator::kInvalidOffset, offset1);
  FencedAllocator::Offset offset2 = allocator_->Alloc(kSize);
  ASSERT_NE(FencedAllocator::kInvalidOffset, offset2);
  allocator_->Free(offset);
  allocator_->Free(offset1);
  EXPECT_EQ(kBufferSize - 3 * kSize,
            allocator_->GetLargestFreeOrPendingSize());

  // Free the last one, pending a token.
  int32 token = helper_.get()->InsertToken();
  allocator_->FreePendingToken(offset2, token);

  // Now all the buffers have been freed...
  EXPECT_EQ(kBufferSize, allocator_->GetLargestFreeOrPendingSize());
  // .. but one is still waiting for the token.
  EXPECT_EQ(kBufferSize - 3 * kSize,
            allocator_->GetLargestFreeSize());

  // The way we hooked up the helper and engine, it won't process commands
  // until it has to wait for something. Which means the token shouldn't have
  // passed yet at this point.
  EXPECT_GT(token, command_buffer_->GetToken());
  // This allocation will need to reclaim the space freed above, so that should
  // process the commands until the token is passed, but it will succeed.
  offset = allocator_->Alloc(kBufferSize);
  ASSERT_NE(FencedAllocator::kInvalidOffset, offset);
  // Check that the token has indeed passed.
  EXPECT_LE(token, command_buffer_->GetToken());
  allocator_->Free(offset);

  // Everything now has been freed...
  EXPECT_EQ(kBufferSize, allocator_->GetLargestFreeOrPendingSize());
  // ... for real.
  EXPECT_EQ(kBufferSize, allocator_->GetLargestFreeSize());
}

// Test fixture for FencedAllocatorWrapper test - Creates a
// FencedAllocatorWrapper, using a CommandBufferHelper with a mock
// AsyncAPIInterface for its interface (calling it directly, not through the
// RPC mechanism), making sure Noops are ignored and SetToken are properly
// forwarded to the engine.
class FencedAllocatorWrapperTest : public BaseFencedAllocatorTest {
 protected:
  virtual void SetUp() {
    BaseFencedAllocatorTest::SetUp();

    // Though allocating this buffer isn't strictly necessary, it makes
    // allocations point to valid addresses, so they could be used for
    // something.
    buffer_.reset(new char[kBufferSize]);
    allocator_.reset(new FencedAllocatorWrapper(kBufferSize, helper_.get(),
                                                buffer_.get()));
  }

  virtual void TearDown() {
    // If the GPUProcessor posts any tasks, this forces them to run.
    MessageLoop::current()->RunAllPending();

    EXPECT_TRUE(allocator_->CheckConsistency());
    allocator_.release();
    buffer_.release();

    BaseFencedAllocatorTest::TearDown();
  }

  scoped_ptr<FencedAllocatorWrapper> allocator_;
  scoped_array<char> buffer_;
};

// Checks basic alloc and free.
TEST_F(FencedAllocatorWrapperTest, TestBasic) {
  allocator_->CheckConsistency();

  const unsigned int kSize = 16;
  void *pointer = allocator_->Alloc(kSize);
  ASSERT_TRUE(pointer);
  EXPECT_LE(buffer_.get(), static_cast<char *>(pointer));
  EXPECT_GE(kBufferSize, static_cast<char *>(pointer) - buffer_.get() + kSize);
  EXPECT_TRUE(allocator_->CheckConsistency());

  allocator_->Free(pointer);
  EXPECT_TRUE(allocator_->CheckConsistency());

  char *pointer_char = allocator_->AllocTyped<char>(kSize);
  ASSERT_TRUE(pointer_char);
  EXPECT_LE(buffer_.get(), pointer_char);
  EXPECT_GE(buffer_.get() + kBufferSize, pointer_char + kSize);
  allocator_->Free(pointer_char);
  EXPECT_TRUE(allocator_->CheckConsistency());

  unsigned int *pointer_uint = allocator_->AllocTyped<unsigned int>(kSize);
  ASSERT_TRUE(pointer_uint);
  EXPECT_LE(buffer_.get(), reinterpret_cast<char *>(pointer_uint));
  EXPECT_GE(buffer_.get() + kBufferSize,
            reinterpret_cast<char *>(pointer_uint + kSize));

  // Check that it did allocate kSize * sizeof(unsigned int). We can't tell
  // directly, except from the remaining size.
  EXPECT_EQ(kBufferSize - kSize * sizeof(*pointer_uint),
            allocator_->GetLargestFreeSize());
  allocator_->Free(pointer_uint);
}

// Checks out-of-memory condition.
TEST_F(FencedAllocatorWrapperTest, TestOutOfMemory) {
  allocator_->CheckConsistency();

  const unsigned int kSize = 16;
  const unsigned int kAllocCount = kBufferSize / kSize;
  CHECK(kAllocCount * kSize == kBufferSize);

  // Allocate several buffers to fill in the memory.
  void *pointers[kAllocCount];
  for (unsigned int i = 0; i < kAllocCount; ++i) {
    pointers[i] = allocator_->Alloc(kSize);
    EXPECT_TRUE(pointers[i]);
    EXPECT_TRUE(allocator_->CheckConsistency());
  }

  // This allocation should fail.
  void *pointer_failed = allocator_->Alloc(kSize);
  EXPECT_FALSE(pointer_failed);
  EXPECT_TRUE(allocator_->CheckConsistency());

  // Free one successful allocation, reallocate with half the size
  allocator_->Free(pointers[0]);
  EXPECT_TRUE(allocator_->CheckConsistency());
  pointers[0] = allocator_->Alloc(kSize/2);
  EXPECT_TRUE(pointers[0]);
  EXPECT_TRUE(allocator_->CheckConsistency());

  // This allocation should fail as well.
  pointer_failed = allocator_->Alloc(kSize);
  EXPECT_FALSE(pointer_failed);
  EXPECT_TRUE(allocator_->CheckConsistency());

  // Free up everything.
  for (unsigned int i = 0; i < kAllocCount; ++i) {
    allocator_->Free(pointers[i]);
    EXPECT_TRUE(allocator_->CheckConsistency());
  }
}

// Checks the free-pending-token mechanism.
TEST_F(FencedAllocatorWrapperTest, TestFreePendingToken) {
  allocator_->CheckConsistency();

  const unsigned int kSize = 16;
  const unsigned int kAllocCount = kBufferSize / kSize;
  CHECK(kAllocCount * kSize == kBufferSize);

  // Allocate several buffers to fill in the memory.
  void *pointers[kAllocCount];
  for (unsigned int i = 0; i < kAllocCount; ++i) {
    pointers[i] = allocator_->Alloc(kSize);
    EXPECT_TRUE(pointers[i]);
    EXPECT_TRUE(allocator_->CheckConsistency());
  }

  // This allocation should fail.
  void *pointer_failed = allocator_->Alloc(kSize);
  EXPECT_FALSE(pointer_failed);
  EXPECT_TRUE(allocator_->CheckConsistency());

  // Free one successful allocation, pending fence.
  int32 token = helper_.get()->InsertToken();
  allocator_->FreePendingToken(pointers[0], token);
  EXPECT_TRUE(allocator_->CheckConsistency());

  // The way we hooked up the helper and engine, it won't process commands
  // until it has to wait for something. Which means the token shouldn't have
  // passed yet at this point.
  EXPECT_GT(token, command_buffer_->GetToken());

  // This allocation will need to reclaim the space freed above, so that should
  // process the commands until the token is passed.
  pointers[0] = allocator_->Alloc(kSize);
  EXPECT_TRUE(pointers[0]);
  EXPECT_TRUE(allocator_->CheckConsistency());
  // Check that the token has indeed passed.
  EXPECT_LE(token, command_buffer_->GetToken());

  // Free up everything.
  for (unsigned int i = 0; i < kAllocCount; ++i) {
    allocator_->Free(pointers[i]);
    EXPECT_TRUE(allocator_->CheckConsistency());
  }
}


}  // namespace command_buffer
}  // namespace o3d
