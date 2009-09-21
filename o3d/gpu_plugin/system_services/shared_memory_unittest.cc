// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"
#include "o3d/gpu_plugin/np_utils/np_browser_stub.h"
#include "o3d/gpu_plugin/system_services/shared_memory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

namespace o3d {
namespace gpu_plugin {

class SharedMemoryTest : public testing::Test {
 protected:
  virtual void SetUp() {
    shared_memory_ = NPCreateObject<SharedMemory>(NULL);
  }

  StubNPBrowser stub_browser_;
  NPObjectPointer<SharedMemory> shared_memory_;
};

TEST_F(SharedMemoryTest, MemoryIsNotMappedBeforeInitialization) {
  EXPECT_TRUE(NULL == shared_memory_->handle);
  EXPECT_TRUE(NULL == shared_memory_->ptr);
  EXPECT_EQ(0, shared_memory_->size);
  EXPECT_EQ(0, shared_memory_->GetSize());
}

TEST_F(SharedMemoryTest, InitializesAndReturnsMappedMemory) {
  EXPECT_TRUE(shared_memory_->Initialize(65536));
  EXPECT_TRUE(NULL != shared_memory_->handle);
  EXPECT_TRUE(NULL == shared_memory_->ptr);
  EXPECT_EQ(65536, shared_memory_->size);
  EXPECT_EQ(65536, shared_memory_->GetSize());

  EXPECT_TRUE(shared_memory_->Map());
  ASSERT_TRUE(NULL != shared_memory_->ptr);
  EXPECT_EQ(65536, shared_memory_->size);

  // Test that memory can be written to.
  int8* ptr = static_cast<int8*>(shared_memory_->ptr);
  for (int i = 0; i < 65536; ++i) {
    ptr[i] = 7;
  }
}

TEST_F(SharedMemoryTest, MapFailsBeforeInitialization) {
  EXPECT_FALSE(shared_memory_->Map());
}

TEST_F(SharedMemoryTest, InitializeFailsForNegativeSize) {
  EXPECT_FALSE(shared_memory_->Initialize(-1));
}

TEST_F(SharedMemoryTest, SecondCallToInitializeFails) {
  EXPECT_TRUE(shared_memory_->Initialize(65536));
  EXPECT_FALSE(shared_memory_->Initialize(65536));
}

TEST_F(SharedMemoryTest, InitializeRoundsUpToPageSizeButReportsRequestedSize) {
  EXPECT_TRUE(shared_memory_->Initialize(7));

  EXPECT_TRUE(shared_memory_->Map());
  EXPECT_EQ(7, shared_memory_->size);

  // Test that memory can be written to.
  int8* ptr = static_cast<int8*>(shared_memory_->ptr);
  for (int i = 0; i < 7; ++i) {
    ptr[i] = 7;
  }
}

TEST_F(SharedMemoryTest, SecondMapDoesNothing) {
  EXPECT_TRUE(shared_memory_->Initialize(65536));

  EXPECT_TRUE(shared_memory_->Map());
  ASSERT_TRUE(NULL != shared_memory_->handle);
  ASSERT_TRUE(NULL != shared_memory_->ptr);
  EXPECT_EQ(65536, shared_memory_->size);

  void* handle = shared_memory_->handle;
  void* ptr = shared_memory_->ptr;
  EXPECT_TRUE(shared_memory_->Map());
  ASSERT_EQ(ptr, shared_memory_->ptr);
  EXPECT_EQ(65536, shared_memory_->size);
}

TEST_F(SharedMemoryTest, CanInitializeWithHandle) {
  base::SharedMemory* temp_shared_memory = new base::SharedMemory;
  EXPECT_TRUE(temp_shared_memory->Create(std::wstring(), false, false, 65536));

  shared_memory_->Initialize(temp_shared_memory, 65536);
  EXPECT_TRUE(NULL != shared_memory_->handle);
  EXPECT_TRUE(NULL == shared_memory_->ptr);
  EXPECT_EQ(65536, shared_memory_->size);

  EXPECT_TRUE(shared_memory_->Map());
  EXPECT_TRUE(NULL != shared_memory_->handle);
  EXPECT_TRUE(NULL != shared_memory_->ptr);
  EXPECT_EQ(65536, shared_memory_->size);
}

TEST_F(SharedMemoryTest, CanSetInt32) {
  base::SharedMemory* temp_shared_memory = new base::SharedMemory;
  EXPECT_TRUE(temp_shared_memory->Create(std::wstring(), false, false, 65536));

  shared_memory_->Initialize(temp_shared_memory, 65536);
  EXPECT_TRUE(shared_memory_->Map());

  EXPECT_TRUE(shared_memory_->SetInt32(4, 7));
  EXPECT_EQ(7, static_cast<int32*>(shared_memory_->ptr)[1]);

  EXPECT_TRUE(shared_memory_->SetInt32(4, 8));
  EXPECT_EQ(8, static_cast<int32*>(shared_memory_->ptr)[1]);
}

TEST_F(SharedMemoryTest, FailsIfSetInt32CalledBeforeMap) {
  base::SharedMemory* temp_shared_memory = new base::SharedMemory;
  EXPECT_TRUE(temp_shared_memory->Create(std::wstring(), false, false, 65536));

  shared_memory_->Initialize(temp_shared_memory, 65536);

  EXPECT_FALSE(shared_memory_->SetInt32(4, 7));
}

TEST_F(SharedMemoryTest, FailsIfSetInt32OffsetIsOutOfRange) {
  base::SharedMemory* temp_shared_memory = new base::SharedMemory;
  EXPECT_TRUE(temp_shared_memory->Create(std::wstring(), false, false, 65536));

  shared_memory_->Initialize(temp_shared_memory, 65536);
  EXPECT_TRUE(shared_memory_->Map());

  EXPECT_FALSE(shared_memory_->SetInt32(-1, 7));
  EXPECT_FALSE(shared_memory_->SetInt32(65536, 7));
}

TEST_F(SharedMemoryTest, FailsIfSetInt32OffsetIsMisaligned) {
  base::SharedMemory* temp_shared_memory = new base::SharedMemory;
  EXPECT_TRUE(temp_shared_memory->Create(std::wstring(), false, false, 65536));

  shared_memory_->Initialize(temp_shared_memory, 65536);
  EXPECT_TRUE(shared_memory_->Map());

  EXPECT_FALSE(shared_memory_->SetInt32(1, 7));
}

}  // namespace gpu_plugin
}  // namespace o3d
