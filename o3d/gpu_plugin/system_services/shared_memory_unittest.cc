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

namespace gpu_plugin {

class SharedMemoryTest : public testing::Test {
 protected:
  virtual void SetUp() {
    shared_memory_ = NPCreateObject<SharedMemory>(NULL);
  }

  StubNPBrowser stub_browser_;
  NPObjectPointer<SharedMemory> shared_memory_;
};

TEST_F(SharedMemoryTest, SizeIsaZeroBeforeInitialization) {
  EXPECT_EQ(0, shared_memory_->GetSize());
}

TEST_F(SharedMemoryTest, InitializesAndReturnsMappedMemory) {
  EXPECT_TRUE(shared_memory_->Initialize(65536));
  EXPECT_EQ(65536, shared_memory_->GetSize());

  size_t size;
  int8* ptr = static_cast<int8*>(NPN_MapMemory(NULL,
                                               shared_memory_.Get(),
                                               &size));
  ASSERT_TRUE(NULL != ptr);
  EXPECT_EQ(65536, size);

  // Test that memory can be written to.
  for (int i = 0; i < 65536; ++i) {
    ptr[i] = 7;
  }
}

TEST_F(SharedMemoryTest, MapFailsBeforeInitialization) {
  size_t size = 7;
  int8* ptr = static_cast<int8*>(NPN_MapMemory(NULL,
                                               shared_memory_.Get(),
                                               &size));
  EXPECT_TRUE(NULL == ptr);
  EXPECT_EQ(0, size);
}

TEST_F(SharedMemoryTest, InitializeFailsForNegativeSize) {
  EXPECT_FALSE(shared_memory_->Initialize(-1));
}

TEST_F(SharedMemoryTest, SecondCallToInitializeFails) {
  EXPECT_TRUE(shared_memory_->Initialize(65536));
  EXPECT_FALSE(shared_memory_->Initialize(65536));
}

TEST_F(SharedMemoryTest, InitializeRoundsUpToPageSize) {
  EXPECT_TRUE(shared_memory_->Initialize(7));
  EXPECT_EQ(7, shared_memory_->GetSize());

  size_t size;
  int8* ptr = static_cast<int8*>(NPN_MapMemory(NULL,
                                               shared_memory_.Get(),
                                               &size));
  ASSERT_TRUE(NULL != ptr);
  EXPECT_EQ(7, size);

  // Test that memory can be written to.
  for (int i = 0; i < 7; ++i) {
    ptr[i] = 7;
  }
}

TEST_F(SharedMemoryTest, SecondMapDoesNothing) {
  EXPECT_TRUE(shared_memory_->Initialize(65536));

  size_t size1;
  int8* ptr1 = static_cast<int8*>(NPN_MapMemory(NULL,
                                               shared_memory_.Get(),
                                               &size1));

  size_t size2;
  int8* ptr2 = static_cast<int8*>(NPN_MapMemory(NULL,
                                               shared_memory_.Get(),
                                               &size2));

  EXPECT_EQ(ptr1, ptr2);
  EXPECT_EQ(65536, size1);
  EXPECT_EQ(65536, size2);
}

}  // namespace gpu_plugin
